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

configure_release_target() {
    preset="$1"
    target_id="$2"

    bundle_root="$repo_root/.deps/c.pkt.systems/$target_id/root"
    if [ ! -f "$bundle_root/lib/cmake/CURL/CURLConfig.cmake" ]; then
        printf 'missing c.pkt.systems CURL CMake package for %s under %s\n' "$target_id" "$bundle_root/lib/cmake/CURL" >&2
        exit 1
    fi
    if [ ! -f "$bundle_root/lib/cmake/OpenSSL/OpenSSLConfig.cmake" ]; then
        printf 'missing c.pkt.systems OpenSSL CMake package for %s under %s\n' "$target_id" "$bundle_root/lib/cmake/OpenSSL" >&2
        exit 1
    fi

    cmake --preset "$preset" \
        -D LONEJSON_BUILD_WITH_CURL=ON \
        -D LONEJSON_BUILD_WITH_OPENSSL=ON \
        -D LONEJSON_BUILD_WITH_JWT=ON \
        -D LONEJSON_C_PKT_SYSTEMS_ROOT="$bundle_root"
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
}

run_build_only_target() {
    preset="$1"
    target_id="$2"
    package_preset="$3"

    printf '\n== %s ==\n' "$preset"
    configure_release_target "$preset" "$target_id"
    cmake --build --preset "$preset"
    cmake --build --preset "$package_preset"
}

require_command cmake
require_command ctest
require_command make
require_command ninja
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
    darwin_tool_bin="$(dirname -- "$darwin_toolchain")"
    export PATH="$darwin_tool_bin:$PATH"
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
make package-verify

printf '\nRelease matrix completed successfully.\n'
