#!/usr/bin/env bash

set -eu

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        printf 'missing required command: %s\n' "$1" >&2
        exit 1
    fi
}

require_file() {
    if [ ! -e "$1" ]; then
        printf 'missing required file: %s\n' "$1" >&2
        exit 1
    fi
}

require_linux_origin_rpath() {
    archive="$1"
    dynamic_metadata="$2"

    rpath_values="$(printf '%s\n' "$dynamic_metadata" |
        sed -n 's/.*\(RUNPATH\|RPATH\).*: \[\(.*\)\].*/\2/p')"
    if [ -z "$rpath_values" ]; then
        return
    fi
    old_ifs="$IFS"
    IFS=':'
    for rpath_value in $rpath_values; do
        if [ "$rpath_value" != '$ORIGIN' ] &&
           [ "${rpath_value#'$ORIGIN'/}" = "$rpath_value" ]; then
            IFS="$old_ifs"
            printf 'non-origin rpath in %s: %s\n' "$archive" "$rpath_value" >&2
            exit 1
        fi
    done
    IFS="$old_ifs"
}

release_archive_path() {
    target_id="$1"

    printf '%s\n' "$repo_root/dist/liblonejson-$("$repo_root/scripts/release_version.sh")-$target_id.tar.gz"
}

require_archive_contract() {
    archive="$1"
    target_id="$2"

    tar -tzf "$archive" | while IFS= read -r entry; do
        entry_without_root="${entry#*/}"
        case "$entry_without_root" in
            "" | \
            "include/" | \
            "include/lonejson.h" | \
            "lib/" | \
            "lib/liblonejson.a" | \
            "lib/liblonejson.so" | \
            "lib/liblonejson.so."* | \
            "lib/liblonejson.dylib" | \
            "lib/liblonejson."*".dylib" | \
            "share/" | \
            "share/doc/" | \
            "share/doc/liblonejson/" | \
            "share/doc/liblonejson/LICENSE" | \
            "share/doc/liblonejson/README.md")
                ;;
            *)
                printf 'unexpected file in release archive %s: %s\n' "$archive" "$entry_without_root" >&2
                exit 1
                ;;
        esac
    done

    tmp_dir="$(mktemp -d)"
    trap 'rm -rf "$tmp_dir"' EXIT
    tar -xzf "$archive" -C "$tmp_dir"
    package_root="$(find "$tmp_dir" -mindepth 1 -maxdepth 1 -type d | sort | head -n 1)"
    if [ -z "$package_root" ]; then
        printf 'failed to inspect archive: %s\n' "$archive" >&2
        exit 1
    fi

    if [ "${target_id#*apple-darwin}" != "$target_id" ]; then
        shared_lib="$(find "$package_root/lib" -maxdepth 1 -type f -name 'liblonejson*.dylib' | sort | head -n 1)"
        if [ -z "$shared_lib" ]; then
            printf 'missing packaged shared library in %s\n' "$archive" >&2
            exit 1
        fi
        otool_bin="${OSXCROSS_ROOT:-$HOME/.local/cross/osxcross}/bin/arm64-apple-darwin25-otool"
        require_command "$otool_bin"
        dynamic_metadata="$("$otool_bin" -L "$shared_lib"; "$otool_bin" -l "$shared_lib")"
        case "$dynamic_metadata" in
            *libcurl* | *c.pkt.systems* | *".deps/"* | *"$repo_root"* | *"/home/"* | *"/build/"*)
                printf 'forbidden dependency or path leak in %s\n' "$archive" >&2
                exit 1
                ;;
        esac
        rpath_paths="$(printf '%s\n' "$dynamic_metadata" | sed -n 's/^[[:space:]]*path \([^ ]*\).*/\1/p')"
        if [ -n "$rpath_paths" ] && printf '%s\n' "$rpath_paths" | grep -Ev '^@loader_path(/|$)' >/dev/null; then
            printf 'non-loader-relative rpath in %s\n' "$archive" >&2
            exit 1
        fi
    else
        shared_lib="$(find "$package_root/lib" -maxdepth 1 -type f -name 'liblonejson.so*' ! -type l | sort | head -n 1)"
        if [ -z "$shared_lib" ]; then
            printf 'missing packaged shared library in %s\n' "$archive" >&2
            exit 1
        fi
        dynamic_metadata="$(readelf -d "$shared_lib")"
        case "$dynamic_metadata" in
            *libcurl* | *c.pkt.systems* | *".deps/"* | *"$repo_root"* | *"/home/"* | *"/build/"*)
                printf 'forbidden dependency or path leak in %s\n' "$archive" >&2
                exit 1
                ;;
        esac
        require_linux_origin_rpath "$archive" "$dynamic_metadata"
    fi

    rm -rf "$tmp_dir"
    trap - EXIT
}

