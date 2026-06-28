#!/usr/bin/env bash
set -euo pipefail

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

eval "$(PATH="$host_bin:$PATH" "$repo_root/scripts/discover_target_tools.sh" \
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
