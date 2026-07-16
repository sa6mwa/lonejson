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

  string(TOLOWER "${_lonejson_native_processor}" _lonejson_native_processor)
  if(_lonejson_native_processor STREQUAL "amd64")
    set(_lonejson_native_processor x86_64)
  endif()
  if(NOT _lonejson_native_processor STREQUAL "x86_64" OR
      NOT _lonejson_native_libc STREQUAL "gnu")
    message(FATAL_ERROR
      "native Bootlin workflow requires an x86_64 glibc Linux host because the pinned compiler executables are x86_64 glibc binaries; got ${_lonejson_native_processor}/${_lonejson_native_libc}")
  endif()

  set(_lonejson_native_target_arch x86_64)
  set(_lonejson_native_cmake_processor x86_64)
  set(_lonejson_native_emulator qemu-x86_64)

  set(${out_target_id}
    "${_lonejson_native_target_arch}-linux-${_lonejson_native_libc}" PARENT_SCOPE)
  set(${out_processor} "${_lonejson_native_cmake_processor}" PARENT_SCOPE)
  set(${out_target_arch} "${_lonejson_native_target_arch}" PARENT_SCOPE)
  set(${out_target_libc} "${_lonejson_native_libc}" PARENT_SCOPE)
  set(${out_emulator} "${_lonejson_native_emulator}" PARENT_SCOPE)
endfunction()