require_curl_symbol() {
    archive="$1"
    target_id="$2"

    tmp_dir="$(mktemp -d)"
    trap 'rm -rf "$tmp_dir"' EXIT
    tar -xzf "$archive" -C "$tmp_dir"
    package_root="$(find "$tmp_dir" -mindepth 1 -maxdepth 1 -type d | sort | head -n 1)"
    if [ -z "$package_root" ]; then
        printf 'failed to inspect archive: %s\n' "$archive" >&2
        exit 1
    fi

    if [ "${target_id#*apple-darwin}" != "$target_id" ]; then
        shared_lib="$(find "$package_root/lib" -maxdepth 1 -type f -name 'liblonejson*.dylib' | sort | head -n 1)"
        if [ -z "$shared_lib" ]; then
            printf 'missing packaged shared library in %s\n' "$archive" >&2
            exit 1
        fi
        nm_tool="${OSXCROSS_ROOT:-$HOME/.local/cross/osxcross}/bin/arm64-apple-darwin25-nm"
        require_command "$nm_tool"
        if ! "$nm_tool" -gU "$shared_lib" | grep -Eq '(^|[[:space:]])_?lonejson_curl_parse_init$'; then
            printf 'missing lonejson_curl_* ABI symbol in %s\n' "$archive" >&2
            exit 1
        fi
    else
        shared_lib="$(find "$package_root/lib" -maxdepth 1 -type f -name 'liblonejson.so*' ! -type l | sort | head -n 1)"
        if [ -z "$shared_lib" ]; then
            printf 'missing packaged shared library in %s\n' "$archive" >&2
            exit 1
        fi
        if ! nm -D --defined-only "$shared_lib" | grep -Eq '(^|[[:space:]])lonejson_curl_parse_init$'; then
            printf 'missing lonejson_curl_* ABI symbol in %s\n' "$archive" >&2
            exit 1
        fi
    fi

    rm -rf "$tmp_dir"
    trap - EXIT
}

configure_release_target() {
    preset="$1"
    target_id="$2"

    bundle_root="$repo_root/.deps/c.pkt.systems/$target_id/root"
    curl_library="$(find "$bundle_root/lib" -maxdepth 1 -type f \( -name 'libcurl.so*' -o -name 'libcurl.*.dylib' -o -name 'libcurl.dylib' \) | sort | head -n 1)"
    if [ -z "$curl_library" ]; then
        printf 'missing c.pkt.systems libcurl for %s under %s\n' "$target_id" "$bundle_root/lib" >&2
        exit 1
    fi

    cmake --preset "$preset" \
        -U CURL_DIR \
        -U CURL_INCLUDE_DIR \
        -U CURL_LIBRARY_DEBUG \
        -U CURL_LIBRARY_RELEASE \
        -D LONEJSON_BUILD_WITH_CURL=ON \
        -D CMAKE_PREFIX_PATH="$bundle_root" \
        -D CURL_INCLUDE_DIR="$bundle_root/include" \
        -D CURL_LIBRARY_RELEASE="$curl_library"
}

run_target() {
    preset="$1"
    target_id="$2"
    package_preset="$3"

    printf '\n== %s ==\n' "$preset"
    configure_release_target "$preset" "$target_id"
    cmake --build --preset "$preset"
    ctest --preset "$preset"
    cmake --build --preset "$package_preset"
    archive="$(release_archive_path "$target_id")"
    require_archive_contract "$archive" "$target_id"
    require_curl_symbol "$archive" "$target_id"
}

