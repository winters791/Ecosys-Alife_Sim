cmake_minimum_required(VERSION 3.20)
project(luca LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# --- core: pure C++ simulation library, no Godot dependency -----------------
add_library(luca_core STATIC
  core/genome.cpp
  core/phenotype.cpp
  core/energy.cpp
  core/mutation.cpp
  core/world.cpp
  core/sim.cpp
  core/metrics.cpp
)
target_include_directories(luca_core PUBLIC core)

# --- headless runner ---------------------------------------------------------
add_executable(luca_headless headless/main.cpp)
target_link_libraries(luca_headless PRIVATE luca_core)
