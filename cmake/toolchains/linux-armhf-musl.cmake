include("${CMAKE_CURRENT_LIST_DIR}/lonejson_bootlin.cmake")
lonejson_configure_bootlin_toolchain(
  armhf-linux-musl arm armhf musl qemu-arm)
