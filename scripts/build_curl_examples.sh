#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
bundle_root="$("${repo_root}/scripts/detect_liblockdc_bundle.sh")"
output_dir="${repo_root}/examples/bin"
bundle_lib_dir="${bundle_root}/lib"

mkdir -p "${output_dir}"

cc \
  -D_POSIX_C_SOURCE=200809L \
  -DLONEJSON_WITH_CURL \
  -std=c89 \
  -Wall \
  -Wextra \
  -Wpedantic \
  -pedantic-errors \
  -I "${repo_root}/include" \
  -I "${bundle_root}/include" \
  -o "${output_dir}/curl_get" \
  "${repo_root}/examples/curl_get.c" \
  "${repo_root}/src/lonejson.c" \
  -L "${bundle_lib_dir}" \
  -Wl,-rpath,"${bundle_lib_dir}" \
  -Wl,-rpath-link,"${bundle_lib_dir}" \
  -lcurl -lssl -lcrypto -lnghttp2 -lssh2 -lz

cc \
  -D_POSIX_C_SOURCE=200809L \
  -DLONEJSON_WITH_CURL \
  -std=c89 \
  -Wall \
  -Wextra \
  -Wpedantic \
  -pedantic-errors \
  -I "${repo_root}/include" \
  -I "${bundle_root}/include" \
  -o "${output_dir}/curl_put" \
  "${repo_root}/examples/curl_put.c" \
  "${repo_root}/src/lonejson.c" \
  -L "${bundle_lib_dir}" \
  -Wl,-rpath,"${bundle_lib_dir}" \
  -Wl,-rpath-link,"${bundle_lib_dir}" \
  -lcurl -lssl -lcrypto -lnghttp2 -lssh2 -lz
