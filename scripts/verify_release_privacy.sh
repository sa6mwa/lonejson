#!/usr/bin/env bash
set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
exec "$repo_root/scripts/verify_release_artifacts.sh" "$repo_root" "$@"
