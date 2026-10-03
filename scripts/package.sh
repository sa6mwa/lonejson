#!/usr/bin/env bash
set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
[[ $# -eq 0 ]] || { printf 'usage: %s\n' "$0" >&2; exit 2; }
for target in release-matrix package-source package-checksums package-verify; do
  make -C "$repo_root" "$target"
done
