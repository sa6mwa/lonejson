#!/usr/bin/env bash
set -euo pipefail

workspace_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
mkdir -p "$workspace_root/build"

# The lifecycle resolver is both the inventory and provisioning authority. Its
# status output must stay useful before a target is installed so callers can
# distinguish a downloadable Bootlin collection from optional osxcross.

repo_root=$1
resolver="$repo_root/scripts/cpkt-toolchains.sh"
tmp_dir=$(mktemp -d "$workspace_root/build/test_cpkt_toolchain_resolver.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

grep -F 'with_cache_lock "$(cache_root)/locks/bootlin-$name.lock"' "$resolver" >/dev/null
grep -F 'CPKT_TOOLCHAIN_LOCK_TIMEOUT' "$resolver" >/dev/null

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
CPKT_TOOLCHAIN_LOCK_TIMEOUT=17 \
PATH="$lock_test_bin:$PATH" \
bash -s "$resolver" <<'EOF'
set -euo pipefail
resolver=$1
source "$resolver"
bootlin_values() {
  printf '%s\n' "fixture|fixture|unused|fixture|sysroot|$CPKT_TOOLCHAIN_CACHE/roots/fixture"
}
bootlin_ready() {
  [[ -f "$1/.ready" ]]
}
download_file() {
  printf '%s\n' 'provisioner did not re-check the shared root after locking' >&2
  return 1
}
install_bootlin fixture
[[ -f "$CPKT_TOOLCHAIN_CACHE/roots/fixture/.ready" ]]
EOF

set +e
CPKT_TOOLCHAIN_CACHE="$tmp_dir/invalid-timeout-cache" \
CPKT_TOOLCHAIN_LOCK_TIMEOUT=invalid bash -s "$resolver" \
  >"$tmp_dir/invalid-timeout.out" 2>"$tmp_dir/invalid-timeout.err" <<'EOF'
set -euo pipefail
resolver=$1
source "$resolver"
bootlin_values() { printf '%s\n' 'fixture|fixture|unused|fixture|sysroot|/tmp/fixture'; }
bootlin_ready() { return 1; }
install_bootlin fixture
EOF
invalid_timeout_status=$?
set -e
if [[ $invalid_timeout_status -eq 0 ]]; then
  printf 'expected invalid toolchain lock timeout to fail\n' >&2
  exit 1
fi
grep -F 'CPKT_TOOLCHAIN_LOCK_TIMEOUT must be a positive integer' \
  "$tmp_dir/invalid-timeout.err" >/dev/null

# A corrupt shared archive must be discarded and reacquired while the same
# per-target lock is held, so a transient partial download never requires
# manual cache cleanup.
recovery_cache="$tmp_dir/recovery-cache"
recovery_payload="$tmp_dir/recovery-payload"
recovery_archive="$tmp_dir/fixture.tar.xz"
mkdir -p "$recovery_payload/fixture/bin" "$recovery_cache/archives"
: >"$recovery_payload/fixture/.ready"
tar -C "$recovery_payload" -cJf "$recovery_archive" fixture
recovery_sha256=$(sha256sum "$recovery_archive" | awk '{print $1}')
printf '%s\n' corrupt >"$recovery_cache/archives/fixture.tar.xz"
mkdir -p "$recovery_cache/roots/fixture"
: >"$recovery_cache/roots/fixture/stale"

recovery_log="$tmp_dir/recovery.log"
CPKT_TOOLCHAIN_CACHE="$recovery_cache" \
CPKT_TOOLCHAIN_TEST_GOOD_ARCHIVE="$recovery_archive" \
CPKT_TOOLCHAIN_TEST_EXPECTED_SHA256="$recovery_sha256" \
bash -s "$resolver" >"$recovery_log" 2>&1 <<'EOF'
set -euo pipefail
resolver=$1
source "$resolver"
bootlin_values() {
  printf '%s\n' "fixture|fixture|$CPKT_TOOLCHAIN_TEST_EXPECTED_SHA256|fixture|sysroot|$CPKT_TOOLCHAIN_CACHE/roots/fixture"
}
bootlin_ready() {
  [[ -f "$1/.ready" ]]
}
download_file() {
  cp "$CPKT_TOOLCHAIN_TEST_GOOD_ARCHIVE" "$2"
}
install_bootlin fixture
EOF

grep -F 'discarding corrupt cached archive' "$recovery_log" >/dev/null
[[ "$(sha256sum "$recovery_cache/archives/fixture.tar.xz" | awk '{print $1}')" == "$recovery_sha256" ]]
test -f "$recovery_cache/roots/fixture/.ready"
test ! -e "$recovery_cache/roots/fixture/stale"

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

# Downstream SDK consumers do not generate Mach interfaces or provision MIG.
# Select the newest complete 25.x prefix; an exact pin is authoritative.
osxcross="$tmp_dir/osxcross-prefixes"
mkdir -p "$osxcross/bin" "$osxcross/SDK"
for prefix in arm64-apple-darwin25.4 arm64-apple-darwin25.10 arm64-apple-darwin25.11; do
  for tool in clang clang++ ld ar ranlib strip nm otool; do
    printf '#!/usr/bin/env bash\nexit 0\n' >"$osxcross/bin/$prefix-$tool"
    chmod +x "$osxcross/bin/$prefix-$tool"
  done
done
rm "$osxcross/bin/arm64-apple-darwin25.11-otool"
OSXCROSS_ROOT="$osxcross" CPKT_TOOLCHAIN_CACHE="$tmp_dir/no-mig-cache" \
  "$resolver" ensure arm64-apple-darwin >"$tmp_dir/darwin-description"
grep -Fx 'prefix=arm64-apple-darwin25.10' "$tmp_dir/darwin-description" >/dev/null
grep -Fx 'status=ready' "$tmp_dir/darwin-description" >/dev/null
[[ ! -e "$tmp_dir/no-mig-cache" ]]
OSXCROSS_ROOT="$osxcross" CPKT_OSXCROSS_HOST=arm64-apple-darwin25.4 \
  "$resolver" discover arm64-apple-darwin >"$tmp_dir/darwin-description"
grep -Fx 'prefix=arm64-apple-darwin25.4' "$tmp_dir/darwin-description" >/dev/null
OSXCROSS_ROOT="$osxcross" CPKT_OSXCROSS_HOST=arm64-apple-darwin25.11 \
  "$resolver" discover arm64-apple-darwin >"$tmp_dir/darwin-description"
grep -Fx 'status=missing' "$tmp_dir/darwin-description" >/dev/null
if OSXCROSS_ROOT="$osxcross" CPKT_OSXCROSS_HOST=arm64-apple-darwin25.11 \
  "$resolver" ensure arm64-apple-darwin >"$tmp_dir/out" 2>"$tmp_dir/err"; then
  printf 'incomplete pinned osxcross prefix accepted\n' >&2; exit 1
fi

# A renamed verified toolchain archive must repopulate an empty install root
# without calling the network acquisition function.
mv "$recovery_cache/archives/fixture.tar.xz" "$recovery_cache/archives/renamed.tar.xz"
rm -rf "$recovery_cache/roots/fixture"
CPKT_TOOLCHAIN_CACHE="$recovery_cache" \
CPKT_TOOLCHAIN_TEST_EXPECTED_SHA256="$recovery_sha256" \
bash -s "$resolver" <<'EOF'
set -euo pipefail
source "$1"
bootlin_values() {
  printf '%s\n' "fixture|fixture|$CPKT_TOOLCHAIN_TEST_EXPECTED_SHA256|fixture|sysroot|$CPKT_TOOLCHAIN_CACHE/roots/fixture"
}
bootlin_ready() { [[ -f "$1/.ready" ]]; }
download_file() { printf 'network called on digest hit\n' >&2; exit 1; }
install_bootlin fixture
EOF
[[ -f "$recovery_cache/roots/fixture/.ready" ]]
