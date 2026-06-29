#define LONEJSON_WITH_OPENSSL 1
#define LONEJSON_WITH_JWT 1
#define LONEJSON_WITH_CURL 1
#define LONEJSON_WITH_OIDC 1
#include "lonejson.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const char lonejson_fuzz_rs256_token[] =
    "eyJhbGciOiJSUzI1NiIsImtpZCI6InJzYS10ZXN0IiwidHlwIjoiSldUIn0."
    "eyJpc3MiOiJpc3N1ZXIiLCJzdWIiOiJzIiwiYXVkIjoiYXBpIiwiZXhwIjoyMDAwLCJuYm"
    "YiOjkwMCwiaWF0IjoxMDAwfQ."
    "PoouvDAoloqvsfTQxadOTpQGXKyeHq0lx6WQEPv0qvg59KMuy8lD-XBTBPCF_MpQGoe3DS"
    "84CSg27iktG7z12Qv6TX1gqbJUO2wkwhuW4dIWFPY9tDhI3e05W5yz8D70wARx7CL9tHKW"
    "sOpLwHRWf5ugfrq1PuofcC9atB7D-QUfrmmJ01NXbQl4aq6DJ02M7azHTLq-15X3TuE2CH"
    "N5P_zo_zkaJT8V0QhoQ3MUhUE_pBxMtAByRIUOEW32RbWjYgkwZ_zxaVkbhXv1CYznQCzi"
    "kX2wXn9OQ_1z0TCH7bT5Ao3EXEQeiK7Fhuq8lyPFbkhDc_yCjxFRjm7ufSFZbg";

static const char lonejson_fuzz_rs256_jwk_json[] =
    "{\"kty\":\"RSA\",\"kid\":\"rsa-test\",\"use\":\"sig\",\"alg\":\"RS256\","
    "\"n\":\"nGFfcf9mkkjv4XoIzgmENq-A3pTE4uT7gzmYMDB4_xwXvHaTogDTrduaIKcd-"
    "oziNa6mM1HXGk-4q8084Wvvz44ZTyRlaVKm2eRHPqjJ1hmxB80nG7iWEkORAKazobRfB8"
    "g7fGXZWhL0JsWqd51igefciKMefuvjs-2_JvusIF6uXu3jSCVRsqXkoZGnYsauGUq4Gcsp"
    "GtCHe5M4oie5kJrfbwcZgajJp4HS-ZUd4m1q12BPSuUSqi5Vb3wS6fLdVsQjxZXVqyk1O"
    "gnI3Ar5by-bbCTML4NZB8icr9uti6nO1TabV4M-skfnGyUFgbOWLxznKmHKphgpiMtHjWy"
    "YoQ\",\"e\":\"AQAB\"}";

static const char lonejson_fuzz_rs256_jwks_json[] =
    "{\"keys\":[{\"kty\":\"RSA\",\"kid\":\"rsa-test\",\"use\":\"sig\","
    "\"alg\":\"RS256\","
    "\"n\":\"nGFfcf9mkkjv4XoIzgmENq-A3pTE4uT7gzmYMDB4_xwXvHaTogDTrduaIKcd-"
    "oziNa6mM1HXGk-4q8084Wvvz44ZTyRlaVKm2eRHPqjJ1hmxB80nG7iWEkORAKazobRfB8"
    "g7fGXZWhL0JsWqd51igefciKMefuvjs-2_JvusIF6uXu3jSCVRsqXkoZGnYsauGUq4Gcsp"
    "GtCHe5M4oie5kJrfbwcZgajJp4HS-ZUd4m1q12BPSuUSqi5Vb3wS6fLdVsQjxZXVqyk1O"
    "gnI3Ar5by-bbCTML4NZB8icr9uti6nO1TabV4M-skfnGyUFgbOWLxznKmHKphgpiMtHjWy"
    "YoQ\",\"e\":\"AQAB\"}]}";

