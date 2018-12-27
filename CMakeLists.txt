cmake_minimum_required(VERSION 3.13)

project(Doppelkopf2)

find_package(Boost 1.68 REQUIRED system)

add_library(doko2 src/doko/Action.cpp)
target_include_directories(doko2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(doko2 PUBLIC cxx_std_17)

add_subdirectory(tests)