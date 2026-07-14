#!/usr/bin/env bash
set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
description="$("$repo_root/scripts/cpkt-toolchains.sh" discover arm64-apple-darwin)"
grep -Fx 'status=ready' <<<"$description" >/dev/null || exit 1
printf '%s\n' "$description"
