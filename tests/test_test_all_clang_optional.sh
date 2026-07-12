#!/usr/bin/env bash
set -euo pipefail

# Rationale: test-all must include supported sanitizer gates while giving
# explicit skips for unavailable compiler support, not silent omissions.

repo_root=$1

without_support=$(make -C "$repo_root" -n test-all LONEJSON_HAVE_TSAN=0 LONEJSON_HAVE_MSAN=0)
printf '%s\n' "$without_support" | grep -F 'test-host-curl' >/dev/null
if printf '%s\n' "$without_support" | grep -E '(^|[[:space:]])make[[:space:]]+tsan($|[[:space:]])' >/dev/null; then
  echo "test-all should not invoke tsan when tsan is unsupported" >&2
  exit 1
fi
if printf '%s\n' "$without_support" | grep -E '(^|[[:space:]])make[[:space:]]+msan($|[[:space:]])' >/dev/null; then
  echo "test-all should not invoke msan when msan is unsupported" >&2
  exit 1
fi
printf '%s\n' "$without_support" | grep -F 'Skipping tsan: unsupported toolchain' >/dev/null
printf '%s\n' "$without_support" | grep -F 'Skipping msan: unsupported toolchain' >/dev/null

tsan_only=$(make -C "$repo_root" -n test-all LONEJSON_HAVE_TSAN=1 LONEJSON_HAVE_MSAN=0)
printf '%s\n' "$tsan_only" | grep -E '(^|[[:space:]])make[[:space:]]+tsan($|[[:space:]])' >/dev/null
if printf '%s\n' "$tsan_only" | grep -E '(^|[[:space:]])make[[:space:]]+msan($|[[:space:]])' >/dev/null; then
  echo "test-all should not invoke msan when msan is unsupported" >&2
  exit 1
fi
printf '%s\n' "$tsan_only" | grep -F 'Skipping msan: unsupported toolchain' >/dev/null

full_support=$(make -C "$repo_root" -n test-all LONEJSON_HAVE_TSAN=1 LONEJSON_HAVE_MSAN=1)
printf '%s\n' "$full_support" | grep -E '(^|[[:space:]])make[[:space:]]+tsan($|[[:space:]])' >/dev/null
printf '%s\n' "$full_support" | grep -E '(^|[[:space:]])make[[:space:]]+msan($|[[:space:]])' >/dev/null

release_pipeline_test_all=$(make -C "$repo_root" -n test-all LONEJSON_TEST_ALL_HOST_CURL=0 LONEJSON_HAVE_TSAN=0 LONEJSON_HAVE_MSAN=0)
if printf '%s\n' "$release_pipeline_test_all" | grep -E '(^|[[:space:]])make[[:space:]]+test-host-curl($|[[:space:]])' >/dev/null; then
  echo "release-pipeline test-all mode should not invoke standalone test-host-curl" >&2
  exit 1
fi
printf '%s\n' "$release_pipeline_test_all" | grep -F 'Skipping test-host-curl: release-matrix runs the full curl-enabled host release tests before packaging' >/dev/null

asan_dry_run=$(make -C "$repo_root" -n asan)
printf '%s\n' "$asan_dry_run" | grep -F 'lua_external_liblonejson_tests' >/dev/null
printf '%s\n' "$asan_dry_run" | grep -F 'lua_target_tests' >/dev/null

tsan_dry_run=$(make -C "$repo_root" -n tsan)
printf '%s\n' "$tsan_dry_run" | grep -F 'lua_external_liblonejson_tests' >/dev/null
printf '%s\n' "$tsan_dry_run" | grep -F 'lua_target_tests' >/dev/null

msan_dry_run=$(make -C "$repo_root" -n msan)
printf '%s\n' "$msan_dry_run" | grep -F 'lua_external_liblonejson_tests' >/dev/null
printf '%s\n' "$msan_dry_run" | grep -F 'lua_target_tests' >/dev/null
