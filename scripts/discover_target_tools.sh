#!/usr/bin/env bash
set -euo pipefail

build_dir=
target_id=

usage() {
  printf 'usage: %s --build-dir DIR --target-id TARGET\n' "$0" >&2
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      if [[ $# -lt 2 ]]; then
        usage
        exit 1
      fi
      build_dir=$2
      shift 2
      ;;
    --target-id)
      if [[ $# -lt 2 ]]; then
        usage
        exit 1
      fi
      target_id=$2
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      usage
      exit 1
      ;;
  esac
done

if [[ -z "$build_dir" || -z "$target_id" ]]; then
  usage
  exit 1
fi

cache_file="$build_dir/CMakeCache.txt"

cache_value() {
  local name=$1
  if [[ -f "$cache_file" ]]; then
    sed -n "s/^${name}:[^=]*=//p" "$cache_file" | tail -n 1
  fi
}

target_default_compiler() {
  case "$target_id" in
    x86_64-linux-gnu) printf '%s\n' cc ;;
    x86_64-linux-musl) printf '%s\n' musl-gcc ;;
    aarch64-linux-gnu) printf '%s\n' aarch64-linux-gnu-gcc ;;
    aarch64-linux-musl) printf '%s\n' aarch64-linux-musl-gcc ;;
    armhf-linux-gnu) printf '%s\n' arm-linux-gnueabihf-gcc ;;
    armhf-linux-musl) printf '%s\n' arm-linux-musleabihf-gcc ;;
    arm64-apple-darwin)
      printf '%s\n' "${OSXCROSS_ROOT:-$HOME/.local/cross/osxcross}/bin/${CPKT_OSXCROSS_HOST:-arm64-apple-darwin25}-clang"
      ;;
    *)
      printf 'unknown target id: %s\n' "$target_id" >&2
      exit 1
      ;;
  esac
}

