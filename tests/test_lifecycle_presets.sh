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

preset_block() {
  local target_id=$1
  awk -v name="$target_id-release" '
    $0 ~ "\"name\": \"" name "\"" { active = 1 }
    active { print }
    active && /^    },$/ { exit }
  ' "$presets"
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

while IFS='|' read -r target_id target_arch target_os target_libc; do
  target_preset="$(preset_block "$target_id")"
  grep -F "\"LONEJSON_TARGET_ARCH\": \"$target_arch\"" <<<"$target_preset" >/dev/null
  grep -F "\"LONEJSON_TARGET_OS\": \"$target_os\"" <<<"$target_preset" >/dev/null
  grep -F "\"LONEJSON_TARGET_LIBC\": \"$target_libc\"" <<<"$target_preset" >/dev/null
done <<'EOF'
x86_64-linux-gnu|x86_64|linux|gnu
x86_64-linux-musl|x86_64|linux|musl
aarch64-linux-gnu|aarch64|linux|gnu
aarch64-linux-musl|aarch64|linux|musl
armhf-linux-gnu|armhf|linux|gnu
armhf-linux-musl|armhf|linux|musl
arm64-apple-darwin|arm64|darwin|
EOF

require_text '"name": "package-archive-x86_64-linux-gnu"'
require_text '"configurePreset": "x86_64-linux-gnu-release"'
require_text '"name": "package-archive-x86_64-linux-musl"'
require_text '"configurePreset": "x86_64-linux-musl-release"'

reject_text '"name": "linux-gnu-release"'
reject_text '"name": "linux-musl-release"'
reject_text '"configurePreset": "linux-gnu-release"'
reject_text '"configurePreset": "linux-musl-release"'
reject_text '${sourceDir}/.deps/c.pkt.systems'
