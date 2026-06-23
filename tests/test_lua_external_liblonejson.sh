#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
lua_exec=${2:-lua}
luarocks_exec=${3:-luarocks}
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

rock_tree="$tmp_dir/luarocks"
rockspec="$tmp_dir/lonejson-0.0.0-1.rockspec"
libdir="$repo_root/build/debug"

mkdir -p "$rock_tree"

(
  cd "$repo_root"
  lib_ext="$("$luarocks_exec" config variables.LIB_EXTENSION)"
  ./scripts/render_release_rockspec.sh \
    "0.0.0" "$rockspec" "git+file://$repo_root" "" "$lib_ext"
  LONEJSON_LIBDIR="$libdir" \
    "$luarocks_exec" make --tree "$rock_tree" "$rockspec" >/dev/null
)

module_path=$(
  find "$rock_tree" -type f -path '*/lonejson/core.*' | LC_ALL=C sort | head -n 1
)
if [[ -z "$module_path" || ! -f "$module_path" ]]; then
  printf 'Lua core module was not built in %s\n' "$rock_tree" >&2
  exit 1
fi

if command -v nm >/dev/null 2>&1; then
  if nm -D --defined-only "$module_path" 2>/dev/null |
       awk '{print $NF}' | grep -E '^lonejson_' >/dev/null; then
    printf 'Lua module exports lonejson core API symbols:\n' >&2
    nm -D --defined-only "$module_path" 2>/dev/null |
      awk '{print $NF}' | grep -E '^lonejson_' >&2
    exit 1
  fi
fi

if command -v readelf >/dev/null 2>&1; then
  if ! readelf -d "$module_path" | grep -E 'Shared library: \[liblonejson\.so' >/dev/null; then
    printf 'Lua module does not declare a liblonejson.so dynamic dependency\n' >&2
    readelf -d "$module_path" >&2
    exit 1
  fi
elif command -v otool >/dev/null 2>&1; then
  if ! otool -L "$module_path" | grep -E 'liblonejson\.(dylib|[0-9]+\.dylib)' >/dev/null; then
    printf 'Lua module does not declare a liblonejson dynamic dependency\n' >&2
    otool -L "$module_path" >&2
    exit 1
  fi
fi

eval "$("$luarocks_exec" path --tree "$rock_tree")"
export LD_LIBRARY_PATH="$libdir:${LD_LIBRARY_PATH:-}"
export DYLD_LIBRARY_PATH="$libdir:${DYLD_LIBRARY_PATH:-}"
"$lua_exec" -e 'assert(require("lonejson"))'
