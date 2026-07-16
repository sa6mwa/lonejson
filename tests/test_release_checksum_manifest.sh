#!/usr/bin/env bash
set -euo pipefail

# Rationale: release uploads are selected from the checksum manifest, so stale
# or omitted release-looking artifacts must fail before publishing.

repo_root=$1
tmp_dir="$(mktemp -d)"
temp_exact_tag=""

# This test intentionally mutates the checkout under test. Checksum manifest
# generation must be proven against the same git metadata a real release build
# consumes. The impossible version below is reserved as a lightweight test-only
# tag; a stale copy means a previous interrupted test left cleanup work behind.
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

# Remove any stale copy of the reserved test-only tag before untagged manifest
# checks, then recreate and delete it only inside the tagged manifest block.
delete_test_tag "$release_test_tag"

dist_dir="$tmp_dir/dist"
mkdir -p "$dist_dir"
mkdir -p "$tmp_dir/cmake"
cp "$repo_root/cmake/lonejson_dist_dir.cmake" "$tmp_dir/cmake/"

for artifact in \
  lonejson-1.2.3.tar.gz \
  lonejson-1.2.3.h.gz \
  lonejson-lua-1.2.3.tar.gz \
  lonejson-1.2.3-1.rockspec \
  lonejson-1.2.3-1.src.rock \
  liblonejson-1.2.3-x86_64-linux-gnu.tar.gz \
  liblonejson-1.2.3-arm64-apple-darwin-smoke-test.zip; do
  printf '%s\n' "$artifact" >"$dist_dir/$artifact"
done

cmake -D "LONEJSON_ROOT=$tmp_dir" -D LONEJSON_VERSION=1.2.3 \
  -P "$repo_root/cmake/package_checksums.cmake"

manifest="$dist_dir/lonejson-1.2.3-CHECKSUMS"
if [[ ! -f "$manifest" ]]; then
  printf 'missing checksum manifest: %s\n' "$manifest" >&2
  exit 1
fi

for artifact in \
  lonejson-lua-1.2.3.tar.gz \
  liblonejson-1.2.3-arm64-apple-darwin-smoke-test.zip; do
  if ! grep -F "  $artifact" "$manifest" >/dev/null; then
    printf 'checksum manifest is missing release artifact: %s\n' "$artifact" >&2
    cat "$manifest" >&2
    exit 1
  fi
done

# The configured artifact directory must flow through the CMake target rather
# than silently falling back to the checkout's default dist/ directory.
custom_dist_dir="$tmp_dir/custom-dist"
custom_build_dir="$tmp_dir/custom-build"
default_version=$(env -u LONEJSON_VERSION_OVERRIDE \
  "$repo_root/scripts/release_version.sh")
mkdir -p "$custom_dist_dir"
printf '%s\n' 'lonejson lifecycle artifact directory' >"$custom_dist_dir/.lonejson-dist"
printf '%s\n' custom >"$custom_dist_dir/lonejson-${default_version}.tar.gz"
env -u LONEJSON_VERSION_OVERRIDE cmake \
  -S "$repo_root" -B "$custom_build_dir" -G Ninja \
  -D LONEJSON_DIST_DIR="$custom_dist_dir" \
  -D LONEJSON_BUILD_TESTS=OFF \
  -D LONEJSON_BUILD_EXAMPLES=OFF >/dev/null
cmake --build "$custom_build_dir" --target package-checksums >/dev/null
custom_manifest="$custom_dist_dir/lonejson-${default_version}-CHECKSUMS"
[[ -f "$custom_manifest" ]]
grep -F "  lonejson-${default_version}.tar.gz" "$custom_manifest" >/dev/null

if ! git -C "$repo_root" describe --tags --exact-match >/dev/null 2>&1; then
  temp_exact_tag="$release_test_tag"
  create_test_tag "$temp_exact_tag"
  tagged_dist_dir="$tmp_dir/tagged-dist"
  tagged_build_dir="$tmp_dir/tagged-build"
  mkdir -p "$tagged_dist_dir"
  printf '%s\n' 'lonejson lifecycle artifact directory' >"$tagged_dist_dir/.lonejson-dist"
  printf '%s\n' tagged >"$tagged_dist_dir/lonejson-${release_test_version}.tar.gz"
  env -u LONEJSON_VERSION_OVERRIDE cmake \
    -S "$repo_root" -B "$tagged_build_dir" -G Ninja \
    -D LONEJSON_DIST_DIR="$tagged_dist_dir" \
    -D LONEJSON_BUILD_TESTS=OFF \
    -D LONEJSON_BUILD_EXAMPLES=OFF >/dev/null
  cmake --build "$tagged_build_dir" --target package-checksums >/dev/null
  tagged_manifest="$tagged_dist_dir/lonejson-${release_test_version}-CHECKSUMS"
  [[ -f "$tagged_manifest" ]]
  grep -F "  lonejson-${release_test_version}.tar.gz" "$tagged_manifest" >/dev/null
  delete_test_tag "$temp_exact_tag"
  temp_exact_tag=""
fi
