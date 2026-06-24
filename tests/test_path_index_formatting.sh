#!/usr/bin/env bash
set -euo pipefail

repo_root=$1

for source_file in \
  "$repo_root/src/impl/13_json_value_visit.h" \
  "$repo_root/src/impl/21_parser_values.h"; do
  if grep -F '(unsigned long)frame->next_index' "$source_file" >/dev/null; then
    printf 'path index formatter truncates size_t through unsigned long: %s\n' \
      "$source_file" >&2
    exit 1
  fi
  if grep -F 'snprintf(frame->index_text' "$source_file" >/dev/null; then
    printf 'path index formatter bypasses size_t-safe helper: %s\n' \
      "$source_file" >&2
    exit 1
  fi
done
