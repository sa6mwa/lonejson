#!/usr/bin/env bash
set -euo pipefail

# Rationale: the cross sanitizer matrix is a narrow hardening route; this test
# keeps it pointed at the supported target/preset instead of growing ad hoc.

repo_root=$1
script="$repo_root/scripts/run_cross_sanitizer_matrix.sh"
script_text=$(cat "$script")

output=$("$script" --dry-run)

printf '%s\n' "$output" | grep -F "== armhf-linux-gnu-release asan ==" >/dev/null
printf '%s\n' "$script_text" | grep -F -- 'armhf-linux-gnu-release armhf-linux-gnu asan' >/dev/null

printf '%s\n' "$script_text" | grep -F -- '/usr/bin/qemu-arm' >/dev/null
printf '%s\n' "$output" | grep -F -- '-DLONEJSON_ENABLE_ASAN=ON' >/dev/null
printf '%s\n' "$output" | grep -F -- '-DLONEJSON_BUILD_WITH_CURL=ON' >/dev/null
printf '%s\n' "$output" | grep -F -- '-DLONEJSON_BUILD_WITH_OPENSSL=ON' >/dev/null
printf '%s\n' "$output" | grep -F -- '-DLONEJSON_BUILD_WITH_JWT=ON' >/dev/null
printf '%s\n' "$output" | grep -F -- '-DLONEJSON_BUILD_WITH_OIDC=ON' >/dev/null
printf '%s\n' "$output" | grep -F -- '-DLONEJSON_TEST_TIMEOUT=600' >/dev/null
printf '%s\n' "$output" | grep -F -- 'ctest --test-dir' >/dev/null

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

printf '%s\n' "$script_text" | grep -F -- 'toolchain_resolver="$repo_root/scripts/cpkt-toolchains.sh"' >/dev/null
printf '%s\n' "$script_text" | grep -F -- 'lifecycle-managed Bootlin GCC' >/dev/null
if printf '%s\n' "$script_text" | grep -F -- '$HOME/.local/cross' >/dev/null; then
  printf 'cross sanitizer matrix must not depend on a workstation-local cross prefix\n' >&2
  exit 1
fi

if printf '%s\n' "$script_text" | grep -F -- 'return 0' | grep -F -- 'run_probe' >/dev/null; then
  printf 'cross sanitizer probes must be release-blocking\n' >&2
  exit 1
fi
