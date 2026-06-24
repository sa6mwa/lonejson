#!/usr/bin/env bash
set -euo pipefail

repo_root=$1

without_support=$(make -C "$repo_root" -n test-all LONEJSON_HAVE_TSAN=0 LONEJSON_HAVE_MSAN=0)
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

asan_dry_run=$(make -C "$repo_root" -n asan)
printf '%s\n' "$asan_dry_run" | grep -F 'lua_external_liblonejson_tests' >/dev/null
printf '%s\n' "$asan_dry_run" | grep -F 'lua_target_tests' >/dev/null

tsan_dry_run=$(make -C "$repo_root" -n tsan)
printf '%s\n' "$tsan_dry_run" | grep -F 'lua_external_liblonejson_tests' >/dev/null
printf '%s\n' "$tsan_dry_run" | grep -F 'lua_target_tests' >/dev/null

msan_dry_run=$(make -C "$repo_root" -n msan)
printf '%s\n' "$msan_dry_run" | grep -F 'lua_external_liblonejson_tests' >/dev/null
printf '%s\n' "$msan_dry_run" | grep -F 'lua_target_tests' >/dev/null
