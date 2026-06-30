#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
server_port=${LONEJSON_M2M_E2E_SERVER_PORT:-18082}
secrets_file="$repo_root/build/host-curl/m2m-fixture-secrets.json"
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
    printf 'missing required command for M2M e2e: %s\n' "$1" >&2
    exit 77
  fi
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

if [[ ! -x "$repo_root/build/host-curl/lonejson_m2m_fixture_server" ]]; then
  printf 'missing build/host-curl/lonejson_m2m_fixture_server; run make test-m2m-e2e\n' >&2
  exit 1
fi

rm -f "$secrets_file"
"$repo_root/build/host-curl/lonejson_m2m_fixture_server" \
  "$server_port" "$secrets_file" 5 >"$repo_root/build/host-curl/m2m-fixture-server.log" 2>&1 &
server_pid=$!

for _ in $(seq 1 90); do
  if [[ -s "$secrets_file" ]] && curl -fsS --max-time 2 "http://127.0.0.1:$server_port/health" >/dev/null 2>&1; then
    break
  fi
  sleep 1
done
if [[ ! -s "$secrets_file" ]]; then
  printf 'timed out waiting for M2M fixture secrets\n' >&2
  exit 1
fi

client_id="$(json_field client_id <"$secrets_file")"
client_secret="$(json_field client_secret <"$secrets_file")"
api_key="$(json_field api_key <"$secrets_file")"

if curl -fsS --max-time 5 "http://127.0.0.1:$server_port/protected" >/dev/null 2>&1; then
  printf 'protected endpoint accepted missing credentials\n' >&2
  exit 1
fi

curl -fsS --max-time 5 -u "$client_id:$client_secret" \
  "http://127.0.0.1:$server_port/protected" \
  | python3 -c '
import json
import sys
value = json.load(sys.stdin)
if value.get("ok") is not True or value.get("authorized") is not True:
    raise SystemExit("Basic credentials were not authorized")
'

curl -fsS --max-time 5 \
  -H "Authorization: Bearer $api_key" \
  "http://127.0.0.1:$server_port/protected" \
  | python3 -c '
import json
import sys
value = json.load(sys.stdin)
if value.get("ok") is not True or value.get("authorized") is not True:
    raise SystemExit("Bearer API key was not authorized")
'

if curl -fsS --max-time 5 -u "$client_id:wrong-secret" \
  "http://127.0.0.1:$server_port/protected" >/dev/null 2>&1; then
  printf 'protected endpoint accepted wrong client secret\n' >&2
  exit 1
fi

wait "$server_pid"
server_pid=

printf '%s\n' 'M2M credential e2e passed'
