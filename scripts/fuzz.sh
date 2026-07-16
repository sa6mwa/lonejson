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

rm -rf "$work_dir"
mkdir -p "$seed_dir"
for source_dir in "$@"; do
  while IFS= read -r -d '' seed; do
    cp "$seed" "$seed_dir/$(basename -- "$source_dir")-$(basename -- "$seed")"
  done < <(find "$source_dir" -type f -print0)
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

exec "$afl_fuzz" -V "$seconds" -i "$seed_dir" -o "$output_dir" -- "$executable" @@
