#ifdef LONEJSON_WITH_JWT
#ifdef LONEJSON_WITH_OPENSSL
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>
#include <openssl/params.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#endif

static int lonejson__base64url_value(unsigned char ch) {
  if (ch >= (unsigned char)'A' && ch <= (unsigned char)'Z') {
    return (int)(ch - (unsigned char)'A');
  }
  if (ch >= (unsigned char)'a' && ch <= (unsigned char)'z') {
    return (int)(ch - (unsigned char)'a') + 26;
  }
  if (ch >= (unsigned char)'0' && ch <= (unsigned char)'9') {
    return (int)(ch - (unsigned char)'0') + 52;
  }
  if (ch == (unsigned char)'-') {
    return 62;
  }
  if (ch == (unsigned char)'_') {
    return 63;
  }
  return -1;
}

static const lonejson_field lonejson__jwk_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(lonejson_jwk, kty, "kty"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_jwk, kid, "kid"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_jwk, alg, "alg"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_jwk, use, "use"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_jwk, crv, "crv"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_jwk, n, "n"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_jwk, e, "e"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_jwk, x, "x"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_jwk, y, "y"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_jwk, k, "k")};
LONEJSON_MAP_DEFINE(lonejson__jwk_map, lonejson_jwk, lonejson__jwk_fields);

static const lonejson_field lonejson__jwks_fields[] = {
    LONEJSON_FIELD_OBJECT_ARRAY(lonejson_jwks, keys, "keys", lonejson_jwk,
                                &lonejson__jwk_map, LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(lonejson__jwks_map, lonejson_jwks, lonejson__jwks_fields);

#ifdef LONEJSON_WITH_OIDC
static const lonejson_field lonejson__oidc_discovery_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(lonejson_oidc_discovery, issuer, "issuer"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oidc_discovery, authorization_endpoint,
                                "authorization_endpoint"),
    LONEJSON_FIELD_STRING_ALLOC_REQ(lonejson_oidc_discovery, token_endpoint,
                                    "token_endpoint"),
    LONEJSON_FIELD_STRING_ALLOC_REQ(lonejson_oidc_discovery, jwks_uri,
                                    "jwks_uri")};
LONEJSON_MAP_DEFINE(lonejson__oidc_discovery_map, lonejson_oidc_discovery,
                    lonejson__oidc_discovery_fields);

static const lonejson_field lonejson__oauth2_token_response_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oauth2_token_response, access_token,
                                "access_token"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oauth2_token_response, token_type,
                                "token_type"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oauth2_token_response, refresh_token,
                                "refresh_token"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oauth2_token_response, scope, "scope"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oauth2_token_response, id_token,
                                "id_token"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oauth2_token_response, error, "error"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oauth2_token_response,
                                error_description, "error_description"),
    LONEJSON_FIELD_STRING_ALLOC(lonejson_oauth2_token_response, error_uri,
                                "error_uri"),
    LONEJSON_FIELD_I64_PRESENT(lonejson_oauth2_token_response, expires_in,
                               has_expires_in, "expires_in")};
LONEJSON_MAP_DEFINE(lonejson__oauth2_token_response_map,
                    lonejson_oauth2_token_response,
                    lonejson__oauth2_token_response_fields);
#endif

#define LONEJSON__JWT_DEFAULT_MAX_HEADER_BYTES (16u * 1024u)
#define LONEJSON__JWT_DEFAULT_MAX_CLAIMS_BYTES (256u * 1024u)
#define LONEJSON__JWT_MAX_RSA_COMPONENT_BYTES (8u * 1024u)
#define LONEJSON__OIDC_DEFAULT_MAX_DISCOVERY_BYTES (256u * 1024u)
#define LONEJSON__OIDC_DEFAULT_MAX_JWKS_BYTES (1024u * 1024u)
#define LONEJSON__OAUTH2_DEFAULT_MAX_FORM_BODY_BYTES (64u * 1024u)
#define LONEJSON__OAUTH2_DEFAULT_MAX_TOKEN_RESPONSE_BYTES (1024u * 1024u)
#define LONEJSON__OIDC_DEFAULT_MAX_AUTH_URL_BYTES (16u * 1024u)
#define LONEJSON__OIDC_DEFAULT_MAX_CALLBACK_QUERY_BYTES (16u * 1024u)
#define LONEJSON__OIDC_PKCE_DEFAULT_VERIFIER_BYTES 32u
#define LONEJSON__OIDC_PKCE_MIN_VERIFIER_CHARS 43u
#define LONEJSON__OIDC_PKCE_MAX_VERIFIER_CHARS 128u

#ifdef LONEJSON_WITH_OIDC
void lonejson_http_response_init(lonejson_http_response *response) {
  if (response != NULL) {
    memset(response, 0, sizeof(*response));
    lonejson_owned_buffer_init(&response->body);
  }
}

void lonejson_http_response_cleanup(lonejson_http_response *response) {
  if (response == NULL) {
    return;
  }
  lonejson__owned_free(response->content_type);
  lonejson_owned_buffer_free(&response->body);
  lonejson_http_response_init(response);
}

lonejson_status lonejson_http_provider_init(
    lonejson_http_provider *provider,
    const lonejson_http_provider_config *config, lonejson_error *error) {
  lonejson__clear_error(error);
  if (provider == NULL || config == NULL || config->request == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "HTTP provider config and request callback "
                                   "are required");
  }
  memset(provider, 0, sizeof(*provider));
  provider->user_data = config->user_data;
  provider->user_agent = config->user_agent;
  provider->request = config->request;
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_http_provider_init_simple(
    lonejson_http_provider *provider, void *user_data, const char *user_agent,
    lonejson_status (*request)(void *user_data,
                               const lonejson_http_request *request,
                               lonejson_http_response *response,
                               lonejson_error *error),
    lonejson_error *error) {
  lonejson_http_provider_config config;

  memset(&config, 0, sizeof(config));
  config.user_data = user_data;
  config.user_agent = user_agent;
  config.request = request;
  return lonejson_http_provider_init(provider, &config, error);
}

static lonejson_status lonejson__http_request_validate(
    const lonejson_http_request *request, lonejson_error *error) {
  if (request == NULL || request->method == NULL || request->url == NULL ||
      request->method[0] == '\0' || request->url[0] == '\0') {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "HTTP method and URL are required");
  }
  if (request->body == NULL && request->body_len != 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "HTTP request body is required");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__http_provider_request(
    lonejson *runtime, const lonejson_http_request *request,
    lonejson_http_response *response, lonejson_error *error) {
  lonejson__runtime_borrow borrow;
  const lonejson_runtime *runtime_state;
  lonejson_http_request local_request;
  lonejson_status status;

  if (response == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "HTTP response output is required");
  }
  lonejson_http_response_cleanup(response);
  status = lonejson__http_request_validate(request, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  runtime_state = lonejson__require_runtime_borrow(runtime, &borrow, error);
  if (runtime_state == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (!runtime_state->has_http_provider ||
      runtime_state->http_provider.request == NULL) {
    lonejson__runtime_borrow_release(&borrow);
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "runtime HTTP provider is not configured");
  }
  local_request = *request;
  if (local_request.user_agent == NULL) {
    local_request.user_agent = runtime_state->http_provider.user_agent;
  }
  status = runtime_state->http_provider.request(
      runtime_state->http_provider.user_data, &local_request, response, error);
  lonejson__runtime_borrow_release(&borrow);
  return status;
}

static lonejson_status
lonejson__http_require_success(const lonejson_http_response *response,
                               const char *what, lonejson_error *error) {
  if (response == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "HTTP response is required");
  }
  if (response->status_code < 200 || response->status_code >= 300) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "%s returned HTTP status %ld", what,
                               response->status_code);
  }
  return LONEJSON_STATUS_OK;
}
#endif

typedef enum lonejson__jwt_claim_field {
  LONEJSON__JWT_FIELD_NONE = 0,
  LONEJSON__JWT_FIELD_HEADER_ALG,
  LONEJSON__JWT_FIELD_HEADER_KID,
  LONEJSON__JWT_FIELD_HEADER_TYP,
  LONEJSON__JWT_FIELD_ISS,
  LONEJSON__JWT_FIELD_SUB,
  LONEJSON__JWT_FIELD_AUD_STRING,
  LONEJSON__JWT_FIELD_AUD_ARRAY_ITEM,
  LONEJSON__JWT_FIELD_EXP,
  LONEJSON__JWT_FIELD_NBF,
  LONEJSON__JWT_FIELD_IAT
} lonejson__jwt_claim_field;

typedef struct lonejson__jwt_claim_visit {
  int header_mode;
  lonejson_jwt_header *header;
  lonejson_jwt_claims *claims;
  lonejson__jwt_claim_field active;
  char *buffer;
  size_t len;
  size_t capacity;
  int root_object_seen;
  int seen_alg;
  int seen_kid;
  int seen_typ;
  int seen_iss;
  int seen_sub;
  int seen_aud;
  int seen_exp;
  int seen_nbf;
  int seen_iat;
} lonejson__jwt_claim_visit;

static lonejson_status lonejson__base64url_check(const char *data, size_t len,
                                                 size_t *out_len,
                                                 lonejson_error *error) {
  size_t i;
  size_t rem;
  size_t decoded_len;

  if (data == NULL && len != 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "base64url input is required");
  }
  if (out_len == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "base64url decoded length output is required");
  }
  for (i = 0u; i < len; ++i) {
    if (lonejson__base64url_value((unsigned char)data[i]) < 0) {
      return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, i, 0u, 0u,
                                 "invalid base64url character");
    }
  }
  rem = len & 3u;
  if (rem == 1u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, len, 0u, 0u,
                               "invalid base64url segment length");
  }
  decoded_len = (len / 4u) * 3u;
  if (rem == 2u) {
    decoded_len += 1u;
  } else if (rem == 3u) {
    decoded_len += 2u;
  }
  *out_len = decoded_len;
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_base64url_decoded_len(const char *data, size_t len,
                                               size_t *out_len,
                                               lonejson_error *error) {
  lonejson__clear_error(error);
  return lonejson__base64url_check(data, len, out_len, error);
}

