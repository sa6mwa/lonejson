# Use the complete pinned Bootlin x86_64 GNU collection for native builds.
# LLVM is separately pinned only for diagnostics requiring compiler-rt.
if(LONEJSON_USE_PINNED_LLVM)
  set(_lonejson_llvm_resolver "${CMAKE_CURRENT_LIST_DIR}/../scripts/cpkt-llvm.sh")
  execute_process(
    COMMAND "${_lonejson_llvm_resolver}" discover
    RESULT_VARIABLE _lonejson_llvm_result
    OUTPUT_VARIABLE _lonejson_llvm_description
    ERROR_VARIABLE _lonejson_llvm_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE)
  if(NOT _lonejson_llvm_result EQUAL 0)
    message(FATAL_ERROR
      "Pinned LLVM diagnostics toolchain is unavailable. Run `make toolchains-llvm`.\n${_lonejson_llvm_error}")
  endif()
  foreach(_lonejson_key root version cc cxx ld ar nm objcopy objdump readelf fuzzer_runtime msan_runtime)
    string(REGEX MATCH "${_lonejson_key}=([^\r\n]+)" _lonejson_match "${_lonejson_llvm_description}")
    if(NOT _lonejson_match)
      message(FATAL_ERROR "LLVM resolver did not report ${_lonejson_key}")
    endif()
    set(_lonejson_llvm_${_lonejson_key} "${CMAKE_MATCH_1}")
  endforeach()
  set(CMAKE_C_COMPILER "${_lonejson_llvm_cc}" CACHE FILEPATH "" FORCE)
  set(CMAKE_CXX_COMPILER "${_lonejson_llvm_cxx}" CACHE FILEPATH "" FORCE)
  set(CMAKE_LINKER "${_lonejson_llvm_ld}" CACHE FILEPATH "" FORCE)
  set(CMAKE_AR "${_lonejson_llvm_ar}" CACHE FILEPATH "" FORCE)
  set(CMAKE_NM "${_lonejson_llvm_nm}" CACHE FILEPATH "" FORCE)
  set(CMAKE_OBJCOPY "${_lonejson_llvm_objcopy}" CACHE FILEPATH "" FORCE)
  set(CMAKE_OBJDUMP "${_lonejson_llvm_objdump}" CACHE FILEPATH "" FORCE)
  set(CMAKE_READELF "${_lonejson_llvm_readelf}" CACHE FILEPATH "" FORCE)
  set(LONEJSON_PINNED_LLVM_ROOT "${_lonejson_llvm_root}" CACHE PATH "" FORCE)
  set(LONEJSON_PINNED_LLVM_VERSION "${_lonejson_llvm_version}" CACHE STRING "" FORCE)
  set(LONEJSON_PINNED_LLVM_FUZZER_RUNTIME "${_lonejson_llvm_fuzzer_runtime}" CACHE FILEPATH "" FORCE)
  set(LONEJSON_PINNED_LLVM_MSAN_RUNTIME "${_lonejson_llvm_msan_runtime}" CACHE FILEPATH "" FORCE)
  execute_process(
    COMMAND "${CMAKE_C_COMPILER}" --version
    RESULT_VARIABLE _lonejson_llvm_version_result
    OUTPUT_VARIABLE _lonejson_llvm_version_output
    ERROR_VARIABLE _lonejson_llvm_version_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE)
  string(FIND "${_lonejson_llvm_version_output}" "clang version ${_lonejson_llvm_version}" _lonejson_llvm_version_match)
  if(NOT _lonejson_llvm_version_result EQUAL 0 OR _lonejson_llvm_version_match EQUAL -1)
    message(FATAL_ERROR
      "Pinned LLVM compiler version mismatch: expected ${_lonejson_llvm_version}.\n${_lonejson_llvm_version_output}\n${_lonejson_llvm_version_error}")
  endif()
  message(STATUS "Using pinned LLVM ${_lonejson_llvm_version} diagnostics toolchain")
elseif(NOT DEFINED CMAKE_C_COMPILER AND "$ENV{CC}" STREQUAL "" AND
    NOT CMAKE_TOOLCHAIN_FILE)
  include("${CMAKE_CURRENT_LIST_DIR}/toolchains/lonejson_bootlin.cmake")
  lonejson_configure_bootlin_toolchain(
    x86_64-linux-gnu x86_64 x86_64 "" qemu-x86_64)
endif()