run_build_only_target() {
    preset="$1"
    target_id="$2"
    package_preset="$3"

    printf '\n== %s ==\n' "$preset"
    configure_release_target "$preset" "$target_id"
    cmake --build --preset "$preset"
    cmake --build --preset "$package_preset"
    archive="$(release_archive_path "$target_id")"
    require_archive_contract "$archive" "$target_id"
    require_curl_symbol "$archive" "$target_id"
}

require_command cmake
require_command ctest
require_command make
require_command nm
require_command readelf
require_command luarocks
require_command musl-gcc
require_command aarch64-linux-gnu-gcc
require_command arm-linux-gnueabihf-gcc
require_command aarch64-linux-musl-gcc
require_command arm-linux-musleabihf-gcc
require_command qemu-aarch64
require_command qemu-arm

require_file "$HOME/.local/cross/aarch64-linux-musl/aarch64-linux-musl/lib/ld-musl-aarch64.so.1"
require_file "$HOME/.local/cross/arm-linux-musleabihf/arm-linux-musleabihf/lib/ld-musl-armhf.so.1"

darwin_toolchain="${OSXCROSS_ROOT:-$HOME/.local/cross/osxcross}/bin/arm64-apple-darwin25-clang"

cd "$repo_root"

cmake --preset host
cmake --build --preset package-clean-dist

cmake -D LONEJSON_SOURCE_DIR="$repo_root" -D LONEJSON_C_PKT_SYSTEMS_TARGET_ID=x86_64-linux-gnu -P cmake/fetch_c_pkt_systems.cmake
cmake -D LONEJSON_SOURCE_DIR="$repo_root" -D LONEJSON_C_PKT_SYSTEMS_TARGET_ID=x86_64-linux-musl -P cmake/fetch_c_pkt_systems.cmake
cmake -D LONEJSON_SOURCE_DIR="$repo_root" -D LONEJSON_C_PKT_SYSTEMS_TARGET_ID=aarch64-linux-gnu -P cmake/fetch_c_pkt_systems.cmake
cmake -D LONEJSON_SOURCE_DIR="$repo_root" -D LONEJSON_C_PKT_SYSTEMS_TARGET_ID=aarch64-linux-musl -P cmake/fetch_c_pkt_systems.cmake
cmake -D LONEJSON_SOURCE_DIR="$repo_root" -D LONEJSON_C_PKT_SYSTEMS_TARGET_ID=armhf-linux-gnu -P cmake/fetch_c_pkt_systems.cmake
cmake -D LONEJSON_SOURCE_DIR="$repo_root" -D LONEJSON_C_PKT_SYSTEMS_TARGET_ID=armhf-linux-musl -P cmake/fetch_c_pkt_systems.cmake
run_target linux-gnu-release x86_64-linux-gnu package-archive-linux-gnu
run_target linux-musl-release x86_64-linux-musl package-archive-linux-musl
run_target aarch64-linux-gnu-release aarch64-linux-gnu package-archive-aarch64-linux-gnu
run_target aarch64-linux-musl-release aarch64-linux-musl package-archive-aarch64-linux-musl
run_target armhf-linux-gnu-release armhf-linux-gnu package-archive-armhf-linux-gnu
run_target armhf-linux-musl-release armhf-linux-musl package-archive-armhf-linux-musl
if [ -x "$darwin_toolchain" ]; then
    cmake -D LONEJSON_SOURCE_DIR="$repo_root" -D LONEJSON_C_PKT_SYSTEMS_TARGET_ID=arm64-apple-darwin -P cmake/fetch_c_pkt_systems.cmake
    run_build_only_target arm64-apple-darwin-release arm64-apple-darwin package-archive-arm64-apple-darwin
    ./scripts/smoke_darwin_release.sh arm64-apple-darwin-release
    cmake --build --preset arm64-apple-darwin-release --target package-darwin-smoke-bundle
else
    printf '\n== arm64-apple-darwin-release ==\n'
    printf 'Skipping Darwin release target: osxcross toolchain not available at %s\n' "$darwin_toolchain"
fi

cmake --build --preset package-single-header
cmake --build --preset package-source
make release-lua-artifacts
cmake --build --preset package-checksums

printf '\nRelease matrix completed successfully.\n'
