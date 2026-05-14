#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

detect_arch() {
  case "$("${UNAME:-uname}" -m)" in
    x86_64|amd64) printf '%s' x86_64 ;;
    aarch64|arm64) printf '%s' aarch64 ;;
    armv7*|armv6*|armhf) printf '%s' armhf ;;
    *) printf '%s' unsupported ;;
  esac
}

detect_libc() {
  if ldd --version 2>&1 | tr '[:upper:]' '[:lower:]' | grep -q musl; then
    printf '%s' musl
  else
    printf '%s' gnu
  fi
}

detect_target_id() {
  case "$("${UNAME:-uname}" -s)" in
    Darwin)
      case "$("${UNAME:-uname}" -m)" in
        arm64|aarch64) printf '%s' arm64-apple-darwin ;;
        *) printf '%s' unsupported ;;
      esac
      ;;
    *)
      printf '%s-linux-%s' "$(detect_arch)" "$(detect_libc)"
      ;;
  esac
}

target_id="${LONEJSON_C_PKT_SYSTEMS_TARGET_ID:-}"
if [[ -z "${target_id}" ]]; then
  target_id="$(detect_target_id)"
fi
bundle_root="${repo_root}/.deps/c.pkt.systems/${target_id}/root"

if [[ ! -d "${bundle_root}" ]]; then
  printf '%s\n' "missing c.pkt.systems bundle at ${bundle_root}" >&2
  printf '%s\n' "hint: run 'make deps-host' first" >&2
  exit 1
fi

printf '%s\n' "${bundle_root}"
