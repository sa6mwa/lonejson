#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
case "$("${UNAME:-uname}" -s)" in
  Darwin)
    [[ "$("${UNAME:-uname}" -m)" == arm64 ]] || { printf 'native Darwin builds require Apple Silicon (arm64)\n' >&2; exit 1; }
    printf '%s\n' arm64-apple-darwin
    ;;
  Linux) exec "$repo_root/scripts/detect_native_bootlin_target.sh" ;;
  *) printf 'unsupported native build host\n' >&2; exit 1 ;;
esac
