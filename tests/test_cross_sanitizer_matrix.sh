#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
script="$repo_root/scripts/run_cross_sanitizer_matrix.sh"

output=$("$script" --dry-run)

for sanitizer in asan tsan msan; do
  for preset in \
    aarch64-linux-gnu-release \
    aarch64-linux-musl-release \
    armhf-linux-gnu-release \
    armhf-linux-musl-release; do
    printf '%s\n' "$output" | \
      grep -F "== $preset $sanitizer ==" >/dev/null
  done
done

printf '%s\n' "$output" | grep -F -- 'qemu-aarch64' >/dev/null
printf '%s\n' "$output" | grep -F -- 'qemu-arm' >/dev/null
printf '%s\n' "$output" | grep -F -- '-DLONEJSON_ENABLE_ASAN=ON' >/dev/null
printf '%s\n' "$output" | grep -F -- '-DLONEJSON_ENABLE_TSAN=ON' >/dev/null
printf '%s\n' "$output" | grep -F -- '-DLONEJSON_ENABLE_MSAN=ON' >/dev/null
printf '%s\n' "$output" | grep -F -- '-DLONEJSON_BUILD_WITH_CURL=ON' >/dev/null
printf '%s\n' "$output" | grep -F -- '-DLONEJSON_BUILD_WITH_OPENSSL=ON' >/dev/null
printf '%s\n' "$output" | grep -F -- '-DLONEJSON_BUILD_WITH_JWT=ON' >/dev/null
printf '%s\n' "$output" | grep -F -- '-DLONEJSON_BUILD_WITH_OIDC=ON' >/dev/null
printf '%s\n' "$output" | grep -F -- 'ctest --test-dir' >/dev/null
printf '%s\n' "$output" | grep -F -- '-E lonejson_\(' >/dev/null

if printf '%s\n' "$output" | grep -F -- 'scripts/clean.sh' >/dev/null; then
  printf 'cross sanitizer matrix must not clean generated state\n' >&2
  exit 1
fi
