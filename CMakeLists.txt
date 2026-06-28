cmake_minimum_required(VERSION 3.21)

execute_process(
  COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/scripts/release_version.sh"
  OUTPUT_VARIABLE LONEJSON_RESOLVED_VERSION
  ERROR_VARIABLE LONEJSON_VERSION_ERROR
  RESULT_VARIABLE LONEJSON_VERSION_RESULT
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_STRIP_TRAILING_WHITESPACE
)
if(NOT LONEJSON_VERSION_RESULT EQUAL 0 OR NOT LONEJSON_RESOLVED_VERSION)
  if(NOT LONEJSON_VERSION_ERROR)
    set(LONEJSON_VERSION_ERROR "version resolver produced no version")
  endif()
  message(FATAL_ERROR "${LONEJSON_VERSION_ERROR}")
endif()

project(lonejson VERSION ${LONEJSON_RESOLVED_VERSION} LANGUAGES C)

include(CTest)
find_program(LONEJSON_LUA_EXECUTABLE
  NAMES lua lua5.5 lua5.4 lua5.3 luajit
  REQUIRED)
find_program(LONEJSON_LUAROCKS_EXECUTABLE NAMES luarocks)
find_program(CLANG_FORMAT_BIN NAMES clang-format)
find_program(LONEJSON_CLANG_EXECUTABLE NAMES clang)

option(LONEJSON_BUILD_TESTS "Build lonejson tests." ON)
option(LONEJSON_BUILD_EXAMPLES "Build lonejson examples." OFF)
option(LONEJSON_BUILD_BENCHMARKS "Build lonejson benchmarks." OFF)
option(LONEJSON_BUILD_FUZZERS "Build lonejson libFuzzer targets." OFF)
option(LONEJSON_ENABLE_ASAN "Enable AddressSanitizer and UndefinedBehaviorSanitizer." OFF)
option(LONEJSON_ENABLE_TSAN "Enable ThreadSanitizer." OFF)
option(LONEJSON_ENABLE_MSAN "Enable MemorySanitizer." OFF)
option(LONEJSON_ENABLE_STACK_USAGE "Emit compiler stack-usage reports when supported." OFF)
option(LONEJSON_BUILD_WITH_CURL "Build lonejson with curl adapter APIs enabled." OFF)
option(LONEJSON_BUILD_WITH_OPENSSL "Build lonejson with OpenSSL-backed auth/crypto support enabled." OFF)
set(LONEJSON_ABI_VERSION "19" CACHE STRING "Shared-library ABI / SONAME version"
    FORCE)

set(LONEJSON_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(LONEJSON_SHARED_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build")
set(LONEJSON_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(LONEJSON_GENERATED_INCLUDE_DIR "${LONEJSON_GENERATED_DIR}/include")
set(LONEJSON_SINGLE_HEADER_ALIAS_INCLUDE_DIR "${LONEJSON_GENERATED_DIR}/single-header/include")
set(LONEJSON_GENERATED_FIXTURE_DIR "${LONEJSON_SHARED_BUILD_DIR}/generated/fixtures")
set(LONEJSON_TEST_BUNDLE_ROOT "" CACHE PATH "Root directory for extracted c.pkt.systems dependency bundle content used by tests.")
set(LONEJSON_C_PKT_SYSTEMS_ROOT "" CACHE PATH "Root directory for extracted c.pkt.systems dependency bundle content.")
set(LONEJSON_TARGET_ARCH "${CMAKE_SYSTEM_PROCESSOR}" CACHE STRING "Artifact architecture name")
set(LONEJSON_TARGET_OS "${CMAKE_SYSTEM_NAME}" CACHE STRING "Artifact operating system name")
set(LONEJSON_TARGET_LIBC "" CACHE STRING "Artifact libc suffix for Linux targets (gnu, musl, or empty)")
set(LONEJSON_STACK_USAGE_TARGETS "")
set(LONEJSON_SINGLE_HEADER_BUILD "${LONEJSON_GENERATED_INCLUDE_DIR}/lonejson_single_header.h")
set(LONEJSON_SINGLE_HEADER_BUILD_ALIAS "${LONEJSON_SINGLE_HEADER_ALIAS_INCLUDE_DIR}/lonejson.h")
set(LONEJSON_SINGLE_HEADER_DIST_GZ "${CMAKE_CURRENT_SOURCE_DIR}/dist/lonejson-${LONEJSON_RESOLVED_VERSION}.h.gz")

function(lonejson_configure_c_target target)
  if(UNIX OR APPLE OR CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_compile_definitions(${target} PRIVATE
      _POSIX_C_SOURCE=200809L
      _FILE_OFFSET_BITS=64)
  endif()
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_compile_definitions(${target} PRIVATE _DARWIN_C_SOURCE=1)
  endif()
  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target} PRIVATE
      -std=c89
      -Wall
      -Wextra
      -Wpedantic
      -pedantic-errors
      $<$<CONFIG:Release>:-Werror>
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
    target_compile_definitions(${target} PRIVATE
      _POSIX_C_SOURCE=200809L
      _FILE_OFFSET_BITS=64)
  endif()
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_compile_definitions(${target} PRIVATE _DARWIN_C_SOURCE=1)
  endif()
  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target} PRIVATE
      ${LONEJSON_STRICT_DOWNSTREAM_WARNING_FLAGS})
  endif()
endfunction()

function(lonejson_configure_lua_c_target target)
  if(UNIX OR APPLE OR CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_compile_definitions(${target} PRIVATE
      _POSIX_C_SOURCE=200809L
      _FILE_OFFSET_BITS=64)
  endif()
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_compile_definitions(${target} PRIVATE _DARWIN_C_SOURCE=1)
  endif()
  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target} PRIVATE
      -std=c99
      -Wall
      -Wextra
      $<$<CONFIG:Release>:-Werror>)
  endif()
