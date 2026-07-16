# Host workflows must use a Bootlin runtime matching the host processor and
# libc, so a host Lua interpreter remains ABI-compatible with liblonejson.
include("${CMAKE_CURRENT_LIST_DIR}/lonejson_bootlin.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/lonejson_native_bootlin_target.cmake")
lonejson_detect_native_bootlin_target(
  _lonejson_native_target_id
  _lonejson_native_cmake_processor
  _lonejson_native_target_arch
  _lonejson_native_libc
  _lonejson_native_emulator)
lonejson_configure_bootlin_toolchain(
  "${_lonejson_native_target_id}"
  "${_lonejson_native_cmake_processor}"
  "${_lonejson_native_target_arch}"
  "${_lonejson_native_libc}"
  "${_lonejson_native_emulator}")
