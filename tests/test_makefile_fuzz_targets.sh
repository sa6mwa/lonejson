#!/usr/bin/env bash
set -euo pipefail

repo_root=$1

dry_run=$(make --no-print-directory -C "$repo_root" -n fuzz FUZZ_TIME=0)

require_line() {
  local pattern=$1
  local message=$2

  if ! printf '%s\n' "$dry_run" | grep -F -- "$pattern" >/dev/null; then
    printf '%s\n' "$dry_run" >&2
    printf '%s\n' "$message" >&2
    exit 1
  fi
}

require_line 'lonejson_fuzz_path_value_visitor' \
  'make fuzz does not build/run lonejson_fuzz_path_value_visitor'
require_line 'build/fuzz/corpus/path_value_visitor' \
  'make fuzz does not reference the path value visitor corpus directory'
require_line 'fuzz/corpus/path_value_visitor/.' \
  'make fuzz does not copy the path value visitor seed corpus'
require_line 'artifacts/path_value_visitor' \
  'make fuzz does not prepare path value visitor artifacts'
