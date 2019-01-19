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

#ifndef DOKO_SERVER_CLIENT_HPP
#define DOKO_SERVER_CLIENT_HPP

#include "doko/card_assignment.hpp"
#include "doko/net/server.hpp"
#include "doko/uct_tree.hpp"

#include <nlohmann/json.hpp>

#include <fmt/color.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>

#include <boost/log/sources/logger.hpp>

#include <optional>
#include <queue>
#include <string>

namespace doko {
enum class ai_errc { computation_already_running, computation_aborted };
error_code make_error_code(ai_errc) noexcept;

inline std::ostream& operator<<(std::ostream& os, doko::color color) {
  std::array<const char*, 4> names{"diamonds", "hearts", "spades", "clubs"};
  os << names[to_integer(color)];
  return os;
}

inline std::ostream& operator<<(std::ostream& os, doko::face face) {
  std::array<const char*, 6> names{"nine", "jack", "queen",
                                   "king", "ten",  "ace"};
  os << names[to_integer(face)];
  return os;
}

inline std::ostream& operator<<(std::ostream& os, doko::party party) {
  if (party == doko::party::re) {
    os << "Re";
  } else {
    os << "Contra";
  }
  return os;
}

inline std::ostream& operator<<(std::ostream& os, doko::action action) {
  if (const doko::announcement* bid = action.as_bid()) {
    os << "announcement(" << bid->party() << ')';
  } else if (const doko::card* card = action.as_card()) {
    os << "card(" << card->color() << ", " << card->face() << ')';
  }
  return os;
}
} // namespace doko

namespace boost::system {
template <> struct is_error_code_enum<doko::ai_errc> : std::true_type {};
} // namespace boost::system

namespace doko {
struct ai_action_kernel_options {
  std::ptrdiff_t n_trees;
  std::ptrdiff_t n_rollouts;
};

struct ai_contract_kernel_options {
  std::ptrdiff_t n_trees;
  std::ptrdiff_t n_rollouts;
};

struct ai_client_options {
  std::string table_name;
  ai_action_kernel_options action_kernel_options;
  ai_contract_kernel_options contract_kernel_options;
};

template <typename RandomNumberGenerator, typename Rules>
void rollout_n(uct_tree& tree, std::ptrdiff_t n, RandomNumberGenerator& gen,
               const Rules& rules) {
  for (std::ptrdiff_t i = 0; i < n; ++i) {
    tree.rollout_once(gen, rules);
  }
}

struct accumulated_uct_stats {
  static_vector<std::ptrdiff_t, 13> visits;
  static_vector<doko::action, 13> actions;
  static_vector<std::array<double, 4>, 13> eyes;
  static_vector<std::array<double, 4>, 13> scores;
};

void accumulate_stats(accumulated_uct_stats& stats,
                      const uct_tree& tree) noexcept;

class ai_action_kernel {
public:
  ai_action_kernel(uct_tree tree, ai_action_kernel_options options)
      : tree_(std::move(tree)), n_trees_{options.n_trees},
        n_rollouts_{options.n_rollouts} {}

