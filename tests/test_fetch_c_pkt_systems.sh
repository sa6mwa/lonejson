#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

bundle_dir_name="c.pkt.systems-0.6.0-x86_64-linux-gnu"
assets_dir="$tmp_dir/assets"
bundle_root="$assets_dir/$bundle_dir_name"
success_root="$tmp_dir/success-root"
failure_root="$tmp_dir/failure-root"
pinned_version_root="$tmp_dir/pinned-version-root"
archive_path="$assets_dir/$bundle_dir_name.tar.gz"

mkdir -p \
  "$bundle_root/include/curl" \
  "$bundle_root/include/openssl" \
  "$bundle_root/include/nghttp2" \
  "$bundle_root/lib/cmake/CURL" \
  "$bundle_root/lib/cmake/OpenSSL" \
  "$bundle_root/lib/cmake/nghttp2" \
  "$bundle_root/lib/cmake/zlib" \
  "$bundle_root/lib/cmake/libssh2" \
  "$bundle_root/lib/pkgconfig"

cat >"$bundle_root/include/curl/curlver.h" <<'EOF'
#define LIBCURL_VERSION "8.20.0"
EOF
cat >"$bundle_root/include/openssl/opensslv.h" <<'EOF'
# define OPENSSL_VERSION_TEXT "OpenSSL 3.6.2 7 Apr 2026"
EOF
touch \
  "$bundle_root/include/nghttp2/nghttp2.h" \
  "$bundle_root/include/zlib.h" \
  "$bundle_root/lib/libcurl.so" \
  "$bundle_root/lib/libssl.so" \
  "$bundle_root/lib/libcrypto.so" \
  "$bundle_root/lib/cmake/CURL/CURLConfig.cmake" \
  "$bundle_root/lib/cmake/OpenSSL/OpenSSLConfig.cmake" \
  "$bundle_root/lib/cmake/nghttp2/nghttp2Config.cmake" \
  "$bundle_root/lib/cmake/zlib/ZLIBConfig.cmake" \
  "$bundle_root/lib/cmake/libssh2/libssh2-config.cmake" \
  "$bundle_root/lib/pkgconfig/libcurl.pc" \
  "$bundle_root/lib/pkgconfig/libcrypto.pc"

mkdir -p "$assets_dir"
tar -czf "$archive_path" -C "$assets_dir" "$bundle_dir_name"
archive_sha256=$(sha256sum "$archive_path" | awk '{print $1}')

success_log="$tmp_dir/success.log"
cmake \
  -D LONEJSON_SOURCE_DIR="$success_root" \
  -D LONEJSON_C_PKT_SYSTEMS_TARGET_ID=x86_64-linux-gnu \
  -D LONEJSON_C_PKT_SYSTEMS_BASE_URL="file://$assets_dir" \
  -D LONEJSON_C_PKT_SYSTEMS_EXPECTED_SHA256_OVERRIDE="$archive_sha256" \
  -D LONEJSON_C_PKT_SYSTEMS_DOWNLOAD_RETRIES=3 \
  -D LONEJSON_C_PKT_SYSTEMS_RETRY_DELAY_SECONDS=0 \
  -D LONEJSON_C_PKT_SYSTEMS_TEST_FAIL_DOWNLOAD_ATTEMPTS=2 \
  -P "$repo_root/cmake/fetch_c_pkt_systems.cmake" \
  >"$success_log" 2>&1

grep -q 'Download attempt 1/3 failed' "$success_log"
grep -q 'Download attempt 2/3 failed' "$success_log"
test -f "$success_root/.deps/c.pkt.systems/x86_64-linux-gnu/root/include/curl/curlver.h"
test -f "$success_root/.deps/c.pkt.systems/x86_64-linux-gnu/root/lib/cmake/CURL/CURLConfig.cmake"
test -f "$success_root/.deps/c.pkt.systems/x86_64-linux-gnu/root/lib/pkgconfig/libcurl.pc"
test -f "$success_root/.deps/c.pkt.systems/x86_64-linux-gnu/root/.lonejson-c-pkt-systems-version"

failure_log="$tmp_dir/failure.log"
if cmake \
  -D LONEJSON_SOURCE_DIR="$failure_root" \
  -D LONEJSON_C_PKT_SYSTEMS_TARGET_ID=x86_64-linux-gnu \
  -D LONEJSON_C_PKT_SYSTEMS_BASE_URL="file://$assets_dir" \
  -D LONEJSON_C_PKT_SYSTEMS_EXPECTED_SHA256_OVERRIDE="$archive_sha256" \
  -D LONEJSON_C_PKT_SYSTEMS_DOWNLOAD_RETRIES=3 \
  -D LONEJSON_C_PKT_SYSTEMS_RETRY_DELAY_SECONDS=0 \
  -D LONEJSON_C_PKT_SYSTEMS_TEST_FAIL_DOWNLOAD_ATTEMPTS=3 \
  -P "$repo_root/cmake/fetch_c_pkt_systems.cmake" \
  >"$failure_log" 2>&1; then
  printf 'expected fetch_c_pkt_systems.cmake to fail after exhausting retries\n' >&2
  exit 1
fi

grep -q 'after 3 attempts' "$failure_log"
test ! -e "$failure_root/.deps/c.pkt.systems/x86_64-linux-gnu/root/.lonejson-c-pkt-systems-version"

pinned_version_log="$tmp_dir/pinned-version.log"
if cmake \
  -D LONEJSON_SOURCE_DIR="$pinned_version_root" \
  -D LONEJSON_C_PKT_SYSTEMS_VERSION=0.2.0 \
  -D LONEJSON_C_PKT_SYSTEMS_TARGET_ID=x86_64-linux-gnu \
  -D LONEJSON_C_PKT_SYSTEMS_BASE_URL="file://$assets_dir" \
  -P "$repo_root/cmake/fetch_c_pkt_systems.cmake" \
  >"$pinned_version_log" 2>&1; then
  printf 'expected c.pkt.systems version override to fail before download\n' >&2
  exit 1
fi

grep -q 'LONEJSON_C_PKT_SYSTEMS_VERSION is not configurable' "$pinned_version_log"
test ! -e "$pinned_version_root/.deps/c.pkt.systems"
