#!/usr/bin/env bash
set -euo pipefail

# Rationale: shared hosts must not make AFL++ release fuzzing fail solely
# because every CPU is already pinned, while explicit caller affinity policy
# must still take precedence.

repo_root=$1
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

test_root="$tmp_dir/repo"
mkdir -p "$test_root/scripts" "$tmp_dir/seeds"
cp "$repo_root/scripts/fuzz.sh" "$test_root/scripts/fuzz.sh"
chmod +x "$test_root/scripts/fuzz.sh"
printf 'seed\n' >"$tmp_dir/seeds/input"

fake_afl="$tmp_dir/fake-afl-fuzz"
cat >"$fake_afl" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
printf 'try=%s no=%s\n' "${AFL_TRY_AFFINITY-}" "${AFL_NO_AFFINITY-}" >"$AFL_AFFINITY_LOG"
printf '%s\n' "$@" >"$AFL_AFFINITY_LOG.args"
while [[ $# -gt 0 ]]; do
  if [[ "$1" == -o ]]; then
    mkdir -p "$2"
    if [[ -n "${TEST_FINDING:-}" ]]; then
      mkdir -p "$2/default/$TEST_FINDING"
      printf 'saved input' >"$2/default/$TEST_FINDING/id:000000"
    fi
  fi
  shift
done
exit "${TEST_ENGINE_STATUS:-0}"
EOF
chmod +x "$fake_afl"
fake_target="$tmp_dir/target"
printf '#!/bin/sh\necho lonejson-fuzz-no-core-v1\n' >"$fake_target"
chmod +x "$fake_target"

run_case() {
  local case_name=$1
  local expected=$2
  shift 2
  local log_path="$tmp_dir/$case_name.log"
  (
    unset AFL_TRY_AFFINITY AFL_NO_AFFINITY
    "$@" AFL_AFFINITY_LOG="$log_path" \
      "$test_root/scripts/fuzz.sh" "$fake_afl" 1 "$case_name" "$fake_target" "$tmp_dir/seeds"
  )
  if [[ "$(<"$log_path")" != "$expected" ]]; then
    printf '%s affinity policy mismatch: expected %s, got %s\n' \
      "$case_name" "$expected" "$(<"$log_path")" >&2
    exit 1
  fi
}

run_case default 'try=1 no=' env
run_case no_affinity 'try= no=1' env AFL_NO_AFFINITY=1
run_case try_affinity 'try=custom no=' env AFL_TRY_AFFINITY=custom
run_case executions 'try=1 no=' env LONEJSON_FUZZ_EXECUTIONS=1000
[[ "$(head -2 "$tmp_dir/executions.log.args")" == $'-E\n1000' ]]
[[ "$(head -2 "$tmp_dir/default.log.args")" == $'-V\n1' ]]

for finding in crashes hangs; do
  if AFL_AFFINITY_LOG="$tmp_dir/finding.log" TEST_FINDING="$finding" \
    "$test_root/scripts/fuzz.sh" "$fake_afl" 1 "$finding" "$fake_target" "$tmp_dir/seeds"; then
    printf 'saved %s did not fail the gate\n' "$finding" >&2
    exit 1
  fi
  [[ -f "$test_root/build/fuzz/afl/$finding/output/default/$finding/id:000000" ]]
done
status=0
AFL_AFFINITY_LOG="$tmp_dir/engine.log" TEST_ENGINE_STATUS=42 \
  "$test_root/scripts/fuzz.sh" "$fake_afl" 1 engine "$fake_target" "$tmp_dir/seeds" || status=$?
[[ "$status" == 42 ]]

if LONEJSON_FUZZ_EXECUTIONS=invalid "$test_root/scripts/fuzz.sh" "$fake_afl" \
  1 invalid "$fake_target" "$tmp_dir/seeds"; then
  echo 'invalid execution budget was accepted' >&2
  exit 1
fi

if "$test_root/scripts/fuzz.sh" "$fake_afl" 1 unsafe /bin/true "$tmp_dir/seeds"; then
  printf 'unprotected target was accepted\n' >&2
  exit 1
fi
printf '#!/bin/sh\necho lonejson-fuzz-no-core-v1\nexit 42\n' >"$fake_target"
if "$test_root/scripts/fuzz.sh" "$fake_afl" 1 broken "$fake_target" "$tmp_dir/seeds"; then
  echo 'failed isolation probe was accepted' >&2
  exit 1
fi
