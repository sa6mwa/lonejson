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

target_compiler() {
    target_id="$1"

    case "$target_id" in
        x86_64-linux-gnu) printf '%s\n' cc ;;
        x86_64-linux-musl) printf '%s\n' musl-gcc ;;
        aarch64-linux-gnu) printf '%s\n' aarch64-linux-gnu-gcc ;;
        aarch64-linux-musl) printf '%s\n' aarch64-linux-musl-gcc ;;
        armhf-linux-gnu) printf '%s\n' arm-linux-gnueabihf-gcc ;;
        armhf-linux-musl) printf '%s\n' arm-linux-musleabihf-gcc ;;
        arm64-apple-darwin)
            printf '%s\n' "${OSXCROSS_ROOT:-$HOME/.local/cross/osxcross}/bin/arm64-apple-darwin25-clang"
            ;;
        *)
            printf 'unknown target compiler for %s\n' "$target_id" >&2
            exit 1
            ;;
    esac
}

target_cmake_system_name() {
    target_id="$1"

    case "$target_id" in
        *apple-darwin) printf '%s\n' Darwin ;;
        *) printf '%s\n' Linux ;;
    esac
}

target_raw_link_flags() {
    target_id="$1"

    case "$target_id" in
        arm64-apple-darwin)
            printf '%s\n' "--ld-path=${OSXCROSS_ROOT:-$HOME/.local/cross/osxcross}/bin/arm64-apple-darwin25-ld"
            ;;
        *)
            printf '%s\n' ""
            ;;
    esac
}

target_raw_compile_flags() {
    target_id="$1"

    case "$target_id" in
        arm64-apple-darwin)
            printf '%s\n' "-mmacosx-version-min=$(target_darwin_deployment_target)"
            ;;
        *)
            printf '%s\n' ""
            ;;
    esac
}

target_darwin_deployment_target() {
    printf '%s\n' "${LONEJSON_MACOS_DEPLOYMENT_TARGET:-15.0}"
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
            "lib/cmake/" | \
            "lib/cmake/lonejson/" | \
            "lib/cmake/lonejson/lonejsonConfig.cmake" | \
            "lib/cmake/lonejson/lonejsonConfigVersion.cmake" | \
            "lib/liblonejson.a" | \
            "lib/liblonejson.so" | \
            "lib/liblonejson.so."* | \
            "lib/liblonejson.dylib" | \
            "lib/liblonejson."*".dylib" | \
            "lib/pkgconfig/" | \
            "lib/pkgconfig/lonejson.pc" | \
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

    require_file "$package_root/lib/pkgconfig/lonejson.pc"
    require_file "$package_root/lib/cmake/lonejson/lonejsonConfig.cmake"
    require_file "$package_root/lib/cmake/lonejson/lonejsonConfigVersion.cmake"
    if grep -RE 'libcurl|c\.pkt\.systems|\.deps/|/home/|/build/' \
        "$package_root/lib/pkgconfig/lonejson.pc" \
        "$package_root/lib/cmake/lonejson" >/dev/null; then
        printf 'forbidden dependency or path leak in release metadata for %s\n' "$archive" >&2
        exit 1
    fi
    if grep -Eq '^(Requires|Requires.private):.*curl' "$package_root/lib/pkgconfig/lonejson.pc"; then
        printf 'unexpected curl pkg-config dependency in %s\n' "$archive" >&2
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

require_archive_consumer_metadata() {
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

    compiler="$(target_compiler "$target_id")"
    require_command "$compiler"

    consumer_source="$tmp_dir/consumer.c"
    cat >"$consumer_source" <<'EOF'
#include <lonejson.h>

int main(void) {
    lonejson *runtime;
    lonejson_error error;

    lonejson_error_init(&error);
    runtime = lonejson_new(NULL, &error);
    if (runtime == NULL) {
        return 1;
    }
    if (lonejson_validate_cstr(runtime, "{\"ok\":true}", &error) != LONEJSON_STATUS_OK) {
        lonejson_free(runtime);
        return 1;
    }
    lonejson_free(runtime);
    return 0;
}
EOF

    require_command pkg-config
    pkg_config_flags="$(PKG_CONFIG_PATH="$package_root/lib/pkgconfig" pkg-config --cflags --libs lonejson)"
    raw_compile_flags="$(target_raw_compile_flags "$target_id")"
    raw_link_flags="$(target_raw_link_flags "$target_id")"
    # shellcheck disable=SC2086
    "$compiler" "$consumer_source" $raw_compile_flags $pkg_config_flags $raw_link_flags -o "$tmp_dir/pkg-config-consumer"

    cmake_source_dir="$tmp_dir/cmake-consumer"
    cmake_build_dir="$tmp_dir/cmake-build"
    mkdir -p "$cmake_source_dir"
    cp "$consumer_source" "$cmake_source_dir/main.c"
    cat >"$cmake_source_dir/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.21)
project(lonejson_archive_consumer C)
find_package(lonejson CONFIG REQUIRED)
add_executable(lonejson_archive_consumer main.c)
target_link_libraries(lonejson_archive_consumer PRIVATE lonejson::lonejson)
EOF

    if [ "${target_id#*apple-darwin}" != "$target_id" ]; then
        darwin_deployment_target="$(target_darwin_deployment_target)"
        cmake_args=(
            -S "$cmake_source_dir"
            -B "$cmake_build_dir"
            -G Ninja
            -D "CMAKE_PREFIX_PATH=$package_root"
            -D "lonejson_DIR=$package_root/lib/cmake/lonejson"
            -D "CMAKE_TOOLCHAIN_FILE=$repo_root/cmake/toolchains/arm64-apple-darwin.cmake"
            -D "LONEJSON_MACOS_DEPLOYMENT_TARGET=$darwin_deployment_target"
            -D "CMAKE_OSX_DEPLOYMENT_TARGET=$darwin_deployment_target"
        )
    else
        cmake_system_name="$(target_cmake_system_name "$target_id")"
        cmake_args=(
            -S "$cmake_source_dir"
            -B "$cmake_build_dir"
            -G Ninja
            -D "CMAKE_PREFIX_PATH=$package_root"
            -D "lonejson_DIR=$package_root/lib/cmake/lonejson"
            -D "CMAKE_C_COMPILER=$compiler"
            -D "CMAKE_SYSTEM_NAME=$cmake_system_name"
            -D CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
        )
    fi
    cmake "${cmake_args[@]}"
    cmake --build "$cmake_build_dir"

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
    if [ ! -f "$bundle_root/lib/cmake/CURL/CURLConfig.cmake" ]; then
        printf 'missing c.pkt.systems CURL CMake package for %s under %s\n' "$target_id" "$bundle_root/lib/cmake/CURL" >&2
        exit 1
    fi

    cmake --preset "$preset" \
        -D LONEJSON_BUILD_WITH_CURL=ON \
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
    archive="$(release_archive_path "$target_id")"
    require_archive_contract "$archive" "$target_id"
    require_curl_symbol "$archive" "$target_id"
    require_archive_consumer_metadata "$archive" "$target_id"
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
    require_archive_consumer_metadata "$archive" "$target_id"
}

require_command cmake
require_command ctest
require_command make
require_command nm
require_command readelf
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
