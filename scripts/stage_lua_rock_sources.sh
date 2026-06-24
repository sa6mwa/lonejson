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
  src/lua/lonejson_lua.c
  src/lua/lonejson_lua_module.inc.h
  src/lua/lonejson_lua_record.inc.h
  src/lua/lonejson_lua_schema.inc.h
  src/lua/lonejson_lua_schema_methods.inc.h
  src/lua/lonejson_lua_stream_spool.inc.h
  src/lua/lonejson_lua_support.inc.h
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

missing=0
while IFS= read -r staged_file; do
  rel_file=${staged_file#"$stage_dir"/}
  rel_dir=$(dirname "$rel_file")
  while IFS= read -r line; do
    if [[ $line =~ ^[[:space:]]*#[[:space:]]*include[[:space:]]*\"([^\"]+)\" ]]; then
      include_path=${BASH_REMATCH[1]}
      if [[ ! -f "$stage_dir/$rel_dir/$include_path" \
            && ! -f "$stage_dir/include/$include_path" \
            && ! -f "$stage_dir/src/$include_path" ]]; then
        printf 'unresolved Lua rock source include: %s includes %s\n' \
          "$rel_file" "$include_path" >&2
        missing=1
      fi
    fi
  done <"$staged_file"
done < <(find "$stage_dir" -type f \( -name '*.c' -o -name '*.h' \) | LC_ALL=C sort)

if [[ "$missing" -ne 0 ]]; then
  exit 1
fi

printf '%s\n' "$release_version" >"$stage_dir/VERSION"
printf '%s\n' "${files[@]}" VERSION >"$stage_dir/RELEASE_MANIFEST"
