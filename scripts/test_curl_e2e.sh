#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

"${repo_root}/scripts/build_curl_examples.sh"

"${repo_root}/examples/bin/curl_get"
"${repo_root}/examples/bin/curl_put"
