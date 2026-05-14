#!/usr/bin/env bash

set -eu

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
preset="${1:-arm64-apple-darwin-release}"
target_host="${LONEJSON_OSXCROSS_HOST:-arm64-apple-darwin25}"
deployment_target="${LONEJSON_MACOS_DEPLOYMENT_TARGET:-15.0}"
osxcross_root="${OSXCROSS_ROOT:-$HOME/.local/cross/osxcross}"
tool_bin="${osxcross_root}/bin"
cc="${tool_bin}/${target_host}-clang"
ld="${tool_bin}/${target_host}-ld"
otool="${tool_bin}/${target_host}-otool"
ar="${tool_bin}/${target_host}-ar"
build_dir="${repo_root}/build/${preset}"
version="$("${repo_root}/scripts/release_version.sh")"
archive="${repo_root}/dist/liblonejson-${version}-arm64-apple-darwin.tar.gz"
smoke_dir="${build_dir}/darwin-smoke"

require_file() {
    if [ ! -e "$1" ]; then
        printf 'missing required file: %s\n' "$1" >&2
        exit 1
    fi
}

require_file "$cc"
require_file "$ld"
require_file "$otool"
require_file "$ar"
require_file "$archive"

dylib="$(find "$build_dir" -maxdepth 1 -type f -name 'liblonejson*.dylib' | sort | head -n 1)"
static_lib="${build_dir}/liblonejson.a"
if [ -z "$dylib" ]; then
    printf 'missing Darwin shared library in %s\n' "$build_dir" >&2
    exit 1
fi
require_file "$static_lib"

if ! "$otool" -hv "$dylib" | grep -Eq 'ARM64|arm64'; then
    printf 'Darwin shared library is not arm64: %s\n' "$dylib" >&2
    "$otool" -hv "$dylib" >&2 || true
    exit 1
fi

if "$otool" -l "$dylib" | grep -Eq '(\$ORIGIN|RUNPATH|LC_RPATH|@loader_path|@executable_path)'; then
    printf 'Darwin shared library contains an unexpected runtime search path marker: %s\n' "$dylib" >&2
    "$otool" -l "$dylib" >&2 || true
    exit 1
fi

install_name="$("$otool" -D "$dylib" | tail -n +2)"
if printf '%s\n' "$install_name" | grep -Eq '(^/|/home/|/build/|\$ORIGIN|@loader_path|@executable_path)'; then
    printf 'Darwin shared library install name contains an unexpected path: %s\n' "$dylib" >&2
    "$otool" -D "$dylib" >&2 || true
    exit 1
fi

if ! "$ar" -t "$static_lib" | grep -q '\.c\.o$'; then
    printf 'Darwin static archive does not contain object files: %s\n' "$static_lib" >&2
    exit 1
fi

cmake -E rm -rf "$smoke_dir"
cmake -E make_directory "$smoke_dir/extract" "$smoke_dir/bin"
tar -xzf "$archive" -C "$smoke_dir/extract"
package_root="$(find "$smoke_dir/extract" -mindepth 1 -maxdepth 1 -type d | sort | head -n 1)"
if [ -z "$package_root" ]; then
    printf 'failed to extract Darwin release archive: %s\n' "$archive" >&2
    exit 1
fi

require_file "${package_root}/include/lonejson.h"
require_file "${package_root}/lib/liblonejson.a"
packaged_dylib="$(find "${package_root}/lib" -maxdepth 1 -type f -name 'liblonejson*.dylib' | sort | head -n 1)"
if [ -z "$packaged_dylib" ]; then
    printf 'Darwin release archive does not contain a shared library\n' >&2
    exit 1
fi

"$cc" -std=c89 -Wall -Wextra -Werror -I "${package_root}/include" \
    "-mmacosx-version-min=${deployment_target}" "--ld-path=${ld}" \
    "${repo_root}/tests/test_link_consumer.c" \
    "${package_root}/lib/liblonejson.a" \
    -o "${smoke_dir}/bin/static-link-smoke"
"$cc" -std=c89 -Wall -Wextra -Werror -I "${package_root}/include" \
    "-mmacosx-version-min=${deployment_target}" "--ld-path=${ld}" \
    "${repo_root}/tests/test_link_consumer.c" "$packaged_dylib" \
    -o "${smoke_dir}/bin/shared-link-smoke"

if ! "$otool" -hv "${smoke_dir}/bin/static-link-smoke" | grep -Eq 'ARM64|arm64'; then
    printf 'Darwin static link smoke executable is not arm64\n' >&2
    exit 1
fi
if ! "$otool" -hv "${smoke_dir}/bin/shared-link-smoke" | grep -Eq 'ARM64|arm64'; then
    printf 'Darwin shared link smoke executable is not arm64\n' >&2
    exit 1
fi

printf 'Darwin release smoke passed for %s\n' "$preset"
