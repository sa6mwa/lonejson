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

# Build the real-curl replay client through the pinned native toolchain.
python3 "$repo_root/tests/test_curl_rewind_fixture.py"
bundle_root="$("$repo_root/scripts/detect_c_pkt_systems_bundle.sh")"
cmake --preset host-curl -S "$repo_root" -D LONEJSON_C_PKT_SYSTEMS_ROOT="$bundle_root"
cmake --build "$repo_root/build/host-curl" --target lonejson_curl_rewind_e2e
"$repo_root/build/host-curl/lonejson_curl_rewind_e2e" \
  "https://localhost:${nginx_https_port}" "$ca_file"
