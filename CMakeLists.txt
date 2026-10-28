cmake_minimum_required(VERSION 3.16)
project(braindust C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# raylib 5.5: the system package (Fedora: raylib-devel), else fetched and built.
# On Android (built by Gradle, see android/) always fetched, and built for Android.
if(ANDROID)
  set(PLATFORM "Android" CACHE STRING "raylib platform" FORCE)
else()
  find_package(raylib 5.5 QUIET)
endif()
if(NOT raylib_FOUND)
  include(FetchContent)
  FetchContent_Declare(raylib URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(raylib)
endif()

set(GAME_SOURCES
  src/main.cpp src/world.cpp src/content.cpp src/character.cpp src/level.cpp src/tunables.cpp src/input.cpp
  src/ui.cpp src/panel.cpp src/menu.cpp src/touch.cpp src/platform.cpp src/scene3d.cpp src/renderer.cpp src/scenery.cpp src/actors.cpp src/hud.cpp)

if(ANDROID)
  # the library android.app.NativeActivity loads (android/app/src/main/AndroidManifest.xml); raylib runs our main()
  add_library(braindust SHARED ${GAME_SOURCES})
  target_include_directories(braindust PRIVATE ${ANDROID_NDK}/sources/android/native_app_glue)
  target_link_libraries(braindust PRIVATE raylib android log)
  target_link_options(braindust PRIVATE -u ANativeActivity_onCreate)  # the glue's entry point is in raylib: keep it
else()
  add_executable(braindust ${GAME_SOURCES})
  target_link_libraries(braindust PRIVATE raylib)
  if(UNIX AND NOT APPLE)
    target_link_libraries(braindust PRIVATE m)
  endif()
endif()
target_compile_options(braindust PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wno-missing-field-initializers>)

if(NOT ANDROID)
  add_executable(character_test EXCLUDE_FROM_ALL tests/character_test.cpp src/character.cpp src/platform.cpp)
  target_link_libraries(character_test PRIVATE raylib)
  add_executable(collision_test EXCLUDE_FROM_ALL tests/collision_test.cpp src/world.cpp src/content.cpp src/character.cpp src/input.cpp src/touch.cpp src/ui.cpp src/platform.cpp)
  target_link_libraries(collision_test PRIVATE raylib)
  add_executable(gameplay_test EXCLUDE_FROM_ALL tests/gameplay_test.cpp src/world.cpp src/content.cpp src/character.cpp src/input.cpp src/touch.cpp src/ui.cpp src/platform.cpp)
  target_link_libraries(gameplay_test PRIVATE raylib)
  add_executable(platform_test EXCLUDE_FROM_ALL tests/platform_test.cpp src/touch.cpp src/ui.cpp src/platform.cpp)
  target_link_libraries(platform_test PRIVATE raylib)
endif()
