#!/usr/bin/env bash
set -euo pipefail

# Exercise the real pinned engine with prescribed inputs, not a race against
# random mutation. A crash must retain its input and fail the gate; a hang must
# be distinguished from a crash. No host settings or elevated privileges.
repo_root=$1
descriptor=$("$repo_root/scripts/cpkt-aflpp.sh" discover)
cc=$(sed -n 's/^cc=//p' <<<"$descriptor")
afl=$(sed -n 's/^afl_fuzz=//p' <<<"$descriptor")
showmap=$(sed -n 's/^afl_showmap=//p' <<<"$descriptor")
mkdir -p "$repo_root/build/fuzz"
test_root=$(mktemp -d "$repo_root/build/fuzz/crash-contract.XXXXXX")
trap 'rm -rf "$test_root"' EXIT
mkdir -p "$test_root/scripts" "$test_root/seeds"
cp "$repo_root/scripts/fuzz.sh" "$test_root/scripts/"
cat >"$test_root/fixture.c" <<'EOF'
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/prctl.h>
#include <unistd.h>
int lonejson_fuzz_one_input(const uint8_t *data, size_t size) {
  if (prctl(PR_GET_DUMPABLE, 0L, 0L, 0L, 0L) != 0) _exit(99);
  if (size && data[0] == 'C') raise(SIGSEGV);
  if (size && data[0] == 'A') raise(SIGABRT);
  if (size && data[0] == 'H') for (;;) pause();
  return 0;
}
EOF
"$cc" -Wall -Wextra -Werror -Wl,--fatal-warnings "$repo_root/fuzz/afl_driver.c" \
  "$test_root/fixture.c" -o "$test_root/fixture"
[[ "$("$test_root/fixture" --check-fuzz-isolation)" == lonejson-fuzz-no-core-v1 ]]

check_input() {
  local input=$1 expected=$2 status=0
  printf '%s' "$input" >"$test_root/input"
  AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 "$showmap" -q -t 1000 \
    -o "$test_root/trace" -- "$test_root/fixture" "$test_root/input" \
    >"$test_root/showmap.log" 2>&1 || status=$?
  if [[ "$status" != "$expected" ]]; then
    cat "$test_root/showmap.log" >&2
    printf 'input %s: expected AFL status %s, got %s\n' "$input" "$expected" "$status" >&2
    exit 1
  fi
}
check_input N 0
check_input C 2
check_input A 2
check_input H 1

printf N >"$test_root/seeds/normal"
# Force the first mutation to crash: no random discovery or timing dependency.
cat >"$test_root/mutator.c" <<'EOF'
#include <stddef.h>
void *afl_custom_init(void *afl, unsigned int seed) {
  (void)afl; (void)seed;
  return (void *)1;
}
size_t afl_custom_fuzz(void *data, unsigned char *buf, size_t size,
                      unsigned char **out, unsigned char *add,
                      size_t add_size, size_t max_size) {
  static unsigned char crash = 'C';
  (void)data; (void)buf; (void)size; (void)add; (void)add_size; (void)max_size;
  *out = &crash;
  return 1;
}
void afl_custom_deinit(void *data) { (void)data; }
EOF
"$cc" -Wall -Wextra -Werror -Wl,--fatal-warnings -shared -fPIC \
  "$test_root/mutator.c" -o "$test_root/mutator.so"
status=0
AFL_CUSTOM_MUTATOR_LIBRARY="$test_root/mutator.so" AFL_CUSTOM_MUTATOR_ONLY=1 \
  LONEJSON_FUZZ_EXECUTIONS=100 "$test_root/scripts/fuzz.sh" "$afl" 1 fixture \
  "$test_root/fixture" "$test_root/seeds" >"$test_root/runner.log" 2>&1 || status=$?
[[ "$status" != 0 ]] || { echo 'crash did not fail the gate' >&2; exit 1; }
grep -q 'Fuzz failure retained for reproduction:' "$test_root/runner.log" || {
  cat "$test_root/runner.log" >&2; exit 1;
}
crash=$(find "$test_root/build/fuzz/afl/fixture/output" -path '*/crashes/id:*' -type f -print -quit)
[[ -n "$crash" ]]
printf C >"$test_root/expected-crash"
cmp "$test_root/expected-crash" "$crash"

# Initial bad seeds must fail too; AFL++ otherwise skips timed-out seeds when
# an explicit timeout is supplied. The staged seed remains available to replay.
for input in C H; do
  printf '%s' "$input" >"$test_root/seeds/bad"
  status=0
  LONEJSON_FUZZ_EXECUTIONS=100 "$test_root/scripts/fuzz.sh" "$afl" 1 "seed-$input" \
    "$test_root/fixture" "$test_root/seeds" >"$test_root/seed.log" 2>&1 || status=$?
  [[ "$status" != 0 ]] || { cat "$test_root/seed.log" >&2; exit 1; }
  cmp "$test_root/seeds/bad" "$test_root/build/fuzz/afl/seed-$input/seeds/seeds-bad"
done
printf 'AFL crash handling: normal, SIGSEGV, SIGABRT, hang, retained crash and failing gate passed\n'
