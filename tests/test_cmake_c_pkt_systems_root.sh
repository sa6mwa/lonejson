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
module_build_dir="$tmp_dir/module-build"
missing_root_build_dir="$tmp_dir/missing-root-build"
missing_config_build_dir="$tmp_dir/missing-config-build"
missing_config_root="$tmp_dir/missing-config-root"

mkdir -p "$fake_root/include" "$fake_root/lib/cmake/CURL"
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

mkdir -p "$stale_root/include" "$stale_root/lib/cmake/CURL"
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
mkdir -p "$fake_modules/fake-system-curl/include"

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
  -D LONEJSON_C_PKT_SYSTEMS_ROOT="$fake_root" \
  -D CURL_DIR="$stale_root/lib/cmake/CURL" \
  -D CURL_INCLUDE_DIR="$tmp_dir/stale-curl-include" \
  -D CURL_LIBRARY_RELEASE="$tmp_dir/stale-libcurl.a" \
  -D LONEJSON_BUILD_TESTS=OFF \
  -D LONEJSON_BUILD_EXAMPLES=OFF \
  >"$tmp_dir/root-configure.log" 2>&1

grep -F "CURL_DIR:PATH=$fake_root/lib/cmake/CURL" \
  "$root_build_dir/CMakeCache.txt" >/dev/null

cmake -S "$repo_root" -B "$module_build_dir" \
  -G Ninja \
  -D CMAKE_MODULE_PATH="$fake_modules" \
  -D LONEJSON_BUILD_WITH_CURL=ON \
  -D LONEJSON_BUILD_TESTS=OFF \
  -D LONEJSON_BUILD_EXAMPLES=OFF \
  >"$tmp_dir/module-configure.log" 2>&1

if cmake -S "$repo_root" -B "$missing_root_build_dir" \
  -G Ninja \
  -D LONEJSON_BUILD_WITH_CURL=ON \
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
