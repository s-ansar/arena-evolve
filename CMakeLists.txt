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

# The simulation core is header-only for now. It becomes a normal library once it has .cpp files.
add_library(sim INTERFACE)
target_include_directories(sim INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(sim INTERFACE project_warnings)

# Tests: GoogleTest is downloaded at configure time, so nothing needs installing by hand.
enable_testing()
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)  # Extracted files get the extraction time, so rebuilds work right (CMP0135).
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)  # Needed on Windows (MSVC runtime match).
FetchContent_MakeAvailable(googletest)

add_executable(sim_tests tests/sim_test.cpp)
target_link_libraries(sim_tests PRIVATE sim GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(sim_tests)
