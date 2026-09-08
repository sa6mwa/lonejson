#!/usr/bin/env bash
set -euo pipefail
repo_root=$1
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT
fixture="$tmp_dir/repo"
mkdir -p "$fixture/scripts" "$fixture/cmake/toolchains" "$tmp_dir/bin" "$tmp_dir/SDK"
for name in cpkt-toolchains build_curl_examples build_lua_rock detect_c_pkt_systems_bundle detect_native_target; do
  cp "$repo_root/scripts/$name.sh" "$fixture/scripts/"
done
cp "$repo_root/cmake/toolchains/"{native,lonejson_native_darwin,arm64-apple-darwin}.cmake "$fixture/cmake/toolchains/"
cp "$repo_root/cmake/LonejsonCompiler.cmake" "$fixture/cmake/"
cat >"$tmp_dir/bin/uname" <<'EOF'
#!/usr/bin/env bash
case "$1" in
  -s) printf '%s\n' "${TEST_SYSTEM:-Darwin}" ;;
  -m) printf '%s\n' "${TEST_ARCH:-arm64}" ;;
esac
EOF
cat >"$tmp_dir/bin/xcrun" <<'EOF'
#!/usr/bin/env bash
[[ "${FAIL_XCRUN:-0}" == 0 ]] || exit 1
[[ "$1" == --sdk && "$2" == macosx ]]
case "$3" in
  --show-sdk-path) printf '%s\n' "$TEST_ROOT/SDK" ;;
  --find) printf '%s/bin/%s\n' "$TEST_ROOT" "$4" ;;
  *) exit 1 ;;
esac
EOF
cat >"$tmp_dir/bin/clang" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' "${0##*/} $*" >>"$TEST_ROOT/compiler.log"
case "$*" in
  *pkce_provider_no_openssl.c*)
    while [[ $# -gt 0 ]]; do
      if [[ "$1" == -o ]]; then
        printf '#!/usr/bin/env bash\nprintf "PKCE executed\\n" >>"$TEST_ROOT/compiler.log"\n' >"$2"
        chmod +x "$2"
        break
      fi
      shift
    done
    ;;
esac
EOF
for tool in clang++ ld ar ranlib strip nm otool install_name_tool; do
  cp "$tmp_dir/bin/clang" "$tmp_dir/bin/$tool"
done
cat >"$tmp_dir/bin/pkg-config" <<'EOF'
#!/usr/bin/env bash
case "$*" in
  *--cflags*) printf '%s\n' '-DTEST_CURL' ;;
  *--libs*) printf '%s\n' '-lcurl' ;;
esac
EOF
chmod +x "$tmp_dir/bin/"*
export TEST_ROOT="$tmp_dir" UNAME="$tmp_dir/bin/uname"
export PATH="$tmp_dir/bin:$PATH" OSXCROSS_ROOT="$tmp_dir/missing-osxcross" CC=/bin/false
export CPKT_OSXCROSS_HOST=arm64-apple-darwin25 PKG_CONFIG="$tmp_dir/bin/pkg-config"
unset LONEJSON_C_PKT_SYSTEMS_TARGET_ID CPKT_SYSROOT
description=$("$fixture/scripts/cpkt-toolchains.sh" ensure arm64-apple-darwin)
grep -Fx 'source=apple' <<<"$description"
grep -Fx "cc=$tmp_dir/bin/clang" <<<"$description"
grep -Fx "sysroot=$tmp_dir/SDK" <<<"$description"
[[ "$("$fixture/scripts/detect_native_target.sh")" == arm64-apple-darwin ]]
mkdir -p "$fixture/.cache/c.pkt.systems/arm64-apple-darwin/root/lib/pkgconfig"
"$fixture/scripts/build_curl_examples.sh"
[[ $(wc -l <"$tmp_dir/compiler.log") -eq 2 ]]
grep -F -- '-arch arm64 -isysroot' "$tmp_dir/compiler.log" >/dev/null
grep -F -- '-Wl,-fatal_warnings' "$tmp_dir/compiler.log" >/dev/null
if grep -E -- 'rpath-link|--fatal-warnings|osxcross|gcc' "$tmp_dir/compiler.log"; then exit 1; fi
# LuaRocks must likewise use the native compiler even if its CC is overridden.
sh "$fixture/scripts/build_lua_rock.sh" /bin/false '-O2 -fPIC' -shared o so "$tmp_dir/headers" "$tmp_dir/lib"
[[ $(wc -l <"$tmp_dir/compiler.log") -eq 5 ]]
grep -F 'clang -arch arm64 -isysroot' "$tmp_dir/compiler.log" >/dev/null

# The PKCE fixture compiles and executes through native Apple discovery.
: >"$tmp_dir/compiler.log"
bash "$repo_root/tests/test_oidc_pkce_provider_no_openssl.sh" "$fixture"
grep -F "clang -arch arm64 -isysroot $tmp_dir/SDK -std=c89" "$tmp_dir/compiler.log" >/dev/null
grep -Fx 'PKCE executed' "$tmp_dir/compiler.log"
if FAIL_XCRUN=1 bash "$repo_root/tests/test_oidc_pkce_provider_no_openssl.sh" "$fixture" >"$tmp_dir/error" 2>&1; then exit 1; fi
grep -F 'install Xcode or Command Line Tools' "$tmp_dir/error"

