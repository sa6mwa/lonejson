#ifdef LONEJSON_WITH_JWT
static void ljlua_auth_push_optional_string(lua_State *L, const char *value,
                                            const char *name) {
  if (value != NULL) {
    lua_pushstring(L, value);
    lua_setfield(L, -2, name);
  }
}

static void ljlua_auth_push_jwk(lua_State *L, const lonejson_jwk *jwk) {
  lua_newtable(L);
  ljlua_auth_push_optional_string(L, jwk->kty, "kty");
  ljlua_auth_push_optional_string(L, jwk->kid, "kid");
  ljlua_auth_push_optional_string(L, jwk->alg, "alg");
  ljlua_auth_push_optional_string(L, jwk->use, "use");
  ljlua_auth_push_optional_string(L, jwk->crv, "crv");
  ljlua_auth_push_optional_string(L, jwk->n, "n");
  ljlua_auth_push_optional_string(L, jwk->e, "e");
  ljlua_auth_push_optional_string(L, jwk->x, "x");
  ljlua_auth_push_optional_string(L, jwk->y, "y");
  ljlua_auth_push_optional_string(L, jwk->k, "k");
}

static void ljlua_auth_push_jwks(lua_State *L, const lonejson_jwks *jwks) {
  size_t i;
  const lonejson_jwk *items = (const lonejson_jwk *)jwks->keys.items;

  lua_newtable(L);
  lua_createtable(L, (int)jwks->keys.count, 0);
  for (i = 0u; i < jwks->keys.count; ++i) {
    ljlua_auth_push_jwk(L, &items[i]);
    lua_rawseti(L, -2, (lua_Integer)i + 1);
  }
  lua_setfield(L, -2, "keys");
}

static void ljlua_auth_push_header(lua_State *L,
                                   const lonejson_jwt_header *header) {
  lua_newtable(L);
  ljlua_auth_push_optional_string(L, header->alg, "alg");
  ljlua_auth_push_optional_string(L, header->kid, "kid");
  ljlua_auth_push_optional_string(L, header->typ, "typ");
}

static void ljlua_auth_push_claims(lua_State *L,
                                   const lonejson_jwt_claims *claims) {
  size_t i;

  lua_newtable(L);
  ljlua_auth_push_optional_string(L, claims->iss, "iss");
  ljlua_auth_push_optional_string(L, claims->sub, "sub");
  ljlua_auth_push_optional_string(L, claims->aud, "aud");
  if (claims->aud_array.count != 0u) {
    lua_createtable(L, (int)claims->aud_array.count, 0);
    for (i = 0u; i < claims->aud_array.count; ++i) {
      lua_pushstring(L, claims->aud_array.items[i]);
      lua_rawseti(L, -2, (lua_Integer)i + 1);
    }
    lua_setfield(L, -2, "aud");
  }
  if (claims->has_exp) {
    lua_pushinteger(L, (lua_Integer)claims->exp);
    lua_setfield(L, -2, "exp");
  }
  if (claims->has_nbf) {
    lua_pushinteger(L, (lua_Integer)claims->nbf);
    lua_setfield(L, -2, "nbf");
  }
  if (claims->has_iat) {
    lua_pushinteger(L, (lua_Integer)claims->iat);
    lua_setfield(L, -2, "iat");
  }
}

#ifdef LONEJSON_WITH_OIDC
static void
ljlua_auth_push_oidc_discovery(lua_State *L,
                               const lonejson_oidc_discovery *discovery) {
  lua_createtable(L, 0, 4);
  ljlua_auth_push_optional_string(L, discovery->issuer, "issuer");
  ljlua_auth_push_optional_string(L, discovery->authorization_endpoint,
                                  "authorization_endpoint");
  ljlua_auth_push_optional_string(L, discovery->token_endpoint,
                                  "token_endpoint");
  ljlua_auth_push_optional_string(L, discovery->jwks_uri, "jwks_uri");
}
#endif