endfunction()

string(TOLOWER "${LONEJSON_TARGET_OS}" LONEJSON_TARGET_OS_LOWER)
string(TOLOWER "${CMAKE_SYSTEM_NAME}" LONEJSON_CMAKE_SYSTEM_NAME_LOWER)
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
elseif(LONEJSON_TARGET_OS_LOWER STREQUAL "darwin" AND
       LONEJSON_TARGET_ARCH_LOWER STREQUAL "aarch64")
  set(LONEJSON_TARGET_ID "arm64-apple-darwin")
else()
  set(LONEJSON_TARGET_ID "${LONEJSON_TARGET_ARCH_LOWER}-${LONEJSON_TARGET_OS_LOWER}")
endif()
set(LONEJSON_RELEASE_TARGET_IDS
  x86_64-linux-gnu
  x86_64-linux-musl
  aarch64-linux-gnu
  aarch64-linux-musl
  armhf-linux-gnu
  armhf-linux-musl
  arm64-apple-darwin)

set(LONEJSON_CAN_RUN_LUA_NATIVE_TESTS OFF)
if(NOT CMAKE_CROSSCOMPILING AND
    LONEJSON_TARGET_OS_LOWER STREQUAL LONEJSON_CMAKE_SYSTEM_NAME_LOWER)
  if(NOT LONEJSON_TARGET_OS_LOWER STREQUAL "linux" OR
      LONEJSON_TARGET_LIBC STREQUAL "gnu")
    set(LONEJSON_CAN_RUN_LUA_NATIVE_TESTS ON)
  endif()
endif()
function(lonejson_find_curl)
  if(LONEJSON_C_PKT_SYSTEMS_ROOT)
    get_filename_component(_cpkt_root "${LONEJSON_C_PKT_SYSTEMS_ROOT}" ABSOLUTE)
    set(_cpkt_curl_config
        "${_cpkt_root}/lib/cmake/CURL/CURLConfig.cmake")
    if(NOT IS_DIRECTORY "${_cpkt_root}")
      message(FATAL_ERROR
        "LONEJSON_C_PKT_SYSTEMS_ROOT does not exist: ${_cpkt_root}. "
        "Run the matching make deps-* target or unset "
        "LONEJSON_C_PKT_SYSTEMS_ROOT to use system curl.")
    endif()
    if(NOT EXISTS "${_cpkt_curl_config}")
      message(FATAL_ERROR
        "LONEJSON_C_PKT_SYSTEMS_ROOT is missing CURLConfig.cmake: "
        "${_cpkt_curl_config}. Use a c.pkt.systems v0.2.0 or newer "
        "bundle, or unset LONEJSON_C_PKT_SYSTEMS_ROOT to use system curl.")
    endif()
    set(LONEJSON_C_PKT_SYSTEMS_ROOT "${_cpkt_root}" CACHE PATH
        "Root directory for extracted c.pkt.systems dependency bundle content."
        FORCE)
    set(CURL_DIR "${_cpkt_root}/lib/cmake/CURL" CACHE PATH
        "Directory containing CURLConfig.cmake." FORCE)
    find_package(CURL CONFIG REQUIRED
      PATHS "${_cpkt_root}"
      NO_DEFAULT_PATH
      NO_CMAKE_FIND_ROOT_PATH)
  else()
    find_package(CURL REQUIRED)
  endif()
  if(NOT TARGET CURL::libcurl)
    message(FATAL_ERROR
      "CURL was found but did not provide the CURL::libcurl target.")
  endif()
endfunction()

if(LONEJSON_BUILD_WITH_CURL)
  lonejson_find_curl()
endif()

function(lonejson_find_openssl)
  if(LONEJSON_C_PKT_SYSTEMS_ROOT)
    get_filename_component(_cpkt_root "${LONEJSON_C_PKT_SYSTEMS_ROOT}" ABSOLUTE)
    set(_cpkt_openssl_config
        "${_cpkt_root}/lib/cmake/OpenSSL/OpenSSLConfig.cmake")
    if(NOT IS_DIRECTORY "${_cpkt_root}")
      message(FATAL_ERROR
        "LONEJSON_C_PKT_SYSTEMS_ROOT does not exist: ${_cpkt_root}. "
        "Run the matching make deps-* target or unset "
        "LONEJSON_C_PKT_SYSTEMS_ROOT to use system OpenSSL.")
    endif()
    if(NOT EXISTS "${_cpkt_openssl_config}")
      message(FATAL_ERROR
        "LONEJSON_C_PKT_SYSTEMS_ROOT is missing OpenSSLConfig.cmake: "
        "${_cpkt_openssl_config}. Use a c.pkt.systems v0.6.0 or newer "
        "bundle, or unset LONEJSON_C_PKT_SYSTEMS_ROOT to use system OpenSSL.")
    endif()
    set(LONEJSON_C_PKT_SYSTEMS_ROOT "${_cpkt_root}" CACHE PATH
        "Root directory for extracted c.pkt.systems dependency bundle content."
        FORCE)
    set(OpenSSL_DIR "${_cpkt_root}/lib/cmake/OpenSSL" CACHE PATH
        "Directory containing OpenSSLConfig.cmake." FORCE)
    find_package(OpenSSL CONFIG REQUIRED
      PATHS "${_cpkt_root}"
      NO_DEFAULT_PATH
      NO_CMAKE_FIND_ROOT_PATH)
  else()
    find_package(OpenSSL REQUIRED)
  endif()
  if(NOT TARGET OpenSSL::SSL OR NOT TARGET OpenSSL::Crypto)
    message(FATAL_ERROR
      "OpenSSL was found but did not provide OpenSSL::SSL and "
      "OpenSSL::Crypto targets.")
  endif()
