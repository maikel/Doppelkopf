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

#include "doko/card_assignment.hpp"

namespace doko {
int count_remaining_slots(const assign_cards_state& state,
                          player_id player) noexcept {
  return 12 - std::accumulate(
                  state.card_to_players.begin(), state.card_to_players.end(), 0,
                  [=](int sum, const static_vector<player_id, 2>& players) {
                    return sum +
                           std::count(players.begin(), players.end(), player);
                  });
}

std::array<std::array<card, 12>, 4>
make_assignment(const std::array<static_vector<player_id, 2>, 24>&
                    card_to_players) noexcept {
  std::array<static_vector<card, 12>, 4> vectors;
  for (int c = 0; c < 24; ++c) {
    FUB_ASSERT(card_to_players[c].size() == 2);
    vectors[to_integer(card_to_players[c][0])].push_back(to_card(c));
    vectors[to_integer(card_to_players[c][1])].push_back(to_card(c));
  }
  std::array<std::array<card, 12>, 4> assignment;
  for (int p = 0; p < 4; ++p) {
    FUB_ASSERT(vectors[p].size() == 12);
    std::transform(vectors[p].begin(), vectors[p].end(), assignment[p].begin(),
                   [p](doko::card c) {
                     return doko::card(c.color(), c.face(), player_id(p));
                   });
  }
  return assignment;
}

static_vector<player_id, 4>
make_candidate_set(const std::array<int, 4>& flags) noexcept {
  static_vector<player_id, 4> candidate_set;
  for (int p = 0; p < 4; ++p) {
    if (flags[p]) {
      candidate_set.push_back(player_id(p));
    }
  }
  return candidate_set;
}

static_vector<card, 48> filter_possible_cards(const assign_cards_state& state,
                                              player_id player) noexcept {
  static_vector<card, 48> remaining;
  const int p = to_integer(player);
  for (doko::card card : state.remaining_cards) {
    if (state.card_to_candidates[to_integer(card)][p]) {
      remaining.push_back(card);
    }
  }
  return remaining;
}

void assign_card(assign_cards_state& state, player_id player,
                 doko::card card) noexcept {
  const int c = to_integer(card);
  const int p = to_integer(player);
  FUB_ASSERT(state.card_to_candidates[c][p]);
  FUB_ASSERT(state.card_to_players[c].size() < 2);
  auto remaining_pos =
      std::find(state.remaining_cards.rbegin(), state.remaining_cards.rend(),
                doko::card(card.color(), card.face()));
  FUB_ASSERT(remaining_pos != state.remaining_cards.rend());
  const int remaining_slots = count_remaining_slots(state, player);
  FUB_ASSERT(remaining_slots > 0);
  auto remaining_last = state.remaining_cards.rbegin();
  std::iter_swap(remaining_pos, remaining_last);
  state.remaining_cards.pop_back();
  state.card_to_players[c].push_back(player);
  if (state.card_to_players[c].size() == 2) {
    state.card_to_candidates[c].fill(0);
  }
  if (remaining_slots == 1) {
    for (std::array<int, 4>& flags : state.card_to_candidates) {
      flags[p] = 0;
    }
  }
  if (card == doko::card(color::clubs, face::queen)) {
    auto pos = std::find(state.players_who_need_clubs_queen.begin(),
                         state.players_who_need_clubs_queen.end(), player);
    if (pos != state.players_who_need_clubs_queen.end()) {
      std::iter_swap(pos, state.players_who_need_clubs_queen.end() - 1);
      state.players_who_need_clubs_queen.pop_back();
    }
  }
}

bool assign_if_unique_card_to_candidate(assign_cards_state& state) {
  int card_n = 0;
  for (const std::array<int, 4>& candidate_set : state.card_to_candidates) {
    if (std::count(candidate_set.begin(), candidate_set.end(), 1) == 1) {
      const player_id player = player_id(std::distance(
          candidate_set.begin(),
          std::find(candidate_set.begin(), candidate_set.end(), 1)));
      assign_card(state, player, to_card(card_n));
      return true;
    }
    ++card_n;
  }
  return false;
}

bool assign_if_unique_candidate_to_cards(assign_cards_state& state) {
  // Now check the uniqueness from the other point of view
  for (int p = 0; p < 4; ++p) {
    const player_id player = player_id(p);
    const int remaining_slots = count_remaining_slots(state, player);
    if (remaining_slots) {
      const static_vector<card, 48> possible_cards =
          filter_possible_cards(state, player);
      if (possible_cards.size() == remaining_slots) {
        for (doko::card card : possible_cards) {
          assign_card(state, player, card);
        }
        return true;
      }
    }
  }
  return false;
}

bool assign_if_clubs_queen_required(assign_cards_state& state) {
  if (!state.players_who_need_clubs_queen.empty()) {
    assign_card(state, state.players_who_need_clubs_queen[0],
                card(color::clubs, face::queen));
    return true;
  }
  return false;
}

} // namespace doko