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

#ifndef DOKO_UCT_TREE_SEARCH_HPP
#define DOKO_UCT_TREE_SEARCH_HPP

#include "doko/game_rules.hpp"
#include "doko/random.hpp"
#include "doko/static_vector.hpp"

#include "fub/core/span.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace doko {
template <typename Range, typename Comparator, typename Projection>
auto max_element(Range&& range, Comparator comp, Projection proj) {
  return std::max_element(range.begin(), range.end(), [&](auto&& x, auto&& y) {
    return comp(proj(x), proj(y));
  });
}

template <typename Rules, typename RandomNumberGenerator>
span<const action>
rollout(const Rules& rules, static_vector<action, 58>& actions,
        const running_game_state& initial_state, RandomNumberGenerator& gen) {
  running_game_state state = initial_state;
  span<const card> hand = state.hands[int(state.player)];
  while (hand.size() > 0) {
    const action next =
        select_random_legal_action(actions, hand, state.trick, gen, rules);
    state.player =
        observe_action(rules, state.trick, state.hands, next, actions);
    actions.push_back(next);
    hand = state.hands[int(state.player)];
  }
  return actions;
}

template <typename RandomNumberGenerator>
std::array<card, 48> random_cards(RandomNumberGenerator& gen) {
  std::array<card, 48> cards;
  for (int n = 0; n < 48; ++n) {
    cards[n] = card(color((n / 6) % 4), face(n % 6));
  }
  std::shuffle(cards.begin(), cards.end(), gen);
  return cards;
}

template <typename RandomNumberGenerator>
std::array<std::array<card, 12>, 4>
random_initial_hands(RandomNumberGenerator& gen) {
  std::array<card, 48> cards = random_cards(gen);
  std::array<std::array<card, 12>, 4> hands;
  auto assign_cards_to_player = [&](int n) {
    std::copy_n(cards.begin() + n * 12, 12, hands[n].begin());
    for (card& c : hands[n]) {
      c = card(c.color(), c.face(), player_id(n));
    }
  };
  assign_cards_to_player(0);
  assign_cards_to_player(1);
  assign_cards_to_player(2);
  assign_cards_to_player(3);
  return hands;
}

class uct_tree {
public:
  struct nodes {
    std::vector<doko::action> actions{};
    std::vector<std::ptrdiff_t> parents{};
    std::vector<static_vector<std::ptrdiff_t, 13>> children{};
    std::vector<std::ptrdiff_t> num_visits{};
    std::vector<std::array<double, 4>> eyes{};
    std::vector<std::array<double, 4>> scores{};
  };

  uct_tree(const initial_game_state& state,
           span<const doko::action> past_actions,
           std::ptrdiff_t max_rollouts = 0)
      : past_actions_(past_actions.begin(), past_actions.end()),
        initial_game_state_{state} {
    nodes_.actions.reserve(max_rollouts + 1);
    nodes_.parents.reserve(max_rollouts + 1);
    nodes_.children.reserve(max_rollouts + 1);
    nodes_.num_visits.reserve(max_rollouts + 1);
    nodes_.eyes.reserve(max_rollouts + 1);
    nodes_.scores.reserve(max_rollouts + 1);
    // Set Root Element
    nodes_.actions.push_back(doko::action{});
    nodes_.parents.push_back(0);
    nodes_.children.push_back({});
    nodes_.num_visits.push_back(0);
    nodes_.eyes.push_back({});
    nodes_.scores.push_back({});
  }

  // Observers

  const initial_game_state& initial_state() const noexcept {
    return initial_game_state_;
  }

  std::ptrdiff_t size() const noexcept { return nodes_.actions.size(); }

  player_id player(std::ptrdiff_t n) const noexcept {
    return nodes_.actions[n].player();
  }

  std::ptrdiff_t num_visits(std::ptrdiff_t n) const noexcept {
    return nodes_.num_visits[n];
  }

  doko::action action(std::ptrdiff_t n) const noexcept {
    return nodes_.actions[n];
  }

  std::ptrdiff_t parent(std::ptrdiff_t n) const noexcept {
    return nodes_.parents[n];
  }

  span<const std::ptrdiff_t> children(std::ptrdiff_t n) const noexcept {
    return nodes_.children[n];
  }

  const std::array<double, 4>& eyes(std::ptrdiff_t n) const noexcept {
    return nodes_.eyes[n];
  }

  const std::array<double, 4>& scores(std::ptrdiff_t n) const
      noexcept {
    return nodes_.scores[n];
  }

  double expected_eyes(std::ptrdiff_t n, player_id p) const noexcept {
    return nodes_.eyes[n][to_integer(p)] / num_visits(n);
  }

  double expected_score(std::ptrdiff_t n, player_id p) const noexcept {
    return nodes_.scores[n][to_integer(p)] / num_visits(n);
  }

  // Modifier

  void reset(const initial_game_state& state,
             span<const doko::action> past_actions) {
    past_actions_.clear();
    nodes_.actions.clear();
    nodes_.parents.clear();
    nodes_.children.clear();
    nodes_.num_visits.clear();
    nodes_.eyes.clear();
    nodes_.scores.clear();
    // Construct tree members
    past_actions_.insert(past_actions_.end(), past_actions.begin(),
                         past_actions.end());
    initial_game_state_ = state;
    // Set Root Element
    nodes_.actions.push_back(doko::action{});
    nodes_.parents.push_back(0);
    nodes_.children.push_back({});
    nodes_.num_visits.push_back(0);
    nodes_.eyes.push_back({});
    nodes_.scores.push_back({});
  }

