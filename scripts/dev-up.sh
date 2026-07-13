#!/usr/bin/env bash
set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
lua_bin="${LUA:-lua}"
generated_fixture_dir="$repo_root/build/generated/fixtures"

cd "$repo_root"
"$repo_root/scripts/ensure_test_certs.sh"
"$repo_root/scripts/ensure_large_fixtures.sh" "$lua_bin" "$repo_root/scripts/generate_large_fixtures.lua" "$generated_fixture_dir"
"$lua_bin" "$repo_root/scripts/generate_large_fixtures.lua" "$repo_root/docker/nginx/generated/variants"
"$repo_root/scripts/compose.sh" up -d --build --force-recreate

printf '%s\n' 'lonejson e2e services are up.'
printf '  compose file: %s\n' "$repo_root/docker-compose.yaml"
printf '  HTTPS fixture: https://localhost:8443/\n'
printf '  OIDC issuer: https://localhost:18443/default\n'
printf '  generated state: %s\n' "$repo_root/docker/nginx/generated"
