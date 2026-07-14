#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

default_version=$("$repo_root/scripts/release_version.sh")
if [[ "$default_version" != "0.0.0" ]]; then
  case "$(git -C "$repo_root" describe --tags --exact-match 2>/dev/null || true)" in
    v[0-9]*.[0-9]*.[0-9]*) ;;
    *)
      printf 'untagged git worktree resolved unexpected version: %s\n' "$default_version" >&2
      exit 1
      ;;
  esac
fi

LONEJSON_VERSION_OVERRIDE=7.8.9 "$repo_root/scripts/release_version.sh" |
  grep -qx '7.8.9'

set +e
LONEJSON_VERSION_OVERRIDE=7.8.9-rc.1 "$repo_root/scripts/release_version.sh" \
  >"$tmp_dir/invalid.out" 2>"$tmp_dir/invalid.err"
invalid_status=$?
set -e
if [[ "$invalid_status" -eq 0 ]]; then
  printf 'invalid LONEJSON_VERSION_OVERRIDE was accepted\n' >&2
  exit 1
fi
grep -F 'invalid LONEJSON_VERSION_OVERRIDE value' "$tmp_dir/invalid.err" >/dev/null

cmake -S "$repo_root" -B "$tmp_dir/cmake-version" -G Ninja \
  -D LONEJSON_VERSION_OVERRIDE=7.8.9 \
  -D LONEJSON_BUILD_TESTS=OFF \
  -D LONEJSON_BUILD_EXAMPLES=OFF \
  >"$tmp_dir/cmake-version.out" 2>"$tmp_dir/cmake-version.err"
grep -qx 'CMAKE_PROJECT_VERSION:STATIC=7.8.9' \
  "$tmp_dir/cmake-version/CMakeCache.txt"

cmake -S "$repo_root" -B "$tmp_dir/cmake-environment-version" -G Ninja \
  -D LONEJSON_BUILD_TESTS=OFF \
  -D LONEJSON_BUILD_EXAMPLES=OFF \
  >"$tmp_dir/cmake-environment-default.out" \
  2>"$tmp_dir/cmake-environment-default.err"
LONEJSON_VERSION_OVERRIDE=7.8.9 cmake \
  -S "$repo_root" -B "$tmp_dir/cmake-environment-version" -G Ninja \
  -D LONEJSON_BUILD_TESTS=OFF \
  -D LONEJSON_BUILD_EXAMPLES=OFF \
  >"$tmp_dir/cmake-environment-version.out" \
  2>"$tmp_dir/cmake-environment-version.err"
grep -qx 'CMAKE_PROJECT_VERSION:STATIC=7.8.9' \
  "$tmp_dir/cmake-environment-version/CMakeCache.txt"

make -s -C "$repo_root" print-release-version LONEJSON_VERSION_OVERRIDE=7.8.9 |
  grep -qx '7.8.9'
