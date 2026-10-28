#!/usr/bin/env bash
set -euo pipefail

# Rationale: test-all must retain the required native Valgrind gate while
# preserving an explicit skip only for optional TSan support.

repo_root=$1

without_support=$(make -C "$repo_root" -n test-all LONEJSON_HAVE_TSAN=0)
printf '%s\n' "$without_support" | grep -F 'test-host-curl' >/dev/null
printf '%s\n' "$without_support" | grep -E '(^|[[:space:]])make[[:space:]]+tsan($|[[:space:]])' >/dev/null
printf '%s\n' "$without_support" | grep -E '(^|[[:space:]])make[[:space:]]+valgrind($|[[:space:]])' >/dev/null

tsan_only=$(make -C "$repo_root" -n test-all LONEJSON_HAVE_TSAN=1)
printf '%s\n' "$tsan_only" | grep -E '(^|[[:space:]])make[[:space:]]+tsan($|[[:space:]])' >/dev/null
printf '%s\n' "$tsan_only" | grep -E '(^|[[:space:]])make[[:space:]]+valgrind($|[[:space:]])' >/dev/null

hardening_dry_run=$(make -C "$repo_root" -n prerelease-hardening LONEJSON_HAVE_TSAN=0)
printf '%s\n' "$hardening_dry_run" | grep -E '(^|[[:space:]])make[[:space:]]+bench-check($|[[:space:]])' >/dev/null

asan_dry_run=$(make -C "$repo_root" -n asan)
printf '%s\n' "$asan_dry_run" | grep -F 'lonejson_lua_(target' >/dev/null

tsan_dry_run=$(make -C "$repo_root" -n tsan)
printf '%s\n' "$tsan_dry_run" | grep -F 'lonejson_lua_(target' >/dev/null
printf '%s\n' "$tsan_dry_run" | grep -F 'check_bootlin_tsan_support.sh' >/dev/null

valgrind_dry_run=$(make -C "$repo_root" -n valgrind)
printf '%s\n' "$valgrind_dry_run" | grep -F -- 'MemoryCheckCommandOptions=--leak-check=full' >/dev/null
printf '%s\n' "$valgrind_dry_run" | grep -F -- "-T MemCheck" >/dev/null
printf '%s\n' "$valgrind_dry_run" | grep -F 'lonejson_curl_rewind_tests' >/dev/null
printf '%s\n' "$valgrind_dry_run" | grep -F 'lonejson_lua_target_runner' >/dev/null
printf '%s\n' "$valgrind_dry_run" | grep -F 'lua_target_tests' >/dev/null

# A failed prerequisite must stop the real TSan recipe before stale test
# binaries can supply a green result. Exercise each phase with recording tools.
mkdir -p "$repo_root/build"
work=$(mktemp -d "$repo_root/build/tsan-gate.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
mkdir "$work/bin"
cat >"$work/bin/cmake" <<'SH'
#!/bin/bash
for argument in "$@"; do
  if [[ "$argument" == -P ]]; then exec "$TSAN_REAL_CMAKE" "$@"; fi
done
printf 'cmake %s\n' "$*" >>"$TSAN_GATE_EVENTS"
case "$1" in
  --preset) [[ "${TSAN_GATE_FAIL:-}" != configure ]] ;;
  --build) [[ "${TSAN_GATE_FAIL:-}" != build ]] ;;
  *) exit 91 ;;
esac
SH
cat >"$work/bin/ctest" <<'SH'
#!/bin/bash
printf 'ctest %s\n' "$*" >>"$TSAN_GATE_EVENTS"
[[ "${TSAN_GATE_FAIL:-}" != test ]]
SH
chmod +x "$work/bin/"*
export TSAN_GATE_EVENTS="$work/events"
export TSAN_REAL_CMAKE="$(command -v cmake)"
for phase in configure build test; do
  : >"$TSAN_GATE_EVENTS"
  if PATH="$work/bin:$PATH" TSAN_GATE_FAIL="$phase" \
      make -s -C "$repo_root" tsan LONEJSON_HAVE_TSAN=1 >"$work/failure" 2>&1; then
    printf 'TSan gate swallowed failed %s\n' "$phase" >&2; exit 1
  fi
  expected=1
  [[ "$phase" != build ]] || expected=2
  [[ "$phase" != test ]] || expected=3
  [[ $(wc -l <"$TSAN_GATE_EVENTS") == "$expected" ]]
done
: >"$TSAN_GATE_EVENTS"
PATH="$work/bin:$PATH" make -s -C "$repo_root" tsan LONEJSON_HAVE_TSAN=0
[[ ! -s "$TSAN_GATE_EVENTS" ]]
if PATH="$work/bin:$PATH" make -s -C "$repo_root" tsan LONEJSON_HAVE_TSAN=invalid >"$work/invalid" 2>&1; then
  printf 'invalid TSan support setting supplied a skip\n' >&2; exit 1
fi
grep -F 'LONEJSON_HAVE_TSAN must be' "$work/invalid" >/dev/null
[[ ! -s "$TSAN_GATE_EVENTS" ]]
