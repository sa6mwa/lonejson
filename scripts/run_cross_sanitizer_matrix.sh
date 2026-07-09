#!/usr/bin/env bash
set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
cpkt_aarch64_musl_prefix="${CPKT_AARCH64_MUSL_PREFIX:-$HOME/.local/cross/aarch64-linux-musl}"
cpkt_armhf_musl_prefix="${CPKT_ARMHF_MUSL_PREFIX:-$HOME/.local/cross/arm-linux-musleabihf}"
host_policy_ctest_exclude='lonejson_(discover_target_tools_tests|darwin_macho_metadata_tests|darwin_linker_route_tests|c_pkt_systems_fetch_retry_tests|cmake_threads_optional_tests|cmake_c_pkt_systems_root_tests|test_all_clang_optional_tests|check_clang_sanitizer_support_tests|cmake_fuzz_sanitizer_conflict_tests|cmake_fuzz_auth_optional_tests|release_werror_tests|source_release_tarball_tests|lua_src_rock_privacy_tests|lua_public_boundary_tests|lua_surface_coverage_tests|release_artifact_verify_tests|release_archive_verify_tests|lua_native_test_target_filter_tests|run_release_matrix_darwin_target_tests|release_checksum_manifest_tests|ctest_metadata_tests|short_names_tests|short_names_disabled_tests|single_header_strict_warning_tests|single_header_strict_warning_build_tests|single_header_strict_clang_build_tests|single_header_config_default|single_header_config_omit_protocol|single_header_config_lj_implementation|single_header_config_lj_config_aliases|single_header_config_small_parser_default_candidate_rejects|single_header_config_short_names_disabled|static_link_tests|shared_link_tests|shared_soversion_tests|single_header_version_tests|header_abi_version_tests|single_header_release_version_tests|bench_gate_tests)'

usage() {
    printf 'usage: %s [--dry-run]\n' "$(basename -- "$0")" >&2
}

dry_run=0
if [ "${1:-}" = "--dry-run" ]; then
    dry_run=1
    shift
fi
if [ "$#" -ne 0 ]; then
    usage
    exit 2
fi

diagnostic() {
    surface="$1"
    phase="$2"
    class="$3"
    reason="$4"
    next="$5"
    cat >&2 <<EOF
PKT_DIAGNOSTIC_BEGIN
surface=$surface
phase=$phase
status=failed
class=$class
reason=$reason
next=$next
PKT_DIAGNOSTIC_END
EOF
}

require_file() {
    if [ ! -e "$1" ]; then
        diagnostic cross-sanitizers prerequisites external-tool-unavailable "missing $1" "install the target toolchain/sysroot before running cross sanitizer gates"
        exit 1
    fi
}

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        diagnostic cross-sanitizers prerequisites external-tool-unavailable "missing command $1" "install $1 before running cross sanitizer gates"
        exit 1
    fi
}

run_or_print() {
    if [ "$dry_run" -eq 1 ]; then
        printf '+'
        printf ' %q' "$@"
        printf '\n'
    else
        "$@"
    fi
}

target_compiler() {
    case "$1" in
        aarch64-linux-gnu) printf '%s\n' /usr/bin/aarch64-linux-gnu-gcc ;;
        aarch64-linux-musl) printf '%s\n' "$cpkt_aarch64_musl_prefix/bin/aarch64-linux-musl-gcc" ;;
        armhf-linux-gnu) printf '%s\n' /usr/bin/arm-linux-gnueabihf-gcc ;;
        armhf-linux-musl) printf '%s\n' "$cpkt_armhf_musl_prefix/bin/arm-linux-musleabihf-gcc" ;;
        *) return 1 ;;
    esac
}

target_sysroot() {
    case "$1" in
        aarch64-linux-gnu) printf '%s\n' /usr/aarch64-linux-gnu ;;
        aarch64-linux-musl) printf '%s\n' "$cpkt_aarch64_musl_prefix/aarch64-linux-musl" ;;
        armhf-linux-gnu) printf '%s\n' /usr/arm-linux-gnueabihf ;;
        armhf-linux-musl) printf '%s\n' "$cpkt_armhf_musl_prefix/arm-linux-musleabihf" ;;
        *) return 1 ;;
    esac
}

target_emulator() {
    case "$1" in
        aarch64-linux-*) printf '%s\n' /usr/bin/qemu-aarch64 ;;
        armhf-linux-*) printf '%s\n' /usr/bin/qemu-arm ;;
        *) return 1 ;;
    esac
}

target_toolchain_file() {
    case "$1" in
        aarch64-linux-gnu) printf '%s\n' "$repo_root/cmake/toolchains/linux-aarch64-gnu.cmake" ;;
        aarch64-linux-musl) printf '%s\n' "$repo_root/cmake/toolchains/linux-aarch64-musl.cmake" ;;
        armhf-linux-gnu) printf '%s\n' "$repo_root/cmake/toolchains/linux-armhf-gnu.cmake" ;;
        armhf-linux-musl) printf '%s\n' "$repo_root/cmake/toolchains/linux-armhf-musl.cmake" ;;
        *) return 1 ;;
    esac
}

sanitizer_cmake_flag() {
    case "$1" in
        asan) printf '%s\n' -DLONEJSON_ENABLE_ASAN=ON ;;
        *) return 1 ;;
    esac
}

