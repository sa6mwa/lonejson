#!/usr/bin/env bash
set -euo pipefail

# Every default native configuration and every Linux target toolchain must use
# one pinned Bootlin collection. The toolchain module independently rejects a
# compiler whose reported triple differs from the target contract.

repo_root=$1
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

cmake_cmd=$(command -v "${CMAKE_COMMAND:-cmake}")
ninja_cmd=$(command -v ninja)

cache_value() {
  local cache_file=$1 name=$2
  sed -n "s/^${name}:[^=]*=//p" "$cache_file" | tail -n 1
}

bootlin_description=$("$repo_root/scripts/cpkt-toolchains.sh" discover x86_64-linux-gnu)
grep -Fx 'source=bootlin' <<<"$bootlin_description" >/dev/null
grep -Fx 'status=ready' <<<"$bootlin_description" >/dev/null
bootlin_cc=$(printf '%s\n' "$bootlin_description" | sed -n 's/^cc=//p')
bootlin_cxx=$(printf '%s\n' "$bootlin_description" | sed -n 's/^cxx=//p')
bootlin_ld=$(printf '%s\n' "$bootlin_description" | sed -n 's/^ld=//p')
bootlin_ar=$(printf '%s\n' "$bootlin_description" | sed -n 's/^ar=//p')
bootlin_ranlib=$(printf '%s\n' "$bootlin_description" | sed -n 's/^ranlib=//p')
bootlin_strip=$(printf '%s\n' "$bootlin_description" | sed -n 's/^strip=//p')
bootlin_nm=$(printf '%s\n' "$bootlin_description" | sed -n 's/^nm=//p')
bootlin_objcopy=$(printf '%s\n' "$bootlin_description" | sed -n 's/^objcopy=//p')
bootlin_objdump=$(printf '%s\n' "$bootlin_description" | sed -n 's/^objdump=//p')
bootlin_addr2line=$(printf '%s\n' "$bootlin_description" | sed -n 's/^addr2line=//p')
bootlin_gdb=$(printf '%s\n' "$bootlin_description" | sed -n 's/^gdb=//p')
bootlin_readelf=$(printf '%s\n' "$bootlin_description" | sed -n 's/^readelf=//p')
bootlin_sysroot=$(printf '%s\n' "$bootlin_description" | sed -n 's/^sysroot=//p')
bootlin_target=$(printf '%s\n' "$bootlin_description" | sed -n 's/^target_triple=//p')
bootlin_libstdcxx_a=$(printf '%s\n' "$bootlin_description" | sed -n 's/^libstdcxx_a=//p')
bootlin_libgcc_a=$(printf '%s\n' "$bootlin_description" | sed -n 's/^libgcc_a=//p')

[[ "$($bootlin_cc -dumpmachine)" == "$bootlin_target" ]]
[[ -x "$bootlin_cxx" ]]
[[ -x "$bootlin_ld" ]]
[[ -x "$bootlin_ar" ]]
[[ -x "$bootlin_ranlib" ]]
[[ -x "$bootlin_strip" ]]
[[ -x "$bootlin_nm" ]]
[[ -x "$bootlin_objcopy" ]]
[[ -x "$bootlin_objdump" ]]
[[ -x "$bootlin_addr2line" ]]
[[ -x "$bootlin_gdb" ]]
[[ -x "$bootlin_readelf" ]]
[[ -f "$bootlin_libstdcxx_a" ]]
[[ -f "$bootlin_libgcc_a" ]]
[[ -f "$bootlin_sysroot/usr/include/stdio.h" ]]
[[ -e "$bootlin_sysroot/usr/lib/libc.so" || -e "$bootlin_sysroot/lib/libc.so" ||
   -e "$bootlin_sysroot/lib/libc.so.6" ]]

"$cmake_cmd" -S "$repo_root" -B "$tmp_dir/default" -G Ninja \
  -D CMAKE_MAKE_PROGRAM="$ninja_cmd" \
  -D LONEJSON_BUILD_TESTS=OFF \
  -D LONEJSON_BUILD_EXAMPLES=OFF \
  >"$tmp_dir/default.log" 2>&1
