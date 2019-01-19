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

#ifndef DOKO_GAME_SERVER_HPP
#define DOKO_GAME_SERVER_HPP

#include "doko/net/asio.hpp"
#include "doko/net/json.hpp"

#include <list>
#include <map>
#include <optional>
#include <unordered_set>
#include <variant>
#include <queue>

namespace doko {
class server;
class table;

class server_session : public std::enable_shared_from_this<server_session> {
public:
  server_session() = delete;
  server_session(const server_session&) = delete;
  server_session(server_session&&) = delete;
  server_session& operator=(const server_session&) = delete;
  server_session& operator=(server_session&&) = delete;

  /// Deregister with the game server.
  ~server_session() noexcept;

  // Factory Method

  static std::shared_ptr<server_session>
  make_shared(tcp::socket socket, std::shared_ptr<server> server);

  // Accessor Methods

  const std::string& name() const noexcept;
  const server& get_server() const noexcept;
  const table* joined_table() const noexcept;

  //  Modifier Methods

  void run();
  void send(nlohmann::json message);
  void join_table(table& table);
  void leave_table();

private:
  server_session(tcp::socket socket, std::shared_ptr<server> server);

  void async_read();
  void on_read(const nlohmann::json&);

  void async_write();

  std::shared_ptr<server> server_;
  websocket::stream<tcp::socket> websocket_;
  std::string name_{"Gast"};
  std::queue<std::string> send_message_queue_{};
  bool is_sending_{false};
  table* joined_table_{nullptr};
  boost::beast::flat_buffer input_buffer_{};
};

void to_json(nlohmann::json& j, server_session* session);

class table {
public:
  explicit table(std::string name, doko::server* server);

  void join(server_session& session);
  void leave(server_session& session);
  void take_seat(server_session& session, int seat);

  void choose(declared_contract contract, server_session& session);
  void choose(specialized_contract contract, server_session& session);
  void play(action action, server_session& session);
  void next_game();

  const std::string& name() const noexcept { return name_; }

  const std::array<server_session*, 4>& players() const noexcept {
    return players_;
  }

  const std::unordered_set<server_session*>& observers() const noexcept {
    return observers_;
  }

  const std::optional<game_state_machine>& game() const noexcept {
    return game_;
  }

  server& get_server() const noexcept {
    return *server_;
  }

private:
  std::string name_;
  std::optional<game_state_machine> game_{};
  std::array<server_session*, 4> players_;
  std::unordered_set<server_session*> observers_;
  server* server_;
};

void to_json(nlohmann::json& j, const table& table);

class server : public std::enable_shared_from_this<server> {
public:
  server(tcp::acceptor acceptor)
      : acceptor_(std::move(acceptor)),
        socket_(acceptor_.get_executor().context()) {}

  std::list<table>& tables() noexcept;
  const std::list<table>& tables() const noexcept;
  const std::unordered_set<server_session*>& sessions() const noexcept;

  void join(server_session& session);
  void leave(server_session& session);
  void create_table(const std::string& name);
  void destroy_table(const std::string& name);

  void listen();

private:
  tcp::acceptor acceptor_;
  tcp::socket socket_;
  std::list<table> tables_{};
  std::unordered_set<server_session*> clients_{};
};

} // namespace doko

#endif