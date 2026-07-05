#!/usr/bin/env bash
set -euo pipefail

host_name=${1:-}

if [[ -z "$host_name" ]]; then
  host_name=$(uname -n)
fi

hash_output() {
  local value=$1

  value=$(printf '%s' "$value" | tr '[:upper:]' '[:lower:]')
  if [[ "$value" =~ ^[0-9a-f]{32}$ ]]; then
    printf '%s\n' "$value"
    return 0
  fi
  return 1
}

if command -v md5sum >/dev/null 2>&1; then
  hash=$(printf '%s\n' "$host_name" | md5sum 2>/dev/null | awk '{print $1}' ||
    true)
  if hash_output "$hash"; then
    exit 0
  fi
fi

if command -v md5 >/dev/null 2>&1; then
  hash=$(printf '%s\n' "$host_name" | md5 -q 2>/dev/null || true)
  if hash_output "$hash"; then
    exit 0
  fi
fi

if command -v openssl >/dev/null 2>&1; then
  hash=$(printf '%s\n' "$host_name" | openssl dgst -md5 -r 2>/dev/null |
    awk '{print $1}')
  if hash_output "$hash"; then
    exit 0
  fi
fi

printf 'no supported md5 command found for benchmark host id\n' >&2
exit 1