static lonejson *ljlua_auth_runtime_arg(lua_State *L, int *arg,
                                        lonejson **owned_runtime,
                                        lonejson_error *error) {
  lonejson *runtime = NULL;

  *owned_runtime = NULL;
  *arg = 1;
  if (lua_gettop(L) >= 1 && luaL_testudata(L, 1, LJLUA_RUNTIME_MT) != NULL) {
    runtime = ljlua_check_runtime(L, 1)->runtime;
    *arg = 2;
  }
  return ljlua_ensure_visit_runtime(L, runtime, owned_runtime, error);
}

static int ljlua_auth_read_string_list(lua_State *L, int table_index,
                                       const char *field,
                                       const char *const **out_items,
                                       size_t *out_count, int required) {
  size_t i;
  size_t count;
  const char **items;

  *out_items = NULL;
  *out_count = 0u;
  lua_getfield(L, table_index, field);
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
    if (required) {
      return luaL_error(L, "policy.%s is required", field);
    }
    return 0;
  }
  luaL_checktype(L, -1, LUA_TTABLE);
  count = (size_t)lua_rawlen(L, -1);
  if (count == 0u && required) {
    lua_pop(L, 1);
    return luaL_error(L, "policy.%s must not be empty", field);
  }
  items = (const char **)calloc(count == 0u ? 1u : count, sizeof(items[0]));
  if (items == NULL) {
    lua_pop(L, 1);
    return luaL_error(L, "failed to allocate policy.%s", field);
  }
  for (i = 0u; i < count; ++i) {
    lua_rawgeti(L, -1, (lua_Integer)i + 1);
    items[i] = luaL_checkstring(L, -1);
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
  *out_items = items;
  *out_count = count;
  return 0;
}

static void ljlua_auth_free_policy(lonejson_jwt_claim_policy *policy) {
  free((void *)policy->accepted_algs);
  free((void *)policy->accepted_issuers);
  free((void *)policy->accepted_audiences);
  free((void *)policy->required_claims);
}

static int ljlua_auth_read_policy(lua_State *L, int index,
                                  lonejson_jwt_claim_policy *policy,
                                  int require_trust_policy) {
  memset(policy, 0, sizeof(*policy));
  if (lua_isnoneornil(L, index)) {
    if (require_trust_policy) {
      return luaL_error(L, "JWT claim policy is required");
    }
    return 0;
  }
  luaL_checktype(L, index, LUA_TTABLE);
  index = lua_absindex(L, index);
  if (require_trust_policy) {
    ljlua_auth_read_string_list(L, index, "accepted_algs",
                                &policy->accepted_algs,
                                &policy->accepted_alg_count, 1);
    ljlua_auth_read_string_list(L, index, "accepted_issuers",
                                &policy->accepted_issuers,
                                &policy->accepted_issuer_count, 1);
    ljlua_auth_read_string_list(L, index, "accepted_audiences",
                                &policy->accepted_audiences,
                                &policy->accepted_audience_count, 1);
    ljlua_auth_read_string_list(L, index, "required_claims",
                                &policy->required_claims,
                                &policy->required_claim_count, 0);
    lua_getfield(L, index, "now");
    policy->now = (lonejson_int64)luaL_checkinteger(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, index, "allowed_clock_skew");
    policy->allowed_clock_skew =
        lua_isnil(L, -1) ? 0 : (lonejson_int64)luaL_checkinteger(L, -1);
    lua_pop(L, 1);
  }
  lua_getfield(L, index, "max_token_bytes");
  policy->max_token_bytes =
      lua_isnil(L, -1) ? 0u : (size_t)luaL_checkinteger(L, -1);
  lua_pop(L, 1);
  lua_getfield(L, index, "max_decoded_header_bytes");
  policy->max_decoded_header_bytes =
      lua_isnil(L, -1) ? 0u : (size_t)luaL_checkinteger(L, -1);
  lua_pop(L, 1);
  lua_getfield(L, index, "max_decoded_claims_bytes");
  policy->max_decoded_claims_bytes =
      lua_isnil(L, -1) ? 0u : (size_t)luaL_checkinteger(L, -1);
  lua_pop(L, 1);
  return 0;
}

