// Copyright (c) 2019 Maikel Nadolski
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "doko/net/server.hpp"

#include "fub/core/function_ref.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <map>
#include <random>
#include <string>
#include <variant>

namespace doko {
namespace {
std::optional<player_id> find_player_id(const server_session& session) {
  if (session.joined_table()) {
    std::array<server_session*, 4> players = session.joined_table()->players();
    auto pos = std::find(players.begin(), players.end(), &session);
    if (pos != players.end()) {
      return player_id(std::distance(players.begin(), pos));
    }
    return {};
  }
  return {};
}

/// Returns the first buffer in a sequence as a string view.
template <class ConstBufferSequence>
std::string_view buffers_front_string(ConstBufferSequence const& buffers) {
  auto const b = boost::beast::buffers_front(buffers);
  return {reinterpret_cast<char const*>(b.data()), b.size()};
}
} // namespace

server_session::server_session(tcp::socket socket,
                               std::shared_ptr<server> server)
    : server_(std::move(server)), websocket_(std::move(socket)) {}

server_session::~server_session() noexcept {
  server_->leave(*this);
  if (joined_table_) {
    leave_table();
  }
}

std::shared_ptr<server_session>
server_session::make_shared(tcp::socket socket,
                            std::shared_ptr<server> server) {
  return std::shared_ptr<server_session>(
      new server_session(std::move(socket), std::move(server)));
}

// Acessor Methods

const std::string& server_session::name() const noexcept { return name_; }

const server& server_session::get_server() const noexcept { return *server_; }

const table* server_session::joined_table() const noexcept {
  return joined_table_;
}

// Modifier Methods

void server_session::async_write() {
  websocket_.async_write(net::buffer(send_message_queue_.front()),
                         [self = shared_from_this()](error_code ec, auto) {
                           if (ec) {
                             self->is_sending_ = false;
                             return;
                           }
                           fmt::print("[OUT] {}\n",
                                      self->send_message_queue_.front());
                           self->send_message_queue_.pop();
                           if (!self->send_message_queue_.empty()) {
                             self->async_write();
                           } else {
                             self->is_sending_ = false;
                           }
                         });
}

void server_session::send(nlohmann::json message) {
  send_message_queue_.push(message.dump());
  if (!is_sending_) {
    is_sending_ = true;
    async_write();
  }
}

void server_session::join_table(doko::table& table) {
  if (joined_table_) {
    leave_table();
  }
  joined_table_ = &table;
  joined_table_->join(*this);
}

void server_session::leave_table() {
  if (joined_table_) {
    joined_table_->leave(*this);
    joined_table_ = nullptr;
  }
}

void server_session::on_read(const nlohmann::json& input) {
  using command_function =
      fub::function_ref<void(server_session&, const nlohmann::json&)>;
  static const auto commands = [] {
    std::map<std::string, command_function> commands;
    commands.insert(
        {"play", [](server_session& session, const nlohmann::json& input) {
           if (std::optional<player_id> player = find_player_id(session)) {
             session.joined_table_->play(input.at("action").get<action>(),
                                         session);
           }
         }});
    commands.insert(
        {"choose", [](server_session& session, const nlohmann::json& input) {
           if (std::optional<player_id> player = find_player_id(session)) {
             if (input.count("declared_contract")) {
               const auto contract =
                   input["declared_contract"].get<declared_contract>();
               session.joined_table_->choose(contract, session);
             } else if (input.count("specialized_contract")) {
               const auto contract =
                   input["specialized_contract"].get<specialized_contract>();
               session.joined_table_->choose(contract, session);
             }
           }
         }});
    commands.insert({"create_table",
                     [](server_session& session, const nlohmann::json& input) {
                       session.server_->create_table(
                           input.at("name").get<std::string>());
                     }});
    commands.insert({"destroy_table",
                     [](server_session& session, const nlohmann::json& input) {
                       session.server_->destroy_table(
                           input.at("name").get<std::string>());
                     }});
    commands.insert(
        {"tables", [](server_session& session, const nlohmann::json&) {
           session.send({{"tables", session.server_->tables()}});
         }});
    commands.insert({"join_table",
                     [](server_session& session, const nlohmann::json& input) {
                       std::list<table>& tables = session.server_->tables();
                       const std::string name = input.at("name");
                       auto pos = std::find_if(
                           tables.begin(), tables.end(),
                           [&](const table& t) { return t.name() == name; });
                       if (pos == tables.end()) {
                         throw std::runtime_error("Table Not Found.s");
                       }
                       session.join_table(*pos);
                     }});
    commands.insert(
        {"leave_table", [](server_session& session, const nlohmann::json&) {
           session.leave_table();
         }});
    commands.insert(
        {"take_seat", [](server_session& session, const nlohmann::json& j) {
           if (session.joined_table_) {
             int seat = j.at("seat").get<int>();
             session.joined_table_->take_seat(session, seat);
           } else {
             throw std::runtime_error("This session joined no table yet.");
           }
         }});
    return commands;
  }();
  std::string command;
  try {
    command = input.at("command").get<std::string>();
    commands.at(command)(*this, input);
  } catch (std::out_of_range&) {
    send({{"error", "Command Not Found"}, {"command", command}});
  } catch (std::exception& err) {
    send({{"error", err.what()}});
  }
}

void server_session::async_read() {
  websocket_.async_read(
      input_buffer_, [self = shared_from_this()](error_code ec, auto) {
        if (ec) {
          return;
        }
        nlohmann::json input;
        const std::string_view data =
            buffers_front_string(self->input_buffer_.data());
        fmt::print("[IN] {}\n", data);
        try {
          input = nlohmann::json::parse(data);
        } catch (nlohmann::json::parse_error& e) {
          fmt::print("[WARNING] A client sent an ill-formed message.\n");
          self->async_read();
          return;
        }
        self->on_read(input);
        self->input_buffer_.consume(data.size());
        self->async_read();
      });
}

void server_session::run() {
  websocket_.async_accept([self = shared_from_this()](error_code ec) {
    if (ec) {
      return;
    }
    self->server_->join(*self);
    self->async_read();
  });
}

namespace {
template <typename SessionRange>
void send_all(SessionRange&& sessions, const nlohmann::json& msg) {
  for (server_session* session : sessions) {
    session->send(msg);
  }
}

template <typename RandomNumberGenerator>
std::array<std::array<card, 12>, 4>
make_random_hands(RandomNumberGenerator& generator) {
  std::array<int, 48> ints;
  std::iota(ints.begin(), ints.end(), 0);
  std::shuffle(ints.begin(), ints.end(), generator);
  std::array<card, 48> cards;
  std::transform(ints.begin(), ints.end(), cards.begin(), [](int n) {
    return doko::card(color((n / 6) % 4), face(n % 6));
  });
  std::array<std::array<card, 12>, 4> hands;
  card* c = cards.begin();
  std::transform(c, c + 12, hands[0].begin(), [](card c) {
    return card(c.color(), c.face(), player_id::first);
  });
  std::transform(c + 12, c + 24, hands[1].begin(), [](card c) {
    return card(c.color(), c.face(), player_id::second);
  });
  std::transform(c + 24, c + 36, hands[2].begin(), [](card c) {
    return card(c.color(), c.face(), player_id::third);
  });
  std::transform(c + 36, c + 48, hands[3].begin(), [](card c) {
    return card(c.color(), c.face(), player_id::fourth);
  });
  return hands;
}

std::optional<static_vector<card, 12>> get_hand(const game_state_machine& game,
                                                player_id player) {
  const int p = to_integer(player);
  return std::visit(
      overload_set{
          [&](const game_state_machine::declare_contracts& c)
              -> std::optional<static_vector<card, 12>> {
            return to_static_vector(c.state.hands[p]);
          },
          [&](const game_state_machine::specialize_contracts& c)
              -> std::optional<static_vector<card, 12>> {
            return to_static_vector(c.state.hands[p]);
          },
          [&](const game_state_machine::running& r)
              -> std::optional<static_vector<card, 12>> {
            return r.state.hands[p];
          },
          [&](const game_state_machine::score&)
              -> std::optional<static_vector<card, 12>> { return {}; }},
      game.state);
}

nlohmann::json as_detailed_json(const table& table,
                                const server_session& session) {
  nlohmann::json j = table;
  j["game"] = table.game();
  std::optional<player_id> player = find_player_id(session);
  j["player_id"] = player;
  if (player && table.game()) {
    j["game"]["hand"] = get_hand(*table.game(), *player);
  }
  j = {{"joined_table", std::move(j)}};
  return j;
}
} // namespace

void to_json(nlohmann::json& j, const doko::table& table) {
  j = {{"name", table.name()},
       {"players", table.players()},
       {"observers", table.observers()}};
}

void to_json(nlohmann::json& j, doko::server_session* session) {
  if (session) {
    j = {{"name", session->name()}};
  } else {
    j = nullptr;
  }
}

table::table(std::string name, server* server)
    : name_{std::move(name)},
      game_({}), players_{}, observers_{}, server_{server} {}

void notify_change(const doko::table& table) {
  for (server_session* ob : table.observers()) {
    ob->send(as_detailed_json(table, *ob));
  }
  send_all(table.get_server().sessions(),
           {{"tables", table.get_server().tables()}});
}

void table::join(server_session& session) {
  if (auto [_, inserted] = observers_.insert(&session); inserted) {
    notify_change(*this);
  }
}

void table::leave(server_session& session) {
  if (observers_.erase(&session)) {
    if (auto pos = std::find(players_.begin(), players_.end(), &session);
        pos != players_.end()) {
      const int n = std::distance(players_.begin(), pos);
      players_[n] = nullptr;
    }
    notify_change(*this);
  }
}

void table::play(action action, server_session& session) {
  game_.value().play(action);
  send_all(observers_, {{"command", "observe"}, {"action", action}});
  if (const game_state_machine::score* score =
          std::get_if<game_state_machine::score>(&game_->state)) {
    notify_change(*this);
  } else {
    int next = to_integer(
        std::get<game_state_machine::running>(game_->state).state.player);
    if (players_[next]) {
      players_[next]->send({{"command", "play"}});
    }
  }
}

void table::choose(declared_contract contract, server_session& session) {
  game_.value().choose(contract);
  send_all(observers_,
           {{"command", "observe"}, {"declared_contract", contract}});
  if (const game_state_machine::specialize_contracts* game =
          std::get_if<game_state_machine::specialize_contracts>(
              &game_->state)) {
    notify_change(*this);
    const int next = to_integer(game->state.player);
    if (players_[next]) {
      players_[next]->send({{"command", "specialize"}});
    }
  } else if (const game_state_machine::running* game =
                 std::get_if<game_state_machine::running>(&game_->state)) {
    notify_change(*this);
    const int next = to_integer(game->state.player);
    if (players_[next]) {
      players_[next]->send({{"command", "play"}});
    }
  } else {
    const int next =
        to_integer(std::get<game_state_machine::declare_contracts>(game_->state)
                       .state.player);
    if (players_[next]) {
      players_[next]->send({{"command", "declare"}});
    }
  }
}

void table::choose(specialized_contract contract, server_session& session) {
  game_.value().choose(contract);
  send_all(observers_,
           {{"command", "observe"}, {"specialized_contract", contract}});
  if (const game_state_machine::running* game =
          std::get_if<game_state_machine::running>(&game_->state)) {
    notify_change(*this);
    const int next = to_integer(game->state.player);
    if (players_[next]) {
      players_[next]->send({{"command", "play"}});
    }
  } else {
    const int next = to_integer(
        std::get<game_state_machine::specialize_contracts>(game_->state)
            .state.player);
    if (players_[next]) {
      players_[next]->send({{"command", "sepcialize"}});
    }
  }
}

void table::take_seat(server_session& session, int seat) {
  if (0 <= seat && seat < 4 && !players_[seat]) {
    join(session);
    players_[seat] = &session;
    if (!game_ && std::count_if(players_.begin(), players_.end(),
                                [](server_session* s) { return s; }) == 4) {
      std::mt19937 gen{std::random_device{}()};
      std::array<std::array<card, 12>, 4> hands = make_random_hands(gen);
      game_ = game_state_machine(player_id::first, hands);
      notify_change(*this);
      players_[0]->send({{"command", "declare"}});
    } else {
      notify_change(*this);
    }
    return;
  }
  if (!(0 <= seat && seat < 4)) {
    throw std::invalid_argument("Invalid Argument");
  }
  throw std::runtime_error("Seat Already Taken");
}

void server::join(server_session& session) {
  clients_.insert(&session);
  send_all(sessions(), {{"clients", clients_}});
  session.send({{"tables", tables_}});
}

void server::leave(server_session& session) {
  if (clients_.erase(&session)) {
    send_all(sessions(), {{"clients", clients_}});
  }
}

void server::create_table(const std::string& name) {
  if (std::any_of(tables_.begin(), tables_.end(),
                  [&](const table& t) { return t.name() == name; })) {
    throw std::runtime_error("Duplicate Table");
  }
  tables_.emplace_back(name, this);
  send_all(sessions(), {{"tables", tables_}});
}

void server::destroy_table(const std::string& name) {
  auto pos = std::find_if(tables_.begin(), tables_.end(),
                          [&](const table& t) { return t.name() == name; });
  if (pos == tables_.end()) {
    throw std::runtime_error("Table Not Found");
  }
  if (std::any_of(pos->players().begin(), pos->players().end(),
                  [](server_session* s) { return s; }) ||
      pos->observers().size() > 0) {
    throw std::runtime_error("Table Not Empty");
  }
  tables_.erase(pos);
  send_all(sessions(), {{"tables", tables_}});
}

std::list<table>& server::tables() noexcept { return tables_; }
const std::list<table>& server::tables() const noexcept { return tables_; }

const std::unordered_set<server_session*>& server::sessions() const noexcept {
  return clients_;
}

void server::listen() {
  acceptor_.async_accept(socket_, [self = shared_from_this()](error_code ec) {
    if (ec) {
      throw std::runtime_error("Listen");
    }
    fmt::print("[INFO] Accepted a TCP connection to remote address '{}'.\n",
               self->socket_.remote_endpoint().address().to_string());
    server_session::make_shared(std::move(self->socket_), self)->run();
    self->listen();
  });
}

} // namespace doko