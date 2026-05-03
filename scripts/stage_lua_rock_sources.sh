#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 3 ]]; then
  printf 'usage: %s <repo-root> <stage-dir> <release-version>\n' "$0" >&2
  exit 1
fi

repo_root=$1
stage_dir=$2
release_version=$3

files=(
  LICENSE
  README.md
  lonejson.rockspec.in
  scripts/build_lua_rock.sh
  scripts/render_release_rockspec.sh
  include/lonejson.h
  src/lonejson.c
  src/lonejson_impl.h
  src/lonejson_internal.h
  src/impl/00_prelude.h
  src/impl/10_json_runtime.h
  src/impl/20_parser.h
  src/impl/25_json_pull.h
  src/impl/30_api.h
  src/impl/35_json_pull_impl.h
  src/lua/lonejson_lua.c
  lua/lonejson/init.lua
)

rm -rf "$stage_dir"
mkdir -p "$stage_dir"

for path in "${files[@]}"; do
  if [[ ! -f "$repo_root/$path" ]]; then
    printf 'missing Lua rock source input: %s\n' "$path" >&2
    exit 1
  fi
  mkdir -p "$stage_dir/$(dirname "$path")"
  cp "$repo_root/$path" "$stage_dir/$path"
done

printf '%s\n' "$release_version" >"$stage_dir/VERSION"
printf '%s\n' "${files[@]}" VERSION >"$stage_dir/RELEASE_MANIFEST"
