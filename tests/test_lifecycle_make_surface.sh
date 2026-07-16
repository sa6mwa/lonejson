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
require_text 'make test-e2e               Run all deterministic local e2e gates serially; set LONEJSON_*_E2E_PORT to avoid host-port conflicts.'
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
require_text 'make toolchains-all         Install pinned Bootlin Linux toolchains in the shared lifecycle cache.'
require_text 'make toolchains-x86_64-linux-gnu Install the pinned x86_64 glibc Bootlin collection.'
require_text 'make release-lua-artifacts  Build the standalone Lua source package, release rockspec, and source rock in dist/.'
require_text 'make clean                  Remove build/, dist/, .cache/, devenv/volumes/, examples/bin/, and generated Lua module artifacts; preserve shared caches.'
require_text 'make cross-build            Configure and build every supported Linux cross-release preset.'
require_text 'make cross-test             Standard alias for make test-cross.'
require_text 'make package-single-header  Build the version-stamped standalone-header artifact in dist/.'
require_text 'cross-build: deps-cross'
require_text 'cross-test: deps-cross'
require_text 'test-cross: cross-test'
require_text './scripts/build.sh $(DEBUG_PRESET) --stage-examples'
require_text './scripts/test.sh $(DEBUG_PRESET)'
require_text './scripts/host_test.sh'
require_text './scripts/cross_build.sh $(CROSS_RELEASE_PRESETS)'
require_text './scripts/cross_test.sh "$(HOST_POLICY_CTEST_EXCLUDE)" $(CROSS_RELEASE_PRESETS)'
require_text 'target_id="$$(./scripts/detect_native_bootlin_target.sh)" && ./scripts/deps.sh "$$target_id"'
require_text './scripts/detect_native_bootlin_target.sh'
require_text './scripts/package.sh'
require_text './scripts/package-verify.sh'
require_text 'verify-release-privacy: package-verify'
require_text './scripts/validate_luarocks.sh'
require_text './scripts/validate_luarocks.sh "$(RELEASE_ROCK)" "$(LONEJSON_LUA_LIBDIR)" "$(LUA)" "$(LUAROCKS)"'
require_text './scripts/run_linux_release_matrix.sh'
require_text './scripts/fuzz.sh'

if grep -F './scripts/run_release_matrix.sh' "$makefile" >/dev/null; then
  printf 'release-matrix must route through scripts/run_linux_release_matrix.sh\n' >&2
  exit 1
fi
if grep -F './scripts/run_afl_fuzz.sh' "$makefile" >/dev/null; then
  printf 'fuzz must route through scripts/fuzz.sh\n' >&2
  exit 1
fi

help_text=$(make -C "$repo_root" help)
while IFS= read -r target; do
  [[ "$target" == help ]] && continue
  grep -F "make $target" <<<"$help_text" >/dev/null || {
    printf 'phony lifecycle target is missing from make help: %s\n' "$target" >&2
    exit 1
  }
done < <(awk '
  /^\.PHONY:/ { collecting = 1; next }
  collecting {
    line = $0
    sub(/^[[:space:]]*/, "", line)
    sub(/[[:space:]]*\\$/, "", line)
    if (line == "") next
    count = split(line, targets, /[[:space:]]+/)
    for (target_index = 1; target_index <= count; ++target_index) if (targets[target_index] != "") print targets[target_index]
    if ($0 !~ /\\$/) exit
  }
' "$makefile")

for script in \
  deps build test host_test cross_build cross_test fuzz package package-verify \
  run_timed osxcross_available verify_release_privacy validate_luarocks run_linux_release_matrix \
  compose dev-up dev-down dev-reset dev-ps dev-logs test-e2e; do
  if [[ ! -x "$repo_root/scripts/$script.sh" ]]; then
    printf 'missing executable lifecycle script: scripts/%s.sh\n' "$script" >&2
    exit 1
  fi
done

grep -F 'exec "$repo_root/scripts/time_step.sh"' "$repo_root/scripts/run_timed.sh" >/dev/null
grep -F 'exec "$repo_root/scripts/fuzz.sh"' "$repo_root/scripts/run_afl_fuzz.sh" >/dev/null
grep -F 'verify_release_artifacts.sh' "$repo_root/scripts/package-verify.sh" >/dev/null
grep -F 'verify_release_archives.sh' "$repo_root/scripts/package-verify.sh" >/dev/null
grep -F 'verify_release_artifacts.sh' "$repo_root/scripts/verify_release_privacy.sh" >/dev/null
grep -F 'cpkt-toolchains.sh" discover arm64-apple-darwin' "$repo_root/scripts/osxcross_available.sh" >/dev/null
grep -F 'exec "${repo_root}/scripts/run_release_matrix.sh"' "$repo_root/scripts/run_linux_release_matrix.sh" >/dev/null
grep -Fx 'set -euo pipefail' "$repo_root/scripts/run_linux_release_matrix.sh" >/dev/null
grep -Fx 'set -euo pipefail' "$repo_root/scripts/run_release_matrix.sh" >/dev/null
grep -Fx 'set -euo pipefail' "$repo_root/scripts/smoke_darwin_release.sh" >/dev/null

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
grep -F 'scripts/resolve_lua55.sh' "$repo_root/CMakeLists.txt" >/dev/null
grep -F 'LUA ?= $(shell ./scripts/resolve_lua55.sh 2>/dev/null)' "$repo_root/Makefile" >/dev/null
grep -F 'LUA="$(LUA)" ./scripts/dev-up.sh' "$repo_root/Makefile" >/dev/null
grep -F 'Lua 5.5 executable' "$repo_root/scripts/resolve_lua55.sh" >/dev/null

grep -F 'name: ${LONEJSON_COMPOSE_PROJECT_NAME:-lonejson-e2e}' \
  "$repo_root/docker-compose.yaml" >/dev/null
grep -F 'LONEJSON_NGINX_HTTPS_E2E_PORT' "$repo_root/docker-compose.yaml" >/dev/null
grep -F './devenv/volumes/nginx/generated' "$repo_root/docker-compose.yaml" >/dev/null
grep -F 'LONEJSON_COMPOSE_PROJECT_NAME' "$repo_root/scripts/compose.sh" >/dev/null
grep -F 'service-readiness' "$repo_root/scripts/dev-up.sh" >/dev/null
grep -F 'compose state and recent logs follow' "$repo_root/scripts/test-e2e.sh" >/dev/null
