#!/usr/bin/env bash
set -euo pipefail

# Rationale: release uploads are selected from the checksum manifest, so stale
# or omitted release-looking artifacts must fail before publishing.

repo_root=$1
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

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
mkdir -p "$custom_dist_dir"
printf '%s\n' 'lonejson lifecycle artifact directory' >"$custom_dist_dir/.lonejson-dist"
printf '%s\n' custom >"$custom_dist_dir/lonejson-0.0.0.tar.gz"
cmake -S "$repo_root" -B "$custom_build_dir" -G Ninja \
  -D LONEJSON_DIST_DIR="$custom_dist_dir" \
  -D LONEJSON_BUILD_TESTS=OFF \
  -D LONEJSON_BUILD_EXAMPLES=OFF >/dev/null
cmake --build "$custom_build_dir" --target package-checksums >/dev/null
custom_manifest="$custom_dist_dir/lonejson-0.0.0-CHECKSUMS"
[[ -f "$custom_manifest" ]]
grep -F '  lonejson-0.0.0.tar.gz' "$custom_manifest" >/dev/null
