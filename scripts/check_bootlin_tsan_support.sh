#!/usr/bin/env bash
set -euo pipefail

# ThreadSanitizer belongs to the selected default toolchain, not to an
# unrelated host Clang installation. Do not download while Make is parsing:
# lifecycle configuration ensures the pinned collection before this gate.

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
compiler="$("$repo_root/scripts/cpkt-toolchains.sh" discover x86_64-linux-gnu 2>/dev/null |
  sed -n 's/^cc=//p')"
if [[ -z "$compiler" || ! -x "$compiler" ]]; then
  printf '0\n'
  exit 0
fi

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT
printf '%s\n' 'int main(void) { return 0; }' >"$tmp_dir/probe.c"
if "$compiler" -std=c89 -fsanitize=thread "$tmp_dir/probe.c" -o "$tmp_dir/probe" >/dev/null 2>&1; then
  printf '1\n'
else
  printf '0\n'
fi
