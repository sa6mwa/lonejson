#!/usr/bin/env bash

set -euo pipefail

if [ "$#" -ne 3 ]; then
    printf 'usage: %s <lua> <generator.lua> <output-dir>\n' "$0" >&2
    exit 1
fi

lua_bin="$1"
generator="$2"
output_dir="$3"
lock_dir="${output_dir}.lock"

mkdir -p "$(dirname -- "$output_dir")"

prune_legacy_fixture_dirs() {
    local build_root legacy_dir

    build_root="$(dirname -- "$(dirname -- "$output_dir")")"
    if [ ! -d "$build_root" ]; then
        return
    fi
    for legacy_dir in "$build_root"/*/generated/fixtures; do
        if [ ! -d "$legacy_dir" ]; then
            continue
        fi
        if [ "$legacy_dir" = "$output_dir" ]; then
            continue
        fi
        rm -rf -- "$legacy_dir"
    done
}

lock_acquired=0
cleanup() {
    if [ "$lock_acquired" -eq 1 ]; then
        rmdir -- "$lock_dir"
    fi
}
trap cleanup EXIT INT TERM HUP

while ! mkdir -- "$lock_dir" 2>/dev/null; do
    sleep 0.1
done
lock_acquired=1

"$lua_bin" "$generator" "$output_dir"
prune_legacy_fixture_dirs
