#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

dist_dir="$tmp_dir/dist"
mkdir -p "$dist_dir"

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
