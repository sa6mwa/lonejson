#!/usr/bin/env bash
set -euo pipefail
repo_root=$(CDPATH= cd -- "$1" && pwd -P)
mkdir -p "$repo_root/build"
tmp_dir=$(mktemp -d "$repo_root/build/version-config.XXXXXX")
trap 'rm -rf -- "$tmp_dir"' EXIT
expected_default=$(env -u LONEJSON_VERSION_OVERRIDE "$repo_root/scripts/release_version.sh")
override_version=$(LONEJSON_VERSION_OVERRIDE=7.8.9 "$repo_root/scripts/release_version.sh")
version_authoritative=0
git_top=$(git -C "$repo_root" rev-parse --show-toplevel 2>/dev/null || true)
if [[ "$git_top" != "$repo_root" ]]; then
  version_authoritative=1
else
  while read -r tag; do
    if [[ "$tag" =~ ^v[0-9]+[.][0-9]+[.][0-9]+$ ]] &&
       [[ "$(git -C "$repo_root" cat-file -t "refs/tags/$tag")" == commit ]]; then
      version_authoritative=1
    fi
  done < <(git -C "$repo_root" tag --points-at HEAD)
fi
cmake -S "$repo_root" -B "$tmp_dir/cmake-version" -G Ninja \
  -D LONEJSON_VERSION_OVERRIDE=7.8.9 \
  -D LONEJSON_BUILD_TESTS=OFF \
  -D LONEJSON_BUILD_EXAMPLES=OFF \
  >"$tmp_dir/cmake-version.out" 2>"$tmp_dir/cmake-version.err"
grep -qx "CMAKE_PROJECT_VERSION:STATIC=$override_version" \
  "$tmp_dir/cmake-version/CMakeCache.txt"

env -u LONEJSON_VERSION_OVERRIDE cmake \
  -S "$repo_root" -B "$tmp_dir/cmake-environment-version" -G Ninja \
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
grep -qx "CMAKE_PROJECT_VERSION:STATIC=$override_version" \
  "$tmp_dir/cmake-environment-version/CMakeCache.txt"
grep -qx 'LONEJSON_VERSION_OVERRIDE:STRING=' \
  "$tmp_dir/cmake-environment-version/CMakeCache.txt"
env -u LONEJSON_VERSION_OVERRIDE cmake \
  -S "$repo_root" -B "$tmp_dir/cmake-environment-version" -G Ninja \
  -D LONEJSON_BUILD_TESTS=OFF \
  -D LONEJSON_BUILD_EXAMPLES=OFF \
  >"$tmp_dir/cmake-environment-cleared.out" \
  2>"$tmp_dir/cmake-environment-cleared.err"
grep -qx "CMAKE_PROJECT_VERSION:STATIC=$expected_default" \
  "$tmp_dir/cmake-environment-version/CMakeCache.txt"

make -s -C "$repo_root" print-release-version LONEJSON_VERSION_OVERRIDE=7.8.9 |
  grep -qx "$override_version"

if [[ "$version_authoritative" == 1 ]]; then
  make -s -C "$repo_root" print-release-version LONEJSON_VERSION_OVERRIDE=bad |
    grep -qx "$expected_default"
else
  set +e
  make -s -C "$repo_root" print-release-version LONEJSON_VERSION_OVERRIDE=bad \
    >"$tmp_dir/make-invalid.out" 2>"$tmp_dir/make-invalid.err"
  make_invalid_status=$?
  set -e
  if [[ "$make_invalid_status" -eq 0 ]]; then
    printf 'make accepted invalid LONEJSON_VERSION_OVERRIDE\n' >&2
    exit 1
  fi
  grep -F 'invalid LONEJSON_VERSION_OVERRIDE value: bad' \
    "$tmp_dir/make-invalid.err" >/dev/null
fi

if [[ "$version_authoritative" == 0 ]]; then
  if cmake -S "$repo_root" -B "$tmp_dir/cmake-invalid" -G Ninja \
      -D LONEJSON_VERSION_OVERRIDE=bad -D LONEJSON_BUILD_TESTS=OFF \
      >"$tmp_dir/cmake-invalid.log" 2>&1; then
    printf 'CMake accepted invalid version override\n' >&2; exit 1
  fi
  grep -F 'invalid LONEJSON_VERSION_OVERRIDE' "$tmp_dir/cmake-invalid.log" >/dev/null
else
  cmake -S "$repo_root" -B "$tmp_dir/cmake-invalid" -G Ninja \
    -D LONEJSON_VERSION_OVERRIDE=bad -D LONEJSON_BUILD_TESTS=OFF \
    >"$tmp_dir/cmake-invalid.log" 2>&1
  grep -qx "CMAKE_PROJECT_VERSION:STATIC=$expected_default" "$tmp_dir/cmake-invalid/CMakeCache.txt"
fi
