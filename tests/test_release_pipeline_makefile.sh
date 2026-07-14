#!/usr/bin/env bash
set -euo pipefail

# Rationale: prerelease and release must exercise the same proof graph, with
# release adding only generated-state cleaning first. This prevents a check from
# existing only in release, where it is discovered too late for fast iteration.

repo_root=$1
makefile="$repo_root/Makefile"

require_text() {
  local text=$1
  if ! grep -F -- "$text" "$makefile" >/dev/null; then
    printf 'missing required Makefile text: %s\n' "$text" >&2
    exit 1
  fi
}

reject_text() {
  local text=$1
  if grep -F -- "$text" "$makefile" >/dev/null; then
    printf 'forbidden Makefile text: %s\n' "$text" >&2
    exit 1
  fi
}

require_text 'release-pipeline'
require_text 'prerelease: release-pipeline'
require_text 'prerelease-hardening: prerelease'
require_text '+$(TIME_STEP) hardening/bench-check $(MAKE) bench-check'
require_text 'release-pipeline:'
require_text '+$(TIME_STEP) prerelease/format $(MAKE) format'
require_text '+$(TIME_STEP) prerelease/test-all $(MAKE) test-all LONEJSON_TEST_ALL_HOST_CURL=0'
require_text '+$(TIME_STEP) prerelease/release-matrix $(MAKE) release-matrix'
require_text 'Skipping test-host-curl: release-matrix runs the full curl-enabled host release tests before packaging'
require_text 'release:'
require_text '$(TIME_STEP) release/clean ./scripts/clean.sh'
require_text '+$(TIME_STEP) release/pipeline $(MAKE) release-pipeline'

reject_text 'prerelease: test-all'
reject_text '+$(TIME_STEP) release/prerelease'
reject_text '+$(TIME_STEP) release/release-matrix'
reject_text '+$(TIME_STEP) bench-check $(MAKE) bench-check'

line_number() {
  local text=$1
  grep -nF -- "$text" "$makefile" | head -n 1 | cut -d: -f1
}

format_line=$(line_number '+$(TIME_STEP) prerelease/format $(MAKE) format')
test_all_line=$(line_number '+$(TIME_STEP) prerelease/test-all $(MAKE) test-all LONEJSON_TEST_ALL_HOST_CURL=0')
matrix_line=$(line_number '+$(TIME_STEP) prerelease/release-matrix $(MAKE) release-matrix')
clean_line=$(line_number '$(TIME_STEP) release/clean ./scripts/clean.sh')
pipeline_line=$(line_number '+$(TIME_STEP) release/pipeline $(MAKE) release-pipeline')

if (( format_line >= test_all_line || test_all_line >= matrix_line )); then
  printf 'release-pipeline must run format, test-all, then release-matrix\n' >&2
  exit 1
fi

if (( clean_line >= pipeline_line )); then
  printf 'release must clean before invoking release-pipeline\n' >&2
  exit 1
fi
