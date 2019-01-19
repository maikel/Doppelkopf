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

#ifndef DOKO_GAMERULES_HPP
#define DOKO_GAMERULES_HPP

#include "doko/action.hpp"
#include "doko/static_vector.hpp"

#include "fub/core/span.hpp"

#include <array>
#include <optional>
#include <variant>

namespace doko {
template <typename T>
static_vector<T, 12> to_static_vector(const std::array<T, 12>& array) {
  return static_vector<T, 12>(array.begin(), array.end());
}

inline std::array<static_vector<card, 12>, 4>
to_static_vectors(const std::array<std::array<card, 12>, 4>& array) {
  std::array<static_vector<card, 12>, 4> result;
  std::transform(
      array.begin(), array.end(), result.begin(),
      [](const std::array<card, 12>& arr) { return to_static_vector(arr); });
  return result;
}

using fub::span;

template <typename CardRange>
std::array<card, 12> initial_hand(player_id player, const CardRange& hand,
                                  span<const action> history) {
  std::array<card, 12> initial;
  card* out = std::copy(hand.begin(), hand.end(), initial.begin());
  for (action action : history) {
    if (const card* card = action.as_card(); card && card->player() == player) {
      *out++ = *card;
    }
  }
  return initial;
}

inline void erase_card(static_vector<card, 12>& hand, card c) {
  card* pos = std::find(hand.begin(), hand.end(), c);
  std::swap(*pos, hand[hand.size() - 1]);
  hand.pop_back();
}

struct score_state {
  std::array<party, 4> player_to_party{};
  std::array<int, 2> party_to_eyes{0, 0};
  std::array<int, 2> party_to_num_bids{0, 0};
  std::array<int, 2> party_to_bonus_points{0, 0};
  std::array<int, 2> party_to_min_points{121, 121};
};

struct normal_game_rules {
  static party initial_party(span<const card, 12> initial_hand) noexcept;

  static party current_party(span<const card, 12> initial_hand,
                             span<const action> actions) noexcept;

  static party observed_party(player_id this_player,
                              span<const action> actions) noexcept;

  static bool is_trump(card card) noexcept;

  static const card* find_winner(span<const card, 4> trick,
                                 span<const action> actions) noexcept;

  static static_vector<action, 13>
  legal_actions(span<const card> current_hand, span<const card> trick,
                span<const action> history) noexcept;

  static score_state compute_score_state(span<const action> history) noexcept;

  static std::array<int, 4> compute_scores(const score_state& state) noexcept;
};

struct marriage_rules {
  player_id bride;

  party initial_party(span<const card, 12> initial_hand) const noexcept {
    return normal_game_rules::initial_party(initial_hand);
  }

  party current_party(span<const card, 12> initial_hand,
                      span<const action> actions) const noexcept;

  party observed_party(player_id this_player, span<const action> actions) const
      noexcept;

  bool is_trump(card card) const noexcept {
    return normal_game_rules::is_trump(card);
  }

  const card* find_winner(span<const card, 4> trick,
                          span<const action> actions) const noexcept {
    return normal_game_rules::find_winner(trick, actions);
  }

  static_vector<action, 13> legal_actions(span<const card> current_hand,
                                          span<const card> trick,
                                          span<const action> history) const
      noexcept {
    return normal_game_rules::legal_actions(current_hand, trick, history);
  }

  score_state compute_score_state(span<const action> history) const noexcept;

  std::array<int, 4> compute_scores(const score_state& state) const noexcept;
};

enum class solo_type : unsigned char {
  jack,
  queen,
  diamonds,
  hearts,
  spades,
  clubs
};

struct solo_rules {
  player_id solo_player;
  solo_type type;

  party initial_party(span<const card, 12> initial_hand) const noexcept;

  party current_party(span<const card, 12> initial_hand,
                      span<const action> actions) const noexcept;

  party observed_party(player_id this_player, span<const action> actions) const
      noexcept {
    return this_player == solo_player ? party::re : party::contra;
  }

