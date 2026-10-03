#!/usr/bin/env bash
set -euo pipefail

workspace_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
mkdir -p "$workspace_root/build"

# Rationale: Darwin osxcross builds must prove the compiler driver routes links
# through the target linker, not the host linker. The route uses PATH plus
# Clang's absolute-linker option; the exact spelling must follow current Clang
# diagnostics so release consumers remain warning-clean under -Werror.

repo_root=$1
tmp_dir="$(mktemp -d "$workspace_root/build/test_darwin_linker_route.XXXXXX")"
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
  'set(_lonejson_darwin_linker_flag "--ld-path=${CMAKE_LINKER}")'
require_text cmake/package_darwin_smoke_bundle.cmake \
  'set(ENV{PATH} "${_lonejson_linker_dir}:$ENV{PATH}")'
require_text cmake/package_darwin_smoke_bundle.cmake \
  '"--ld-path=${CMAKE_LINKER}"'
require_text scripts/smoke_darwin_release.sh \
  'export PATH="$tool_bin:$PATH"'
require_text scripts/smoke_darwin_release.sh \
  '"--ld-path=${ld}"'
require_text scripts/verify_release_archives.sh \
  'printf '\''%s\n'\'' "--ld-path=$LINKER"'
require_text scripts/verify_release_archives.sh \
  'PATH="$linker_dir:$PATH" "$@"'

reject_text cmake/toolchains/arm64-apple-darwin.cmake \
  'set(_lonejson_darwin_linker_flag "-fuse-ld='
reject_text cmake/package_darwin_smoke_bundle.cmake '-fuse-ld='
reject_text scripts/smoke_darwin_release.sh '-fuse-ld='
reject_text scripts/verify_release_archives.sh '-fuse-ld='
reject_text cmake/package_darwin_smoke_bundle.cmake '-Wno-fuse-ld-path'
reject_text scripts/smoke_darwin_release.sh '-Wno-fuse-ld-path'
reject_text scripts/verify_release_archives.sh '-Wno-fuse-ld-path'

description=$("$repo_root/scripts/cpkt-toolchains.sh" discover arm64-apple-darwin)
if [[ "$description" != *$'status=ready'* ]]; then
  printf 'SKIP osxcross link-route smoke: no complete Darwin toolchain\n'
  exit 0
fi
cc=$(printf '%s\n' "$description" | sed -n 's/^cc=//p')
ld=$(printf '%s\n' "$description" | sed -n 's/^ld=//p')

cat >"$tmp_dir/main.c" <<'EOF'
int main(void) {
  return 0;
}
EOF

tool_bin="$(dirname -- "$ld")"
route_log="$tmp_dir/route.log"
env PATH="$tool_bin:/usr/bin:/bin" "$cc" -### \
  "-mmacosx-version-min=${LONEJSON_MACOS_DEPLOYMENT_TARGET:-15.0}" \
  "--ld-path=$ld" \
  "$tmp_dir/main.c" \
  -o "$tmp_dir/a.out" \
  >"$route_log" 2>&1 || true

if ! grep -F -- "$ld" "$route_log" >/dev/null; then
  printf 'osxcross dry-run did not route through target ld: %s\n' "$ld" >&2
  cat "$route_log" >&2
  exit 1
fi

# Prove the selected route can produce a real target binary warning-clean.
env PATH="$tool_bin:$PATH" "$cc" -Wall -Wextra -Werror -Wl,-fatal_warnings \
  "-mmacosx-version-min=${LONEJSON_MACOS_DEPLOYMENT_TARGET:-15.0}" \
  "--ld-path=$ld" "$tmp_dir/main.c" -o "$tmp_dir/a.out"
otool=$(printf '%s\n' "$description" | sed -n 's/^otool=//p')
"$otool" -hv "$tmp_dir/a.out" | grep -E 'ARM64|arm64' >/dev/null
