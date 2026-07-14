#!/usr/bin/env bash
set -euo pipefail

script_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
repo_root="${script_dir%/scripts}"
compose_file="$repo_root/docker-compose.yaml"

if [[ ! -f "$compose_file" ]]; then
  printf '%s\n' "missing compose file: $compose_file" >&2
  exit 1
fi

if [[ -z "${LONEJSON_COMPOSE_PROJECT_NAME:-}" ]]; then
  checkout_id="$(printf '%s' "$repo_root" | cksum | awk '{print $1}')"
  export LONEJSON_COMPOSE_PROJECT_NAME="lonejson-e2e-$checkout_id"
fi

if command -v nerdctl >/dev/null 2>&1; then
  exec nerdctl compose -f "$compose_file" "$@"
fi

if command -v docker >/dev/null 2>&1; then
  exec docker compose -f "$compose_file" "$@"
fi

printf '%s\n' "Neither nerdctl nor docker is available in PATH." >&2
exit 1
