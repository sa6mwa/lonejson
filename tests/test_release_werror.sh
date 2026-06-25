#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
cmake_bin=$2
generator=$3
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

build_dir="$tmp_dir/build"
fake_lua_root="$tmp_dir/cpkt"

mkdir -p "$fake_lua_root/include" "$fake_lua_root/lib"
: >"$fake_lua_root/include/lua.h"
: >"$fake_lua_root/lib/liblua.so"

"$cmake_bin" -S "$repo_root" -B "$build_dir" \
  -G "$generator" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DLONEJSON_BUILD_TESTS=ON \
  -DLONEJSON_BUILD_EXAMPLES=ON \
  -DLONEJSON_C_PKT_SYSTEMS_ROOT="$fake_lua_root" \
  >/dev/null

if [[ ! -f "$build_dir/compile_commands.json" ]]; then
  printf 'missing release compile_commands.json\n' >&2
  exit 1
fi

require_release_werror() {
  local source=$1

  if ! awk -v source="$repo_root/$source" '
    index($0, "\"command\":") { command = $0 }
    index($0, "\"file\": \"" source "\"") {
      found = 1
      if (index(command, "-Werror") == 0) {
        exit 1
      }
    }
    END {
      if (!found) {
        exit 2
      }
    }
  ' "$build_dir/compile_commands.json"; then
    printf 'release compile command for %s is missing -Werror\n' "$source" >&2
    exit 1
  fi
}

require_release_werror src/lonejson.c
require_release_werror tests/test_main.c
require_release_werror src/lua/lonejson_lua.c
