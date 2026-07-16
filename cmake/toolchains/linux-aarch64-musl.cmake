include("${CMAKE_CURRENT_LIST_DIR}/lonejson_bootlin.cmake")
lonejson_configure_bootlin_toolchain(
  aarch64-linux-musl aarch64 aarch64 musl qemu-aarch64)
