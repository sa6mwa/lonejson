#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
tmp_dir=$(mktemp -d)
temp_exact_tag=""

# This test intentionally mutates the checkout under test. Release version
# discovery must be proven against the real repository CMake sees during a
# release, not against a copied fixture with different git metadata. The
# impossible version below is reserved as a lightweight test-only tag; a stale
# copy means a previous interrupted test left cleanup work behind.
release_test_tag="v99.99.99"
release_test_version="${release_test_tag#v}"
delete_test_tag() {
  local tag_name=$1
  if git -C "$repo_root" rev-parse --verify "refs/tags/$tag_name" \
    >/dev/null 2>&1; then
    git -C "$repo_root" tag -d "$tag_name" >/dev/null
  fi
}
create_test_tag() {
  local tag_name=$1
  delete_test_tag "$tag_name"
  git -C "$repo_root" tag "$tag_name"
  temp_exact_tag="$tag_name"
}
cleanup() {
  if [[ -n "$temp_exact_tag" ]]; then
    delete_test_tag "$temp_exact_tag" || true
  fi
  rm -rf "$tmp_dir"
}
trap cleanup EXIT HUP INT TERM

# Remove any stale copy of the reserved test-only tag before the first
# untagged assertion, then recreate and delete it only inside the tagged block.
delete_test_tag "$release_test_tag"

default_version=$(env -u LONEJSON_VERSION_OVERRIDE \
  "$repo_root/scripts/release_version.sh")
exact_tag=$(git -C "$repo_root" describe --tags --exact-match 2>/dev/null || true)
case "$exact_tag" in
  v[0-9]*.[0-9]*.[0-9]*)
    expected_default="${exact_tag#v}"
    ;;
  *)
    expected_default="0.0.0"
    ;;
esac
if [[ "$default_version" != "$expected_default" ]]; then
  printf 'git worktree resolved unexpected version: got %s expected %s\n' \
    "$default_version" "$expected_default" >&2
  exit 1
fi

override_version=$(LONEJSON_VERSION_OVERRIDE=7.8.9 \
  "$repo_root/scripts/release_version.sh")
if [[ -n "$exact_tag" ]]; then
  if [[ "$override_version" != "$expected_default" ]]; then
    printf 'tagged git worktree did not prefer exact tag over override: got %s expected %s\n' \
      "$override_version" "$expected_default" >&2
    exit 1
  fi
else
  if [[ "$override_version" != "7.8.9" ]]; then
    printf 'untagged git worktree did not honor version override: got %s\n' \
      "$override_version" >&2
    exit 1
  fi

  temp_exact_tag="$release_test_tag"
  create_test_tag "$temp_exact_tag"
  env -u LONEJSON_VERSION_OVERRIDE "$repo_root/scripts/release_version.sh" |
    grep -qx "$release_test_version"
  LONEJSON_VERSION_OVERRIDE=7.8.9 "$repo_root/scripts/release_version.sh" |
    grep -qx "$release_test_version"
  env -u LONEJSON_VERSION_OVERRIDE cmake \
    -S "$repo_root" -B "$tmp_dir/cmake-temporary-tag-version" -G Ninja \
    -D LONEJSON_BUILD_TESTS=OFF \
    -D LONEJSON_BUILD_EXAMPLES=OFF \
    >"$tmp_dir/cmake-temporary-tag-version.out" \
    2>"$tmp_dir/cmake-temporary-tag-version.err"
  grep -qx "CMAKE_PROJECT_VERSION:STATIC=$release_test_version" \
    "$tmp_dir/cmake-temporary-tag-version/CMakeCache.txt"
  delete_test_tag "$temp_exact_tag"
  temp_exact_tag=""
fi

if [[ -n "$exact_tag" ]]; then
  LONEJSON_VERSION_OVERRIDE=7.8.9-rc.1 \
    "$repo_root/scripts/release_version.sh" |
    grep -qx "$expected_default"
else
  set +e
  LONEJSON_VERSION_OVERRIDE=7.8.9-rc.1 "$repo_root/scripts/release_version.sh" \
    >"$tmp_dir/invalid.out" 2>"$tmp_dir/invalid.err"
  invalid_status=$?
  set -e
  if [[ "$invalid_status" -eq 0 ]]; then
    printf 'invalid LONEJSON_VERSION_OVERRIDE was accepted\n' >&2
    exit 1
  fi
  grep -F 'invalid LONEJSON_VERSION_OVERRIDE value' \
    "$tmp_dir/invalid.err" >/dev/null
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

if [[ -n "$exact_tag" ]]; then
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
