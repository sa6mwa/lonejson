#!/usr/bin/env bash
set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
checksums=${1:-}
if [[ -n "$checksums" ]]; then
  "$repo_root/scripts/verify_release_artifacts.sh" "$repo_root" "$checksums"
  exec "$repo_root/scripts/verify_release_archives.sh" "$repo_root" "$checksums"
fi
"$repo_root/scripts/verify_release_artifacts.sh" "$repo_root"
exec "$repo_root/scripts/verify_release_archives.sh" "$repo_root"