lonejson_status lonejson_base64url_decode(const char *data, size_t len,
                                          unsigned char *out, size_t capacity,
                                          size_t *needed,
                                          lonejson_error *error) {
  size_t decoded_len;
  size_t in_pos;
  size_t out_pos;
  lonejson_status status;

  lonejson__clear_error(error);
  if (needed == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "base64url needed output is required");
  }
  status = lonejson__base64url_check(data, len, &decoded_len, error);
  if (status != LONEJSON_STATUS_OK) {
    *needed = 0u;
    return status;
  }
  *needed = decoded_len;
  if (decoded_len > capacity) {
    return lonejson__set_error(error, LONEJSON_STATUS_TRUNCATED, 0u, 0u, 0u,
                               "base64url output buffer is too small");
  }
  if (decoded_len != 0u && out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "base64url output buffer is required");
  }

  in_pos = 0u;
  out_pos = 0u;
  while (in_pos < len) {
    int values[4];
    size_t group_len;
    unsigned int bits;

    group_len = len - in_pos;
    if (group_len > 4u) {
      group_len = 4u;
    }
    values[0] = lonejson__base64url_value((unsigned char)data[in_pos]);
    values[1] =
        (group_len > 1u)
            ? lonejson__base64url_value((unsigned char)data[in_pos + 1u])
            : 0;
    values[2] =
        (group_len > 2u)
            ? lonejson__base64url_value((unsigned char)data[in_pos + 2u])
            : 0;
    values[3] =
        (group_len > 3u)
            ? lonejson__base64url_value((unsigned char)data[in_pos + 3u])
            : 0;
    bits = ((unsigned int)values[0] << 18) | ((unsigned int)values[1] << 12) |
           ((unsigned int)values[2] << 6) | (unsigned int)values[3];
    if (group_len >= 2u) {
      out[out_pos++] = (unsigned char)((bits >> 16) & 0xffu);
    }
    if (group_len >= 3u) {
      out[out_pos++] = (unsigned char)((bits >> 8) & 0xffu);
    }
    if (group_len == 4u) {
      out[out_pos++] = (unsigned char)(bits & 0xffu);
    }
    in_pos += group_len;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__jwt_check_segment(const char *token,
                                                   size_t begin, size_t end,
                                                   int allow_empty,
                                                   lonejson_error *error) {
  size_t decoded_len;
  lonejson_status status;

  if (end < begin) {
    return lonejson__set_error(error, LONEJSON_STATUS_INTERNAL_ERROR, begin, 0u,
                               0u, "invalid JWT segment bounds");
  }
  if (!allow_empty && end == begin) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, begin, 0u,
                               0u, "JWT compact segment must not be empty");
  }
  status = lonejson__base64url_check(token + begin, end - begin, &decoded_len,
                                     error);
  if (status != LONEJSON_STATUS_OK) {
    if (error != NULL) {
      error->offset += begin;
    }
    return status;
  }
  (void)decoded_len;
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_jwt_parse_compact(const char *token, size_t len,
                                           lonejson_jwt_compact *out,
                                           lonejson_error *error) {
  size_t i;
  size_t first_dot;
  size_t second_dot;
  int seen_first;
  int seen_second;
  lonejson_status status;

  lonejson__clear_error(error);
  if (token == NULL || out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JWT compact token and output are required");
  }
  memset(out, 0, sizeof(*out));
  first_dot = 0u;
  second_dot = 0u;
  seen_first = 0;
  seen_second = 0;
  for (i = 0u; i < len; ++i) {
    if (token[i] == '.') {
      if (!seen_first) {
        first_dot = i;
        seen_first = 1;
      } else if (!seen_second) {
        second_dot = i;
        seen_second = 1;
      } else {
        return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, i, 0u,
                                   0u,
                                   "JWT compact token has too many segments");
      }
    }
  }
  if (!seen_first || !seen_second) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, len, 0u, 0u,
                               "JWT compact token must contain three segments");
  }
  status = lonejson__jwt_check_segment(token, 0u, first_dot, 0, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status =
      lonejson__jwt_check_segment(token, first_dot + 1u, second_dot, 0, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__jwt_check_segment(token, second_dot + 1u, len, 1, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  out->header.data = token;
  out->header.len = first_dot;
  out->payload.data = token + first_dot + 1u;
  out->payload.len = second_dot - first_dot - 1u;
  out->signature.data = token + second_dot + 1u;
  out->signature.len = len - second_dot - 1u;
  out->signing_input.data = token;
  out->signing_input.len = second_dot;
  return LONEJSON_STATUS_OK;
}

static int lonejson__auth_streq(const char *a, const char *b) {
  if (a == NULL || b == NULL) {
    return 0;
  }
  return strcmp(a, b) == 0;
}

static lonejson_status lonejson__jwk_require_member(const lonejson_jwk *jwk,
                                                    const char *value,
                                                    const char *member,
                                                    lonejson_error *error) {
  (void)jwk;
  if (value == NULL || value[0] == '\0') {
    char message[160];
    (void)snprintf(message, sizeof(message), "JWK %s member is required",
                   member);
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               message);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__jwk_check_base64url_member(const char *value, const char *member,
                                     int required, lonejson_error *error) {
  size_t decoded_len;
  lonejson_status status;

  if (value == NULL) {
    if (!required) {
      return LONEJSON_STATUS_OK;
    }
    return lonejson__jwk_require_member(NULL, value, member, error);
  }
  if (required && value[0] == '\0') {
    return lonejson__jwk_require_member(NULL, value, member, error);
  }
  status = lonejson__base64url_check(value, strlen(value), &decoded_len, error);
  if (status != LONEJSON_STATUS_OK) {
    if (error != NULL) {
      (void)snprintf(error->message, sizeof(error->message),
                     "invalid base64url in JWK %s member", member);
    }
    return status;
  }
  (void)decoded_len;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__jwk_validate(lonejson_jwk *jwk,
                                              lonejson_error *error) {
  lonejson_status status;

  if (jwk == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JWK object is required");
  }
  if (jwk->kty == NULL || jwk->kty[0] == '\0') {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               "JWK kty member is required");
  }
  if (lonejson__auth_streq(jwk->kty, "RSA")) {
    status = lonejson__jwk_check_base64url_member(jwk->n, "n", 1, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    status = lonejson__jwk_check_base64url_member(jwk->e, "e", 1, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  } else if (lonejson__auth_streq(jwk->kty, "EC")) {
    status = lonejson__jwk_require_member(jwk, jwk->crv, "crv", error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    status = lonejson__jwk_check_base64url_member(jwk->x, "x", 1, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    status = lonejson__jwk_check_base64url_member(jwk->y, "y", 1, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  } else if (lonejson__auth_streq(jwk->kty, "oct")) {
    status = lonejson__jwk_check_base64url_member(jwk->k, "k", 1, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  } else if (lonejson__auth_streq(jwk->kty, "OKP")) {
    status = lonejson__jwk_require_member(jwk, jwk->crv, "crv", error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    status = lonejson__jwk_check_base64url_member(jwk->x, "x", 1, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  status = lonejson__jwk_check_base64url_member(jwk->n, "n", 0, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__jwk_check_base64url_member(jwk->e, "e", 0, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__jwk_check_base64url_member(jwk->x, "x", 0, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__jwk_check_base64url_member(jwk->y, "y", 0, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__jwk_check_base64url_member(jwk->k, "k", 0, error);
}

void lonejson_jwk_init(lonejson_jwk *jwk) {
  if (jwk != NULL) {
    memset(jwk, 0, sizeof(*jwk));
  }
}

void lonejson_jwk_cleanup(lonejson_jwk *jwk) {
  if (jwk != NULL) {
    lonejson_cleanup(&lonejson__jwk_map, jwk);
    memset(jwk, 0, sizeof(*jwk));
  }
}

void lonejson_jwks_init(lonejson_jwks *jwks) {
  if (jwks != NULL) {
    memset(jwks, 0, sizeof(*jwks));
  }
}

void lonejson_jwks_cleanup(lonejson_jwks *jwks) {
  if (jwks != NULL) {
    lonejson_cleanup(&lonejson__jwks_map, jwks);
    memset(jwks, 0, sizeof(*jwks));
  }
}

lonejson_status lonejson_jwk_parse_json(lonejson *runtime, const char *json,
                                        size_t len, lonejson_jwk *out,
                                        lonejson_error *error) {
  lonejson_status status;

  lonejson__clear_error(error);
  if (out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JWK output is required");
  }
  if (json == NULL && len != 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JWK JSON input is required");
  }
  status =
      lonejson_parse_buffer(runtime, &lonejson__jwk_map, out, json, len, error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_jwk_cleanup(out);
    return status;
  }
  status = lonejson__jwk_validate(out, error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_jwk_cleanup(out);
  }
  return status;
}

lonejson_status lonejson_jwks_parse_json(lonejson *runtime, const char *json,
                                         size_t len, lonejson_jwks *out,
                                         lonejson_error *error) {
  lonejson_status status;
  size_t i;

  lonejson__clear_error(error);
  if (out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JWKS output is required");
  }
  if (json == NULL && len != 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JWKS JSON input is required");
  }
  status = lonejson_parse_buffer(runtime, &lonejson__jwks_map, out, json, len,
                                 error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_jwks_cleanup(out);
    return status;
  }
  if (out->keys.count == 0u) {
    lonejson_jwks_cleanup(out);
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               "JWKS keys array must not be empty");
  }
  for (i = 0u; i < out->keys.count; ++i) {
    lonejson_jwk *jwk = &((lonejson_jwk *)out->keys.items)[i];
    status = lonejson__jwk_validate(jwk, error);
    if (status != LONEJSON_STATUS_OK) {
      lonejson_jwks_cleanup(out);
      return status;
    }
  }
  return LONEJSON_STATUS_OK;
}

static int lonejson__jwk_match_filter(const char *value, const char *filter) {
  if (filter == NULL) {
    return 1;
  }
  return value != NULL && strcmp(value, filter) == 0;
}

lonejson_status lonejson_jwks_select(const lonejson_jwks *jwks,
                                     const lonejson_jwk_select_options *options,
                                     const lonejson_jwk **out,
                                     lonejson_error *error) {
  size_t i;

  lonejson__clear_error(error);
  if (jwks == NULL || out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JWKS and output pointer are required");
  }
  *out = NULL;
  for (i = 0u; i < jwks->keys.count; ++i) {
    const lonejson_jwk *jwk = &((const lonejson_jwk *)jwks->keys.items)[i];
    if (options != NULL &&
        (!lonejson__jwk_match_filter(jwk->kid, options->kid) ||
         !lonejson__jwk_match_filter(jwk->kty, options->kty) ||
         !lonejson__jwk_match_filter(jwk->alg, options->alg) ||
         !lonejson__jwk_match_filter(jwk->use, options->use))) {
      continue;
    }
    *out = jwk;
    return LONEJSON_STATUS_OK;
  }
  return LONEJSON_STATUS_OK;
}

#ifdef LONEJSON_WITH_OIDC
static int lonejson__oidc_find_https_authority_end(const char *url, size_t len,
                                                   int allow_query_fragment,
                                                   size_t *authority_end) {
  size_t i;
  size_t authority_start = 8u;

  if (url == NULL || len <= authority_start ||
      memcmp(url, "https://", authority_start) != 0) {
    return 0;
  }
  if (url[authority_start] == '/' || url[authority_start] == '?' ||
      url[authority_start] == '#') {
    return 0;
  }
  for (i = authority_start; i < len; ++i) {
    if (url[i] == '/' || url[i] == '?' || url[i] == '#') {
      break;
    }
  }
  if (i == authority_start) {
    return 0;
  }
  if (!allow_query_fragment) {
    size_t j;
    for (j = i; j < len; ++j) {
      if (url[j] == '?' || url[j] == '#') {
        return 0;
      }
    }
  }
  if (authority_end != NULL) {
    *authority_end = i;
  }
  return 1;
}

static lonejson_status
lonejson__oidc_require_https_url(const char *value, const char *member,
                                 int allow_query_fragment,
                                 lonejson_error *error) {
  if (value == NULL || value[0] == '\0' ||
      !lonejson__oidc_find_https_authority_end(value, strlen(value),
                                               allow_query_fragment, NULL)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               "OIDC %s must be an https URL with a host",
                               member);
  }
  return LONEJSON_STATUS_OK;
}

void lonejson_oidc_discovery_init(lonejson_oidc_discovery *discovery) {
  if (discovery != NULL) {
    memset(discovery, 0, sizeof(*discovery));
  }
}

void lonejson_oidc_discovery_cleanup(lonejson_oidc_discovery *discovery) {
  if (discovery != NULL) {
    lonejson_cleanup(&lonejson__oidc_discovery_map, discovery);
    memset(discovery, 0, sizeof(*discovery));
  }
}

lonejson_status lonejson_oidc_discovery_url(const char *issuer,
                                            lonejson_owned_buffer *out,
                                            lonejson_error *error) {
  static const char well_known[] = "/.well-known/openid-configuration";
  size_t issuer_len;
  size_t authority_end;
  size_t trim_len;
  size_t path_len;
  size_t total_len;
  lonejson_allocator allocator;
  char *data;

  lonejson__clear_error(error);
  if (issuer == NULL || out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "OIDC issuer and output are required");
  }
  issuer_len = strlen(issuer);
  if (!lonejson__oidc_find_https_authority_end(issuer, issuer_len, 0,
                                               &authority_end)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               "OIDC issuer must be an https URL with a host");
  }
  trim_len = issuer_len;
  while (trim_len > authority_end && issuer[trim_len - 1u] == '/') {
    --trim_len;
  }
  path_len = trim_len > authority_end ? trim_len - authority_end : 0u;
  if (authority_end > SIZE_MAX - (sizeof(well_known) - 1u) ||
      authority_end + (sizeof(well_known) - 1u) > SIZE_MAX - path_len - 1u) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "OIDC discovery URL is too large");
  }
  total_len = authority_end + (sizeof(well_known) - 1u) + path_len;
  allocator = lonejson_default_allocator();
  data = (char *)lonejson__buffer_alloc(&allocator, total_len + 1u);
  if (data == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to allocate OIDC discovery URL");
  }
  memcpy(data, issuer, authority_end);
  memcpy(data + authority_end, well_known, sizeof(well_known) - 1u);
  if (path_len != 0u) {
    memcpy(data + authority_end + (sizeof(well_known) - 1u),
           issuer + authority_end, path_len);
  }
  data[total_len] = '\0';
  lonejson_owned_buffer_free(out);
  out->data = data;
  out->len = total_len;
  out->alloc_size = total_len + 1u;
  out->allocator = allocator;
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_oidc_discovery_parse_json(lonejson *runtime,
                                                   const char *json, size_t len,
                                                   lonejson_oidc_discovery *out,
                                                   lonejson_error *error) {
  lonejson_status status;

  lonejson__clear_error(error);
  if (out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "OIDC discovery output is required");
  }
  if (json == NULL && len != 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "OIDC discovery JSON input is required");
  }
  status = lonejson_parse_buffer(runtime, &lonejson__oidc_discovery_map, out,
                                 json, len, error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_oidc_discovery_cleanup(out);
    return status;
  }
  status = lonejson__oidc_require_https_url(out->issuer, "issuer", 0, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oidc_require_https_url(out->token_endpoint,
                                              "token_endpoint", 1, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status =
        lonejson__oidc_require_https_url(out->jwks_uri, "jwks_uri", 1, error);
  }
  if (status == LONEJSON_STATUS_OK && out->authorization_endpoint != NULL) {
    status = lonejson__oidc_require_https_url(
        out->authorization_endpoint, "authorization_endpoint", 1, error);
  }
  if (status != LONEJSON_STATUS_OK) {
    lonejson_oidc_discovery_cleanup(out);
  }
  return status;
}

lonejson_status lonejson_oidc_discovery_validate_issuer(
    const lonejson_oidc_discovery *discovery, const char *expected_issuer,
    lonejson_error *error) {
  lonejson__clear_error(error);
  if (discovery == NULL || expected_issuer == NULL) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "OIDC discovery and expected issuer are required");
  }
  if (!lonejson__oidc_find_https_authority_end(
          expected_issuer, strlen(expected_issuer), 0, NULL)) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
        "OIDC expected issuer must be an https URL with a host");
  }
  if (discovery->issuer == NULL ||
      strcmp(discovery->issuer, expected_issuer) != 0) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
        "OIDC discovery issuer does not match expected issuer");
  }
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_oidc_fetch_discovery(lonejson *runtime,
                                              const char *issuer,
                                              size_t max_response_bytes,
                                              lonejson_oidc_discovery *out,
                                              lonejson_error *error) {
  lonejson_owned_buffer url;
  lonejson_http_request request;
  lonejson_http_response response;
  size_t max_bytes;
  lonejson_status status;

  lonejson__clear_error(error);
  if (out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "OIDC discovery output is required");
  }
  max_bytes = max_response_bytes == 0u
                  ? LONEJSON__OIDC_DEFAULT_MAX_DISCOVERY_BYTES
                  : max_response_bytes;
  lonejson_owned_buffer_init(&url);
  lonejson_http_response_init(&response);
  status = lonejson_oidc_discovery_url(issuer, &url, error);
  if (status == LONEJSON_STATUS_OK) {
    memset(&request, 0, sizeof(request));
    request.method = "GET";
    request.url = url.data;
    request.max_response_bytes = max_bytes;
    status = lonejson__http_provider_request(runtime, &request, &response,
                                             error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__http_require_success(&response, "OIDC discovery",
                                            error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_oidc_discovery_parse_json(
        runtime, response.body.data, response.body.len, out, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_oidc_discovery_validate_issuer(out, issuer, error);
    if (status != LONEJSON_STATUS_OK) {
      lonejson_oidc_discovery_cleanup(out);
    }
  }
  lonejson_http_response_cleanup(&response);
  lonejson_owned_buffer_free(&url);
  return status;
}

static lonejson_status lonejson__oidc_jwks_cache_validate_policy(
    const lonejson_oidc_jwks_cache_policy *policy, lonejson_error *error) {
  lonejson_int64 max_i64 = (lonejson_int64)(LONEJSON_UINT64_MAX >> 1);

  if (policy == NULL || policy->issuer == NULL || policy->jwks_uri == NULL) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "OIDC JWKS cache policy, issuer, and jwks_uri are required");
  }
  if (policy->ttl_seconds <= 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "OIDC JWKS cache ttl_seconds must be positive");
  }
  if (policy->now > 0 && policy->ttl_seconds > max_i64 - policy->now) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "OIDC JWKS cache expiry overflows");
  }
  if (lonejson__oidc_require_https_url(policy->issuer, "issuer", 0, error) !=
      LONEJSON_STATUS_OK) {
    return error != NULL ? error->code : LONEJSON_STATUS_INVALID_JSON;
  }
  return lonejson__oidc_require_https_url(policy->jwks_uri, "jwks_uri", 1,
                                          error);
}

static int lonejson__oidc_jwks_cache_matches_policy(
    const lonejson_oidc_jwks_cache *cache,
    const lonejson_oidc_jwks_cache_policy *policy) {
  return cache != NULL && policy != NULL && cache->has_jwks &&
         cache->issuer != NULL && cache->jwks_uri != NULL &&
         strcmp(cache->issuer, policy->issuer) == 0 &&
         strcmp(cache->jwks_uri, policy->jwks_uri) == 0 &&
         policy->now < cache->expires_at;
}

void lonejson_oidc_jwks_cache_init(lonejson_oidc_jwks_cache *cache) {
  if (cache != NULL) {
    memset(cache, 0, sizeof(*cache));
  }
}

void lonejson_oidc_jwks_cache_cleanup(lonejson_oidc_jwks_cache *cache) {
  if (cache != NULL) {
    lonejson__owned_free(cache->issuer);
    lonejson__owned_free(cache->jwks_uri);
    lonejson_jwks_cleanup(&cache->jwks);
    memset(cache, 0, sizeof(*cache));
  }
}

static int lonejson__oauth2_form_is_unreserved(unsigned char ch) {
  return (ch >= (unsigned char)'A' && ch <= (unsigned char)'Z') ||
         (ch >= (unsigned char)'a' && ch <= (unsigned char)'z') ||
         (ch >= (unsigned char)'0' && ch <= (unsigned char)'9') ||
         ch == (unsigned char)'-' || ch == (unsigned char)'.' ||
         ch == (unsigned char)'_' || ch == (unsigned char)'~';
}

static lonejson_status lonejson__oauth2_form_append_raw(
    lonejson_owned_buffer *out, const char *data, size_t len, size_t max_bytes,
    lonejson_error *error) {
  if (out->len > max_bytes || len > max_bytes - out->len) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "OAuth2 form body exceeds configured limit");
  }
  return lonejson_owned_buffer_sink(out, data, len, error);
}

