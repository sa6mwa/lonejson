#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
lua_exec=${2:-lua}
luarocks_exec=${3:-luarocks}
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

rock_tree="$tmp_dir/luarocks"
rockspec="$tmp_dir/lonejson-0.0.0-1.rockspec"
test_lua="$tmp_dir/test_lua_schema_cache.lua"

mkdir -p "$rock_tree"
cat >"$test_lua" <<'EOF'
local lonejson = require("lonejson")
local lj = lonejson.new()
local Schema = lj.schema("CacheableNested", {
  lj.field("payload", lj.object {
    fields = {
      lj.field("name", lj.string { fixed_capacity = 32, overflow = "fail" }),
      lj.field("count", lj.i64()),
    },
  }),
})
local json = [[{"payload":{"name":"alpha","count":3}}]]

assert(type(lj.core._test_reset_map_analysis_count) == "function")
assert(type(lj.core._test_get_map_analysis_count) == "function")
assert(type(lj.core._test_map_cache_enabled) == "function")
assert(type(lj.core._test_schema_map_cacheable) == "function")
assert(type(lj.core._test_schema_map_cookie) == "function")
assert(lj.core._test_map_cache_enabled())
assert(lj.core._test_schema_map_cacheable(Schema))

local Schema2 = lj.schema("CacheableNested2", {
  lj.field("payload", lj.object {
    fields = {
      lj.field("name", lj.string { fixed_capacity = 32, overflow = "fail" }),
      lj.field("count", lj.i64()),
    },
  }),
})
assert(lj.core._test_schema_map_cookie(Schema) ~= lj.core._test_schema_map_cookie(Schema2))

lj.core._test_reset_map_analysis_count()
local first = Schema:decode(json)
assert(first.payload.name == "alpha")
local count_after_first = lj.core._test_get_map_analysis_count()
assert(count_after_first == 0)

local second = Schema:decode(json)
assert(second.payload.name == "alpha")
local count_after_second = lj.core._test_get_map_analysis_count()
assert(count_after_second == count_after_first,
       string.format("expected stable analysis counter facade, got %d then %d",
                     count_after_first, count_after_second))
EOF

(
  cd "$repo_root"
  lib_ext="$("$luarocks_exec" config variables.LIB_EXTENSION)"
  ./scripts/render_release_rockspec.sh \
    "0.0.0" "$rockspec" "git+file://$repo_root" "" "$lib_ext"
  CFLAGS="${CFLAGS:+$CFLAGS }-DLONEJSON_TEST_MAP_ANALYSIS_COUNTER=1" \
  LONEJSON_LIBDIR="$repo_root/build/debug" \
    "$luarocks_exec" make --tree "$rock_tree" "$rockspec" >/dev/null
)

eval "$("$luarocks_exec" path --tree "$rock_tree")"
export LD_LIBRARY_PATH="$repo_root/build/debug:${LD_LIBRARY_PATH:-}"
export DYLD_LIBRARY_PATH="$repo_root/build/debug:${DYLD_LIBRARY_PATH:-}"
"$lua_exec" "$test_lua"
