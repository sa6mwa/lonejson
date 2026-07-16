#!/usr/bin/env bash
set -euo pipefail

repo_root=${1:?usage: test_msan_handle_probes.sh REPO_ROOT}
prelude="$repo_root/src/impl/00_prelude.h"

grep -F '__has_feature(memory_sanitizer)' "$prelude" >/dev/null
grep -F '#include <sanitizer/msan_interface.h>' "$prelude" >/dev/null
grep -F '__msan_test_shadow(ptr, size)' "$prelude" >/dev/null

require_guard() {
  local function_name=$1
  if ! awk -v name="$function_name" '
    $0 ~ name { inside = 1 }
    inside && /lonejson__bytes_are_initialized/ { found = 1 }
    inside && /^}/ { exit found ? 0 : 1 }
    END { if (!inside || !found) exit 1 }
  ' "$prelude"; then
    printf '%s must guard handle probes with lonejson__bytes_are_initialized\n' \
      "$function_name" >&2
    exit 1
  fi
}

require_guard 'lonejson__spooled_is_initialized'
require_guard 'lonejson__json_value_is_initialized'
require_guard 'lonejson__string_array_stream_is_initialized'
require_guard 'lonejson__mapped_array_stream_is_initialized'
require_guard 'lonejson__source_is_initialized'
