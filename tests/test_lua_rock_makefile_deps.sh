#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
luarocks_exec=${2:-luarocks}
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

lua_rock_tree="$tmp_dir/luarocks"
lua_rockspec="$lua_rock_tree/lonejson-0.0.0-1.rockspec"
lua_stamp="$lua_rock_tree/.installed.stamp"
lua_lock="$lua_rock_tree/.build.lock"

make_lua_rock() {
  make --no-print-directory -C "$repo_root" "$@" \
    LUA_ROCK_TREE="$lua_rock_tree" \
    LUA_ROCKSPEC="$lua_rockspec" \
    LUA_ROCK_STAMP="$lua_stamp" \
    LUA_ROCK_BUILD_LOCK="$lua_lock" \
    LUAROCKS="$luarocks_exec"
}

dry_run=$(make_lua_rock -B -n lua-rock)
configure_line=$(printf '%s\n' "$dry_run" |
  awk '/cmake --preset debug/ { print NR; exit }')
build_line=$(printf '%s\n' "$dry_run" |
  awk '/cmake --build --preset debug --target lonejson_shared/ { print NR; exit }')

if [ -z "$configure_line" ] || [ -z "$build_line" ]; then
  printf '%s\n' "$dry_run" >&2
  printf 'lua-rock dry-run did not configure and build debug liblonejson\n' >&2
  exit 1
fi
if [ "$configure_line" -ge "$build_line" ]; then
  printf '%s\n' "$dry_run" >&2
  printf 'lua-rock builds debug liblonejson before configuring debug\n' >&2
  exit 1
fi

mkdir -p "$lua_rock_tree"
touch "$lua_rockspec" "$lua_stamp"
touch -t 200001010000 "$lua_rockspec" "$lua_stamp"

core_source_dry_run=$(make_lua_rock -n lua-rock)
if ! printf '%s\n' "$core_source_dry_run" |
    grep -q 'cmake --build --preset debug --target lonejson_shared'; then
  printf '%s\n' "$core_source_dry_run" >&2
  printf 'lua-rock stamp did not rebuild when src/lonejson.c changed\n' >&2
  exit 1
fi

touch -t 200001010000 "$lua_rockspec" "$lua_stamp"
impl_header_dry_run=$(make_lua_rock -n lua-rock)
if ! printf '%s\n' "$impl_header_dry_run" |
    grep -q 'cmake --build --preset debug --target lonejson_shared'; then
  printf '%s\n' "$impl_header_dry_run" >&2
  printf 'lua-rock stamp did not rebuild when src/impl headers changed\n' >&2
  exit 1
fi
