#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
issuer=${LONEJSON_OIDC_E2E_ISSUER:-https://localhost:18443/default}
audience=${LONEJSON_OIDC_E2E_AUDIENCE:-lonejson-api}
server_port=${LONEJSON_OIDC_E2E_SERVER_PORT:-18081}
ca_file=${LONEJSON_OIDC_E2E_CAINFO:-$repo_root/docker/nginx/certs/server.crt}
discovery_url=${LONEJSON_OIDC_E2E_DISCOVERY_URL:-https://localhost:18443/.well-known/openid-configuration/default}
server_pid=

cleanup() {
  if [[ -n "${server_pid}" ]] && kill -0 "$server_pid" >/dev/null 2>&1; then
    kill "$server_pid" >/dev/null 2>&1 || true
    wait "$server_pid" >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT

require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    printf 'missing required command for OIDC e2e: %s\n' "$1" >&2
    exit 77
  fi
}

wait_url() {
  local url=$1
  local i
  for i in $(seq 1 90); do
    if curl -fsS --max-time 3 --cacert "$ca_file" "$url" >/dev/null 2>&1; then
      return 0
    fi
    sleep 1
  done
  printf 'timed out waiting for %s\n' "$url" >&2
  return 1
}

json_field() {
  local field=$1
  python3 -c '
import json
import sys
field = sys.argv[1]
value = json.load(sys.stdin).get(field)
if value is None:
    sys.exit(2)
print(value)
' "$field"
}

require_command curl
require_command python3

if [[ ! -x "$repo_root/build/host-curl/lonejson_oidc_fixture_server" ]]; then
  printf 'missing build/host-curl/lonejson_oidc_fixture_server; run make test-oidc-e2e\n' >&2
  exit 1
fi

wait_url "$discovery_url"
discovery_json="$(curl -fsS --max-time 10 --cacert "$ca_file" "$discovery_url")"
token_endpoint="$(printf '%s' "$discovery_json" | json_field token_endpoint)"

token_json="$(
  curl -fsS --max-time 10 --cacert "$ca_file" -X POST "$token_endpoint" \
    -H 'Content-Type: application/x-www-form-urlencoded' \
    --data-urlencode 'grant_type=client_credentials' \
    --data-urlencode 'client_id=lonejson-m2m' \
    --data-urlencode 'client_secret=lonejson-secret' \
    --data-urlencode 'scope=openid profile' \
    --data-urlencode "audience=$audience"
)"
access_token="$(printf '%s' "$token_json" | json_field access_token)"

refresh_token="$(
  printf '%s' "$token_json" | python3 -c '
import json
import sys
value = json.load(sys.stdin).get("refresh_token")
if value:
    print(value)
'
)"
if [[ -n "$refresh_token" ]]; then
  curl -fsS --max-time 10 --cacert "$ca_file" -X POST "$token_endpoint" \
    -H 'Content-Type: application/x-www-form-urlencoded' \
    --data-urlencode 'grant_type=refresh_token' \
    --data-urlencode "refresh_token=$refresh_token" \
    --data-urlencode 'client_id=lonejson-m2m' \
    --data-urlencode 'client_secret=lonejson-secret' \
    >/dev/null
else
  printf '%s\n' \
    'OIDC e2e note: provider did not issue a refresh_token for client_credentials; refresh-token request remains covered by unit/provider tests.' >&2
fi

LONEJSON_OIDC_E2E_CAINFO="$ca_file" \
  "$repo_root/build/host-curl/lonejson_oidc_fixture_server" \
  "$issuer" "$audience" "$server_port" 3 >"$repo_root/build/host-curl/oidc-fixture-server.log" 2>&1 &
server_pid=$!

ready=0
for _ in $(seq 1 90); do
  if curl -fsS --max-time 2 "http://127.0.0.1:$server_port/health" >/dev/null 2>&1; then
    ready=1
    break
  fi
  sleep 1
done
if [[ "$ready" != 1 ]]; then
  printf 'timed out waiting for http://127.0.0.1:%s/health\n' "$server_port" >&2
  exit 1
fi

if curl -fsS --max-time 5 "http://127.0.0.1:$server_port/protected" >/dev/null 2>&1; then
  printf 'protected endpoint accepted missing bearer token\n' >&2
  exit 1
fi

curl -fsS --max-time 5 "http://127.0.0.1:$server_port/protected" \
  -H "Authorization: Bearer $access_token" \
  | python3 -c '
import json
import sys
value = json.load(sys.stdin)
if value.get("ok") is not True or value.get("authorized") is not True:
    raise SystemExit("lonejson fixture server did not authorize token")
'

wait "$server_pid"
server_pid=

curl -fsS --max-time 5 "http://127.0.0.1:18080/health" >/dev/null
curl -fsS --max-time 5 "http://127.0.0.1:18080/protected" \
  -H "Authorization: Bearer $access_token" >/dev/null

printf '%s\n' 'OIDC/OAuth2 e2e passed'
