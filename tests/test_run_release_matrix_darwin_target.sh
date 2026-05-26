#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
script_path="$repo_root/scripts/run_release_matrix.sh"
script="$(cat "$script_path")"

darwin_block="$(
  sed -n '/if \[ "\${target_id#\*apple-darwin}" != "\$target_id" \]; then/,/else/p' \
    "$script_path"
)"

printf '%s\n' "$darwin_block" | grep -F -- 'darwin_deployment_target="$(target_darwin_deployment_target)"' >/dev/null
printf '%s\n' "$darwin_block" | grep -F -- '-D "LONEJSON_MACOS_DEPLOYMENT_TARGET=$darwin_deployment_target"' >/dev/null
printf '%s\n' "$darwin_block" | grep -F -- '-D "CMAKE_OSX_DEPLOYMENT_TARGET=$darwin_deployment_target"' >/dev/null
printf '%s\n' "$script" | grep -F -- 'target_raw_compile_flags()' >/dev/null
printf '%s\n' "$script" | grep -F -- 'printf '\''%s\n'\'' "-mmacosx-version-min=$(target_darwin_deployment_target)"' >/dev/null
printf '%s\n' "$script" | grep -F -- 'raw_compile_flags="$(target_raw_compile_flags "$target_id")"' >/dev/null
printf '%s\n' "$script" | grep -F -- '"$compiler" "$consumer_source" $raw_compile_flags $pkg_config_flags $raw_link_flags -o "$tmp_dir/pkg-config-consumer"' >/dev/null
