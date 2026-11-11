cmake_minimum_required(VERSION 3.20)
project(luca LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Warnings for our own targets only (not for third-party code).
if(MSVC)
  set(LUCA_WARNINGS /W4)
else()
  set(LUCA_WARNINGS -Wall -Wextra -Wpedantic)
endif()

# --- core: pure C++ simulation library, no Godot dependency -----------------
add_library(luca_core STATIC
  core/genome.cpp
  core/phenotype.cpp
  core/energy.cpp
  core/mutation.cpp
  core/world.cpp
  core/powers.cpp
  core/sim.cpp
  core/metrics.cpp
)
target_include_directories(luca_core PUBLIC core)
target_compile_options(luca_core PRIVATE ${LUCA_WARNINGS})
set_target_properties(luca_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# --- headless runner ---------------------------------------------------------
add_executable(luca_headless headless/main.cpp)
target_link_libraries(luca_headless PRIVATE luca_core)
target_compile_options(luca_headless PRIVATE ${LUCA_WARNINGS})

# --- tests -------------------------------------------------------------------
enable_testing()
add_executable(test_powers tests/test_powers.cpp)
target_link_libraries(test_powers PRIVATE luca_core)
target_compile_options(test_powers PRIVATE ${LUCA_WARNINGS})
add_test(NAME powers COMMAND test_powers)

# --- Godot viewer extension (optional) ---------------------------------------
# The core never depends on Godot; this thin wrapper depends on the core.
#   git clone --depth 1 https://github.com/godotengine/godot-cpp thirdparty/godot-cpp
#   cmake -S . -B build-godot -G Ninja -DCMAKE_BUILD_TYPE=Release -DLUCA_BUILD_GODOT=ON
#   cmake --build build-godot --target luca_godot
option(LUCA_BUILD_GODOT "Build the Godot viewer extension (needs thirdparty/godot-cpp)" OFF)
if(LUCA_BUILD_GODOT)
  set(GODOTCPP_API_VERSION "4.7" CACHE STRING "Godot API version to build against")
  set(GODOTCPP_TARGET "template_debug" CACHE STRING "godot-cpp build target")
  set(GODOTCPP_SYSTEM_HEADERS ON CACHE BOOL "Keep godot-cpp's own warnings out of our build")
  add_subdirectory(thirdparty/godot-cpp EXCLUDE_FROM_ALL)

  add_library(luca_godot SHARED
    godot/register_types.cpp
    godot/luca_world.cpp
  )
  target_link_libraries(luca_godot PRIVATE luca_core godot-cpp)
  target_compile_options(luca_godot PRIVATE ${LUCA_WARNINGS})
  set_target_properties(luca_godot PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/viewer/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/viewer/bin"
  )
endif()
