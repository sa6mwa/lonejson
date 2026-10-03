#!/usr/bin/env bash
set -euo pipefail

workspace_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
mkdir -p "$workspace_root/build"

# Two independently configured builds may share the same dist directory. The
# single-header generator must serialize its shared temporary artifact rather
# than allowing one process to remove it while the other formats it.
repo_root=$1
tmp_dir=$(mktemp -d "$workspace_root/build/test_single_header_concurrency.XXXXXX")
first_pid=
second_pid=

cleanup() {
  for pid in "$first_pid" "$second_pid"; do
    if [[ -n "$pid" ]]; then
      kill "$pid" 2>/dev/null || true
      wait "$pid" 2>/dev/null || true
    fi
  done
  rm -rf "$tmp_dir"
}
trap cleanup EXIT

"$repo_root/scripts/stage_release_sources.sh" "$repo_root" "$tmp_dir/source" 0.0.0
source_root="$tmp_dir/source"
dist_dir="$tmp_dir/dist"
mkdir -p "$dist_dir"
printf '%s\n' 'lonejson lifecycle artifact directory' >"$dist_dir/.lonejson-dist"

formatter="$tmp_dir/clang-format"
printf '%s\n' \
  '#!/usr/bin/env bash' \
  'set -euo pipefail' \
  'for path in "$@"; do' \
  '  [[ "$path" == -* ]] && continue' \
  '  [[ -f "$path" ]]' \
  'done' \
  'sleep 0.2' \
  'for path in "$@"; do' \
  '  [[ "$path" == -* ]] && continue' \
  '  [[ -f "$path" ]]' \
  'done' >"$formatter"
chmod +x "$formatter"

run_generator() {
  local build_dir=$1
  mkdir -p "$build_dir/generated/include" "$build_dir/generated/single-header/include"
  cmake \
    -D "LONEJSON_ROOT=$source_root" \
    -D "LONEJSON_DIST_DIR=$dist_dir" \
    -D "LONEJSON_BINARY_DIR=$build_dir" \
    -D LONEJSON_VERSION=0.0.0 \
    -D "LONEJSON_PUBLIC_HEADER=$source_root/include/lonejson.h" \
    -D "LONEJSON_INTERNAL_IMPL=$source_root/src/lonejson_impl.h" \
    -D "LONEJSON_SINGLE_HEADER_BUILD=$build_dir/generated/include/lonejson_single_header.h" \
    -D "LONEJSON_SINGLE_HEADER_BUILD_ALIAS=$build_dir/generated/single-header/include/lonejson.h" \
    -D "LONEJSON_SINGLE_HEADER_DIST_GZ=$dist_dir/lonejson-0.0.0.h.gz" \
    -D "LONEJSON_CLANG_FORMAT_BIN=$formatter" \
    -P "$repo_root/cmake/package_single_header.cmake"
}

run_generator "$source_root/build/first" >"$tmp_dir/first.log" 2>&1 &
first_pid=$!
run_generator "$source_root/build/second" >"$tmp_dir/second.log" 2>&1 &
second_pid=$!

if ! wait "$first_pid"; then
  cat "$tmp_dir/first.log" >&2
  exit 1
fi
first_pid=
if ! wait "$second_pid"; then
  cat "$tmp_dir/second.log" >&2
  exit 1
fi
second_pid=

gzip -cd "$dist_dir/lonejson-0.0.0.h.gz" | \
  grep -F '#define LONEJSON_VERSION_MAJOR 0' >/dev/null
[[ ! -e "$dist_dir/lonejson.h.tmp" ]]
