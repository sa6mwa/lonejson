include("${CMAKE_CURRENT_LIST_DIR}/lonejson_bootlin.cmake")
lonejson_configure_bootlin_toolchain(
  x86_64-linux-musl x86_64 x86_64 musl qemu-x86_64)
