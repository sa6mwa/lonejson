cmake_minimum_required(VERSION 3.21)

execute_process(
  COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/scripts/release_version.sh"
  OUTPUT_VARIABLE LONEJSON_RESOLVED_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT LONEJSON_RESOLVED_VERSION)
  set(LONEJSON_RESOLVED_VERSION "0.0.0")
endif()

project(lonejson VERSION ${LONEJSON_RESOLVED_VERSION} LANGUAGES C)

include(CTest)
find_program(LONEJSON_LUA_EXECUTABLE
  NAMES lua lua5.5 lua5.4 lua5.3 luajit
  REQUIRED)
find_program(CLANG_FORMAT_BIN NAMES clang-format)
find_program(LONEJSON_CLANG_EXECUTABLE NAMES clang)

option(LONEJSON_BUILD_TESTS "Build lonejson tests." ON)
option(LONEJSON_BUILD_EXAMPLES "Build lonejson examples." OFF)
option(LONEJSON_BUILD_BENCHMARKS "Build lonejson benchmarks." OFF)
option(LONEJSON_BUILD_FUZZERS "Build lonejson libFuzzer targets." OFF)
option(LONEJSON_ENABLE_ASAN "Enable AddressSanitizer and UndefinedBehaviorSanitizer." OFF)
option(LONEJSON_ENABLE_STACK_USAGE "Emit compiler stack-usage reports when supported." OFF)
option(LONEJSON_BUILD_WITH_CURL "Build lonejson with curl adapter APIs enabled." OFF)
set(LONEJSON_ABI_VERSION "3" CACHE STRING "Shared-library ABI / SONAME version"
    FORCE)

