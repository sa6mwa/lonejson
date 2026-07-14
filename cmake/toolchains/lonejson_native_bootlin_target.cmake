# Resolve the Bootlin target that matches the machine running a normal Linux
# configuration.  Keep this separate from compiler setup so both the native
# preset toolchain and a plain CMake invocation share the same host policy.
function(lonejson_detect_native_bootlin_target out_target_id out_processor
         out_target_arch out_target_libc out_emulator)
  set(_lonejson_native_processor "${CMAKE_HOST_SYSTEM_PROCESSOR}")
  if(NOT _lonejson_native_processor)
    execute_process(
      COMMAND uname -m
      RESULT_VARIABLE _lonejson_uname_result
      OUTPUT_VARIABLE _lonejson_native_processor
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  endif()
  string(TOLOWER "${_lonejson_native_processor}" _lonejson_native_processor)
  if(_lonejson_native_processor STREQUAL "x86_64" OR
      _lonejson_native_processor STREQUAL "amd64")
    set(_lonejson_native_target_arch x86_64)
    set(_lonejson_native_cmake_processor x86_64)
    set(_lonejson_native_emulator qemu-x86_64)
  elseif(_lonejson_native_processor STREQUAL "aarch64" OR
         _lonejson_native_processor STREQUAL "arm64")
    set(_lonejson_native_target_arch aarch64)
    set(_lonejson_native_cmake_processor aarch64)
    set(_lonejson_native_emulator qemu-aarch64)
  elseif(_lonejson_native_processor MATCHES "^armv[67]l$" OR
         _lonejson_native_processor STREQUAL "armhf")
    set(_lonejson_native_target_arch armhf)
    set(_lonejson_native_cmake_processor arm)
    set(_lonejson_native_emulator qemu-arm)
  else()
    message(FATAL_ERROR
      "unsupported Linux host processor for native Bootlin workflow: ${_lonejson_native_processor}")
  endif()

  execute_process(
    COMMAND ldd --version
    RESULT_VARIABLE _lonejson_ldd_result
    OUTPUT_VARIABLE _lonejson_ldd_output
    ERROR_VARIABLE _lonejson_ldd_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE)
  string(TOLOWER "${_lonejson_ldd_output}${_lonejson_ldd_error}" _lonejson_ldd_text)
  if(_lonejson_ldd_text MATCHES "musl")
    set(_lonejson_native_libc musl)
  elseif(_lonejson_ldd_text MATCHES "glibc|gnu c library|gnu libc")
    set(_lonejson_native_libc gnu)
  else()
    message(FATAL_ERROR
      "unable to determine Linux host libc from ldd --version; expected musl or glibc")
  endif()

  set(${out_target_id}
    "${_lonejson_native_target_arch}-linux-${_lonejson_native_libc}" PARENT_SCOPE)
  set(${out_processor} "${_lonejson_native_cmake_processor}" PARENT_SCOPE)
  set(${out_target_arch} "${_lonejson_native_target_arch}" PARENT_SCOPE)
  set(${out_target_libc} "${_lonejson_native_libc}" PARENT_SCOPE)
  set(${out_emulator} "${_lonejson_native_emulator}" PARENT_SCOPE)
endfunction()
