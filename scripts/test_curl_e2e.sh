#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
nginx_https_port=${LONEJSON_NGINX_HTTPS_E2E_PORT:-8443}
ca_file=${LONEJSON_CURL_E2E_CAINFO:-$repo_root/devenv/volumes/nginx/certs/server.crt}

"${repo_root}/scripts/build_curl_examples.sh"

LONEJSON_CURL_E2E_CAINFO="$ca_file" \
LONEJSON_CURL_E2E_GET_URL="https://localhost:${nginx_https_port}/variants/ingest-target.json" \
  "${repo_root}/examples/bin/curl_get"
LONEJSON_CURL_E2E_CAINFO="$ca_file" \
LONEJSON_CURL_E2E_PUT_URL="https://localhost:${nginx_https_port}/ingest" \
  "${repo_root}/examples/bin/curl_put"
