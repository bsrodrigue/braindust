cmake_minimum_required(VERSION 3.16)
project(braindust CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# raylib 5.5: the system package (Fedora: raylib-devel), else fetched and built.
find_package(raylib 5.5 QUIET)
if(NOT raylib_FOUND)
  include(FetchContent)
  FetchContent_Declare(raylib URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(raylib)
endif()

add_executable(braindust
  src/main.cpp src/world.cpp src/character.cpp src/level.cpp src/tunables.cpp src/input.cpp
  src/ui.cpp src/panel.cpp src/scene3d.cpp src/renderer.cpp src/scenery.cpp src/actors.cpp src/hud.cpp)
target_link_libraries(braindust PRIVATE raylib)
if(UNIX AND NOT APPLE)
  target_link_libraries(braindust PRIVATE m)
endif()
target_compile_options(braindust PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wno-missing-field-initializers>)

add_executable(character_test EXCLUDE_FROM_ALL tests/character_test.cpp src/character.cpp)
target_link_libraries(character_test PRIVATE raylib)