[[ "$(cache_value "$tmp_dir/default/CMakeCache.txt" CMAKE_C_COMPILER)" == "$bootlin_cc" ]]
[[ "$(cache_value "$tmp_dir/default/CMakeCache.txt" CMAKE_LINKER)" == "$bootlin_ld" ]]
[[ "$(cache_value "$tmp_dir/default/CMakeCache.txt" CMAKE_AR)" == "$bootlin_ar" ]]
[[ "$(cache_value "$tmp_dir/default/CMakeCache.txt" CMAKE_RANLIB)" == "$bootlin_ranlib" ]]
[[ "$(cache_value "$tmp_dir/default/CMakeCache.txt" CMAKE_STRIP)" == "$bootlin_strip" ]]
[[ "$(cache_value "$tmp_dir/default/CMakeCache.txt" CMAKE_NM)" == "$bootlin_nm" ]]
[[ "$(cache_value "$tmp_dir/default/CMakeCache.txt" CMAKE_OBJCOPY)" == "$bootlin_objcopy" ]]
[[ "$(cache_value "$tmp_dir/default/CMakeCache.txt" CMAKE_OBJDUMP)" == "$bootlin_objdump" ]]
[[ "$(cache_value "$tmp_dir/default/CMakeCache.txt" CMAKE_ADDR2LINE)" == "$bootlin_addr2line" ]]
[[ "$(cache_value "$tmp_dir/default/CMakeCache.txt" CMAKE_READELF)" == "$bootlin_readelf" ]]
[[ "$(cache_value "$tmp_dir/default/CMakeCache.txt" CMAKE_SYSROOT)" == "$bootlin_sysroot" ]]
[[ "$(cache_value "$tmp_dir/default/CMakeCache.txt" CMAKE_C_COMPILER_TARGET)" == "$bootlin_target" ]]
[[ "$(cache_value "$tmp_dir/default/CMakeCache.txt" LONEJSON_BOOTLIN_GDB)" == "$bootlin_gdb" ]]
if grep -F 'CMAKE_CROSSCOMPILING_EMULATOR:STRING=' "$tmp_dir/default/CMakeCache.txt" >/dev/null; then
  printf 'plain native Bootlin configuration must not install a QEMU emulator\n' >&2
  exit 1
fi
grep -F 'LONEJSON_DEFAULT_C_COMPILER:INTERNAL=bootlin-gcc' \
  "$tmp_dir/default/CMakeCache.txt" >/dev/null

toolchain_env=$("$repo_root/scripts/cpkt-toolchains.sh" env x86_64-linux-gnu)
printf '%s\n' "$toolchain_env" | grep -F "export CC=$bootlin_cc" >/dev/null
printf '%s\n' "$toolchain_env" | grep -F "export LD=$bootlin_ld" >/dev/null
printf '%s\n' "$toolchain_env" | grep -F "CPKT_TOOLCHAIN_LIBSTDCXX_A=$bootlin_libstdcxx_a" >/dev/null

"$cmake_cmd" -S "$repo_root" -B "$tmp_dir/bootlin-x86_64" -G Ninja \
  -D CMAKE_MAKE_PROGRAM="$ninja_cmd" \
  -D CMAKE_TOOLCHAIN_FILE="$repo_root/cmake/toolchains/linux-x86_64-gnu.cmake" \
  -D LONEJSON_BUILD_TESTS=OFF \
  -D LONEJSON_BUILD_EXAMPLES=OFF \
  >"$tmp_dir/bootlin-x86_64.log" 2>&1
[[ "$(cache_value "$tmp_dir/bootlin-x86_64/CMakeCache.txt" CMAKE_C_COMPILER)" == "$bootlin_cc" ]]
[[ "$(cache_value "$tmp_dir/bootlin-x86_64/CMakeCache.txt" CMAKE_LINKER)" == "$bootlin_ld" ]]
[[ "$(cache_value "$tmp_dir/bootlin-x86_64/CMakeCache.txt" CMAKE_C_COMPILER_TARGET)" == "$bootlin_target" ]]

"$cmake_cmd" -S "$repo_root" --preset host -B "$tmp_dir/host" \
  -D LONEJSON_BUILD_TESTS=OFF \
  -D LONEJSON_BUILD_EXAMPLES=OFF \
  >"$tmp_dir/host.log" 2>&1
[[ "$(cache_value "$tmp_dir/host/CMakeCache.txt" CMAKE_C_COMPILER)" == "$bootlin_cc" ]]
[[ "$(cache_value "$tmp_dir/host/CMakeCache.txt" CMAKE_LINKER)" == "$bootlin_ld" ]]
[[ "$(cache_value "$tmp_dir/host/CMakeCache.txt" CMAKE_SYSROOT)" == "$bootlin_sysroot" ]]

set +e
"$cmake_cmd" -S "$repo_root" -B "$tmp_dir/explicit-gcc" -G Ninja \
  -D CMAKE_MAKE_PROGRAM="$ninja_cmd" \
  -D CMAKE_C_COMPILER=/usr/bin/gcc \
  -D LONEJSON_BUILD_TESTS=OFF \
  -D LONEJSON_BUILD_EXAMPLES=OFF \
  >"$tmp_dir/explicit-gcc.log" 2>&1
explicit_gcc_status=$?
set -e
if [[ "$explicit_gcc_status" -eq 0 ]]; then
  [[ "$(cache_value "$tmp_dir/explicit-gcc/CMakeCache.txt" CMAKE_C_COMPILER)" == "$bootlin_cc" ]]
  [[ "$(cache_value "$tmp_dir/explicit-gcc/CMakeCache.txt" CMAKE_LINKER)" == "$bootlin_ld" ]]
  [[ "$(cache_value "$tmp_dir/explicit-gcc/CMakeCache.txt" CMAKE_SYSROOT)" == "$bootlin_sysroot" ]]
else
  grep -E 'must use its pinned Bootlin compiler|Bootlin compiler triple mismatch' \
    "$tmp_dir/explicit-gcc.log" >/dev/null
fi
