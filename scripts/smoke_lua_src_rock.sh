#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  printf 'usage: %s <src-rock>\n' "$0" >&2
  exit 1
fi

rock_path=$1
tmp_dir=

if [[ ! -f "$rock_path" ]]; then
  printf 'smoke_lua_src_rock.sh: source rock not found: %s\n' "$rock_path" >&2
  exit 1
fi

cleanup() {
  if [[ -n "$tmp_dir" && -d "$tmp_dir" ]]; then
    rm -rf -- "$tmp_dir"
  fi
}

trap cleanup EXIT INT TERM

tmp_dir=$(mktemp -d)
luarocks install --tree "$tmp_dir/tree" "$rock_path"
eval "$(luarocks path --tree "$tmp_dir/tree")"
lua -e 'assert(require("lonejson.init"))'
