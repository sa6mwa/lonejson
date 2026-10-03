#!/usr/bin/env bash
set -euo pipefail
nm=${1:?usage: check_library_exports.sh NM SYSTEM LIBRARY ALLOWLIST}
system=${2:?}
library=${3:?}
allowlist=${4:?}
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
mkdir -p "$root/build"
workspace=$(mktemp -d "$root/build/exports.XXXXXX")
trap 'rm -rf "$workspace"' EXIT
if [[ "$system" == Darwin ]]; then
  "$nm" -gU -P "$library" >"$workspace/nm"
  awk '{sub(/^_/, "", $1); print $1}' "$workspace/nm" | LC_ALL=C sort -u >"$workspace/actual"
  "$nm" -gu -P "$library" >"$workspace/imports"
else
  "$nm" --dynamic --defined-only --format=posix "$library" >"$workspace/nm"
  awk '{print $1}' "$workspace/nm" | LC_ALL=C sort -u >"$workspace/actual"
  "$nm" --dynamic --undefined-only --format=posix "$library" >"$workspace/imports"
fi
LC_ALL=C sort -u "$allowlist" >"$workspace/expected"
if ! diff -u "$workspace/expected" "$workspace/actual"; then
  printf 'unexpected dynamic exports: %s\n' "$library" >&2
  exit 1
fi
while read -r symbol _; do
  [[ "$system" != Darwin ]] || symbol=${symbol#_}
  case "$symbol" in
    lonejson_*)
      if ! grep -Fx "$symbol" "$root/cmake/lonejson.exports" >/dev/null; then
        printf 'private lonejson import: %s: %s\n' "$library" "$symbol" >&2
        exit 1
      fi
      ;;
  esac
done <"$workspace/imports"
