#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 3 ]]; then
  printf 'usage: %s <output-src-rock> <rockspec> <source-tarball>\n' "$0" >&2
  exit 1
fi

output_src_rock=$1
rockspec=$2
source_tarball=$3

if [[ ! -f "$rockspec" ]]; then
  printf 'package_lua_src_rock.sh: rockspec not found: %s\n' "$rockspec" >&2
  exit 1
fi
if [[ ! -f "$source_tarball" ]]; then
  printf 'package_lua_src_rock.sh: source tarball not found: %s\n' "$source_tarball" >&2
  exit 1
fi
if ! command -v zip >/dev/null 2>&1; then
  printf 'package_lua_src_rock.sh: zip not found in PATH\n' >&2
  exit 1
fi

rm -f "$output_src_rock"
zip -q -j "$output_src_rock" "$rockspec" "$source_tarball"