static void ljlua_auth_push_decoded_jwt(lua_State *L,
                                        const lonejson_jwt_header *header,
                                        const lonejson_jwt_claims *claims) {
  lua_newtable(L);
  ljlua_auth_push_header(L, header);
  lua_setfield(L, -2, "header");
  ljlua_auth_push_claims(L, claims);
  lua_setfield(L, -2, "claims");
}

static int ljlua_jwt_parse_compact(lua_State *L) {
  const char *token;
  size_t len;
  int arg = 1;
  lonejson_jwt_compact jwt;
  lonejson_error error;
  lonejson_status status;

  if (lua_gettop(L) >= 1 && luaL_testudata(L, 1, LJLUA_RUNTIME_MT) != NULL) {
    (void)ljlua_check_runtime(L, 1);
    arg = 2;
  }
  token = luaL_checklstring(L, arg, &len);
  status = lonejson_jwt_parse_compact(token, len, &jwt, &error);
  if (status != LONEJSON_STATUS_OK) {
    return ljlua_push_status_result(L, status, &error);
  }
  lua_newtable(L);
  lua_pushlstring(L, jwt.header.data, jwt.header.len);
  lua_setfield(L, -2, "header");
  lua_pushlstring(L, jwt.payload.data, jwt.payload.len);
  lua_setfield(L, -2, "payload");
  lua_pushlstring(L, jwt.signature.data, jwt.signature.len);
  lua_setfield(L, -2, "signature");
  lua_pushlstring(L, jwt.signing_input.data, jwt.signing_input.len);
  lua_setfield(L, -2, "signing_input");
  return 1;
}

static int ljlua_base64url_decode(lua_State *L) {
  const char *data;
  size_t len;
  size_t needed;
  unsigned char *out;
  lonejson_error error;
  lonejson_status status;

  data = luaL_checklstring(L, 1, &len);
  status = lonejson_base64url_decoded_len(data, len, &needed, &error);
  if (status != LONEJSON_STATUS_OK) {
    return ljlua_push_status_result(L, status, &error);
  }
  out = (unsigned char *)malloc(needed == 0u ? 1u : needed);
  if (out == NULL) {
    return luaL_error(L, "failed to allocate base64url output");
  }
  status = lonejson_base64url_decode(data, len, out, needed, &needed, &error);
  if (status != LONEJSON_STATUS_OK) {
    free(out);
    return ljlua_push_status_result(L, status, &error);
  }
  lua_pushlstring(L, (const char *)out, needed);
  free(out);
  return 1;
}

static int ljlua_jwk_parse_json(lua_State *L) {
  lonejson *runtime;
  lonejson *owned_runtime;
  lonejson_jwk jwk;
  lonejson_error error;
  lonejson_status status;
  const char *json;
  size_t len;
  int arg;

  runtime = ljlua_auth_runtime_arg(L, &arg, &owned_runtime, &error);
  if (runtime == NULL) {
    return ljlua_push_status_result(L, LONEJSON_STATUS_INVALID_ARGUMENT,
                                    &error);
  }
  json = luaL_checklstring(L, arg, &len);
  lonejson_jwk_init(&jwk);
  status = lonejson_jwk_parse_json(runtime, json, len, &jwk, &error);
  if (owned_runtime != NULL) {
    lonejson_free(owned_runtime);
  }
  if (status != LONEJSON_STATUS_OK) {
    lonejson_jwk_cleanup(&jwk);
    return ljlua_push_status_result(L, status, &error);
  }
  ljlua_auth_push_jwk(L, &jwk);
  lonejson_jwk_cleanup(&jwk);
  return 1;
}

