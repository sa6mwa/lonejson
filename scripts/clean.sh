#!/usr/bin/env bash

set -eu

mode="all"
root_dir=""

usage() {
    printf 'usage: %s [--dist-only] [--root DIR]\n' "$0" >&2
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --dist-only)
            mode="dist"
            shift
            ;;
        --root)
            if [ "$#" -lt 2 ]; then
                usage
                exit 1
            fi
            root_dir="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            usage
            exit 1
            ;;
    esac
done

if [ -z "$root_dir" ]; then
    root_dir="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
else
    root_dir="$(CDPATH= cd -- "$root_dir" && pwd)"
fi

if [ "$root_dir" = "/" ]; then
    printf 'clean.sh: refusing to clean /\n' >&2
    exit 1
fi

remove_path() {
    target_path="$1"
    if [ -e "$target_path" ]; then
        rm -rf -- "$target_path"
    fi
}

if [ "$mode" = "all" ]; then
    remove_path "$root_dir/build"
    remove_path "$root_dir/.deps"
    remove_path "$root_dir/examples/bin"
    remove_path "$root_dir/lonejson"
fi

remove_path "$root_dir/dist"
