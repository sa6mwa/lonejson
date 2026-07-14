#!/usr/bin/env bash
set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
lua_bin="${LUA:-$repo_root/scripts/resolve_lua55.sh}"
if [[ "$lua_bin" == "$repo_root/scripts/resolve_lua55.sh" ]]; then
  lua_bin="$($lua_bin)"
fi
generated_fixture_dir="$repo_root/build/generated/fixtures"
state_root="$repo_root/devenv/volumes"
nginx_https_port="${LONEJSON_NGINX_HTTPS_E2E_PORT:-8443}"
oidc_port="${LONEJSON_OIDC_E2E_PORT:-18443}"
api_fixture_port="${LONEJSON_API_FIXTURE_E2E_PORT:-18080}"
ca_file="$state_root/nginx/certs/server.crt"

wait_url() {
  local url=$1
  local attempts=0
  while [[ "$attempts" -lt 90 ]]; do
    if curl -fsS --max-time 3 --cacert "$ca_file" "$url" >/dev/null 2>&1; then
      return 0
    fi
    attempts=$((attempts + 1))
    sleep 1
  done
  return 1
}

wait_http_url() {
  local url=$1
  local attempts=0
  while [[ "$attempts" -lt 90 ]]; do
    if curl -fsS --max-time 3 "$url" >/dev/null 2>&1; then
      return 0
    fi
    attempts=$((attempts + 1))
    sleep 1
  done
  return 1
}

diagnose_failure() {
  printf '%s\n' 'lonejson e2e readiness failed; compose state follows.' >&2
  "$repo_root/scripts/dev-ps.sh" >&2 || true
  "$repo_root/scripts/compose.sh" logs --tail 120 >&2 || true
  printf '%s\n' 'PKT_DIAGNOSTIC_BEGIN' >&2
  printf '%s\n' 'surface=dev-up' >&2
  printf '%s\n' 'phase=service-readiness' >&2
  printf '%s\n' 'status=failed' >&2
  printf '%s\n' 'class=e2e-service' >&2
  printf '%s\n' 'reason=local-compose-service-not-ready' >&2
  printf '%s\n' 'next=run make dev-logs or override LONEJSON_*_E2E_PORT to avoid a host-port conflict' >&2
  printf '%s\n' 'PKT_DIAGNOSTIC_END' >&2
}

cd "$repo_root"
command -v curl >/dev/null 2>&1 || {
  printf '%s\n' 'dev-up requires curl for deterministic service readiness checks' >&2
  exit 1
}
mkdir -p "$state_root/nginx/generated" "$state_root/nginx/certs"
"$repo_root/scripts/ensure_test_certs.sh"
"$repo_root/scripts/ensure_large_fixtures.sh" "$lua_bin" "$repo_root/scripts/generate_large_fixtures.lua" "$generated_fixture_dir"
"$lua_bin" "$repo_root/scripts/generate_large_fixtures.lua" "$state_root/nginx/generated/variants"
"$repo_root/scripts/compose.sh" up -d --build --force-recreate

if ! wait_url "https://localhost:${nginx_https_port}/variants/ingest-target.json" ||
   ! wait_url "https://localhost:${oidc_port}/.well-known/openid-configuration/default" ||
   ! wait_http_url "http://127.0.0.1:${api_fixture_port}/health"; then
  diagnose_failure
  exit 1
fi

printf '%s\n' 'lonejson e2e services are up.'
printf '  compose file: %s\n' "$repo_root/docker-compose.yaml"
printf '  HTTPS fixture: https://localhost:%s/\n' "$nginx_https_port"
printf '  OIDC issuer: https://localhost:%s/default\n' "$oidc_port"
printf '  API fixture: http://127.0.0.1:%s/health\n' "$api_fixture_port"
printf '  generated state: %s\n' "$state_root"
