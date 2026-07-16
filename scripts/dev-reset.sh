#!/usr/bin/env bash
set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$repo_root"
"$repo_root/scripts/dev-down.sh"
cmake -E rm -rf "$repo_root/devenv/volumes"
