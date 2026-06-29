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
  "$package_root/share/lonejson" \
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
void lonejson_jwt_parse_compact(void);
void lonejson_jwk_parse_json(void);
void lonejson_jwks_parse_json(void);
void lonejson_jwks_select(void);
void lonejson_jwk_cleanup(void);
void lonejson_jwks_cleanup(void);
void lonejson_jwt_decode_compact(void);
void lonejson_jwt_validate_claims(void);
void lonejson_jwt_validate_signature(void);
void lonejson_jwt_header_cleanup(void);
void lonejson_jwt_claims_cleanup(void);
void lonejson_oidc_discovery_url(void);
void lonejson_oidc_discovery_parse_json(void);
void lonejson_oidc_discovery_validate_issuer(void);
void lonejson_oidc_discovery_cleanup(void);
void lonejson_oidc_jwks_cache_init(void);
void lonejson_oidc_jwks_cache_cleanup(void);
void lonejson_oidc_jwks_cache_update_json(void);
void lonejson_oidc_jwks_cache_is_fresh(void);
void lonejson_oidc_jwks_cache_select(void);
void lonejson_oidc_jwks_cache_parse_init(void);
void lonejson_oidc_jwks_cache_write_callback(void);
void lonejson_oidc_jwks_cache_parse_finish(void);
void lonejson_oidc_jwks_cache_parse_cleanup(void);
void lonejson_oauth2_client_credentials_body(void);
void lonejson_oauth2_token_response_init(void);
void lonejson_oauth2_token_response_cleanup(void);
void lonejson_oauth2_token_response_parse_json(void);
void lonejson_oidc_pkce_init(void);
void lonejson_oidc_pkce_cleanup(void);
void lonejson_oidc_pkce_challenge(void);
void lonejson_oidc_pkce_generate(void);
void lonejson_oidc_authorization_url(void);
void lonejson_oidc_authorization_callback_init(void);
void lonejson_oidc_authorization_callback_cleanup(void);
void lonejson_oidc_authorization_callback_parse_query(void);
void lonejson_auth_failure_string(void);
void lonejson_oidc_bearer_validation_init(void);
void lonejson_oidc_bearer_validation_cleanup(void);
void lonejson_oidc_authorization_bearer_token(void);
void lonejson_oidc_validate_bearer_token(void);

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

void lonejson_jwt_parse_compact(void) {
}

void lonejson_jwk_parse_json(void) {
}

void lonejson_jwks_parse_json(void) {
}

void lonejson_jwks_select(void) {
}

void lonejson_jwk_cleanup(void) {
}

void lonejson_jwks_cleanup(void) {
}

void lonejson_jwt_decode_compact(void) {
}

void lonejson_jwt_validate_claims(void) {
}

void lonejson_jwt_validate_signature(void) {
}

void lonejson_jwt_header_cleanup(void) {
}

void lonejson_jwt_claims_cleanup(void) {
}

void lonejson_oidc_discovery_url(void) {
}

void lonejson_oidc_discovery_parse_json(void) {
}

void lonejson_oidc_discovery_validate_issuer(void) {
}

void lonejson_oidc_discovery_cleanup(void) {
}

void lonejson_oidc_jwks_cache_init(void) {
}

void lonejson_oidc_jwks_cache_cleanup(void) {
}

void lonejson_oidc_jwks_cache_update_json(void) {
}

void lonejson_oidc_jwks_cache_is_fresh(void) {
}

void lonejson_oidc_jwks_cache_select(void) {
}

void lonejson_oidc_jwks_cache_parse_init(void) {
}

void lonejson_oidc_jwks_cache_write_callback(void) {
}

void lonejson_oidc_jwks_cache_parse_finish(void) {
}

void lonejson_oidc_jwks_cache_parse_cleanup(void) {
}

void lonejson_oauth2_client_credentials_body(void) {
}

void lonejson_oauth2_token_response_init(void) {
}

void lonejson_oauth2_token_response_cleanup(void) {
}

void lonejson_oauth2_token_response_parse_json(void) {
}

void lonejson_oidc_pkce_init(void) {
}

void lonejson_oidc_pkce_cleanup(void) {
}

void lonejson_oidc_pkce_challenge(void) {
}

void lonejson_oidc_pkce_generate(void) {
}

void lonejson_oidc_authorization_url(void) {
}

void lonejson_oidc_authorization_callback_init(void) {
}

void lonejson_oidc_authorization_callback_cleanup(void) {
}

void lonejson_oidc_authorization_callback_parse_query(void) {
}

void lonejson_auth_failure_string(void) {
}

void lonejson_oidc_bearer_validation_init(void) {
}

void lonejson_oidc_bearer_validation_cleanup(void) {
}

void lonejson_oidc_authorization_bearer_token(void) {
}

