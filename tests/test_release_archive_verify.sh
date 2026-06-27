#!/usr/bin/env bash
set -euo pipefail

repo_root=${1:?usage: test_release_archive_verify.sh REPO_ROOT}

require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    printf 'missing required command for archive verification test: %s\n' "$1" >&2
    exit 77
  fi
}

require_command cc
require_command ar
require_command cmake
require_command ninja
require_command nm
require_command pkg-config
require_command readelf
require_command sha256sum
require_command tar

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

dist_dir="$tmp_dir/dist"
build_root="$tmp_dir/build"
package_root="$tmp_dir/package/liblonejson-9.9.9-x86_64-linux-gnu"
mkdir -p \
  "$dist_dir" \
  "$build_root/linux-gnu-release" \
  "$package_root/include" \
  "$package_root/lib/cmake/lonejson" \
  "$package_root/lib/pkgconfig" \
  "$package_root/share/doc/liblonejson"

cat >"$package_root/include/lonejson.h" <<'EOF'
#ifndef LONEJSON_H
#define LONEJSON_H

typedef struct lonejson lonejson;
typedef enum lonejson_status {
  LONEJSON_STATUS_OK = 0
} lonejson_status;
typedef struct lonejson_error {
  int code;
} lonejson_error;

void lonejson_error_init(lonejson_error *error);
lonejson *lonejson_new(const void *config, lonejson_error *error);
void lonejson_free(lonejson *runtime);
lonejson_status lonejson_validate_cstr(lonejson *runtime, const char *json, lonejson_error *error);
void lonejson_curl_parse_init(void);

#endif
EOF

cat >"$tmp_dir/lonejson_stub.c" <<'EOF'
#include <lonejson.h>

struct lonejson {
  int unused;
};

static lonejson runtime;

void lonejson_error_init(lonejson_error *error) {
  if (error != 0) {
    error->code = 0;
  }
}

lonejson *lonejson_new(const void *config, lonejson_error *error) {
  (void)config;
  lonejson_error_init(error);
  return &runtime;
}

void lonejson_free(lonejson *runtime_arg) {
  (void)runtime_arg;
}

lonejson_status lonejson_validate_cstr(lonejson *runtime_arg, const char *json, lonejson_error *error) {
  (void)runtime_arg;
  (void)json;
  lonejson_error_init(error);
  return LONEJSON_STATUS_OK;
}

void lonejson_curl_parse_init(void) {
}
EOF

cc -shared -fPIC -I"$package_root/include" "$tmp_dir/lonejson_stub.c" \
  -o "$package_root/lib/liblonejson.so"
cc -c -I"$package_root/include" "$tmp_dir/lonejson_stub.c" \
  -o "$tmp_dir/lonejson_stub.o"
ar rcs "$package_root/lib/liblonejson.a" "$tmp_dir/lonejson_stub.o"

cat >"$package_root/lib/pkgconfig/lonejson.pc" <<'EOF'
prefix=${pcfiledir}/../..
libdir=${prefix}/lib
includedir=${prefix}/include

Name: lonejson
Description: lonejson archive verifier fixture
Version: 9.9.9
Libs: -L${libdir} -llonejson
Cflags: -I${includedir}
EOF

cat >"$package_root/lib/cmake/lonejson/lonejsonConfig.cmake" <<'EOF'
add_library(lonejson::lonejson SHARED IMPORTED)
set_target_properties(lonejson::lonejson PROPERTIES
  IMPORTED_LOCATION "${CMAKE_CURRENT_LIST_DIR}/../../liblonejson.so"
  INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/../../../include")
EOF

cat >"$package_root/lib/cmake/lonejson/lonejsonConfigVersion.cmake" <<'EOF'
set(PACKAGE_VERSION 9.9.9)
set(PACKAGE_VERSION_COMPATIBLE TRUE)
EOF

printf 'license\n' >"$package_root/share/doc/liblonejson/LICENSE"
printf 'readme\n' >"$package_root/share/doc/liblonejson/README.md"

cat >"$build_root/linux-gnu-release/CMakeCache.txt" <<EOF
CMAKE_C_COMPILER:FILEPATH=$(command -v cc)
CMAKE_NM:FILEPATH=$(command -v nm)
CMAKE_READELF:FILEPATH=$(command -v readelf)
EOF

tar -C "$tmp_dir/package" -czf \
  "$dist_dir/liblonejson-9.9.9-x86_64-linux-gnu.tar.gz" \
  "liblonejson-9.9.9-x86_64-linux-gnu"
(cd "$dist_dir" && sha256sum liblonejson-9.9.9-x86_64-linux-gnu.tar.gz >lonejson-9.9.9-CHECKSUMS)

"$repo_root/scripts/verify_release_archives.sh" \
  "$repo_root" \
  "$dist_dir/lonejson-9.9.9-CHECKSUMS" \
  "$build_root"

"$repo_root/scripts/verify_release_archives.sh" \
  "$repo_root" \
  "$dist_dir/lonejson-9.9.9-CHECKSUMS" \
  "$tmp_dir/missing-build-root"

broken_dist_dir="$tmp_dir/broken-dist"
broken_package_dir="$tmp_dir/broken-package"
broken_package_root="$broken_package_dir/liblonejson-9.9.9-x86_64-linux-gnu"
mkdir -p "$broken_dist_dir"
cp -R "$tmp_dir/package" "$broken_package_dir"

cat >"$tmp_dir/lonejson_static_without_curl.c" <<'EOF'
#include <lonejson.h>

struct lonejson {
  int unused;
};

static lonejson runtime;

void lonejson_error_init(lonejson_error *error) {
  if (error != 0) {
    error->code = 0;
  }
}

lonejson *lonejson_new(const void *config, lonejson_error *error) {
  (void)config;
  lonejson_error_init(error);
  return &runtime;
}

void lonejson_free(lonejson *runtime_arg) {
  (void)runtime_arg;
}

lonejson_status lonejson_validate_cstr(lonejson *runtime_arg, const char *json, lonejson_error *error) {
  (void)runtime_arg;
  (void)json;
  lonejson_error_init(error);
  return LONEJSON_STATUS_OK;
}
EOF

cc -c -I"$broken_package_root/include" "$tmp_dir/lonejson_static_without_curl.c" \
  -o "$tmp_dir/lonejson_static_without_curl.o"
rm -f "$broken_package_root/lib/liblonejson.a"
ar rcs "$broken_package_root/lib/liblonejson.a" \
  "$tmp_dir/lonejson_static_without_curl.o"

tar -C "$broken_package_dir" -czf \
  "$broken_dist_dir/liblonejson-9.9.9-x86_64-linux-gnu.tar.gz" \
  "liblonejson-9.9.9-x86_64-linux-gnu"
(cd "$broken_dist_dir" && sha256sum liblonejson-9.9.9-x86_64-linux-gnu.tar.gz >lonejson-9.9.9-CHECKSUMS)

broken_log="$tmp_dir/broken.log"
if "$repo_root/scripts/verify_release_archives.sh" \
  "$repo_root" \
  "$broken_dist_dir/lonejson-9.9.9-CHECKSUMS" \
  "$build_root" >"$broken_log" 2>&1; then
  printf 'expected archive verification to fail when static library lacks curl ABI\n' >&2
  exit 1
fi
grep -F 'missing lonejson_curl_* ABI symbol in static library' "$broken_log" >/dev/null
