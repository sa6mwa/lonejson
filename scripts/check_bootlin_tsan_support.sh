#!/usr/bin/env bash
set -euo pipefail

# ThreadSanitizer belongs to the selected native Bootlin toolchain, not to an
# unrelated host compiler. Resolve the native target at gate time so test-all
# cannot skip TSan because Make parsed before toolchain provisioning finished.

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
native_target="$("$repo_root/scripts/detect_native_bootlin_target.sh")"
case "$native_target" in
  *-linux-*) ;;
  *)
    printf '0\n'
    exit 0
    ;;
esac

"$repo_root/scripts/cpkt-toolchains.sh" ensure "$native_target" >/dev/null
compiler="$("$repo_root/scripts/cpkt-toolchains.sh" discover "$native_target" 2>/dev/null |
  sed -n 's/^cc=//p')"
if [[ -z "$compiler" || ! -x "$compiler" ]]; then
  printf 'check_bootlin_tsan_support.sh: missing native Bootlin compiler for %s\n' \
    "$native_target" >&2
  exit 1
fi

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT
printf '%s\n' 'int main(void) { return 0; }' >"$tmp_dir/probe.c"
if "$compiler" -std=c89 -fsanitize=thread "$tmp_dir/probe.c" -o "$tmp_dir/probe" >/dev/null 2>&1; then
  printf '1\n'
else
  printf '0\n'
fi