static lonejson_status lonejson__oauth2_form_append_component(
    lonejson_owned_buffer *out, const char *value, size_t max_bytes,
    lonejson_error *error) {
  static const char hex[] = "0123456789ABCDEF";
  unsigned char ch;
  char encoded[3];
  lonejson_status status;

  while (*value != '\0') {
    ch = (unsigned char)*value++;
    if (lonejson__oauth2_form_is_unreserved(ch)) {
      status = lonejson__oauth2_form_append_raw(out, (const char *)&ch, 1u,
                                                max_bytes, error);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    } else if (ch == (unsigned char)' ') {
      status = lonejson__oauth2_form_append_raw(out, "+", 1u, max_bytes, error);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    } else {
      encoded[0] = '%';
      encoded[1] = hex[(ch >> 4u) & 0x0fu];
      encoded[2] = hex[ch & 0x0fu];
      status = lonejson__oauth2_form_append_raw(out, encoded, sizeof(encoded),
                                                max_bytes, error);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__oauth2_form_append_pair(
    lonejson_owned_buffer *out, const char *key, const char *value,
    int *has_pair, size_t max_bytes, lonejson_error *error) {
  lonejson_status status;

  if (value == NULL) {
    return LONEJSON_STATUS_OK;
  }
  if (*has_pair) {
    status = lonejson__oauth2_form_append_raw(out, "&", 1u, max_bytes, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  status = lonejson__oauth2_form_append_raw(out, key, strlen(key), max_bytes,
                                            error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_raw(out, "=", 1u, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status =
        lonejson__oauth2_form_append_component(out, value, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    *has_pair = 1;
  }
  return status;
}

static int lonejson__ascii_case_equal(const char *a, const char *b) {
  unsigned char ca;
  unsigned char cb;

  if (a == NULL || b == NULL) {
    return 0;
  }
  while (*a != '\0' && *b != '\0') {
    ca = (unsigned char)*a++;
    cb = (unsigned char)*b++;
    if (ca >= (unsigned char)'A' && ca <= (unsigned char)'Z') {
      ca = (unsigned char)(ca - (unsigned char)'A' + (unsigned char)'a');
    }
    if (cb >= (unsigned char)'A' && cb <= (unsigned char)'Z') {
      cb = (unsigned char)(cb - (unsigned char)'A' + (unsigned char)'a');
    }
    if (ca != cb) {
      return 0;
    }
  }
  return *a == '\0' && *b == '\0';
}

lonejson_status lonejson_oauth2_client_credentials_body(
    const lonejson_oauth2_client_credentials *request,
    lonejson_owned_buffer *out, lonejson_error *error) {
  size_t max_bytes;
  lonejson_status status;
  int has_pair = 0;

  lonejson__clear_error(error);
  if (request == NULL || out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "OAuth2 client credentials request is required");
  }
  if (request->client_id == NULL || request->client_id[0] == '\0' ||
      request->client_secret == NULL || request->client_secret[0] == '\0') {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "OAuth2 client_id and client_secret are required");
  }
  lonejson_owned_buffer_free(out);
  max_bytes = request->max_body_bytes == 0u
                  ? LONEJSON__OAUTH2_DEFAULT_MAX_FORM_BODY_BYTES
                  : request->max_body_bytes;
  status = lonejson__oauth2_form_append_pair(
      out, "grant_type", "client_credentials", &has_pair, max_bytes, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(
        out, "client_id", request->client_id, &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(out, "client_secret",
                                               request->client_secret,
                                               &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(
        out, "scope", request->scope, &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(
        out, "audience", request->audience, &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(
        out, "resource", request->resource, &has_pair, max_bytes, error);
  }
  if (status != LONEJSON_STATUS_OK) {
    lonejson_owned_buffer_free(out);
  }
  return status;
}

lonejson_status lonejson_oauth2_refresh_token_body(
    const lonejson_oauth2_refresh_token *request, lonejson_owned_buffer *out,
    lonejson_error *error) {
  size_t max_bytes;
  lonejson_status status;
  int has_pair = 0;

  lonejson__clear_error(error);
  if (request == NULL || out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "OAuth2 refresh token request is required");
  }
  if (request->refresh_token == NULL || request->refresh_token[0] == '\0') {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "OAuth2 refresh_token is required");
  }
  if (request->client_secret != NULL &&
      (request->client_id == NULL || request->client_id[0] == '\0')) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "OAuth2 client_id is required with "
                               "client_secret");
  }
  lonejson_owned_buffer_free(out);
  max_bytes = request->max_body_bytes == 0u
                  ? LONEJSON__OAUTH2_DEFAULT_MAX_FORM_BODY_BYTES
                  : request->max_body_bytes;
  status = lonejson__oauth2_form_append_pair(out, "grant_type",
                                             "refresh_token", &has_pair,
                                             max_bytes, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(
        out, "refresh_token", request->refresh_token, &has_pair, max_bytes,
        error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(
        out, "client_id", request->client_id, &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(
        out, "client_secret", request->client_secret, &has_pair, max_bytes,
        error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(
        out, "scope", request->scope, &has_pair, max_bytes, error);
  }
  if (status != LONEJSON_STATUS_OK) {
    lonejson_owned_buffer_free(out);
  }
  return status;
}

lonejson_status lonejson_oidc_authorization_code_token_body(
    const lonejson_oidc_authorization_code_token *request,
    lonejson_owned_buffer *out, lonejson_error *error) {
  size_t max_bytes;
  lonejson_status status;
  int has_pair = 0;

  lonejson__clear_error(error);
  if (request == NULL || out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "OIDC authorization code token request is "
                               "required");
  }
  if (request->client_id == NULL || request->client_id[0] == '\0' ||
      request->code == NULL || request->code[0] == '\0' ||
      request->redirect_uri == NULL || request->redirect_uri[0] == '\0' ||
      request->code_verifier == NULL || request->code_verifier[0] == '\0') {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "client_id, code, redirect_uri, and code_verifier are required");
  }
  lonejson_owned_buffer_free(out);
  max_bytes = request->max_body_bytes == 0u
                  ? LONEJSON__OAUTH2_DEFAULT_MAX_FORM_BODY_BYTES
                  : request->max_body_bytes;
  status = lonejson__oauth2_form_append_pair(
      out, "grant_type", "authorization_code", &has_pair, max_bytes, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(out, "client_id",
                                               request->client_id, &has_pair,
                                               max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(out, "code", request->code,
                                               &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(
        out, "redirect_uri", request->redirect_uri, &has_pair, max_bytes,
        error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(
        out, "code_verifier", request->code_verifier, &has_pair, max_bytes,
        error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oauth2_form_append_pair(
        out, "client_secret", request->client_secret, &has_pair, max_bytes,
        error);
  }
  if (status != LONEJSON_STATUS_OK) {
    lonejson_owned_buffer_free(out);
  }
  return status;
}

void lonejson_oauth2_token_response_init(
    lonejson_oauth2_token_response *response) {
  if (response != NULL) {
    memset(response, 0, sizeof(*response));
  }
}

void lonejson_oauth2_token_response_cleanup(
    lonejson_oauth2_token_response *response) {
  if (response != NULL) {
    lonejson_cleanup(&lonejson__oauth2_token_response_map, response);
    memset(response, 0, sizeof(*response));
  }
}

static size_t lonejson__base64url_encoded_len(size_t len) {
  size_t full = len / 3u;
  size_t rem = len % 3u;
  return full * 4u + (rem == 0u ? 0u : rem + 1u);
}

static char *lonejson__base64url_encode_alloc(const unsigned char *data,
                                              size_t len,
                                              lonejson_error *error) {
  static const char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  size_t out_len;
  size_t i = 0u;
  size_t o = 0u;
  char *out;
  unsigned int n;

  if (len > (SIZE_MAX / 4u) * 3u) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                              "base64url output size overflows");
    return NULL;
  }
  out_len = lonejson__base64url_encoded_len(len);
  out = (char *)lonejson__owned_malloc(NULL, out_len + 1u);
  if (out == NULL) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                              0u, "failed to allocate base64url output");
    return NULL;
  }
  while (i + 3u <= len) {
    n = ((unsigned int)data[i] << 16u) | ((unsigned int)data[i + 1u] << 8u) |
        (unsigned int)data[i + 2u];
    out[o++] = alphabet[(n >> 18u) & 63u];
    out[o++] = alphabet[(n >> 12u) & 63u];
    out[o++] = alphabet[(n >> 6u) & 63u];
    out[o++] = alphabet[n & 63u];
    i += 3u;
  }
  if (i < len) {
    n = (unsigned int)data[i] << 16u;
    out[o++] = alphabet[(n >> 18u) & 63u];
    if (i + 1u < len) {
      n |= (unsigned int)data[i + 1u] << 8u;
      out[o++] = alphabet[(n >> 12u) & 63u];
      out[o++] = alphabet[(n >> 6u) & 63u];
    } else {
      out[o++] = alphabet[(n >> 12u) & 63u];
    }
  }
  out[o] = '\0';
  return out;
}

static int lonejson__oidc_pkce_verifier_char(unsigned char ch) {
  return lonejson__oauth2_form_is_unreserved(ch);
}

static lonejson_status
lonejson__oidc_pkce_validate_verifier(const char *code_verifier,
                                      lonejson_error *error) {
  size_t len;
  const unsigned char *p;

  if (code_verifier == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "PKCE code_verifier is required");
  }
  len = strlen(code_verifier);
  if (len < LONEJSON__OIDC_PKCE_MIN_VERIFIER_CHARS ||
      len > LONEJSON__OIDC_PKCE_MAX_VERIFIER_CHARS) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "PKCE code_verifier length is invalid");
  }
  for (p = (const unsigned char *)code_verifier; *p != '\0'; ++p) {
    if (!lonejson__oidc_pkce_verifier_char(*p)) {
      return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u,
                                 0u, 0u,
                                 "PKCE code_verifier contains invalid chars");
    }
  }
  return LONEJSON_STATUS_OK;
}

void lonejson_oidc_pkce_init(lonejson_oidc_pkce *pkce) {
  if (pkce != NULL) {
    memset(pkce, 0, sizeof(*pkce));
  }
}

void lonejson_oidc_pkce_cleanup(lonejson_oidc_pkce *pkce) {
  if (pkce != NULL) {
    lonejson__owned_free(pkce->code_verifier);
    lonejson__owned_free(pkce->code_challenge);
    memset(pkce, 0, sizeof(*pkce));
  }
}

lonejson_status lonejson_oidc_pkce_challenge(const char *code_verifier,
                                             lonejson_owned_buffer *out,
                                             lonejson_error *error) {
#ifdef LONEJSON_WITH_OPENSSL
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_len = 0u;
#else
  unsigned char digest[32];
#endif
  char *encoded;
  lonejson_status status;

  lonejson__clear_error(error);
  if (out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "PKCE challenge output is required");
  }
  status = lonejson__oidc_pkce_validate_verifier(code_verifier, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  lonejson_owned_buffer_free(out);
#ifdef LONEJSON_WITH_OPENSSL
  if (EVP_Digest(code_verifier, strlen(code_verifier), digest, &digest_len,
                 EVP_sha256(), NULL) != 1) {
    return lonejson__set_error(error, LONEJSON_STATUS_INTERNAL_ERROR, 0u, 0u,
                               0u, "failed to compute PKCE challenge");
  }
  encoded = lonejson__base64url_encode_alloc(digest, (size_t)digest_len, error);
#else
  (void)digest;
  return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                             "PKCE challenge requires an auth crypto provider");
#endif
  if (encoded == NULL) {
    return error != NULL ? error->code : LONEJSON_STATUS_ALLOCATION_FAILED;
  }
  status = lonejson_owned_buffer_sink(out, encoded, strlen(encoded), error);
  lonejson__owned_free(encoded);
  return status;
}

lonejson_status lonejson_oidc_pkce_generate(size_t verifier_bytes,
                                            lonejson_oidc_pkce *out,
                                            lonejson_error *error) {
  unsigned char random_bytes[96];
  lonejson_owned_buffer challenge;
  size_t bytes;
  char *verifier;
  char *challenge_copy;
  lonejson_status status;

  lonejson__clear_error(error);
  if (out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "PKCE output is required");
  }
  bytes = verifier_bytes == 0u ? LONEJSON__OIDC_PKCE_DEFAULT_VERIFIER_BYTES
                               : verifier_bytes;
  if (bytes < 32u || bytes > sizeof(random_bytes)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "PKCE verifier entropy bytes must be 32..96");
  }
#ifdef LONEJSON_WITH_OPENSSL
  if (RAND_bytes(random_bytes, (int)bytes) != 1) {
    return lonejson__set_error(error, LONEJSON_STATUS_INTERNAL_ERROR, 0u, 0u,
                               0u, "failed to generate PKCE verifier");
  }
#else
  return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                             "PKCE verifier generation requires an auth "
                             "crypto provider");