  template <typename Executor, typename CompletionHandler>
  error_code
  async_rollout(Executor executor, const game_rules& rules,
                player_id initial_player, const std::array<card, 12>& hand,
                span<const action> history, CompletionHandler handle) {
    static std::mt19937 random_number_generator(std::random_device{}());
    bool expected = false;
    if (is_running_flag_.compare_exchange_strong(expected, true,
                                                 std::memory_order_release,
                                                 std::memory_order_relaxed)) {
      static_vector<doko::action, 58> past(history.begin(), history.end());
      std::visit(
          [&](const auto& rules) {
            net::post(executor, [this, rules, initial_player, hand, past,
                                 h = std::move(handle)] {
              assign_cards_state state =
                  make_assign_cards_state(rules, hand, past);
              auto do_rollouts = [&] {
                std::ptrdiff_t iterations{};
                initial_game_state initial_state;
                initial_state.player = initial_player;
                initial_state.hands =
                    assign_cards_randomly(state, random_number_generator);
                tree_.reset(initial_state, past);
                error_code ec{};
                while (iterations < n_rollouts_) {
                  if (!is_running_flag_) {
                    ec = ai_errc::computation_aborted;
                    return ec;
                  }
                  const std::ptrdiff_t n =
                      std::max(batch_size_, n_rollouts_ - iterations);
                  rollout_n(tree_, n, random_number_generator, rules);
                  iterations += n;
                }
                return ec;
              };
              error_code ec = do_rollouts();
              accumulated_uct_stats stats{};
              for (std::ptrdiff_t child : tree_.children(0)) {
                stats.visits.push_back(tree_.num_visits(child));
                stats.actions.push_back(tree_.action(child));
                stats.eyes.push_back(tree_.eyes(child));
                stats.scores.push_back(tree_.scores(child));
              }
              for (std::ptrdiff_t t = 1; t < n_trees_ && !ec; ++t) {
                ec = do_rollouts();
                accumulate_stats(stats, tree_);
              }
              fmt::print("=> Stats for next action:\n");
              int counter = 0;
              int p = int(hand[0].player());
              int best = std::distance(
                  stats.visits.begin(),
                  std::max_element(stats.visits.begin(), stats.visits.end()));
              for (const action a : stats.actions) {
                const std::ptrdiff_t v = stats.visits[counter];
                if (best == counter) {
                  fmt::print(fg(fmt::terminal_color::green),
                             "===> {}: {} {} {}\n", a, v,
                             stats.eyes[counter][p] / v,
                             stats.scores[counter][p] / v);
                } else {
                  fmt::print("===> {}: {} {} {}\n", a, v,
                             stats.eyes[counter][p] / v,
                             stats.scores[counter][p] / v);
                }
                ++counter;
              }
              std::invoke(h, ec, stats);
              is_running_flag_ = false;
              return;
            });
          },
          rules);
      return {};
    }
    return ai_errc::computation_already_running;
  }

  bool is_running() { return is_running_flag_; }

  bool cancel() {
    bool expected = true;
    return is_running_flag_.compare_exchange_strong(
        expected, false, std::memory_order_release, std::memory_order_relaxed);
  }

private:
  uct_tree tree_;
  std::ptrdiff_t n_trees_;
  std::ptrdiff_t n_rollouts_;
  std::atomic<bool> is_running_flag_{false};
  std::ptrdiff_t batch_size_{100};
};

template <typename T, typename... Ts> auto make_array(Ts&&... args) {
  return std::array<T, sizeof...(Ts)>{T(args)...};
}

class ai_contract_kernel {
public:
  ai_contract_kernel(uct_tree tree, ai_contract_kernel_options options)
      : tree_(std::move(tree)), n_trees_{options.n_trees},
        n_rollouts_{options.n_rollouts} {}

