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
require_text 'make dev-ps                 Show the local compose-backed e2e service status.'
require_text 'make compose-ps             Compatibility alias for make dev-ps.'

require_text 'lua-env: lua-rock'
require_text '@$(LUAROCKS) path --tree "$(LUA_ROCK_TREE)"'
require_text 'dev-ps:'
require_text 'compose-ps:'
require_text './scripts/dev-ps.sh'
require_text 'test-e2e:'
require_text './scripts/test-e2e.sh'
require_text 'fuzz: deps-host toolchains-aflpp'

for script in compose dev-up dev-down dev-reset dev-ps dev-logs test-e2e; do
  if [[ ! -x "$repo_root/scripts/$script.sh" ]]; then
    printf 'missing executable lifecycle script: scripts/%s.sh\n' "$script" >&2
    exit 1
  fi
done

[[ -f "$repo_root/docker-compose.yaml" ]] || {
  printf 'missing lifecycle compose file: docker-compose.yaml\n' >&2
  exit 1
}
[[ ! -f "$repo_root/docker-compose.yml" ]] || {
  printf 'obsolete compose file name remains: docker-compose.yml\n' >&2
  exit 1
}

grep -F 'NAMES lua lua5.5 lua5.4 lua5.3 luajit' "$repo_root/CMakeLists.txt" >/dev/null && {
  printf 'obsolete Lua runtime discovery remains in CMakeLists.txt\n' >&2
  exit 1
}
grep -F 'Lua 5\\.5\\.' "$repo_root/CMakeLists.txt" >/dev/null
