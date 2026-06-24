#!/usr/bin/env bash
set -eu

script_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
root_dir="$(CDPATH= cd -- "$script_dir/.." && pwd)"
version_file="$root_dir/VERSION"

git_top="$(git -C "$root_dir" rev-parse --show-toplevel 2>/dev/null || true)"
if [ -n "$git_top" ]; then
    git_top="$(CDPATH= cd -- "$git_top" && pwd)"
fi

if [ "$git_top" = "$root_dir" ]; then
    tag="$(git -C "$root_dir" describe --tags --exact-match 2>/dev/null || true)"
    case "$tag" in
      v[0-9]*.[0-9]*.[0-9]*)
        printf '%s\n' "${tag#v}"
        ;;
      *)
        printf '%s\n' "0.0.0"
        ;;
    esac
    exit 0
fi

if [ ! -f "$version_file" ]; then
    printf 'release_version.sh: non-git source tree is missing VERSION\n' >&2
    exit 1
fi

version="$(tr -d '\r\n' < "$version_file")"
if [[ "$version" =~ ^[0-9]+[.][0-9]+[.][0-9]+$ ]]; then
    printf '%s\n' "$version"
else
    printf 'release_version.sh: invalid VERSION value: %s\n' "$version" >&2
    exit 1
fi
