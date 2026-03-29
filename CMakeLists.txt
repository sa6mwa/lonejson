cmake_minimum_required(VERSION 3.21)

project(lonejson VERSION 0.1.0 LANGUAGES C)

include(CTest)
find_program(LONEJSON_LUA_EXECUTABLE
  NAMES lua lua5.5 lua5.4 lua5.3 luajit
  REQUIRED)

option(LONEJSON_BUILD_TESTS "Build lonejson tests." ON)
option(LONEJSON_BUILD_EXAMPLES "Build lonejson examples." OFF)
option(LONEJSON_BUILD_BENCHMARKS "Build lonejson benchmarks." OFF)
option(LONEJSON_BUILD_FUZZERS "Build lonejson libFuzzer targets." OFF)
option(LONEJSON_ENABLE_ASAN "Enable AddressSanitizer and UndefinedBehaviorSanitizer." OFF)
option(LONEJSON_ENABLE_STACK_USAGE "Emit compiler stack-usage reports when supported." OFF)

set(LONEJSON_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(LONEJSON_TEST_BUNDLE_ROOT "" CACHE PATH "Root directory for extracted liblockdc dev bundle content used by tests.")
set(LONEJSON_BENCH_BUNDLE_ROOT "" CACHE PATH "Root directory for extracted liblockdc dev bundle content used by benchmarks.")
set(LONEJSON_GENERATED_FIXTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/fixtures")
set(LONEJSON_STACK_USAGE_TARGETS "")

add_custom_target(lonejson_generate_fixtures
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${LONEJSON_GENERATED_FIXTURE_DIR}"
  COMMAND "${LONEJSON_LUA_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_large_fixtures.lua" "${LONEJSON_GENERATED_FIXTURE_DIR}"
  VERBATIM)

add_library(lonejson INTERFACE)
target_include_directories(lonejson
  INTERFACE
    "$<BUILD_INTERFACE:${LONEJSON_PUBLIC_INCLUDE_DIR}>"
    "$<INSTALL_INTERFACE:include>")
target_compile_options(lonejson
  INTERFACE
    -D_POSIX_C_SOURCE=200809L
    -std=c89
    -Wall
    -Wextra
    -Wpedantic
    -pedantic-errors)

if(LONEJSON_ENABLE_ASAN AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

if(LONEJSON_ENABLE_STACK_USAGE AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-fstack-usage)
endif()

if(LONEJSON_BUILD_TESTS)
  enable_testing()
  add_executable(lonejson_tests tests/test_main.c)
  add_executable(lonejson_short_names_tests tests/test_short_names.c)
  add_executable(lonejson_short_names_disabled_tests
    tests/test_short_names_disabled.c)
  target_include_directories(lonejson_tests PRIVATE "${LONEJSON_PUBLIC_INCLUDE_DIR}")
  target_include_directories(lonejson_short_names_tests PRIVATE
    "${LONEJSON_PUBLIC_INCLUDE_DIR}")
  target_include_directories(lonejson_short_names_disabled_tests PRIVATE
    "${LONEJSON_PUBLIC_INCLUDE_DIR}")
  target_compile_definitions(lonejson_tests PRIVATE
    LONEJSON_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    LONEJSON_GENERATED_FIXTURE_DIR="${LONEJSON_GENERATED_FIXTURE_DIR}")
  target_link_libraries(lonejson_tests PRIVATE lonejson)
  target_link_libraries(lonejson_short_names_tests PRIVATE lonejson)
  target_link_libraries(lonejson_short_names_disabled_tests PRIVATE lonejson)
  add_dependencies(lonejson_tests lonejson_generate_fixtures)
  add_test(NAME lonejson_tests COMMAND lonejson_tests)
  add_test(NAME lonejson_short_names_tests COMMAND lonejson_short_names_tests)
  add_test(NAME lonejson_short_names_disabled_tests
    COMMAND lonejson_short_names_disabled_tests)
  list(APPEND LONEJSON_STACK_USAGE_TARGETS lonejson_tests)
  list(APPEND LONEJSON_STACK_USAGE_TARGETS lonejson_short_names_tests)
  list(APPEND LONEJSON_STACK_USAGE_TARGETS lonejson_short_names_disabled_tests)
endif()

if(LONEJSON_BUILD_EXAMPLES)
  set(LONEJSON_STANDALONE_EXAMPLES
    parse_string
    parse_file
    parse_reader
    push_parser
    spooled_text
    spooled_bytes
    source_text
    source_bytes
    serialize_string
    serialize_file
    serialize_jsonl
    fixed_storage)
  foreach(example_name IN LISTS LONEJSON_STANDALONE_EXAMPLES)
    add_executable("example_${example_name}" "examples/${example_name}.c")
    target_include_directories("example_${example_name}" PRIVATE "${LONEJSON_PUBLIC_INCLUDE_DIR}")
    target_link_libraries("example_${example_name}" PRIVATE lonejson)
    list(APPEND LONEJSON_STACK_USAGE_TARGETS "example_${example_name}")
  endforeach()
endif()

if(LONEJSON_BUILD_BENCHMARKS)
  if(NOT LONEJSON_BENCH_BUNDLE_ROOT)
    message(FATAL_ERROR "LONEJSON_BUILD_BENCHMARKS requires -D LONEJSON_BENCH_BUNDLE_ROOT=/path/to/liblockdc/.../root")
  endif()
  add_executable(lonejson_bench bench/lonejson_bench.c)
  target_include_directories(lonejson_bench PRIVATE
    "${LONEJSON_PUBLIC_INCLUDE_DIR}"
    "${LONEJSON_BENCH_BUNDLE_ROOT}/include")
  target_link_directories(lonejson_bench PRIVATE "${LONEJSON_BENCH_BUNDLE_ROOT}/lib")
  target_link_libraries(lonejson_bench PRIVATE lonejson yajl_s)
  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(lonejson_bench PRIVATE -Wno-long-long)
  endif()
  list(APPEND LONEJSON_STACK_USAGE_TARGETS lonejson_bench)
endif()

if(LONEJSON_BUILD_FUZZERS)
  if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "LONEJSON_BUILD_FUZZERS requires Clang/libFuzzer.")
  endif()
  add_executable(lonejson_fuzz_validate fuzz/fuzz_validate.c)
  target_include_directories(lonejson_fuzz_validate PRIVATE "${LONEJSON_PUBLIC_INCLUDE_DIR}")
  target_link_libraries(lonejson_fuzz_validate PRIVATE lonejson)
  target_compile_options(lonejson_fuzz_validate PRIVATE
    -fsanitize=fuzzer-no-link,address,undefined
    -fno-omit-frame-pointer)
  target_link_options(lonejson_fuzz_validate PRIVATE
    -fsanitize=fuzzer,address,undefined)

  add_executable(lonejson_fuzz_mapped_parse fuzz/fuzz_mapped_parse.c)
  target_include_directories(lonejson_fuzz_mapped_parse PRIVATE "${LONEJSON_PUBLIC_INCLUDE_DIR}")
  target_link_libraries(lonejson_fuzz_mapped_parse PRIVATE lonejson)
  target_compile_options(lonejson_fuzz_mapped_parse PRIVATE
    -fsanitize=fuzzer-no-link,address,undefined
    -fno-omit-frame-pointer)
  target_link_options(lonejson_fuzz_mapped_parse PRIVATE
    -fsanitize=fuzzer,address,undefined)
endif()

add_custom_target(format
  COMMAND clang-format -i
    "${CMAKE_CURRENT_SOURCE_DIR}/include/lonejson.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/bench/lonejson_bench.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/lua/lonejson_lua.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_validate.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_mapped_parse.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_main.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_short_names.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_short_names_disabled.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/parse_string.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/parse_file.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/parse_reader.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/push_parser.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/spooled_text.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/spooled_bytes.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/source_text.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/source_bytes.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/serialize_string.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/serialize_file.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/serialize_jsonl.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/fixed_storage.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/curl_get.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/curl_put.c"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  VERBATIM)

add_custom_target(print-liblockdc-bundle
  COMMAND "${CMAKE_COMMAND}" -E echo "LONEJSON_TEST_BUNDLE_ROOT=${LONEJSON_TEST_BUNDLE_ROOT}"
  VERBATIM)

add_custom_target(stack-usage-report
  COMMAND "${LONEJSON_LUA_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/report_stack_usage.lua" "${CMAKE_CURRENT_BINARY_DIR}"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  VERBATIM)
if(LONEJSON_STACK_USAGE_TARGETS)
  add_dependencies(stack-usage-report ${LONEJSON_STACK_USAGE_TARGETS})
endif()
