#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
makefile="$repo_root/Makefile"

require_text() {
  local text=$1
  if ! grep -F -- "$text" "$makefile" >/dev/null; then
    printf 'missing lifecycle Make surface: %s\n' "$text" >&2
    exit 1
  fi
}

require_text 'make lua-env                Print shell exports for using the repo-local Lua rock and debug C library.'
require_text 'make test-e2e               Run all deterministic local e2e gates serially.'
require_text 'make dev-ps                 Alias for make compose-ps.'
require_text 'make compose-ps             Show the local compose stack status.'

require_text 'lua-env: lua-rock'
require_text '@$(LUAROCKS) path --tree "$(LUA_ROCK_TREE)"'
require_text 'dev-ps: compose-ps'
require_text 'compose-ps:'
require_text '$(COMPOSE) -f docker-compose.yml ps'
require_text 'test-e2e:'
require_text '+$(TIME_STEP) e2e/curl $(MAKE) test-curl-e2e'
require_text '+$(TIME_STEP) e2e/oidc $(MAKE) test-oidc-e2e'
require_text '+$(TIME_STEP) e2e/m2m $(MAKE) test-m2m-e2e'
require_text 'fuzz: deps-host toolchains-aflpp'

grep -F 'NAMES lua lua5.5 lua5.4 lua5.3 luajit' "$repo_root/CMakeLists.txt" >/dev/null && {
  printf 'obsolete Lua runtime discovery remains in CMakeLists.txt\n' >&2
  exit 1
}
grep -F 'Lua 5\\.5\\.' "$repo_root/CMakeLists.txt" >/dev/null
