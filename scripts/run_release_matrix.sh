#!/usr/bin/env bash

set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        printf 'missing required command: %s\n' "$1" >&2
        exit 1
    fi
}

configure_release_target() {
    preset="$1"
    target_id="$2"

    bundle_root="$repo_root/.cache/c.pkt.systems/$target_id/root"
    if [ ! -f "$bundle_root/lib/cmake/CURL/CURLConfig.cmake" ]; then
        printf 'missing c.pkt.systems CURL CMake package for %s under %s\n' "$target_id" "$bundle_root/lib/cmake/CURL" >&2
        exit 1
    fi
    cmake --preset "$preset" \
        -D LONEJSON_BUILD_WITH_CURL=ON \
        -D LONEJSON_BUILD_WITH_OPENSSL=ON \
        -D LONEJSON_BUILD_WITH_JWT=ON \
        -D LONEJSON_BUILD_WITH_OIDC=ON \
        -D LONEJSON_C_PKT_SYSTEMS_ROOT="$bundle_root"
}

run_target() {
    preset="$1"
    target_id="$2"
    package_preset="$3"
    ctest_mode="${4:-package-only}"
    ctest_exclude="${5:-}"

    printf '\n== %s ==\n' "$preset"
    configure_release_target "$preset" "$target_id"
    cmake --build --preset "$preset"
    if [ "$ctest_mode" = "full" ]; then
        if [ -n "$ctest_exclude" ]; then
            ctest --preset "$preset" --output-on-failure -E "$ctest_exclude"
        else
            ctest --preset "$preset"
        fi
    fi
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
require_command qemu-aarch64
require_command qemu-arm
"$repo_root/scripts/cpkt-toolchains.sh" ensure all

# LuaRocks and host-tooling checks compile host modules against the configured
# build tree. They are exercised by the native release target, but cannot link
# a foreign target library; QEMU coverage below still executes every runnable
# target test and package check.
cross_ctest_exclude='lonejson_(discover_target_tools_tests|compiler_selection_tests|darwin_macho_metadata_tests|darwin_linker_route_tests|c_pkt_systems_fetch_retry_tests|cmake_threads_optional_tests|cmake_c_pkt_systems_root_tests|test_all_hardening_tests|bootlin_tsan_support_tests|cross_sanitizer_matrix_tests|cmake_fuzz_sanitizer_conflict_tests|cmake_fuzz_auth_optional_tests|release_werror_tests|source_release_tarball_tests|lua_legacy_uservalue_tests|lua_schema_cache_tests|lua_encode_stats_tests|lua_external_liblonejson_tests|lua_src_rock_privacy_tests|lua_public_boundary_tests|lua_surface_coverage_tests|lua_source_stage_manifest_tests|release_artifact_verify_tests|release_archive_verify_tests|lua_native_test_target_filter_tests|run_release_matrix_darwin_target_tests|release_checksum_manifest_tests|ctest_metadata_tests|short_names_tests|short_names_disabled_tests|single_header_strict_warning_tests|single_header_strict_warning_build_tests|single_header_strict_toolchain_build_tests|single_header_config_default|single_header_config_omit_protocol|single_header_config_lj_implementation|single_header_config_short_names_disabled|static_link_tests|shared_link_tests|shared_soversion_tests|single_header_version_tests|header_abi_version_tests|single_header_release_version_tests|bench_gate_tests)'

darwin_toolchain=
if darwin_description="$("$repo_root/scripts/osxcross_available.sh" 2>/dev/null)"; then
    darwin_toolchain="$(printf '%s\n' "$darwin_description" | sed -n 's/^cc=//p')"
fi

if [ "${LONEJSON_RELEASE_MATRIX_PREFLIGHT_ONLY:-0}" = "1" ]; then
    printf 'Release matrix preflight completed successfully.\n'
    exit 0
fi

cd "$repo_root"

cmake --preset host
cmake --build --preset package-clean-dist

"$repo_root/scripts/deps.sh" x86_64-linux-gnu
"$repo_root/scripts/deps.sh" x86_64-linux-musl
"$repo_root/scripts/deps.sh" aarch64-linux-gnu
"$repo_root/scripts/deps.sh" aarch64-linux-musl
"$repo_root/scripts/deps.sh" armhf-linux-gnu
"$repo_root/scripts/deps.sh" armhf-linux-musl
run_target x86_64-linux-gnu-release x86_64-linux-gnu package-archive-x86_64-linux-gnu full
run_target x86_64-linux-musl-release x86_64-linux-musl package-archive-x86_64-linux-musl full "$cross_ctest_exclude"
run_target aarch64-linux-gnu-release aarch64-linux-gnu package-archive-aarch64-linux-gnu full "$cross_ctest_exclude"
run_target aarch64-linux-musl-release aarch64-linux-musl package-archive-aarch64-linux-musl full "$cross_ctest_exclude"
run_target armhf-linux-gnu-release armhf-linux-gnu package-archive-armhf-linux-gnu full "$cross_ctest_exclude"
run_target armhf-linux-musl-release armhf-linux-musl package-archive-armhf-linux-musl full "$cross_ctest_exclude"
if [ -n "$darwin_toolchain" ] && [ -x "$darwin_toolchain" ]; then
    darwin_tool_bin="$(dirname -- "$darwin_toolchain")"
    export PATH="$darwin_tool_bin:$PATH"
    "$repo_root/scripts/deps.sh" arm64-apple-darwin
    run_build_only_target arm64-apple-darwin-release arm64-apple-darwin package-archive-arm64-apple-darwin
    ./scripts/smoke_darwin_release.sh arm64-apple-darwin-release
    cmake --build --preset arm64-apple-darwin-release --target package-darwin-smoke-bundle
else
    printf '\n== arm64-apple-darwin-release ==\n'
    printf 'Skipping Darwin release target: local osxcross toolchain is unavailable\n'
fi

cmake --build --preset package-single-header
cmake --build --preset package-source
make release-lua-artifacts
cmake --build --preset package-checksums
make package-verify

printf '\nRelease matrix completed successfully.\n'
