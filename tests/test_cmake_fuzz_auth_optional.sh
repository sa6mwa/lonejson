#!/usr/bin/env bash
set -euo pipefail

# Rationale: fuzz builds must remain available when optional auth integration
# dependencies are absent, so parser hardening is not tied to provider setup.

repo_root=$1
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

afl_description=$("$repo_root/scripts/cpkt-aflpp.sh" discover)
afl_cc=$(printf '%s\n' "$afl_description" | sed -n 's/^cc=//p')

cmake -S "$repo_root" -B "$tmp_dir/build" \
  -DCMAKE_TOOLCHAIN_FILE="$repo_root/cmake/toolchains/linux-x86_64-aflpp.cmake" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DLONEJSON_BUILD_TESTS=OFF \
  -DLONEJSON_BUILD_EXAMPLES=OFF \
  -DLONEJSON_BUILD_FUZZERS=ON \
  -DLONEJSON_BUILD_WITH_CURL=OFF \
  -DLONEJSON_BUILD_WITH_OPENSSL=OFF \
  -DLONEJSON_BUILD_WITH_JWT=OFF \
  -DLONEJSON_BUILD_WITH_OIDC=OFF >/dev/null

cache_cc=$(sed -n 's/^CMAKE_C_COMPILER:[^=]*=//p' "$tmp_dir/build/CMakeCache.txt" | tail -n 1)
[[ "$cache_cc" == "$afl_cc" ]]

if cmake --build "$tmp_dir/build" --target help | grep -F 'lonejson_fuzz_jwt' >/dev/null; then
  printf 'lonejson_fuzz_jwt target must not be present when auth features are disabled\n' >&2
  exit 1
fi
