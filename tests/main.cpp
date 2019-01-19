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


#include "card.hpp"
#include <iostream>

#include <experimental/coroutine>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <boost/asio/experimental.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/write.hpp>

namespace net = boost::asio;
namespace netx = boost::asio::experimental;
namespace beast = boost::beast;
namespace ws = boost::beast::websocket;

netx::awaitable<void> do_session(net::ip::tcp::socket socket) {
  try {
    ws::stream<net::ip::tcp::socket> websocket(std::move(socket));
    auto token = co_await netx::this_coro::token();
    co_await websocket.async_accept(token);
    std::cout << "Accepted a connection.\n";
    for (;;) {
      beast::multi_buffer buffer;
      co_await websocket.async_read(buffer, token);
      websocket.text(websocket.got_text());
      co_await websocket.async_write(buffer.data(), token);
    }
  } catch (std::exception& e) {
    std::cerr << "Error in do_session: " << e.what() << '\n';
  }
}

netx::awaitable<void> listener(net::ip::tcp::acceptor acceptor) {
  auto token = co_await netx::this_coro::token();
  do_session(co_await acceptor.async_accept(token));
}

int main(int argc, const char * argv[]) {
  net::io_context context(1);
  netx::co_spawn(context, [&]{
    constexpr int port = 1337;
    const auto protocol = net::ip::tcp::v4();
    return listener(net::ip::tcp::acceptor(context, {protocol, port}));
  }, netx::detached);
  net::signal_set signals(context, SIGINT, SIGTERM);
  signals.async_wait([&](auto, auto){ context.stop(); });
  context.run();
  return 0;
}
