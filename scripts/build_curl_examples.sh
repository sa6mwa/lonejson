#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
bundle_root="$("${repo_root}/scripts/detect_c_pkt_systems_bundle.sh")"
output_dir="${repo_root}/examples/bin"
bundle_lib_dir="${bundle_root}/lib"
cc_bin="${CC:-cc}"
uname_bin="${UNAME:-uname}"

mkdir -p "${output_dir}"

link_flags=(-L "${bundle_lib_dir}" -Wl,-rpath,"${bundle_lib_dir}")
case "$("${uname_bin}" -s)" in
  Darwin) ;;
  *) link_flags+=(-Wl,-rpath-link,"${bundle_lib_dir}") ;;
esac

build_example() {
  local name=$1

  "${cc_bin}" \
    -D_POSIX_C_SOURCE=200809L \
    -DLONEJSON_WITH_CURL \
    -std=c89 \
    -Wall \
    -Wextra \
    -Wpedantic \
    -pedantic-errors \
    -I "${repo_root}/include" \
    -I "${bundle_root}/include" \
    -o "${output_dir}/${name}" \
    "${repo_root}/examples/${name}.c" \
    "${repo_root}/src/lonejson.c" \
    "${link_flags[@]}" \
    -lcurl -lssl -lcrypto -lnghttp2 -lssh2 -lz
}

build_example curl_get
build_example curl_put