static void fuzz_decode_segment(const char *data, size_t len) {
  lonejson_error error;
  unsigned char stack_buf[128];
  unsigned char *heap_buf;
  size_t needed;
  size_t decoded_len;

  lonejson_error_init(&error);
  decoded_len = 0u;
  if (lonejson_base64url_decoded_len(data, len, &decoded_len, &error) !=
      LONEJSON_STATUS_OK) {
    decoded_len = 0u;
  }
  (void)lonejson_base64url_decode(data, len, stack_buf, sizeof(stack_buf),
                                  &needed, &error);
  if (decoded_len <= 4096u) {
    heap_buf = (unsigned char *)malloc(decoded_len == 0u ? 1u : decoded_len);
    if (heap_buf != NULL) {
      (void)lonejson_base64url_decode(data, len, heap_buf, decoded_len, &needed,
                                      &error);
      free(heap_buf);
    }
  }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  lonejson_jwt_compact jwt;
  lonejson_jwt_header header;
  lonejson_jwt_claims claims;
  lonejson_jwt_claim_policy policy;
  lonejson_jwk jwk;
  lonejson_jwks jwks;
  lonejson_oidc_jwks_cache cache;
  lonejson_oidc_jwks_cache_parse cache_parse;
  lonejson_oidc_jwks_cache_policy cache_policy;
  lonejson_oauth2_client_credentials credentials;
  lonejson_oauth2_token_response token_response;
  lonejson_oidc_pkce pkce;
  lonejson_oidc_authorization_request authorization_request;
  lonejson_oidc_authorization_callback authorization_callback;
  lonejson_oidc_bearer_validation_request bearer_request;
  lonejson_oidc_bearer_validation bearer_validation;
  lonejson_oidc_discovery discovery;
  lonejson_owned_buffer discovery_url;
  lonejson_owned_buffer form_body;
  lonejson_owned_buffer pkce_challenge;
  lonejson_owned_buffer authorization_url;
  lonejson_error error;
  lonejson *runtime;
  char *text;
  const char *mode_text;
  size_t mode_size;

  if (size > 65536u) {
    return 0;
  }
  text = (char *)malloc(size + 1u);
  if (text == NULL) {
    return 0;
  }
  if (size != 0u) {
    memcpy(text, data, size);
  }
  text[size] = '\0';

  fuzz_decode_segment(text, size);
  lonejson_error_init(&error);
  if (lonejson_jwt_parse_compact(text, size, &jwt, &error) ==
      LONEJSON_STATUS_OK) {
    fuzz_decode_segment(jwt.header.data, jwt.header.len);
    fuzz_decode_segment(jwt.payload.data, jwt.payload.len);
    fuzz_decode_segment(jwt.signature.data, jwt.signature.len);
    fuzz_decode_segment(jwt.signing_input.data, jwt.signing_input.len);
  }
  runtime = lonejson_new(NULL, &error);
  if (runtime != NULL) {
    static const char *const algs[] = {"RS256", "ES256"};
    static const char *const issuers[] = {"issuer", "https://issuer.example"};
    static const char *const audiences[] = {"api", "client"};
    static const char *const required[] = {"iss", "aud"};

    memset(&policy, 0, sizeof(policy));
    policy.accepted_algs = algs;
    policy.accepted_alg_count = sizeof(algs) / sizeof(algs[0]);
    policy.accepted_issuers = issuers;
    policy.accepted_issuer_count = sizeof(issuers) / sizeof(issuers[0]);
    policy.accepted_audiences = audiences;
    policy.accepted_audience_count = sizeof(audiences) / sizeof(audiences[0]);
    policy.required_claims = required;
    policy.required_claim_count = sizeof(required) / sizeof(required[0]);
    policy.now = 1000;
    policy.allowed_clock_skew = 30;
    policy.max_token_bytes = 65536u;
    policy.max_decoded_header_bytes = 4096u;
    policy.max_decoded_claims_bytes = 16384u;
    lonejson_jwt_header_init(&header);
    lonejson_jwt_claims_init(&claims);
    if (lonejson_jwt_decode_compact(runtime, text, size, &policy, &header,
                                    &claims, &error) == LONEJSON_STATUS_OK) {
      (void)lonejson_jwt_validate_claims(&header, &claims, &policy, &error);
    }
    lonejson_jwt_header_cleanup(&header);
    lonejson_jwt_claims_cleanup(&claims);

    if (size > 4u && memcmp(text, "sig:", 4u) == 0) {
      mode_text = text + 4u;
      mode_size = size - 4u;
      lonejson_jwt_header_init(&header);
      lonejson_jwt_claims_init(&claims);
      if (lonejson_jwt_decode_compact(runtime, mode_text, mode_size, &policy,
                                      &header, &claims, &error) ==
          LONEJSON_STATUS_OK) {
        lonejson_jwk_init(&jwk);
        if (lonejson_jwk_parse_json(runtime, lonejson_fuzz_rs256_jwk_json,
                                    strlen(lonejson_fuzz_rs256_jwk_json), &jwk,
                                    &error) == LONEJSON_STATUS_OK &&
            lonejson_jwt_parse_compact(mode_text, mode_size, &jwt, &error) ==
                LONEJSON_STATUS_OK) {
          (void)lonejson_jwt_validate_signature(&jwt, &header, &jwk, &error);
        }
        lonejson_jwk_cleanup(&jwk);
      }
      lonejson_jwt_header_cleanup(&header);
      lonejson_jwt_claims_cleanup(&claims);
    }

    if (size > 4u && memcmp(text, "jwk:", 4u) == 0) {
      mode_text = text + 4u;
      mode_size = size - 4u;
      lonejson_jwk_init(&jwk);
      if (lonejson_jwk_parse_json(runtime, mode_text, mode_size, &jwk,
                                  &error) == LONEJSON_STATUS_OK) {
        if (lonejson_jwt_parse_compact(lonejson_fuzz_rs256_token,
                                       strlen(lonejson_fuzz_rs256_token), &jwt,
                                       &error) == LONEJSON_STATUS_OK) {
          lonejson_jwt_header_init(&header);
          lonejson_jwt_claims_init(&claims);
          if (lonejson_jwt_decode_compact(runtime, lonejson_fuzz_rs256_token,
                                          strlen(lonejson_fuzz_rs256_token),
                                          &policy, &header, &claims,
                                          &error) == LONEJSON_STATUS_OK) {
            (void)lonejson_jwt_validate_signature(&jwt, &header, &jwk, &error);
          }
          lonejson_jwt_header_cleanup(&header);
          lonejson_jwt_claims_cleanup(&claims);
        }
      }
      lonejson_jwk_cleanup(&jwk);
    }

    lonejson_jwks_init(&jwks);
    if (lonejson_jwks_parse_json(runtime, text, size, &jwks, &error) ==
        LONEJSON_STATUS_OK) {
      lonejson_jwk_select_options options;
      const lonejson_jwk *selected;
      memset(&options, 0, sizeof(options));
      (void)lonejson_jwks_select(&jwks, &options, &selected, &error);
      if (jwks.keys.count != 0u) {
        const lonejson_jwk *first = (const lonejson_jwk *)jwks.keys.items;
        options.kid = first->kid;
        options.kty = first->kty;
        options.alg = first->alg;
        options.use = first->use;
        (void)lonejson_jwks_select(&jwks, &options, &selected, &error);
      }
    }
    lonejson_jwks_cleanup(&jwks);

    memset(&cache_policy, 0, sizeof(cache_policy));
    cache_policy.issuer = "https://issuer.example";
    cache_policy.jwks_uri = "https://issuer.example/jwks";
    cache_policy.max_jwks_bytes = 4096u;
    cache_policy.now = 1000;
    cache_policy.ttl_seconds = 60;
    lonejson_oidc_jwks_cache_init(&cache);
    if (lonejson_oidc_jwks_cache_update_json(runtime, &cache, &cache_policy,
                                             text, size, &error) ==
        LONEJSON_STATUS_OK) {
      lonejson_jwk_select_options options;
      const lonejson_jwk *selected;
      memset(&options, 0, sizeof(options));
      (void)lonejson_oidc_jwks_cache_is_fresh(&cache, &cache_policy);
      (void)lonejson_oidc_jwks_cache_select(&cache, &cache_policy, &options,
                                            &selected, &error);
    }
    lonejson_oidc_jwks_cache_cleanup(&cache);

    if (size > 6u && memcmp(text, "cache:", 6u) == 0) {
      mode_text = text + 6u;
      mode_size = size - 6u;
      lonejson_oidc_jwks_cache_init(&cache);
      if (lonejson_oidc_jwks_cache_parse_init(&cache_parse, runtime, &cache,
                                              &cache_policy) ==
          LONEJSON_STATUS_OK) {
        if (mode_size != 0u) {
          (void)lonejson_oidc_jwks_cache_write_callback(
              (char *)mode_text, 1u, mode_size, &cache_parse);
        }
        (void)lonejson_oidc_jwks_cache_parse_finish(&cache_parse);
        cache_parse.cleanup(&cache_parse);
      }
      lonejson_oidc_jwks_cache_cleanup(&cache);
    }

    memset(&credentials, 0, sizeof(credentials));
    credentials.client_id = size > 0u ? text : "client";
    credentials.client_secret = "secret";
    credentials.scope = "read write";
    credentials.audience = size > 4u ? text + (size / 2u) : NULL;
    credentials.max_body_bytes = 4096u;
    lonejson_owned_buffer_init(&form_body);
    (void)lonejson_oauth2_client_credentials_body(&credentials, &form_body,
                                                  &error);
    lonejson_owned_buffer_free(&form_body);

    lonejson_oauth2_token_response_init(&token_response);
    (void)lonejson_oauth2_token_response_parse_json(
        runtime, text, size, 4096u, &token_response, &error);
    lonejson_oauth2_token_response_cleanup(&token_response);

    lonejson_owned_buffer_init(&pkce_challenge);
    if (size >= 43u && size <= 128u) {
      (void)lonejson_oidc_pkce_challenge(text, &pkce_challenge, &error);
    }
    lonejson_owned_buffer_free(&pkce_challenge);

    lonejson_oidc_pkce_init(&pkce);
    (void)lonejson_oidc_pkce_generate(32u, &pkce, &error);
    lonejson_oidc_pkce_cleanup(&pkce);

    memset(&authorization_request, 0, sizeof(authorization_request));
    authorization_request.authorization_endpoint = "https://issuer.example/auth";
    authorization_request.client_id = "client";
    authorization_request.redirect_uri = "http://127.0.0.1/callback";
    authorization_request.scope = "openid profile";
    authorization_request.state = "state";
    authorization_request.nonce = "nonce";
    authorization_request.code_challenge = "challenge";
    authorization_request.max_url_bytes = 4096u;
    lonejson_owned_buffer_init(&authorization_url);
    (void)lonejson_oidc_authorization_url(&authorization_request,
                                          &authorization_url, &error);
    lonejson_owned_buffer_free(&authorization_url);

    lonejson_oidc_authorization_callback_init(&authorization_callback);
    (void)lonejson_oidc_authorization_callback_parse_query(
        text, size, "state", 4096u, &authorization_callback, &error);
    lonejson_oidc_authorization_callback_cleanup(&authorization_callback);

    (void)lonejson_oidc_authorization_bearer_token(text, &jwt.header, &error);
    lonejson_oidc_jwks_cache_init(&cache);
    lonejson_oidc_bearer_validation_init(&bearer_validation);
    if (lonejson_oidc_jwks_cache_update_json(
            runtime, &cache, &cache_policy, lonejson_fuzz_rs256_jwks_json,
            strlen(lonejson_fuzz_rs256_jwks_json), &error) ==
        LONEJSON_STATUS_OK) {
      memset(&bearer_request, 0, sizeof(bearer_request));
      bearer_request.authorization_header = text;
      bearer_request.jwks_cache = &cache;
      bearer_request.jwks_policy = &cache_policy;
      bearer_request.claim_policy = &policy;
      (void)lonejson_oidc_validate_bearer_token(runtime, &bearer_request,
                                                &bearer_validation, &error);
      bearer_request.authorization_header =
          "Bearer "
          "eyJhbGciOiJSUzI1NiIsImtpZCI6InJzYS10ZXN0IiwidHlwIjoiSldUIn0."
          "eyJpc3MiOiJpc3N1ZXIiLCJzdWIiOiJzIiwiYXVkIjoiYXBpIiwiZXhwIjoyMDAw"
          "LCJuYmYiOjkwMCwiaWF0IjoxMDAwfQ."
          "PoouvDAoloqvsfTQxadOTpQGXKyeHq0lx6WQEPv0qvg59KMuy8lD-XBTBPCF_MpQGo"
          "e3DS84CSg27iktG7z12Qv6TX1gqbJUO2wkwhuW4dIWFPY9tDhI3e05W5yz8D70wAR"
          "x7CL9tHKWsOpLwHRWf5ugfrq1PuofcC9atB7D-QUfrmmJ01NXbQl4aq6DJ02M7azHT"
          "Lq-15X3TuE2CHN5P_zo_zkaJT8V0QhoQ3MUhUE_pBxMtAByRIUOEW32RbWjYgkwZ_"
          "zxaVkbhXv1CYznQCzikX2wXn9OQ_1z0TCH7bT5Ao3EXEQeiK7Fhuq8lyPFbkhDc_y"
          "CjxFRjm7ufSFZbg";
      (void)lonejson_oidc_validate_bearer_token(runtime, &bearer_request,
                                                &bearer_validation, &error);
    }
    lonejson_oidc_bearer_validation_cleanup(&bearer_validation);
    lonejson_oidc_jwks_cache_cleanup(&cache);

    lonejson_owned_buffer_init(&discovery_url);
    (void)lonejson_oidc_discovery_url(text, &discovery_url, &error);
    lonejson_owned_buffer_free(&discovery_url);

    lonejson_oidc_discovery_init(&discovery);
    if (lonejson_oidc_discovery_parse_json(runtime, text, size, &discovery,
                                           &error) == LONEJSON_STATUS_OK) {
      (void)lonejson_oidc_discovery_validate_issuer(
          &discovery, "https://issuer.example", &error);
      if (discovery.issuer != NULL) {
        (void)lonejson_oidc_discovery_validate_issuer(&discovery,
                                                      discovery.issuer, &error);
      }
    }
    lonejson_oidc_discovery_cleanup(&discovery);
    lonejson_free(runtime);
  }
  free(text);
  return 0;
}
