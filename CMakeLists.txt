cmake_minimum_required(VERSION 3.16)
project(fzx VERSION 0.1.0 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "fzx: Using default build type - RelWithDebInfo")
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Release" "RelWithDebInfo" "MinSizeRel" "Debug")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


option(FZX_BUILD_EXECUTABLE "Build terminal application" OFF)
option(FZX_BUILD_LUA_MODULE "Build Lua module" ON)
option(FZX_BUILD_TESTS      "Build tests" OFF)
option(FZX_BUILD_BENCHMARKS "Build benchmarks" OFF)

option(FZX_BUNDLE_DEPENDENCIES "Use bundled dependencies" ON)
option(FZX_BUNDLE_FMT       "Use bundled dependency - fmt" ${FZX_BUNDLE_DEPENDENCIES})
option(FZX_BUNDLE_LUA       "Use bundled dependency - Lua 5.1" ${FZX_BUNDLE_DEPENDENCIES})
option(FZX_BUNDLE_CATCH2    "Use bundled dependency - Catch2" ${FZX_BUNDLE_DEPENDENCIES})
option(FZX_BUNDLE_BENCHMARK "Use bundled dependency - benchmark" ${FZX_BUNDLE_DEPENDENCIES})

find_package(Threads REQUIRED)
if(FZX_BUILD_LUA_MODULE)
  if(FZX_BUNDLE_LUA)
    set(LUA_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/lua/include")
  else()
    find_package(Lua 5.1 EXACT REQUIRED)
  endif()
endif()
if(FZX_BUILD_EXECUTABLE)
  if(FZX_BUNDLE_FMT)
    # TODO: init submodule
    add_subdirectory(deps/fmt)
  else()
    find_package(fmt REQUIRED)
  endif()
endif()
if(FZX_BUILD_TESTS)
  if(FZX_BUNDLE_CATCH2)
    # TODO: init submodule
    add_subdirectory(deps/Catch2)
  else()
    find_package(Catch2 3 REQUIRED)
  endif()
endif()
if(FZX_BUILD_BENCHMARKS)
  if(FZX_BUNDLE_BENCHMARK)
    # TODO: init submodule
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "")
    set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "")
    add_subdirectory(deps/benchmark)
  else()
    find_package(benchmark REQUIRED)
  endif()
endif()


list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(FZX_ENABLE_NATIVE  "Optimize for the current architecture" ON)
option(FZX_ENABLE_LTO     "Enable link time optimizations" ON)
option(FZX_ENABLE_SSE2    "Enable SSE2 (x86_64)" ON)
option(FZX_ENABLE_SSE41   "Enable SSE4.1 (x86_64)" ON)
option(FZX_ENABLE_AVX     "Enable AVX (x86_64)" ON)
option(FZX_ENABLE_AVX2    "Enable AVX2 (x86_64)" ON)
option(FZX_ENABLE_NEON    "Enable NEON (arm64)" OFF)

option(FZX_ENABLE_UBSAN   "Enable undefined behavior sanitizer" OFF)
option(FZX_ENABLE_ASAN    "Enable address sanitizer" OFF)
option(FZX_ENABLE_TSAN    "Enable thread sanitizer" OFF)
if(FZX_ENABLE_ASAN AND FZX_ENABLE_TSAN)
  message(FATAL_ERROR "fzx: ASAN and TSAN are not compatible")
endif()

option(FZX_ENABLE_COVERAGE "Enable code coverage" OFF)
option(FZX_ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
option(FZX_SAVE_OPTIMIZATION_RECORDS "Save Clang optimization records" OFF)


if(FZX_ENABLE_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT lto)
  if(lto)
    message(STATUS "fzx: Link time optimizations enabled")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON)
  endif()
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")

if(FZX_ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY clang-tidy)
  if(CLANG_TIDY)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY}")
  endif()
endif()

add_library(fzxopts INTERFACE)

if(FZX_ENABLE_UBSAN OR FZX_ENABLE_ASAN OR FZX_ENABLE_TSAN)
  target_compile_options(fzxopts INTERFACE -fno-omit-frame-pointer -fno-optimize-sibling-calls)
