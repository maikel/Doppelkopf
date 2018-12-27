cmake_minimum_required(VERSION 3.13)

project(Doppelkopf2_tests)

add_executable(StaticVector_test StaticVector_test.cpp)
target_link_libraries(StaticVector_test PRIVATE doko2)
