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

cd "$repo_root"
case "$preset" in
  debug|debug-lua|host)
    bundle_root=$("$repo_root/scripts/detect_cpkt_bundle.sh")
    cmake --preset "$preset" -S "$repo_root" -D "LONEJSON_CPKT_ROOT=$bundle_root" ;;
  *) cmake --preset "$preset" -S "$repo_root" ;;
esac
cmake --build --preset "$preset"
if [[ "$stage_examples" -eq 1 ]]; then
  "$repo_root/scripts/stage_standalone_examples.sh"
fi
