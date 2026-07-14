#!/usr/bin/env bash
set -euo pipefail

# AFL++ is a shared immutable cache root. A provisioner which waited for the
# lock must retain a root another checkout published before it could build or
# replace anything.

repo_root=$1
resolver="$repo_root/scripts/cpkt-aflpp.sh"
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

grep -F 'lock_file="$lock_dir/aflplusplus-${version}-x86_64-linux-gnu.lock"' "$resolver" >/dev/null
grep -F 'flock "$lock_fd"' "$resolver" >/dev/null
grep -F 'while this process waited for the shared-cache lock' "$resolver" >/dev/null
grep -F 'flock -u "$lock_fd"' "$resolver" >/dev/null
grep -F 'revision=2' "$resolver" >/dev/null
grep -F 'helper="$r/lib/afl"' "$resolver" >/dev/null
grep -F 'export AFL_PATH=$published_root/lib/afl' "$resolver" >/dev/null
grep -F 'ready "$tmp/root" "$r"' "$resolver" >/dev/null
grep -F 'afl-fuzz afl-showmap' "$resolver" >/dev/null
grep -F -- '-DAFL_PATH=\"$helper\"' "$resolver" >/dev/null
grep -F -- '-DBIN_PATH=\"$r/bin\"' "$resolver" >/dev/null
if grep -F 'afl-tmin afl-gotcpu afl-analyze afl-cmin' "$resolver" >/dev/null; then
  printf 'AFL++ resolver must not build unused auxiliary utilities\n' >&2
  exit 1
fi

lock_test_cache="$tmp_dir/lock-test-cache"
lock_test_root="$lock_test_cache/roots/fixture"
lock_test_bin="$tmp_dir/lock-test-bin"
mkdir -p "$lock_test_bin"
cat >"$lock_test_bin/flock" <<'EOF'
#!/usr/bin/env bash
if [[ "${1:-}" != -u ]]; then
  mkdir -p "$CPKT_AFLPP_TEST_READY_ROOT"
  : >"$CPKT_AFLPP_TEST_READY_ROOT/.ready"
fi
EOF
chmod +x "$lock_test_bin/flock"

CPKT_AFLPP_TEST_READY_ROOT="$lock_test_root" \
PATH="$lock_test_bin:$PATH" \
bash -s "$resolver" "$lock_test_cache" "$lock_test_root" <<'EOF'
set -euo pipefail
resolver=$1
fixture_cache=$2
fixture_root=$3
source "$resolver"
cache() {
  printf '%s\n' "$fixture_cache"
}
root() {
  printf '%s\n' "$fixture_root"
}
ready() {
  [[ -f "$1/.ready" ]]
}
ensure
[[ -f "$fixture_root/.ready" ]]
EOF
