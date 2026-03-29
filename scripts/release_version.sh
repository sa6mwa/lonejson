#!/usr/bin/env bash
set -eu

tag="$(git describe --tags --exact-match 2>/dev/null || true)"

case "$tag" in
  v[0-9]*.[0-9]*.[0-9]*)
    printf '%s\n' "${tag#v}"
    ;;
  *)
    printf '%s\n' "0.0.0"
    ;;
esac