endfunction()

if(LONEJSON_BUILD_WITH_OPENSSL)
  lonejson_find_openssl()
endif()

function(lonejson_target_include_curl target)
  if(CURL_INCLUDE_DIRS)
    target_include_directories(${target} SYSTEM PRIVATE ${CURL_INCLUDE_DIRS})
    return()
  endif()
  get_target_property(curl_include_dirs CURL::libcurl INTERFACE_INCLUDE_DIRECTORIES)
  if(curl_include_dirs)
    target_include_directories(${target} SYSTEM PRIVATE ${curl_include_dirs})
  endif()
endfunction()

set(LONEJSON_ENABLED_SANITIZER_COUNT 0)
if(LONEJSON_ENABLE_ASAN)
  math(EXPR LONEJSON_ENABLED_SANITIZER_COUNT
    "${LONEJSON_ENABLED_SANITIZER_COUNT} + 1")
endif()
if(LONEJSON_ENABLE_TSAN)
  math(EXPR LONEJSON_ENABLED_SANITIZER_COUNT
    "${LONEJSON_ENABLED_SANITIZER_COUNT} + 1")
endif()
if(LONEJSON_ENABLE_MSAN)
  math(EXPR LONEJSON_ENABLED_SANITIZER_COUNT
    "${LONEJSON_ENABLED_SANITIZER_COUNT} + 1")
endif()
if(LONEJSON_ENABLED_SANITIZER_COUNT GREATER 1)
  message(FATAL_ERROR
    "Enable at most one of LONEJSON_ENABLE_ASAN, LONEJSON_ENABLE_TSAN, or "
    "LONEJSON_ENABLE_MSAN at a time.")
endif()

if(LONEJSON_ENABLE_ASAN AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

if(LONEJSON_ENABLE_TSAN)
  if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    message(FATAL_ERROR "LONEJSON_ENABLE_TSAN requires Clang or GCC.")
  endif()
  add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
  add_link_options(-fsanitize=thread)
endif()

if(LONEJSON_ENABLE_MSAN)
  if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "LONEJSON_ENABLE_MSAN requires Clang.")
  endif()
  add_compile_options(
    -fsanitize=memory
    -fsanitize-memory-track-origins=2
    -fno-omit-frame-pointer
    -O1)
  add_link_options(
    -fsanitize=memory
    -fsanitize-memory-track-origins=2)
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
    lonejson_target_include_curl(${target})
  endif()
endforeach()

if(LONEJSON_BUILD_FUZZERS)
  if(LONEJSON_ENABLE_TSAN OR LONEJSON_ENABLE_MSAN)
    message(FATAL_ERROR
      "LONEJSON_BUILD_FUZZERS is incompatible with "
      "LONEJSON_ENABLE_TSAN and LONEJSON_ENABLE_MSAN.")
  endif()
  if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "LONEJSON_BUILD_FUZZERS requires Clang/libFuzzer.")
  endif()
  target_compile_options(lonejson_static PRIVATE
    -fsanitize=fuzzer-no-link,address,undefined
    -fno-omit-frame-pointer)
endif()

set_target_properties(lonejson_shared PROPERTIES
  OUTPUT_NAME lonejson
  VERSION ${PROJECT_VERSION}
  SOVERSION ${LONEJSON_ABI_VERSION}
)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set_target_properties(lonejson_shared PROPERTIES
    MACOSX_RPATH ON
    INSTALL_NAME_DIR "@rpath"
    BUILD_WITH_INSTALL_NAME_DIR ON)
endif()
set_target_properties(lonejson_static PROPERTIES
  OUTPUT_NAME lonejson
)