  template <typename RandomNumberGenerator, typename Rules>
  void rollout_once(RandomNumberGenerator& generator, const Rules& rules) {
    static_vector<doko::action, 58> actions(past_actions_.begin(),
                                            past_actions_.end());
    const auto [selected, history, state] =
        select_child_to_expand(actions, rules);
    expand(selected, state, history, rules);
    span<const doko::action> rollout =
        doko::rollout(rules, actions, state, generator);
    update_weights(selected, state, rollout, rules);
  }

private:
  struct selected_return {
    std::ptrdiff_t selected;
    span<const doko::action> history;
    running_game_state state;
  };

  bool are_valid_hands(const std::array<static_vector<card, 12>, 4>& hands) {
    int min = 12;
    int max = 0;
    for (int i = 1; i < 4; ++i) {
      min = std::min(min, hands[i].size());
      max = std::max(max, hands[i].size());
    }
    return (max - min) <= 1;
  }

  template <typename Rules>
  selected_return
  select_child_to_expand(static_vector<doko::action, 58>& actions,
                         const Rules& rules) {
    running_game_state state = current_state(rules, initial_state(), actions);
    std::ptrdiff_t selected = 0;
    while (nodes_.children[selected].size() > 0) {
      FUB_ASSERT(are_valid_hands(state.hands));
      selected = *max_element(
          nodes_.children[selected], std::less<>{},
          [&](std::ptrdiff_t child_id) {
            if (nodes_.num_visits[child_id] == 0) {
              return std::numeric_limits<double>::infinity();
            }
            const player_id this_player = this->player(child_id);
            const double avg_eyes = expected_eyes(child_id, this_player);
            const double avg_score = expected_score(child_id, this_player);
            const double normalized_eyes = avg_eyes / 240.0;
            const double weight = normalized_eyes + avg_score;
            constexpr double c = 4.0;
            const double visit_ratio =
                std::log(static_cast<double>(nodes_.num_visits[selected])) /
                nodes_.num_visits[child_id];
            return weight + c * std::sqrt(visit_ratio);
          });
      state.player = observe_action(rules, state.trick, state.hands,
                                    nodes_.actions[selected], actions);
      actions.push_back(nodes_.actions[selected]);
    }
    return {selected, actions, state};
  }

  template <typename Rules>
  void expand(std::ptrdiff_t selected, const running_game_state& state,
              span<const doko::action> history, const Rules& rules) {
    static_vector<doko::action, 13> followups = rules.legal_actions(
        state.hands[int(state.player)], state.trick, history);
    std::ptrdiff_t count = nodes_.actions.size();
    fub::span considered{followups};
    if (considered.size() > 0 && considered[0].as_bid()) {
      considered = considered.subspan(1);
    }
    for (doko::action action : considered) {
      nodes_.children[selected].push_back(count++);
      nodes_.actions.push_back(action);
      nodes_.parents.push_back(selected);
      nodes_.children.push_back({});
      nodes_.num_visits.push_back(0);
      nodes_.eyes.push_back({});
      nodes_.scores.push_back({});
    }
  }

  template <typename Rules>
  void update_weights(std::ptrdiff_t selected, const running_game_state& state,
                      span<const doko::action> rollout, const Rules& rules) {
    auto score_state = rules.compute_score_state(rollout);
    auto scores = rules.compute_scores(score_state);
    std::ptrdiff_t parent = nodes_.parents[selected];
    auto get_eyes = [&](int p) {
      return score_state
          .party_to_eyes[to_integer(score_state.player_to_party[p])];
    };
    while (parent != selected) {
      nodes_.num_visits[selected] += 1;
      std::transform(scores.begin(), scores.end(),
                     nodes_.scores[selected].begin(),
                     nodes_.scores[selected].begin(), std::plus<>{});
      const std::array<int, 4> eyes{get_eyes(0), get_eyes(1), get_eyes(2),
                                    get_eyes(3)};
      std::transform(eyes.begin(), eyes.end(), nodes_.eyes[selected].begin(),
                     nodes_.eyes[selected].begin(), std::plus<>{});
      selected = parent;
      parent = nodes_.parents[parent];
    }
    nodes_.num_visits[0] += 1;
    std::transform(scores.begin(), scores.end(),
                   nodes_.scores[selected].begin(),
                   nodes_.scores[selected].begin(), std::plus<>{});
    const std::array<int, 4> eyes{get_eyes(0), get_eyes(1), get_eyes(2),
                                  get_eyes(3)};
    std::transform(eyes.begin(), eyes.end(), nodes_.eyes[selected].begin(),
                   nodes_.eyes[selected].begin(), std::plus<>{});
  }

private:
  std::vector<doko::action> past_actions_;
  initial_game_state initial_game_state_;
  nodes nodes_;
};

} // namespace doko

#endif // DOKO_UCT_TREE_SEARCH_HPP