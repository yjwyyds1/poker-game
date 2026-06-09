cmake_minimum_required(VERSION 3.16)
project(PokerGame VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Core library (no UI dependency)
add_library(poker_core STATIC
    core/card.hpp       core/card.cpp
    core/deck.hpp       core/deck.cpp
    core/hand_evaluator.hpp core/hand_evaluator.cpp
    core/player.hpp     core/player.cpp
    core/game_engine.hpp core/game_engine.cpp
)
target_include_directories(poker_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Console test app
add_executable(poker_console
    tests/console_main.cpp
)
target_link_libraries(poker_console PRIVATE poker_core)

# Unit tests
add_executable(poker_tests
    tests/test_evaluator.cpp
)
target_link_libraries(poker_tests PRIVATE poker_core)
