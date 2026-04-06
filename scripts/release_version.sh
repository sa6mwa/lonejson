#!/usr/bin/env bash
set -eu

script_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
root_dir="$(CDPATH= cd -- "$script_dir/.." && pwd)"
version_file="$root_dir/VERSION"

if [ -f "$version_file" ]; then
    version="$(tr -d '\r\n' < "$version_file")"
    case "$version" in
        [0-9]*.[0-9]*.[0-9]*)
            printf '%s\n' "$version"
            exit 0
            ;;
    esac
fi

tag="$(git describe --tags --exact-match 2>/dev/null || true)"

case "$tag" in
  v[0-9]*.[0-9]*.[0-9]*)
    printf '%s\n' "${tag#v}"
    ;;
  *)
    printf '%s\n' "0.0.0"
    ;;
esac
