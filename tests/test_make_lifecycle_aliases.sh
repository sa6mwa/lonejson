#!/usr/bin/env bash
set -euo pipefail

repo_root=$1

help_text=$(make -C "$repo_root" help)
for target in \
  build-debug \
  package \
  prerelease-artifacts \
  package-source \
  package-source-smoke \
  package-checksums \
  package-verify \
  verify-release-archives \
  verify-release-privacy \
  prerelease \
  prerelease-hardening \
  test-debug; do
  if ! printf '%s\n' "$help_text" | grep -F "make $target" >/dev/null; then
    printf 'make help is missing target: %s\n' "$target" >&2
    exit 1
  fi
done

check_dry_run_contains() {
  local target=$1
  local expected=$2
  local output

  output=$(make -C "$repo_root" -n "$target" 2>&1 || true)
  if ! printf '%s\n' "$output" | grep -F "$expected" >/dev/null; then
    printf 'make -n %s is missing: %s\n' "$target" "$expected" >&2
    exit 1
  fi
}

check_make_database_contains() {
  local target=$1
  local expected=$2
  local output

  output=$(make -C "$repo_root" -qp "$target" 2>&1 || true)
  if ! printf '%s\n' "$output" | grep -F "$expected" >/dev/null; then
    printf 'make database for %s is missing: %s\n' "$target" "$expected" >&2
    exit 1
  fi
}

check_dry_run_contains build-debug 'cmake --preset debug'
check_dry_run_contains test-debug 'ctest --preset debug'
check_dry_run_contains package-source 'cmake --build --preset package-source'
check_dry_run_contains prerelease-artifacts 'cmake --build --preset package-clean-dist'
check_dry_run_contains prerelease-artifacts 'cmake --build --preset package-single-header'
check_dry_run_contains prerelease-artifacts 'make release-lua-artifacts'
check_dry_run_contains prerelease-artifacts 'cmake --build --preset package-checksums'
check_dry_run_contains prerelease-artifacts 'make package-verify'
check_dry_run_contains package-source-smoke 'scripts/test_release_from_source.sh'
check_dry_run_contains package-checksums 'cmake --build --preset package-checksums'
check_dry_run_contains verify-release-archives 'scripts/verify_release_artifacts.sh'
check_dry_run_contains verify-release-archives 'scripts/verify_release_archives.sh'
check_dry_run_contains verify-release-privacy 'scripts/verify_release_artifacts.sh'
check_dry_run_contains verify-release-privacy 'scripts/verify_release_archives.sh'
check_make_database_contains prerelease 'prerelease: test-all prerelease-artifacts'
check_dry_run_contains prerelease-hardening 'scripts/run_release_matrix.sh'
