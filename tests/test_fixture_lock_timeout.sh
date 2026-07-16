#!/usr/bin/env bash
set -euo pipefail

# Fixture generation is checkout-local generated state. A stale lock must not
# make configure or test commands wait forever.
repo_root=$1
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

output_dir="$tmp_dir/fixtures"
mkdir -p "${output_dir}.lock"

set +e
LONEJSON_LOCK_TIMEOUT_SECONDS=1 \
  "$repo_root/scripts/ensure_large_fixtures.sh" /bin/true /dev/null "$output_dir" \
  >"$tmp_dir/timeout.out" 2>"$tmp_dir/timeout.err"
timeout_status=$?
set -e
if [[ $timeout_status -eq 0 ]]; then
  printf 'fixture generation unexpectedly acquired a held lock\n' >&2
  exit 1
fi
grep -F 'timed out after 1s waiting for generated fixture lock' \
  "$tmp_dir/timeout.err" >/dev/null

set +e
LONEJSON_LOCK_TIMEOUT_SECONDS=invalid \
  "$repo_root/scripts/ensure_large_fixtures.sh" /bin/true /dev/null "$output_dir" \
  >"$tmp_dir/invalid.out" 2>"$tmp_dir/invalid.err"
invalid_status=$?
set -e
if [[ $invalid_status -eq 0 ]]; then
  printf 'invalid fixture lock timeout unexpectedly succeeded\n' >&2
  exit 1
fi
grep -F 'LONEJSON_LOCK_TIMEOUT_SECONDS must be a positive integer' \
  "$tmp_dir/invalid.err" >/dev/null