#endif
  verifier = lonejson__base64url_encode_alloc(random_bytes, bytes, error);
  if (verifier == NULL) {
    return error != NULL ? error->code : LONEJSON_STATUS_ALLOCATION_FAILED;
  }
  lonejson_owned_buffer_init(&challenge);
  status = lonejson_oidc_pkce_challenge(verifier, &challenge, error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson__owned_free(verifier);
    return status;
  }
  challenge_copy = lonejson__owned_strdup(NULL, challenge.data);
  lonejson_owned_buffer_free(&challenge);
  if (challenge_copy == NULL) {
    lonejson__owned_free(verifier);
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to allocate PKCE challenge");
  }
  lonejson_oidc_pkce_cleanup(out);
  out->code_verifier = verifier;
  out->code_challenge = challenge_copy;
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_oauth2_token_response_parse_json(
    lonejson *runtime, const char *json, size_t len, size_t max_response_bytes,
    lonejson_oauth2_token_response *out, lonejson_error *error) {
  lonejson_status status;
  size_t max_bytes;

  lonejson__clear_error(error);
  if (out == NULL || (json == NULL && len != 0u)) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "OAuth2 token response JSON and output are required");
  }
  max_bytes = max_response_bytes == 0u
                  ? LONEJSON__OAUTH2_DEFAULT_MAX_TOKEN_RESPONSE_BYTES
                  : max_response_bytes;
  if (len > max_bytes) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_OVERFLOW, len, 0u, 0u,
        "OAuth2 token response exceeds configured limit");
  }
  lonejson_oauth2_token_response_cleanup(out);
  status = lonejson_parse_buffer(runtime, &lonejson__oauth2_token_response_map,
                                 out, json, len, error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_oauth2_token_response_cleanup(out);
    return status;
  }
  if (out->error != NULL) {
    lonejson_oauth2_token_response_cleanup(out);
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "OAuth2 token endpoint returned an error");
  }
  if (out->access_token == NULL || out->access_token[0] == '\0' ||
      out->token_type == NULL || out->token_type[0] == '\0') {
    lonejson_oauth2_token_response_cleanup(out);
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
        "OAuth2 token response requires access_token and token_type");
  }
  if (!lonejson__ascii_case_equal(out->token_type, "Bearer")) {
    lonejson_oauth2_token_response_cleanup(out);
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "OAuth2 token response token_type is not Bearer");
  }
  if (out->has_expires_in && out->expires_in < 0) {
    lonejson_oauth2_token_response_cleanup(out);
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               "OAuth2 token response expires_in is negative");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__oauth2_token_endpoint_post(
    lonejson *runtime, const char *token_endpoint, const void *request_options,
    lonejson_status (*body_fn)(const void *, lonejson_owned_buffer *,
                               lonejson_error *),
    size_t max_response_bytes, lonejson_oauth2_token_response *out,
    lonejson_error *error) {
  lonejson_owned_buffer body;
  lonejson_http_request request;
  lonejson_http_response response;
  size_t max_bytes;
  lonejson_status status;

  lonejson__clear_error(error);
  if (out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "OAuth2 token response output is required");
  }
  if (body_fn == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "OAuth2 token request body builder is "
                                   "required");
  }
  status = lonejson__oidc_require_https_url(token_endpoint, "token_endpoint", 1,
                                            error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  max_bytes = max_response_bytes == 0u
                  ? LONEJSON__OAUTH2_DEFAULT_MAX_TOKEN_RESPONSE_BYTES
                  : max_response_bytes;
  lonejson_owned_buffer_init(&body);
  lonejson_http_response_init(&response);
  status = body_fn(request_options, &body, error);
  if (status == LONEJSON_STATUS_OK) {
    memset(&request, 0, sizeof(request));
    request.method = "POST";
    request.url = token_endpoint;
    request.content_type = "application/x-www-form-urlencoded";
    request.body = body.data;
    request.body_len = body.len;
    request.max_response_bytes = max_bytes;
    status = lonejson__http_provider_request(runtime, &request, &response,
                                             error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__http_require_success(&response, "OAuth2 token endpoint",
                                            error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_oauth2_token_response_parse_json(
        runtime, response.body.data, response.body.len, max_bytes, out, error);
  }
  lonejson_http_response_cleanup(&response);
  lonejson_owned_buffer_free(&body);
  return status;
}

static lonejson_status lonejson__oauth2_client_credentials_body_adapter(
    const void *request, lonejson_owned_buffer *out, lonejson_error *error) {
  return lonejson_oauth2_client_credentials_body(
      (const lonejson_oauth2_client_credentials *)request, out, error);
}

static lonejson_status lonejson__oauth2_refresh_token_body_adapter(
    const void *request, lonejson_owned_buffer *out, lonejson_error *error) {
  return lonejson_oauth2_refresh_token_body(
      (const lonejson_oauth2_refresh_token *)request, out, error);
}

static lonejson_status lonejson__oidc_authorization_code_token_body_adapter(
    const void *request, lonejson_owned_buffer *out, lonejson_error *error) {
  return lonejson_oidc_authorization_code_token_body(
      (const lonejson_oidc_authorization_code_token *)request, out, error);
}

lonejson_status lonejson_oauth2_client_credentials_request(
    lonejson *runtime, const char *token_endpoint,
    const lonejson_oauth2_client_credentials *request_options,
    size_t max_response_bytes, lonejson_oauth2_token_response *out,
    lonejson_error *error) {
  return lonejson__oauth2_token_endpoint_post(
      runtime, token_endpoint, request_options,
      lonejson__oauth2_client_credentials_body_adapter, max_response_bytes, out,
      error);
}

lonejson_status lonejson_oauth2_refresh_token_request(
    lonejson *runtime, const char *token_endpoint,
    const lonejson_oauth2_refresh_token *request_options,
    size_t max_response_bytes, lonejson_oauth2_token_response *out,
    lonejson_error *error) {
  return lonejson__oauth2_token_endpoint_post(
      runtime, token_endpoint, request_options,
      lonejson__oauth2_refresh_token_body_adapter, max_response_bytes, out,
      error);
}

lonejson_status lonejson_oidc_authorization_code_token_request(
    lonejson *runtime, const char *token_endpoint,
    const lonejson_oidc_authorization_code_token *request_options,
    size_t max_response_bytes, lonejson_oauth2_token_response *out,
    lonejson_error *error) {
  return lonejson__oauth2_token_endpoint_post(
      runtime, token_endpoint, request_options,
      lonejson__oidc_authorization_code_token_body_adapter, max_response_bytes,
      out, error);
}

static lonejson_status lonejson__oidc_authorization_append_pair(
    lonejson_owned_buffer *out, const char *key, const char *value,
    int *has_pair, size_t max_bytes, lonejson_error *error) {
  return lonejson__oauth2_form_append_pair(out, key, value, has_pair, max_bytes,
                                           error);
}

lonejson_status lonejson_oidc_authorization_url(
    const lonejson_oidc_authorization_request *request,
    lonejson_owned_buffer *out, lonejson_error *error) {
  size_t max_bytes;
  lonejson_status status;
  int has_query;
  int has_pair = 0;

  lonejson__clear_error(error);
  if (request == NULL || out == NULL || request->authorization_endpoint == NULL ||
      request->client_id == NULL || request->redirect_uri == NULL ||
      request->state == NULL || request->nonce == NULL ||
      request->code_challenge == NULL) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "authorization endpoint, client_id, redirect_uri, state, nonce, and "
        "code_challenge are required");
  }
  status = lonejson__oidc_require_https_url(
      request->authorization_endpoint, "authorization_endpoint", 1, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  max_bytes = request->max_url_bytes == 0u
                  ? LONEJSON__OIDC_DEFAULT_MAX_AUTH_URL_BYTES
                  : request->max_url_bytes;
  lonejson_owned_buffer_free(out);
  status = lonejson__oauth2_form_append_raw(
      out, request->authorization_endpoint, strlen(request->authorization_endpoint),
      max_bytes, error);
  has_query = strchr(request->authorization_endpoint, '?') != NULL;
  if (status == LONEJSON_STATUS_OK) {
    status =
        lonejson__oauth2_form_append_raw(out, has_query ? "&" : "?", 1u,
                                         max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oidc_authorization_append_pair(
        out, "response_type", "code", &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oidc_authorization_append_pair(
        out, "client_id", request->client_id, &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oidc_authorization_append_pair(
        out, "redirect_uri", request->redirect_uri, &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK && request->scope != NULL) {
    status = lonejson__oidc_authorization_append_pair(
        out, "scope", request->scope, &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oidc_authorization_append_pair(
        out, "state", request->state, &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oidc_authorization_append_pair(
        out, "nonce", request->nonce, &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oidc_authorization_append_pair(
        out, "code_challenge", request->code_challenge, &has_pair, max_bytes,
        error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__oidc_authorization_append_pair(
        out, "code_challenge_method", "S256", &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK && request->audience != NULL) {
    status = lonejson__oidc_authorization_append_pair(
        out, "audience", request->audience, &has_pair, max_bytes, error);
  }
  if (status == LONEJSON_STATUS_OK && request->resource != NULL) {
    status = lonejson__oidc_authorization_append_pair(
        out, "resource", request->resource, &has_pair, max_bytes, error);
  }
  if (status != LONEJSON_STATUS_OK) {
    lonejson_owned_buffer_free(out);
  }
  return status;
}

void lonejson_oidc_authorization_callback_init(
    lonejson_oidc_authorization_callback *callback) {
  if (callback != NULL) {
    memset(callback, 0, sizeof(*callback));
  }
}

void lonejson_oidc_authorization_callback_cleanup(
    lonejson_oidc_authorization_callback *callback) {
  if (callback != NULL) {
    lonejson__owned_free(callback->code);
    lonejson__owned_free(callback->state);
    lonejson__owned_free(callback->error);
    lonejson__owned_free(callback->error_description);
    lonejson__owned_free(callback->error_uri);
    memset(callback, 0, sizeof(*callback));
  }
}

static int lonejson__oauth2_hex_value(char ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if (ch >= 'A' && ch <= 'F') {
    return ch - 'A' + 10;
  }
  if (ch >= 'a' && ch <= 'f') {
    return ch - 'a' + 10;
  }
  return -1;
}

static lonejson_status lonejson__oauth2_percent_decode(
    const char *data, size_t len, lonejson_owned_buffer *out,
    lonejson_error *error) {
  size_t i;
  int hi;
  int lo;
  char ch;

  lonejson_owned_buffer_free(out);
  for (i = 0u; i < len; ++i) {
    ch = data[i];
    if (ch == '+') {
      ch = ' ';
    } else if (ch == '%') {
      if (i + 2u >= len) {
        return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, i, 0u,
                                   0u, "callback query has bad percent escape");
      }
      hi = lonejson__oauth2_hex_value(data[i + 1u]);
      lo = lonejson__oauth2_hex_value(data[i + 2u]);
      if (hi < 0 || lo < 0) {
        return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, i, 0u,
                                   0u, "callback query has bad percent escape");
      }
      ch = (char)((hi << 4) | lo);
      i += 2u;
    }
    if (lonejson_owned_buffer_sink(out, &ch, 1u, error) !=
        LONEJSON_STATUS_OK) {
      return error != NULL ? error->code : LONEJSON_STATUS_ALLOCATION_FAILED;
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__oidc_callback_assign(
    char **dst, const char *value, lonejson_error *error) {
  if (*dst != NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_DUPLICATE_FIELD, 0u, 0u,
                               0u, "duplicate callback query parameter");
  }
  *dst = lonejson__owned_strdup(NULL, value);
  if (*dst == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to allocate callback parameter");
  }
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_oidc_authorization_callback_parse_query(
    const char *query, size_t len, const char *expected_state,
    size_t max_query_bytes, lonejson_oidc_authorization_callback *out,
    lonejson_error *error) {
  size_t max_bytes;
  size_t pos = 0u;
  size_t key_begin;
  size_t key_len;
  size_t value_begin;
  size_t value_len;
  lonejson_owned_buffer key;
  lonejson_owned_buffer value;
  lonejson_status status = LONEJSON_STATUS_OK;

  lonejson__clear_error(error);
  if (out == NULL || expected_state == NULL || expected_state[0] == '\0' ||
      (query == NULL && len != 0u)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "callback query, expected_state, and output are "
                               "required");
  }
  max_bytes = max_query_bytes == 0u
                  ? LONEJSON__OIDC_DEFAULT_MAX_CALLBACK_QUERY_BYTES
                  : max_query_bytes;
  if (len > max_bytes) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, len, 0u, 0u,
                               "callback query exceeds configured limit");
  }
  lonejson_oidc_authorization_callback_cleanup(out);
  lonejson_owned_buffer_init(&key);
  lonejson_owned_buffer_init(&value);
  if (len != 0u && query[0] == '?') {
    pos = 1u;
  }
  while (status == LONEJSON_STATUS_OK && pos <= len) {
    key_begin = pos;
    while (pos < len && query[pos] != '=' && query[pos] != '&') {
      ++pos;
    }
    key_len = pos - key_begin;
    value_begin = pos;
    value_len = 0u;
    if (pos < len && query[pos] == '=') {
      ++pos;
      value_begin = pos;
      while (pos < len && query[pos] != '&') {
        ++pos;
      }
      value_len = pos - value_begin;
    }
    if (pos < len && query[pos] == '&') {
      ++pos;
    } else if (pos == len) {
      ++pos;
    }
    if (key_len == 0u) {
      continue;
    }
    status =
        lonejson__oauth2_percent_decode(query + key_begin, key_len, &key, error);
    if (status == LONEJSON_STATUS_OK) {
      status = lonejson__oauth2_percent_decode(query + value_begin, value_len,
                                               &value, error);
    }
    if (status != LONEJSON_STATUS_OK) {
      break;
    }
    if (strcmp(key.data, "code") == 0) {
      status = lonejson__oidc_callback_assign(
          &out->code, value.data != NULL ? value.data : "", error);
    } else if (strcmp(key.data, "state") == 0) {
      status = lonejson__oidc_callback_assign(
          &out->state, value.data != NULL ? value.data : "", error);
    } else if (strcmp(key.data, "error") == 0) {
      status = lonejson__oidc_callback_assign(
          &out->error, value.data != NULL ? value.data : "", error);
    } else if (strcmp(key.data, "error_description") == 0) {
      status = lonejson__oidc_callback_assign(&out->error_description,
                                              value.data != NULL ? value.data
                                                                 : "",
                                              error);
    } else if (strcmp(key.data, "error_uri") == 0) {
      status = lonejson__oidc_callback_assign(
          &out->error_uri, value.data != NULL ? value.data : "", error);
    }
  }
  lonejson_owned_buffer_free(&key);
  lonejson_owned_buffer_free(&value);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_oidc_authorization_callback_cleanup(out);
    return status;
  }
  if (out->error != NULL) {
    lonejson_oidc_authorization_callback_cleanup(out);
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "authorization callback returned an error");
  }
  if (out->state == NULL || strcmp(out->state, expected_state) != 0) {
    lonejson_oidc_authorization_callback_cleanup(out);
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "authorization callback state mismatch");
  }
  if (out->code == NULL || out->code[0] == '\0') {
    lonejson_oidc_authorization_callback_cleanup(out);
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               "authorization callback code is required");
  }
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_oidc_jwks_cache_update_json(
    lonejson *runtime, lonejson_oidc_jwks_cache *cache,
    const lonejson_oidc_jwks_cache_policy *policy, const char *json, size_t len,
    lonejson_error *error) {
  lonejson_jwks next_jwks;
  char *issuer = NULL;
  char *jwks_uri = NULL;
  size_t max_bytes;
  lonejson_status status;

  lonejson__clear_error(error);
  if (cache == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "OIDC JWKS cache is required");
  }
  if (json == NULL && len != 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "OIDC JWKS JSON input is required");
  }
  status = lonejson__oidc_jwks_cache_validate_policy(policy, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  max_bytes = policy->max_jwks_bytes == 0u
                  ? LONEJSON__OIDC_DEFAULT_MAX_JWKS_BYTES
                  : policy->max_jwks_bytes;
  if (len > max_bytes) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, len, 0u, 0u,
                               "OIDC JWKS response exceeds configured limit");
  }
  lonejson_jwks_init(&next_jwks);
  status = lonejson_jwks_parse_json(runtime, json, len, &next_jwks, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  issuer = lonejson__owned_strdup(NULL, policy->issuer);
  jwks_uri = lonejson__owned_strdup(NULL, policy->jwks_uri);
  if (issuer == NULL || jwks_uri == NULL) {
    lonejson__owned_free(issuer);
    lonejson__owned_free(jwks_uri);
    lonejson_jwks_cleanup(&next_jwks);
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to allocate OIDC JWKS metadata");
  }
  lonejson_oidc_jwks_cache_cleanup(cache);
  cache->issuer = issuer;
  cache->jwks_uri = jwks_uri;
  cache->fetched_at = policy->now;
  cache->expires_at = policy->now + policy->ttl_seconds;
  cache->max_jwks_bytes = max_bytes;
  cache->has_jwks = 1;
  cache->jwks = next_jwks;
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_oidc_jwks_cache_refresh(
    lonejson *runtime, lonejson_oidc_jwks_cache *cache,
    const lonejson_oidc_jwks_cache_policy *policy, lonejson_error *error) {
  lonejson_http_request request;
  lonejson_http_response response;
  size_t max_bytes;
  lonejson_status status;

  lonejson__clear_error(error);
  status = lonejson__oidc_jwks_cache_validate_policy(policy, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  max_bytes = policy->max_jwks_bytes == 0u
                  ? LONEJSON__OIDC_DEFAULT_MAX_JWKS_BYTES
                  : policy->max_jwks_bytes;
  lonejson_http_response_init(&response);
  memset(&request, 0, sizeof(request));
  request.method = "GET";
  request.url = policy->jwks_uri;
  request.max_response_bytes = max_bytes;
  status = lonejson__http_provider_request(runtime, &request, &response, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__http_require_success(&response, "OIDC JWKS", error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_oidc_jwks_cache_update_json(
        runtime, cache, policy, response.body.data, response.body.len, error);
  }
  lonejson_http_response_cleanup(&response);
  return status;
}

int lonejson_oidc_jwks_cache_is_fresh(
    const lonejson_oidc_jwks_cache *cache,
    const lonejson_oidc_jwks_cache_policy *policy) {
  if (lonejson__oidc_jwks_cache_validate_policy(policy, NULL) !=
      LONEJSON_STATUS_OK) {
    return 0;
  }
  return lonejson__oidc_jwks_cache_matches_policy(cache, policy);
}

lonejson_status lonejson_oidc_jwks_cache_select(
    const lonejson_oidc_jwks_cache *cache,
    const lonejson_oidc_jwks_cache_policy *policy,
    const lonejson_jwk_select_options *options, const lonejson_jwk **out,
    lonejson_error *error) {
  lonejson_status status;

  lonejson__clear_error(error);
  if (out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "OIDC JWKS selected key output is required");
  }
  *out = NULL;
  status = lonejson__oidc_jwks_cache_validate_policy(policy, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (!lonejson__oidc_jwks_cache_matches_policy(cache, policy)) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "OIDC JWKS cache is missing or stale");
  }
  return lonejson_jwks_select(&cache->jwks, options, out, error);
}

#ifdef LONEJSON_WITH_CURL
lonejson_status lonejson_oidc_jwks_cache_parse_init(
    lonejson_oidc_jwks_cache_parse *ctx, lonejson *runtime,
    lonejson_oidc_jwks_cache *cache,
    const lonejson_oidc_jwks_cache_policy *policy) {
  lonejson_status status;
  char *issuer;
  char *jwks_uri;

  if (ctx == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (lonejson__curl_state_is_live(ctx->_reserved_state)) {
    lonejson_oidc_jwks_cache_parse_cleanup(ctx);
  }
  memset(ctx, 0, sizeof(*ctx));
  lonejson__oidc_jwks_cache_assign_methods(ctx);
  lonejson__curl_state_mark_live(ctx->_reserved_state);
  lonejson_owned_buffer_init(&ctx->response);
  if (runtime == NULL || cache == NULL || policy == NULL) {
    return lonejson__set_error(
        &ctx->error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "runtime, JWKS cache, and cache policy are required");
  }
  status = lonejson__oidc_jwks_cache_validate_policy(policy, &ctx->error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  issuer = lonejson__owned_strdup(NULL, policy->issuer);
  jwks_uri = lonejson__owned_strdup(NULL, policy->jwks_uri);
  if (issuer == NULL || jwks_uri == NULL) {
    lonejson__owned_free(issuer);
    lonejson__owned_free(jwks_uri);
    return lonejson__set_error(&ctx->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                               0u, 0u, 0u,
                               "failed to allocate OIDC JWKS cache policy");
  }
  ctx->runtime = runtime;
  ctx->cache = cache;
  ctx->policy = *policy;
  ctx->policy.issuer = issuer;
  ctx->policy.jwks_uri = jwks_uri;
  return LONEJSON_STATUS_OK;
}

size_t lonejson_oidc_jwks_cache_write_callback(char *ptr, size_t size,
                                               size_t nmemb, void *userdata) {
  lonejson_oidc_jwks_cache_parse *ctx =
      (lonejson_oidc_jwks_cache_parse *)userdata;
  size_t bytes;
  size_t max_bytes;

  if (ctx == NULL || !lonejson__curl_state_is_live(ctx->_reserved_state)) {
    return 0u;
  }
  if (size != 0u && nmemb > SIZE_MAX / size) {
    (void)lonejson__set_error(&ctx->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                              "OIDC JWKS response chunk size overflows");
    return 0u;
  }
  bytes = size * nmemb;
  if (bytes == 0u) {
    return 0u;
  }
  if (ptr == NULL) {
    (void)lonejson__set_error(&ctx->error, LONEJSON_STATUS_INVALID_ARGUMENT,
                              0u, 0u, 0u,
                              "OIDC JWKS response chunk is required");
    return 0u;
  }
  max_bytes = ctx->policy.max_jwks_bytes == 0u
                  ? LONEJSON__OIDC_DEFAULT_MAX_JWKS_BYTES
                  : ctx->policy.max_jwks_bytes;
  if (ctx->response.len > max_bytes ||
      bytes > max_bytes - ctx->response.len) {
    (void)lonejson__set_error(&ctx->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                              "OIDC JWKS response exceeds configured limit");
    return 0u;
  }
  return lonejson_owned_buffer_sink(&ctx->response, ptr, bytes, &ctx->error) ==
                 LONEJSON_STATUS_OK
             ? bytes
             : 0u;
}

lonejson_status lonejson_oidc_jwks_cache_parse_finish(
    lonejson_oidc_jwks_cache_parse *ctx) {
  if (ctx == NULL || !lonejson__curl_state_is_live(ctx->_reserved_state)) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (ctx->error.code != LONEJSON_STATUS_OK) {
    return ctx->error.code;
  }
  return lonejson_oidc_jwks_cache_update_json(
      ctx->runtime, ctx->cache, &ctx->policy, ctx->response.data,
      ctx->response.len, &ctx->error);
}

void lonejson_oidc_jwks_cache_parse_cleanup(
    lonejson_oidc_jwks_cache_parse *ctx) {
  if (ctx == NULL) {
    return;
  }
  if (!lonejson__curl_state_is_live(ctx->_reserved_state)) {
    return;
  }
  lonejson__owned_free((void *)ctx->policy.issuer);
  lonejson__owned_free((void *)ctx->policy.jwks_uri);
  lonejson_owned_buffer_free(&ctx->response);
  memset(ctx, 0, sizeof(*ctx));
  lonejson__oidc_jwks_cache_assign_methods(ctx);
}
#endif
#endif

static int lonejson__jwt_path_is(const lonejson_value_path *path,
                                 const char *name) {
  return path != NULL && path->segment_count == 1u &&
         strlen(name) == path->segments[0].len &&
         memcmp(path->segments[0].data, name, path->segments[0].len) == 0;
}

static int lonejson__jwt_path_is_aud_item(const lonejson_value_path *path) {
  size_t i;
  if (path == NULL || path->segment_count != 2u ||
      path->segments[0].len != 3u ||
      memcmp(path->segments[0].data, "aud", 3u) != 0) {
    return 0;
  }
  if (path->segments[1].len == 0u) {
    return 0;
  }
  for (i = 0u; i < path->segments[1].len; ++i) {
    unsigned char ch = (unsigned char)path->segments[1].data[i];
    if (ch < (unsigned char)'0' || ch > (unsigned char)'9') {
      return 0;
    }
  }
  return 1;
}

static lonejson_status lonejson__jwt_claim_type_error(lonejson_error *error,
                                                      const char *claim) {
  return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                             "JWT claim has invalid type: %s", claim);
}

static lonejson_status
lonejson__jwt_claim_duplicate_error(lonejson_error *error, const char *claim) {
  return lonejson__set_error(error, LONEJSON_STATUS_DUPLICATE_FIELD, 0u, 0u, 0u,
                             "duplicate JWT registered claim: %s", claim);
}

static lonejson_status
lonejson__jwt_visit_buffer_append(lonejson__jwt_claim_visit *state,
                                  const char *data, size_t len,
                                  lonejson_error *error) {
  char *next;
  size_t next_cap;

  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  if (state->len > SIZE_MAX - len - 1u) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "JWT claim value is too large");
  }
  if (state->len + len + 1u > state->capacity) {
    next_cap = state->capacity == 0u ? 32u : state->capacity;
    while (next_cap < state->len + len + 1u) {
      if (next_cap > SIZE_MAX / 2u) {
        next_cap = state->len + len + 1u;
        break;
      }
      next_cap *= 2u;
    }
    next = (char *)lonejson__owned_realloc(NULL, state->buffer, next_cap);
    if (next == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u, "failed to allocate JWT claim value");
    }
    state->buffer = next;
    state->capacity = next_cap;
  }
  memcpy(state->buffer + state->len, data, len);
  state->len += len;
  state->buffer[state->len] = '\0';
  return LONEJSON_STATUS_OK;
}

static void lonejson__jwt_visit_buffer_reset(lonejson__jwt_claim_visit *state) {
  state->len = 0u;
  if (state->buffer != NULL) {
    state->buffer[0] = '\0';
  }
}

static char *lonejson__jwt_visit_buffer_take(lonejson__jwt_claim_visit *state) {
  char *out = state->buffer;
  if (out == NULL) {
    out = (char *)lonejson__owned_malloc(NULL, 1u);
    if (out == NULL) {
      return NULL;
    }
    out[0] = '\0';
  }
  state->buffer = NULL;
  state->len = 0u;
  state->capacity = 0u;
  return out;
}

static lonejson_status
lonejson__jwt_string_array_append(lonejson_string_array *array, char *value,
                                  lonejson_error *error) {
  char **next;
  size_t next_capacity;

  if (array->count == array->capacity) {
    next_capacity = array->capacity == 0u ? 4u : array->capacity * 2u;
    if (next_capacity < array->capacity) {
      return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                                 "JWT audience array is too large");
    }
    next = (char **)lonejson__owned_realloc(
        NULL, array->items, next_capacity * sizeof(array->items[0]));
    if (next == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u,
                                 "failed to allocate JWT audience array");
    }
    array->items = next;
    array->capacity = next_capacity;
    array->flags |= LONEJSON_ARRAY_OWNS_ITEMS;
  }
  array->items[array->count++] = value;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__jwt_begin_string_field(lonejson__jwt_claim_visit *state,
                                 lonejson__jwt_claim_field field, int *seen,
                                 const char *name, lonejson_error *error) {
  if (*seen) {
    return lonejson__jwt_claim_duplicate_error(error, name);
  }
  *seen = 1;
  state->active = field;
  lonejson__jwt_visit_buffer_reset(state);
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__jwt_claim_string_begin(void *user, const lonejson_value_path *path,
                                 lonejson_error *error) {
  lonejson__jwt_claim_visit *state = (lonejson__jwt_claim_visit *)user;

  if (state->header_mode) {
    if (lonejson__jwt_path_is(path, "alg")) {
      return lonejson__jwt_begin_string_field(state,
                                              LONEJSON__JWT_FIELD_HEADER_ALG,
                                              &state->seen_alg, "alg", error);
    }
    if (lonejson__jwt_path_is(path, "kid")) {
      return lonejson__jwt_begin_string_field(state,
                                              LONEJSON__JWT_FIELD_HEADER_KID,
                                              &state->seen_kid, "kid", error);
    }
    if (lonejson__jwt_path_is(path, "typ")) {
      return lonejson__jwt_begin_string_field(state,
                                              LONEJSON__JWT_FIELD_HEADER_TYP,
                                              &state->seen_typ, "typ", error);
    }
    return LONEJSON_STATUS_OK;
  }
  if (lonejson__jwt_path_is(path, "iss")) {
    return lonejson__jwt_begin_string_field(state, LONEJSON__JWT_FIELD_ISS,
                                            &state->seen_iss, "iss", error);
  }
  if (lonejson__jwt_path_is(path, "sub")) {
    return lonejson__jwt_begin_string_field(state, LONEJSON__JWT_FIELD_SUB,
                                            &state->seen_sub, "sub", error);
  }
  if (lonejson__jwt_path_is(path, "aud")) {
    return lonejson__jwt_begin_string_field(
        state, LONEJSON__JWT_FIELD_AUD_STRING, &state->seen_aud, "aud", error);
  }
  if (lonejson__jwt_path_is_aud_item(path)) {
    state->active = LONEJSON__JWT_FIELD_AUD_ARRAY_ITEM;
    lonejson__jwt_visit_buffer_reset(state);
    return LONEJSON_STATUS_OK;
  }
  if (lonejson__jwt_path_is(path, "exp") ||
      lonejson__jwt_path_is(path, "nbf") ||
      lonejson__jwt_path_is(path, "iat")) {
    return lonejson__jwt_claim_type_error(error, "registered");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__jwt_claim_string_chunk(void *user, const lonejson_value_path *path,
                                 const char *data, size_t len,
                                 lonejson_error *error) {
  lonejson__jwt_claim_visit *state = (lonejson__jwt_claim_visit *)user;
  (void)path;
  if (state->active == LONEJSON__JWT_FIELD_NONE) {
    return LONEJSON_STATUS_OK;
  }
  return lonejson__jwt_visit_buffer_append(state, data, len, error);
}

static lonejson_status
lonejson__jwt_claim_string_end(void *user, const lonejson_value_path *path,
                               lonejson_error *error) {
  lonejson__jwt_claim_visit *state = (lonejson__jwt_claim_visit *)user;
  char *value;
  (void)path;

  switch (state->active) {
  case LONEJSON__JWT_FIELD_HEADER_ALG:
    state->header->alg = lonejson__jwt_visit_buffer_take(state);
    if (state->header->alg == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u, "failed to allocate JWT claim value");
    }
    break;
  case LONEJSON__JWT_FIELD_HEADER_KID:
    state->header->kid = lonejson__jwt_visit_buffer_take(state);
    if (state->header->kid == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u, "failed to allocate JWT claim value");
    }
    break;
  case LONEJSON__JWT_FIELD_HEADER_TYP:
    state->header->typ = lonejson__jwt_visit_buffer_take(state);
    if (state->header->typ == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u, "failed to allocate JWT claim value");
    }
    break;
  case LONEJSON__JWT_FIELD_ISS:
    state->claims->iss = lonejson__jwt_visit_buffer_take(state);
    if (state->claims->iss == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u, "failed to allocate JWT claim value");
    }
    break;
  case LONEJSON__JWT_FIELD_SUB:
    state->claims->sub = lonejson__jwt_visit_buffer_take(state);
    if (state->claims->sub == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u, "failed to allocate JWT claim value");
    }
    break;
  case LONEJSON__JWT_FIELD_AUD_STRING:
    state->claims->aud = lonejson__jwt_visit_buffer_take(state);
    if (state->claims->aud == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u, "failed to allocate JWT claim value");
    }
    break;
  case LONEJSON__JWT_FIELD_AUD_ARRAY_ITEM:
    value = lonejson__jwt_visit_buffer_take(state);
    if (value == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u, "failed to allocate JWT claim value");
    }
    if (lonejson__jwt_string_array_append(&state->claims->aud_array, value,
                                          error) != LONEJSON_STATUS_OK) {
      lonejson__owned_free(value);
      return error != NULL ? error->code : LONEJSON_STATUS_ALLOCATION_FAILED;
    }
    break;
  default:
    break;
  }
  state->active = LONEJSON__JWT_FIELD_NONE;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__jwt_begin_number_field(lonejson__jwt_claim_visit *state,
                                 lonejson__jwt_claim_field field, int *seen,
                                 const char *name, lonejson_error *error) {
  if (*seen) {
    return lonejson__jwt_claim_duplicate_error(error, name);
  }
  *seen = 1;
  state->active = field;
  lonejson__jwt_visit_buffer_reset(state);
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__jwt_claim_number_begin(void *user, const lonejson_value_path *path,
                                 lonejson_error *error) {
  lonejson__jwt_claim_visit *state = (lonejson__jwt_claim_visit *)user;

  if (state->header_mode) {
    if (lonejson__jwt_path_is(path, "alg") ||
        lonejson__jwt_path_is(path, "kid") ||
        lonejson__jwt_path_is(path, "typ")) {
      return lonejson__jwt_claim_type_error(error, "registered");
    }
    return LONEJSON_STATUS_OK;
  }
  if (lonejson__jwt_path_is(path, "exp")) {
    return lonejson__jwt_begin_number_field(state, LONEJSON__JWT_FIELD_EXP,
                                            &state->seen_exp, "exp", error);
  }
  if (lonejson__jwt_path_is(path, "nbf")) {
    return lonejson__jwt_begin_number_field(state, LONEJSON__JWT_FIELD_NBF,
                                            &state->seen_nbf, "nbf", error);
  }
  if (lonejson__jwt_path_is(path, "iat")) {
    return lonejson__jwt_begin_number_field(state, LONEJSON__JWT_FIELD_IAT,
                                            &state->seen_iat, "iat", error);
  }
  if (lonejson__jwt_path_is(path, "iss") ||
      lonejson__jwt_path_is(path, "sub") ||
      lonejson__jwt_path_is(path, "aud") ||
      lonejson__jwt_path_is_aud_item(path)) {
    return lonejson__jwt_claim_type_error(error, "registered");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__jwt_claim_number_chunk(void *user, const lonejson_value_path *path,
                                 const char *data, size_t len,
                                 lonejson_error *error) {
  lonejson__jwt_claim_visit *state = (lonejson__jwt_claim_visit *)user;
  (void)path;
  if (state->active != LONEJSON__JWT_FIELD_EXP &&
      state->active != LONEJSON__JWT_FIELD_NBF &&
      state->active != LONEJSON__JWT_FIELD_IAT) {
    return LONEJSON_STATUS_OK;
  }
  return lonejson__jwt_visit_buffer_append(state, data, len, error);
}

static lonejson_status
lonejson__jwt_claim_number_end(void *user, const lonejson_value_path *path,
                               lonejson_error *error) {
  lonejson__jwt_claim_visit *state = (lonejson__jwt_claim_visit *)user;
  lonejson_int64 value;
  (void)path;

  if (state->active != LONEJSON__JWT_FIELD_EXP &&
      state->active != LONEJSON__JWT_FIELD_NBF &&
      state->active != LONEJSON__JWT_FIELD_IAT) {
    state->active = LONEJSON__JWT_FIELD_NONE;
    return LONEJSON_STATUS_OK;
  }
  if (state->buffer == NULL ||
      !lonejson__parse_i64_token(state->buffer, &value)) {
    state->active = LONEJSON__JWT_FIELD_NONE;
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT date claim must be an integer");
  }
  if (value < 0) {
    state->active = LONEJSON__JWT_FIELD_NONE;
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT date claim must be non-negative");
  }
  if (state->active == LONEJSON__JWT_FIELD_EXP) {
    state->claims->exp = value;
    state->claims->has_exp = 1;
  } else if (state->active == LONEJSON__JWT_FIELD_NBF) {
    state->claims->nbf = value;
    state->claims->has_nbf = 1;
  } else {
    state->claims->iat = value;
    state->claims->has_iat = 1;
  }
  state->active = LONEJSON__JWT_FIELD_NONE;
  lonejson__jwt_visit_buffer_reset(state);
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__jwt_claim_object_begin(void *user, const lonejson_value_path *path,
                                 lonejson_error *error) {
  lonejson__jwt_claim_visit *state = (lonejson__jwt_claim_visit *)user;
  if (path != NULL && path->segment_count == 0u) {
    if (state->root_object_seen) {
      return lonejson__set_error(error, LONEJSON_STATUS_INTERNAL_ERROR, 0u, 0u,
                                 0u, "invalid JWT claim visitor state");
    }
    state->root_object_seen = 1;
    return LONEJSON_STATUS_OK;
  }
  if (state->header_mode && (lonejson__jwt_path_is(path, "alg") ||
                             lonejson__jwt_path_is(path, "kid") ||
                             lonejson__jwt_path_is(path, "typ"))) {
    return lonejson__jwt_claim_type_error(error, "registered");
  }
  if (!state->header_mode && (lonejson__jwt_path_is(path, "iss") ||
                              lonejson__jwt_path_is(path, "sub") ||
                              lonejson__jwt_path_is(path, "aud") ||
                              lonejson__jwt_path_is(path, "exp") ||
                              lonejson__jwt_path_is(path, "nbf") ||
                              lonejson__jwt_path_is(path, "iat") ||
                              lonejson__jwt_path_is_aud_item(path))) {
    return lonejson__jwt_claim_type_error(error, "registered");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__jwt_claim_array_begin(void *user, const lonejson_value_path *path,
                                lonejson_error *error) {
  lonejson__jwt_claim_visit *state = (lonejson__jwt_claim_visit *)user;
  if (!state->header_mode && lonejson__jwt_path_is(path, "aud")) {
    if (state->seen_aud) {
      return lonejson__jwt_claim_duplicate_error(error, "aud");
    }
    state->seen_aud = 1;
    return LONEJSON_STATUS_OK;
  }
  if (state->header_mode && (lonejson__jwt_path_is(path, "alg") ||
                             lonejson__jwt_path_is(path, "kid") ||
                             lonejson__jwt_path_is(path, "typ"))) {
    return lonejson__jwt_claim_type_error(error, "registered");
  }
  if (!state->header_mode && (lonejson__jwt_path_is(path, "iss") ||
                              lonejson__jwt_path_is(path, "sub") ||
                              lonejson__jwt_path_is(path, "exp") ||
                              lonejson__jwt_path_is(path, "nbf") ||
                              lonejson__jwt_path_is(path, "iat") ||
                              lonejson__jwt_path_is_aud_item(path))) {
    return lonejson__jwt_claim_type_error(error, "registered");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__jwt_claim_literal_type(void *user, const lonejson_value_path *path,
                                 lonejson_error *error) {
  lonejson__jwt_claim_visit *state = (lonejson__jwt_claim_visit *)user;
  if (state->header_mode && (lonejson__jwt_path_is(path, "alg") ||
                             lonejson__jwt_path_is(path, "kid") ||
                             lonejson__jwt_path_is(path, "typ"))) {
    return lonejson__jwt_claim_type_error(error, "registered");
  }
  if (!state->header_mode && (lonejson__jwt_path_is(path, "iss") ||
                              lonejson__jwt_path_is(path, "sub") ||
                              lonejson__jwt_path_is(path, "aud") ||
                              lonejson__jwt_path_is(path, "exp") ||
                              lonejson__jwt_path_is(path, "nbf") ||
                              lonejson__jwt_path_is(path, "iat") ||
                              lonejson__jwt_path_is_aud_item(path))) {
    return lonejson__jwt_claim_type_error(
        error, path->segment_count == 0u ? "root" : "registered");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__jwt_claim_bool(void *user,
                                                const lonejson_value_path *path,
                                                int value,
                                                lonejson_error *error) {
  (void)value;
  return lonejson__jwt_claim_literal_type(user, path, error);
}

static lonejson_path_value_visitor lonejson__jwt_claim_visitor(void) {
  lonejson_path_value_visitor visitor = lonejson_default_path_value_visitor();
  visitor.object_begin = lonejson__jwt_claim_object_begin;
  visitor.array_begin = lonejson__jwt_claim_array_begin;
  visitor.string_begin = lonejson__jwt_claim_string_begin;
  visitor.string_chunk = lonejson__jwt_claim_string_chunk;
  visitor.string_end = lonejson__jwt_claim_string_end;
  visitor.number_begin = lonejson__jwt_claim_number_begin;
  visitor.number_chunk = lonejson__jwt_claim_number_chunk;
  visitor.number_end = lonejson__jwt_claim_number_end;
  visitor.boolean_value = lonejson__jwt_claim_bool;
  visitor.null_value = lonejson__jwt_claim_literal_type;
  return visitor;
}

void lonejson_jwt_header_init(lonejson_jwt_header *header) {
  if (header != NULL) {
    memset(header, 0, sizeof(*header));
  }
}

void lonejson_jwt_header_cleanup(lonejson_jwt_header *header) {
  if (header != NULL) {
    lonejson__owned_free(header->alg);
    lonejson__owned_free(header->kid);
    lonejson__owned_free(header->typ);
    memset(header, 0, sizeof(*header));
  }
}

void lonejson_jwt_claims_init(lonejson_jwt_claims *claims) {
  if (claims != NULL) {
    memset(claims, 0, sizeof(*claims));
  }
}

void lonejson_jwt_claims_cleanup(lonejson_jwt_claims *claims) {
  size_t i;
  if (claims != NULL) {
    lonejson__owned_free(claims->iss);
    lonejson__owned_free(claims->sub);
    lonejson__owned_free(claims->aud);
    for (i = 0u; i < claims->aud_array.count; ++i) {
      lonejson__owned_free(claims->aud_array.items[i]);
    }
    lonejson__owned_free(claims->aud_array.items);
    memset(claims, 0, sizeof(*claims));
  }
}

static lonejson_status lonejson__jwt_parse_decoded_json(
    lonejson *runtime, const char *json, size_t len, int header_mode,
    lonejson_jwt_header *header, lonejson_jwt_claims *claims,
    lonejson_error *error) {
  lonejson__jwt_claim_visit state;
  lonejson_path_value_visitor visitor;
  lonejson_status status;

  memset(&state, 0, sizeof(state));
  state.header_mode = header_mode;
  state.header = header;
  state.claims = claims;
  visitor = lonejson__jwt_claim_visitor();
  status = lonejson_visit_path_value_buffer(runtime, json, len, &visitor,
                                            &state, error);
  lonejson__owned_free(state.buffer);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (!state.root_object_seen) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               header_mode ? "JWT header must be an object"
                                           : "JWT claims must be an object");
  }
  return LONEJSON_STATUS_OK;
}

static size_t lonejson__jwt_limit_or_default(size_t configured,
                                             size_t fallback) {
  return configured == 0u ? fallback : configured;
}

lonejson_status lonejson_jwt_decode_compact(
    lonejson *runtime, const char *token, size_t len,
    const lonejson_jwt_claim_policy *limits, lonejson_jwt_header *header,
    lonejson_jwt_claims *claims, lonejson_error *error) {
  lonejson_jwt_compact compact;
  size_t header_len;
  size_t claims_len;
  size_t header_limit;
  size_t claims_limit;
  char *header_json;
  char *claims_json;
  size_t needed;
  lonejson_status status;

  lonejson__clear_error(error);
  if (header == NULL || claims == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "JWT header and claims outputs are required");
  }
  if (limits != NULL && limits->max_token_bytes != 0u &&
      len > limits->max_token_bytes) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, len, 0u, 0u,
                               "JWT compact token exceeds configured limit");
  }
  lonejson_jwt_header_cleanup(header);
  lonejson_jwt_claims_cleanup(claims);
  status = lonejson_jwt_parse_compact(token, len, &compact, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson_base64url_decoded_len(
      compact.header.data, compact.header.len, &header_len, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson_base64url_decoded_len(
      compact.payload.data, compact.payload.len, &claims_len, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  header_limit = lonejson__jwt_limit_or_default(
      limits == NULL ? 0u : limits->max_decoded_header_bytes,
      LONEJSON__JWT_DEFAULT_MAX_HEADER_BYTES);
  claims_limit = lonejson__jwt_limit_or_default(
      limits == NULL ? 0u : limits->max_decoded_claims_bytes,
      LONEJSON__JWT_DEFAULT_MAX_CLAIMS_BYTES);
  if (header_len > header_limit || claims_len > claims_limit) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "JWT decoded JSON exceeds configured limit");
  }
  header_json = (char *)lonejson__owned_malloc(NULL, header_len + 1u);
  claims_json = (char *)lonejson__owned_malloc(NULL, claims_len + 1u);
  if (header_json == NULL || claims_json == NULL) {
    lonejson__owned_free(header_json);
    lonejson__owned_free(claims_json);
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to allocate decoded JWT JSON");
  }
  status = lonejson_base64url_decode(compact.header.data, compact.header.len,
                                     (unsigned char *)header_json, header_len,
                                     &needed, error);
  if (status == LONEJSON_STATUS_OK) {
    header_json[header_len] = '\0';
    status = lonejson_base64url_decode(
        compact.payload.data, compact.payload.len, (unsigned char *)claims_json,
        claims_len, &needed, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    claims_json[claims_len] = '\0';
    status = lonejson__jwt_parse_decoded_json(runtime, header_json, header_len,
                                              1, header, claims, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson__jwt_parse_decoded_json(runtime, claims_json, claims_len,
                                              0, header, claims, error);
  }
  lonejson__owned_free(header_json);
  lonejson__owned_free(claims_json);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_jwt_header_cleanup(header);
    lonejson_jwt_claims_cleanup(claims);
  }
  return status;
}

static int lonejson__jwt_string_in_list(const char *value,
                                        const char *const *items,
                                        size_t count) {
  size_t i;
  if (value == NULL) {
    return 0;
  }
  for (i = 0u; i < count; ++i) {
    if (items[i] != NULL && strcmp(value, items[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

static int lonejson__jwt_claim_has_audience(const lonejson_jwt_claims *claims,
                                            const char *audience) {
  size_t i;
  if (claims->aud != NULL && strcmp(claims->aud, audience) == 0) {
    return 1;
  }
  for (i = 0u; i < claims->aud_array.count; ++i) {
    if (claims->aud_array.items[i] != NULL &&
        strcmp(claims->aud_array.items[i], audience) == 0) {
      return 1;
    }
  }
  return 0;
}

static int
lonejson__jwt_any_audience_accepted(const lonejson_jwt_claims *claims,
                                    const lonejson_jwt_claim_policy *policy) {
  size_t i;
  for (i = 0u; i < policy->accepted_audience_count; ++i) {
    if (policy->accepted_audiences[i] != NULL &&
        lonejson__jwt_claim_has_audience(claims,
                                         policy->accepted_audiences[i])) {
      return 1;
    }
  }
  return 0;
}

static int lonejson__jwt_has_required_claim(const lonejson_jwt_claims *claims,
                                            const char *name) {
  if (strcmp(name, "iss") == 0) {
    return claims->iss != NULL;
  }
  if (strcmp(name, "sub") == 0) {
    return claims->sub != NULL;
  }
  if (strcmp(name, "aud") == 0) {
    return claims->aud != NULL || claims->aud_array.count != 0u;
  }
  if (strcmp(name, "exp") == 0) {
    return claims->has_exp;
  }
  if (strcmp(name, "nbf") == 0) {
    return claims->has_nbf;
  }
  if (strcmp(name, "iat") == 0) {
    return claims->has_iat;
  }
  return 0;
}

lonejson_status lonejson_jwt_validate_claims(
    const lonejson_jwt_header *header, const lonejson_jwt_claims *claims,
    const lonejson_jwt_claim_policy *policy, lonejson_error *error) {
  size_t i;
  lonejson_int64 skew;

  lonejson__clear_error(error);
  if (header == NULL || claims == NULL || policy == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "JWT header, claims, and policy are required");
  }
  if (policy->accepted_algs == NULL || policy->accepted_alg_count == 0u ||
      policy->accepted_issuers == NULL || policy->accepted_issuer_count == 0u ||
      policy->accepted_audiences == NULL ||
      policy->accepted_audience_count == 0u || policy->now < 0 ||
      policy->allowed_clock_skew < 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JWT claim policy is incomplete");
  }
  if (header->alg == NULL || strcmp(header->alg, "none") == 0 ||
      !lonejson__jwt_string_in_list(header->alg, policy->accepted_algs,
                                    policy->accepted_alg_count)) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT algorithm is not accepted");
  }
  if (!lonejson__jwt_string_in_list(claims->iss, policy->accepted_issuers,
                                    policy->accepted_issuer_count)) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT issuer is not accepted");
  }
  if (!lonejson__jwt_any_audience_accepted(claims, policy)) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT audience is not accepted");
  }
  for (i = 0u; i < policy->required_claim_count; ++i) {
    if (policy->required_claims == NULL || policy->required_claims[i] == NULL ||
        !lonejson__jwt_has_required_claim(claims, policy->required_claims[i])) {
      return lonejson__set_error(error, LONEJSON_STATUS_MISSING_REQUIRED_FIELD,
                                 0u, 0u, 0u, "JWT required claim is missing");
    }
  }
  skew = policy->allowed_clock_skew;
  if (claims->has_exp && policy->now >= claims->exp &&
      policy->now - claims->exp >= skew) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT is expired");
  }
  if (claims->has_nbf && policy->now < claims->nbf &&
      claims->nbf - policy->now > skew) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT is not yet valid");
  }
  if (claims->has_iat && policy->now < claims->iat &&
      claims->iat - policy->now > skew) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT issued-at time is in the future");
  }
  return LONEJSON_STATUS_OK;
}

#ifdef LONEJSON_WITH_OPENSSL
static unsigned char *lonejson__jwt_decode_base64url_alloc(
    const char *data, size_t len, size_t *out_len, const char *what,
    lonejson_error *error) {
  unsigned char *out;
  size_t decoded_len;
  size_t needed;
  lonejson_status status;

  status = lonejson_base64url_decoded_len(data, len, &decoded_len, error);
  if (status != LONEJSON_STATUS_OK) {
    return NULL;
  }
  out = (unsigned char *)lonejson__owned_malloc(
      NULL, decoded_len == 0u ? 1u : decoded_len);
  if (out == NULL) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                              0u, "failed to allocate decoded JWT %s", what);
    return NULL;
  }
  status = lonejson_base64url_decode(data, len, out, decoded_len, &needed,
                                     error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson__owned_free(out);
    return NULL;
  }
  *out_len = decoded_len;
  return out;
}

static EVP_PKEY *lonejson__jwt_rsa_public_key_from_jwk(
    const lonejson_jwk *jwk, lonejson_error *error) {
  EVP_PKEY_CTX *ctx = NULL;
  EVP_PKEY *pkey = NULL;
  OSSL_PARAM_BLD *builder = NULL;
  OSSL_PARAM *params = NULL;
  BIGNUM *n_bn = NULL;
  BIGNUM *e_bn = NULL;
  unsigned char *n = NULL;
  unsigned char *e = NULL;
  size_t n_len = 0u;
  size_t e_len = 0u;
  int ok = 0;

  if (jwk->n == NULL || jwk->e == NULL || jwk->n[0] == '\0' ||
      jwk->e[0] == '\0') {
    (void)lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                              "RSA JWK n and e members are required");
    return NULL;
  }
  n = lonejson__jwt_decode_base64url_alloc(jwk->n, strlen(jwk->n), &n_len,
                                           "JWK modulus", error);
  e = lonejson__jwt_decode_base64url_alloc(jwk->e, strlen(jwk->e), &e_len,
                                           "JWK exponent", error);
  if (n == NULL || e == NULL || n_len == 0u || e_len == 0u) {
    if (n != NULL && n_len == 0u) {
      (void)lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                                "RSA JWK modulus must not be empty");
    } else if (e != NULL && e_len == 0u) {
      (void)lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                                "RSA JWK exponent must not be empty");
    }
    goto done;
  }
  if (n_len > LONEJSON__JWT_MAX_RSA_COMPONENT_BYTES ||
      e_len > LONEJSON__JWT_MAX_RSA_COMPONENT_BYTES) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                              "RSA JWK key material exceeds configured limit");
    goto done;
  }
  if (n_len > (size_t)INT_MAX || e_len > (size_t)INT_MAX) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                              "RSA JWK key material exceeds supported size");
    goto done;
  }
  n_bn = BN_bin2bn(n, (int)n_len, NULL);
  e_bn = BN_bin2bn(e, (int)e_len, NULL);
  if (n_bn == NULL || e_bn == NULL) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                              0u, "failed to allocate RSA JWK key material");
    goto done;
  }
  builder = OSSL_PARAM_BLD_new();
  ctx = EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL);
  if (builder == NULL || ctx == NULL) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                              0u, "failed to allocate RSA public key context");
    goto done;
  }
  if (!OSSL_PARAM_BLD_push_BN(builder, OSSL_PKEY_PARAM_RSA_N, n_bn) ||
      !OSSL_PARAM_BLD_push_BN(builder, OSSL_PKEY_PARAM_RSA_E, e_bn)) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                              0u, "failed to allocate RSA public key params");
    goto done;
  }
  params = OSSL_PARAM_BLD_to_param(builder);
  if (params == NULL) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                              0u, "failed to allocate RSA public key params");
    goto done;
  }
  if (EVP_PKEY_fromdata_init(ctx) <= 0 ||
      EVP_PKEY_fromdata(ctx, &pkey, EVP_PKEY_PUBLIC_KEY, params) <= 0 ||
      pkey == NULL) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                              "RSA JWK public key is invalid");
    goto done;
  }
  ok = 1;

