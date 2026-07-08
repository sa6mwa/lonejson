#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
script="$repo_root/scripts/run_cross_sanitizer_matrix.sh"
script_text=$(cat "$script")

output=$("$script" --dry-run)

printf '%s\n' "$output" | grep -F "== armhf-linux-gnu-release asan ==" >/dev/null
printf '%s\n' "$script_text" | grep -F -- 'armhf-linux-gnu-release armhf-linux-gnu asan' >/dev/null

printf '%s\n' "$output" | grep -F -- 'qemu-arm' >/dev/null
printf '%s\n' "$output" | grep -F -- '-DLONEJSON_ENABLE_ASAN=ON' >/dev/null
printf '%s\n' "$output" | grep -F -- '-DLONEJSON_BUILD_WITH_CURL=ON' >/dev/null
printf '%s\n' "$output" | grep -F -- '-DLONEJSON_BUILD_WITH_OPENSSL=ON' >/dev/null
printf '%s\n' "$output" | grep -F -- '-DLONEJSON_BUILD_WITH_JWT=ON' >/dev/null
printf '%s\n' "$output" | grep -F -- '-DLONEJSON_BUILD_WITH_OIDC=ON' >/dev/null
printf '%s\n' "$output" | grep -F -- '-DLONEJSON_TEST_TIMEOUT=600' >/dev/null
printf '%s\n' "$output" | grep -F -- 'ctest --test-dir' >/dev/null
printf '%s\n' "$output" | grep -F -- '-E lonejson_\(' >/dev/null

if printf '%s\n' "$output" | grep -F -- 'scripts/clean.sh' >/dev/null; then
  printf 'cross sanitizer matrix must not clean generated state\n' >&2
  exit 1
fi

if printf '%s\n' "$output" | grep -F -- 'qemu-aarch64' >/dev/null; then
  printf 'cross sanitizer matrix must not advertise unsupported aarch64 QEMU sanitizer coverage\n' >&2
  exit 1
fi

if printf '%s\n' "$output" | grep -F -- '-DLONEJSON_ENABLE_TSAN=ON' >/dev/null; then
  printf 'cross sanitizer matrix must not advertise unsupported cross TSan coverage\n' >&2
  exit 1
fi

if printf '%s\n' "$output" | grep -F -- '-DLONEJSON_ENABLE_MSAN=ON' >/dev/null; then
  printf 'cross sanitizer matrix must not advertise unsupported cross MSan coverage\n' >&2
  exit 1
fi

if printf '%s\n' "$script_text" | grep -E '^(aarch64|armhf-linux-musl|aarch64-linux-musl)' >/dev/null; then
  printf 'cross sanitizer matrix must list only currently executable QEMU sanitizer targets\n' >&2
  exit 1
fi

if printf '%s\n' "$script_text" | grep -F -- 'PKT_SKIP' >/dev/null; then
  printf 'cross sanitizer matrix must not skip configured sanitizer targets\n' >&2
  exit 1
fi

printf '%s\n' "$script_text" | grep -F -- 'CPKT_AARCH64_MUSL_PREFIX' >/dev/null
printf '%s\n' "$script_text" | grep -F -- 'CPKT_ARMHF_MUSL_PREFIX' >/dev/null
if printf '%s\n' "$script_text" | grep -F -- '$HOME/.local/cross/aarch64-linux-musl/bin' >/dev/null; then
  printf 'cross sanitizer matrix must resolve aarch64 musl tools through the lifecycle prefix override\n' >&2
  exit 1
fi
if printf '%s\n' "$script_text" | grep -F -- '$HOME/.local/cross/arm-linux-musleabihf/bin' >/dev/null; then
  printf 'cross sanitizer matrix must resolve armhf musl tools through the lifecycle prefix override\n' >&2
  exit 1
fi

if printf '%s\n' "$script_text" | grep -F -- 'return 0' | grep -F -- 'run_probe' >/dev/null; then
  printf 'cross sanitizer probes must be release-blocking\n' >&2
  exit 1
fi
