#!/usr/bin/env bash
set -euo pipefail

# Rationale: the public Make fuzz target must exercise every maintained fuzzer
# and seed corpus, including Lua fuzz smoke, from one predictable command.

repo_root=$1

dry_run=$(make --no-print-directory -C "$repo_root" -n fuzz FUZZ_TIME=0)
lua_dry_run=$(make --no-print-directory -C "$repo_root" -n lua-fuzz)

require_line() {
  local pattern=$1
  local message=$2
  local output=${3:-$dry_run}

  if ! printf '%s\n' "$output" | grep -F -- "$pattern" >/dev/null; then
    printf '%s\n' "$output" >&2
    printf '%s\n' "$message" >&2
    exit 1
  fi
}

require_line 'lonejson_fuzz_path_value_visitor' \
  'make fuzz does not build/run lonejson_fuzz_path_value_visitor'
require_line 'lonejson_fuzz_base64' \
  'make fuzz does not build/run lonejson_fuzz_base64'
require_line 'scripts/run_afl_fuzz.sh' \
  'make fuzz does not run the pinned AFL++ driver'
require_line 'fuzz/corpus/base64' \
  'make fuzz does not pass the base64 seed corpus to AFL++'
require_line 'fuzz/corpus/path_value_visitor' \
  'make fuzz does not pass the path value visitor corpus to AFL++'
require_line 'lonejson_fuzz_jwt' \
  'make fuzz does not build/run lonejson_fuzz_jwt'
require_line 'fuzz/corpus/jwt' \
  'make fuzz does not pass the JWT corpus to AFL++'
require_line 'make lua-fuzz' \
  'make fuzz does not run the Lua binding fuzz smoke'
require_line 'tests/test_lua_fuzz.lua' \
  'make lua-fuzz does not run the Lua randomized binding fuzz script' \
  "$lua_dry_run"