first_executable() {
  local candidate
  for candidate in "$@"; do
    [[ -n "$candidate" ]] || continue
    if [[ "$candidate" == */* ]]; then
      if [[ -x "$candidate" ]]; then
        printf '%s\n' "$candidate"
        return 0
      fi
    elif command -v "$candidate" >/dev/null 2>&1; then
      command -v "$candidate"
      return 0
    fi
  done
  return 1
}

tool_prefix() {
  case "$target_id" in
    x86_64-linux-gnu) printf '%s\n' "" ;;
    x86_64-linux-musl) printf '%s\n' "" ;;
    aarch64-linux-gnu) printf '%s\n' "aarch64-linux-gnu-" ;;
    aarch64-linux-musl) printf '%s\n' "aarch64-linux-musl-" ;;
    armhf-linux-gnu) printf '%s\n' "arm-linux-gnueabihf-" ;;
    armhf-linux-musl) printf '%s\n' "arm-linux-musleabihf-" ;;
    arm64-apple-darwin) printf '%s\n' "${CPKT_OSXCROSS_HOST:-arm64-apple-darwin25}-" ;;
  esac
}

target_host_prefix() {
  local prefix_value
  prefix_value="$(tool_prefix)"
  printf '%s\n' "${prefix_value%-}"
}

target_allows_unprefixed_path_tool() {
  case "$target_id" in
    *apple-darwin) return 1 ;;
    *) return 0 ;;
  esac
}

cc="$(cache_value CMAKE_C_COMPILER)"
if [[ -z "$cc" ]]; then
  cc="$(target_default_compiler)"
fi
cc="$(first_executable "$cc" || true)"
if [[ -z "$cc" ]]; then
  printf 'target compiler unavailable for %s\n' "$target_id" >&2
  exit 1
fi

cc_dir=
if [[ "$cc" == */* ]]; then
  cc_dir="$(CDPATH= cd -- "$(dirname -- "$cc")" && pwd)"
fi
prefix="$(tool_prefix)"
host_prefix="$(target_host_prefix)"

configured_strip="$(cache_value CMAKE_STRIP)"
configured_linker="$(cache_value CMAKE_LINKER)"
configured_ar="$(cache_value CMAKE_AR)"
configured_nm="$(cache_value CMAKE_NM)"
configured_install_name_tool="$(cache_value CMAKE_INSTALL_NAME_TOOL)"
configured_otool="$(cache_value CMAKE_OTOOL)"
configured_readelf="$(cache_value CMAKE_READELF)"
if [[ -z "$configured_otool" ]]; then
  configured_otool="${CPKT_OTOOL:-}"
fi

strip_tool="$(first_executable \
  "${LONEJSON_STRIP:-}" \
  "$configured_strip" \
  "${cc_dir:+$cc_dir/${prefix}strip}" \
  "${cc_dir:+$cc_dir/strip}" \
  "${prefix}strip" || true)"
if [[ -z "$strip_tool" ]] && target_allows_unprefixed_path_tool; then
  strip_tool="$(first_executable strip || true)"
fi
linker_tool="$(first_executable \
  "${LONEJSON_LINKER:-}" \
  "$configured_linker" \
  "${cc_dir:+$cc_dir/${prefix}ld}" \
  "${cc_dir:+$cc_dir/ld}" \
  "${prefix}ld" || true)"
if [[ -z "$linker_tool" ]] && target_allows_unprefixed_path_tool; then
  linker_tool="$(first_executable ld || true)"
fi
ar_tool="$(first_executable \
  "${LONEJSON_AR:-}" \
  "$configured_ar" \
  "${cc_dir:+$cc_dir/${prefix}ar}" \
  "${cc_dir:+$cc_dir/ar}" \
  "${prefix}ar" || true)"
if [[ -z "$ar_tool" ]] && target_allows_unprefixed_path_tool; then
  ar_tool="$(first_executable ar || true)"
fi
nm_tool="$(first_executable \
  "${LONEJSON_NM:-}" \
  "$configured_nm" \
  "${cc_dir:+$cc_dir/${prefix}nm}" \
  "${cc_dir:+$cc_dir/nm}" \
  "${prefix}nm" || true)"
if [[ -z "$nm_tool" ]] && target_allows_unprefixed_path_tool; then
  nm_tool="$(first_executable nm || true)"
fi
readelf_tool="$(first_executable \
  "${LONEJSON_READELF:-}" \
  "$configured_readelf" \
  "${cc_dir:+$cc_dir/${prefix}readelf}" \
  "${cc_dir:+$cc_dir/readelf}" \
  "${prefix}readelf" \
  readelf || true)"

otool_tool=
install_name_tool=
if [[ "$target_id" == *apple-darwin ]]; then
  otool_tool="$(first_executable \
    "${LONEJSON_OTOOL:-}" \
    "$configured_otool" \
    "${cc_dir:+$cc_dir/${prefix}otool}" \
    "${cc_dir:+$cc_dir/otool}" \
    "${prefix}otool" || true)"
  install_name_tool="$(first_executable \
    "${LONEJSON_INSTALL_NAME_TOOL:-}" \
    "$configured_install_name_tool" \
    "${cc_dir:+$cc_dir/${prefix}install_name_tool}" \
    "${cc_dir:+$cc_dir/install_name_tool}" \
    "${prefix}install_name_tool" || true)"
fi

quote_assignment() {
  local key=$1
  local value=$2
  printf '%s=%q\n' "$key" "$value"
}

quote_assignment TARGET_ID "$target_id"
quote_assignment TARGET_HOST_PREFIX "$host_prefix"
quote_assignment TARGET_TOOL_PREFIX "$prefix"
quote_assignment CC "$cc"
quote_assignment LINKER "$linker_tool"
quote_assignment AR "$ar_tool"
quote_assignment STRIP "$strip_tool"
quote_assignment NM "$nm_tool"
quote_assignment READELF "$readelf_tool"
quote_assignment OTOOL "$otool_tool"
quote_assignment INSTALL_NAME_TOOL "$install_name_tool"
