cmake_minimum_required(VERSION 3.25)
project(fzx VERSION 0.1.0 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Using default build type - RelWithDebInfo")
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Release" "RelWithDebInfo" "MinSizeRel" "Debug")
endif()

option(BUILD_EXECUTABLE "Build terminal application" ON)
option(BUILD_LUA_MODULE "Build Lua module" ON)
option(BUILD_TESTS "Build tests" OFF)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)

option(ENABLE_NATIVE  "Optimize for the current architecture" ON)
option(ENABLE_LTO     "Enable link time optimizations" ON)
# TODO: detect support
option(ENABLE_SSE2    "Enable SSE2 (x86_64)" OFF)
option(ENABLE_SSE3    "Enable SSE3 (x86_64)" OFF) # unused
option(ENABLE_SSSE3   "Enable SSSE3 (x86_64)" OFF) # unused
option(ENABLE_SSE41   "Enable SSE4.1 (x86_64)" OFF) # unused
option(ENABLE_AVX     "Enable AVX (x86_64)" OFF) # unused
option(ENABLE_AVX2    "Enable AVX2 (x86_64)" OFF)

option(ENABLE_UBSAN   "Enable undefined behavior sanitizer" OFF)
option(ENABLE_ASAN    "Enable address sanitizer" OFF)
option(ENABLE_TSAN    "Enable thread sanitizer" OFF)
if(ENABLE_ASAN AND ENABLE_TSAN)
  message(FATAL_ERROR "ASAN and TSAN are not compatible")
endif()

option(ENABLE_COVERAGE "Enable code coverage" OFF)
option(ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
option(SAVE_OPTIMIZATION_RECORDS "Save Clang optimization records" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(ENABLE_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT lto)
  if(lto)
    message(STATUS "Link time optimizations enabled")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON)
  endif()
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")

if(ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY clang-tidy)
  if(CLANG_TIDY)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY}")
  endif()
endif()

find_package(Threads REQUIRED)
if(BUILD_EXECUTABLE)
  find_package(fmt REQUIRED) # TODO: remove fmt dependency. clang can't link fmt built with gcc
endif()
if(BUILD_LUA_MODULE)
  find_package(Lua 5.1 EXACT REQUIRED)
endif()
if(BUILD_TESTS)
  find_package(Catch2 3 REQUIRED)
endif()
if(BUILD_BENCHMARKS)
  find_package(benchmark REQUIRED)
endif()

if(ENABLE_UBSAN OR ENABLE_ASAN OR ENABLE_TSAN)
  add_compile_options(-fno-omit-frame-pointer -fno-optimize-sibling-calls)
endif()
if(ENABLE_UBSAN)
  add_compile_options(-fsanitize=undefined)
  add_link_options(-fsanitize=undefined)
endif()
if(ENABLE_ASAN)
  add_compile_options(-fsanitize=address)
  add_link_options(-fsanitize=address)
endif()
if(ENABLE_TSAN)
  add_compile_options(-fsanitize=thread)
  add_link_options(-fsanitize=thread)
endif()

if(ENABLE_NATIVE)
  add_compile_options(-march=native)
endif()

add_compile_options(-Wall -Wextra)
set(compile_options
  -Werror=unused-result
  -Werror=strict-aliasing
  -Werror=pointer-arith
  -Werror=char-subscripts
  -Werror=vla)
include(CheckCXXCompilerFlag)
foreach(option ${compile_options})
  string(TOUPPER "HAS${option}" option_name)
  string(REGEX REPLACE "[=-]" "_" option_name "${option_name}")
  check_cxx_compiler_flag("${option}" ${option_name})
  if(${option_name})
    add_compile_options($<$<CONFIG:Debug>:${option}>)
  endif()
endforeach()

if(ENABLE_COVERAGE)
  add_compile_options(--coverage)
  add_link_options(--coverage)
  add_custom_target(coverage
    COMMAND cmake -E make_directory cov
    COMMAND gcovr
      --html-details cov/cov.html
      --exclude-unreachable-branches
      --gcov-ignore-parse-errors
      --root "${CMAKE_CURRENT_SOURCE_DIR}"
      --filter "${CMAKE_CURRENT_SOURCE_DIR}/src/.*"
      .)
  add_custom_target(coverage-clean
    COMMAND gcovr --delete
    COMMAND cmake -E rm -rf -- cov)
endif()

if(SAVE_OPTIMIZATION_RECORDS)
  if(NOT "${CMAKE_C_COMPILER_ID}" MATCHES "Clang")
    message(FATAL_ERROR "SAVE_OPTIMIZATION_RECORDS is supported only on Clang")
  endif()
  add_compile_options(-fsave-optimization-record=yaml)
endif()

add_compile_definitions(
  # Disable DEBUG_ASSERT and enable ASSUME in release builds
  $<$<CONFIG:Release,RelWithDebInfo,MinSizeRel>:FZX_OPTIMIZE>
  # Enable SIMD
  $<$<BOOL:${ENABLE_SSE2}>:FZX_SSE2>
  $<$<BOOL:${ENABLE_SSE3}>:FZX_SSE3>
  $<$<BOOL:${ENABLE_SSSE3}>:FZX_SSSE3>
  $<$<BOOL:${ENABLE_SSE41}>:FZX_SSE41>
  $<$<BOOL:${ENABLE_AVX}>:FZX_AVX>
  $<$<BOOL:${ENABLE_AVX2}>:FZX_AVX2>)

add_subdirectory(src/fzx)

if(BUILD_TESTS)
  add_subdirectory(test)
endif()

if(BUILD_BENCHMARKS)
  add_subdirectory(benchmark)
endif()