static int ljlua_jwks_parse_json(lua_State *L) {
  lonejson *runtime;
  lonejson *owned_runtime;
  lonejson_jwks jwks;
  lonejson_error error;
  lonejson_status status;
  const char *json;
  size_t len;
  int arg;

  runtime = ljlua_auth_runtime_arg(L, &arg, &owned_runtime, &error);
  if (runtime == NULL) {
    return ljlua_push_status_result(L, LONEJSON_STATUS_INVALID_ARGUMENT,
                                    &error);
  }
  json = luaL_checklstring(L, arg, &len);
  lonejson_jwks_init(&jwks);
  status = lonejson_jwks_parse_json(runtime, json, len, &jwks, &error);
  if (owned_runtime != NULL) {
    lonejson_free(owned_runtime);
  }
  if (status != LONEJSON_STATUS_OK) {
    lonejson_jwks_cleanup(&jwks);
    return ljlua_push_status_result(L, status, &error);
  }
  ljlua_auth_push_jwks(L, &jwks);
  lonejson_jwks_cleanup(&jwks);
  return 1;
}

static void ljlua_auth_read_jwk_select(lua_State *L, int index,
                                       lonejson_jwk_select_options *options) {
  memset(options, 0, sizeof(*options));
  if (lua_isnoneornil(L, index)) {
    return;
  }
  luaL_checktype(L, index, LUA_TTABLE);
  index = lua_absindex(L, index);
  lua_getfield(L, index, "kid");
  options->kid = lua_isnil(L, -1) ? NULL : luaL_checkstring(L, -1);
  lua_getfield(L, index, "kty");
  options->kty = lua_isnil(L, -1) ? NULL : luaL_checkstring(L, -1);
  lua_getfield(L, index, "alg");
  options->alg = lua_isnil(L, -1) ? NULL : luaL_checkstring(L, -1);
  lua_getfield(L, index, "use");
  options->use = lua_isnil(L, -1) ? NULL : luaL_checkstring(L, -1);
  lua_pop(L, 4);
}

static int ljlua_jwks_select_json(lua_State *L) {
  lonejson *runtime;
  lonejson *owned_runtime;
  lonejson_jwks jwks;
  lonejson_jwk_select_options options;
  const lonejson_jwk *selected;
  lonejson_error error;
  lonejson_status status;
  const char *json;
  size_t len;
  int arg;

  runtime = ljlua_auth_runtime_arg(L, &arg, &owned_runtime, &error);
  if (runtime == NULL) {
    return ljlua_push_status_result(L, LONEJSON_STATUS_INVALID_ARGUMENT,
                                    &error);
  }
  json = luaL_checklstring(L, arg, &len);
  ljlua_auth_read_jwk_select(L, arg + 1, &options);
  lonejson_jwks_init(&jwks);
  status = lonejson_jwks_parse_json(runtime, json, len, &jwks, &error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_jwks_select(&jwks, &options, &selected, &error);
  }
  if (owned_runtime != NULL) {
    lonejson_free(owned_runtime);
  }
  if (status != LONEJSON_STATUS_OK) {
    lonejson_jwks_cleanup(&jwks);
    return ljlua_push_status_result(L, status, &error);
  }
  if (selected == NULL) {
    lonejson_jwks_cleanup(&jwks);
    lua_pushnil(L);
    return 1;
  }
  ljlua_auth_push_jwk(L, selected);
  lonejson_jwks_cleanup(&jwks);
  return 1;
}

#ifdef LONEJSON_WITH_OIDC
static int ljlua_oidc_discovery_url(lua_State *L) {
  lonejson_owned_buffer out;
  lonejson_error error;
  lonejson_status status;
  const char *issuer;

  issuer = luaL_checkstring(L, 1);
  lonejson_error_init(&error);
  lonejson_owned_buffer_init(&out);
  status = lonejson_oidc_discovery_url(issuer, &out, &error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_owned_buffer_free(&out);
    return ljlua_push_status_result(L, status, &error);
  }
  lua_pushlstring(L, out.data, out.len);
  lonejson_owned_buffer_free(&out);
  return 1;
}

