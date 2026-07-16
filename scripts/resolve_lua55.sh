#!/usr/bin/env bash
set -euo pipefail

for candidate in lua5.5 lua; do
  if ! lua_bin=$(command -v "$candidate" 2>/dev/null); then
    continue
  fi
  if lua_version="$($lua_bin -v 2>&1)" && [[ "$lua_version" == *'Lua 5.5.'* ]]; then
    printf '%s\n' "$lua_bin"
    exit 0
  fi
done

printf '%s\n' 'lonejson requires a Lua 5.5 executable (tried lua5.5 and lua)' >&2
exit 1