  bool is_trump(card card) const noexcept;

  const card* find_winner(span<const card, 4> trick,
                          span<const action> actions) const noexcept;

  static_vector<action, 13> legal_actions(span<const card> current_hand,
                                          span<const card> trick,
                                          span<const action> history) const
      noexcept;

  score_state compute_score_state(span<const action> history) const noexcept;

  std::array<int, 4> compute_scores(const score_state& state) const noexcept;
};

template <typename Rules>
player_id observe_action(const Rules& rules, static_vector<card, 4>& trick,
                         action new_action, span<const action> actions) {
  player_id player = new_action.player();
  if (const card* card = new_action.as_card()) {
    trick.push_back(*card);
    if (trick.size() < 4) {
      player = next_player(player);
    } else {
      player = rules.find_winner(trick, actions)->player();
      trick.clear();
    }
  }
  return player;
}

template <typename Rules, typename HandRange>
player_id observe_action(const Rules& rules, static_vector<card, 4>& trick,
                         HandRange& hands, action new_action,
                         span<const action> actions) {
  player_id player = new_action.player();
  if (const card* card = new_action.as_card()) {
    auto&& hand = hands[int(player)];
    erase_card(hand, *card);
    trick.push_back(*card);
    if (trick.size() < 4) {
      player = next_player(player);
    } else {
      player = rules.find_winner(trick, actions)->player();
      trick.clear();
    }
  }
  return player;
}

struct initial_game_state {
  player_id player;
  std::array<std::array<card, 12>, 4> hands;
};

struct running_player_state {
  player_id player;
  static_vector<card, 12> hand;
  static_vector<card, 4> trick;
};

struct running_game_state {
  player_id player;
  std::array<static_vector<card, 12>, 4> hands;
  static_vector<card, 4> trick;
};

template <typename Rules>
running_game_state current_state(const Rules& rules,
                                 const initial_game_state& state,
                                 span<const action> history) {
  running_game_state result;
  result.player = state.player;
  result.hands = to_static_vectors(state.hands);
  int count = 1;
  for (action action : history) {
    result.player = observe_action(rules, result.trick, result.hands, action,
                                   history.subspan(0, count));
    ++count;
  }
  return result;
}

using game_rules = std::variant<normal_game_rules, marriage_rules, solo_rules>;

template <typename HandRange>
player_id observe_action(const game_rules& rules, static_vector<card, 4>& trick,
                         HandRange& hands, action new_action,
                         span<const action> past) {
  return std::visit(
      [&](auto rule) {
        return observe_action(rule, trick, hands, new_action, past);
      },
      rules);
}

enum class healthiness : unsigned char { healthy, reservation };

struct declared_contract {
  player_id player;
  healthiness health;
};

struct specialized_contract {
  player_id player;
  game_rules rules;
};

struct exception : std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct invalid_state : exception {
  using exception::exception;
};

struct not_next_player : exception {
  not_next_player(player_id player)
      : exception("Not Next Player"), player_{player} {}
  player_id player_;
};

struct game_state_machine {
  struct declare_contracts {
    std::array<std::optional<healthiness>, 4> choices;
    initial_game_state state;
  };

  struct specialize_contracts {
    std::array<std::optional<game_rules>, 4> choices;
    initial_game_state state;
  };

  struct running {
    game_rules rules;
    running_game_state state;
    static_vector<action, 58> actions;
  };

  struct score {
    std::array<int, 4> eyes;
    std::array<int, 4> score;
    static_vector<action, 58> actions;
  };

  game_state_machine(player_id first,
                     const std::array<std::array<card, 12>, 4>& hands);

  void choose(declared_contract health);
  void choose(specialized_contract contract);
  void play(action action);
  void next_game(const std::array<std::array<card, 12>, 4>& hands);

  player_id first_player;
  std::variant<declare_contracts, specialize_contracts, running, score> state;
};

} // namespace doko

#endif // DOKO_GAMERULES_HPP
