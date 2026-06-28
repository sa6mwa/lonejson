#!/usr/bin/env bash
set -euo pipefail

repo_root=$1

help_text=$(make -C "$repo_root" help)
for target in \
  finalize-slice \
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
  prerelease-live \
  prerelease-hardening \
  release-matrix \
  test-install-tree \
  example-smoke-local \
  deps-debug \
  deps-release \
  dev-up \
  dev-down \
  dev-reset \
  dev-logs \
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
cmake --list-presets -S "$repo_root" | grep -F '"debug-lua"' >/dev/null
check_dry_run_contains test-debug 'ctest --preset debug'
check_dry_run_contains finalize-slice 'make format'
check_dry_run_contains finalize-slice 'make test-debug'
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
check_dry_run_contains prerelease-live 'LONEJSON_ENABLE_LIVE_TESTS=1'
check_dry_run_contains prerelease-hardening 'scripts/clean.sh'
check_dry_run_contains release 'scripts/clean.sh'
check_dry_run_contains release 'make release-matrix'
check_dry_run_contains release-matrix 'scripts/run_release_matrix.sh'
check_dry_run_contains test-install-tree 'scripts/verify_release_archives.sh'
check_dry_run_contains example-smoke-local 'scripts/stage_standalone_examples.sh'
check_make_database_contains deps-debug 'deps-debug: deps-host'
check_make_database_contains deps-release 'deps-release: deps-all'
check_make_database_contains dev-up 'dev-up: compose-up'
check_make_database_contains dev-down 'dev-down: compose-down'
check_dry_run_contains dev-reset 'docker/nginx/generated'
check_make_database_contains dev-logs 'dev-logs: compose-logs'
