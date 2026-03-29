#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

detect_arch() {
  case "$(uname -m)" in
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

arch="${LONEJSON_LIBLOCKDC_ARCH:-$(detect_arch)}"
libc="${LONEJSON_LIBLOCKDC_LIBC:-$(detect_libc)}"
bundle_root="${repo_root}/.deps/liblockdc/${arch}-linux-${libc}/root"

if [[ ! -d "${bundle_root}" ]]; then
  printf '%s\n' "missing liblockdc bundle at ${bundle_root}" >&2
  printf '%s\n' "hint: run 'make deps-host' first" >&2
  exit 1
fi

printf '%s\n' "${bundle_root}"
