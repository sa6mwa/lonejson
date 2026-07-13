#!/usr/bin/env bash
set -euo pipefail

# Rationale: package verification must use the tools that match the configured
# target build, not whatever similarly named host tool appears on PATH.

repo_root=$1
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

fake_bin="$tmp_dir/toolchain/bin"
build_dir="$tmp_dir/build"
mkdir -p "$fake_bin" "$build_dir"

for tool in \
  aarch64-linux-gnu-gcc \
  aarch64-linux-gnu-ld \
  aarch64-linux-gnu-ar \
  aarch64-linux-gnu-strip \
  aarch64-linux-gnu-nm \
  aarch64-linux-gnu-readelf \
  arm64-apple-darwin25-clang \
  arm64-apple-darwin25-ld \
  arm64-apple-darwin25-ar \
  arm64-apple-darwin25-strip \
  arm64-apple-darwin25-nm \
  arm64-apple-darwin25-otool \
  arm64-apple-darwin25-install_name_tool; do
  printf '#!/usr/bin/env bash\nexit 0\n' >"$fake_bin/$tool"
  chmod +x "$fake_bin/$tool"
done

cat >"$build_dir/CMakeCache.txt" <<EOF
CMAKE_C_COMPILER:FILEPATH=$fake_bin/aarch64-linux-gnu-gcc
EOF

eval "$("$repo_root/scripts/discover_target_tools.sh" \
  --build-dir "$build_dir" \
  --target-id aarch64-linux-gnu)"

[[ "$CC" == "$fake_bin/aarch64-linux-gnu-gcc" ]]
[[ "$TARGET_HOST_PREFIX" == "aarch64-linux-gnu" ]]
[[ "$TARGET_TOOL_PREFIX" == "aarch64-linux-gnu-" ]]
[[ "$LINKER" == "$fake_bin/aarch64-linux-gnu-ld" ]]
[[ "$AR" == "$fake_bin/aarch64-linux-gnu-ar" ]]
[[ "$STRIP" == "$fake_bin/aarch64-linux-gnu-strip" ]]
[[ "$NM" == "$fake_bin/aarch64-linux-gnu-nm" ]]
[[ "$READELF" == "$fake_bin/aarch64-linux-gnu-readelf" ]]
[[ -z "$OTOOL" ]]

printf '#!/usr/bin/env bash\nexit 0\n' >"$fake_bin/clang"
chmod +x "$fake_bin/clang"
cat >"$build_dir/CMakeCache.txt" <<EOF
CMAKE_C_COMPILER:FILEPATH=$fake_bin/clang
CMAKE_C_COMPILER_TARGET:STRING=aarch64-buildroot-linux-gnu
CMAKE_C_COMPILER_EXTERNAL_TOOLCHAIN:PATH=$tmp_dir/bootlin
CMAKE_SYSROOT:PATH=$tmp_dir/bootlin/aarch64-buildroot-linux-gnu/sysroot
EOF

eval "$("$repo_root/scripts/discover_target_tools.sh" \
  --build-dir "$build_dir" \
  --target-id aarch64-linux-gnu)"

[[ "$TARGET_CFLAGS" == '--target=aarch64-buildroot-linux-gnu --gcc-toolchain='"$tmp_dir"'/bootlin --sysroot='"$tmp_dir"'/bootlin/aarch64-buildroot-linux-gnu/sysroot -B'"$tmp_dir"'/bootlin/aarch64-buildroot-linux-gnu/bin' ]]

cat >"$build_dir/CMakeCache.txt" <<EOF
CMAKE_C_COMPILER:FILEPATH=$fake_bin/arm64-apple-darwin25-clang
EOF

eval "$("$repo_root/scripts/discover_target_tools.sh" \
  --build-dir "$build_dir" \
  --target-id arm64-apple-darwin)"

[[ "$CC" == "$fake_bin/arm64-apple-darwin25-clang" ]]
[[ "$TARGET_HOST_PREFIX" == "arm64-apple-darwin25" ]]
[[ "$TARGET_TOOL_PREFIX" == "arm64-apple-darwin25-" ]]
[[ "$LINKER" == "$fake_bin/arm64-apple-darwin25-ld" ]]
[[ "$AR" == "$fake_bin/arm64-apple-darwin25-ar" ]]
[[ "$STRIP" == "$fake_bin/arm64-apple-darwin25-strip" ]]
[[ "$NM" == "$fake_bin/arm64-apple-darwin25-nm" ]]
[[ "$OTOOL" == "$fake_bin/arm64-apple-darwin25-otool" ]]
[[ "$INSTALL_NAME_TOOL" == "$fake_bin/arm64-apple-darwin25-install_name_tool" ]]

