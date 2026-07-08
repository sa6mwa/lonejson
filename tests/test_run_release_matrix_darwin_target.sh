#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
matrix_script_path="$repo_root/scripts/run_release_matrix.sh"
matrix_script="$(cat "$matrix_script_path")"
verify_script_path="$repo_root/scripts/verify_release_archives.sh"
verify_script="$(cat "$verify_script_path")"
darwin_smoke_script_path="$repo_root/cmake/package_darwin_smoke_bundle.cmake"
darwin_smoke_script="$(cat "$darwin_smoke_script_path")"
darwin_release_smoke_script_path="$repo_root/scripts/smoke_darwin_release.sh"
darwin_release_smoke_script="$(cat "$darwin_release_smoke_script_path")"
cmake_lists="$(cat "$repo_root/CMakeLists.txt")"
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

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
  "$darwin_smoke_script_path" >/dev/null
grep -F 'LONEJSON_ABI_VERSION is required for Darwin smoke bundle' \
  "$darwin_smoke_script_path" >/dev/null
if grep -E 'set\(LONEJSON_ABI_VERSION "[0-9]+"\)' \
    "$darwin_smoke_script_path" >/dev/null; then
  printf 'Darwin smoke bundle must not hard-code an ABI fallback\n' >&2
  exit 1
fi
printf '%s\n' "$darwin_smoke_script" | \
  grep -F -- 'lonejson_import_cache_path(LONEJSON_C_PKT_SYSTEMS_ROOT)' >/dev/null
printf '%s\n' "$darwin_smoke_script" | \
  grep -F -- 'LONEJSON_C_PKT_SYSTEMS_ROOT is required for Darwin static smoke link' >/dev/null
printf '%s\n' "$darwin_smoke_script" | \
  grep -F -- '${LONEJSON_C_PKT_SYSTEMS_ROOT}/lib/libcrypto.a' >/dev/null
printf '%s\n' "$darwin_smoke_script" | \
  grep -F -- '"${lonejson_darwin_crypto_static}"' >/dev/null
printf '%s\n' "$darwin_release_smoke_script" | \
  grep -F -- 'cache_value LONEJSON_C_PKT_SYSTEMS_ROOT' >/dev/null
printf '%s\n' "$darwin_release_smoke_script" | \
  grep -F -- 'missing LONEJSON_C_PKT_SYSTEMS_ROOT for Darwin static smoke link' >/dev/null
printf '%s\n' "$darwin_release_smoke_script" | \
  grep -F -- 'crypto_static="${cpkt_root}/lib/libcrypto.a"' >/dev/null
printf '%s\n' "$darwin_release_smoke_script" | \
  grep -F -- '"$crypto_static"' >/dev/null
if grep -F 'liblonejson.4.dylib' \
    "$darwin_smoke_script_path" >/dev/null; then
  printf 'Darwin smoke bundle must not hard-code ABI dylib aliases\n' >&2
  exit 1
fi
if printf '%s\n' "$matrix_script" | grep -F -- 'require_archive_contract' >/dev/null; then
  printf 'run_release_matrix.sh must delegate archive checks to package-verify\n' >&2
  exit 1
fi
printf '%s\n' "$matrix_script" | grep -F -- 'missing c.pkt.systems CURL CMake package' >/dev/null
printf '%s\n' "$matrix_script" | grep -F -- 'missing c.pkt.systems OpenSSL CMake package' >/dev/null
printf '%s\n' "$matrix_script" | grep -F -- '-D LONEJSON_BUILD_WITH_OPENSSL=ON' >/dev/null
printf '%s\n' "$matrix_script" | grep -F -- '-D LONEJSON_BUILD_WITH_JWT=ON' >/dev/null
printf '%s\n' "$matrix_script" | grep -F -- '-D LONEJSON_BUILD_WITH_OIDC=ON' >/dev/null
printf '%s\n' "$matrix_script" | grep -F -- 'run_target linux-gnu-release x86_64-linux-gnu package-archive-linux-gnu full' >/dev/null
printf '%s\n' "$matrix_script" | grep -F -- 'ctest --preset "$preset"' >/dev/null
if printf '%s\n' "$matrix_script" | grep -F -- 'host_policy_ctest_exclude=' >/dev/null; then
  printf 'release matrix must not keep a filtered cross-target CTest replay surface\n' >&2
  exit 1
fi
if printf '%s\n' "$matrix_script" | grep -F -- 'ctest --preset "$preset" -E' >/dev/null; then
  printf 'release matrix must not rerun prerelease CTest subsets for package targets\n' >&2
  exit 1
