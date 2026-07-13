#!/usr/bin/env bash
set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
keep_services="${LONEJSON_E2E_KEEP_DEVSERVICES:-0}"
started_services=0

cleanup() {
  if [[ "$started_services" = 1 && "$keep_services" != 1 ]]; then
    "$repo_root/scripts/dev-down.sh" >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT

cd "$repo_root"

if [[ "$keep_services" != 1 ]]; then
  "$repo_root/scripts/dev-up.sh"
  started_services=1
fi

"$repo_root/scripts/time_step.sh" e2e/curl make --no-print-directory LONEJSON_E2E_SERVICES_READY=1 test-curl-e2e
"$repo_root/scripts/time_step.sh" e2e/oidc make --no-print-directory LONEJSON_E2E_SERVICES_READY=1 test-oidc-e2e
"$repo_root/scripts/time_step.sh" e2e/m2m make --no-print-directory test-m2m-e2e
