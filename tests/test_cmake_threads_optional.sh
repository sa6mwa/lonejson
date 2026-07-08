#!/usr/bin/env bash
set -euo pipefail

# Rationale: thread support is optional for consumers; disabling Threads must
# not break the core C build graph.

repo_root=$1
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

cmake -S "$repo_root" -B "$tmp_dir/build" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_STANDARD=11 \
  -DCMAKE_C_EXTENSIONS=OFF \
  -DCMAKE_DISABLE_FIND_PACKAGE_Threads=TRUE \
  -DLONEJSON_BUILD_TESTS=OFF \
  -DLONEJSON_BUILD_EXAMPLES=OFF \
  -DLONEJSON_BUILD_BENCHMARKS=OFF \
  -DLONEJSON_BUILD_FUZZERS=OFF >/dev/null

cmake --build "$tmp_dir/build" --target lonejson_static >/dev/null
