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

#include "doko/game_rules.hpp"
#include "doko/static_vector.hpp"

#include <algorithm>
#include <functional>
#include <numeric>

namespace doko {
namespace {
constexpr std::array<int, 24> make_normal_game_trump_ordering() noexcept {
  std::array<int, 24> flags{};
  int counter = 1;
  flags[to_integer(color::diamonds, face::nine)] = counter++;
  flags[to_integer(color::diamonds, face::king)] = counter++;
  flags[to_integer(color::diamonds, face::ten)] = counter++;
  flags[to_integer(color::diamonds, face::ace)] = counter++;
  flags[to_integer(color::diamonds, face::jack)] = counter++;
  flags[to_integer(color::hearts, face::jack)] = counter++;
  flags[to_integer(color::spades, face::jack)] = counter++;
  flags[to_integer(color::clubs, face::jack)] = counter++;
  flags[to_integer(color::diamonds, face::queen)] = counter++;
  flags[to_integer(color::hearts, face::queen)] = counter++;
  flags[to_integer(color::spades, face::queen)] = counter++;
  flags[to_integer(color::clubs, face::queen)] = counter++;
  flags[to_integer(color::hearts, face::ten)] = counter++;
  return flags;
}

constexpr const std::array<int, 24> normal_game_trump_ordering_ =
    make_normal_game_trump_ordering();

int trump_order(card card) noexcept {
  return normal_game_trump_ordering_[to_integer(card.color(), card.face())];
}

bool less_trump(card lhs, card rhs) {
  return trump_order(lhs) < trump_order(rhs);
}

struct less_normal_game {
  bool operator()(card lhs, card rhs) noexcept {
    if (trump_order(lhs)) {
      return trump_order(rhs) ? less_trump(lhs, rhs) : false;
    }
    if (trump_order(rhs)) {
      return true;
    }
    return lhs.color() == rhs.color() ? lhs.eyes() < rhs.eyes() : false;
  }
};
} // namespace

bool normal_game_rules::is_trump(card card) noexcept {
  return trump_order(card);
}

const card*
normal_game_rules::find_winner(fub::span<const card, 4> trick,
                               fub::span<const action> actions) noexcept {
  const card* max =
      std::max_element(trick.begin(), trick.end(), less_normal_game{});
  const std::size_t n_cards =
      std::count_if(actions.begin(), actions.end(),
                    [](action action) { return action.as_card(); });
  if (n_cards <= 36 && *max == card(color::hearts, face::ten)) {
    const card* pos =
        std::find(max + 1, trick.end(), card(color::hearts, face::ten));
    if (pos != trick.end()) {
      max = pos;
    }
  }
  return max;
}

party normal_game_rules::initial_party(
    fub::span<const card, 12> initial_hand) noexcept {
  const bool contains_old_lady =
      std::find(initial_hand.begin(), initial_hand.end(),
                card(color::clubs, face::queen)) != initial_hand.end();
  return contains_old_lady ? party::re : party::contra;
}

party normal_game_rules::current_party(fub::span<const card, 12> initial_hand,
                                       fub::span<const action>) noexcept {
  return normal_game_rules::initial_party(initial_hand);
}

party normal_game_rules::observed_party(
    player_id this_player, fub::span<const action> actions) noexcept {
  for (action action : actions) {
    if (const announcement* bid = action.as_bid();
        bid && action.player() == this_player) {
      return bid->party();
    }
    if (const card* card = action.as_card()) {
      const doko::card old_lady(color::clubs, face::queen, this_player);
      if (old_lady == *card) {
        return party::re;
      }
    }
  }
  return party::contra;
}

namespace {
template <typename Rules, typename CardRange>
static_vector<card, 12> filter_color(const Rules& rules, CardRange&& hand,
                                     color color) {
  static_vector<card, 12> colored{};
  for (card card : hand) {
    if (!rules.is_trump(card) && card.color() == color) {
      colored.push_back(card);
    }
  }
  return colored;
}

template <typename Rules, typename CardRange>
static_vector<card, 12> filter_trump(const Rules& rules, CardRange&& hand) {
  static_vector<card, 12> trumps{};
  for (card card : hand) {
    if (rules.is_trump(card)) {
      trumps.push_back(card);
    }
  }
  return trumps;
}
} // namespace

static_vector<action, 13>
normal_game_rules::legal_actions(span<const card> hand, span<const card> trick,
                                 span<const action> history) noexcept {
  static_vector<action, 13> actions{};
  const player_id player = hand[0].player();
  const party party =
      normal_game_rules::initial_party(initial_hand(player, hand, history));
  const int n_bids =
      std::count_if(history.begin(), history.end(), [=](action action) {
        const announcement* bid = action.as_bid();
        return bid && bid->party() == party;
      });
  if (n_bids < 5 && hand.size() > 10 - n_bids) {
    actions.push_back(announcement{party, player});
  }
  if (trick.size() == 0) {
    actions.emplace_back(hand);
    return actions;
  }
  static_vector<card, 12> cards =
      is_trump(trick[0])
          ? filter_trump(normal_game_rules(), hand)
          : filter_color(normal_game_rules(), hand, trick[0].color());
  if (cards.size() > 0) {
    actions.emplace_back(cards);
  } else {
    actions.emplace_back(hand);
  }
  return actions;
}

namespace {
template <typename Rules>
score_state compute_score_state(const Rules& rules,
                                fub::span<const action> history) noexcept {
  score_state state;
  state.player_to_party =
      std::array<party, 4>{rules.observed_party(player_id(0), history),
                           rules.observed_party(player_id(1), history),
                           rules.observed_party(player_id(2), history),
                           rules.observed_party(player_id(3), history)};
  static_vector<card, 4> trick{};
  const action* first = history.begin();
  auto party_of = [&](player_id player) {
    return int(state.player_to_party[int(player)]);
  };
  for (const action* action = first; action != history.end(); ++action) {
    player_id player = action->player();
    if (const card* c = action->as_card()) {
      trick.push_back(*c);
      if (trick.size() < 4) {
        player = next_player(player);
      } else {
        player = rules.find_winner(trick, {first, action + 1})->player();
        const int trick_value =
            std::accumulate(trick.begin(), trick.end(), 0,
                            [](int eyes, card c) { return eyes + c.eyes(); });
        state.party_to_eyes[party_of(player)] += trick_value;
        // Bonus Point Doppelkopf
        if (trick_value >= 40) {
          state.party_to_bonus_points[party_of(player)] += 1;
        }
        // Bonus Point Fuchs
        if (const card* fuchs = std::find(trick.begin(), trick.end(),
                                          card(color::diamonds, face::ace));
            (fuchs != trick.end()) &&
            (party_of(fuchs->player()) != party_of(player))) {
          state.party_to_bonus_points[party_of(player)] += 1;
        }
        // Bonus Point Charlie
        if (const card* charlie = std::find(trick.begin(), trick.end(),
                                            card(color::clubs, face::jack));
            (charlie != trick.end()) &&
            (party_of(charlie->player()) != party_of(player)) &&
            (history.end() - action <= 4)) {
          state.party_to_bonus_points[party_of(player)] += 1;
        }
        trick.clear();
      }
    }
    if (const announcement* bid = action->as_bid()) {
      const int party = int(bid->party());
      state.party_to_num_bids[party] += 1;
      const int n_bids = state.party_to_num_bids[party];
      state.party_to_min_points[party] = 121 + 30 * (n_bids - 1);
      const int opponent = (party + 1) % 2;
      if (state.party_to_num_bids[opponent] == 0) {
        state.party_to_min_points[opponent] = 120 - 30 * (n_bids - 1);
      }
    }
  }
  return state;
}

std::array<int, 4> compute_scores(const score_state& state) {
  constexpr int contra = int(party::contra);
  constexpr int re = int(party::re);
  std::array<int, 4> scores_per_player{};
  auto party_of = [&](player_id player) {
    return int(state.player_to_party[int(player)]);
  };
  int points = 0;
  if (state.party_to_min_points[contra] <= state.party_to_eyes[contra]) {
    const int diff =
        state.party_to_eyes[contra] - state.party_to_min_points[contra];
    points +=
        2 * std::max(0, state.party_to_num_bids[contra] - 1) + 2 + diff / 30;
    if (state.party_to_num_bids[contra] > 0) {
      points *= 2;
    }
    if (state.party_to_num_bids[re] > 0) {
      points *= 2;
    }
    auto Score = [&](player_id player) {
      return party_of(player) == contra ? points : -points;
    };
    scores_per_player[0] = Score(player_id(0));
    scores_per_player[1] = Score(player_id(1));
    scores_per_player[2] = Score(player_id(2));
    scores_per_player[3] = Score(player_id(3));
  }
  if (state.party_to_min_points[re] <= state.party_to_eyes[re]) {
    const int diff = state.party_to_eyes[re] - state.party_to_min_points[re];
    points += 2 * std::max(0, state.party_to_num_bids[re] - 1) + 1 + diff / 30;
    if (state.party_to_num_bids[contra] > 0) {
      points *= 2;
    }
    if (state.party_to_num_bids[re] > 0) {
      points *= 2;
    }
    auto Score = [&](player_id player) {
      return party_of(player) == re ? points : -points;
    };
    scores_per_player[0] = Score(player_id(0));
    scores_per_player[1] = Score(player_id(1));
    scores_per_player[2] = Score(player_id(2));
    scores_per_player[3] = Score(player_id(3));
  }
  const int bonus_diff =
      state.party_to_bonus_points[re] - state.party_to_bonus_points[contra];
  auto AddBonus = [&](player_id player) {
    scores_per_player[int(player)] +=
        party_of(player) == int(party::re) ? bonus_diff : -bonus_diff;
  };
  AddBonus(player_id(0));
  AddBonus(player_id(1));
  AddBonus(player_id(2));
  AddBonus(player_id(3));
  if (std::count(state.player_to_party.begin(), state.player_to_party.end(),
                 party::re) == 1) {
    for (int p = 0; p < 3; ++p) {
      if (state.player_to_party[p] == party::re) {
        scores_per_player[p] *= 3;
      }
    }
  }
  return scores_per_player;
}
} // namespace

score_state
normal_game_rules::compute_score_state(span<const action> history) noexcept {
  return ::doko::compute_score_state(normal_game_rules(), history);
}

std::array<int, 4>
normal_game_rules::compute_scores(const score_state& state) noexcept {
  return ::doko::compute_scores(state);
}

score_state
marriage_rules::compute_score_state(span<const action> history) const noexcept {
  return ::doko::compute_score_state(*this, history);
}

std::array<int, 4>
marriage_rules::compute_scores(const score_state& state) const noexcept {
  return ::doko::compute_scores(state);
}

party marriage_rules::observed_party(player_id this_player,
                                     span<const action> actions) const
    noexcept {
  if (this_player == bride) {
    return party::re;
  }
  static_vector<card, 4> trick{};
  int count = 0;
  int trick_count = 0;
  for (action a : actions) {
    player_id winner =
        observe_action(*this, trick, a, actions.subspan(0, count));
    if (trick.size() == 0) {
      if (winner != bride && trick_count < 3) {
        return winner == this_player ? party::re : party::contra;
      }
      ++trick_count;
    }
  }
  return party::contra;
}

score_state solo_rules::compute_score_state(span<const action> history) const
    noexcept {
  score_state state;
  state.player_to_party =
      std::array<party, 4>{observed_party(player_id(0), history),
                           observed_party(player_id(1), history),
                           observed_party(player_id(2), history),
                           observed_party(player_id(3), history)};
  static_vector<card, 4> trick{};
  const action* first = history.begin();
  auto party_of = [&](player_id player) {
    return int(state.player_to_party[int(player)]);
  };
  for (const action* action = first; action != history.end(); ++action) {
    player_id player = action->player();
    if (const card* c = action->as_card()) {
      trick.push_back(*c);
      if (trick.size() < 4) {
        player = next_player(player);
      } else {
        player = find_winner(trick, {first, action + 1})->player();
        const int trick_value =
            std::accumulate(trick.begin(), trick.end(), 0,
                            [](int eyes, card c) { return eyes + c.eyes(); });
        state.party_to_eyes[party_of(player)] += trick_value;
        trick.clear();
      }
    }
    if (const announcement* bid = action->as_bid()) {
      const int party = int(bid->party());
      state.party_to_num_bids[party] += 1;
      const int n_bids = state.party_to_num_bids[party];
      state.party_to_min_points[party] = 121 + 30 * (n_bids - 1);
      const int opponent = (party + 1) % 2;
      if (state.party_to_num_bids[opponent] == 0) {
        state.party_to_min_points[opponent] = 120 - 30 * (n_bids - 1);
      }
    }
  }
  return state;
}

std::array<int, 4> solo_rules::compute_scores(const score_state& state) const
    noexcept {
  return ::doko::compute_scores(state);
}

namespace {
constexpr std::array<int, 24>
make_solo_game_trump_ordering(solo_type type) noexcept {
  std::array<int, 24> flags{};
  int counter = 1;
  auto color_trumps = [&](color c) {
    flags[to_integer(c, face::nine)] = counter++;
    flags[to_integer(c, face::king)] = counter++;
    flags[to_integer(c, face::ten)] = counter++;
    flags[to_integer(c, face::ace)] = counter++;
    flags[to_integer(color::diamonds, face::jack)] = counter++;
    flags[to_integer(color::hearts, face::jack)] = counter++;
    flags[to_integer(color::spades, face::jack)] = counter++;
    flags[to_integer(color::clubs, face::jack)] = counter++;
    flags[to_integer(color::diamonds, face::queen)] = counter++;
    flags[to_integer(color::hearts, face::queen)] = counter++;
    flags[to_integer(color::spades, face::queen)] = counter++;
    flags[to_integer(color::clubs, face::queen)] = counter++;
    flags[to_integer(color::hearts, face::ten)] = counter++;
  };
  switch (type) {
  case solo_type::jack:
    flags[to_integer(color::diamonds, face::jack)] = counter++;
    flags[to_integer(color::hearts, face::jack)] = counter++;
    flags[to_integer(color::spades, face::jack)] = counter++;
    flags[to_integer(color::clubs, face::jack)] = counter++;
    break;
  case solo_type::queen:
    flags[to_integer(color::diamonds, face::queen)] = counter++;
    flags[to_integer(color::hearts, face::queen)] = counter++;
    flags[to_integer(color::spades, face::queen)] = counter++;
    flags[to_integer(color::clubs, face::queen)] = counter++;
    break;
  case solo_type::diamonds:
    color_trumps(color::diamonds);
    break;
  case solo_type::hearts:
    color_trumps(color::hearts);
    break;
  case solo_type::spades:
    color_trumps(color::spades);
    break;
  case solo_type::clubs:
    color_trumps(color::clubs);
    break;
  }
  return flags;
}

static std::array<std::array<int, 24>, 6> solo_trumps{
    make_solo_game_trump_ordering(solo_type::jack),
    make_solo_game_trump_ordering(solo_type::queen),
    make_solo_game_trump_ordering(solo_type::diamonds),
    make_solo_game_trump_ordering(solo_type::hearts),
    make_solo_game_trump_ordering(solo_type::spades),
    make_solo_game_trump_ordering(solo_type::clubs)};

struct less_solo {
  solo_type type;

