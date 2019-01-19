#include "doko/uct_tree.hpp"
#include "doko/card_assignment.hpp"

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <iostream>
#include <random>

std::ostream& operator<<(std::ostream& os, doko::color color) {
  std::array<const char*, 4> names{"diamonds", "hearts", "spades", "clubs"};
  os << names[to_integer(color)];
  return os;
}

std::ostream& operator<<(std::ostream& os, doko::face face) {
  std::array<const char*, 6> names{"nine", "jack", "queen",
                                   "king", "ten",  "ace"};
  os << names[to_integer(face)];
  return os;
}

std::ostream& operator<<(std::ostream& os, doko::party party) {
  if (party == doko::party::re) {
    os << "Re";
  } else {
    os << "Contra";
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, doko::action action) {
  if (const doko::announcement* bid = action.as_bid()) {
    os << "announcement(" << bid->party() << ')';
  } else if (const doko::card* card = action.as_card()) {
    os << "card(" << card->color() << ", " << card->face() << ')';
  }
  return os;
}

TEST_CASE("Play a complete Game") {
  using namespace doko;
  std::mt19937 gen{2019};
  std::array<std::array<card, 12>, 4> hands = random_initial_hands(gen);

  auto Assignment = [&](player_id p) {
    std::array<static_vector<card, 12>, 4> assignment =
        to_static_vectors(hands);
    for (int i = 0; i < 4; ++i) {
      if (i != int(p)) {
        assignment[i].clear();
      }
    }
    return assignment;
  };

  constexpr int n_trees = 10;
  constexpr int n_iters = 100;

  static_vector<action, 48> history{};
  running_game_state state{player_id::first, to_static_vectors(hands)};
  uct_tree tree({player_id::first, hands}, history, 13 * n_iters);

  while (history.size() != 48) {
    std::array<std::ptrdiff_t, 13> visits{};
    for (int i = 0; i < n_trees; ++i) {
      auto assign =
          make_assign_cards_state(normal_game_rules{}, hands[0], history);
      auto initial_hands = assign_cards_randomly(assign, gen);
      tree.reset({player_id::first, initial_hands}, history);
      for (std::ptrdiff_t i = 0; i < n_iters; ++i) {
        tree.rollout_once(gen, normal_game_rules{});
      }
      int count = 0;
      for (std::ptrdiff_t child : tree.children(0)) {
        visits[count] += tree.num_visits(child);
        ++count;
      }
    }
    const action action = tree.action(
        1 + std::distance(visits.begin(),
                          std::max_element(visits.begin(), visits.end())));
    REQUIRE(action.player() == state.player);
    state.player =
        observe_action(normal_game_rules{}, state.trick, action, history);
    history.push_back(action);
  }
}