done:
  lonejson__owned_free(n);
  lonejson__owned_free(e);
  BN_free(n_bn);
  BN_free(e_bn);
  OSSL_PARAM_free(params);
  OSSL_PARAM_BLD_free(builder);
  EVP_PKEY_CTX_free(ctx);
  if (!ok) {
    EVP_PKEY_free(pkey);
    pkey = NULL;
  }
  return pkey;
}

static lonejson_status lonejson__jwt_validate_rs256_signature(
    const lonejson_jwt_compact *jwt, const lonejson_jwk *jwk, int pss,
    lonejson_error *error) {
  EVP_PKEY *pkey = NULL;
  EVP_MD_CTX *md = NULL;
  EVP_PKEY_CTX *pkey_ctx = NULL;
  unsigned char *signature = NULL;
  size_t signature_len = 0u;
  int verify_result;

  signature = lonejson__jwt_decode_base64url_alloc(
      jwt->signature.data, jwt->signature.len, &signature_len, "signature",
      error);
  if (signature == NULL) {
    return error != NULL ? error->code : LONEJSON_STATUS_INVALID_JSON;
  }
  if (signature_len == 0u) {
    lonejson__owned_free(signature);
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u,
                               0u, "JWT signature must not be empty");
  }
  pkey = lonejson__jwt_rsa_public_key_from_jwk(jwk, error);
  if (pkey == NULL) {
    lonejson__owned_free(signature);
    return error != NULL ? error->code : LONEJSON_STATUS_TYPE_MISMATCH;
  }
  md = EVP_MD_CTX_new();
  if (md == NULL) {
    lonejson__owned_free(signature);
    EVP_PKEY_free(pkey);
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to allocate JWT verifier");
  }
  if (EVP_DigestVerifyInit(md, &pkey_ctx, EVP_sha256(), NULL, pkey) <= 0 ||
      EVP_PKEY_CTX_set_rsa_padding(
          pkey_ctx, pss ? RSA_PKCS1_PSS_PADDING : RSA_PKCS1_PADDING) <= 0 ||
      (pss && EVP_PKEY_CTX_set_rsa_pss_saltlen(pkey_ctx, 32) <= 0)) {
    lonejson__owned_free(signature);
    EVP_MD_CTX_free(md);
    EVP_PKEY_free(pkey);
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u,
                               0u, "failed to initialize JWT verifier");
  }
  verify_result =
      EVP_DigestVerify(md, signature, signature_len,
                       (const unsigned char *)jwt->signing_input.data,
                       jwt->signing_input.len);
  lonejson__owned_free(signature);
  EVP_MD_CTX_free(md);
  EVP_PKEY_free(pkey);
  if (verify_result == 1) {
    return LONEJSON_STATUS_OK;
  }
  return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                             "JWT signature validation failed");
}

