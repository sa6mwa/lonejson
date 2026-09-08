#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
bundle_root="$("${repo_root}/scripts/detect_c_pkt_systems_bundle.sh")"
output_dir="${repo_root}/examples/bin"
bundle_lib_dir="${bundle_root}/lib"
target_id="${LONEJSON_C_PKT_SYSTEMS_TARGET_ID:-}"
if [[ -z "$target_id" ]]; then
  target_id="$("${repo_root}/scripts/detect_native_target.sh")"
fi
toolchain_env="$("${repo_root}/scripts/cpkt-toolchains.sh" env "$target_id")"
eval "$toolchain_env"
cc_bin="$CC"
pkg_config_bin="${PKG_CONFIG:-pkg-config}"

mkdir -p "${output_dir}"

if ! command -v "${pkg_config_bin}" >/dev/null 2>&1; then
  printf '%s\n' "missing pkg-config; install it or set PKG_CONFIG" >&2
  exit 1
fi

pkg_config_path="${bundle_root}/lib/pkgconfig"
if [[ -n "${PKG_CONFIG_PATH:-}" ]]; then
  pkg_config_path="${pkg_config_path}:${PKG_CONFIG_PATH}"
fi
if ! PKG_CONFIG_PATH="${pkg_config_path}" "${pkg_config_bin}" --exists libcurl; then
  printf '%s\n' "missing libcurl.pc under ${bundle_root}/lib/pkgconfig" >&2
  exit 1
fi

curl_cflags=()
curl_libs=()
read -r -a curl_cflags <<<"$(PKG_CONFIG_PATH="${pkg_config_path}" "${pkg_config_bin}" --cflags libcurl)"
read -r -a curl_libs <<<"$(PKG_CONFIG_PATH="${pkg_config_path}" "${pkg_config_bin}" --libs --static libcurl)"

rpath_flags=(-Wl,-rpath,"${bundle_lib_dir}")
case "$target_id" in
  arm64-apple-darwin) rpath_flags+=(-Wl,-fatal_warnings) ;;
  *) rpath_flags+=(-Wl,-rpath-link,"${bundle_lib_dir}" -Wl,--fatal-warnings) ;;
esac

target_flags=()
if [[ "$target_id" == arm64-apple-darwin ]]; then
  target_flags+=(-arch arm64)
  if [[ "${CPKT_TOOLCHAIN_SOURCE:-}" == apple ]]; then
    target_flags+=(-isysroot "$CPKT_SYSROOT")
  fi
fi

build_example() {
  local name=$1

  "${cc_bin}" \
    "${target_flags[@]}" \
    -D_POSIX_C_SOURCE=200809L \
    -DLONEJSON_WITH_CURL \
    -std=c89 \
    -Wall \
    -Wextra \
    -Werror \
    -Wpedantic \
    -pedantic-errors \
    -I "${repo_root}/include" \
    "${curl_cflags[@]}" \
    -o "${output_dir}/${name}" \
    "${repo_root}/examples/${name}.c" \
    "${repo_root}/src/lonejson.c" \
    "${rpath_flags[@]}" \
    "${curl_libs[@]}"
}

build_example curl_get
build_example curl_put
