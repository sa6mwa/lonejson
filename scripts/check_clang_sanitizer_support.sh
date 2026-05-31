#!/usr/bin/env bash
set -euo pipefail

sanitizer="${1:-}"
case "$sanitizer" in
  thread) flag='-fsanitize=thread' ;;
  memory) flag='-fsanitize=memory' ;;
  *)
    printf '0\n'
    exit 0
    ;;
esac

if ! command -v clang >/dev/null 2>&1; then
  printf '0\n'
  exit 0
fi

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT
src="$tmp_dir/probe.c"
bin="$tmp_dir/probe"
printf '%s\n' 'int main(void) { return 0; }' >"$src"

if clang -std=c89 "$flag" "$src" -o "$bin" >/dev/null 2>&1; then
  printf '1\n'
else
  printf '0\n'
fi