static EVP_PKEY *lonejson__jwt_ec_public_key_from_jwk(
    const lonejson_jwk *jwk, lonejson_error *error) {
  EVP_PKEY_CTX *ctx = NULL;
  EVP_PKEY *pkey = NULL;
  OSSL_PARAM_BLD *builder = NULL;
  OSSL_PARAM *params = NULL;
  unsigned char *x = NULL;
  unsigned char *y = NULL;
  unsigned char pub[65];
  size_t x_len = 0u;
  size_t y_len = 0u;
  int ok = 0;

  if (!lonejson__auth_streq(jwk->crv, "P-256")) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                              "ES256 requires a P-256 JWK");
    return NULL;
  }
  x = lonejson__jwt_decode_base64url_alloc(jwk->x, strlen(jwk->x), &x_len,
                                           "JWK x coordinate", error);
  y = lonejson__jwt_decode_base64url_alloc(jwk->y, strlen(jwk->y), &y_len,
                                           "JWK y coordinate", error);
  if (x == NULL || y == NULL) {
    goto done;
  }
  if (x_len != 32u || y_len != 32u) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                              "ES256 JWK coordinates must be 32 bytes");
    goto done;
  }
  pub[0] = 0x04u;
  memcpy(pub + 1u, x, 32u);
  memcpy(pub + 33u, y, 32u);
  builder = OSSL_PARAM_BLD_new();
  ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
  if (builder == NULL || ctx == NULL) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                              0u, "failed to allocate EC public key context");
    goto done;
  }
  if (!OSSL_PARAM_BLD_push_utf8_string(builder, OSSL_PKEY_PARAM_GROUP_NAME,
                                       "prime256v1", 0u) ||
      !OSSL_PARAM_BLD_push_octet_string(builder, OSSL_PKEY_PARAM_PUB_KEY, pub,
                                        sizeof(pub))) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                              0u, "failed to allocate EC public key params");
    goto done;
  }
  params = OSSL_PARAM_BLD_to_param(builder);
  if (params == NULL) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                              0u, "failed to allocate EC public key params");
    goto done;
  }
  if (EVP_PKEY_fromdata_init(ctx) <= 0 ||
      EVP_PKEY_fromdata(ctx, &pkey, EVP_PKEY_PUBLIC_KEY, params) <= 0 ||
      pkey == NULL) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                              "EC JWK public key is invalid");
    goto done;
  }
  ok = 1;

