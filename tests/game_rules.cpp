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

#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"

TEST_CASE("Normal Game Scoring") {
  using namespace doko;
  score_state state;

  state.player_to_party[0] = party::re;
  state.player_to_party[1] = party::re;
  state.player_to_party[2] = party::contra;
  state.player_to_party[3] = party::contra;

  SECTION("re wins with 121") {
    state.party_to_eyes[to_integer(party::re)] = 121;
    state.party_to_eyes[to_integer(party::contra)] =
        240 - state.party_to_eyes[to_integer(party::re)];
    std::array<int, 4> scores = normal_game_rules{}.compute_scores(state);
    REQUIRE(scores == std::array<int, 4>{1, 1, -1, -1});
  }

  SECTION("re wins with 150") {
    state.party_to_eyes[to_integer(party::re)] = 150;
    state.party_to_eyes[to_integer(party::contra)] =
        240 - state.party_to_eyes[to_integer(party::re)];
    std::array<int, 4> scores = normal_game_rules{}.compute_scores(state);
    REQUIRE(scores == std::array<int, 4>{1, 1, -1, -1});
  }

  SECTION("re wins with 151") {
    state.party_to_eyes[to_integer(party::re)] = 151;
    state.party_to_eyes[to_integer(party::contra)] =
        240 - state.party_to_eyes[to_integer(party::re)];
    std::array<int, 4> scores = normal_game_rules{}.compute_scores(state);
    REQUIRE(scores == std::array<int, 4>{2, 2, -2, -2});
  }

  SECTION("announce k90 + re wins with 151") {
    state.party_to_num_bids[to_integer(party::re)] = 2;
    state.party_to_min_points[to_integer(party::re)] = 151;
    state.party_to_eyes[to_integer(party::re)] = 151;
    state.party_to_eyes[to_integer(party::contra)] =
        240 - state.party_to_eyes[to_integer(party::re)];
    std::array<int, 4> scores = normal_game_rules{}.compute_scores(state);
    REQUIRE(scores == std::array<int, 4>{6, 6, -6, -6});
  }

  SECTION("contra wins with 121") {
    state.party_to_eyes[to_integer(party::re)] = 119;
    state.party_to_eyes[to_integer(party::contra)] =
        240 - state.party_to_eyes[to_integer(party::re)];
    std::array<int, 4> scores = normal_game_rules{}.compute_scores(state);
    REQUIRE(scores == std::array<int, 4>{-2, -2, 2, 2});
  }
}

TEST_CASE("Solo Game Scoring") {
  using namespace doko;
  score_state state;

  state.player_to_party[0] = party::re;
  state.player_to_party[1] = party::contra;
  state.player_to_party[2] = party::contra;
  state.player_to_party[3] = party::contra;

  SECTION("re wins with 121") {
    state.party_to_eyes[to_integer(party::re)] = 121;
    state.party_to_eyes[to_integer(party::contra)] =
        240 - state.party_to_eyes[to_integer(party::re)];
    std::array<int, 4> scores = normal_game_rules{}.compute_scores(state);
    REQUIRE(scores == std::array<int, 4>{3, -1, -1, -1});
  }

  SECTION("re wins with 150") {
    state.party_to_eyes[to_integer(party::re)] = 150;
    state.party_to_eyes[to_integer(party::contra)] =
        240 - state.party_to_eyes[to_integer(party::re)];
    std::array<int, 4> scores = normal_game_rules{}.compute_scores(state);
    REQUIRE(scores == std::array<int, 4>{3, -1, -1, -1});
  }

  SECTION("re wins with 151") {
    state.party_to_eyes[to_integer(party::re)] = 151;
    state.party_to_eyes[to_integer(party::contra)] =
        240 - state.party_to_eyes[to_integer(party::re)];
    std::array<int, 4> scores = normal_game_rules{}.compute_scores(state);
    REQUIRE(scores == std::array<int, 4>{6, -2, -2, -2});
  }

  SECTION("contra wins with 121") {
    state.party_to_eyes[to_integer(party::re)] = 119;
    state.party_to_eyes[to_integer(party::contra)] =
        240 - state.party_to_eyes[to_integer(party::re)];
    std::array<int, 4> scores = normal_game_rules{}.compute_scores(state);
    REQUIRE(scores == std::array<int, 4>{-6, 2, 2, 2});
  }
}