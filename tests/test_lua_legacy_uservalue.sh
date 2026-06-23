#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
lua_exec=${2:-lua}
luarocks_exec=${3:-luarocks}
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

rock_tree="$tmp_dir/luarocks"
rockspec="$tmp_dir/lonejson-0.0.0-1.rockspec"

mkdir -p "$rock_tree"
(
  cd "$repo_root"
  lib_ext="$("$luarocks_exec" config variables.LIB_EXTENSION)"
  ./scripts/render_release_rockspec.sh \
    "0.0.0" "$rockspec" "git+file://$repo_root" "" "$lib_ext"
  CFLAGS="${CFLAGS:+$CFLAGS }-DLONEJSON_TEST_FORCE_LUA_LEGACY_USERVALUE=1" \
  LONEJSON_LIBDIR="$repo_root/build/debug" \
    "$luarocks_exec" make --tree "$rock_tree" "$rockspec" >/dev/null
)

eval "$("$luarocks_exec" path --tree "$rock_tree")"
export LD_LIBRARY_PATH="$repo_root/build/debug:${LD_LIBRARY_PATH:-}"
export DYLD_LIBRARY_PATH="$repo_root/build/debug:${DYLD_LIBRARY_PATH:-}"
"$lua_exec" "$repo_root/tests/test_lua.lua"
"$lua_exec" "$repo_root/tests/test_lua_fuzz.lua"
