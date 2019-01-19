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

#ifndef DOKO_CARD_ASSIGNMENT_HPP
#define DOKO_CARD_ASSIGNMENT_HPP

#include "doko/action.hpp"
#include "doko/random.hpp"
#include "doko/static_vector.hpp"

#include <array>
#include <numeric>

namespace doko {

struct assign_cards_state {
  assign_cards_state() noexcept {
    card_to_candidates.fill({1, 1, 1, 1});
    std::array<int, 48> card_nums;
    std::iota(card_nums.begin(), card_nums.end(), 0);
    remaining_cards.resize(48);
    std::transform(card_nums.begin(), card_nums.end(), remaining_cards.begin(),
                   [](int num) { return to_card(num); });
  }

  std::array<static_vector<player_id, 2>, 24> card_to_players;
  std::array<std::array<int, 4>, 24> card_to_candidates;
  static_vector<player_id, 2> players_who_need_clubs_queen;
  static_vector<card, 48> remaining_cards;
};

void assign_card(assign_cards_state& state, player_id, card) noexcept;

static_vector<card, 48> filter_possible_cards(const assign_cards_state& state,
                                              player_id) noexcept;

int count_remaining_slots(const assign_cards_state& state, player_id) noexcept;

std::array<std::array<card, 12>, 4>
make_assignment(const std::array<static_vector<player_id, 2>, 24>&
                    card_to_players) noexcept;

static_vector<player_id, 4>
make_candidate_set(const std::array<int, 4>& flags) noexcept;

template <typename Rules>
void observe_action(const Rules& rules, assign_cards_state& state,
                    static_vector<card, 12>& hand,
                    static_vector<card, 4>& trick, doko::action action) {
  overload_set visitor{
      [&](doko::card card) {
        assign_card(state, card.player(), card);
        if (!trick.empty()) {
          const int p = int(card.player());
          if (rules.is_trump(trick[0]) && !rules.is_trump(card)) {
            for (int i = 0; i < 24; ++i) {
              if (rules.is_trump(to_card(i))) {
                state.card_to_candidates[i][p] = 0;
              }
            }
          } else if (!rules.is_trump(trick[0]) &&
                     (rules.is_trump(card) ||
                      trick[0].color() != card.color())) {
            const color color_of_trick = trick[0].color();
            for (int f = 0; f < 6; ++f) {
              doko::face face = doko::face(f);
              if (!rules.is_trump(doko::card(color_of_trick, face))) {
                const int i = to_integer(color_of_trick, face);
                state.card_to_candidates[i][p] = 0;
              }
            }
          }
        }
        trick.push_back(card);
        if (trick.size() == 4) {
          trick.clear();
        }
        doko::card* pos = std::find(hand.begin(), hand.end(), card);
        if (pos != hand.end()) {
          std::iter_swap(pos, hand.end() - 1);
          hand.pop_back();
        }
      },
      [&](announcement a) {
        const int cq = to_integer(color::clubs, face::queen);
        if (a.party() == party::re) {
          const auto& have_it = state.card_to_players[cq];
          const auto& need_it = state.players_who_need_clubs_queen;
          auto have_pos = std::find(have_it.begin(), have_it.end(), a.player());
          auto need_pos = std::find(need_it.begin(), need_it.end(), a.player());
          if (have_pos != have_it.end() && need_pos != need_it.end()) {
            FUB_ASSERT(have_it.size() < 2);
            FUB_ASSERT(need_it.size() < 2);
            FUB_ASSERT(state.card_to_candidates[cq][int(a.player())] == 1);
            state.players_who_need_clubs_queen.push_back(a.player());
          }
        } else {
          state.card_to_candidates[cq][int(a.player())] = 0;
        }
      }};
  visit(visitor, action);
}

template <typename Rules, typename CardRange, typename ActionRange>
assign_cards_state make_assign_cards_state(const Rules& rules,
                                           const CardRange& hand,
                                           const ActionRange& observed) {
  assign_cards_state state{};
  static_vector<card, 12> cards(hand.begin(), hand.end());
  static_vector<card, 4> trick;
  for (doko::action action : observed) {
    observe_action(rules, state, cards, trick, action);
  }
  for (doko::card card : cards) {
    assign_card(state, card.player(), card);
  }
  return state;
}

bool assign_if_unique_card_to_candidate(assign_cards_state& state);

bool assign_if_unique_candidate_to_cards(assign_cards_state& state);

bool assign_if_clubs_queen_required(assign_cards_state& state);

template <typename RandomNumberGenerator>
std::array<std::array<card, 12>, 4>
assign_cards_randomly(assign_cards_state& state, RandomNumberGenerator& gen) {
  // Assign cards as long as we have not finished.
  while (!state.remaining_cards.empty()) {
    FUB_ASSERT(state.remaining_cards.size() ==
               count_remaining_slots(state, player_id::first) +
                   count_remaining_slots(state, player_id::second) +
                   count_remaining_slots(state, player_id::third) +
                   count_remaining_slots(state, player_id::fourth));

    if (assign_if_unique_card_to_candidate(state) ||
        assign_if_unique_candidate_to_cards(state) ||
        assign_if_clubs_queen_required(state)) {
      continue;
    }

    // Assign one card randomly here and repeat the checks.
    const doko::card last = state.remaining_cards.back();
    const static_vector<player_id, 4> candidate_set =
        make_candidate_set(state.card_to_candidates[to_integer(last)]);
    const player_id player =
        *select_randomly(candidate_set.begin(), candidate_set.end(), gen);
    assign_card(state, player, last);
  }
  return make_assignment(state.card_to_players);
}

} // namespace doko

#endif