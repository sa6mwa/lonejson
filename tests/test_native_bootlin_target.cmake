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

function(lonejson_assert_native_target host_processor expected_arch
         expected_processor expected_emulator)
  set(CMAKE_HOST_SYSTEM_PROCESSOR "${host_processor}")
  lonejson_detect_native_bootlin_target(
    lonejson_target_id
    lonejson_processor
    lonejson_arch
    lonejson_libc
    lonejson_emulator)
  if(NOT lonejson_target_id STREQUAL
      "${expected_arch}-linux-${lonejson_expected_libc}")
    message(FATAL_ERROR
      "${host_processor} selected ${lonejson_target_id}, expected ${expected_arch}-linux-${lonejson_expected_libc}")
  endif()
  if(NOT lonejson_processor STREQUAL "${expected_processor}" OR
     NOT lonejson_arch STREQUAL "${expected_arch}" OR
     NOT lonejson_libc STREQUAL "${lonejson_expected_libc}" OR
     NOT lonejson_emulator STREQUAL "${expected_emulator}")
    message(FATAL_ERROR "native Bootlin target metadata is inconsistent for ${host_processor}")
  endif()
endfunction()

lonejson_assert_native_target(x86_64 x86_64 x86_64 qemu-x86_64)
lonejson_assert_native_target(amd64 x86_64 x86_64 qemu-x86_64)
lonejson_assert_native_target(aarch64 aarch64 aarch64 qemu-aarch64)
lonejson_assert_native_target(arm64 aarch64 aarch64 qemu-aarch64)
lonejson_assert_native_target(armv7l armhf arm qemu-arm)
lonejson_assert_native_target(armhf armhf arm qemu-arm)