set(LONEJSON_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(LONEJSON_SHARED_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build")
set(LONEJSON_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(LONEJSON_GENERATED_INCLUDE_DIR "${LONEJSON_GENERATED_DIR}/include")
set(LONEJSON_SINGLE_HEADER_ALIAS_INCLUDE_DIR "${LONEJSON_GENERATED_DIR}/single-header/include")
set(LONEJSON_GENERATED_FIXTURE_DIR "${LONEJSON_SHARED_BUILD_DIR}/generated/fixtures")
set(LONEJSON_TEST_BUNDLE_ROOT "" CACHE PATH "Root directory for extracted liblockdc dev bundle content used by tests.")
set(LONEJSON_TARGET_ARCH "${CMAKE_SYSTEM_PROCESSOR}" CACHE STRING "Artifact architecture name")
set(LONEJSON_TARGET_OS "${CMAKE_SYSTEM_NAME}" CACHE STRING "Artifact operating system name")
set(LONEJSON_TARGET_LIBC "" CACHE STRING "Artifact libc suffix for Linux targets (gnu, musl, or empty)")
set(LONEJSON_STACK_USAGE_TARGETS "")
set(LONEJSON_SINGLE_HEADER_BUILD "${LONEJSON_GENERATED_INCLUDE_DIR}/lonejson_single_header.h")
set(LONEJSON_SINGLE_HEADER_BUILD_ALIAS "${LONEJSON_SINGLE_HEADER_ALIAS_INCLUDE_DIR}/lonejson.h")
set(LONEJSON_SINGLE_HEADER_DIST_GZ "${CMAKE_CURRENT_SOURCE_DIR}/dist/lonejson-${LONEJSON_RESOLVED_VERSION}.h.gz")

function(lonejson_configure_c_target target)
  if(UNIX OR APPLE OR CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_compile_definitions(${target} PRIVATE _POSIX_C_SOURCE=200809L)
  endif()
  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target} PRIVATE
      -std=c89
      -Wall
      -Wextra
      -Wpedantic
      -pedantic-errors
    )
  endif()
endfunction()

set(LONEJSON_STRICT_DOWNSTREAM_WARNING_FLAGS
  -std=c89
  -Wall
  -Wextra
  -Werror
  -Wpedantic
  -Wno-long-long
  -Wstrict-prototypes
  -Wmissing-prototypes
  -Wshadow
  -Wpointer-arith
  -Wcast-qual
  -Wwrite-strings
  -Wdeclaration-after-statement)

function(lonejson_configure_strict_downstream_target target)
  if(UNIX OR APPLE OR CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_compile_definitions(${target} PRIVATE _POSIX_C_SOURCE=200809L)
  endif()
  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target} PRIVATE
      ${LONEJSON_STRICT_DOWNSTREAM_WARNING_FLAGS})
  endif()
endfunction()

string(TOLOWER "${LONEJSON_TARGET_OS}" LONEJSON_TARGET_OS_LOWER)
string(TOLOWER "${LONEJSON_TARGET_ARCH}" LONEJSON_TARGET_ARCH_LOWER)
if(LONEJSON_TARGET_ARCH_LOWER STREQUAL "amd64")
  set(LONEJSON_TARGET_ARCH_LOWER "x86_64")
endif()
if(LONEJSON_TARGET_ARCH_LOWER STREQUAL "arm64")
  set(LONEJSON_TARGET_ARCH_LOWER "aarch64")
endif()
if(LONEJSON_TARGET_OS_LOWER STREQUAL "linux" AND NOT LONEJSON_TARGET_LIBC)
  set(LONEJSON_TARGET_LIBC "gnu")
endif()
if(LONEJSON_TARGET_OS_LOWER STREQUAL "linux")
  set(LONEJSON_TARGET_ID "${LONEJSON_TARGET_ARCH_LOWER}-linux-${LONEJSON_TARGET_LIBC}")
else()
  set(LONEJSON_TARGET_ID "${LONEJSON_TARGET_ARCH_LOWER}-${LONEJSON_TARGET_OS_LOWER}")
endif()
set(LONEJSON_RELEASE_VARIANT_SUFFIX "")
if(LONEJSON_BUILD_WITH_CURL)
  set(LONEJSON_RELEASE_VARIANT_SUFFIX "-curl")
  find_package(CURL REQUIRED)
endif()

if(LONEJSON_ENABLE_ASAN AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

if(LONEJSON_ENABLE_STACK_USAGE AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-fstack-usage)
endif()

add_custom_target(lonejson_generate_fixtures
  COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/scripts/ensure_large_fixtures.sh"
    "${LONEJSON_LUA_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_large_fixtures.lua"
    "${LONEJSON_GENERATED_FIXTURE_DIR}"
  VERBATIM)

set(LONEJSON_SOURCES
  src/lonejson.c
)

add_library(lonejson_shared SHARED ${LONEJSON_SOURCES})
add_library(lonejson_static STATIC ${LONEJSON_SOURCES})
add_library(lonejson ALIAS lonejson_static)

foreach(target lonejson_shared lonejson_static)
  target_include_directories(${target}
    PUBLIC
      $<BUILD_INTERFACE:${LONEJSON_PUBLIC_INCLUDE_DIR}>
      $<INSTALL_INTERFACE:include>
  )
  set_target_properties(${target} PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    VISIBILITY_INLINES_HIDDEN NO
  )
  lonejson_configure_c_target(${target})
  if(LONEJSON_BUILD_WITH_CURL)
    target_compile_definitions(${target} PRIVATE LONEJSON_WITH_CURL)
    target_link_libraries(${target} PUBLIC CURL::libcurl)
  endif()
endforeach()

set_target_properties(lonejson_shared PROPERTIES
  OUTPUT_NAME lonejson
  VERSION ${PROJECT_VERSION}
  SOVERSION ${LONEJSON_ABI_VERSION}
)
set_target_properties(lonejson_static PROPERTIES
  OUTPUT_NAME lonejson
)

if(LONEJSON_BUILD_TESTS)
  enable_testing()
  add_executable(lonejson_tests tests/test_main.c)
  target_include_directories(lonejson_tests PRIVATE "${LONEJSON_PUBLIC_INCLUDE_DIR}")
  target_compile_definitions(lonejson_tests PRIVATE
    LONEJSON_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    LONEJSON_GENERATED_FIXTURE_DIR="${LONEJSON_GENERATED_FIXTURE_DIR}")
  if(LONEJSON_BUILD_WITH_CURL)
    target_compile_definitions(lonejson_tests PRIVATE LONEJSON_WITH_CURL)
    target_link_libraries(lonejson_tests PRIVATE CURL::libcurl)
  endif()
  lonejson_configure_c_target(lonejson_tests)
  add_dependencies(lonejson_tests lonejson_generate_fixtures)
  add_test(NAME lonejson_tests COMMAND lonejson_tests)
  list(APPEND LONEJSON_STACK_USAGE_TARGETS lonejson_tests)

  add_executable(lonejson_short_names_tests tests/test_short_names.c)
  target_include_directories(lonejson_short_names_tests PRIVATE
    "${LONEJSON_SINGLE_HEADER_ALIAS_INCLUDE_DIR}")
  add_dependencies(lonejson_short_names_tests package-single-header)
  lonejson_configure_c_target(lonejson_short_names_tests)
  add_test(NAME lonejson_short_names_tests COMMAND lonejson_short_names_tests)
  list(APPEND LONEJSON_STACK_USAGE_TARGETS lonejson_short_names_tests)

  add_executable(lonejson_short_names_disabled_tests
    tests/test_short_names_disabled.c)
  target_include_directories(lonejson_short_names_disabled_tests PRIVATE
    "${LONEJSON_SINGLE_HEADER_ALIAS_INCLUDE_DIR}")
  add_dependencies(lonejson_short_names_disabled_tests package-single-header)
  lonejson_configure_c_target(lonejson_short_names_disabled_tests)
  add_test(NAME lonejson_short_names_disabled_tests
    COMMAND lonejson_short_names_disabled_tests)
  list(APPEND LONEJSON_STACK_USAGE_TARGETS lonejson_short_names_disabled_tests)

  add_executable(lonejson_single_header_strict_warning_tests
    tests/test_single_header_strict_warnings.c)
  target_include_directories(lonejson_single_header_strict_warning_tests PRIVATE
    "${LONEJSON_SINGLE_HEADER_ALIAS_INCLUDE_DIR}")
  add_dependencies(lonejson_single_header_strict_warning_tests
    package-single-header)
  lonejson_configure_strict_downstream_target(
    lonejson_single_header_strict_warning_tests)
  add_test(NAME lonejson_single_header_strict_warning_tests
    COMMAND lonejson_single_header_strict_warning_tests)
  list(APPEND LONEJSON_STACK_USAGE_TARGETS
    lonejson_single_header_strict_warning_tests)

  if(LONEJSON_CLANG_EXECUTABLE)
    set(LONEJSON_SINGLE_HEADER_STRICT_CLANG_OBJECT
      "${CMAKE_CURRENT_BINARY_DIR}/generated/lonejson_single_header_strict_clang.o")
    add_custom_command(
      OUTPUT "${LONEJSON_SINGLE_HEADER_STRICT_CLANG_OBJECT}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
      COMMAND "${LONEJSON_CLANG_EXECUTABLE}"
        ${LONEJSON_STRICT_DOWNSTREAM_WARNING_FLAGS}
        -D_POSIX_C_SOURCE=200809L
        -I "${LONEJSON_SINGLE_HEADER_ALIAS_INCLUDE_DIR}"
        -c "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_single_header_strict_warnings.c"
        -o "${LONEJSON_SINGLE_HEADER_STRICT_CLANG_OBJECT}"
      DEPENDS
        package-single-header
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_single_header_strict_warnings.c"
      VERBATIM)
    add_custom_target(lonejson_single_header_strict_clang ALL
      DEPENDS "${LONEJSON_SINGLE_HEADER_STRICT_CLANG_OBJECT}")
  endif()

  add_executable(lonejson_static_link_tests tests/test_link_consumer.c)
  target_include_directories(lonejson_static_link_tests PRIVATE
    "${LONEJSON_PUBLIC_INCLUDE_DIR}")
  target_link_libraries(lonejson_static_link_tests PRIVATE lonejson_static)
  lonejson_configure_c_target(lonejson_static_link_tests)
  add_test(NAME lonejson_static_link_tests COMMAND lonejson_static_link_tests)
  list(APPEND LONEJSON_STACK_USAGE_TARGETS lonejson_static_link_tests)

  add_executable(lonejson_shared_link_tests tests/test_link_consumer.c)
  target_include_directories(lonejson_shared_link_tests PRIVATE
    "${LONEJSON_PUBLIC_INCLUDE_DIR}")
  target_link_libraries(lonejson_shared_link_tests PRIVATE lonejson_shared)
  lonejson_configure_c_target(lonejson_shared_link_tests)
  add_test(NAME lonejson_shared_link_tests COMMAND lonejson_shared_link_tests)
  list(APPEND LONEJSON_STACK_USAGE_TARGETS lonejson_shared_link_tests)

  if(UNIX AND NOT APPLE)
    add_test(
      NAME lonejson_shared_soversion_tests
      COMMAND ${CMAKE_COMMAND}
        -DLONEJSON_SONAME_FILE_NAME=$<TARGET_SONAME_FILE_NAME:lonejson_shared>
        -DLONEJSON_ABI_VERSION=${LONEJSON_ABI_VERSION}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_soversion.cmake)
  endif()

  add_test(
    NAME lonejson_single_header_version_tests
    COMMAND ${CMAKE_COMMAND}
      -DLONEJSON_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -DLONEJSON_PUBLIC_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/include/lonejson.h
      -DLONEJSON_SINGLE_HEADER_BUILD=${LONEJSON_SINGLE_HEADER_BUILD}
      -DLONEJSON_SINGLE_HEADER_BUILD_ALIAS=${LONEJSON_SINGLE_HEADER_BUILD_ALIAS}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_single_header_version.cmake)
  add_test(
    NAME lonejson_single_header_release_version_tests
    COMMAND ${CMAKE_COMMAND}
      -DLONEJSON_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -DLONEJSON_RELEASE_VERSION=${LONEJSON_RESOLVED_VERSION}
      -DLONEJSON_SINGLE_HEADER_DIST_GZ=${LONEJSON_SINGLE_HEADER_DIST_GZ}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_single_header_release_version.cmake)
endif()

if(LONEJSON_BUILD_EXAMPLES)
  set(LONEJSON_LINKED_EXAMPLES
    generator_pull
    parse_file
    parse_reader
    push_parser
    spooled_text
    spooled_bytes
    source_text
    source_bytes
    serialize_string
    serialize_file
    fixed_storage)
  foreach(example_name IN LISTS LONEJSON_LINKED_EXAMPLES)
    add_executable("example_${example_name}" "examples/${example_name}.c")
    target_include_directories("example_${example_name}" PRIVATE
      "${LONEJSON_PUBLIC_INCLUDE_DIR}")
    target_link_libraries("example_${example_name}" PRIVATE lonejson)
    lonejson_configure_c_target("example_${example_name}")
    list(APPEND LONEJSON_STACK_USAGE_TARGETS "example_${example_name}")
  endforeach()

  set(LONEJSON_SINGLE_HEADER_EXAMPLES
    parse_string
    serialize_jsonl)
  foreach(example_name IN LISTS LONEJSON_SINGLE_HEADER_EXAMPLES)
    add_executable("example_${example_name}" "examples/${example_name}.c")
    target_include_directories("example_${example_name}" PRIVATE
      "${LONEJSON_SINGLE_HEADER_ALIAS_INCLUDE_DIR}")
    add_dependencies("example_${example_name}" package-single-header)
    lonejson_configure_c_target("example_${example_name}")
    list(APPEND LONEJSON_STACK_USAGE_TARGETS "example_${example_name}")
  endforeach()
endif()

if(LONEJSON_BUILD_BENCHMARKS)
  add_executable(lonejson_bench bench/lonejson_bench.c)
  target_include_directories(lonejson_bench PRIVATE
    "${LONEJSON_SINGLE_HEADER_ALIAS_INCLUDE_DIR}")
  target_compile_definitions(lonejson_bench PRIVATE LONEJSON_IMPLEMENTATION)
  add_dependencies(lonejson_bench package-single-header)
  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(lonejson_bench PRIVATE -Wno-long-long)
  endif()
  lonejson_configure_c_target(lonejson_bench)
  list(APPEND LONEJSON_STACK_USAGE_TARGETS lonejson_bench)
  add_test(
    NAME lonejson_bench_gate_tests
    COMMAND ${CMAKE_COMMAND}
      -DLONEJSON_BENCH_EXE=$<TARGET_FILE:lonejson_bench>
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_bench_gate.cmake)
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
  lonejson_configure_c_target(lonejson_fuzz_validate)

  add_executable(lonejson_fuzz_mapped_parse fuzz/fuzz_mapped_parse.c)
  target_include_directories(lonejson_fuzz_mapped_parse PRIVATE "${LONEJSON_PUBLIC_INCLUDE_DIR}")
  target_link_libraries(lonejson_fuzz_mapped_parse PRIVATE lonejson)
  target_compile_options(lonejson_fuzz_mapped_parse PRIVATE
    -fsanitize=fuzzer-no-link,address,undefined
    -fno-omit-frame-pointer)
  target_link_options(lonejson_fuzz_mapped_parse PRIVATE
    -fsanitize=fuzzer,address,undefined)
  lonejson_configure_c_target(lonejson_fuzz_mapped_parse)

  add_executable(lonejson_fuzz_json_value fuzz/fuzz_json_value.c)
  target_include_directories(lonejson_fuzz_json_value PRIVATE "${LONEJSON_PUBLIC_INCLUDE_DIR}")
  target_link_libraries(lonejson_fuzz_json_value PRIVATE lonejson)
  target_compile_options(lonejson_fuzz_json_value PRIVATE
    -fsanitize=fuzzer-no-link,address,undefined
    -fno-omit-frame-pointer)
  target_link_options(lonejson_fuzz_json_value PRIVATE
    -fsanitize=fuzzer,address,undefined)
  lonejson_configure_c_target(lonejson_fuzz_json_value)

  add_executable(lonejson_fuzz_value_visitor fuzz/fuzz_value_visitor.c)
  target_include_directories(lonejson_fuzz_value_visitor PRIVATE "${LONEJSON_PUBLIC_INCLUDE_DIR}")
  target_link_libraries(lonejson_fuzz_value_visitor PRIVATE lonejson)
  target_compile_options(lonejson_fuzz_value_visitor PRIVATE
    -fsanitize=fuzzer-no-link,address,undefined
    -fno-omit-frame-pointer)
  target_link_options(lonejson_fuzz_value_visitor PRIVATE
    -fsanitize=fuzzer,address,undefined)
  lonejson_configure_c_target(lonejson_fuzz_value_visitor)
endif()

if(CLANG_FORMAT_BIN)
  add_custom_target(format
    COMMAND "${CLANG_FORMAT_BIN}" -i
      "${CMAKE_CURRENT_SOURCE_DIR}/include/lonejson.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/lonejson.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/lonejson_impl.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/lonejson_internal.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/impl/00_prelude.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/impl/10_json_runtime.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/impl/25_json_pull.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/impl/20_parser.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/impl/30_api.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/impl/35_json_pull_impl.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/bench/lonejson_bench.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/lua/lonejson_lua.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_validate.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_mapped_parse.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_json_value.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_value_visitor.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_main.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_link_consumer.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_short_names.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_short_names_disabled.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_single_header_strict_warnings.c"
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
else()
  add_custom_target(format
    COMMAND "${CMAKE_COMMAND}" -E echo "clang-format not found; format target unavailable"
    COMMAND "${CMAKE_COMMAND}" -E false
    VERBATIM)
endif()

add_custom_target(stack-usage-report
  COMMAND "${LONEJSON_LUA_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/report_stack_usage.lua" "${CMAKE_CURRENT_BINARY_DIR}"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  VERBATIM)
if(LONEJSON_STACK_USAGE_TARGETS)
  add_dependencies(stack-usage-report ${LONEJSON_STACK_USAGE_TARGETS})
endif()

install(FILES include/lonejson.h DESTINATION include)
install(TARGETS lonejson_shared
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  ARCHIVE DESTINATION lib
)
install(TARGETS lonejson_static
  ARCHIVE DESTINATION lib
)

add_custom_target(package-archive
  COMMAND "${CMAKE_COMMAND}"
    -DLONEJSON_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
    -DLONEJSON_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
    -DLONEJSON_VERSION=${LONEJSON_RESOLVED_VERSION}
    -DLONEJSON_TARGET_ID=${LONEJSON_TARGET_ID}
    -DLONEJSON_PUBLIC_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/include/lonejson.h
    -DLONEJSON_STATIC_LIB=$<TARGET_FILE:lonejson_static>
    -DLONEJSON_SHARED_LIB=$<TARGET_FILE:lonejson_shared>
    -DLONEJSON_STATIC_LIB_NAME=$<TARGET_FILE_NAME:lonejson_static>
    -DLONEJSON_SHARED_LINK_NAME=$<TARGET_LINKER_FILE_NAME:lonejson_shared>
    -DLONEJSON_SHARED_LIB_NAME=$<TARGET_FILE_NAME:lonejson_shared>
    -DLONEJSON_SHARED_SONAME=$<TARGET_SONAME_FILE_NAME:lonejson_shared>
    -DLONEJSON_RELEASE_VARIANT_SUFFIX=${LONEJSON_RELEASE_VARIANT_SUFFIX}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/package_archive.cmake
  DEPENDS lonejson_shared lonejson_static
  VERBATIM
)

add_custom_target(package-source
  COMMAND "${CMAKE_COMMAND}"
    -DLONEJSON_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
    -DLONEJSON_VERSION=${LONEJSON_RESOLVED_VERSION}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/package_source.cmake
  VERBATIM
)

add_custom_command(
  OUTPUT
    ${LONEJSON_SINGLE_HEADER_BUILD}
    ${LONEJSON_SINGLE_HEADER_BUILD_ALIAS}
    ${LONEJSON_SINGLE_HEADER_DIST_GZ}
  COMMAND "${CMAKE_COMMAND}"
    -DLONEJSON_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
    -DLONEJSON_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
    -DLONEJSON_VERSION=${LONEJSON_RESOLVED_VERSION}
    -DLONEJSON_PUBLIC_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/include/lonejson.h
    -DLONEJSON_INTERNAL_IMPL=${CMAKE_CURRENT_SOURCE_DIR}/src/lonejson_impl.h
    -DLONEJSON_SINGLE_HEADER_BUILD=${LONEJSON_SINGLE_HEADER_BUILD}
    -DLONEJSON_SINGLE_HEADER_BUILD_ALIAS=${LONEJSON_SINGLE_HEADER_BUILD_ALIAS}
    -DLONEJSON_SINGLE_HEADER_DIST_GZ=${LONEJSON_SINGLE_HEADER_DIST_GZ}
    -DLONEJSON_CLANG_FORMAT_BIN=${CLANG_FORMAT_BIN}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/package_single_header.cmake
  DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/lonejson.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lonejson_impl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/00_prelude.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/10_json_runtime.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/20_parser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/25_json_pull.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/30_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/35_json_pull_impl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/package_single_header.cmake
  VERBATIM
)

add_custom_target(package-single-header
  DEPENDS
    ${LONEJSON_SINGLE_HEADER_BUILD}
    ${LONEJSON_SINGLE_HEADER_BUILD_ALIAS}
    ${LONEJSON_SINGLE_HEADER_DIST_GZ}
)

add_custom_target(package-checksums
  COMMAND "${CMAKE_COMMAND}"
    -DLONEJSON_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
    -DLONEJSON_VERSION=${LONEJSON_RESOLVED_VERSION}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/package_checksums.cmake
  VERBATIM
)

add_custom_target(package-clean-dist
  COMMAND "${CMAKE_COMMAND}"
    -DLONEJSON_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/package_clean_dist.cmake
  VERBATIM
)
