#ifdef LONEJSON_WITH_JWT
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>
#include <openssl/params.h>
#include <openssl/rsa.h>

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
#endif

#define LONEJSON__JWT_DEFAULT_MAX_HEADER_BYTES (16u * 1024u)
#define LONEJSON__JWT_DEFAULT_MAX_CLAIMS_BYTES (256u * 1024u)
#define LONEJSON__JWT_MAX_RSA_COMPONENT_BYTES (8u * 1024u)
#define LONEJSON__OIDC_DEFAULT_MAX_JWKS_BYTES (1024u * 1024u)

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

int lonejson_oidc_jwks_cache_is_fresh(
    const lonejson_oidc_jwks_cache *cache,
    const lonejson_oidc_jwks_cache_policy *policy) {
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
    const lonejson_jwt_compact *jwt, const lonejson_jwk *jwk,
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
      EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PADDING) <= 0) {
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

lonejson_status lonejson_jwt_validate_signature(
    const lonejson_jwt_compact *jwt, const lonejson_jwt_header *header,
    const lonejson_jwk *jwk, lonejson_error *error) {
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
  if (strcmp(header->alg, "RS256") != 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT signature algorithm is not supported");
  }
  if (!lonejson__auth_streq(jwk->kty, "RSA")) {
    return lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
                               "JWT JWK key type does not match algorithm");
  }
  return lonejson__jwt_validate_rs256_signature(jwt, jwk, error);
}
#endif
