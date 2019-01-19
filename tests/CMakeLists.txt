cmake_minimum_required(VERSION 3.8)

project(Doppelkopf2_tests)

enable_testing()

add_executable(static_vector static_vector.cpp)
target_link_libraries(static_vector PRIVATE doko2)

add_executable(game_rules game_rules.cpp)
target_link_libraries(game_rules PRIVATE doko2 Catch2::Catch2)

add_executable(card_assignment card_assignment.cpp)
target_link_libraries(card_assignment PRIVATE doko2 Catch2::Catch2)

add_executable(uct_tree uct_tree.cpp)
target_link_libraries(uct_tree PRIVATE doko2 Catch2::Catch2)


# add_executable(rollout rollout.cpp)
# target_link_libraries(rollout PRIVATE doko2)

# add_executable(GameServer GameServer.cpp)
# target_link_libraries(GameServer PRIVATE doko2)