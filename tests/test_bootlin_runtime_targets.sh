#!/usr/bin/env bash
set -euo pipefail

build_dir=${1:?usage: test_bootlin_runtime_targets.sh BUILD_DIR EXECUTABLE...}
shift
if [[ $# -eq 0 ]]; then
  printf 'missing development executable runtime targets\n' >&2
  exit 1
fi

for executable in "$@"; do
  "$(dirname -- "$0")/test_bootlin_runtime.sh" "$build_dir" "$executable"
done