sanitizer_probe_flags() {
    case "$1" in
        asan) printf '%s\n' '-fsanitize=address,undefined -fno-omit-frame-pointer' ;;
        *) return 1 ;;
    esac
}

sanitizer_env_name() {
    case "$1" in
        asan) printf '%s\n' ASAN_OPTIONS ;;
        *) return 1 ;;
    esac
}

sanitizer_env_value() {
    case "$1" in
        asan) printf '%s\n' detect_leaks=0:abort_on_error=1:halt_on_error=1 ;;
        *) return 1 ;;
    esac
}

run_probe() {
    target_id="$1"
    sanitizer="$2"
    compiler="$(target_compiler "$target_id")"
    sysroot="$(target_sysroot "$target_id")"
    emulator="$(target_emulator "$target_id")"
    flags="$(sanitizer_probe_flags "$sanitizer")"
    env_name="$(sanitizer_env_name "$sanitizer")"
    env_value="$(sanitizer_env_value "$sanitizer")"
    probe_dir="$repo_root/build/cross-sanitizer-probes/$target_id/$sanitizer"
    probe_c="$probe_dir/probe.c"
    probe_exe="$probe_dir/probe"

    require_file "$compiler"
    require_file "$sysroot"
    require_file "$emulator"

    if [ "$dry_run" -eq 1 ]; then
        printf '+ probe %s %s with %s and %s -L %s\n' "$target_id" "$sanitizer" "$compiler" "$emulator" "$sysroot"
        return 0
    fi

    rm -rf "$probe_dir"
    mkdir -p "$probe_dir"
    printf 'int main(void){return 0;}\n' >"$probe_c"
    # shellcheck disable=SC2086
    if ! "$compiler" $flags "$probe_c" -o "$probe_exe" >"$probe_dir/build.out" 2>"$probe_dir/build.err"; then
        cat "$probe_dir/build.err" >&2
        diagnostic cross-sanitizers "probe-$target_id-$sanitizer" external-tool-unavailable "target compiler cannot build $sanitizer probe" "install a target compiler/runtime that supports $sanitizer for $target_id; see $probe_dir/build.err"
        exit 1
    fi
    set +e
    dash -c 'env "$1=$2" "$3" -L "$4" "$5"' sh \
        "$env_name" "$env_value" "$emulator" "$sysroot" "$probe_exe" \
        >"$probe_dir/run.out" 2>"$probe_dir/run.err"
    probe_status=$?
    set -e
    if [ "$probe_status" -ne 0 ]; then
        cat "$probe_dir/run.err" >&2
        diagnostic cross-sanitizers "probe-$target_id-$sanitizer" sanitizer "target $sanitizer runtime cannot execute under QEMU" "fix the $target_id QEMU/sanitizer runtime route before release; see $probe_dir/run.err"
        exit 1
    fi
    return 0
}

run_target_sanitizer() {
    preset="$1"
    target_id="$2"
    sanitizer="$3"
    build_dir="$repo_root/build/$preset-$sanitizer"
    toolchain_file="$(target_toolchain_file "$target_id")"
    bundle_root="$repo_root/.deps/c.pkt.systems/$target_id/root"
    sanitizer_flag="$(sanitizer_cmake_flag "$sanitizer")"
    env_name="$(sanitizer_env_name "$sanitizer")"
    env_value="$(sanitizer_env_value "$sanitizer")"

    printf '\n== %s %s ==\n' "$preset" "$sanitizer"
    run_probe "$target_id" "$sanitizer"
    run_or_print cmake -S "$repo_root" -B "$build_dir" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$toolchain_file" \
        -DCMAKE_BUILD_TYPE=Debug \
        "$sanitizer_flag" \
        -DLONEJSON_BUILD_WITH_CURL=ON \
        -DLONEJSON_BUILD_WITH_OPENSSL=ON \
        -DLONEJSON_BUILD_WITH_JWT=ON \
        -DLONEJSON_BUILD_WITH_OIDC=ON \
        -DLONEJSON_TEST_TIMEOUT=600 \
        -DLONEJSON_C_PKT_SYSTEMS_ROOT="$bundle_root"
    run_or_print cmake --build "$build_dir"
    if ! run_or_print env "$env_name=$env_value" \
        UBSAN_OPTIONS=halt_on_error=1:abort_on_error=1 \
        ctest --test-dir "$build_dir" --output-on-failure --timeout 600 \
        -E "$host_policy_ctest_exclude"; then
        diagnostic cross-sanitizers "ctest-$target_id-$sanitizer" test "sanitized target tests failed" "fix the reported target sanitizer test failure"
        exit 1
    fi
}

cd "$repo_root"

require_command dash

# Keep this matrix literal and target-owned. Today the installed QEMU/toolchain
# stack can prove ASan+UBSan only for armhf-linux-gnu:
# - aarch64-linux-gnu ASan: target libasan8-arm64-cross segfaults before main.
# - aarch64-linux-gnu TSan: QEMU user VMA range is unsupported.
# - GCC cross MSan: unsupported by the installed GCC cross compilers.
# - musl cross ASan/TSan: sanitizer runtime libraries are not installed.
while read -r preset target_id sanitizer; do
    [ -n "$preset" ] || continue
    run_target_sanitizer "$preset" "$target_id" "$sanitizer"
done <<'EOF'
armhf-linux-gnu-release armhf-linux-gnu asan
EOF
