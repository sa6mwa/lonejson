function(lonejson_configure_bootlin_toolchain target_id processor target_arch target_libc emulator)
  get_filename_component(_lonejson_source_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.." ABSOLUTE)
  set(_lonejson_resolver "${_lonejson_source_dir}/scripts/cpkt-toolchains.sh")
  if(NOT EXISTS "${_lonejson_resolver}")
    message(FATAL_ERROR "missing lifecycle toolchain resolver: ${_lonejson_resolver}")
  endif()

  execute_process(
    COMMAND "${_lonejson_resolver}" ensure "${target_id}"
    RESULT_VARIABLE _lonejson_result
    OUTPUT_VARIABLE _lonejson_description
    ERROR_VARIABLE _lonejson_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE)
  if(NOT _lonejson_result EQUAL 0)
    message(FATAL_ERROR
      "Bootlin ${target_id} toolchain provisioning failed.\n${_lonejson_error}")
  endif()

  string(REGEX MATCH "status=([^\r\n]+)" _lonejson_status_match "${_lonejson_description}")
  if(NOT _lonejson_status_match OR NOT "${CMAKE_MATCH_1}" STREQUAL "ready")
    message(FATAL_ERROR
      "Bootlin resolver did not provision a ready ${target_id} collection.\n${_lonejson_description}")
  endif()

  foreach(_lonejson_key root prefix target_triple sysroot libc cc cxx ld ar ranlib strip nm objcopy objdump addr2line gdb readelf bin)
    string(REGEX MATCH "${_lonejson_key}=([^\r\n]+)" _lonejson_match "${_lonejson_description}")
    if(NOT _lonejson_match)
      message(FATAL_ERROR "Bootlin resolver did not report ${_lonejson_key} for ${target_id}")
    endif()
    set(_lonejson_${_lonejson_key} "${CMAKE_MATCH_1}")
  endforeach()

  string(TOLOWER "${CMAKE_HOST_SYSTEM_PROCESSOR}" _lonejson_host_processor)
  if(_lonejson_host_processor STREQUAL "amd64")
    set(_lonejson_host_processor "x86_64")
  elseif(_lonejson_host_processor STREQUAL "arm64")
    set(_lonejson_host_processor "aarch64")
  endif()
  string(TOLOWER "${target_arch}" _lonejson_target_arch)
  if(_lonejson_target_arch STREQUAL "amd64")
    set(_lonejson_target_arch "x86_64")
  elseif(_lonejson_target_arch STREQUAL "arm64")
    set(_lonejson_target_arch "aarch64")
  endif()

  string(TOLOWER "${target_libc}" _lonejson_target_libc)
  set(_lonejson_host_libc "")
  if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    execute_process(
      COMMAND ldd --version
      RESULT_VARIABLE _lonejson_host_ldd_result
      OUTPUT_VARIABLE _lonejson_host_ldd_output
      ERROR_VARIABLE _lonejson_host_ldd_error
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_STRIP_TRAILING_WHITESPACE)
    string(TOLOWER
      "${_lonejson_host_ldd_output}${_lonejson_host_ldd_error}"
      _lonejson_host_ldd_text)
    if(_lonejson_host_ldd_result EQUAL 0 AND
        _lonejson_host_ldd_text MATCHES "musl")
      set(_lonejson_host_libc "musl")
    elseif(_lonejson_host_ldd_result EQUAL 0 AND
        _lonejson_host_ldd_text MATCHES "glibc|gnu c library|gnu libc")
      set(_lonejson_host_libc "gnu")
    endif()
  endif()

  # A Linux binary is executable without QEMU only when both its processor and
  # dynamic-loader ABI match the host.  In particular, an x86_64 musl binary
  # cannot run directly on an x86_64 glibc host without the musl loader.
  set(_lonejson_is_native_runtime FALSE)
  if(_lonejson_host_processor STREQUAL _lonejson_target_arch AND
      ("${_lonejson_target_libc}" STREQUAL "" OR
       "${_lonejson_host_libc}" STREQUAL "${_lonejson_target_libc}"))
    set(_lonejson_is_native_runtime TRUE)
  else()
    set(CMAKE_SYSTEM_NAME Linux CACHE STRING "" FORCE)
    set(CMAKE_SYSTEM_PROCESSOR "${processor}" CACHE STRING "" FORCE)
    set(CMAKE_SYSTEM_NAME Linux PARENT_SCOPE)
    set(CMAKE_SYSTEM_PROCESSOR "${processor}" PARENT_SCOPE)
  endif()
  set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
  set(CMAKE_SYSROOT "${_lonejson_sysroot}" CACHE PATH "" FORCE)
  set(CMAKE_C_COMPILER_TARGET "${_lonejson_target_triple}" CACHE STRING "" FORCE)
  set(CMAKE_CXX_COMPILER_TARGET "${_lonejson_target_triple}" CACHE STRING "" FORCE)
  set(CMAKE_LINKER "${_lonejson_ld}" CACHE FILEPATH "" FORCE)
  set(CMAKE_AR "${_lonejson_ar}" CACHE FILEPATH "" FORCE)
  set(CMAKE_RANLIB "${_lonejson_ranlib}" CACHE FILEPATH "" FORCE)
  set(CMAKE_STRIP "${_lonejson_strip}" CACHE FILEPATH "" FORCE)
  set(CMAKE_NM "${_lonejson_nm}" CACHE FILEPATH "" FORCE)
  set(CMAKE_OBJCOPY "${_lonejson_objcopy}" CACHE FILEPATH "" FORCE)
  set(CMAKE_OBJDUMP "${_lonejson_objdump}" CACHE FILEPATH "" FORCE)
  set(CMAKE_ADDR2LINE "${_lonejson_addr2line}" CACHE FILEPATH "" FORCE)
  set(CMAKE_READELF "${_lonejson_readelf}" CACHE FILEPATH "" FORCE)

  set(CMAKE_C_COMPILER "${_lonejson_cc}" CACHE FILEPATH "" FORCE)
  set(LONEJSON_DEFAULT_C_COMPILER "bootlin-gcc" CACHE INTERNAL "" FORCE)
  message(STATUS "Using the pinned Bootlin GCC compiler for ${target_id}")

  file(REAL_PATH "${CMAKE_C_COMPILER}" _lonejson_actual_compiler)
  file(REAL_PATH "${_lonejson_cc}" _lonejson_expected_compiler)
  if(NOT "${_lonejson_actual_compiler}" STREQUAL "${_lonejson_expected_compiler}")
    message(FATAL_ERROR
      "${target_id} must use its pinned Bootlin compiler: expected ${_lonejson_cc}, got ${CMAKE_C_COMPILER}")
  endif()

  execute_process(
    COMMAND "${CMAKE_C_COMPILER}" -dumpmachine
    RESULT_VARIABLE _lonejson_compiler_result
    OUTPUT_VARIABLE _lonejson_compiler_triple
    ERROR_VARIABLE _lonejson_compiler_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE)
  if(NOT _lonejson_compiler_result EQUAL 0 OR
      NOT "${_lonejson_compiler_triple}" STREQUAL "${_lonejson_target_triple}")
    message(FATAL_ERROR
      "Bootlin compiler triple mismatch for ${target_id}: expected ${_lonejson_target_triple}, got ${_lonejson_compiler_triple}.\n${_lonejson_compiler_error}")
  endif()

  execute_process(
    COMMAND "${CMAKE_C_COMPILER}" -print-sysroot
    RESULT_VARIABLE _lonejson_sysroot_result
    OUTPUT_VARIABLE _lonejson_compiler_sysroot
    ERROR_VARIABLE _lonejson_sysroot_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE)
  file(REAL_PATH "${_lonejson_sysroot}" _lonejson_expected_sysroot)
  file(REAL_PATH "${_lonejson_compiler_sysroot}" _lonejson_actual_sysroot)
  if(NOT _lonejson_sysroot_result EQUAL 0 OR
      NOT "${_lonejson_actual_sysroot}" STREQUAL "${_lonejson_expected_sysroot}")
    message(FATAL_ERROR
      "Bootlin compiler sysroot mismatch for ${target_id}: expected ${_lonejson_expected_sysroot}, got ${_lonejson_compiler_sysroot}.\n${_lonejson_sysroot_error}")
  endif()

  execute_process(
    COMMAND "${CMAKE_C_COMPILER}" -print-prog-name=ld
    RESULT_VARIABLE _lonejson_driver_linker_result
    OUTPUT_VARIABLE _lonejson_driver_linker
    ERROR_VARIABLE _lonejson_driver_linker_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE)
  file(REAL_PATH "${_lonejson_root}" _lonejson_real_root)
  file(REAL_PATH "${_lonejson_driver_linker}" _lonejson_real_driver_linker)
  string(FIND "${_lonejson_real_driver_linker}" "${_lonejson_real_root}/" _lonejson_driver_linker_root)
  if(NOT _lonejson_driver_linker_result EQUAL 0 OR
      NOT "${_lonejson_driver_linker_root}" STREQUAL "0")
    message(FATAL_ERROR
      "Bootlin compiler linker is outside its collection for ${target_id}: ${_lonejson_driver_linker}.\n${_lonejson_driver_linker_error}")
  endif()

  execute_process(
    COMMAND "${CMAKE_C_COMPILER}" -print-file-name=libc.so
    RESULT_VARIABLE _lonejson_libc_result
    OUTPUT_VARIABLE _lonejson_driver_libc
    ERROR_VARIABLE _lonejson_libc_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE)
  file(REAL_PATH "${_lonejson_driver_libc}" _lonejson_real_driver_libc)
  string(FIND "${_lonejson_real_driver_libc}" "${_lonejson_expected_sysroot}/" _lonejson_driver_libc_root)
  if(NOT _lonejson_libc_result EQUAL 0 OR
      NOT "${_lonejson_driver_libc_root}" STREQUAL "0")
    message(FATAL_ERROR
      "Bootlin compiler libc is outside its sysroot for ${target_id}: ${_lonejson_driver_libc}.\n${_lonejson_libc_error}")
  endif()

  set(CMAKE_FIND_ROOT_PATH "${_lonejson_sysroot}")
  set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
  set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
  set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
  set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
  if(NOT _lonejson_is_native_runtime)
    set(CMAKE_CROSSCOMPILING_EMULATOR "/usr/bin/${emulator};-L;${_lonejson_sysroot}" CACHE STRING "" FORCE)
    set(CMAKE_CROSSCOMPILING_EMULATOR "/usr/bin/${emulator};-L;${_lonejson_sysroot}" PARENT_SCOPE)
  endif()
  if(_lonejson_is_native_runtime AND NOT CMAKE_TOOLCHAIN_FILE)
    if(NOT DEFINED LONEJSON_TARGET_ARCH)
      set(LONEJSON_TARGET_ARCH "${target_arch}" CACHE STRING "")
    endif()
    if(NOT DEFINED LONEJSON_TARGET_OS)
      set(LONEJSON_TARGET_OS linux CACHE STRING "")
    endif()
    if(NOT DEFINED LONEJSON_TARGET_LIBC)
      set(LONEJSON_TARGET_LIBC "${target_libc}" CACHE STRING "")
    endif()
  else()
    # Toolchain files run before project(), when CMake's host processor may
    # still be unavailable.  Preserve an explicitly requested target identity
    # instead of replacing it with the compiler collection's Linux defaults.
    if(NOT DEFINED LONEJSON_TARGET_ARCH)
      set(LONEJSON_TARGET_ARCH "${target_arch}" CACHE STRING "" FORCE)
    endif()
    if(NOT DEFINED LONEJSON_TARGET_OS)
      set(LONEJSON_TARGET_OS linux CACHE STRING "" FORCE)
    endif()
    if(NOT DEFINED LONEJSON_TARGET_LIBC)
      set(LONEJSON_TARGET_LIBC "${target_libc}" CACHE STRING "" FORCE)
    endif()
  endif()
  set(LONEJSON_BOOTLIN_TOOLCHAIN_ROOT "${_lonejson_root}" CACHE PATH "" FORCE)
  set(LONEJSON_BOOTLIN_TARGET_TRIPLE "${_lonejson_target_triple}" CACHE STRING "" FORCE)
  set(LONEJSON_BOOTLIN_LIBC "${_lonejson_libc}" CACHE STRING "" FORCE)
  set(LONEJSON_BOOTLIN_GDB "${_lonejson_gdb}" CACHE FILEPATH "" FORCE)
endfunction()
