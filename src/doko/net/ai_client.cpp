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

#include "doko/net/ai_client.hpp"
#include "doko/card_assignment.hpp"

#include <fmt/format.h>

namespace doko {
namespace {
struct ai_error_category : boost::system::error_category {
  const char* name() const noexcept override { return "doko_ai_errors"; }

  std::string message(int ev) const override {
    switch (static_cast<ai_errc>(ev)) {
    case ai_errc::computation_already_running:
      return "Computation is already running.";
    case ai_errc::computation_aborted:
      return "Computation have been aborted.";
    default:
      return "Invalid error code.";
    }
  }
};

const ai_error_category error_category{};

template <class ConstBufferSequence>
std::string_view buffers_front_string(ConstBufferSequence const& buffers) {
  auto const b = boost::beast::buffers_front(buffers);
  return {reinterpret_cast<char const*>(b.data()), b.size()};
}
} // namespace

void accumulate_stats(accumulated_uct_stats& stats,
                      const uct_tree& tree) noexcept {
  for (std::ptrdiff_t child : tree.children(0)) {
    stats.visits[child - 1] += tree.num_visits(child);
    std::transform(tree.eyes(child).begin(), tree.eyes(child).end(),
                   stats.eyes[child - 1].begin(), stats.eyes[child - 1].begin(),
                   std::plus<>{});
    std::transform(tree.scores(child).begin(), tree.scores(child).end(),
                   stats.scores[child - 1].begin(),
                   stats.scores[child - 1].begin(), std::plus<>{});
  }
}

error_code make_error_code(ai_errc value) noexcept {
  return {static_cast<int>(value), error_category};
}

ai_client::ai_client(net::io_context& context, ai_client_options options)
    : resolver_(context),
      websocket_(tcp::socket(context)), options_{std::move(options)} {}

std::shared_ptr<ai_client> ai_client::make_shared(net::io_context& context,
                                                  ai_client_options options) {
  return std::shared_ptr<ai_client>{new ai_client(context, std::move(options))};
}

void ai_client::async_read() {
  websocket_.async_read(
      buffer_, [self = shared_from_this()](error_code ec, auto) {
        if (ec) {
          throw std::runtime_error("async_read error");
        }
        std::string_view input = buffers_front_string(self->buffer_.data());
        self->on_read(input);
        self->buffer_.consume(input.size());
        self->async_read();
      });
}

void ai_client::on_read(std::string_view message) {
  // This is a small helper function to send json messages to the server.
  auto send = [&](const nlohmann::json& j) {
    net::post(websocket_.get_executor(), [j, this] {
      send_message_queue_.push(j.dump());
      if (!is_sending_) {
        async_write();
      }
    });
  };

  // parse the input message of the server.
  nlohmann::json input;
  try {
    input = nlohmann::json::parse(message);
  } catch (nlohmann::json::parse_error& error) {
    // TODO report error to log system
    fmt::print("[WARNING] Recieved an ill-formed message from the server.\n");
    fmt::print("[WARNING] This is very odd and should never occur.\n");
    return;
  }

  if (input.count("error")) {
    // TODO report error to log system
    return;
  }
  if (!input.count("command")) {
    state_.merge_patch(input);
  }
  // Wait with doing any actions until we recieve all tables from the server.
  if (!state_.count("tables")) {
    return;
  }

  // If we have not joined a table yet, create and join the assigned table.
  // else if we are not an active player we try to take a seat which is left.
  if (state_.count("joined_table") == 0 || state_["joined_table"].is_null()) {
    // If the required table does not exist yet, we try to create it.
    auto pos = std::find_if(state_["tables"].begin(), state_["tables"].end(),
                            [&](const nlohmann::json& table) {
                              return table["name"].get<std::string>() ==
                                     options_.table_name;
                            });
    if (pos == state_["tables"].end()) {
      send({{"command", "create_table"}, {"name", options_.table_name}});
    } else {
      send({{"command", "join_table"}, {"name", options_.table_name}});
    }
  } else if (state_["joined_table"].count("player_id") == 0 ||
             state_["joined_table"]["player_id"].is_null()) {
    const nlohmann::json& players = state_["joined_table"].at("players");
    auto pos =
        std::find_if(players.begin(), players.end(),
                     [](const nlohmann::json& p) { return p.is_null(); });
    if (pos == players.end()) {
      throw std::runtime_error("No seat left to take.");
    }
    send({{"command", "take_seat"},
          {"seat", std::distance(players.begin(), pos)}});
  }

  // We recieve for each action in a game a notification formed like
  //
  // {"command": "observe", "action": <action>}
  //
  // These will always be sent before we will be asked to do an action.
  if (input.count("command") &&
      input["command"].get<std::string>() == "observe" &&
      input.count("action")) {
    const action a = input["action"];
    observed_actions_.push_back(a);
    fmt::print("<= Player #{} plays {}\n", to_integer(a.player()), a);
  }

  if (input.count("command") && input["command"].get<std::string>() == "play") {
    const player_id this_player =
        state_["joined_table"]["player_id"].get<player_id>();
    const action* observed =
        std::find_if(observed_actions_.begin(), observed_actions_.end(),
                     [](action a) { return a.as_card(); });
    const player_id initial_player = (observed == observed_actions_.end())
                                         ? this_player
                                         : observed->player();
    const game_rules rules =
        state_["joined_table"]["game"]["rules"].get<game_rules>();
    if (!ai_action_kernel_) {
      uct_tree new_tree({}, observed_actions_,
                        options_.action_kernel_options.n_rollouts);
      ai_action_kernel_.emplace(std::move(new_tree),
                                options_.action_kernel_options);
    }
    ai_action_kernel_->async_rollout(
        thread_pool_.get_executor(), rules, initial_player, *initial_hand_,
        observed_actions_,
        [send](error_code ec, const accumulated_uct_stats& stats) {
          if (ec == ai_errc::computation_aborted) {
            return;
          }
          const std::ptrdiff_t* max_visits =
              std::max_element(stats.visits.begin(), stats.visits.end());
          const doko::action best =
              stats.actions[std::distance(stats.visits.begin(), max_visits)];
          send({{"command", "play"}, {"action", best}});
        });
  }

  if (input.count("command") &&
      input["command"].get<std::string>() == "declare") {
    const player_id this_player =
        state_["joined_table"]["player_id"].get<player_id>();
    const player_id initial_player =
        state_["joined_table"]["game"]["initial_player"].get<player_id>();
    initial_hand_ =
        state_["joined_table"]["game"].at("hand").get<std::array<card, 12>>();
    if (!ai_contract_kernel_) {
      uct_tree new_tree({}, observed_actions_,
                        options_.contract_kernel_options.n_rollouts);
      ai_contract_kernel_.emplace(std::move(new_tree),
                                  options_.contract_kernel_options);
    }
    ai_contract_kernel_->async_rollout(
        thread_pool_.get_executor(), initial_player, *initial_hand_,
        [send](error_code ec, declared_contract contract) {
          send({{"command", "choose"}, {"declared_contract", contract}});
        });
  }

  if (input.count("command") &&
      input["command"].get<std::string>() == "specialize") {
    const player_id this_player =
        state_["joined_table"]["player_id"].get<player_id>();
    send({{"command", "choose"},
          {"specialized_contract",
           specialized_contract{.player = this_player,
                                .rules = *ai_contract_kernel_->best_rules()}}});
  }
}

void ai_client::async_write() {
  websocket_.async_write(net::buffer(send_message_queue_.front()),
                         [self = shared_from_this()](error_code ec, auto) {
                           if (ec) {
                             self->is_sending_ = false;
                             throw std::runtime_error("write error");
                           }
                           self->send_message_queue_.pop();
                           if (!self->send_message_queue_.empty()) {
                             self->async_write();
                           } else {
                             self->is_sending_ = false;
                           }
                         });
}

void ai_client::run(std::string_view host, std::string_view service) {
  host_ = host;
  resolver_.async_resolve(
      host, service,
      [self = shared_from_this(), host](error_code ec,
                                        tcp::resolver::results_type results) {
        if (ec) {
          throw std::runtime_error(
              fmt::format("Failed to resolve the hostname '{}'.", host));
        }
        net::async_connect(
            self->websocket_.next_layer(), results,
            [self](error_code ec, const tcp::endpoint& endpoint) {
              if (ec) {
                const std::string address = endpoint.address().to_string();
                throw std::runtime_error(fmt::format(
                    "Failed to connect to '{}': {}.", address, ec.message()));
              }
              self->websocket_.async_handshake(
                  self->host_, "/", [self](error_code ec) {
                    if (ec) {
                      throw std::runtime_error("handshake failed");
                    }
                    self->async_read();
                  });
            });
      });
}

} // namespace doko