if(LONEJSON_BUILD_TESTS)
  enable_testing()
  if(LONEJSON_BUILD_WITH_CURL AND LONEJSON_TARGET_ID IN_LIST LONEJSON_RELEASE_TARGET_IDS)
    add_test(NAME lonejson_build_tree_curl_abi_tests
      COMMAND bash
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_curl_abi_symbols.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${LONEJSON_TARGET_ID}"
        "$<TARGET_FILE:lonejson_shared>"
        "$<TARGET_FILE:lonejson_static>"
        "build tree ${LONEJSON_TARGET_ID}")
    set_tests_properties(lonejson_build_tree_curl_abi_tests PROPERTIES
      LABELS "packaging;smoke;local"
      TIMEOUT 20)
  endif()

  add_executable(lonejson_tests tests/test_main.c)
  target_include_directories(lonejson_tests PRIVATE "${LONEJSON_PUBLIC_INCLUDE_DIR}")
  target_compile_definitions(lonejson_tests PRIVATE
    LONEJSON_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    LONEJSON_GENERATED_FIXTURE_DIR="${LONEJSON_GENERATED_FIXTURE_DIR}"
    LONEJSON_TEST_RUNTIME_HANDLE_CACHE_DRAIN=1)
  if(LONEJSON_BUILD_WITH_CURL)
    target_compile_definitions(lonejson_tests PRIVATE LONEJSON_WITH_CURL)
    target_link_libraries(lonejson_tests PRIVATE CURL::libcurl)
  endif()
  lonejson_configure_c_target(lonejson_tests)
  add_dependencies(lonejson_tests lonejson_generate_fixtures)
  add_test(NAME lonejson_tests COMMAND lonejson_tests)
  list(APPEND LONEJSON_STACK_USAGE_TARGETS lonejson_tests)

  add_executable(lonejson_map_cache_no_tls_tests
    tests/test_map_cache_no_tls.c)
  target_include_directories(lonejson_map_cache_no_tls_tests PRIVATE
    "${LONEJSON_PUBLIC_INCLUDE_DIR}")
  target_compile_definitions(lonejson_map_cache_no_tls_tests PRIVATE
    LONEJSON_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    LONEJSON_GENERATED_FIXTURE_DIR="${LONEJSON_GENERATED_FIXTURE_DIR}"
    LONEJSON_TEST_RUNTIME_HANDLE_CACHE_DRAIN=1)
  lonejson_configure_c_target(lonejson_map_cache_no_tls_tests)
  add_test(NAME lonejson_map_cache_no_tls_tests
    COMMAND lonejson_map_cache_no_tls_tests)
  list(APPEND LONEJSON_STACK_USAGE_TARGETS lonejson_map_cache_no_tls_tests)

  find_program(LONEJSON_BASH_EXECUTABLE bash)
  if(LONEJSON_BASH_EXECUTABLE AND LONEJSON_LUA_EXECUTABLE
      AND LONEJSON_LUAROCKS_EXECUTABLE)
    if(LONEJSON_CAN_RUN_LUA_NATIVE_TESTS)
      add_test(
        NAME lonejson_bench_baseline_history_tests
        COMMAND "${LONEJSON_BASH_EXECUTABLE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_bench_baseline_history.sh"
          "${CMAKE_CURRENT_SOURCE_DIR}"
          "${LONEJSON_LUA_EXECUTABLE}"
          "${LONEJSON_LUAROCKS_EXECUTABLE}")
      add_test(
        NAME lonejson_bench_retry_confirm_tests
        COMMAND "${LONEJSON_BASH_EXECUTABLE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_bench_retry_confirm.sh"
          "${CMAKE_CURRENT_SOURCE_DIR}"
          "${LONEJSON_LUA_EXECUTABLE}"
          "${LONEJSON_LUAROCKS_EXECUTABLE}")
      add_test(
        NAME lonejson_lua_legacy_uservalue_tests
        COMMAND "${LONEJSON_BASH_EXECUTABLE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lua_legacy_uservalue.sh"
          "${CMAKE_CURRENT_SOURCE_DIR}"
          "${LONEJSON_LUA_EXECUTABLE}"
          "${LONEJSON_LUAROCKS_EXECUTABLE}"
          "${CMAKE_CURRENT_BINARY_DIR}")
      add_test(
        NAME lonejson_lua_schema_cache_tests
        COMMAND "${LONEJSON_BASH_EXECUTABLE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lua_schema_cache.sh"
          "${CMAKE_CURRENT_SOURCE_DIR}"
          "${LONEJSON_LUA_EXECUTABLE}"
          "${LONEJSON_LUAROCKS_EXECUTABLE}"
          "${CMAKE_CURRENT_BINARY_DIR}")
      add_test(
        NAME lonejson_lua_encode_stats_tests
        COMMAND "${LONEJSON_BASH_EXECUTABLE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lua_encode_stats.sh"
          "${CMAKE_CURRENT_SOURCE_DIR}"
          "${LONEJSON_LUA_EXECUTABLE}"
          "${LONEJSON_LUAROCKS_EXECUTABLE}"
          "${CMAKE_CURRENT_BINARY_DIR}")
      add_test(
        NAME lonejson_lua_external_liblonejson_tests
        COMMAND "${LONEJSON_BASH_EXECUTABLE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lua_external_liblonejson.sh"
          "${CMAKE_CURRENT_SOURCE_DIR}"
          "${LONEJSON_LUA_EXECUTABLE}"
          "${LONEJSON_LUAROCKS_EXECUTABLE}"
          "${CMAKE_CURRENT_BINARY_DIR}")
      add_test(
        NAME lonejson_lua_rock_makefile_deps_tests
        COMMAND "${LONEJSON_BASH_EXECUTABLE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lua_rock_makefile_deps.sh"
          "${CMAKE_CURRENT_SOURCE_DIR}"
          "${LONEJSON_LUAROCKS_EXECUTABLE}")
    endif()
  endif()

  set(LONEJSON_TARGET_LUA_INCLUDE_DIR
    "${LONEJSON_C_PKT_SYSTEMS_ROOT}/include")
  set(LONEJSON_TARGET_LUA_LIBRARY
    "${LONEJSON_C_PKT_SYSTEMS_ROOT}/lib/liblua.so")
  if(LONEJSON_TARGET_OS_LOWER STREQUAL "linux" AND
      LONEJSON_C_PKT_SYSTEMS_ROOT AND
      EXISTS "${LONEJSON_TARGET_LUA_INCLUDE_DIR}/lua.h" AND
      EXISTS "${LONEJSON_TARGET_LUA_LIBRARY}")
    add_library(lonejson_lua_target_core MODULE src/lua/lonejson_lua.c)
    target_include_directories(lonejson_lua_target_core PRIVATE
      "${LONEJSON_PUBLIC_INCLUDE_DIR}"
      "${LONEJSON_TARGET_LUA_INCLUDE_DIR}")
    target_compile_definitions(lonejson_lua_target_core PRIVATE
      LONEJSON_TEST_FORCE_LUA_LEGACY_USERVALUE=1
      LONEJSON_TEST_MAP_ANALYSIS_COUNTER=1
      LONEJSON_TEST_LUA_ENCODE_STATS=1
      LONEJSON_TEST_LUA_CANDIDATE_INFO=1
      LONEJSON_TEST_LUA_SIMULATE_NO_MAXINTEGER=1)
    target_link_libraries(lonejson_lua_target_core PRIVATE
      lonejson_shared
      "${LONEJSON_TARGET_LUA_LIBRARY}")
    set_target_properties(lonejson_lua_target_core PROPERTIES
      PREFIX ""
      OUTPUT_NAME core
      LIBRARY_OUTPUT_DIRECTORY
        "${CMAKE_CURRENT_BINARY_DIR}/lua-target/lonejson"
      BUILD_RPATH
        "${CMAKE_CURRENT_BINARY_DIR};${LONEJSON_C_PKT_SYSTEMS_ROOT}/lib")
    if(LONEJSON_BUILD_WITH_CURL)
      target_compile_definitions(lonejson_lua_target_core PRIVATE
        LONEJSON_WITH_CURL)
      lonejson_target_include_curl(lonejson_lua_target_core)
      target_link_libraries(lonejson_lua_target_core PRIVATE CURL::libcurl)
    endif()
    lonejson_configure_lua_c_target(lonejson_lua_target_core)

    add_executable(lonejson_lua_target_runner
      tests/test_lua_embedded_runner.c)
    target_include_directories(lonejson_lua_target_runner PRIVATE
      "${LONEJSON_TARGET_LUA_INCLUDE_DIR}")
    target_link_libraries(lonejson_lua_target_runner PRIVATE
      "${LONEJSON_TARGET_LUA_LIBRARY}")
    set_target_properties(lonejson_lua_target_runner PROPERTIES
      BUILD_RPATH "${LONEJSON_C_PKT_SYSTEMS_ROOT}/lib")
    lonejson_configure_lua_c_target(lonejson_lua_target_runner)
    add_dependencies(lonejson_lua_target_runner lonejson_lua_target_core)
    add_test(
      NAME lonejson_lua_target_tests
      COMMAND lonejson_lua_target_runner
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lua.lua"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lua_fuzz.lua"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lua_schema_cache.lua"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lua_encode_stats.lua")
  endif()
  if(LONEJSON_BASH_EXECUTABLE)
    add_test(
      NAME lonejson_makefile_fuzz_targets_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_makefile_fuzz_targets.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_make_lifecycle_aliases_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_make_lifecycle_aliases.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_discover_target_tools_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_discover_target_tools.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_darwin_macho_metadata_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_darwin_macho_metadata.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_darwin_linker_route_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_darwin_linker_route.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_path_index_formatting_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_path_index_formatting.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_c_pkt_systems_fetch_retry_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_fetch_c_pkt_systems.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_cmake_threads_optional_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cmake_threads_optional.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_cmake_c_pkt_systems_root_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cmake_c_pkt_systems_root.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_test_all_clang_optional_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_test_all_clang_optional.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_check_clang_sanitizer_support_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_check_clang_sanitizer_support.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_cmake_fuzz_sanitizer_conflict_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cmake_fuzz_sanitizer_conflicts.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_release_werror_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_release_werror.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_COMMAND}"
        "${CMAKE_GENERATOR}")
    add_test(
      NAME lonejson_source_release_tarball_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_source_release_tarball.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_COMMAND}")
    add_test(
      NAME lonejson_lua_src_rock_privacy_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lua_src_rock_privacy.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_lua_source_stage_manifest_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lua_source_stage_manifest.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_release_artifact_verify_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_release_artifact_verify.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_release_archive_verify_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_release_archive_verify.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_lua_native_test_target_filter_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lua_native_test_target_filter.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_COMMAND}"
        "${CMAKE_GENERATOR}")
    add_test(
      NAME lonejson_run_release_matrix_darwin_target_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_run_release_matrix_darwin_target.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_release_checksum_manifest_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_release_checksum_manifest.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
      NAME lonejson_ctest_metadata_tests
      COMMAND "${LONEJSON_BASH_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_ctest_metadata.sh"
        "${CMAKE_CURRENT_BINARY_DIR}")
  endif()

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
  add_test(
    NAME lonejson_single_header_strict_warning_build_tests
    COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_CURRENT_BINARY_DIR}"
      --target lonejson_single_header_strict_warning_tests)
  set_tests_properties(lonejson_single_header_strict_warning_build_tests
    PROPERTIES RUN_SERIAL TRUE)

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
        -D_FILE_OFFSET_BITS=64
        -I "${LONEJSON_SINGLE_HEADER_ALIAS_INCLUDE_DIR}"
        -c "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_single_header_strict_warnings.c"
        -o "${LONEJSON_SINGLE_HEADER_STRICT_CLANG_OBJECT}"
      DEPENDS
        package-single-header
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_single_header_strict_warnings.c"
      VERBATIM)
    add_custom_target(lonejson_single_header_strict_clang ALL
      DEPENDS "${LONEJSON_SINGLE_HEADER_STRICT_CLANG_OBJECT}")
    add_test(
      NAME lonejson_single_header_strict_clang_build_tests
      COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_CURRENT_BINARY_DIR}"
        --target lonejson_single_header_strict_clang)
    set_tests_properties(lonejson_single_header_strict_clang_build_tests
      PROPERTIES RUN_SERIAL TRUE)
  endif()

  function(lonejson_add_single_header_config_test target)
    add_executable(${target} tests/test_single_header_config_matrix.c)
    target_include_directories(${target} PRIVATE
      "${LONEJSON_SINGLE_HEADER_ALIAS_INCLUDE_DIR}")
    add_dependencies(${target} package-single-header)
    if(ARGN)
      target_compile_definitions(${target} PRIVATE ${ARGN})
    endif()
    lonejson_configure_c_target(${target})
    add_test(NAME ${target} COMMAND ${target})
    list(APPEND LONEJSON_STACK_USAGE_TARGETS ${target})
    set(LONEJSON_STACK_USAGE_TARGETS "${LONEJSON_STACK_USAGE_TARGETS}"
      PARENT_SCOPE)
  endfunction()

  lonejson_add_single_header_config_test(
    lonejson_single_header_config_default)
  lonejson_add_single_header_config_test(
    lonejson_single_header_config_omit_protocol
    TEST_OMIT_PROTOCOL_FRAMING)
  lonejson_add_single_header_config_test(
    lonejson_single_header_config_lj_implementation
    TEST_LJ_IMPLEMENTATION_ALIAS
    TEST_USE_SHORT_NAMES)
  lonejson_add_single_header_config_test(
    lonejson_single_header_config_lj_config_aliases
    TEST_LJ_CONFIG_ALIASES
    TEST_USE_SHORT_NAMES)
  lonejson_add_single_header_config_test(
    lonejson_single_header_config_short_names_disabled
    TEST_DISABLE_SHORT_NAMES)

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
    NAME lonejson_header_abi_version_tests
    COMMAND ${CMAKE_COMMAND}
      -DLONEJSON_PUBLIC_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/include/lonejson.h
      -DLONEJSON_ABI_VERSION=${LONEJSON_ABI_VERSION}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_header_abi_version.cmake)
  add_test(
    NAME lonejson_single_header_release_version_tests
    COMMAND ${CMAKE_COMMAND}
      -DLONEJSON_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -DLONEJSON_RELEASE_VERSION=${LONEJSON_RESOLVED_VERSION}
      -DLONEJSON_SINGLE_HEADER_DIST_GZ=${LONEJSON_SINGLE_HEADER_DIST_GZ}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_single_header_release_version.cmake)

  get_property(lonejson_registered_tests DIRECTORY PROPERTY TESTS)
  foreach(lonejson_registered_test IN LISTS lonejson_registered_tests)
    get_property(lonejson_test_has_timeout TEST
      "${lonejson_registered_test}" PROPERTY TIMEOUT SET)
    if(NOT lonejson_test_has_timeout)
      set_property(TEST "${lonejson_registered_test}" PROPERTY TIMEOUT 120)
    endif()
    get_property(lonejson_test_has_labels TEST
      "${lonejson_registered_test}" PROPERTY LABELS SET)
    if(NOT lonejson_test_has_labels)
      set_property(TEST "${lonejson_registered_test}" PROPERTY LABELS local)
    endif()
  endforeach()
