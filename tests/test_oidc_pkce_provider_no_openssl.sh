#!/usr/bin/env bash
set -euo pipefail

repo_root=${1:?usage: test_oidc_pkce_provider_no_openssl.sh REPO_ROOT}
cc_bin=${CC:-cc}
tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

cat >"$tmp_dir/pkce_provider_no_openssl.c" <<'C_EOF'
#include "lonejson.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define EXPECT(expr)                                                            \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "EXPECT failed at %s:%d: %s\n", __FILE__, __LINE__,     \
              #expr);                                                          \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

static lonejson_status fake_random_bytes(void *user, unsigned char *dst,
                                         size_t len, lonejson_error *error) {
  size_t i;
  (void)user;
  (void)error;
  for (i = 0u; i < len; ++i) {
    dst[i] = (unsigned char)i;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status fake_sha256(void *user, const void *data, size_t len,
                                   unsigned char out[32],
                                   lonejson_error *error) {
  static const char verifier[] = "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
  size_t i;
  (void)error;
  if (data != NULL && len == sizeof(verifier) - 1u &&
      memcmp(data, verifier, sizeof(verifier) - 1u) == 0 && user != NULL) {
    memcpy(out, user, 32u);
    return LONEJSON_STATUS_OK;
  }
  for (i = 0u; i < 32u; ++i) {
    out[i] = (unsigned char)i;
  }
  return LONEJSON_STATUS_OK;
}

int main(void) {
  static const char verifier[] = "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
  static const unsigned char verifier_digest[32] = {
      0x13u, 0xd3u, 0x1eu, 0x96u, 0x1au, 0x1au, 0xd8u, 0xecu,
      0x2fu, 0x16u, 0xb1u, 0x0cu, 0x4cu, 0x98u, 0x2eu, 0x08u,
      0x76u, 0xa8u, 0x78u, 0xadu, 0x6du, 0xf1u, 0x44u, 0x56u,
      0x6eu, 0xe1u, 0x89u, 0x4au, 0xcbu, 0x70u, 0xf9u, 0xc3u};
  lonejson_auth_provider provider;
  lonejson_auth_provider sha_only;
  lonejson_auth_provider random_only;
  lonejson_owned_buffer challenge;
  lonejson_oidc_pkce pkce;
  lonejson_config config;
  lonejson_error error;
  lonejson *runtime;

  lonejson_error_init(&error);
  lonejson_owned_buffer_init(&challenge);
  EXPECT(lonejson_oidc_pkce_challenge("too-short", &challenge, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_oidc_pkce_challenge(verifier, &challenge, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);

  memset(&provider, 0, sizeof(provider));
  provider.user_data = (void *)verifier_digest;
  provider.random_bytes = fake_random_bytes;
  provider.sha256 = fake_sha256;
  config = lonejson_default_config();
  config.auth_provider = &provider;
  runtime = lonejson_new(&config, &error);
  EXPECT(runtime != NULL);
  EXPECT(runtime->oidc_pkce_challenge_with_runtime != NULL);
  EXPECT(runtime->oidc_pkce_generate_with_runtime != NULL);
  EXPECT(runtime->oidc_pkce_challenge_with_runtime(runtime, verifier,
                                                   &challenge, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(strcmp(challenge.data,
                "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM") == 0);
  lonejson_owned_buffer_free(&challenge);

  lonejson_oidc_pkce_init(&pkce);
  EXPECT(lonejson_oidc_pkce_generate_with_runtime(runtime, 32u, &pkce,
                                                  &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(pkce.code_verifier,
                "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8") == 0);
  EXPECT(strcmp(pkce.code_challenge,
                "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8") == 0);
  lonejson_oidc_pkce_cleanup(&pkce);
  lonejson_free(runtime);

  memset(&sha_only, 0, sizeof(sha_only));
  sha_only.sha256 = fake_sha256;
  config = lonejson_default_config();
  config.auth_provider = &sha_only;
  runtime = lonejson_new(&config, &error);
  EXPECT(lonejson_oidc_pkce_generate_with_runtime(runtime, 32u, &pkce,
                                                  &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  lonejson_free(runtime);

  memset(&random_only, 0, sizeof(random_only));
  random_only.random_bytes = fake_random_bytes;
  config = lonejson_default_config();
  config.auth_provider = &random_only;
  runtime = lonejson_new(&config, &error);
  EXPECT(lonejson_oidc_pkce_challenge_with_runtime(runtime, verifier,
                                                   &challenge, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  lonejson_free(runtime);

  return failures == 0 ? 0 : 1;
}
C_EOF

"$cc_bin" -std=c89 -Wall -Wextra -Werror -Wpedantic -pedantic-errors \
  -D_POSIX_C_SOURCE=200809L -D_FILE_OFFSET_BITS=64 \
  -DLONEJSON_WITH_JWT -DLONEJSON_WITH_OIDC \
  -I"$repo_root/include" \
  -I"$repo_root/src" \
  "$tmp_dir/pkce_provider_no_openssl.c" \
  "$repo_root/src/lonejson.c" \
  -o "$tmp_dir/pkce_provider_no_openssl"
"$tmp_dir/pkce_provider_no_openssl"
