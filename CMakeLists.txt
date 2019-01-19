cmake_minimum_required(VERSION 3.8)

project(Doppelkopf2)

find_package(Boost 1.68 REQUIRED system log)
find_package(fmt REQUIRED)

add_library(doko2 
  src/doko/action.cpp 
  src/doko/game_rules.cpp 
  src/doko/card_assignment.cpp)
target_include_directories(doko2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(doko2 PUBLIC cxx_std_17)
target_link_libraries(doko2 PUBLIC Boost::system fmt::fmt)

add_library(doko2_net
  src/doko/net/json.cpp
  src/doko/net/ai_client.cpp
  src/doko/net/server.cpp)
target_link_libraries(doko2_net PUBLIC doko2 Boost::log)

add_subdirectory(apps)

add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/Catch2)
add_subdirectory(tests)