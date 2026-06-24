#!/usr/bin/env bash
set -euo pipefail

repo_root=$1

help_text=$(make -C "$repo_root" help)
for target in \
  build-debug \
  package \
  package-source \
  package-source-smoke \
  package-checksums \
  package-verify \
  verify-release-archives \
  verify-release-privacy \
  prerelease \
  prerelease-hardening \
  test-debug; do
  printf '%s\n' "$help_text" | grep -F "make $target" >/dev/null
done

check_dry_run_contains() {
  local target=$1
  local expected=$2
  local output

  output=$(make -C "$repo_root" -n "$target")
  printf '%s\n' "$output" | grep -F "$expected" >/dev/null
}

check_dry_run_contains build-debug 'cmake --preset debug'
check_dry_run_contains test-debug 'ctest --preset debug'
check_dry_run_contains package-source 'cmake --build --preset package-source'
check_dry_run_contains package-source-smoke 'scripts/test_release_from_source.sh'
check_dry_run_contains package-checksums 'cmake --build --preset package-checksums'
check_dry_run_contains verify-release-archives 'scripts/verify_release_artifacts.sh'
check_dry_run_contains verify-release-archives 'scripts/verify_release_archives.sh'
check_dry_run_contains verify-release-privacy 'scripts/verify_release_artifacts.sh'
check_dry_run_contains verify-release-privacy 'scripts/verify_release_archives.sh'
check_dry_run_contains prerelease 'make test'
check_dry_run_contains prerelease-hardening 'scripts/run_release_matrix.sh'
