cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED LONEJSON_ROOT OR LONEJSON_ROOT STREQUAL "")
  get_filename_component(LONEJSON_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()

include("${LONEJSON_ROOT}/cmake/toolchains/lonejson_native_bootlin_target.cmake")
lonejson_detect_native_bootlin_target(
  _lonejson_native_target_id
  _lonejson_native_cmake_processor
  _lonejson_native_target_arch
  _lonejson_native_libc
  _lonejson_native_emulator)
message("${_lonejson_native_target_id}")
