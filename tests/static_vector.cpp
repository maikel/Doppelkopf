#include "doko/action.hpp"
#include "doko/static_vector.hpp"

#include <array>
#include <cstdio>

int main() {
  using namespace doko;
  struct alignas(64) State {
    static_vector<action, 58> actions;
    std::array<card, 48> initial_hands;
    player_id initial_player;
    player_id current_player;
    std::uint8_t contra_eyes;
    std::uint8_t re_eyes;
    std::uint8_t contra_bid;
    std::uint8_t re_bid;
  };

  const int alignment = alignof(State);
  const int size = sizeof(State);
  State s{};
  std::printf("alignment: %d, size: %d\n", alignment, size);
  intptr_t origin = (intptr_t)s.actions.data();
  std::printf("actions: 0, hands: %ld, initial_player: %ld, current_player: "
              "%ld, contra_eyes: %ld, re_eyes: %ld\n",
              (intptr_t)s.initial_hands.data() - origin,
              (intptr_t)&s.initial_player - origin,
              (intptr_t)&s.current_player - origin,
              (intptr_t)&s.contra_eyes - origin, (intptr_t)&s.re_eyes - origin);
}