done:
  lonejson__owned_free(x);
  lonejson__owned_free(y);
  OSSL_PARAM_free(params);
  OSSL_PARAM_BLD_free(builder);
  EVP_PKEY_CTX_free(ctx);
  if (!ok) {
    EVP_PKEY_free(pkey);
    pkey = NULL;
  }
  return pkey;
}

static unsigned char *lonejson__jwt_es256_der_signature_alloc(
    const unsigned char *signature, size_t signature_len, size_t *out_len,
    lonejson_error *error) {
  ECDSA_SIG *sig = NULL;
  BIGNUM *r = NULL;
  BIGNUM *s = NULL;
  unsigned char *der = NULL;
  unsigned char *cursor;
  int der_len;

  if (signature_len != 64u) {
    (void)lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                              "ES256 signature must be 64 bytes");
    return NULL;
  }
  sig = ECDSA_SIG_new();
  r = BN_bin2bn(signature, 32, NULL);
  s = BN_bin2bn(signature + 32u, 32, NULL);
  if (sig == NULL || r == NULL || s == NULL) {
    ECDSA_SIG_free(sig);
    BN_free(r);
    BN_free(s);
    (void)lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                              0u, "failed to allocate ES256 signature");
    return NULL;
  }
  if (ECDSA_SIG_set0(sig, r, s) != 1) {
    ECDSA_SIG_free(sig);
    BN_free(r);
    BN_free(s);
    (void)lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                              0u, "failed to prepare ES256 signature");
    return NULL;
  }
  r = NULL;
  s = NULL;
  der_len = i2d_ECDSA_SIG(sig, NULL);
  if (der_len <= 0) {
    ECDSA_SIG_free(sig);
    (void)lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                              "ES256 signature is invalid");
    return NULL;
  }
  der = (unsigned char *)lonejson__owned_malloc(NULL, (size_t)der_len);
  if (der == NULL) {
    ECDSA_SIG_free(sig);
    (void)lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                              0u, "failed to allocate ES256 signature");
    return NULL;
  }
  cursor = der;
  der_len = i2d_ECDSA_SIG(sig, &cursor);
  ECDSA_SIG_free(sig);
  if (der_len <= 0) {
    lonejson__owned_free(der);
    (void)lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                              "ES256 signature is invalid");
    return NULL;
  }
  *out_len = (size_t)der_len;
  return der;
}

static lonejson_status lonejson__jwt_validate_es256_signature(
    const lonejson_jwt_compact *jwt, const lonejson_jwk *jwk,
    lonejson_error *error) {
  EVP_PKEY *pkey = NULL;
  EVP_MD_CTX *md = NULL;
  unsigned char *signature = NULL;
  unsigned char *der_signature = NULL;
  size_t signature_len = 0u;
  size_t der_signature_len = 0u;
  int verify_result;

  signature = lonejson__jwt_decode_base64url_alloc(
      jwt->signature.data, jwt->signature.len, &signature_len, "signature",
      error);
  if (signature == NULL) {
    return error != NULL ? error->code : LONEJSON_STATUS_INVALID_JSON;
  }
  der_signature = lonejson__jwt_es256_der_signature_alloc(
      signature, signature_len, &der_signature_len, error);
  lonejson__owned_free(signature);
  if (der_signature == NULL) {
    return error != NULL ? error->code : LONEJSON_STATUS_TYPE_MISMATCH;
  }
  pkey = lonejson__jwt_ec_public_key_from_jwk(jwk, error);
  if (pkey == NULL) {
    lonejson__owned_free(der_signature);
    return error != NULL ? error->code : LONEJSON_STATUS_TYPE_MISMATCH;
  }
  md = EVP_MD_CTX_new();
  if (md == NULL) {
    lonejson__owned_free(der_signature);
    EVP_PKEY_free(pkey);
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to allocate JWT verifier");
  }
  if (EVP_DigestVerifyInit(md, NULL, EVP_sha256(), NULL, pkey) <= 0) {
    lonejson__owned_free(der_signature);
    EVP_MD_CTX_free(md);
    EVP_PKEY_free(pkey);
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u,
                               0u, "failed to initialize JWT verifier");
  }
  verify_result =
      EVP_DigestVerify(md, der_signature, der_signature_len,
                       (const unsigned char *)jwt->signing_input.data,
                       jwt->signing_input.len);
  lonejson__owned_free(der_signature);
  EVP_MD_CTX_free(md);
  EVP_PKEY_free(pkey);
  if (verify_result == 1) {
    return LONEJSON_STATUS_OK;
  }
  return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                             "JWT signature validation failed");
}

static lonejson_status lonejson__jwt_validate_eddsa_signature(
    const lonejson_jwt_compact *jwt, const lonejson_jwk *jwk,
    lonejson_error *error) {
  EVP_PKEY *pkey = NULL;
  EVP_MD_CTX *md = NULL;
  unsigned char *x = NULL;
  unsigned char *signature = NULL;
  size_t x_len = 0u;
  size_t signature_len = 0u;
  int verify_result;

  if (!lonejson__auth_streq(jwk->crv, "Ed25519")) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "EdDSA requires an Ed25519 JWK");
  }
  x = lonejson__jwt_decode_base64url_alloc(jwk->x, strlen(jwk->x), &x_len,
                                           "JWK x coordinate", error);
  signature = lonejson__jwt_decode_base64url_alloc(
      jwt->signature.data, jwt->signature.len, &signature_len, "signature",
      error);
  if (x == NULL || signature == NULL) {
    lonejson__owned_free(x);
    lonejson__owned_free(signature);
    return error != NULL ? error->code : LONEJSON_STATUS_INVALID_JSON;
  }
  if (x_len != 32u || signature_len != 64u) {
    lonejson__owned_free(x);
    lonejson__owned_free(signature);
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "Ed25519 JWK or signature length is invalid");
  }
  pkey = EVP_PKEY_new_raw_public_key_ex(NULL, "ED25519", NULL, x, x_len);
  lonejson__owned_free(x);
  if (pkey == NULL) {
    lonejson__owned_free(signature);
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "Ed25519 JWK public key is invalid");
  }
  md = EVP_MD_CTX_new();
  if (md == NULL) {
    EVP_PKEY_free(pkey);
    lonejson__owned_free(signature);
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to allocate JWT verifier");
  }
  if (EVP_DigestVerifyInit(md, NULL, NULL, NULL, pkey) <= 0) {
    EVP_MD_CTX_free(md);
    EVP_PKEY_free(pkey);
    lonejson__owned_free(signature);
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u,
                               0u, "failed to initialize JWT verifier");
  }
  verify_result =
      EVP_DigestVerify(md, signature, signature_len,
                       (const unsigned char *)jwt->signing_input.data,
                       jwt->signing_input.len);
  EVP_MD_CTX_free(md);
  EVP_PKEY_free(pkey);
  lonejson__owned_free(signature);
  if (verify_result == 1) {
    return LONEJSON_STATUS_OK;
  }
  return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                             "JWT signature validation failed");
}

static lonejson_status lonejson__openssl_auth_verify_jws(
    void *user_data, const lonejson_jws_verify_request *request,
    lonejson_error *error) {
  const lonejson_jwt_compact *jwt;
  const lonejson_jwt_header *header;
  const lonejson_jwk *jwk;

  (void)user_data;
  if (request == NULL || request->jwt == NULL || request->header == NULL ||
      request->jwk == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JWS verification request is required");
  }
  jwt = request->jwt;
  header = request->header;
  jwk = request->jwk;
  if (strcmp(header->alg, "RS256") == 0 ||
      strcmp(header->alg, "PS256") == 0) {
    if (!lonejson__auth_streq(jwk->kty, "RSA")) {
      return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u,
                                 0u,
                                 "JWT JWK key type does not match algorithm");
    }
    return lonejson__jwt_validate_rs256_signature(
        jwt, jwk, strcmp(header->alg, "PS256") == 0, error);
  }
  if (strcmp(header->alg, "ES256") == 0) {
    if (!lonejson__auth_streq(jwk->kty, "EC")) {
      return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u,
                                 0u,
                                 "JWT JWK key type does not match algorithm");
    }
    return lonejson__jwt_validate_es256_signature(jwt, jwk, error);
  }
  if (strcmp(header->alg, "EdDSA") == 0) {
    if (!lonejson__auth_streq(jwk->kty, "OKP")) {
      return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u,
                                 0u,
                                 "JWT JWK key type does not match algorithm");
    }
    return lonejson__jwt_validate_eddsa_signature(jwt, jwk, error);
  }
  {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT signature algorithm is not supported");
  }
}

