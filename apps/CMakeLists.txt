cmake_minimum_required(VERSION 3.8)

project(Doppelkopf2_apps)

add_executable(ai_client ai_client.cpp)
target_link_libraries(ai_client PRIVATE doko2 doko2_net fmt::fmt)

add_executable(server server.cpp)
target_link_libraries(server PRIVATE doko2 doko2_net fmt::fmt)