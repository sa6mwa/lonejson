#!/usr/bin/env bash
set -euo pipefail

# A target is native only when its normalized architecture matches CMake's
# actual host architecture; an x86_64 target on ARM must use the QEMU route.

repo_root=$1
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

cat >"$tmp_dir/check.cmake" <<EOF
set(CMAKE_HOST_SYSTEM_PROCESSOR aarch64)
include("$repo_root/cmake/toolchains/lonejson_bootlin.cmake")
lonejson_configure_bootlin_toolchain(
  x86_64-linux-gnu x86_64 x86_64 gnu qemu-x86_64)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "x86_64 must be cross-compiled on an aarch64 host")
endif()
if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
  message(FATAL_ERROR "cross target processor was not configured")
endif()
if(NOT "\${CMAKE_CROSSCOMPILING_EMULATOR}" MATCHES "^/usr/bin/qemu-x86_64;-L;.+/sysroot$")
  message(FATAL_ERROR "x86_64 cross target must configure qemu-x86_64")
endif()
EOF

cmake -P "$tmp_dir/check.cmake"
