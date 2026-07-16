#!/usr/bin/env bash
set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
preset=${1:-}
stage_examples=0

if [[ -z "$preset" ]]; then
  printf 'usage: %s PRESET [--stage-examples]\n' "$0" >&2
  exit 2
fi
shift
if [[ "${1:-}" == --stage-examples ]]; then
  stage_examples=1
  shift
fi
if [[ $# -ne 0 ]]; then
  printf 'usage: %s PRESET [--stage-examples]\n' "$0" >&2
  exit 2
fi

cmake --preset "$preset" -S "$repo_root"
cmake --build --preset "$preset"
if [[ "$stage_examples" -eq 1 ]]; then
  "$repo_root/scripts/stage_standalone_examples.sh"
fi
