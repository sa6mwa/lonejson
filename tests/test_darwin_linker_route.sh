#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

require_text() {
  local file=$1
  local text=$2
  if ! grep -F -- "$text" "$repo_root/$file" >/dev/null; then
    printf 'missing required text in %s: %s\n' "$file" "$text" >&2
    exit 1
  fi
}

reject_text() {
  local file=$1
  local text=$2
  if grep -F -- "$text" "$repo_root/$file" >/dev/null; then
    printf 'forbidden text in %s: %s\n' "$file" "$text" >&2
    exit 1
  fi
}

require_text cmake/toolchains/arm64-apple-darwin.cmake \
  'set(ENV{PATH} "${LONEJSON_OSXCROSS_BIN_DIR}:$ENV{PATH}")'
require_text cmake/toolchains/arm64-apple-darwin.cmake \
  'set(_lonejson_darwin_linker_flag "-fuse-ld=${CMAKE_LINKER}")'
require_text cmake/package_darwin_smoke_bundle.cmake \
  'set(ENV{PATH} "${_lonejson_linker_dir}:$ENV{PATH}")'
require_text cmake/package_darwin_smoke_bundle.cmake \
  '"-fuse-ld=${CMAKE_LINKER}"'
require_text scripts/smoke_darwin_release.sh \
  'export PATH="$tool_bin:$PATH"'
require_text scripts/smoke_darwin_release.sh \
  '"-fuse-ld=${ld}"'
require_text scripts/verify_release_archives.sh \
  'printf '\''%s\n'\'' "-fuse-ld=$LINKER"'
require_text scripts/verify_release_archives.sh \
  'PATH="$linker_dir:$PATH" "$@"'

reject_text cmake/toolchains/arm64-apple-darwin.cmake '--ld-path='
reject_text cmake/package_darwin_smoke_bundle.cmake '--ld-path='
reject_text scripts/smoke_darwin_release.sh '--ld-path='
reject_text scripts/verify_release_archives.sh '--ld-path='

osxcross_root=${OSXCROSS_ROOT:-$HOME/.local/cross/osxcross}
target_host=${LONEJSON_OSXCROSS_HOST:-arm64-apple-darwin25}
cc="$osxcross_root/bin/$target_host-clang"
ld="$osxcross_root/bin/$target_host-ld"

if [[ ! -x "$cc" || ! -x "$ld" ]]; then
  printf 'skipping osxcross link-route smoke; missing %s or %s\n' "$cc" "$ld"
  exit 0
fi

cat >"$tmp_dir/main.c" <<'EOF'
int main(void) {
  return 0;
}
EOF

tool_bin="$(dirname -- "$ld")"
route_log="$tmp_dir/route.log"
env PATH="$tool_bin:/usr/bin:/bin" "$cc" -### \
  "-mmacosx-version-min=${LONEJSON_MACOS_DEPLOYMENT_TARGET:-15.0}" \
  "-fuse-ld=$ld" \
  "$tmp_dir/main.c" \
  -o "$tmp_dir/a.out" \
  >"$route_log" 2>&1 || true

if ! grep -F -- "$ld" "$route_log" >/dev/null; then
  printf 'osxcross dry-run did not route through target ld: %s\n' "$ld" >&2
  cat "$route_log" >&2
  exit 1
fi
