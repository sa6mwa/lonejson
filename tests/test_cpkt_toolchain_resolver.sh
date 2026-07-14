#!/usr/bin/env bash
set -euo pipefail

# The lifecycle resolver is both the inventory and provisioning authority. Its
# status output must stay useful before a target is installed so callers can
# distinguish a downloadable Bootlin collection from optional osxcross.

repo_root=$1
resolver="$repo_root/scripts/cpkt-toolchains.sh"
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

# The shared immutable root must be serialized per pinned collection: a
# process that waited for publication re-checks readiness before it could ever
# replace a root another checkout has started using.
grep -F 'lock_file="$lock_dir/$name.lock"' "$resolver" >/dev/null
grep -F 'flock "$lock_fd"' "$resolver" >/dev/null
grep -F 'while this process waited for the per-target shared-cache lock' "$resolver" >/dev/null
grep -F 'flock -u "$lock_fd"' "$resolver" >/dev/null

# Simulate another checkout publishing the immutable root while this process
# waits for its lock. The provisioner must re-check readiness and leave that
# published root untouched rather than falling through to rm -rf/re-extract.
lock_test_cache="$tmp_dir/lock-test-cache"
lock_test_root="$lock_test_cache/roots/fixture"
lock_test_bin="$tmp_dir/lock-test-bin"
mkdir -p "$lock_test_bin"
cat >"$lock_test_bin/flock" <<'EOF'
#!/usr/bin/env bash
if [[ "${1:-}" != -u ]]; then
  mkdir -p "$CPKT_TOOLCHAIN_TEST_READY_ROOT"
  : >"$CPKT_TOOLCHAIN_TEST_READY_ROOT/.ready"
fi
EOF
chmod +x "$lock_test_bin/flock"

CPKT_TOOLCHAIN_CACHE="$lock_test_cache" \
CPKT_TOOLCHAIN_TEST_READY_ROOT="$lock_test_root" \
PATH="$lock_test_bin:$PATH" \
bash -s "$resolver" <<'EOF'
set -euo pipefail
resolver=$1
source "$resolver"
toolchain_values() {
  printf '%s\n' "fixture|fixture|unused|fixture|sysroot|$CPKT_TOOLCHAIN_CACHE/roots/fixture"
}
toolchain_ready() {
  [[ -f "$1/.ready" ]]
}
download_file() {
  printf '%s\n' 'provisioner did not re-check the shared root after locking' >&2
  return 1
}
ensure_target fixture
[[ -f "$CPKT_TOOLCHAIN_CACHE/roots/fixture/.ready" ]]
EOF

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