static int ljlua_oidc_discovery_parse_json(lua_State *L) {
  lonejson *runtime;
  lonejson *owned_runtime;
  lonejson_oidc_discovery discovery;
  lonejson_error error;
  lonejson_status status;
  const char *json;
  const char *expected_issuer;
  size_t len;
  int arg;

  runtime = ljlua_auth_runtime_arg(L, &arg, &owned_runtime, &error);
  if (runtime == NULL) {
    return ljlua_push_status_result(L, LONEJSON_STATUS_INVALID_ARGUMENT,
                                    &error);
  }
  json = luaL_checklstring(L, arg, &len);
  expected_issuer =
      lua_isnoneornil(L, arg + 1) ? NULL : luaL_checkstring(L, arg + 1);
  lonejson_oidc_discovery_init(&discovery);
  status = lonejson_oidc_discovery_parse_json(runtime, json, len, &discovery,
                                              &error);
  if (status == LONEJSON_STATUS_OK && expected_issuer != NULL) {
    status = lonejson_oidc_discovery_validate_issuer(&discovery,
                                                     expected_issuer, &error);
  }
  if (owned_runtime != NULL) {
    lonejson_free(owned_runtime);
  }
  if (status != LONEJSON_STATUS_OK) {
    lonejson_oidc_discovery_cleanup(&discovery);
    return ljlua_push_status_result(L, status, &error);
  }
  ljlua_auth_push_oidc_discovery(L, &discovery);
  lonejson_oidc_discovery_cleanup(&discovery);
  return 1;
}
#endif

static int ljlua_jwt_decode_compact(lua_State *L) {
  lonejson *runtime;
  lonejson *owned_runtime;
  lonejson_jwt_header header;
  lonejson_jwt_claims claims;
  lonejson_jwt_claim_policy limits;
  lonejson_error error;
  lonejson_status status;
  const char *token;
  size_t len;
  int arg;

  runtime = ljlua_auth_runtime_arg(L, &arg, &owned_runtime, &error);
  if (runtime == NULL) {
    return ljlua_push_status_result(L, LONEJSON_STATUS_INVALID_ARGUMENT,
                                    &error);
  }
  token = luaL_checklstring(L, arg, &len);
  ljlua_auth_read_policy(L, arg + 1, &limits, 0);
  lonejson_jwt_header_init(&header);
  lonejson_jwt_claims_init(&claims);
  status = lonejson_jwt_decode_compact(runtime, token, len, &limits, &header,
                                       &claims, &error);
  if (owned_runtime != NULL) {
    lonejson_free(owned_runtime);
  }
  ljlua_auth_free_policy(&limits);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_jwt_header_cleanup(&header);
    lonejson_jwt_claims_cleanup(&claims);
    return ljlua_push_status_result(L, status, &error);
  }
  ljlua_auth_push_decoded_jwt(L, &header, &claims);
  lonejson_jwt_header_cleanup(&header);
  lonejson_jwt_claims_cleanup(&claims);
  return 1;
}

static int ljlua_jwt_validate_compact_claims(lua_State *L) {
  lonejson *runtime;
  lonejson *owned_runtime;
  lonejson_jwt_header header;
  lonejson_jwt_claims claims;
  lonejson_jwt_claim_policy policy;
  lonejson_error error;
  lonejson_status status;
  const char *token;
  size_t len;
  int arg;

  runtime = ljlua_auth_runtime_arg(L, &arg, &owned_runtime, &error);
  if (runtime == NULL) {
    return ljlua_push_status_result(L, LONEJSON_STATUS_INVALID_ARGUMENT,
                                    &error);
  }
  token = luaL_checklstring(L, arg, &len);
  ljlua_auth_read_policy(L, arg + 1, &policy, 1);
  lonejson_jwt_header_init(&header);
  lonejson_jwt_claims_init(&claims);
  status = lonejson_jwt_decode_compact(runtime, token, len, &policy, &header,
                                       &claims, &error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_jwt_validate_claims(&header, &claims, &policy, &error);
  }
  if (owned_runtime != NULL) {
    lonejson_free(owned_runtime);
  }
  ljlua_auth_free_policy(&policy);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_jwt_header_cleanup(&header);
    lonejson_jwt_claims_cleanup(&claims);
    return ljlua_push_status_result(L, status, &error);
  }
  ljlua_auth_push_decoded_jwt(L, &header, &claims);
  lonejson_jwt_header_cleanup(&header);
  lonejson_jwt_claims_cleanup(&claims);
  return 1;
}
#endif
