#!/usr/bin/env bash
set -euo pipefail

repo_root=$1

if rg -n '\blonejson__' "$repo_root/src/lua"; then
  printf '%s\n' \
    'Lua binding must stay on the public lonejson API boundary; found lonejson__ internal helper use.' >&2
  exit 1
fi
