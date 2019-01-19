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
#include "doko/game_rules.hpp"

#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"

#include <algorithm>
#include <array>
#include <numeric>
#include <random>

using namespace doko;

using Assignment = std::array<std::array<card, 12>, 4>;

bool is_valid_assignment(const Assignment& assignment) {
  std::array<int, 24> card_counts{};
  for (int player = 0; player < 4; ++player) {
    for (card card : assignment[player]) {
      if (card.player() != player_id(player)) {
        return false;
      }
      card_counts[to_integer(card.color(), card.face())] += 1;
    }
  }
  return std::all_of(card_counts.begin(), card_counts.end(),
                     [](int count) { return count == 2; });
}

template <typename Generator>
std::array<card, 12> make_random_hand(Generator& gen) {
  std::array<int, 48> cards;
  std::iota(cards.begin(), cards.end(), 0);
  std::shuffle(cards.begin(), cards.end(), gen);
  std::array<card, 12> hand;
  std::transform(cards.begin(), cards.begin() + 12, hand.begin(),
                 [](int n) { return card(color((n / 6) % 4), face(n % 6)); });
  return hand;
}

template <typename T>
static_vector<T, 12> Tostatic_vector(const std::array<T, 12>& array) {
  static_vector<T, 12> vec{};
  for (const T& x : array) {
    vec.push_back(x);
  }
  return vec;
}

TEST_CASE("At Start of a Game") {
  const std::array<card, 12> hand{
      card(color::clubs, face::queen),    card(color::clubs, face::queen),
      card(color::diamonds, face::queen), card(color::diamonds, face::queen),
      card(color::hearts, face::jack),    card(color::clubs, face::ten),
      card(color::spades, face::ace),     card(color::spades, face::king),
      card(color::spades, face::king),    card(color::spades, face::nine),
      card(color::hearts, face::nine),    card(color::hearts, face::nine)};

  auto assign_to_player = [&hand](player_id player) {
    assign_cards_state state = make_assign_cards_state(
        normal_game_rules{}, hand, span<const action>{});
    std::mt19937 generator{2019};
    Assignment assignment = assign_cards_randomly(state, generator);
    REQUIRE(is_valid_assignment(assignment));
  };

  SECTION("As player 1") { assign_to_player(player_id::first); }
  SECTION("As player 2") { assign_to_player(player_id::second); }
  SECTION("As player 3") { assign_to_player(player_id::third); }
  SECTION("As player 4") { assign_to_player(player_id::fourth); }
}

TEST_CASE("After one played card") {
  const std::array<card, 12> hand{
      card(color::clubs, face::queen),    card(color::clubs, face::queen),
      card(color::diamonds, face::queen), card(color::diamonds, face::queen),
      card(color::hearts, face::jack),    card(color::clubs, face::ten),
      card(color::spades, face::ace),     card(color::spades, face::king),
      card(color::spades, face::king),    card(color::spades, face::nine),
      card(color::hearts, face::nine),    card(color::hearts, face::nine)};

  const std::vector<action> history{
      card(color::spades, face::nine, player_id::first)};

  assign_cards_state state =
      make_assign_cards_state(normal_game_rules{}, hand, history);
  std::mt19937 generator{2019};
  for (int i = 0; i < 100; ++i) {
    Assignment assignment = assign_cards_randomly(state, generator);
    REQUIRE(is_valid_assignment(assignment));
  }
}

TEST_CASE("After the first trick") {
  const std::array<card, 12> hand{
      card(color::clubs, face::queen),    card(color::clubs, face::queen),
      card(color::diamonds, face::queen), card(color::diamonds, face::queen),
      card(color::hearts, face::jack),    card(color::clubs, face::ten),
      card(color::spades, face::ace),     card(color::spades, face::king),
      card(color::spades, face::king),    card(color::spades, face::nine),
      card(color::hearts, face::nine),    card(color::hearts, face::nine)};

  const std::vector<action> history{
      card(color::spades, face::ace, player_id::first),
      card(color::spades, face::ace, player_id::second),
      card(color::diamonds, face::ace, player_id::third),
      card(color::spades, face::nine, player_id::fourth)};

  assign_cards_state state =
      make_assign_cards_state(normal_game_rules{}, hand, history);
  std::mt19937 generator{2019};
  for (int i = 0; i < 100; ++i) {
    Assignment assignment = assign_cards_randomly(state, generator);
    REQUIRE(is_valid_assignment(assignment));
    REQUIRE(std::find(assignment[1].begin(), assignment[1].end(),
                      card(color::spades, face::ace, player_id::second)) !=
            assignment[1].end());
    REQUIRE(std::find(assignment[2].begin(), assignment[2].end(),
                      card(color::diamonds, face::ace, player_id::third)) !=
            assignment[2].end());
    REQUIRE(std::find(assignment[3].begin(), assignment[3].end(),
                      card(color::spades, face::nine, player_id::fourth)) !=
            assignment[3].end());
    bool no_spades =
        std::none_of(assignment[2].begin(), assignment[2].end(), [](card c) {
          return !normal_game_rules::is_trump(c) && c.color() == color::spades;
        });
    REQUIRE(no_spades);
  }
}