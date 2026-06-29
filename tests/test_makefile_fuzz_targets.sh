#!/usr/bin/env bash
set -euo pipefail

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
require_line 'build/fuzz/corpus/path_value_visitor' \
  'make fuzz does not reference the path value visitor corpus directory'
require_line 'fuzz/corpus/path_value_visitor/.' \
  'make fuzz does not copy the path value visitor seed corpus'
require_line 'artifacts/path_value_visitor' \
  'make fuzz does not prepare path value visitor artifacts'
require_line 'lonejson_fuzz_candidate_stream' \
  'make fuzz does not build/run lonejson_fuzz_candidate_stream'
require_line 'build/fuzz/corpus/candidate_stream' \
  'make fuzz does not reference the candidate stream corpus directory'
require_line 'fuzz/corpus/candidate_stream/.' \
  'make fuzz does not copy the candidate stream seed corpus'
require_line 'artifacts/candidate_stream' \
  'make fuzz does not prepare candidate stream artifacts'
require_line 'lonejson_fuzz_jwt' \
  'make fuzz does not build/run lonejson_fuzz_jwt'
require_line 'build/fuzz/corpus/jwt' \
  'make fuzz does not reference the JWT corpus directory'
require_line 'fuzz/corpus/jwt/.' \
  'make fuzz does not copy the JWT seed corpus'
require_line 'artifacts/jwt' \
  'make fuzz does not prepare JWT artifacts'
require_line 'make lua-fuzz' \
  'make fuzz does not run the Lua binding fuzz smoke'
require_line 'tests/test_lua_fuzz.lua' \
  'make lua-fuzz does not run the Lua randomized binding fuzz script' \
  "$lua_dry_run"
