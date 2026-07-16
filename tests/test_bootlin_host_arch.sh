#!/usr/bin/env bash
set -euo pipefail

# A target is native only when its normalized processor and Linux loader ABI
# match CMake's actual host runtime; an x86_64 target on ARM and x86_64 musl
# on a glibc host must both use the QEMU route.

repo_root=$1
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

mkdir -p "$tmp_dir/bin"
cat >"$tmp_dir/bin/qemu-x86_64" <<'EOF'
#!/usr/bin/env bash
exit 0
EOF
chmod +x "$tmp_dir/bin/qemu-x86_64"

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
if(NOT "\${CMAKE_CROSSCOMPILING_EMULATOR}" MATCHES "^${tmp_dir}/bin/qemu-x86_64;-L;.+/sysroot$")
  message(FATAL_ERROR "x86_64 cross target must configure qemu-x86_64")
endif()
EOF

if PATH="$tmp_dir/bin:$PATH" cmake -P "$tmp_dir/check.cmake" \
    >"$tmp_dir/check.out" 2>"$tmp_dir/check.err"; then
  printf 'aarch64 hosts must reject x86_64-hosted Bootlin compiler executables\n' >&2
  exit 1
fi
grep -F 'x86_64 glibc Linux host' "$tmp_dir/check.err" >/dev/null

cat >"$tmp_dir/musl-check.cmake" <<EOF
set(CMAKE_HOST_SYSTEM_PROCESSOR x86_64)
include("$repo_root/cmake/toolchains/lonejson_bootlin.cmake")
lonejson_configure_bootlin_toolchain(
  x86_64-linux-musl x86_64 x86_64 musl qemu-x86_64)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "x86_64 musl must use the cross runtime on a glibc host")
endif()
if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
  message(FATAL_ERROR "x86_64 musl cross target processor was not configured")
endif()
if(NOT "\${CMAKE_CROSSCOMPILING_EMULATOR}" MATCHES "^${tmp_dir}/bin/qemu-x86_64;-L;.+/sysroot$")
  message(FATAL_ERROR "x86_64 musl must configure qemu-x86_64 with its sysroot")
endif()
EOF

host_ldd="$(ldd --version 2>&1 || true)"
if ! grep -Eiq 'musl' <<<"$host_ldd"; then
  PATH="$tmp_dir/bin:$PATH" cmake -P "$tmp_dir/musl-check.cmake"
fi

cat >"$tmp_dir/build-only-check.cmake" <<EOF
set(CMAKE_HOST_SYSTEM_PROCESSOR x86_64)
include("$repo_root/cmake/toolchains/lonejson_bootlin.cmake")
lonejson_configure_bootlin_toolchain(
  x86_64-linux-musl x86_64 x86_64 musl qemu-lonejson-missing)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "x86_64 musl must remain a cross build on a glibc host")
endif()
if(DEFINED CMAKE_CROSSCOMPILING_EMULATOR)
  message(FATAL_ERROR "missing QEMU must not block build-only cross configuration")
endif()
EOF

if ! grep -Eiq 'musl' <<<"$host_ldd"; then
  cmake -P "$tmp_dir/build-only-check.cmake"
fi

mkdir -p "$tmp_dir/musl-bin"
cat >"$tmp_dir/musl-bin/ldd" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' 'musl libc (x86_64)' >&2
exit 1
EOF
chmod +x "$tmp_dir/musl-bin/ldd"

cat >"$tmp_dir/musl-native-check.cmake" <<EOF
set(CMAKE_HOST_SYSTEM_PROCESSOR x86_64)
include("$repo_root/cmake/toolchains/lonejson_bootlin.cmake")
lonejson_configure_bootlin_toolchain(
  x86_64-linux-musl x86_64 x86_64 musl qemu-x86_64)
EOF

if PATH="$tmp_dir/musl-bin:$PATH" cmake -P "$tmp_dir/musl-native-check.cmake" \
    >"$tmp_dir/musl-native.out" 2>"$tmp_dir/musl-native.err"; then
  printf 'musl hosts must reject glibc-hosted Bootlin compiler executables\n' >&2
  exit 1
fi
grep -F 'x86_64 glibc Linux host' "$tmp_dir/musl-native.err" >/dev/null

mkdir -p "$tmp_dir/arm-bin"
cat >"$tmp_dir/arm-bin/ldd" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' 'ldd (GNU libc) 2.39'
EOF
chmod +x "$tmp_dir/arm-bin/ldd"

cat >"$tmp_dir/arm-native-check.cmake" <<EOF
set(CMAKE_HOST_SYSTEM_PROCESSOR armv7l)
include("$repo_root/cmake/toolchains/lonejson_bootlin.cmake")
lonejson_configure_bootlin_toolchain(
  armhf-linux-gnu arm armhf gnu qemu-arm)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "armv7l must stay native for the armhf runtime")
endif()
if(DEFINED CMAKE_CROSSCOMPILING_EMULATOR)
  message(FATAL_ERROR "native armhf runtime must not configure QEMU")
endif()
EOF

if PATH="$tmp_dir/arm-bin:$PATH" cmake -P "$tmp_dir/arm-native-check.cmake" \
    >"$tmp_dir/arm-native.out" 2>"$tmp_dir/arm-native.err"; then
  printf 'armv7l hosts must reject x86_64-hosted Bootlin compiler executables\n' >&2
  exit 1
fi
grep -F 'x86_64 glibc Linux host' "$tmp_dir/arm-native.err" >/dev/null
