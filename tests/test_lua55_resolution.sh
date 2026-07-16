#!/usr/bin/env bash
set -euo pipefail

# CMake, Make, and direct scripts must resolve the same Lua 5.5 executable,
# including hosts that expose it only as lua5.5.

repo_root=$1
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

fake_bin="$tmp_dir/bin"
mkdir -p "$fake_bin"
cat >"$fake_bin/lua" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' 'Lua 5.4.7' >&2
EOF
cat >"$fake_bin/lua5.5" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' 'Lua 5.5.0' >&2
EOF
chmod +x "$fake_bin/lua" "$fake_bin/lua5.5"

resolved=$(PATH="$fake_bin:$PATH" "$repo_root/scripts/resolve_lua55.sh")
[[ "$resolved" == "$fake_bin/lua5.5" ]]

cmake_cmd=$(command -v "${CMAKE_COMMAND:-cmake}")
ninja_cmd=$(command -v ninja)
"$cmake_cmd" -S "$repo_root" -B "$tmp_dir/configured-lua" -G Ninja \
  -D CMAKE_MAKE_PROGRAM="$ninja_cmd" \
  -D LONEJSON_LUA_EXECUTABLE="$fake_bin/lua5.5" \
  -D LONEJSON_BUILD_TESTS=OFF \
  -D LONEJSON_BUILD_EXAMPLES=OFF \
  >"$tmp_dir/configured-lua.log" 2>&1
grep -F "LONEJSON_LUA_EXECUTABLE:FILEPATH=$fake_bin/lua5.5" \
  "$tmp_dir/configured-lua/CMakeCache.txt" >/dev/null

cat >"$fake_bin/lua5.5" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' 'Lua 5.4.7' >&2
EOF
chmod +x "$fake_bin/lua5.5"
if PATH="$fake_bin:$PATH" "$repo_root/scripts/resolve_lua55.sh" \
    >"$tmp_dir/missing.out" 2>"$tmp_dir/missing.err"; then
  printf 'Lua resolver accepted a non-5.5 lua executable\n' >&2
  exit 1
fi
grep -F 'requires a Lua 5.5 executable' "$tmp_dir/missing.err" >/dev/null

if "$cmake_cmd" -S "$repo_root" -B "$tmp_dir/invalid-configured-lua" -G Ninja \
    -D CMAKE_MAKE_PROGRAM="$ninja_cmd" \
    -D LONEJSON_LUA_EXECUTABLE="$fake_bin/lua5.5" \
    -D LONEJSON_BUILD_TESTS=OFF \
    -D LONEJSON_BUILD_EXAMPLES=OFF \
    >"$tmp_dir/invalid-configured-lua.log" 2>&1; then
  printf 'CMake accepted a configured non-5.5 Lua executable\n' >&2
  exit 1
fi
grep -F 'LONEJSON_LUA_EXECUTABLE to be a Lua 5.5 executable' \
  "$tmp_dir/invalid-configured-lua.log" >/dev/null
