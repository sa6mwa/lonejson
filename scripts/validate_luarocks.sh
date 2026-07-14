#!/usr/bin/env bash
set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
exec "$repo_root/scripts/smoke_lua_src_rock.sh" "$@"
