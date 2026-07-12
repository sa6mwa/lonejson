#!/usr/bin/env bash
set -euo pipefail

[[ "${1:-}" == memory ]] || { printf '0\n'; exit 0; }
repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
compiler="$("$repo_root/scripts/cpkt-llvm.sh" discover 2>/dev/null | sed -n 's/^cc=//p')"
if [[ -z "$compiler" || ! -x "$compiler" ]]; then
  printf '0\n'
  exit 0
fi
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT
printf '%s\n' 'int main(void) { return 0; }' >"$tmp_dir/probe.c"
if "$compiler" -std=c89 -fsanitize=memory "$tmp_dir/probe.c" -o "$tmp_dir/probe" >/dev/null 2>&1; then
  printf '1\n'
else
  printf '0\n'
fi