  template <typename Executor, typename CompletionHandler>
  error_code async_rollout(Executor executor, player_id initial_player,
                           const std::array<card, 12>& hand,
                           CompletionHandler handle) {
    static std::mt19937 random_number_generator(std::random_device{}());
    bool expected = false;
    if (is_running_flag_.compare_exchange_strong(expected, true,
                                                 std::memory_order_release,
                                                 std::memory_order_relaxed)) {
      net::post(executor, [this, initial_player, hand, h = std::move(handle)] {
        const player_id this_player = hand[0].player();
        const auto rules_to_test =
            std::tuple{normal_game_rules{},
                       marriage_rules{this_player},
                       solo_rules{this_player, solo_type::jack},
                       solo_rules{this_player, solo_type::queen},
                       solo_rules{this_player, solo_type::diamonds},
                       solo_rules{this_player, solo_type::hearts},
                       solo_rules{this_player, solo_type::spades},
                       solo_rules{this_player, solo_type::clubs}};

        error_code global_error{};
        auto estimate_score = [&](const auto& rules) {
          span<const action> no_history{};
          assign_cards_state state =
              make_assign_cards_state(rules, hand, no_history);
          auto do_rollouts = [&] {
            std::ptrdiff_t iterations{};
            initial_game_state initial_state;
            initial_state.player = overload_set{
                [&](normal_game_rules) { return initial_player; },
                [&](marriage_rules) { return initial_player; },
                [&](solo_rules s) { return s.solo_player; }}(rules);
            initial_state.hands =
                assign_cards_randomly(state, random_number_generator);
            tree_.reset(initial_state, no_history);
            error_code ec{};
            while (iterations < n_rollouts_) {
              if (!is_running_flag_) {
                ec = ai_errc::computation_aborted;
                return ec;
              }
              const std::ptrdiff_t n =
                  std::max(batch_size_, n_rollouts_ - iterations);
              rollout_n(tree_, n, random_number_generator, rules);
              iterations += n;
            }
            return ec;
          };
          if (global_error = do_rollouts(); global_error) {
            return -std::numeric_limits<double>::infinity();
          }
          accumulated_uct_stats stats{};
          for (std::ptrdiff_t child : tree_.children(0)) {
            stats.visits.push_back(tree_.num_visits(child));
            stats.actions.push_back(tree_.action(child));
            stats.eyes.push_back(tree_.eyes(child));
            stats.scores.push_back(tree_.scores(child));
          }
          for (std::ptrdiff_t t = 1; t < n_trees_ && !global_error; ++t) {
            global_error = do_rollouts();
            accumulate_stats(stats, tree_);
          }
          auto best_action = std::distance(
              stats.visits.begin(),
              std::max_element(stats.visits.begin(), stats.visits.end()));
          return stats.scores[best_action][to_integer(this_player)] /
                 stats.visits[best_action];
        };

        const auto estimated_score = std::apply(
            [&](const auto&... rules) {
              return make_array<double>(overload_set{
                  [&](const marriage_rules& marriage) {
                    if (std::count(hand.begin(), hand.end(),
                                   card(color::clubs, face::queen,
                                        this_player)) == 2) {
                      return estimate_score(marriage);
                    }
                    return -std::numeric_limits<double>::infinity();
                  },
                  [&](const auto& other_rules) {
                    return estimate_score(other_rules);
                  }}(rules)...);
            },
            rules_to_test);

        const int n = std::distance(
            estimated_score.begin(),
            std::max_element(estimated_score.begin(), estimated_score.end()));

        fmt::print("=> Estimated scores: {}\n", estimated_score);

        best_rules_ = std::apply(
            [&](const auto&... rules) {
              return make_array<game_rules>(rules...)[n];
            },
            rules_to_test);

        std::invoke(
            h, global_error,
            std::visit(
                overload_set{
                    [this_player](normal_game_rules) -> declared_contract {
                      return {this_player, healthiness::healthy};
                    },
                    [this_player](auto) -> declared_contract {
                      return {this_player, healthiness::reservation};
                    }},
                *best_rules_));
        is_running_flag_ = false;
        return;
      });
      return {};
    }
    return ai_errc::computation_already_running;
  }

  bool is_running() { return is_running_flag_; }

  bool cancel() {
    bool expected = true;
    return is_running_flag_.compare_exchange_strong(
        expected, false, std::memory_order_release, std::memory_order_relaxed);
  }

  std::optional<game_rules> best_rules() {
    if (!is_running_flag_) {
      return best_rules_;
    }
    return {};
  }

private:
  uct_tree tree_;
  std::optional<game_rules> best_rules_{};
  std::ptrdiff_t n_trees_;
  std::ptrdiff_t n_rollouts_;
  std::atomic<bool> is_running_flag_{false};
  std::ptrdiff_t batch_size_{100};
};

class ai_client : public std::enable_shared_from_this<ai_client> {
public:
  static std::shared_ptr<ai_client> make_shared(net::io_context& context,
                                                ai_client_options options);

  ai_client() = delete;
  ai_client(const ai_client&) = delete;
  ai_client& operator=(const ai_client&) = delete;
  ai_client(ai_client&&) = delete;
  ai_client& operator=(ai_client&&) = delete;

  void run(std::string_view host, std::string_view service);

private:
  ai_client(net::io_context& context, ai_client_options options);

  void async_read();
  void on_read(std::string_view message);

  void async_write();
  void on_write();

  tcp::resolver resolver_;
  websocket::stream<tcp::socket> websocket_;
  ai_client_options options_;
  // boost::log::sources::logger logger_{};
  std::string host_{};
  boost::beast::flat_buffer buffer_{};
  nlohmann::json state_{};
  std::queue<std::string> send_message_queue_{};
  bool is_sending_{false};
  static_vector<action, 58> observed_actions_{};
  net::thread_pool thread_pool_{1};
  std::optional<std::array<card, 12>> initial_hand_{};
  std::optional<ai_action_kernel> ai_action_kernel_{};
  std::optional<ai_contract_kernel> ai_contract_kernel_{};
};

} // namespace doko

#endif