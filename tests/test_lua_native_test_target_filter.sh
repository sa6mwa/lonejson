#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
cmake_cmd=$2
cmake_generator=$3

lua_exec="$(command -v lua || true)"
luarocks_exec="$(command -v luarocks || true)"
if [ -z "$lua_exec" ] || [ -z "$luarocks_exec" ]; then
  printf 'skipping Lua native test target filter: lua or luarocks not found\n'
  exit 0
fi

tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/lonejson-lua-native-filter.XXXXXX")"
trap 'rm -rf "$tmp_dir"' EXIT

configure_target() {
  local build_dir=$1
  local libc=$2

  "$cmake_cmd" -S "$repo_root" -B "$build_dir" -G "$cmake_generator" \
    -D LONEJSON_TARGET_OS=linux \
    -D "LONEJSON_TARGET_LIBC=$libc" \
    -D "LONEJSON_LUA_EXECUTABLE=$lua_exec" \
    -D "LONEJSON_LUAROCKS_EXECUTABLE=$luarocks_exec" \
    -D LONEJSON_BUILD_TESTS=ON >/dev/null
}

list_tests() {
  local build_dir=$1

  ctest --test-dir "$build_dir" -N
}

assert_has_native_lua_tests() {
  local tests=$1

  printf '%s\n' "$tests" | grep -F 'lonejson_bench_baseline_history_tests' >/dev/null
  printf '%s\n' "$tests" | grep -F 'lonejson_bench_retry_confirm_tests' >/dev/null
  printf '%s\n' "$tests" | grep -F 'lonejson_lua_legacy_uservalue_tests' >/dev/null
  printf '%s\n' "$tests" | grep -F 'lonejson_lua_schema_cache_tests' >/dev/null
  printf '%s\n' "$tests" | grep -F 'lonejson_lua_encode_stats_tests' >/dev/null
  printf '%s\n' "$tests" | grep -F 'lonejson_lua_external_liblonejson_tests' >/dev/null
  printf '%s\n' "$tests" | grep -F 'lonejson_lua_rock_makefile_deps_tests' >/dev/null
}

assert_lacks_native_lua_tests() {
  local tests=$1

  if printf '%s\n' "$tests" | grep -F 'lonejson_bench_baseline_history_tests' >/dev/null; then
    printf 'bench baseline Lua workflow tests must not run for non-host libc targets\n' >&2
    exit 1
  fi
  if printf '%s\n' "$tests" | grep -F 'lonejson_bench_retry_confirm_tests' >/dev/null; then
    printf 'bench retry Lua workflow tests must not run for non-host libc targets\n' >&2
    exit 1
  fi
  if printf '%s\n' "$tests" | grep -F 'lonejson_lua_legacy_uservalue_tests' >/dev/null; then
    printf 'legacy Lua native tests must not run for non-host libc targets\n' >&2
    exit 1
  fi
  if printf '%s\n' "$tests" | grep -F 'lonejson_lua_schema_cache_tests' >/dev/null; then
    printf 'schema-cache Lua native tests must not run for non-host libc targets\n' >&2
    exit 1
  fi
  if printf '%s\n' "$tests" | grep -F 'lonejson_lua_encode_stats_tests' >/dev/null; then
    printf 'encode-stats Lua native tests must not run for non-host libc targets\n' >&2
    exit 1
  fi
  if printf '%s\n' "$tests" | grep -F 'lonejson_lua_external_liblonejson_tests' >/dev/null; then
    printf 'external liblonejson Lua native tests must not run for non-host libc targets\n' >&2
    exit 1
  fi
  if printf '%s\n' "$tests" | grep -F 'lonejson_lua_rock_makefile_deps_tests' >/dev/null; then
    printf 'Lua rock Makefile dependency tests must not run for non-host libc targets\n' >&2
    exit 1
  fi
}

configure_target "$tmp_dir/gnu" gnu
gnu_tests="$(list_tests "$tmp_dir/gnu")"
assert_has_native_lua_tests "$gnu_tests"

configure_target "$tmp_dir/musl" musl
musl_tests="$(list_tests "$tmp_dir/musl")"
assert_lacks_native_lua_tests "$musl_tests"
