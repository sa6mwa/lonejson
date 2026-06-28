#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
matrix_script_path="$repo_root/scripts/run_release_matrix.sh"
matrix_script="$(cat "$matrix_script_path")"
verify_script_path="$repo_root/scripts/verify_release_archives.sh"
verify_script="$(cat "$verify_script_path")"

printf '%s\n' "$verify_script" | grep -F -- 'darwin_deployment_target="$(target_darwin_deployment_target)"' >/dev/null
printf '%s\n' "$verify_script" | grep -F -- '-D "LONEJSON_MACOS_DEPLOYMENT_TARGET=$darwin_deployment_target"' >/dev/null
printf '%s\n' "$verify_script" | grep -F -- '-D "CMAKE_OSX_DEPLOYMENT_TARGET=$darwin_deployment_target"' >/dev/null
printf '%s\n' "$verify_script" | grep -F -- 'target_raw_compile_flags()' >/dev/null
printf '%s\n' "$verify_script" | grep -F -- 'printf '\''%s\n'\'' "-mmacosx-version-min=$(target_darwin_deployment_target)"' >/dev/null
printf '%s\n' "$verify_script" | grep -F -- 'raw_compile_flags="$(target_raw_compile_flags "$target_id")"' >/dev/null
printf '%s\n' "$verify_script" | grep -F -- 'printf '\''%s\n'\'' "-fuse-ld=$LINKER"' >/dev/null
printf '%s\n' "$verify_script" | grep -F -- 'run_with_target_path "$target_id" "$CC" "$consumer_source" $raw_compile_flags $pkg_config_flags $raw_link_flags -o "$tmp_dir/pkg-config-consumer"' >/dev/null
printf '%s\n' "$verify_script" | grep -F -- 'scripts/discover_target_tools.sh' >/dev/null
printf '%s\n' "$matrix_script" | grep -F -- 'package-darwin-smoke-bundle' >/dev/null
printf '%s\n' "$matrix_script" | grep -F -- 'make package-verify' >/dev/null
grep -F 'liblonejson.${LONEJSON_ABI_VERSION}.dylib' \
  "$repo_root/cmake/package_darwin_smoke_bundle.cmake" >/dev/null
if grep -F 'liblonejson.4.dylib' \
    "$repo_root/cmake/package_darwin_smoke_bundle.cmake" >/dev/null; then
  printf 'Darwin smoke bundle must not hard-code ABI dylib aliases\n' >&2
  exit 1
fi
if printf '%s\n' "$matrix_script" | grep -F -- 'require_archive_contract' >/dev/null; then
  printf 'run_release_matrix.sh must delegate archive checks to package-verify\n' >&2
  exit 1
fi
printf '%s\n' "$matrix_script" | grep -F -- 'missing c.pkt.systems CURL CMake package' >/dev/null
printf '%s\n' "$matrix_script" | grep -F -- '-D LONEJSON_C_PKT_SYSTEMS_ROOT="$bundle_root"' >/dev/null
if printf '%s\n' "$matrix_script" | grep -F -- '-D CURL_LIBRARY_RELEASE=' >/dev/null; then
  printf 'run_release_matrix.sh must not inject raw CURL_LIBRARY_RELEASE paths\n' >&2
  exit 1
fi
if printf '%s\n' "$matrix_script" | grep -F -- '-D CURL_INCLUDE_DIR=' >/dev/null; then
  printf 'run_release_matrix.sh must not inject raw CURL_INCLUDE_DIR paths\n' >&2
  exit 1
fi
if printf '%s\n' "$matrix_script" | grep -F -- '-U CURL_' >/dev/null; then
  printf 'run_release_matrix.sh must not clean legacy CURL cache variables\n' >&2
  exit 1
fi