# Every CMake entry point uses the Apple tools without marking the build cross.
cat >"$tmp_dir/check.cmake" <<'EOF'
set(CMAKE_HOST_SYSTEM_NAME Darwin)
include("${ENTRY}")
if(NOT CMAKE_C_COMPILER STREQUAL "$ENV{TEST_ROOT}/bin/clang" OR
    NOT CMAKE_CXX_COMPILER STREQUAL "$ENV{TEST_ROOT}/bin/clang++" OR
    NOT CMAKE_OSX_SYSROOT STREQUAL "$ENV{TEST_ROOT}/SDK" OR
    NOT CMAKE_OSX_ARCHITECTURES STREQUAL "arm64" OR
    NOT CMAKE_OTOOL STREQUAL "$ENV{TEST_ROOT}/bin/otool" OR
    CMAKE_CROSSCOMPILING OR DEFINED CMAKE_SYSTEM_NAME OR
    CMAKE_EXE_LINKER_FLAGS MATCHES "--ld-path|fuse-ld")
  message(FATAL_ERROR "Native Apple configuration used cross or host Linux tools")
endif()
EOF
for entry in cmake/LonejsonCompiler.cmake cmake/toolchains/native.cmake cmake/toolchains/arm64-apple-darwin.cmake; do
  cmake -D "ENTRY=$fixture/$entry" -P "$tmp_dir/check.cmake"
done
if FAIL_XCRUN=1 "$fixture/scripts/cpkt-toolchains.sh" ensure arm64-apple-darwin >"$tmp_dir/error" 2>&1; then exit 1; fi
grep -F 'install Xcode or Command Line Tools' "$tmp_dir/error"
if TEST_ARCH=x86_64 "$fixture/scripts/cpkt-toolchains.sh" ensure arm64-apple-darwin >"$tmp_dir/error" 2>&1; then exit 1; fi
grep -F 'require Apple Silicon' "$tmp_dir/error"

# Linux-to-Darwin still uses osxcross and Darwin linker flags.
export TEST_SYSTEM=Linux
export CPKT_SYSROOT=/not-an-apple-sdk
mkdir -p "$OSXCROSS_ROOT/bin"
for tool in clang clang++ ld ar ranlib strip nm otool; do
  cp "$tmp_dir/bin/clang" "$OSXCROSS_ROOT/bin/arm64-apple-darwin25-$tool"
done
: >"$tmp_dir/compiler.log"
LONEJSON_C_PKT_SYSTEMS_TARGET_ID=arm64-apple-darwin "$fixture/scripts/build_curl_examples.sh"
grep -F 'arm64-apple-darwin25-clang ' "$tmp_dir/compiler.log" >/dev/null
grep -F -- '-Wl,-fatal_warnings' "$tmp_dir/compiler.log" >/dev/null
if grep -F -- '--fatal-warnings' "$tmp_dir/compiler.log"; then exit 1; fi
if grep -F -- '/not-an-apple-sdk' "$tmp_dir/compiler.log"; then exit 1; fi

# Linux native builds resolve Bootlin regardless of CC.
cat >"$fixture/scripts/detect_native_bootlin_target.sh" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' x86_64-linux-gnu
EOF
cat >"$fixture/scripts/cpkt-toolchains.sh" <<'EOF'
#!/usr/bin/env bash
[[ "$1" == env && "$2" == x86_64-linux-gnu ]]
printf 'export CC=%q\n' "$TEST_ROOT/bin/bootlin-gcc"
EOF
cp "$tmp_dir/bin/clang" "$tmp_dir/bin/bootlin-gcc"
chmod +x "$fixture/scripts/"*.sh
mkdir -p "$fixture/.cache/c.pkt.systems/x86_64-linux-gnu/root/lib/pkgconfig"
: >"$tmp_dir/compiler.log"
"$fixture/scripts/build_curl_examples.sh"
grep -F 'bootlin-gcc ' "$tmp_dir/compiler.log" >/dev/null
grep -F -- '-Wl,--fatal-warnings' "$tmp_dir/compiler.log" >/dev/null
if grep -F -- '-arch arm64' "$tmp_dir/compiler.log"; then exit 1; fi
: >"$tmp_dir/compiler.log"
bash "$repo_root/tests/test_oidc_pkce_provider_no_openssl.sh" "$fixture"
grep -F 'bootlin-gcc -std=c89' "$tmp_dir/compiler.log" >/dev/null
grep -Fx 'PKCE executed' "$tmp_dir/compiler.log"
if grep -E -- '-arch|-isysroot|/not-an-apple-sdk' "$tmp_dir/compiler.log"; then exit 1; fi
