#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

set +e
"$repo_root/scripts/clean.sh" --root "$tmp_dir" >"$tmp_dir/stdout" 2>"$tmp_dir/stderr"
status=$?
set -e
if [[ "$status" -eq 0 ]]; then
  printf 'clean.sh accepted an arbitrary root\n' >&2
  exit 1
fi
grep -F 'refusing to clean unsafe root' "$tmp_dir/stderr" >/dev/null

script_text=$(<"$repo_root/scripts/clean.sh")
for generated_path in \
  'build' \
  'dist' \
  '.cache' \
  '.luarocks-build' \
  'examples/bin' \
  'lonejson' \
  'devenv/volumes'; do
  grep -F "\"\$root_dir\"/$generated_path" <<<"$script_text" >/dev/null
done

grep -F 'CPKT_DEPENDENCY_CACHE' <<<"$script_text" >/dev/null && {
  printf 'clean.sh must not remove the shared dependency archive cache\n' >&2
  exit 1
}
grep -F 'CPKT_TOOLCHAIN_CACHE' <<<"$script_text" >/dev/null && {
  printf 'clean.sh must not remove the shared toolchain cache\n' >&2
  exit 1
}

grep -F 'refusing to remove unexpected path' <<<"$script_text" >/dev/null