void lonejson_oidc_validate_bearer_token(void) {
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

cat >"$package_root/share/lonejson/dependencies.json" <<'EOF'
{
  "schema": "pkt.systems.dependencies.v1",
  "project": "lonejson",
  "version": "9.9.9",
  "target_id": "x86_64-linux-gnu",
  "dependencies": [
    {
      "name": "c.pkt.systems",
      "version": "0.6.0",
      "target_id": "x86_64-linux-gnu",
      "source_url": "https://github.com/sa6mwa/c.pkt.systems/releases/download/v0.6.0/c.pkt.systems-0.6.0-x86_64-linux-gnu.tar.gz",
      "sha256": "0e3f96cef656ad927ff952e5fb195fabaea551150017df0368f6fd30c5fa8039",
      "bundled": false,
      "external": true,
      "role": "release-sdk-build-dependency",
      "provides": [
        "curl",
        "openssl"
      ]
    }
  ]
}
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

missing_metadata_dist_dir="$tmp_dir/missing-metadata-dist"
missing_metadata_package_dir="$tmp_dir/missing-metadata-package"
mkdir -p "$missing_metadata_dist_dir"
cp -R "$tmp_dir/package" "$missing_metadata_package_dir"
rm -f "$missing_metadata_package_dir/liblonejson-9.9.9-x86_64-linux-gnu/share/lonejson/dependencies.json"
tar -C "$missing_metadata_package_dir" -czf \
  "$missing_metadata_dist_dir/liblonejson-9.9.9-x86_64-linux-gnu.tar.gz" \
  "liblonejson-9.9.9-x86_64-linux-gnu"
(cd "$missing_metadata_dist_dir" && sha256sum liblonejson-9.9.9-x86_64-linux-gnu.tar.gz >lonejson-9.9.9-CHECKSUMS)
missing_metadata_log="$tmp_dir/missing-metadata.log"
if "$repo_root/scripts/verify_release_archives.sh" \
  "$repo_root" \
  "$missing_metadata_dist_dir/lonejson-9.9.9-CHECKSUMS" \
  "$build_root" >"$missing_metadata_log" 2>&1; then
  printf 'expected archive verification to fail when dependency manifest is missing\n' >&2
  exit 1
fi
grep -F 'missing required file:' "$missing_metadata_log" >/dev/null

openssl_metadata_dist_dir="$tmp_dir/openssl-metadata-dist"
openssl_metadata_package_dir="$tmp_dir/openssl-metadata-package"
openssl_metadata_root="$openssl_metadata_package_dir/liblonejson-9.9.9-x86_64-linux-gnu"
mkdir -p "$openssl_metadata_dist_dir"
cp -R "$tmp_dir/package" "$openssl_metadata_package_dir"
cat >>"$openssl_metadata_root/lib/pkgconfig/lonejson.pc" <<'EOF'
Requires.private: openssl
EOF
tar -C "$openssl_metadata_package_dir" -czf \
  "$openssl_metadata_dist_dir/liblonejson-9.9.9-x86_64-linux-gnu.tar.gz" \
  "liblonejson-9.9.9-x86_64-linux-gnu"
(cd "$openssl_metadata_dist_dir" && sha256sum liblonejson-9.9.9-x86_64-linux-gnu.tar.gz >lonejson-9.9.9-CHECKSUMS)
openssl_metadata_log="$tmp_dir/openssl-metadata.log"
if "$repo_root/scripts/verify_release_archives.sh" \
  "$repo_root" \
  "$openssl_metadata_dist_dir/lonejson-9.9.9-CHECKSUMS" \
  "$build_root" >"$openssl_metadata_log" 2>&1; then
  printf 'expected archive verification to fail when pkg-config requires OpenSSL\n' >&2
  exit 1
fi
grep -F 'unexpected curl/OpenSSL pkg-config dependency' "$openssl_metadata_log" >/dev/null

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

jwt_broken_dist_dir="$tmp_dir/jwt-broken-dist"
jwt_broken_package_dir="$tmp_dir/jwt-broken-package"
jwt_broken_package_root="$jwt_broken_package_dir/liblonejson-9.9.9-x86_64-linux-gnu"
mkdir -p "$jwt_broken_dist_dir"
cp -R "$tmp_dir/package" "$jwt_broken_package_dir"

cat >"$tmp_dir/lonejson_static_without_jwt.c" <<'EOF'
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

cc -c -I"$jwt_broken_package_root/include" "$tmp_dir/lonejson_static_without_jwt.c" \
  -o "$tmp_dir/lonejson_static_without_jwt.o"
rm -f "$jwt_broken_package_root/lib/liblonejson.a"
ar rcs "$jwt_broken_package_root/lib/liblonejson.a" \
  "$tmp_dir/lonejson_static_without_jwt.o"

tar -C "$jwt_broken_package_dir" -czf \
  "$jwt_broken_dist_dir/liblonejson-9.9.9-x86_64-linux-gnu.tar.gz" \
  "liblonejson-9.9.9-x86_64-linux-gnu"
(cd "$jwt_broken_dist_dir" && sha256sum liblonejson-9.9.9-x86_64-linux-gnu.tar.gz >lonejson-9.9.9-CHECKSUMS)

jwt_broken_log="$tmp_dir/jwt-broken.log"
if "$repo_root/scripts/verify_release_archives.sh" \
  "$repo_root" \
  "$jwt_broken_dist_dir/lonejson-9.9.9-CHECKSUMS" \
  "$build_root" >"$jwt_broken_log" 2>&1; then
  printf 'expected archive verification to fail when static library lacks JWT ABI\n' >&2
  exit 1
fi
grep -F 'missing lonejson_jwt_* ABI symbol in static library' "$jwt_broken_log" >/dev/null