host_bin="$tmp_dir/host/bin"
darwin_cc_only="$tmp_dir/darwin-cc-only/bin"
mkdir -p "$host_bin" "$darwin_cc_only"
printf '#!/usr/bin/env bash\nexit 0\n' >"$darwin_cc_only/arm64-apple-darwin25-clang"
chmod +x "$darwin_cc_only/arm64-apple-darwin25-clang"
for tool in ld ar strip nm otool install_name_tool; do
  printf '#!/usr/bin/env bash\nexit 0\n' >"$host_bin/$tool"
  chmod +x "$host_bin/$tool"
done

cat >"$build_dir/CMakeCache.txt" <<EOF
CMAKE_C_COMPILER:FILEPATH=$darwin_cc_only/arm64-apple-darwin25-clang
EOF

eval "$(PATH="$host_bin:/usr/bin:/bin" "$repo_root/scripts/discover_target_tools.sh" \
  --build-dir "$build_dir" \
  --target-id arm64-apple-darwin)"

[[ "$CC" == "$darwin_cc_only/arm64-apple-darwin25-clang" ]]
[[ "$TARGET_HOST_PREFIX" == "arm64-apple-darwin25" ]]
[[ -z "$LINKER" ]]
[[ -z "$AR" ]]
[[ -z "$STRIP" ]]
[[ -z "$NM" ]]
[[ -z "$OTOOL" ]]
[[ -z "$INSTALL_NAME_TOOL" ]]

toolchain_cache="$tmp_dir/toolchain-cache"
aarch64_musl_root="$toolchain_cache/roots/aarch64--musl--stable-2025.08-1"
armhf_musl_root="$toolchain_cache/roots/armv7-eabihf--musl--stable-2025.08-1"
for spec in \
  "$aarch64_musl_root|aarch64-linux|aarch64-buildroot-linux-musl" \
  "$armhf_musl_root|arm-linux|arm-buildroot-linux-musleabihf"; do
  IFS='|' read -r root prefix triple <<<"$spec"
  mkdir -p "$root/bin" "$root/$triple/sysroot/usr/include" "$root/$triple/sysroot/usr/lib"
  mkdir -p "$root/lib"
  touch "$root/$triple/sysroot/usr/include/stdio.h" \
    "$root/lib/libstdc++.a" "$root/lib/libgcc.a"
  touch "$root/$triple/sysroot/usr/lib/libc.so"
  for tool in gcc g++ ld ar ranlib strip nm objcopy objdump addr2line gdb readelf; do
    if [[ "$tool" == g++ ]]; then
      printf '#!/usr/bin/env bash\ncase "${1:-}" in\n  -print-file-name=libstdc++.a) printf "%%s\\n" "$(dirname "$0")/../lib/libstdc++.a" ;;\n  -print-file-name=libgcc.a) printf "%%s\\n" "$(dirname "$0")/../lib/libgcc.a" ;;\n  *) exit 0 ;;\nesac\n' >"$root/bin/$prefix-$tool"
    else
      printf '#!/usr/bin/env bash\nexit 0\n' >"$root/bin/$prefix-$tool"
    fi
    chmod +x "$root/bin/$prefix-$tool"
  done
done
rm -f "$build_dir/CMakeCache.txt"

eval "$(PATH="/usr/bin:/bin" \
  CPKT_TOOLCHAIN_CACHE="$toolchain_cache" \
  "$repo_root/scripts/discover_target_tools.sh" \
    --build-dir "$build_dir" \
    --target-id aarch64-linux-musl)"

[[ "$CC" == "$aarch64_musl_root/bin/aarch64-linux-gcc" ]]
[[ "$TARGET_HOST_PREFIX" == "aarch64-linux-musl" ]]
[[ "$AR" == "$aarch64_musl_root/bin/aarch64-linux-ar" ]]
[[ "$READELF" == "$aarch64_musl_root/bin/aarch64-linux-readelf" ]]

eval "$(PATH="/usr/bin:/bin" \
  CPKT_TOOLCHAIN_CACHE="$toolchain_cache" \
  "$repo_root/scripts/discover_target_tools.sh" \
    --build-dir "$build_dir" \
    --target-id armhf-linux-musl)"

[[ "$CC" == "$armhf_musl_root/bin/arm-linux-gcc" ]]
[[ "$TARGET_HOST_PREFIX" == "arm-linux-musleabihf" ]]
[[ "$AR" == "$armhf_musl_root/bin/arm-linux-ar" ]]
[[ "$READELF" == "$armhf_musl_root/bin/arm-linux-readelf" ]]
