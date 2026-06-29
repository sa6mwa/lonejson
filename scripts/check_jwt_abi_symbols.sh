#!/usr/bin/env bash
set -euo pipefail

repo_root=${1:?usage: check_jwt_abi_symbols.sh REPO_ROOT BUILD_DIR TARGET_ID SHARED_LIB STATIC_LIB [CONTEXT]}
build_dir=${2:?usage: check_jwt_abi_symbols.sh REPO_ROOT BUILD_DIR TARGET_ID SHARED_LIB STATIC_LIB [CONTEXT]}
target_id=${3:?usage: check_jwt_abi_symbols.sh REPO_ROOT BUILD_DIR TARGET_ID SHARED_LIB STATIC_LIB [CONTEXT]}
shared_lib=${4:?usage: check_jwt_abi_symbols.sh REPO_ROOT BUILD_DIR TARGET_ID SHARED_LIB STATIC_LIB [CONTEXT]}
static_lib=${5:?usage: check_jwt_abi_symbols.sh REPO_ROOT BUILD_DIR TARGET_ID SHARED_LIB STATIC_LIB [CONTEXT]}
context=${6:-$target_id}

repo_root="$(CDPATH= cd -- "$repo_root" && pwd)"
case "$build_dir" in
  /*) ;;
  *) build_dir="$PWD/$build_dir" ;;
esac
if [[ -d "$build_dir" ]]; then
  build_dir="$(CDPATH= cd -- "$build_dir" && pwd)"
fi

if [[ ! -f "$shared_lib" ]]; then
  printf 'missing shared library for JWT ABI check in %s: %s\n' "$context" "$shared_lib" >&2
  exit 1
fi
if [[ ! -f "$static_lib" ]]; then
  printf 'missing static library for JWT ABI check in %s: %s\n' "$context" "$static_lib" >&2
  exit 1
fi

eval "$("$repo_root/scripts/discover_target_tools.sh" \
  --build-dir "$build_dir" \
  --target-id "$target_id")"

if [[ -z "${NM:-}" ]]; then
  printf 'missing target nm for %s\n' "$target_id" >&2
  exit 1
fi

if [[ "$target_id" == *apple-darwin ]]; then
  shared_symbols="$("$NM" -gU "$shared_lib")"
  static_symbols="$("$NM" -gU "$static_lib")"
  symbol_prefix='_?'
else
  shared_symbols="$("$NM" -D --defined-only "$shared_lib")"
  static_symbols="$("$NM" -g --defined-only "$static_lib")"
  symbol_prefix=''
fi

for symbol in \
    lonejson_jwt_parse_compact \
    lonejson_jwk_parse_json \
    lonejson_jwks_parse_json \
    lonejson_jwks_select \
    lonejson_jwk_cleanup \
    lonejson_jwks_cleanup \
    lonejson_jwt_decode_compact \
    lonejson_jwt_validate_claims \
    lonejson_jwt_header_cleanup \
    lonejson_jwt_claims_cleanup \
    lonejson_oidc_discovery_url \
    lonejson_oidc_discovery_parse_json \
    lonejson_oidc_discovery_validate_issuer \
    lonejson_oidc_discovery_cleanup; do
  symbol_regex="(^|[[:space:]])${symbol_prefix}${symbol}$"
  if ! printf '%s\n' "$shared_symbols" | grep -Eq "$symbol_regex"; then
    printf 'missing lonejson_jwt_* ABI symbol in shared library for %s: %s\n' "$context" "$symbol" >&2
    exit 1
  fi
  if ! printf '%s\n' "$static_symbols" | grep -Eq "$symbol_regex"; then
    printf 'missing lonejson_jwt_* ABI symbol in static library for %s: %s\n' "$context" "$symbol" >&2
    exit 1
  fi
done
