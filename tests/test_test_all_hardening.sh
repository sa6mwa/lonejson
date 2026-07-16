#!/usr/bin/env bash
set -euo pipefail

# Rationale: test-all must retain the required native Valgrind gate while
# preserving an explicit skip only for optional TSan support.

repo_root=$1

without_support=$(make -C "$repo_root" -n test-all LONEJSON_HAVE_TSAN=0)
printf '%s\n' "$without_support" | grep -F 'test-host-curl' >/dev/null
printf '%s\n' "$without_support" | grep -E '(^|[[:space:]])make[[:space:]]+tsan($|[[:space:]])' >/dev/null
printf '%s\n' "$without_support" | grep -E '(^|[[:space:]])make[[:space:]]+valgrind($|[[:space:]])' >/dev/null

tsan_only=$(make -C "$repo_root" -n test-all LONEJSON_HAVE_TSAN=1)
printf '%s\n' "$tsan_only" | grep -E '(^|[[:space:]])make[[:space:]]+tsan($|[[:space:]])' >/dev/null
printf '%s\n' "$tsan_only" | grep -E '(^|[[:space:]])make[[:space:]]+valgrind($|[[:space:]])' >/dev/null

release_pipeline_test_all=$(make -C "$repo_root" -n test-all LONEJSON_TEST_ALL_HOST_CURL=0 LONEJSON_HAVE_TSAN=0)
if printf '%s\n' "$release_pipeline_test_all" | grep -E '(^|[[:space:]])make[[:space:]]+test-host-curl($|[[:space:]])' >/dev/null; then
  echo "release-pipeline test-all mode should not invoke standalone test-host-curl" >&2
  exit 1
fi
printf '%s\n' "$release_pipeline_test_all" | grep -F 'Skipping test-host-curl: release-matrix runs the full curl-enabled host release tests before packaging' >/dev/null
printf '%s\n' "$release_pipeline_test_all" | grep -E '(^|[[:space:]])make[[:space:]]+test-e2e($|[[:space:]])' >/dev/null
if printf '%s\n' "$release_pipeline_test_all" | grep -E '(^|[[:space:]])make[[:space:]]+bench-check($|[[:space:]])' >/dev/null; then
  echo "test-all should not invoke bench-check; benchmark gates belong to prerelease-hardening or explicit bench targets" >&2
  exit 1
fi

hardening_dry_run=$(make -C "$repo_root" -n prerelease-hardening LONEJSON_HAVE_TSAN=0)
printf '%s\n' "$hardening_dry_run" | grep -E '(^|[[:space:]])make[[:space:]]+bench-check($|[[:space:]])' >/dev/null

asan_dry_run=$(make -C "$repo_root" -n asan)
printf '%s\n' "$asan_dry_run" | grep -F 'lua_external_liblonejson_tests' >/dev/null
printf '%s\n' "$asan_dry_run" | grep -F 'lua_target_tests' >/dev/null

tsan_dry_run=$(make -C "$repo_root" -n tsan)
printf '%s\n' "$tsan_dry_run" | grep -F 'lua_external_liblonejson_tests' >/dev/null
printf '%s\n' "$tsan_dry_run" | grep -F 'lua_target_tests' >/dev/null
printf '%s\n' "$tsan_dry_run" | grep -F 'check_bootlin_tsan_support.sh' >/dev/null

valgrind_dry_run=$(make -C "$repo_root" -n valgrind)
printf '%s\n' "$valgrind_dry_run" | grep -F 'valgrind --leak-check=full' >/dev/null
