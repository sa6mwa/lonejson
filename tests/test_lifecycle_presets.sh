#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
presets="$repo_root/CMakePresets.json"

require_text() {
  local text=$1
  if ! grep -F -- "$text" "$presets" >/dev/null; then
    printf 'missing lifecycle preset contract: %s\n' "$text" >&2
    exit 1
  fi
}

reject_text() {
  local text=$1
  if grep -F -- "$text" "$presets" >/dev/null; then
    printf 'obsolete lifecycle preset contract remains: %s\n' "$text" >&2
    exit 1
  fi
}

for target_id in \
  x86_64-linux-gnu \
  x86_64-linux-musl \
  aarch64-linux-gnu \
  aarch64-linux-musl \
  armhf-linux-gnu \
  armhf-linux-musl \
  arm64-apple-darwin; do
  require_text '"name": "'"$target_id"'-release"'
  require_text '"LONEJSON_TARGET_ID": "'"$target_id"'"'
  require_text '"LONEJSON_DIST_DIR": "${sourceDir}/dist"'
  require_text '"LONEJSON_C_PKT_SYSTEMS_ROOT": "${sourceDir}/.cache/c.pkt.systems/'"$target_id"'/root"'
done

require_text '"name": "package-archive-x86_64-linux-gnu"'
require_text '"configurePreset": "x86_64-linux-gnu-release"'
require_text '"name": "package-archive-x86_64-linux-musl"'
require_text '"configurePreset": "x86_64-linux-musl-release"'

reject_text '"name": "linux-gnu-release"'
reject_text '"name": "linux-musl-release"'
reject_text '"configurePreset": "linux-gnu-release"'
reject_text '"configurePreset": "linux-musl-release"'
reject_text '${sourceDir}/.deps/c.pkt.systems'
