if(NOT DEFINED LONEJSON_ROOT OR LONEJSON_ROOT STREQUAL "")
  message(FATAL_ERROR "LONEJSON_ROOT is required")
endif()

include("${LONEJSON_ROOT}/cmake/toolchains/lonejson_native_bootlin_target.cmake")

execute_process(
  COMMAND ldd --version
  OUTPUT_VARIABLE lonejson_ldd_output
  ERROR_VARIABLE lonejson_ldd_error
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_STRIP_TRAILING_WHITESPACE)
string(TOLOWER "${lonejson_ldd_output}${lonejson_ldd_error}" lonejson_ldd_text)
if(lonejson_ldd_text MATCHES "musl")
  set(lonejson_expected_libc musl)
else()
  set(lonejson_expected_libc gnu)
endif()

function(lonejson_assert_native_target host_processor)
  set(CMAKE_HOST_SYSTEM_PROCESSOR "${host_processor}")
  lonejson_detect_native_bootlin_target(
    lonejson_target_id
    lonejson_processor
    lonejson_arch
    lonejson_libc
    lonejson_emulator)
  if(NOT lonejson_target_id STREQUAL "x86_64-linux-gnu")
    message(FATAL_ERROR
      "${host_processor} selected ${lonejson_target_id}, expected x86_64-linux-gnu")
  endif()
  if(NOT lonejson_processor STREQUAL "x86_64" OR
     NOT lonejson_arch STREQUAL "x86_64" OR
     NOT lonejson_libc STREQUAL "gnu" OR
     NOT lonejson_emulator STREQUAL "qemu-x86_64")
    message(FATAL_ERROR "native Bootlin target metadata is inconsistent for ${host_processor}")
  endif()
endfunction()

lonejson_assert_native_target(x86_64)
lonejson_assert_native_target(amd64)

set(lonejson_unsupported_check "${CMAKE_CURRENT_BINARY_DIR}/native-unsupported-check.cmake")
file(WRITE "${lonejson_unsupported_check}" "
include(\"${LONEJSON_ROOT}/cmake/toolchains/lonejson_native_bootlin_target.cmake\")
set(CMAKE_HOST_SYSTEM_PROCESSOR \"\$ENV{LONEJSON_TEST_HOST_PROCESSOR}\")
lonejson_detect_native_bootlin_target(
  lonejson_target_id
  lonejson_processor
  lonejson_arch
  lonejson_libc
  lonejson_emulator)
")
function(lonejson_assert_unsupported_host host_processor expected_text)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
      "LONEJSON_TEST_HOST_PROCESSOR=${host_processor}"
      "${CMAKE_COMMAND}" -P "${lonejson_unsupported_check}"
    RESULT_VARIABLE lonejson_unsupported_result
    OUTPUT_VARIABLE lonejson_unsupported_output
    ERROR_VARIABLE lonejson_unsupported_error)
  if(lonejson_unsupported_result EQUAL 0)
    message(FATAL_ERROR "${host_processor} must not select a native Bootlin collection")
  endif()
  if(NOT "${lonejson_unsupported_output}${lonejson_unsupported_error}" MATCHES
      "${expected_text}")
    message(FATAL_ERROR
      "${host_processor} failure should explain the unsupported native Bootlin host")
  endif()
endfunction()

lonejson_assert_unsupported_host(aarch64 "x86_64 glibc Linux host")
lonejson_assert_unsupported_host(arm64 "x86_64 glibc Linux host")
lonejson_assert_unsupported_host(armv7l "x86_64 glibc Linux host")
lonejson_assert_unsupported_host(arm "x86_64 glibc Linux host")
lonejson_assert_unsupported_host(armhf "x86_64 glibc Linux host")
lonejson_assert_unsupported_host(armv6l "x86_64 glibc Linux host")
