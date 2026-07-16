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
EOF
chmod +x "$fake_afl"

run_case() {
  local case_name=$1
  local expected=$2
  shift 2
  local log_path="$tmp_dir/$case_name.log"
  (
    unset AFL_TRY_AFFINITY AFL_NO_AFFINITY
    "$@" AFL_AFFINITY_LOG="$log_path" \
      "$test_root/scripts/fuzz.sh" "$fake_afl" 1 "$case_name" /bin/true "$tmp_dir/seeds"
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
