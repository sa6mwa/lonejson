#!/usr/bin/env bash
set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$repo_root"

exclude=${1:-}
shift || true
if [[ -z "$exclude" || $# -eq 0 ]]; then
  printf 'usage: %s EXCLUDE_REGEX PRESET...\n' "$0" >&2
  exit 2
fi

for preset in "$@"; do
  cmake --preset "$preset"
  cmake --build --preset "$preset"
  ctest --preset "$preset" --output-on-failure -E "$exclude"
done
