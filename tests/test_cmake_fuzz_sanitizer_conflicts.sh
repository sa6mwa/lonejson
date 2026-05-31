#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

run_expect_fail() {
  local name=$1
  shift
  local log="$tmp_dir/$name.log"
  if cmake -S "$repo_root" -B "$tmp_dir/$name" "$@" >"$log" 2>&1; then
    echo "expected cmake configure to fail for $name" >&2
    exit 1
  fi
  grep -F 'LONEJSON_BUILD_FUZZERS is incompatible with' "$log" >/dev/null
  grep -F 'LONEJSON_ENABLE_TSAN' "$log" >/dev/null
  grep -F 'LONEJSON_ENABLE_MSAN.' "$log" >/dev/null
}

run_expect_fail fuzz_tsan \
  -DCMAKE_BUILD_TYPE=Debug \
  -DLONEJSON_BUILD_TESTS=OFF \
  -DLONEJSON_BUILD_EXAMPLES=OFF \
  -DLONEJSON_BUILD_FUZZERS=ON \
  -DLONEJSON_ENABLE_TSAN=ON

if command -v clang >/dev/null 2>&1; then
  run_expect_fail fuzz_msan \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_BUILD_TYPE=Debug \
    -DLONEJSON_BUILD_TESTS=OFF \
    -DLONEJSON_BUILD_EXAMPLES=OFF \
    -DLONEJSON_BUILD_FUZZERS=ON \
    -DLONEJSON_ENABLE_MSAN=ON
fi