endif()
if(FZX_ENABLE_UBSAN)
  target_compile_options(fzxopts INTERFACE -fsanitize=undefined)
  target_link_options(fzxopts INTERFACE -fsanitize=undefined)
endif()
if(FZX_ENABLE_ASAN)
  target_compile_options(fzxopts INTERFACE -fsanitize=address)
  target_link_options(fzxopts INTERFACE -fsanitize=address)
endif()
if(FZX_ENABLE_TSAN)
  target_compile_options(fzxopts INTERFACE -fsanitize=thread)
  target_link_options(fzxopts INTERFACE -fsanitize=thread)
endif()

if(NOT MSVC)
  target_compile_options(fzxopts INTERFACE -Wall -Wextra)
endif()
set(compile_options
  -Werror=unused-result
  -Werror=strict-aliasing
  -Werror=pointer-arith
  -Werror=char-subscripts
  -Werror=vla)
include(CheckCXXCompilerFlag)
foreach(option ${compile_options})
  string(TOUPPER "FZX_HAS${option}" option_name)
  string(REGEX REPLACE "[=-]" "_" option_name "${option_name}")
  check_cxx_compiler_flag("${option}" ${option_name})
  if(${option_name})
    target_compile_options(fzxopts INTERFACE $<$<CONFIG:Debug>:${option}>)
  endif()
endforeach()

if(FZX_ENABLE_COVERAGE)
  target_compile_options(fzxopts INTERFACE --coverage)
  target_link_options(fzxopts INTERFACE --coverage)
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

if(FZX_SAVE_OPTIMIZATION_RECORDS)
  if(NOT "${CMAKE_C_COMPILER_ID}" MATCHES "Clang")
    message(FATAL_ERROR "fzx: FZX_SAVE_OPTIMIZATION_RECORDS is supported only on Clang")
  endif()
  target_compile_options(fzxopts INTERFACE -fsave-optimization-record=yaml)
endif()


include(FzxCPUFeatures)
if(FZX_ENABLE_NATIVE AND NOT MSVC)
  target_compile_options(fzxopts INTERFACE -march=native)
endif()
if(FZX_ENABLE_SSE2 AND FZX_HAS_SSE2)
  message(STATUS "fzx: SSE2 enabled")
  if(NOT MSVC)
    target_compile_options(fzxopts INTERFACE -msse2)
  endif()
  target_compile_definitions(fzxopts INTERFACE FZX_SSE2)
endif()
if(FZX_ENABLE_SSE41 AND FZX_HAS_SSE41)
  message(STATUS "fzx: SSE4.1 enabled")
  if(NOT MSVC)
    target_compile_options(fzxopts INTERFACE -msse4.1)
  endif()
  target_compile_definitions(fzxopts INTERFACE FZX_SSE41)
endif()
if(FZX_ENABLE_AVX AND FZX_HAS_AVX)
  message(STATUS "fzx: AVX enabled")
  if(NOT MSVC)
    target_compile_options(fzxopts INTERFACE -mavx)
  else()
    target_compile_options(fzxopts INTERFACE /arch:AVX)
  endif()
  target_compile_definitions(fzxopts INTERFACE FZX_AVX)
endif()
if(FZX_ENABLE_AVX2 AND FZX_HAS_AVX2)
  message(STATUS "fzx: AVX2 enabled")
  if(NOT MSVC)
    target_compile_options(fzxopts INTERFACE -mavx2)
  else()
    target_compile_options(fzxopts INTERFACE /arch:AVX2)
  endif()
  target_compile_definitions(fzxopts INTERFACE FZX_AVX2)
endif()
if(FZX_ENABLE_NEON)
  # TODO: detect
  message(STATUS "fzx: NEON enabled")
  target_compile_definitions(fzxopts INTERFACE FZX_NEON)
endif()


target_compile_definitions(fzxopts INTERFACE
  # Disable DEBUG_ASSERT and enable ASSUME in release builds
  $<$<CONFIG:Release,RelWithDebInfo,MinSizeRel>:FZX_OPTIMIZE>)


add_subdirectory(src/fzx)

if(FZX_BUILD_TESTS)
  add_subdirectory(test)
endif()

if(FZX_BUILD_BENCHMARKS)
  add_subdirectory(benchmark)
endif()