fi
for non_host_target in \
    'run_target linux-musl-release x86_64-linux-musl package-archive-linux-musl' \
    'run_target aarch64-linux-gnu-release aarch64-linux-gnu package-archive-aarch64-linux-gnu' \
    'run_target aarch64-linux-musl-release aarch64-linux-musl package-archive-aarch64-linux-musl' \
    'run_target armhf-linux-gnu-release armhf-linux-gnu package-archive-armhf-linux-gnu' \
    'run_target armhf-linux-musl-release armhf-linux-musl package-archive-armhf-linux-musl'; do
  printf '%s\n' "$matrix_script" | grep -F -- "$non_host_target" >/dev/null
done
printf '%s\n' "$cmake_lists" | grep -F -- '-DLONEJSON_BUILD_WITH_OPENSSL=${LONEJSON_BUILD_WITH_OPENSSL}' >/dev/null
printf '%s\n' "$cmake_lists" | grep -F -- '-DLONEJSON_BUILD_WITH_JWT=${LONEJSON_BUILD_WITH_JWT}' >/dev/null
printf '%s\n' "$cmake_lists" | grep -F -- '-DLONEJSON_BUILD_WITH_OIDC=${LONEJSON_BUILD_WITH_OIDC}' >/dev/null
printf '%s\n' "$cmake_lists" | grep -F -- 'LINKER:-exported_symbol,_lonejson_*' >/dev/null
printf '%s\n' "$cmake_lists" | grep -F -- 'LINKER:-exported_symbol,_lj_*' >/dev/null
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
if printf '%s\n' "$matrix_script" | grep -F -- '-D OPENSSL_' >/dev/null; then
  printf 'run_release_matrix.sh must not inject raw OPENSSL paths\n' >&2
  exit 1
fi
if printf '%s\n' "$matrix_script" | grep -F -- '-U OPENSSL_' >/dev/null; then
  printf 'run_release_matrix.sh must not clean legacy OPENSSL cache variables\n' >&2
  exit 1
fi

fake_bin="$tmp_dir/bin"
aarch64_musl_prefix="$tmp_dir/aarch64-linux-musl"
armhf_musl_prefix="$tmp_dir/arm-linux-musleabihf"
mkdir -p \
  "$fake_bin" \
  "$aarch64_musl_prefix/bin" \
  "$aarch64_musl_prefix/aarch64-linux-musl/lib" \
  "$armhf_musl_prefix/bin" \
  "$armhf_musl_prefix/arm-linux-musleabihf/lib"
for tool in \
  cmake \
  ctest \
  gzip \
  tar \
  shasum \
  make \
  lua \
  luarocks \
  musl-gcc \
  aarch64-linux-gnu-gcc \
  arm-linux-gnueabihf-gcc \
  qemu-aarch64 \
  qemu-arm; do
  printf '#!/usr/bin/env bash\nexit 0\n' >"$fake_bin/$tool"
  chmod +x "$fake_bin/$tool"
done
for tool in \
  aarch64-linux-musl-gcc \
  aarch64-linux-musl-ar \
  aarch64-linux-musl-ranlib; do
  printf '#!/usr/bin/env bash\nexit 0\n' >"$aarch64_musl_prefix/bin/$tool"
  chmod +x "$aarch64_musl_prefix/bin/$tool"
done
for tool in \
  arm-linux-musleabihf-gcc \
  arm-linux-musleabihf-ar \
  arm-linux-musleabihf-ranlib; do
  printf '#!/usr/bin/env bash\nexit 0\n' >"$armhf_musl_prefix/bin/$tool"
  chmod +x "$armhf_musl_prefix/bin/$tool"
done
touch \
  "$aarch64_musl_prefix/aarch64-linux-musl/lib/ld-musl-aarch64.so.1" \
  "$armhf_musl_prefix/arm-linux-musleabihf/lib/ld-musl-armhf.so.1"

preflight_output="$(PATH="$fake_bin:/usr/bin:/bin" \
  CPKT_AARCH64_MUSL_PREFIX="$aarch64_musl_prefix" \
  CPKT_ARMHF_MUSL_PREFIX="$armhf_musl_prefix" \
  LONEJSON_RELEASE_MATRIX_PREFLIGHT_ONLY=1 \
  "$matrix_script_path")"
[[ "$preflight_output" == "Release matrix preflight completed successfully." ]]
