#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
bundle_root="$("${repo_root}/scripts/detect_c_pkt_systems_bundle.sh")"
build_dir="${repo_root}/build/host-curl"
output_dir="${repo_root}/examples/bin"

# These are development executables.  Build them through the same CMake target
# helper as tests, fuzzers, and ordinary examples so native Linux execution
# selects the pinned Bootlin loader and private runtime paths directly.
cmake --preset host-curl -S "${repo_root}" \
  -D "LONEJSON_C_PKT_SYSTEMS_ROOT=${bundle_root}"
cmake --build "${build_dir}" --target example_curl_get example_curl_put
mkdir -p "${output_dir}"
cp "${build_dir}/example_curl_get" "${output_dir}/curl_get"
cp "${build_dir}/example_curl_put" "${output_dir}/curl_put"
