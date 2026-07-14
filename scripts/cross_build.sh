#!/usr/bin/env bash
set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$repo_root"

if [[ $# -eq 0 ]]; then
  printf 'usage: %s PRESET...\n' "$0" >&2
  exit 2
fi

for preset in "$@"; do
  cmake --preset "$preset"
  cmake --build --preset "$preset"
done