  int trump_ordering(card c) const noexcept {
    return solo_trumps[int(type)][to_integer(c)];
  }

  bool is_trump(card c) const noexcept { return trump_ordering(c); }

  bool less(card c1, card c2) const noexcept {
    return trump_ordering(c1) < trump_ordering(c2);
  }

  bool operator()(card lhs, card rhs) noexcept {
    if (is_trump(lhs)) {
      return is_trump(rhs) ? less(lhs, rhs) : false;
    }
    if (is_trump(rhs)) {
      return true;
    }
    return lhs.color() == rhs.color() ? lhs.eyes() < rhs.eyes() : false;
  }
};
} // namespace

const card* solo_rules::find_winner(span<const card, 4> trick,
                                    span<const action>) const noexcept {
  return std::max_element(trick.begin(), trick.end(), less_solo{type});
}

bool solo_rules::is_trump(card card) const noexcept {
  return less_solo{type}.is_trump(card);
}

static_vector<action, 13>
solo_rules::legal_actions(span<const card> hand, span<const card> trick,
                          span<const action> history) const noexcept {
  static_vector<action, 13> actions{};
  const player_id player = hand[0].player();
  const party party = observed_party(player, history);
  const int n_bids =
      std::count_if(history.begin(), history.end(), [=](action action) {
        const announcement* bid = action.as_bid();
        return bid && bid->party() == party;
      });
  if (n_bids < 5 && hand.size() > 10 - n_bids) {
    actions.push_back(announcement{party, player});
  }
  if (trick.size() == 0) {
    actions.emplace_back(hand);
    return actions;
  }
  static_vector<card, 12> cards =
      is_trump(trick[0]) ? filter_trump(*this, hand)
                         : filter_color(*this, hand, trick[0].color());
  if (cards.size() > 0) {
    actions.emplace_back(cards);
  } else {
    actions.emplace_back(hand);
  }
  return actions;
}

game_state_machine::game_state_machine(
    player_id first, const std::array<std::array<card, 12>, 4>& hands)
    : first_player{first}, state{declare_contracts{{}, {first, hands}}} {}

void game_state_machine::choose(declared_contract contract) {
  std::visit(
      overload_set{
          [](const auto&) { throw invalid_state("choose"); },
          [&](declare_contracts& contracts) {
            player_id player = contract.player;
            if (contracts.state.player != player) {
              throw not_next_player(player);
            }
            contracts.choices[to_integer(player)] = contract.health;
            contracts.state.player = next_player(player);
            // If all choices are made we have to advance the internal
            // state to be running.
            if (contracts.choices[to_integer(contracts.state.player)]) {
              specialize_contracts specialize{};
              std::transform(
                  contracts.choices.begin(), contracts.choices.end(),
                  specialize.choices.begin(),
                  [](const std::optional<healthiness>& health) {
                    return health == healthiness::healthy
                               ? std::optional<game_rules>{normal_game_rules{}}
                               : std::optional<game_rules>{};
                  });
              if (std::all_of(specialize.choices.begin(),
                              specialize.choices.end(),
                              [](const auto& optional) { return optional; })) {
                running_game_state state;
                state.player = contracts.state.player;
                state.trick = static_vector<card, 4>{};
                state.hands = to_static_vectors(contracts.state.hands);
                this->state = running{normal_game_rules{}, state, {}};
              } else {
                player_id next_player = contracts.state.player;
                while (specialize.choices[to_integer(next_player)]) {
                  next_player = doko::next_player(next_player);
                }
                specialize.state.player = next_player;
                specialize.state.hands = contracts.state.hands;
                this->state = specialize;
              }
            }
          }},
      state);
}
void game_state_machine::choose(specialized_contract contract) {
  std::visit(
      overload_set{
          [](const auto&) { throw invalid_state("choose"); },
          [&](specialize_contracts& contracts) {
            player_id next_player = contracts.state.player;
            while (contracts.choices[to_integer(next_player)]) {
              next_player = doko::next_player(next_player);
            }
            if (next_player != contract.player) {
              throw not_next_player(contract.player);
            }
            contracts.choices[to_integer(next_player)] = contract.rules;
            // If all choices are made we have to advance the internal
            // state to be running.
            if (std::all_of(contracts.choices.begin(), contracts.choices.end(),
                            [](const auto& optional) { return optional; })) {
              const std::optional<game_rules>* max = std::max_element(
                  contracts.choices.begin(), contracts.choices.end(),
                  [](const std::optional<game_rules>& lhs,
                     const std::optional<game_rules>& rhs) {
                    return lhs->index() < rhs->index();
                  });
              running_game_state state;
              state.player = this->first_player;
              state.trick = static_vector<card, 4>{};
              state.hands = to_static_vectors(contracts.state.hands);
              if (const solo_rules* solo_rules =
                      std::get_if<doko::solo_rules>(&(**max))) {
                state.player = solo_rules->solo_player;
              } else {
                this->first_player = doko::next_player(this->first_player);
              }
              this->state = running{**max, state, {}};
            } else {
              contracts.state.player = doko::next_player(next_player);
            }
          }},
      state);
}

void game_state_machine::play(action action) {
  std::visit(
      overload_set{
          [](const auto&) { throw invalid_state("play"); },
          [&](running& running) {
            running.state.player =
                observe_action(running.rules, running.state.trick,
                               running.state.hands, action, running.actions);
            running.actions.push_back(action);
            if (std::all_of(running.state.hands.begin(),
                            running.state.hands.end(),
                            [](const static_vector<card, 12>& hand) {
                              return hand.size() == 0;
                            })) {
              auto [scores, eyes] = std::visit(
                  [&](auto r) {
                    const score_state state =
                        r.compute_score_state(running.actions);
                    auto party = [&](int n) {
                      return to_integer(state.player_to_party[n]);
                    };
                    const std::array<int, 4> player_to_eyes{
                        state.party_to_eyes[party(0)],
                        state.party_to_eyes[party(1)],
                        state.party_to_eyes[party(2)],
                        state.party_to_eyes[party(3)]};
                    return std::pair{r.compute_scores(state), player_to_eyes};
                  },
                  running.rules);
              this->state = score{eyes, scores, running.actions};
            }
          }},
      state);
}

void game_state_machine::next_game(
    const std::array<std::array<card, 12>, 4>& hands) {
  std::visit(
      overload_set{
          [](const auto&) { throw invalid_state("next_game"); },
          [&](const score& running) {
            this->state = declare_contracts{{}, {this->first_player, hands}};
          }},
      state);
}

} // namespace doko