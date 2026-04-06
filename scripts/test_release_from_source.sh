#!/usr/bin/env bash

set -euo pipefail

usage() {
    printf 'usage: %s REPO_ROOT SOURCE_TARBALL\n' "$0" >&2
}

if [ "$#" -ne 2 ]; then
    usage
    exit 1
fi

repo_root="$1"
source_tarball="$2"
smoke_root="$repo_root/build/release-source-smoke"
run_root=""

repo_root="$(CDPATH= cd -- "$repo_root" && pwd)"

if [ ! -f "$source_tarball" ]; then
    printf 'test_release_from_source.sh: source tarball not found: %s\n' \
        "$source_tarball" >&2
    exit 1
fi

if [ "$repo_root" = "/" ]; then
    printf 'test_release_from_source.sh: refusing to use /\n' >&2
    exit 1
fi

cleanup() {
    if [ -n "$run_root" ] && [ -d "$run_root" ]; then
        rm -rf -- "$run_root"
    fi
}

trap cleanup EXIT INT TERM

rm -rf -- "$smoke_root"
mkdir -p "$smoke_root"
run_root="$(mktemp -d "$smoke_root/run.XXXXXX")"

tar -xzf "$source_tarball" -C "$run_root"

src_dir="$(find "$run_root" -mindepth 1 -maxdepth 1 -type d | head -n 1)"
if [ -z "$src_dir" ] || [ ! -d "$src_dir" ]; then
    printf 'test_release_from_source.sh: failed to locate extracted source tree\n' >&2
    exit 1
fi

make -C "$src_dir" release