endif()

if(LONEJSON_BUILD_EXAMPLES)
  set(LONEJSON_LINKED_EXAMPLES
    array_stream
    generator_pull
    json_value_buffer
    json_value_nested_parse
    json_value_parse
    json_value_source
    sse_json_value_stream
    json_value_visitor
    nullable_fields
    parse_file
    parse_reader
    push_parser
    spooled_text
    spooled_bytes
    source_text
    source_bytes
    serialize_string
    serialize_file
    value_rewrite_replace_with
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
  # Keep the core JSON benchmark focused on JSON hot paths; protocol framing has
  # separate unit/fuzz coverage and would otherwise perturb single-header layout.
  target_compile_definitions(lonejson_bench PRIVATE
    LONEJSON_IMPLEMENTATION
    LONEJSON__OMIT_PROTOCOL_FRAMING_IMPL)
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

  add_executable(lonejson_fuzz_array_stream fuzz/fuzz_array_stream.c)
  target_include_directories(lonejson_fuzz_array_stream PRIVATE "${LONEJSON_PUBLIC_INCLUDE_DIR}")
  target_link_libraries(lonejson_fuzz_array_stream PRIVATE lonejson)
  target_compile_options(lonejson_fuzz_array_stream PRIVATE
    -fsanitize=fuzzer-no-link,address,undefined
    -fno-omit-frame-pointer)
  target_link_options(lonejson_fuzz_array_stream PRIVATE
    -fsanitize=fuzzer,address,undefined)
  lonejson_configure_c_target(lonejson_fuzz_array_stream)

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

  add_executable(lonejson_fuzz_path_value_visitor
    fuzz/fuzz_path_value_visitor.c)
  target_include_directories(lonejson_fuzz_path_value_visitor
    PRIVATE "${LONEJSON_PUBLIC_INCLUDE_DIR}")
  target_link_libraries(lonejson_fuzz_path_value_visitor PRIVATE lonejson)
  target_compile_options(lonejson_fuzz_path_value_visitor PRIVATE
    -fsanitize=fuzzer-no-link,address,undefined
    -fno-omit-frame-pointer)
  target_link_options(lonejson_fuzz_path_value_visitor PRIVATE
    -fsanitize=fuzzer,address,undefined)
  lonejson_configure_c_target(lonejson_fuzz_path_value_visitor)

  add_executable(lonejson_fuzz_candidate_stream
    fuzz/fuzz_candidate_stream.c)
  target_include_directories(lonejson_fuzz_candidate_stream
    PRIVATE "${LONEJSON_PUBLIC_INCLUDE_DIR}")
  target_link_libraries(lonejson_fuzz_candidate_stream PRIVATE lonejson)
  target_compile_options(lonejson_fuzz_candidate_stream PRIVATE
    -fsanitize=fuzzer-no-link,address,undefined
    -fno-omit-frame-pointer)
  target_link_options(lonejson_fuzz_candidate_stream PRIVATE
    -fsanitize=fuzzer,address,undefined)
  lonejson_configure_c_target(lonejson_fuzz_candidate_stream)

  add_executable(lonejson_fuzz_value_rewrite fuzz/fuzz_value_rewrite.c)
  target_include_directories(lonejson_fuzz_value_rewrite PRIVATE "${LONEJSON_PUBLIC_INCLUDE_DIR}")
  target_link_libraries(lonejson_fuzz_value_rewrite PRIVATE lonejson)
  target_compile_options(lonejson_fuzz_value_rewrite PRIVATE
    -fsanitize=fuzzer-no-link,address,undefined
    -fno-omit-frame-pointer)
  target_link_options(lonejson_fuzz_value_rewrite PRIVATE
    -fsanitize=fuzzer,address,undefined)
  lonejson_configure_c_target(lonejson_fuzz_value_rewrite)

  add_executable(lonejson_fuzz_reader_stream_generator
    fuzz/fuzz_reader_stream_generator.c)
  target_include_directories(lonejson_fuzz_reader_stream_generator
    PRIVATE "${LONEJSON_PUBLIC_INCLUDE_DIR}")
  target_link_libraries(lonejson_fuzz_reader_stream_generator
    PRIVATE lonejson)
  target_compile_options(lonejson_fuzz_reader_stream_generator PRIVATE
    -fsanitize=fuzzer-no-link,address,undefined
    -fno-omit-frame-pointer)
  target_link_options(lonejson_fuzz_reader_stream_generator PRIVATE
    -fsanitize=fuzzer,address,undefined)
  lonejson_configure_c_target(lonejson_fuzz_reader_stream_generator)

  add_executable(lonejson_fuzz_writer_generator_backpressure
    fuzz/fuzz_writer_generator_backpressure.c)
  target_include_directories(lonejson_fuzz_writer_generator_backpressure
    PRIVATE "${LONEJSON_PUBLIC_INCLUDE_DIR}")
  target_link_libraries(lonejson_fuzz_writer_generator_backpressure
    PRIVATE lonejson)
  target_compile_options(lonejson_fuzz_writer_generator_backpressure PRIVATE
    -fsanitize=fuzzer-no-link,address,undefined
    -fno-omit-frame-pointer)
  target_link_options(lonejson_fuzz_writer_generator_backpressure PRIVATE
    -fsanitize=fuzzer,address,undefined)
  lonejson_configure_c_target(lonejson_fuzz_writer_generator_backpressure)

  add_executable(lonejson_fuzz_writer_value_stream
    fuzz/fuzz_writer_value_stream.c)
  target_include_directories(lonejson_fuzz_writer_value_stream
    PRIVATE "${LONEJSON_PUBLIC_INCLUDE_DIR}")
  target_link_libraries(lonejson_fuzz_writer_value_stream PRIVATE lonejson)
  target_compile_options(lonejson_fuzz_writer_value_stream PRIVATE
    -fsanitize=fuzzer-no-link,address,undefined)
  target_link_options(lonejson_fuzz_writer_value_stream PRIVATE
    -fsanitize=fuzzer,address,undefined)
  lonejson_configure_c_target(lonejson_fuzz_writer_value_stream)

  add_executable(lonejson_fuzz_protocol_framing
    fuzz/fuzz_protocol_framing.c)
  target_include_directories(lonejson_fuzz_protocol_framing
    PRIVATE "${LONEJSON_PUBLIC_INCLUDE_DIR}")
  target_link_libraries(lonejson_fuzz_protocol_framing
    PRIVATE lonejson)
  target_compile_options(lonejson_fuzz_protocol_framing PRIVATE
    -fsanitize=fuzzer-no-link,address,undefined
    -fno-omit-frame-pointer)
  target_link_options(lonejson_fuzz_protocol_framing PRIVATE
    -fsanitize=fuzzer,address,undefined)
  lonejson_configure_c_target(lonejson_fuzz_protocol_framing)

  add_executable(lonejson_fuzz_fixed_string_paths
    fuzz/fuzz_fixed_string_paths.c)
  target_include_directories(lonejson_fuzz_fixed_string_paths
    PRIVATE "${LONEJSON_PUBLIC_INCLUDE_DIR}")
  target_link_libraries(lonejson_fuzz_fixed_string_paths PRIVATE lonejson)
  target_compile_options(lonejson_fuzz_fixed_string_paths PRIVATE
    -fsanitize=fuzzer-no-link,address,undefined
    -fno-omit-frame-pointer)
  target_link_options(lonejson_fuzz_fixed_string_paths PRIVATE
    -fsanitize=fuzzer,address,undefined)
  lonejson_configure_c_target(lonejson_fuzz_fixed_string_paths)

  add_executable(lonejson_fuzz_alloc_ceiling
    fuzz/fuzz_alloc_ceiling.c)
  target_include_directories(lonejson_fuzz_alloc_ceiling
    PRIVATE "${LONEJSON_PUBLIC_INCLUDE_DIR}")
  target_link_libraries(lonejson_fuzz_alloc_ceiling PRIVATE lonejson)
  target_compile_options(lonejson_fuzz_alloc_ceiling PRIVATE
    -fsanitize=fuzzer-no-link,address,undefined
    -fno-omit-frame-pointer)
  target_link_options(lonejson_fuzz_alloc_ceiling PRIVATE
    -fsanitize=fuzzer,address,undefined)
  lonejson_configure_c_target(lonejson_fuzz_alloc_ceiling)

  add_executable(lonejson_fuzz_parser_boundaries
    fuzz/fuzz_parser_boundaries.c)
  target_include_directories(lonejson_fuzz_parser_boundaries
    PRIVATE "${LONEJSON_PUBLIC_INCLUDE_DIR}")
  target_link_libraries(lonejson_fuzz_parser_boundaries PRIVATE lonejson)
  target_compile_options(lonejson_fuzz_parser_boundaries PRIVATE
    -fsanitize=fuzzer-no-link,address,undefined
    -fno-omit-frame-pointer)
  target_link_options(lonejson_fuzz_parser_boundaries PRIVATE
    -fsanitize=fuzzer,address,undefined)
  lonejson_configure_c_target(lonejson_fuzz_parser_boundaries)
endif()

if(CLANG_FORMAT_BIN)
  add_custom_target(format
    COMMAND "${CLANG_FORMAT_BIN}" -i
      "${CMAKE_CURRENT_SOURCE_DIR}/include/lonejson.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/lonejson.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/lonejson_impl.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/lonejson_internal.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/impl/00_prelude.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/impl/10_source_runtime.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/impl/11_json_value_io.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/impl/12_json_value_parse.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/impl/13_json_value_visit.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/impl/14_json_value_api.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/impl/20_parser.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/impl/20_parser_map.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/impl/21_parser_values.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/impl/22_parser_actions.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/impl/22_parser_engine.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/impl/23_serializer_io.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/impl/24_serializer.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/impl/25_json_pull.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/impl/30_api.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/impl/32_generator_api.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/impl/33_array_stream_api.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/impl/34_protocol_framing_api.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/impl/35_json_pull_impl.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/impl/36_curl_api.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/impl/37_array_rewrite_api.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/bench/lonejson_bench.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/lua/lonejson_lua.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/lua/lonejson_lua_module.inc.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/lua/lonejson_lua_record.inc.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/lua/lonejson_lua_schema.inc.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/lua/lonejson_lua_schema_methods.inc.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/lua/lonejson_lua_stream_spool.inc.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/lua/lonejson_lua_support.inc.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/lua/lonejson_lua_visitor_facade.inc.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lua_embedded_runner.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_validate.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_mapped_parse.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_array_stream.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_json_value.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_value_visitor.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_path_value_visitor.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_candidate_stream.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_value_rewrite.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_reader_stream_generator.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_writer_generator_backpressure.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_writer_value_stream.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_protocol_framing.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_fixed_string_paths.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_alloc_ceiling.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_parser_boundaries.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_allocator_visitor.inc.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_array_rewrite.inc.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_array_stream.inc.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_curl_misc.inc.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_fixtures_public.inc.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_generator_omit.inc.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_json_value_sources.inc.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_main.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_parse_serialize.inc.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_protocol_framing.inc.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_support.inc.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_value_rewrite.inc.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_writer.inc.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_single_header_config_matrix.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_link_consumer.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_short_names.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_short_names_disabled.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_single_header_strict_warnings.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/examples/parse_string.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/examples/array_stream.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/examples/nullable_fields.c"
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
      "${CMAKE_CURRENT_SOURCE_DIR}/examples/value_rewrite_replace_with.c"
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
    -DLONEJSON_BUILD_WITH_CURL=${LONEJSON_BUILD_WITH_CURL}
    -DLONEJSON_BUILD_WITH_OPENSSL=${LONEJSON_BUILD_WITH_OPENSSL}
    -DLONEJSON_STRIP=${CMAKE_STRIP}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/package_archive.cmake
  DEPENDS lonejson_shared lonejson_static
  VERBATIM
)

add_custom_target(package-darwin-smoke-bundle
  COMMAND "${CMAKE_COMMAND}"
    -DLONEJSON_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
    -DLONEJSON_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
    -DLONEJSON_VERSION=${LONEJSON_RESOLVED_VERSION}
    -DLONEJSON_TARGET_ID=${LONEJSON_TARGET_ID}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/package_darwin_smoke_bundle.cmake
  DEPENDS package-archive
  VERBATIM
)
foreach(example_name
        array_stream
        fixed_storage
        generator_pull
        json_value_buffer
        sse_json_value_stream
        json_value_parse
        json_value_source
        json_value_visitor
        nullable_fields
        parse_file
        parse_reader
        parse_string
        push_parser
        serialize_file
        serialize_jsonl
        serialize_string
        source_bytes
        source_text
        spooled_bytes
        spooled_text
        value_rewrite_replace_with)
  if(TARGET "example_${example_name}")
    add_dependencies(package-darwin-smoke-bundle "example_${example_name}")
  endif()
endforeach()

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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/10_source_runtime.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/11_json_value_io.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/12_json_value_parse.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/13_json_value_visit.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/14_json_value_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/20_parser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/20_parser_map.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/21_parser_values.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/22_parser_actions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/22_parser_engine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/23_serializer_io.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/24_serializer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/25_json_pull.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/30_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/32_generator_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/33_array_stream_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/34_protocol_framing_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/35_json_pull_impl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/36_curl_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/37_array_rewrite_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/38_writer_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/39_value_rewrite_api.h
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
