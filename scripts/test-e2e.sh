#!/usr/bin/env bash
set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
keep_services="${LONEJSON_E2E_KEEP_DEVSERVICES:-0}"
started_services=0

cleanup() {
  status=$?
  if [[ "$status" -ne 0 ]]; then
    printf '%s\n' 'lonejson e2e failed; compose state and recent logs follow.' >&2
    "$repo_root/scripts/dev-ps.sh" >&2 || true
    "$repo_root/scripts/compose.sh" logs --tail 120 >&2 || true
  fi
  if [[ "$started_services" = 1 && "$keep_services" != 1 ]]; then
    "$repo_root/scripts/dev-down.sh" >/dev/null 2>&1 || true
  fi
  exit "$status"
}
trap cleanup EXIT

cd "$repo_root"

if [[ "$keep_services" != 1 ]]; then
  started_services=1
  "$repo_root/scripts/dev-up.sh"
fi

"$repo_root/scripts/time_step.sh" e2e/curl make --no-print-directory LONEJSON_E2E_SERVICES_READY=1 test-curl-e2e
"$repo_root/scripts/time_step.sh" e2e/oidc make --no-print-directory LONEJSON_E2E_SERVICES_READY=1 test-oidc-e2e
"$repo_root/scripts/time_step.sh" e2e/m2m make --no-print-directory test-m2m-e2e