static lonejson_status lonejson__openssl_auth_random_bytes(
    void *user_data, unsigned char *dst, size_t len, lonejson_error *error) {
  (void)user_data;
  if (dst == NULL && len != 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "random byte output is required");
  }
  if (len > (size_t)INT_MAX) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "random byte request exceeds OpenSSL limit");
  }
  if (len != 0u && RAND_bytes(dst, (int)len) != 1) {
    return lonejson__set_error(error, LONEJSON_STATUS_INTERNAL_ERROR, 0u, 0u,
                               0u, "failed to generate random bytes");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__openssl_auth_sha256(
    void *user_data, const void *data, size_t len, unsigned char out[32],
    lonejson_error *error) {
  unsigned int digest_len = 0u;

  (void)user_data;
  if ((data == NULL && len != 0u) || out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "SHA-256 input and output are required");
  }
  if (EVP_Digest(data, len, out, &digest_len, EVP_sha256(), NULL) != 1 ||
      digest_len != 32u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INTERNAL_ERROR, 0u, 0u,
                               0u, "failed to compute SHA-256 digest");
  }
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_auth_provider_init_openssl(
    lonejson_auth_provider *provider,
    const lonejson_openssl_auth_provider_config *config,
    lonejson_error *error) {
  lonejson__clear_error(error);
  if (provider == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "auth provider output is required");
  }
  if (config != NULL && (config->libctx != NULL || config->propq != NULL)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "OpenSSL provider config fields are reserved");
  }
  memset(provider, 0, sizeof(*provider));
  provider->verify_jws = lonejson__openssl_auth_verify_jws;
  provider->random_bytes = lonejson__openssl_auth_random_bytes;
  provider->sha256 = lonejson__openssl_auth_sha256;
  return LONEJSON_STATUS_OK;
}
#endif

lonejson_status lonejson_jwt_validate_signature(
    const lonejson_jwt_compact *jwt, const lonejson_jwt_header *header,
    const lonejson_jwk *jwk, lonejson_error *error) {
  lonejson_jws_verify_request request;
#ifdef LONEJSON_WITH_OPENSSL
  lonejson_auth_provider provider;
#endif

  lonejson__clear_error(error);
  if (jwt == NULL || header == NULL || jwk == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JWT, header, and JWK are required");
  }
  if (header->alg == NULL || strcmp(header->alg, "none") == 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT signature algorithm is not accepted");
  }
  if (jwk->alg != NULL && strcmp(jwk->alg, header->alg) != 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT JWK algorithm does not match header");
  }
  if (header->kid != NULL && jwk->kid != NULL &&
      strcmp(header->kid, jwk->kid) != 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT JWK key id does not match header");
  }
  if (jwk->use != NULL && strcmp(jwk->use, "sig") != 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT JWK use is not valid for signatures");
  }
  request.jwt = jwt;
  request.header = header;
  request.jwk = jwk;
#ifdef LONEJSON_WITH_OPENSSL
  if (lonejson_auth_provider_init_openssl(&provider, NULL, error) !=
      LONEJSON_STATUS_OK) {
    return error != NULL ? error->code : LONEJSON_STATUS_INTERNAL_ERROR;
  }
  return provider.verify_jws(provider.user_data, &request, error);
#else
  (void)request;
  return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                             "JWT signature validation requires an auth "
                             "crypto provider");
#endif
}

lonejson_status lonejson_jwt_validate_signature_with_runtime(
    lonejson *runtime, const lonejson_jwt_compact *jwt,
    const lonejson_jwt_header *header, const lonejson_jwk *jwk,
    lonejson_error *error) {
  lonejson__runtime_borrow borrow;
  const lonejson_runtime *runtime_state;
  lonejson_jws_verify_request request;
  lonejson_status status;

  lonejson__clear_error(error);
  if (jwt == NULL || header == NULL || jwk == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JWT, header, and JWK are required");
  }
  if (header->alg == NULL || strcmp(header->alg, "none") == 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT signature algorithm is not accepted");
  }
  if (jwk->alg != NULL && strcmp(jwk->alg, header->alg) != 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT JWK algorithm does not match header");
  }
  if (header->kid != NULL && jwk->kid != NULL &&
      strcmp(header->kid, jwk->kid) != 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT JWK key id does not match header");
  }
  if (jwk->use != NULL && strcmp(jwk->use, "sig") != 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT JWK use is not valid for signatures");
  }
  runtime_state = lonejson__require_runtime_borrow(runtime, &borrow, error);
  if (runtime_state == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (!runtime_state->has_auth_provider ||
      runtime_state->auth_provider.verify_jws == NULL) {
    lonejson__runtime_borrow_release(&borrow);
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "runtime auth provider cannot verify JWS");
  }
  request.jwt = jwt;
  request.header = header;
  request.jwk = jwk;
  status = runtime_state->auth_provider.verify_jws(
      runtime_state->auth_provider.user_data, &request, error);
  lonejson__runtime_borrow_release(&borrow);
  return status;
}

#ifdef LONEJSON_WITH_OIDC
const char *lonejson_auth_failure_string(lonejson_auth_failure failure) {
  switch (failure) {
  case LONEJSON_AUTH_FAILURE_NONE:
    return "none";
  case LONEJSON_AUTH_FAILURE_MISSING_CREDENTIALS:
    return "missing_credentials";
  case LONEJSON_AUTH_FAILURE_MALFORMED_TOKEN:
    return "malformed_token";
  case LONEJSON_AUTH_FAILURE_CACHE_UNAVAILABLE:
    return "cache_unavailable";
  case LONEJSON_AUTH_FAILURE_KEY_NOT_FOUND:
    return "key_not_found";
  case LONEJSON_AUTH_FAILURE_INVALID_SIGNATURE:
    return "invalid_signature";
  case LONEJSON_AUTH_FAILURE_EXPIRED_TOKEN:
    return "expired_token";
  case LONEJSON_AUTH_FAILURE_NOT_YET_VALID:
    return "not_yet_valid";
  case LONEJSON_AUTH_FAILURE_ISSUER_MISMATCH:
    return "issuer_mismatch";
  case LONEJSON_AUTH_FAILURE_AUDIENCE_MISMATCH:
    return "audience_mismatch";
  case LONEJSON_AUTH_FAILURE_CLAIMS_INVALID:
    return "claims_invalid";
  }
  return "unknown";
}

void lonejson_oidc_bearer_validation_init(
    lonejson_oidc_bearer_validation *validation) {
  if (validation == NULL) {
    return;
  }
  memset(validation, 0, sizeof(*validation));
  lonejson_jwt_header_init(&validation->header);
  lonejson_jwt_claims_init(&validation->claims);
}

void lonejson_oidc_bearer_validation_cleanup(
    lonejson_oidc_bearer_validation *validation) {
  if (validation == NULL) {
    return;
  }
  lonejson_jwt_header_cleanup(&validation->header);
  lonejson_jwt_claims_cleanup(&validation->claims);
  validation->failure = LONEJSON_AUTH_FAILURE_NONE;
  validation->jwk = NULL;
}

static int lonejson__oidc_http_ows(char ch) {
  return ch == ' ' || ch == '\t';
}

static int lonejson__oidc_ascii_prefix_case_equal(const char *a,
                                                  const char *b,
                                                  size_t len) {
  size_t i;
  unsigned char ca;
  unsigned char cb;

  for (i = 0u; i < len; ++i) {
    ca = (unsigned char)a[i];
    cb = (unsigned char)b[i];
    if (ca >= (unsigned char)'A' && ca <= (unsigned char)'Z') {
      ca = (unsigned char)(ca - (unsigned char)'A' + (unsigned char)'a');
    }
    if (cb >= (unsigned char)'A' && cb <= (unsigned char)'Z') {
      cb = (unsigned char)(cb - (unsigned char)'A' + (unsigned char)'a');
    }
    if (ca != cb) {
      return 0;
    }
  }
  return 1;
}

lonejson_status lonejson_oidc_authorization_bearer_token(
    const char *authorization_header, lonejson_jwt_segment *out,
    lonejson_error *error) {
  const char *cursor;
  const char *token_begin;
  const char *token_end;

  lonejson__clear_error(error);
  if (out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "bearer token output is required");
  }
  out->data = NULL;
  out->len = 0u;
  if (authorization_header == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_MISSING_REQUIRED_FIELD,
                               0u, 0u, 0u,
                               "Authorization bearer credential is required");
  }
  cursor = authorization_header;
  while (lonejson__oidc_http_ows(*cursor)) {
    ++cursor;
  }
  if (*cursor == '\0') {
    return lonejson__set_error(error, LONEJSON_STATUS_MISSING_REQUIRED_FIELD,
                               0u, 0u, 0u,
                               "Authorization bearer credential is required");
  }
  if (strlen(cursor) < 7u ||
      !lonejson__oidc_ascii_prefix_case_equal(cursor, "Bearer", 6u) ||
      !lonejson__oidc_http_ows(cursor[6])) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               "Authorization header is not a Bearer token");
  }
  cursor += 7u;
  while (lonejson__oidc_http_ows(*cursor)) {
    ++cursor;
  }
  token_begin = cursor;
  while (*cursor != '\0' && !lonejson__oidc_http_ows(*cursor)) {
    ++cursor;
  }
  token_end = cursor;
  while (lonejson__oidc_http_ows(*cursor)) {
    ++cursor;
  }
  if (token_end == token_begin) {
    return lonejson__set_error(error, LONEJSON_STATUS_MISSING_REQUIRED_FIELD,
                               0u, 0u, 0u,
                               "Authorization bearer token is required");
  }
  if (*cursor != '\0') {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                               "Authorization bearer token has trailing data");
  }
  out->data = token_begin;
  out->len = (size_t)(token_end - token_begin);
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__oidc_bearer_fail(
    lonejson_oidc_bearer_validation *out, lonejson_auth_failure failure,
    lonejson_status status, lonejson_error *error, const char *message) {
  lonejson_oidc_bearer_validation_cleanup(out);
  out->failure = failure;
  if (error != NULL && error->code == LONEJSON_STATUS_OK) {
    (void)lonejson__set_error(error, status, 0u, 0u, 0u, "%s", message);
  }
  return status;
}

static lonejson_auth_failure lonejson__oidc_bearer_classify_claim_failure(
    const lonejson_jwt_header *header, const lonejson_jwt_claims *claims,
    const lonejson_jwt_claim_policy *policy) {
  lonejson_int64 skew;

  if (header == NULL || claims == NULL || policy == NULL ||
      policy->accepted_algs == NULL || policy->accepted_alg_count == 0u ||
      policy->accepted_issuers == NULL || policy->accepted_issuer_count == 0u ||
      policy->accepted_audiences == NULL ||
      policy->accepted_audience_count == 0u || policy->allowed_clock_skew < 0) {
    return LONEJSON_AUTH_FAILURE_CLAIMS_INVALID;
  }
  if (!lonejson__jwt_string_in_list(claims->iss, policy->accepted_issuers,
                                    policy->accepted_issuer_count)) {
    return LONEJSON_AUTH_FAILURE_ISSUER_MISMATCH;
  }
  if (!lonejson__jwt_any_audience_accepted(claims, policy)) {
    return LONEJSON_AUTH_FAILURE_AUDIENCE_MISMATCH;
  }
  skew = policy->allowed_clock_skew;
  if (claims->has_exp && policy->now >= claims->exp &&
      policy->now - claims->exp >= skew) {
    return LONEJSON_AUTH_FAILURE_EXPIRED_TOKEN;
  }
  if ((claims->has_nbf && policy->now < claims->nbf &&
       claims->nbf - policy->now > skew) ||
      (claims->has_iat && policy->now < claims->iat &&
       claims->iat - policy->now > skew)) {
    return LONEJSON_AUTH_FAILURE_NOT_YET_VALID;
  }
  return LONEJSON_AUTH_FAILURE_CLAIMS_INVALID;
}

lonejson_status lonejson_oidc_validate_bearer_token(
    lonejson *runtime, const lonejson_oidc_bearer_validation_request *request,
    lonejson_oidc_bearer_validation *out, lonejson_error *error) {
  lonejson_jwt_segment token;
  lonejson_jwt_compact compact;
  lonejson_jwk_select_options select_options;
  const lonejson_jwk *selected = NULL;
  lonejson_status status;

  lonejson__clear_error(error);
  if (runtime == NULL || request == NULL || out == NULL ||
      request->jwks_cache == NULL || request->jwks_policy == NULL ||
      request->claim_policy == NULL) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "runtime, bearer request, JWKS cache, JWKS policy, and claim policy "
        "are required");
  }
  lonejson_oidc_bearer_validation_cleanup(out);

  status = lonejson_oidc_authorization_bearer_token(
      request->authorization_header, &token, error);
  if (status != LONEJSON_STATUS_OK) {
    return lonejson__oidc_bearer_fail(
        out,
        status == LONEJSON_STATUS_MISSING_REQUIRED_FIELD
            ? LONEJSON_AUTH_FAILURE_MISSING_CREDENTIALS
            : LONEJSON_AUTH_FAILURE_MALFORMED_TOKEN,
        status, error, "Authorization bearer token is invalid");
  }

  status = lonejson_jwt_parse_compact(token.data, token.len, &compact, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_jwt_decode_compact(runtime, token.data, token.len,
                                         request->claim_policy, &out->header,
                                         &out->claims, error);
  }
  if (status != LONEJSON_STATUS_OK) {
    return lonejson__oidc_bearer_fail(out,
                                      LONEJSON_AUTH_FAILURE_MALFORMED_TOKEN,
                                      status, error,
                                      "Authorization bearer JWT is malformed");
  }
  if (out->header.kid == NULL || out->header.kid[0] == '\0') {
    return lonejson__oidc_bearer_fail(
        out, LONEJSON_AUTH_FAILURE_KEY_NOT_FOUND,
        LONEJSON_STATUS_TYPE_MISMATCH, error,
        "JWT kid is required for JWKS bearer validation");
  }

  memset(&select_options, 0, sizeof(select_options));
  select_options.kid = out->header.kid;
  select_options.kty =
      (lonejson__auth_streq(out->header.alg, "RS256") ||
       lonejson__auth_streq(out->header.alg, "PS256"))
          ? "RSA"
          : (lonejson__auth_streq(out->header.alg, "ES256")
                 ? "EC"
                 : (lonejson__auth_streq(out->header.alg, "EdDSA") ? "OKP"
                                                                    : NULL));
  status = lonejson_oidc_jwks_cache_select(
      request->jwks_cache, request->jwks_policy, &select_options, &selected,
      error);
  if (status != LONEJSON_STATUS_OK) {
    return lonejson__oidc_bearer_fail(
        out, LONEJSON_AUTH_FAILURE_CACHE_UNAVAILABLE, status, error,
        "OIDC JWKS cache is unavailable for bearer validation");
  }
  if (selected == NULL) {
    return lonejson__oidc_bearer_fail(
        out, LONEJSON_AUTH_FAILURE_KEY_NOT_FOUND,
        LONEJSON_STATUS_TYPE_MISMATCH, error,
        "no JWKS key matches bearer JWT header");
  }

  status = lonejson_jwt_validate_signature_with_runtime(
      runtime, &compact, &out->header, selected, error);
  if (status != LONEJSON_STATUS_OK) {
    return lonejson__oidc_bearer_fail(
        out, LONEJSON_AUTH_FAILURE_INVALID_SIGNATURE, status, error,
        "bearer JWT signature validation failed");
  }

  status = lonejson_jwt_validate_claims(&out->header, &out->claims,
                                        request->claim_policy, error);
  if (status != LONEJSON_STATUS_OK) {
    return lonejson__oidc_bearer_fail(
        out,
        lonejson__oidc_bearer_classify_claim_failure(
            &out->header, &out->claims, request->claim_policy),
        status, error, "bearer JWT claims validation failed");
  }

  out->failure = LONEJSON_AUTH_FAILURE_NONE;
  out->jwk = selected;
  return LONEJSON_STATUS_OK;
}
#endif
#endif
