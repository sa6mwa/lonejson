#!/usr/bin/env bash
set -euo pipefail

# The lifecycle resolver is both the inventory and provisioning authority. Its
# status output must stay useful before a target is installed so callers can
# distinguish a downloadable Bootlin collection from optional osxcross.

repo_root=$1
resolver="$repo_root/scripts/cpkt-toolchains.sh"
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

targets=$("$resolver" targets)
expected_targets=$'x86_64-linux-gnu\nx86_64-linux-musl\naarch64-linux-gnu\naarch64-linux-musl\narmhf-linux-gnu\narmhf-linux-musl\narm64-apple-darwin'
[[ "$targets" == "$expected_targets" ]]

description=$(CPKT_TOOLCHAIN_CACHE="$tmp_dir/toolchains" OSXCROSS_ROOT="$tmp_dir/osxcross" "$resolver" discover)

for target in x86_64-linux-gnu x86_64-linux-musl aarch64-linux-gnu aarch64-linux-musl armhf-linux-gnu armhf-linux-musl; do
  target_block=$(printf '%s\n' "$description" | awk -v target="$target" '$0 == "target=" target { printing = 1 } printing { print } printing && /^$/ { exit }')
  grep -Fx "target=$target" <<<"$target_block" >/dev/null
  grep -Fx 'source=bootlin' <<<"$target_block" >/dev/null
  grep -Fx 'status=missing' <<<"$target_block" >/dev/null
  grep -Fx 'downloadable=yes' <<<"$target_block" >/dev/null
done

darwin_block=$(printf '%s\n' "$description" | awk '$0 == "target=arm64-apple-darwin" { printing = 1 } printing { print }')
grep -Fx 'source=osxcross' <<<"$darwin_block" >/dev/null
grep -Fx 'status=missing' <<<"$darwin_block" >/dev/null
grep -Fx 'downloadable=no' <<<"$darwin_block" >/dev/null

set +e
CPKT_TOOLCHAIN_CACHE="$tmp_dir/toolchains" OSXCROSS_ROOT="$tmp_dir/osxcross" "$resolver" env x86_64-linux-gnu >"$tmp_dir/env.out" 2>"$tmp_dir/env.err"
status=$?
set -e
[[ "$status" -ne 0 ]]
grep -F 'target is missing; run:' "$tmp_dir/env.err" >/dev/null
