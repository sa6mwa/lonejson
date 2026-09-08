#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 5 ]]; then
  printf 'usage: %s AFL_FUZZ SECONDS NAME EXECUTABLE SEED_DIR...\n' "$0" >&2
  exit 1
fi

afl_fuzz=$1
seconds=$2
name=$3
executable=$4
shift 4
repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
work_dir="$repo_root/build/fuzz/afl/$name"
seed_dir="$work_dir/seeds"
output_dir="$work_dir/output"

if ! isolation=$("$executable" --check-fuzz-isolation) || [[ "$isolation" != lonejson-fuzz-no-core-v1 ]]; then
  printf 'Fuzz target does not provide process-local core isolation: %s\n' "$executable" >&2
  exit 1
fi
# The shared driver disables dumpability before every input, so no external
# core collector can delay input crash reporting. Never apply this bypass to
# arbitrary executables without the driver contract above.
export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1
export AFL_EXIT_ON_SEED_ISSUES=1

rm -rf "$work_dir"
mkdir -p "$seed_dir"
for source_dir in "$@"; do
  while IFS= read -r -d '' seed; do
    cp "$seed" "$seed_dir/$(basename -- "$source_dir")-$(basename -- "$seed")"
  done < <(find "$source_dir" -type f -print0 | LC_ALL=C sort -z)
done
if ! find "$seed_dir" -type f -print -quit | grep -q .; then
  printf 'AFL++ seed set is empty: %s\n' "$name" >&2
  exit 1
fi

# On a shared host AFL++ can find every CPU already pinned by another task and
# abort before fuzzing. Keep its affinity optimization when it can bind, but
# allow the lifecycle smoke and release gates to continue when it cannot.
# An explicit AFL_NO_AFFINITY or AFL_TRY_AFFINITY remains caller-controlled.
if [[ -z "${AFL_NO_AFFINITY+x}" && -z "${AFL_TRY_AFFINITY+x}" ]]; then
  export AFL_TRY_AFFINITY=1
fi

budget=(-V "$seconds")
if [[ -n "${LONEJSON_FUZZ_EXECUTIONS:-}" ]]; then
  [[ "$LONEJSON_FUZZ_EXECUTIONS" =~ ^[1-9][0-9]*$ ]] || {
    printf 'LONEJSON_FUZZ_EXECUTIONS must be a positive integer\n' >&2
    exit 1
  }
  budget=(-E "$LONEJSON_FUZZ_EXECUTIONS")
fi
"$afl_fuzz" "${budget[@]}" -s 1 -t 1000 -i "$seed_dir" -o "$output_dir" -- "$executable" @@
# AFL++ can finish successfully after saving failures; a verification gate must
# fail and retain those inputs for reproduction.
failure=$(find "$output_dir" -type f \( -path '*/crashes/id:*' -o -path '*/hangs/id:*' \) -print -quit)
if [[ -n "$failure" ]]; then
  printf 'Fuzz failure retained for reproduction: %s\n' "$failure" >&2
  exit 1
fi
