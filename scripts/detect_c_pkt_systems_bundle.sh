#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

detect_target_id() {
  case "$("${UNAME:-uname}" -s)" in
    Darwin)
      case "$("${UNAME:-uname}" -m)" in
        arm64|aarch64) printf '%s' arm64-apple-darwin ;;
        *) printf '%s' unsupported ;;
      esac
      ;;
    Linux)
      "$repo_root/scripts/detect_native_bootlin_target.sh"
      ;;
    *)
      printf '%s' unsupported
      ;;
  esac
}

target_id="${LONEJSON_C_PKT_SYSTEMS_TARGET_ID:-}"
if [[ -z "${target_id}" ]]; then
  target_id="$(detect_target_id)"
fi
bundle_root="${repo_root}/.cache/c.pkt.systems/${target_id}/root"

if [[ ! -d "${bundle_root}" ]]; then
  printf '%s\n' "missing c.pkt.systems bundle at ${bundle_root}" >&2
  printf '%s\n' "hint: run 'make deps-host' first" >&2
  exit 1
fi

printf '%s\n' "${bundle_root}"
