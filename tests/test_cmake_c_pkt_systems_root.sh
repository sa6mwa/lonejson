#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

fake_root="$tmp_dir/c.pkt.systems/root"
fake_toolchain="$tmp_dir/toolchain.cmake"
fake_modules="$tmp_dir/modules"
stale_root="$tmp_dir/stale-curl/root"
root_build_dir="$tmp_dir/root-build"
openssl_root_build_dir="$tmp_dir/openssl-root-build"
module_build_dir="$tmp_dir/module-build"
unsupported_target_build_dir="$tmp_dir/unsupported-target-build"
missing_root_build_dir="$tmp_dir/missing-root-build"
missing_config_build_dir="$tmp_dir/missing-config-build"
missing_openssl_config_build_dir="$tmp_dir/missing-openssl-config-build"
jwt_without_openssl_build_dir="$tmp_dir/jwt-without-openssl-build"
missing_config_root="$tmp_dir/missing-config-root"
missing_openssl_config_root="$tmp_dir/missing-openssl-config-root"

mkdir -p "$fake_root/include" "$fake_root/lib/cmake/CURL" "$fake_root/lib/cmake/OpenSSL"
cat >"$fake_root/lib/cmake/CURL/CURLConfig.cmake" <<'EOF'
get_filename_component(_fake_curl_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(CURL_FOUND TRUE)
set(CURL_INCLUDE_DIRS "${_fake_curl_prefix}/include")
if(NOT TARGET CURL::libcurl)
  add_library(CURL::libcurl INTERFACE IMPORTED)
  set_target_properties(CURL::libcurl PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${_fake_curl_prefix}/include")
endif()
EOF
cat >"$fake_root/lib/cmake/OpenSSL/OpenSSLConfig.cmake" <<'EOF'
get_filename_component(_fake_openssl_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(OpenSSL_FOUND TRUE)
set(OPENSSL_INCLUDE_DIR "${_fake_openssl_prefix}/include")
if(NOT TARGET OpenSSL::SSL)
  add_library(OpenSSL::SSL INTERFACE IMPORTED)
  set_target_properties(OpenSSL::SSL PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${_fake_openssl_prefix}/include")
endif()
if(NOT TARGET OpenSSL::Crypto)
  add_library(OpenSSL::Crypto INTERFACE IMPORTED)
  set_target_properties(OpenSSL::Crypto PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${_fake_openssl_prefix}/include")
endif()
EOF

mkdir -p "$stale_root/include" "$stale_root/lib/cmake/CURL" "$stale_root/lib/cmake/OpenSSL"
cat >"$stale_root/lib/cmake/CURL/CURLConfig.cmake" <<'EOF'
get_filename_component(_stale_curl_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(CURL_FOUND TRUE)
set(CURL_INCLUDE_DIRS "${_stale_curl_prefix}/include")
if(NOT TARGET CURL::libcurl)
  add_library(CURL::libcurl INTERFACE IMPORTED)
  set_target_properties(CURL::libcurl PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${_stale_curl_prefix}/include")
endif()
EOF
cat >"$stale_root/lib/cmake/OpenSSL/OpenSSLConfig.cmake" <<'EOF'
get_filename_component(_stale_openssl_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(OpenSSL_FOUND TRUE)
if(NOT TARGET OpenSSL::SSL)
  add_library(OpenSSL::SSL INTERFACE IMPORTED)
  set_target_properties(OpenSSL::SSL PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${_stale_openssl_prefix}/include")
endif()
if(NOT TARGET OpenSSL::Crypto)
  add_library(OpenSSL::Crypto INTERFACE IMPORTED)
  set_target_properties(OpenSSL::Crypto PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${_stale_openssl_prefix}/include")
endif()
EOF

mkdir -p "$fake_modules"
cat >"$fake_modules/FindCURL.cmake" <<'EOF'
set(CURL_FOUND TRUE)
set(CURL_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/fake-system-curl/include")
if(NOT TARGET CURL::libcurl)
  add_library(CURL::libcurl INTERFACE IMPORTED)
  set_target_properties(CURL::libcurl PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/fake-system-curl/include")
endif()
EOF
cat >"$fake_modules/FindOpenSSL.cmake" <<'EOF'
set(OpenSSL_FOUND TRUE)
set(OPENSSL_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/fake-system-openssl/include")
if(NOT TARGET OpenSSL::SSL)
  add_library(OpenSSL::SSL INTERFACE IMPORTED)
  set_target_properties(OpenSSL::SSL PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/fake-system-openssl/include")
endif()
if(NOT TARGET OpenSSL::Crypto)
  add_library(OpenSSL::Crypto INTERFACE IMPORTED)
  set_target_properties(OpenSSL::Crypto PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/fake-system-openssl/include")
endif()
EOF
mkdir -p "$fake_modules/fake-system-curl/include"
mkdir -p "$fake_modules/fake-system-openssl/include"

cat >"$fake_toolchain" <<EOF
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_C_COMPILER /usr/bin/cc CACHE FILEPATH "")
set(CMAKE_FIND_ROOT_PATH "$tmp_dir/nonexistent-sysroot")
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
EOF

cmake -S "$repo_root" -B "$root_build_dir" \
  -G Ninja \
  -D CMAKE_TOOLCHAIN_FILE="$fake_toolchain" \
  -D LONEJSON_BUILD_WITH_CURL=ON \
  -D LONEJSON_BUILD_WITH_OPENSSL=ON \
  -D LONEJSON_C_PKT_SYSTEMS_ROOT="$fake_root" \
  -D CURL_DIR="$stale_root/lib/cmake/CURL" \
  -D CURL_INCLUDE_DIR="$tmp_dir/stale-curl-include" \
  -D CURL_LIBRARY_RELEASE="$tmp_dir/stale-libcurl.a" \
  -D OpenSSL_DIR="$stale_root/lib/cmake/OpenSSL" \
  -D OPENSSL_INCLUDE_DIR="$tmp_dir/stale-openssl-include" \
  -D OPENSSL_SSL_LIBRARY="$tmp_dir/stale-libssl.a" \
  -D OPENSSL_CRYPTO_LIBRARY="$tmp_dir/stale-libcrypto.a" \
  -D LONEJSON_BUILD_TESTS=OFF \
  -D LONEJSON_BUILD_EXAMPLES=OFF \
  >"$tmp_dir/root-configure.log" 2>&1

grep -F "CURL_DIR:PATH=$fake_root/lib/cmake/CURL" \
  "$root_build_dir/CMakeCache.txt" >/dev/null
grep -F "OpenSSL_DIR:PATH=$fake_root/lib/cmake/OpenSSL" \
  "$root_build_dir/CMakeCache.txt" >/dev/null

cmake -S "$repo_root" -B "$openssl_root_build_dir" \
  -G Ninja \
  -D CMAKE_TOOLCHAIN_FILE="$fake_toolchain" \
  -D LONEJSON_BUILD_WITH_OPENSSL=ON \
  -D LONEJSON_C_PKT_SYSTEMS_ROOT="$fake_root" \
  -D OpenSSL_DIR="$stale_root/lib/cmake/OpenSSL" \
  -D OPENSSL_INCLUDE_DIR="$tmp_dir/stale-openssl-include" \
  -D OPENSSL_SSL_LIBRARY="$tmp_dir/stale-libssl.a" \
  -D OPENSSL_CRYPTO_LIBRARY="$tmp_dir/stale-libcrypto.a" \
  -D LONEJSON_BUILD_TESTS=OFF \
  -D LONEJSON_BUILD_EXAMPLES=OFF \
  >"$tmp_dir/openssl-root-configure.log" 2>&1

grep -F "OpenSSL_DIR:PATH=$fake_root/lib/cmake/OpenSSL" \
  "$openssl_root_build_dir/CMakeCache.txt" >/dev/null

cmake -S "$repo_root" -B "$module_build_dir" \
  -G Ninja \
  -D CMAKE_MODULE_PATH="$fake_modules" \
  -D LONEJSON_BUILD_WITH_CURL=ON \
  -D LONEJSON_BUILD_WITH_OPENSSL=ON \
  -D LONEJSON_BUILD_TESTS=OFF \
  -D LONEJSON_BUILD_EXAMPLES=OFF \
  >"$tmp_dir/module-configure.log" 2>&1

cmake -S "$repo_root" -B "$unsupported_target_build_dir" \
  -G Ninja \
  -D CMAKE_MODULE_PATH="$fake_modules" \
  -D LONEJSON_BUILD_WITH_CURL=ON \
  -D LONEJSON_BUILD_WITH_OPENSSL=ON \
  -D LONEJSON_TARGET_OS=FreeBSD \
  -D LONEJSON_BUILD_TESTS=ON \
  -D LONEJSON_BUILD_EXAMPLES=OFF \
  >"$tmp_dir/unsupported-target-configure.log" 2>&1
ctest --test-dir "$unsupported_target_build_dir" -N \
  >"$tmp_dir/unsupported-target-ctest.log"
if grep -F 'lonejson_build_tree_curl_abi_tests' \
    "$tmp_dir/unsupported-target-ctest.log" >/dev/null; then
  printf 'curl ABI CTest should not be registered for unsupported target IDs\n' >&2
  exit 1
fi

if cmake -S "$repo_root" -B "$missing_root_build_dir" \
  -G Ninja \
  -D LONEJSON_BUILD_WITH_CURL=ON \
  -D LONEJSON_BUILD_WITH_OPENSSL=ON \
  -D LONEJSON_C_PKT_SYSTEMS_ROOT="$tmp_dir/missing-root" \
  -D LONEJSON_BUILD_TESTS=OFF \
  -D LONEJSON_BUILD_EXAMPLES=OFF \
  >"$tmp_dir/missing-root.log" 2>&1; then
  printf 'expected missing LONEJSON_C_PKT_SYSTEMS_ROOT to fail configure\n' >&2
  exit 1
fi
grep -F 'LONEJSON_C_PKT_SYSTEMS_ROOT does not exist' \
  "$tmp_dir/missing-root.log" >/dev/null

mkdir -p "$missing_config_root"
if cmake -S "$repo_root" -B "$missing_config_build_dir" \
  -G Ninja \
  -D LONEJSON_BUILD_WITH_CURL=ON \
  -D LONEJSON_C_PKT_SYSTEMS_ROOT="$missing_config_root" \
  -D LONEJSON_BUILD_TESTS=OFF \
  -D LONEJSON_BUILD_EXAMPLES=OFF \
  >"$tmp_dir/missing-config.log" 2>&1; then
  printf 'expected c.pkt.systems root without CURLConfig.cmake to fail configure\n' >&2
  exit 1
fi
grep -F 'LONEJSON_C_PKT_SYSTEMS_ROOT is missing CURLConfig.cmake' \
  "$tmp_dir/missing-config.log" >/dev/null

mkdir -p "$missing_openssl_config_root/lib/cmake/CURL"
cp "$fake_root/lib/cmake/CURL/CURLConfig.cmake" \
  "$missing_openssl_config_root/lib/cmake/CURL/CURLConfig.cmake"
if cmake -S "$repo_root" -B "$missing_openssl_config_build_dir" \
  -G Ninja \
  -D LONEJSON_BUILD_WITH_OPENSSL=ON \
  -D LONEJSON_C_PKT_SYSTEMS_ROOT="$missing_openssl_config_root" \
  -D LONEJSON_BUILD_TESTS=OFF \
  -D LONEJSON_BUILD_EXAMPLES=OFF \
  >"$tmp_dir/missing-openssl-config.log" 2>&1; then
  printf 'expected c.pkt.systems root without OpenSSLConfig.cmake to fail configure\n' >&2
  exit 1
fi
grep -F 'LONEJSON_C_PKT_SYSTEMS_ROOT is missing OpenSSLConfig.cmake' \
  "$tmp_dir/missing-openssl-config.log" >/dev/null

if cmake -S "$repo_root" -B "$jwt_without_openssl_build_dir" \
  -G Ninja \
  -D CMAKE_MODULE_PATH="$fake_modules" \
  -D LONEJSON_BUILD_WITH_JWT=ON \
  -D LONEJSON_BUILD_TESTS=OFF \
  -D LONEJSON_BUILD_EXAMPLES=OFF \
  >"$tmp_dir/jwt-without-openssl.log" 2>&1; then
  printf 'expected JWT configure without OpenSSL to fail\n' >&2
  exit 1
fi
grep -F 'LONEJSON_BUILD_WITH_JWT requires LONEJSON_BUILD_WITH_OPENSSL=ON' \
  "$tmp_dir/jwt-without-openssl.log" >/dev/null
