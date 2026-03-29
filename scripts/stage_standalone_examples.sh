#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${repo_root}/build/debug"
output_dir="${repo_root}/examples/bin"

examples=(
  parse_string
  parse_file
  parse_reader
  push_parser
  spooled_text
  spooled_bytes
  source_text
  source_bytes
  serialize_string
  serialize_file
  serialize_jsonl
  fixed_storage
)

mkdir -p "${output_dir}"

for example_name in "${examples[@]}"; do
  source_path="${build_dir}/example_${example_name}"
  if [ ! -x "${source_path}" ]; then
    printf '%s\n' "missing built example: ${source_path}" >&2
    exit 1
  fi
  cp "${source_path}" "${output_dir}/${example_name}"
done
