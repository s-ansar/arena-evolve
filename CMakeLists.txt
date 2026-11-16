cmake_minimum_required(VERSION 3.20)
project(arena_evolve LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Strict warnings for our own code: they catch real bugs early.
add_library(project_warnings INTERFACE)
if(MSVC)
  target_compile_options(project_warnings INTERFACE /W4)
else()
  target_compile_options(project_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

# Optional sanitizer, e.g.  cmake -DSANITIZE=thread  (address, undefined or thread).
set(SANITIZE "" CACHE STRING "Sanitizer to enable: address, undefined or thread")
if(SANITIZE AND NOT MSVC)
  add_compile_options(-fsanitize=${SANITIZE} -fno-omit-frame-pointer)
  add_link_options(-fsanitize=${SANITIZE})
endif()

# The simulation core: arena physics and the game loops. A static library, so the tests now
# and the server later link the same code.
add_library(sim STATIC src/sim/arena.cpp src/sim/loop.cpp)
target_include_directories(sim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(sim PRIVATE project_warnings)

find_package(Threads REQUIRED)

# Protocol, thread-safe queue, mailbox and the baseline controller. No sockets here, so this
# part builds and is tested on Windows too.
add_library(net_core STATIC src/net/protocol.cpp src/ai/chase.cpp)
target_include_directories(net_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(net_core PUBLIC Threads::Threads PRIVATE project_warnings)

# The server and client use raw POSIX sockets, so they are Linux only (see README).
if(NOT WIN32)
  add_library(net STATIC src/net/socket.cpp src/net/server.cpp src/net/client.cpp)
  target_link_libraries(net PUBLIC sim net_core PRIVATE project_warnings)

  add_executable(arena_server src/net/server_main.cpp)
  target_link_libraries(arena_server PRIVATE net project_warnings)
  add_executable(arena_bot src/net/bot_main.cpp)
  target_link_libraries(arena_bot PRIVATE net project_warnings)
  add_executable(arena_watch src/net/watch_main.cpp)
  target_link_libraries(arena_watch PRIVATE net project_warnings)
endif()

# Tests: GoogleTest is downloaded at configure time, so nothing needs installing by hand.
enable_testing()
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)  # Extracted files get the extraction time, so rebuilds work right (CMP0135).
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)  # Needed on Windows (MSVC runtime match).
FetchContent_MakeAvailable(googletest)

add_executable(sim_tests tests/sim_test.cpp tests/arena_test.cpp)
target_link_libraries(sim_tests PRIVATE sim project_warnings GTest::gtest_main)
add_executable(net_core_tests tests/protocol_test.cpp tests/concurrency_test.cpp tests/chase_test.cpp
                              tests/stream_stats_test.cpp)
target_link_libraries(net_core_tests PRIVATE net_core project_warnings GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(sim_tests)
gtest_discover_tests(net_core_tests)

if(NOT WIN32)
  add_executable(server_tests tests/server_test.cpp)
  target_link_libraries(server_tests PRIVATE net project_warnings GTest::gtest_main)
  gtest_discover_tests(server_tests)
endif()

# Microbenchmarks (Google Benchmark, also downloaded at configure time). Only meaningful in a
# Release build; CI still builds them so they cannot silently break.
option(ARENA_BUILD_BENCHMARKS "Build the Google Benchmark programs" ON)
if(ARENA_BUILD_BENCHMARKS)
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)  # skip the library's own tests
  set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)  # a new compiler warning in their code must not break our build
  FetchContent_Declare(
    benchmark
    URL https://github.com/google/benchmark/archive/refs/tags/v1.9.1.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(benchmark)

  add_executable(sim_bench bench/sim_bench.cpp)
  target_link_libraries(sim_bench PRIVATE sim net_core project_warnings benchmark::benchmark)
endif()
