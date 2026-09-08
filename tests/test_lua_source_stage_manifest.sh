#!/usr/bin/env bash
set -euo pipefail

# Rationale: Lua source archives must be staged from an explicit manifest so
# generated files, local paths, and private source tree state cannot leak.

repo_root=$1
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

stage_dir="$tmp_dir/lonejson-0.0.0"
manifest="$stage_dir/RELEASE_MANIFEST"

"$repo_root/scripts/stage_lua_rock_sources.sh" \
  "$repo_root" "$stage_dir" "0.0.0"

require_manifest_entry() {
  local path=$1
  if [[ ! -f "$stage_dir/$path" ]]; then
    printf 'staged Lua rock source is missing %s\n' "$path" >&2
    exit 1
  fi
  if ! grep -Fx "$path" "$manifest" >/dev/null; then
    printf 'staged Lua rock manifest is missing %s\n' "$path" >&2
    exit 1
  fi
}

relative_path() {
  local base=$1
  local path=$2
  local abs_base
  local abs_dir
  local abs_path

  abs_base=$(cd "$base" && pwd -P)
  abs_dir=$(cd "$(dirname "$path")" && pwd -P)
  abs_path="$abs_dir/$(basename "$path")"

  case "$abs_path" in
    "$abs_base"/*)
      printf '%s\n' "${abs_path#"$abs_base"/}"
      ;;
    *)
      return 1
      ;;
  esac
}

resolve_staged_include() {
  local rel_file=$1
  local include_path=$2
  local rel_dir
  local candidate
  rel_dir=$(dirname "$rel_file")

  for candidate in \
    "$stage_dir/$rel_dir/$include_path" \
    "$stage_dir/include/$include_path" \
    "$stage_dir/src/$include_path"; do
    if [[ -f "$candidate" ]]; then
      relative_path "$stage_dir" "$candidate"
      return 0
    fi
  done

  return 1
}

require_manifest_entry src/lua/lonejson_lua_visitor_facade.inc.h
require_manifest_entry scripts/cpkt-toolchains.sh
require_manifest_entry scripts/detect_native_bootlin_target.sh
require_manifest_entry cmake/toolchains/print_native_bootlin_target.cmake
require_manifest_entry cmake/toolchains/lonejson_native_bootlin_target.cmake

(
  cd "$stage_dir"
  ./scripts/render_release_rockspec.sh \
    "0.0.0" "$tmp_dir/packed.rockspec" \
    "file://lonejson-lua-0.0.0.tar.gz" "" "so" "lonejson-0.0.0"
)
if ! grep -Fx '  dir = "lonejson-0.0.0",' "$tmp_dir/packed.rockspec" >/dev/null; then
  printf 'packed Lua rockspec is missing source.dir\n' >&2
  exit 1
fi

"$repo_root/scripts/render_release_rockspec.sh" \
  "0.0.0" "$tmp_dir/default.rockspec"
if grep -F '@SOURCE_DIR_LINE@' "$tmp_dir/default.rockspec" >/dev/null; then
  printf 'default Lua rockspec still contains source dir placeholder\n' >&2
  exit 1
fi
if grep -F '  dir = ' "$tmp_dir/default.rockspec" >/dev/null; then
  printf 'default Lua rockspec unexpectedly contains source.dir\n' >&2
  exit 1
fi

while IFS= read -r staged_file; do
  rel_file=${staged_file#"$stage_dir"/}
  while IFS= read -r line; do
    if [[ $line =~ ^[[:space:]]*#[[:space:]]*include[[:space:]]*\"([^\"]+)\" ]]; then
      include_path=${BASH_REMATCH[1]}
      if ! resolved_include=$(resolve_staged_include "$rel_file" "$include_path"); then
        printf 'unresolved staged Lua rock source include: %s includes %s\n' \
          "$rel_file" "$include_path" >&2
        exit 1
      fi
      if ! grep -Fx "$resolved_include" "$manifest" >/dev/null; then
        printf 'staged Lua rock manifest is missing include %s from %s\n' \
          "$resolved_include" "$rel_file" >&2
        exit 1
      fi
    fi
  done <"$staged_file"
done < <(find "$stage_dir" -type f \( -name '*.c' -o -name '*.h' \) | LC_ALL=C sort)
