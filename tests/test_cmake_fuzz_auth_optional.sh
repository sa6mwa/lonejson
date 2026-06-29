#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

if ! command -v clang >/dev/null 2>&1; then
  exit 0
fi

cmake -S "$repo_root" -B "$tmp_dir/build" \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DLONEJSON_BUILD_TESTS=OFF \
  -DLONEJSON_BUILD_EXAMPLES=OFF \
  -DLONEJSON_BUILD_FUZZERS=ON \
  -DLONEJSON_BUILD_WITH_CURL=OFF \
  -DLONEJSON_BUILD_WITH_OPENSSL=OFF \
  -DLONEJSON_BUILD_WITH_JWT=OFF \
  -DLONEJSON_BUILD_WITH_OIDC=OFF >/dev/null

if cmake --build "$tmp_dir/build" --target help | grep -F 'lonejson_fuzz_jwt' >/dev/null; then
  printf 'lonejson_fuzz_jwt target must not be present when auth features are disabled\n' >&2
  exit 1
fi

cmake --build "$tmp_dir/build" --target lonejson_fuzz_validate >/dev/null
