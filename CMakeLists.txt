cmake_minimum_required(VERSION 3.20)
project(WildPitch C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(WP_BUILD_GAME  "Build the SDL game executable" ON)
option(WP_BUILD_TESTS "Build headless tests and benchmarks" ON)
option(WP_SANITIZE    "Build with ASan+UBSan" OFF)

# Warnings for our code only (not SDL).
set(WP_WARN -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion
            -Wstrict-prototypes -Wmissing-prototypes -Werror=implicit-function-declaration)
if(WP_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

# ---------------------------------------------------------------------------
# Simulation core: pure C17, NO SDL, NO floats, NO libm. Deterministic.
# ---------------------------------------------------------------------------
file(GLOB WP_SIM_SRC CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/sim/*.c)
add_library(wp_sim STATIC ${WP_SIM_SRC})
target_include_directories(wp_sim PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_options(wp_sim PRIVATE ${WP_WARN})
# PUBLIC: inline maths in sim headers must wrap identically in every target that includes them.
target_compile_options(wp_sim PUBLIC -fwrapv)

# ---------------------------------------------------------------------------
# Game executable (SDL3 vendored + static)
# ---------------------------------------------------------------------------
if(WP_BUILD_GAME)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
  if(NOT EXISTS ${CMAKE_SOURCE_DIR}/third_party/SDL/CMakeLists.txt)
    message(FATAL_ERROR "SDL3 source missing. Run tools/fetch_deps.sh first.")
  endif()
  add_subdirectory(third_party/SDL EXCLUDE_FROM_ALL)

  file(GLOB WP_GAME_SRC CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/game/*.c)
  add_executable(wildpitch ${WP_GAME_SRC})
  target_link_libraries(wildpitch PRIVATE wp_sim SDL3::SDL3-static m)
  target_compile_options(wildpitch PRIVATE ${WP_WARN})
endif()

# ---------------------------------------------------------------------------
# Tests (headless, no SDL)
# ---------------------------------------------------------------------------
if(WP_BUILD_TESTS)
  enable_testing()
  file(GLOB WP_TEST_SRC CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/test_*.c)
  foreach(src ${WP_TEST_SRC})
    get_filename_component(name ${src} NAME_WE)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE wp_sim m)
    target_compile_options(${name} PRIVATE ${WP_WARN})
    add_test(NAME ${name} COMMAND ${name} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  endforeach()
  add_executable(bench_sim ${CMAKE_SOURCE_DIR}/tests/bench_sim.c)
  target_link_libraries(bench_sim PRIVATE wp_sim)
  target_compile_options(bench_sim PRIVATE ${WP_WARN})
endif()
