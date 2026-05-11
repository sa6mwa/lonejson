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
  src/impl/10_source_runtime.h
  src/impl/11_json_value_io.h
  src/impl/12_json_value_parse.h
  src/impl/13_json_value_visit.h
  src/impl/14_json_value_api.h
  src/impl/20_parser.h
  src/impl/20_parser_map.h
  src/impl/21_parser_values.h
  src/impl/22_parser_actions.h
  src/impl/22_parser_engine.h
  src/impl/23_serializer_io.h
  src/impl/24_serializer.h
  src/impl/25_json_pull.h
  src/impl/30_api.h
  src/impl/32_generator_api.h
  src/impl/33_array_stream_api.h
  src/impl/34_protocol_framing_api.h
  src/impl/35_json_pull_impl.h
  src/impl/36_curl_api.h
  src/lua/lonejson_lua.c
  src/lua/lonejson_lua_module.inc
  src/lua/lonejson_lua_record.inc
  src/lua/lonejson_lua_schema.inc
  src/lua/lonejson_lua_schema_methods.inc
  src/lua/lonejson_lua_stream_spool.inc
  src/lua/lonejson_lua_support.inc
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
