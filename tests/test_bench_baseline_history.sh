#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
lua_exec=${2:-lua}
luarocks_exec=${3:-luarocks}
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

make --no-print-directory -C "$repo_root" lua-rock \
  LUA="$lua_exec" LUAROCKS="$luarocks_exec" >/dev/null
eval "$("$luarocks_exec" path --tree "$repo_root/build/luarocks")"

git -C "$tmp_dir" init -q
git -C "$tmp_dir" config user.email test@example.invalid
git -C "$tmp_dir" config user.name 'lonejson test'
mkdir -p "$tmp_dir/perflogs"

write_baseline() {
  local alpha=$1
  local beta=$2
  local gamma=$3
  {
    printf '{"schema_version":1,'
    printf '"timestamp_utc":"2026-01-01T00:00:00Z",'
    printf '"host":"test",'
    printf '"results":['
    printf '{"name":"parse/alpha/lonejson","mib_per_sec":%s}' "$alpha"
    if [ "$beta" != "-" ]; then
      printf ',{"name":"parse/beta/lonejson","mib_per_sec":%s}' "$beta"
    fi
    if [ "$gamma" != "-" ]; then
      printf ',{"name":"stream/gamma/lonejson","mib_per_sec":%s}' "$gamma"
    fi
    printf ']}'
  } >"$tmp_dir/perflogs/baseline.json"
}

write_baseline 100 50 -
git -C "$tmp_dir" add perflogs/baseline.json
git -C "$tmp_dir" commit -q -m 'bench: add first baseline'

write_baseline 120 - 5
git -C "$tmp_dir" add perflogs/baseline.json
git -C "$tmp_dir" commit -q -m 'bench: add second baseline'

write_baseline 90 55 6
git -C "$tmp_dir" add perflogs/baseline.json
git -C "$tmp_dir" commit -q -m 'bench: add third baseline'

output=$("$lua_exec" "$repo_root/scripts/bench_baseline_history.lua" --repo "$tmp_dir" --kind c)

printf '%s\n' "$output" | grep -q 'REGRESSION: 1 metric'
printf '%s\n' "$output" | grep -q 'NOTICE: 1 material regression'
printf '%s\n' "$output" | grep -q 'parse/alpha/lonejson'
printf '%s\n' "$output" | grep -q 'parse/beta/lonejson'
printf '%s\n' "$output" | grep -q 'stream/gamma/lonejson'
printf '%s\n' "$output" | grep -Eq 'parse/beta/lonejson.*50\.00[[:space:]]+\.[[:space:]]+55\.00'

if printf '%s\n' "$output" | awk 'length($0) > 80 { exit 1 }'; then
  exit 0
fi

printf '%s\n' "$output" >&2
printf 'bench baseline history report exceeded 80 columns\n' >&2
exit 1
