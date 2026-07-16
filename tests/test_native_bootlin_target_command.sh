#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
target_id="$("$repo_root/scripts/detect_native_bootlin_target.sh")"

case "$target_id" in
  x86_64-linux-gnu|x86_64-linux-musl|aarch64-linux-gnu|aarch64-linux-musl|armhf-linux-gnu|armhf-linux-musl)
    ;;
  *)
    printf 'native Bootlin target command returned an unsupported target: %s\n' "$target_id" >&2
    exit 1
    ;;
esac
