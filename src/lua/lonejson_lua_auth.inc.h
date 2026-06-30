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
  ljlua_auth_push_optional_string(L, claims->nonce, "nonce");
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
static void ljlua_auth_push_http_request(lua_State *L,
                                         const lonejson_http_request *request) {
  lua_createtable(L, 0, 7);
  ljlua_auth_push_optional_string(L, request->method, "method");
  ljlua_auth_push_optional_string(L, request->url, "url");
  ljlua_auth_push_optional_string(L, request->content_type, "content_type");
  ljlua_auth_push_optional_string(L, request->user_agent, "user_agent");
  if (request->body != NULL || request->body_len != 0u) {
    lua_pushlstring(L, (const char *)request->body, request->body_len);
    lua_setfield(L, -2, "body");
  }
  lua_pushinteger(L, (lua_Integer)request->body_len);
  lua_setfield(L, -2, "body_len");
  lua_pushinteger(L, (lua_Integer)request->max_response_bytes);
  lua_setfield(L, -2, "max_response_bytes");
}

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

static void ljlua_auth_push_oauth2_token_response(
    lua_State *L, const lonejson_oauth2_token_response *response) {
  lua_createtable(L, 0, 5);
  ljlua_auth_push_optional_string(L, response->access_token, "access_token");
  ljlua_auth_push_optional_string(L, response->token_type, "token_type");
  ljlua_auth_push_optional_string(L, response->refresh_token, "refresh_token");
  ljlua_auth_push_optional_string(L, response->scope, "scope");
  ljlua_auth_push_optional_string(L, response->id_token, "id_token");
  if (response->has_expires_in) {
    lua_pushinteger(L, (lua_Integer)response->expires_in);
    lua_setfield(L, -2, "expires_in");
  }
}

static void ljlua_auth_push_jwks_cache(lua_State *L,
                                       const lonejson_oidc_jwks_cache *cache) {
  lua_createtable(L, 0, 5);
  ljlua_auth_push_optional_string(L, cache->issuer, "issuer");
  ljlua_auth_push_optional_string(L, cache->jwks_uri, "jwks_uri");
  lua_pushinteger(L, (lua_Integer)cache->fetched_at);
  lua_setfield(L, -2, "fetched_at");
  lua_pushinteger(L, (lua_Integer)cache->expires_at);
  lua_setfield(L, -2, "expires_at");
  ljlua_auth_push_jwks(L, &cache->jwks);
  lua_setfield(L, -2, "jwks");
}

static void ljlua_auth_push_oidc_pkce(lua_State *L,
                                      const lonejson_oidc_pkce *pkce) {
  lua_createtable(L, 0, 2);
  ljlua_auth_push_optional_string(L, pkce->code_verifier, "code_verifier");
  ljlua_auth_push_optional_string(L, pkce->code_challenge, "code_challenge");
}

static void ljlua_auth_push_oidc_callback(
    lua_State *L, const lonejson_oidc_authorization_callback *callback) {
  lua_createtable(L, 0, 2);
  ljlua_auth_push_optional_string(L, callback->code, "code");
  ljlua_auth_push_optional_string(L, callback->state, "state");
}

static void ljlua_auth_push_bearer_validation(
    lua_State *L, const lonejson_oidc_bearer_validation *validation) {
  lua_createtable(L, 0, 5);
  lua_pushboolean(L, validation->failure == LONEJSON_AUTH_FAILURE_NONE);
  lua_setfield(L, -2, "authorized");
  lua_pushstring(L, lonejson_auth_failure_string(validation->failure));
  lua_setfield(L, -2, "failure");
  ljlua_auth_push_header(L, &validation->header);
  lua_setfield(L, -2, "header");
  ljlua_auth_push_claims(L, &validation->claims);
  lua_setfield(L, -2, "claims");
  if (validation->jwk != NULL) {
    ljlua_auth_push_jwk(L, validation->jwk);
    lua_setfield(L, -2, "jwk");
  }
}

static const char *ljlua_auth_optional_table_string(lua_State *L, int index,
                                                    const char *field) {
  const char *value;

  lua_getfield(L, index, field);
  value = lua_isnil(L, -1) ? NULL : luaL_checkstring(L, -1);
  lua_pop(L, 1);
  return value;
}

static void ljlua_auth_read_client_credentials(
    lua_State *L, int index, lonejson_oauth2_client_credentials *request) {
  lua_Integer max_body_bytes;

  memset(request, 0, sizeof(*request));
  luaL_checktype(L, index, LUA_TTABLE);
  index = lua_absindex(L, index);
  lua_getfield(L, index, "client_id");
  request->client_id = luaL_checkstring(L, -1);
  lua_pop(L, 1);
  lua_getfield(L, index, "client_secret");
  request->client_secret = luaL_checkstring(L, -1);
  lua_pop(L, 1);
  request->scope = ljlua_auth_optional_table_string(L, index, "scope");
  request->audience = ljlua_auth_optional_table_string(L, index, "audience");
  request->resource = ljlua_auth_optional_table_string(L, index, "resource");
  lua_getfield(L, index, "max_body_bytes");
  if (!lua_isnil(L, -1)) {
    max_body_bytes = luaL_checkinteger(L, -1);
    luaL_argcheck(L, max_body_bytes >= 0, index,
                  "max_body_bytes must be non-negative");
    request->max_body_bytes = (size_t)max_body_bytes;
  }
  lua_pop(L, 1);
}

static void ljlua_auth_read_refresh_token(
    lua_State *L, int index, lonejson_oauth2_refresh_token *request) {
  lua_Integer max_body_bytes;

  memset(request, 0, sizeof(*request));
  luaL_checktype(L, index, LUA_TTABLE);
  index = lua_absindex(L, index);
  lua_getfield(L, index, "refresh_token");
  request->refresh_token = luaL_checkstring(L, -1);
  lua_pop(L, 1);
  request->client_id = ljlua_auth_optional_table_string(L, index, "client_id");
  request->client_secret =
      ljlua_auth_optional_table_string(L, index, "client_secret");
  request->scope = ljlua_auth_optional_table_string(L, index, "scope");
  lua_getfield(L, index, "max_body_bytes");
  if (!lua_isnil(L, -1)) {
    max_body_bytes = luaL_checkinteger(L, -1);
    luaL_argcheck(L, max_body_bytes >= 0, index,
                  "max_body_bytes must be non-negative");
    request->max_body_bytes = (size_t)max_body_bytes;
  }
  lua_pop(L, 1);
}

static void ljlua_auth_read_authorization_code_token(
    lua_State *L, int index, lonejson_oidc_authorization_code_token *request) {
  lua_Integer max_body_bytes;

  memset(request, 0, sizeof(*request));
  luaL_checktype(L, index, LUA_TTABLE);
  index = lua_absindex(L, index);
  lua_getfield(L, index, "client_id");
  request->client_id = luaL_checkstring(L, -1);
  lua_pop(L, 1);
  lua_getfield(L, index, "code");
  request->code = luaL_checkstring(L, -1);
  lua_pop(L, 1);
  lua_getfield(L, index, "redirect_uri");
  request->redirect_uri = luaL_checkstring(L, -1);
  lua_pop(L, 1);
  lua_getfield(L, index, "code_verifier");
  request->code_verifier = luaL_checkstring(L, -1);
  lua_pop(L, 1);
  request->client_secret =
      ljlua_auth_optional_table_string(L, index, "client_secret");
  lua_getfield(L, index, "max_body_bytes");
  if (!lua_isnil(L, -1)) {
    max_body_bytes = luaL_checkinteger(L, -1);
    luaL_argcheck(L, max_body_bytes >= 0, index,
                  "max_body_bytes must be non-negative");
    request->max_body_bytes = (size_t)max_body_bytes;
  }
  lua_pop(L, 1);
}

static void ljlua_auth_read_authorization_request(
    lua_State *L, int index, lonejson_oidc_authorization_request *request) {
  lua_Integer max_url_bytes;

  memset(request, 0, sizeof(*request));
  luaL_checktype(L, index, LUA_TTABLE);
  index = lua_absindex(L, index);
  lua_getfield(L, index, "authorization_endpoint");
  request->authorization_endpoint = luaL_checkstring(L, -1);
  lua_pop(L, 1);
  lua_getfield(L, index, "client_id");
  request->client_id = luaL_checkstring(L, -1);
  lua_pop(L, 1);
  lua_getfield(L, index, "redirect_uri");
  request->redirect_uri = luaL_checkstring(L, -1);
  lua_pop(L, 1);
  lua_getfield(L, index, "state");
  request->state = luaL_checkstring(L, -1);
  lua_pop(L, 1);
  lua_getfield(L, index, "nonce");
  request->nonce = luaL_checkstring(L, -1);
  lua_pop(L, 1);
  lua_getfield(L, index, "code_challenge");
  request->code_challenge = luaL_checkstring(L, -1);
  lua_pop(L, 1);
  request->scope = ljlua_auth_optional_table_string(L, index, "scope");
  request->audience = ljlua_auth_optional_table_string(L, index, "audience");
  request->resource = ljlua_auth_optional_table_string(L, index, "resource");
  lua_getfield(L, index, "max_url_bytes");
  if (!lua_isnil(L, -1)) {
    max_url_bytes = luaL_checkinteger(L, -1);
    luaL_argcheck(L, max_url_bytes >= 0, index,
                  "max_url_bytes must be non-negative");
    request->max_url_bytes = (size_t)max_url_bytes;
  }
  lua_pop(L, 1);
}

static void ljlua_auth_read_jwks_cache_policy(
    lua_State *L, int index, lonejson_oidc_jwks_cache_policy *policy) {
  memset(policy, 0, sizeof(*policy));
  luaL_checktype(L, index, LUA_TTABLE);
  index = lua_absindex(L, index);
  lua_getfield(L, index, "issuer");
  policy->issuer = luaL_checkstring(L, -1);
  lua_pop(L, 1);
  lua_getfield(L, index, "jwks_uri");
  policy->jwks_uri = luaL_checkstring(L, -1);
  lua_pop(L, 1);
  lua_getfield(L, index, "now");
  policy->now = (lonejson_int64)luaL_checkinteger(L, -1);
  lua_pop(L, 1);
  lua_getfield(L, index, "ttl_seconds");
  policy->ttl_seconds = (lonejson_int64)luaL_checkinteger(L, -1);
  lua_pop(L, 1);
  lua_getfield(L, index, "max_jwks_bytes");
  policy->max_jwks_bytes =
      lua_isnil(L, -1) ? 0u : (size_t)luaL_checkinteger(L, -1);
  lua_pop(L, 1);
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

#if defined(LONEJSON_WITH_OPENSSL)
static int ljlua_set_openssl_auth_provider(lua_State *L) {
  ljlua_runtime_ud *ud = ljlua_check_runtime(L, 1);
  lonejson_error error;
  lonejson_status status;

  lonejson_error_init(&error);
  status = lonejson_auth_provider_init_openssl(&ud->auth_provider, NULL,
                                               &error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_set_auth_provider(ud->runtime, &ud->auth_provider,
                                        &error);
  }
  if (status != LONEJSON_STATUS_OK) {
    return ljlua_push_status_result(L, status, &error);
  }
  lua_pushboolean(L, 1);
  return 1;
}
#endif

#ifdef LONEJSON_WITH_OIDC
static char *ljlua_auth_strdup(const char *value) {
  size_t len;
  char *copy;

  if (value == NULL) {
    return NULL;
  }
  len = strlen(value);
  copy = (char *)malloc(len + 1u);
  if (copy == NULL) {
    return NULL;
  }
  memcpy(copy, value, len + 1u);
  return copy;
}

static void ljlua_auth_clear_http_provider(lua_State *L, ljlua_runtime_ud *ud) {
  lonejson_error error;

  if (ud == NULL) {
    return;
  }
  if (ud->runtime != NULL) {
    lonejson_error_init(&error);
    (void)lonejson_set_http_provider(ud->runtime, NULL, &error);
  }
  if (ud->http_provider_ref != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, ud->http_provider_ref);
    ud->http_provider_ref = LUA_NOREF;
  }
  free(ud->http_user_agent);
  ud->http_user_agent = NULL;
  memset(&ud->http_provider, 0, sizeof(ud->http_provider));
}

static lonejson_status
ljlua_auth_http_provider_request(void *user_data,
                                 const lonejson_http_request *request,
                                 lonejson_http_response *response,
                                 lonejson_error *error) {
  ljlua_runtime_ud *ud = (ljlua_runtime_ud *)user_data;
  lua_State *L;
  int base;
  int rc;
  int result_index;
  long status_code;
  size_t body_len;
  const char *body;

  if (ud == NULL || ud->L == NULL || ud->http_provider_ref == LUA_NOREF) {
    ljlua_set_callback_error(error, LONEJSON_STATUS_CALLBACK_FAILED,
                             "Lua HTTP provider is not configured");
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  L = ud->L;
  base = lua_gettop(L);
  lua_rawgeti(L, LUA_REGISTRYINDEX, ud->http_provider_ref);
  ljlua_auth_push_http_request(L, request);
  rc = lua_pcall(L, 1, LUA_MULTRET, 0);
  if (rc != LUA_OK) {
    const char *message = lua_tostring(L, -1);
    ljlua_set_callback_error(error, LONEJSON_STATUS_CALLBACK_FAILED,
                             message != NULL ? message
                                             : "Lua HTTP provider failed");
    lua_settop(L, base);
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  if (lua_gettop(L) <= base || lua_isnil(L, base + 1) ||
      (lua_isboolean(L, base + 1) && !lua_toboolean(L, base + 1))) {
    const char *message =
        lua_gettop(L) >= base + 2 ? lua_tostring(L, base + 2) : NULL;
    ljlua_set_callback_error(error, LONEJSON_STATUS_CALLBACK_FAILED,
                             message != NULL ? message
                                             : "Lua HTTP provider failed");
    lua_settop(L, base);
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  result_index = base + 1;
  if (!lua_istable(L, result_index)) {
    ljlua_set_callback_error(error, LONEJSON_STATUS_CALLBACK_FAILED,
                             "Lua HTTP provider response must be a table");
    lua_settop(L, base);
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  lua_getfield(L, result_index, "status_code");
  if (!lua_isnumber(L, -1)) {
    lua_pop(L, 1);
    ljlua_set_callback_error(error, LONEJSON_STATUS_CALLBACK_FAILED,
                             "Lua HTTP provider response.status_code is "
                             "required");
    lua_settop(L, base);
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  status_code = (long)lua_tointeger(L, -1);
  lua_pop(L, 1);
  lua_getfield(L, result_index, "body");
  if (lua_isnil(L, -1)) {
    body = "";
    body_len = 0u;
  } else {
    body = lua_tolstring(L, -1, &body_len);
    if (body == NULL) {
      lua_pop(L, 1);
      ljlua_set_callback_error(error, LONEJSON_STATUS_CALLBACK_FAILED,
                               "Lua HTTP provider response.body must be a "
                               "string");
      lua_settop(L, base);
      return LONEJSON_STATUS_CALLBACK_FAILED;
    }
  }
  if (request != NULL && request->max_response_bytes != 0u &&
      body_len > request->max_response_bytes) {
    lua_pop(L, 1);
    ljlua_set_callback_error(error, LONEJSON_STATUS_OVERFLOW,
                             "Lua HTTP provider response exceeds maximum "
                             "response size");
    lua_settop(L, base);
    return LONEJSON_STATUS_OVERFLOW;
  }
  response->status_code = status_code;
  if (body_len != 0u &&
      lonejson_owned_buffer_sink(&response->body, body, body_len, error) !=
          LONEJSON_STATUS_OK) {
    lua_pop(L, 1);
    lua_settop(L, base);
    return error != NULL ? error->code : LONEJSON_STATUS_ALLOCATION_FAILED;
  }
  lua_pop(L, 1);
  lua_settop(L, base);
  return LONEJSON_STATUS_OK;
}

static int ljlua_set_http_provider(lua_State *L) {
  ljlua_runtime_ud *ud = ljlua_check_runtime(L, 1);
  lonejson_http_provider provider;
  lonejson_error error;
  lonejson_status status;
  const char *user_agent;
  char *owned_user_agent;
  int callback_ref;

  if (lua_isnoneornil(L, 2)) {
    ljlua_auth_clear_http_provider(L, ud);
    lua_pushboolean(L, 1);
    return 1;
  }
  luaL_checktype(L, 2, LUA_TFUNCTION);
  user_agent = lua_isnoneornil(L, 3) ? NULL : luaL_checkstring(L, 3);
  owned_user_agent = NULL;
  if (user_agent != NULL) {
    owned_user_agent = ljlua_auth_strdup(user_agent);
    if (owned_user_agent == NULL) {
      return luaL_error(L, "failed to allocate HTTP provider user_agent");
    }
  }
  lua_pushvalue(L, 2);
  callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  lonejson_error_init(&error);
  status = lonejson_http_provider_init_simple(
      &provider, ud, owned_user_agent, ljlua_auth_http_provider_request,
      &error);
  if (status != LONEJSON_STATUS_OK) {
    luaL_unref(L, LUA_REGISTRYINDEX, callback_ref);
    free(owned_user_agent);
    return ljlua_push_status_result(L, status, &error);
  }
  ljlua_auth_clear_http_provider(L, ud);
  ud->http_provider_ref = callback_ref;
  ud->http_user_agent = owned_user_agent;
  ud->http_provider = provider;
  status = lonejson_set_http_provider(ud->runtime, &ud->http_provider, &error);
  if (status != LONEJSON_STATUS_OK) {
    ljlua_auth_clear_http_provider(L, ud);
    return ljlua_push_status_result(L, status, &error);
  }
  lua_pushboolean(L, 1);
  return 1;
}

static size_t ljlua_auth_optional_max(lua_State *L, int index,
                                      const char *name) {
  lua_Integer value;

  if (lua_isnoneornil(L, index)) {
    return 0u;
  }
  value = luaL_checkinteger(L, index);
  luaL_argcheck(L, value >= 0, index, name);
  return (size_t)value;
}
#endif

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
    lua_getfield(L, index, "expected_nonce");
    policy->expected_nonce =
        lua_isnil(L, -1) ? NULL : luaL_checkstring(L, -1);
    lua_pop(L, 1);
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

static int ljlua_oidc_jwks_cache_select_json(lua_State *L) {
  lonejson *runtime;
  lonejson *owned_runtime;
  lonejson_oidc_jwks_cache cache;
  lonejson_oidc_jwks_cache_policy policy;
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
  ljlua_auth_read_jwks_cache_policy(L, arg + 1, &policy);
  ljlua_auth_read_jwk_select(L, arg + 2, &options);
  lonejson_oidc_jwks_cache_init(&cache);
  status = lonejson_oidc_jwks_cache_update_json(runtime, &cache, &policy, json,
                                                len, &error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_oidc_jwks_cache_select(&cache, &policy, &options,
                                             &selected, &error);
  }
  if (owned_runtime != NULL) {
    lonejson_free(owned_runtime);
  }
  if (status != LONEJSON_STATUS_OK) {
    lonejson_oidc_jwks_cache_cleanup(&cache);
    return ljlua_push_status_result(L, status, &error);
  }
  if (selected == NULL) {
    lonejson_oidc_jwks_cache_cleanup(&cache);
    lua_pushnil(L);
    return 1;
  }
  ljlua_auth_push_jwk(L, selected);
  lonejson_oidc_jwks_cache_cleanup(&cache);
  return 1;
}

static int ljlua_oidc_fetch_discovery(lua_State *L) {
  ljlua_runtime_ud *ud;
  lonejson_oidc_discovery discovery;
  lonejson_error error;
  lonejson_status status;
  const char *issuer;
  size_t max_response_bytes;

  ud = ljlua_check_runtime(L, 1);
  issuer = luaL_checkstring(L, 2);
  max_response_bytes =
      ljlua_auth_optional_max(L, 3, "max_response_bytes must be non-negative");
  lonejson_oidc_discovery_init(&discovery);
  lonejson_error_init(&error);
  status = lonejson_oidc_fetch_discovery(ud->runtime, issuer,
                                         max_response_bytes, &discovery,
                                         &error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_oidc_discovery_cleanup(&discovery);
    return ljlua_push_status_result(L, status, &error);
  }
  ljlua_auth_push_oidc_discovery(L, &discovery);
  lonejson_oidc_discovery_cleanup(&discovery);
  return 1;
}

static int ljlua_oidc_jwks_cache_refresh(lua_State *L) {
  ljlua_runtime_ud *ud;
  lonejson_oidc_jwks_cache cache;
  lonejson_oidc_jwks_cache_policy policy;
  lonejson_error error;
  lonejson_status status;

  ud = ljlua_check_runtime(L, 1);
  ljlua_auth_read_jwks_cache_policy(L, 2, &policy);
  lonejson_oidc_jwks_cache_init(&cache);
  lonejson_error_init(&error);
  status = lonejson_oidc_jwks_cache_refresh(ud->runtime, &cache, &policy,
                                            &error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_oidc_jwks_cache_cleanup(&cache);
    return ljlua_push_status_result(L, status, &error);
  }
  ljlua_auth_push_jwks_cache(L, &cache);
  lonejson_oidc_jwks_cache_cleanup(&cache);
  return 1;
}

static int ljlua_oauth2_client_credentials_body(lua_State *L) {
  lonejson_oauth2_client_credentials request;
  lonejson_owned_buffer out;
  lonejson_error error;
  lonejson_status status;
  int arg = 1;

  if (lua_gettop(L) >= 1 && luaL_testudata(L, 1, LJLUA_RUNTIME_MT) != NULL) {
    arg = 2;
  }
  ljlua_auth_read_client_credentials(L, arg, &request);
  lonejson_owned_buffer_init(&out);
  lonejson_error_init(&error);
  status = lonejson_oauth2_client_credentials_body(&request, &out, &error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_owned_buffer_free(&out);
    return ljlua_push_status_result(L, status, &error);
  }
  lua_pushlstring(L, out.data != NULL ? out.data : "", out.len);
  lonejson_owned_buffer_free(&out);
  return 1;
}

static int ljlua_oauth2_refresh_token_body(lua_State *L) {
  lonejson_oauth2_refresh_token request;
  lonejson_owned_buffer out;
  lonejson_error error;
  lonejson_status status;
  int arg = 1;

  if (lua_gettop(L) >= 1 && luaL_testudata(L, 1, LJLUA_RUNTIME_MT) != NULL) {
    arg = 2;
  }
  ljlua_auth_read_refresh_token(L, arg, &request);
  lonejson_owned_buffer_init(&out);
  lonejson_error_init(&error);
  status = lonejson_oauth2_refresh_token_body(&request, &out, &error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_owned_buffer_free(&out);
    return ljlua_push_status_result(L, status, &error);
  }
  lua_pushlstring(L, out.data != NULL ? out.data : "", out.len);
  lonejson_owned_buffer_free(&out);
  return 1;
}

static int ljlua_oidc_authorization_code_token_body(lua_State *L) {
  lonejson_oidc_authorization_code_token request;
  lonejson_owned_buffer out;
  lonejson_error error;
  lonejson_status status;
  int arg = 1;

  if (lua_gettop(L) >= 1 && luaL_testudata(L, 1, LJLUA_RUNTIME_MT) != NULL) {
    arg = 2;
  }
  ljlua_auth_read_authorization_code_token(L, arg, &request);
  lonejson_owned_buffer_init(&out);
  lonejson_error_init(&error);
  status = lonejson_oidc_authorization_code_token_body(&request, &out, &error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_owned_buffer_free(&out);
    return ljlua_push_status_result(L, status, &error);
  }
  lua_pushlstring(L, out.data != NULL ? out.data : "", out.len);
  lonejson_owned_buffer_free(&out);
  return 1;
}

static int ljlua_oauth2_client_credentials_request(lua_State *L) {
  ljlua_runtime_ud *ud;
  lonejson_oauth2_client_credentials request;
  lonejson_oauth2_token_response response;
  lonejson_error error;
  lonejson_status status;
  const char *token_endpoint;
  size_t max_response_bytes;

  ud = ljlua_check_runtime(L, 1);
  token_endpoint = luaL_checkstring(L, 2);
  ljlua_auth_read_client_credentials(L, 3, &request);
  max_response_bytes =
      ljlua_auth_optional_max(L, 4, "max_response_bytes must be non-negative");
  lonejson_oauth2_token_response_init(&response);
  lonejson_error_init(&error);
  status = lonejson_oauth2_client_credentials_request(
      ud->runtime, token_endpoint, &request, max_response_bytes, &response,
      &error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_oauth2_token_response_cleanup(&response);
    return ljlua_push_status_result(L, status, &error);
  }
  ljlua_auth_push_oauth2_token_response(L, &response);
  lonejson_oauth2_token_response_cleanup(&response);
  return 1;
}

static int ljlua_oauth2_refresh_token_request(lua_State *L) {
  ljlua_runtime_ud *ud;
  lonejson_oauth2_refresh_token request;
  lonejson_oauth2_token_response response;
  lonejson_error error;
  lonejson_status status;
  const char *token_endpoint;
  size_t max_response_bytes;

  ud = ljlua_check_runtime(L, 1);
  token_endpoint = luaL_checkstring(L, 2);
  ljlua_auth_read_refresh_token(L, 3, &request);
  max_response_bytes =
      ljlua_auth_optional_max(L, 4, "max_response_bytes must be non-negative");
  lonejson_oauth2_token_response_init(&response);
  lonejson_error_init(&error);
  status = lonejson_oauth2_refresh_token_request(
      ud->runtime, token_endpoint, &request, max_response_bytes, &response,
      &error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_oauth2_token_response_cleanup(&response);
    return ljlua_push_status_result(L, status, &error);
  }
  ljlua_auth_push_oauth2_token_response(L, &response);
  lonejson_oauth2_token_response_cleanup(&response);
  return 1;
}

static int ljlua_oidc_authorization_code_token_request(lua_State *L) {
  ljlua_runtime_ud *ud;
  lonejson_oidc_authorization_code_token request;
  lonejson_oauth2_token_response response;
  lonejson_error error;
  lonejson_status status;
  const char *token_endpoint;
  size_t max_response_bytes;

  ud = ljlua_check_runtime(L, 1);
  token_endpoint = luaL_checkstring(L, 2);
  ljlua_auth_read_authorization_code_token(L, 3, &request);
  max_response_bytes =
      ljlua_auth_optional_max(L, 4, "max_response_bytes must be non-negative");
  lonejson_oauth2_token_response_init(&response);
  lonejson_error_init(&error);
  status = lonejson_oidc_authorization_code_token_request(
      ud->runtime, token_endpoint, &request, max_response_bytes, &response,
      &error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_oauth2_token_response_cleanup(&response);
    return ljlua_push_status_result(L, status, &error);
  }
  ljlua_auth_push_oauth2_token_response(L, &response);
  lonejson_oauth2_token_response_cleanup(&response);
  return 1;
}

static int ljlua_oauth2_token_response_parse_json(lua_State *L) {
  lonejson *runtime;
  lonejson *owned_runtime;
  lonejson_oauth2_token_response response;
  lonejson_error error;
  lonejson_status status;
  lua_Integer max_response_bytes;
  const char *json;
  size_t len;
  size_t max_bytes = 0u;
  int arg;

  runtime = ljlua_auth_runtime_arg(L, &arg, &owned_runtime, &error);
  if (runtime == NULL) {
    return ljlua_push_status_result(L, LONEJSON_STATUS_INVALID_ARGUMENT,
                                    &error);
  }
  json = luaL_checklstring(L, arg, &len);
  if (!lua_isnoneornil(L, arg + 1)) {
    max_response_bytes = luaL_checkinteger(L, arg + 1);
    luaL_argcheck(L, max_response_bytes >= 0, arg + 1,
                  "max_response_bytes must be non-negative");
    max_bytes = (size_t)max_response_bytes;
  }
  lonejson_oauth2_token_response_init(&response);
  status = lonejson_oauth2_token_response_parse_json(
      runtime, json, len, max_bytes, &response, &error);
  if (owned_runtime != NULL) {
    lonejson_free(owned_runtime);
  }
  if (status != LONEJSON_STATUS_OK) {
    lonejson_oauth2_token_response_cleanup(&response);
    return ljlua_push_status_result(L, status, &error);
  }
  ljlua_auth_push_oauth2_token_response(L, &response);
  lonejson_oauth2_token_response_cleanup(&response);
  return 1;
}

static int ljlua_oidc_pkce_challenge(lua_State *L) {
  lonejson_owned_buffer out;
  lonejson_error error;
  lonejson_status status;
  const char *verifier;

  verifier = luaL_checkstring(L, 1);
  lonejson_owned_buffer_init(&out);
  lonejson_error_init(&error);
  status = lonejson_oidc_pkce_challenge(verifier, &out, &error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_owned_buffer_free(&out);
    return ljlua_push_status_result(L, status, &error);
  }
  lua_pushlstring(L, out.data != NULL ? out.data : "", out.len);
  lonejson_owned_buffer_free(&out);
  return 1;
}

static int ljlua_oidc_pkce_generate(lua_State *L) {
  lonejson_oidc_pkce pkce;
  lonejson_error error;
  lonejson_status status;
  lua_Integer verifier_bytes = 0;

  if (!lua_isnoneornil(L, 1)) {
    verifier_bytes = luaL_checkinteger(L, 1);
    luaL_argcheck(L, verifier_bytes >= 0, 1,
                  "verifier_bytes must be non-negative");
  }
  lonejson_oidc_pkce_init(&pkce);
  lonejson_error_init(&error);
  status =
      lonejson_oidc_pkce_generate((size_t)verifier_bytes, &pkce, &error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_oidc_pkce_cleanup(&pkce);
    return ljlua_push_status_result(L, status, &error);
  }
  ljlua_auth_push_oidc_pkce(L, &pkce);
  lonejson_oidc_pkce_cleanup(&pkce);
  return 1;
}

static int ljlua_oidc_authorization_url(lua_State *L) {
  lonejson_oidc_authorization_request request;
  lonejson_owned_buffer out;
  lonejson_error error;
  lonejson_status status;

  ljlua_auth_read_authorization_request(L, 1, &request);
  lonejson_owned_buffer_init(&out);
  lonejson_error_init(&error);
  status = lonejson_oidc_authorization_url(&request, &out, &error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_owned_buffer_free(&out);
    return ljlua_push_status_result(L, status, &error);
  }
  lua_pushlstring(L, out.data != NULL ? out.data : "", out.len);
  lonejson_owned_buffer_free(&out);
  return 1;
}

static int ljlua_oidc_authorization_callback_parse_query(lua_State *L) {
  lonejson_oidc_authorization_callback callback;
  lonejson_error error;
  lonejson_status status;
  lua_Integer max_query_bytes;
  const char *query;
  const char *expected_state;
  size_t len;
  size_t max_bytes = 0u;

  query = luaL_checklstring(L, 1, &len);
  expected_state = luaL_checkstring(L, 2);
  if (!lua_isnoneornil(L, 3)) {
    max_query_bytes = luaL_checkinteger(L, 3);
    luaL_argcheck(L, max_query_bytes >= 0, 3,
                  "max_query_bytes must be non-negative");
    max_bytes = (size_t)max_query_bytes;
  }
  lonejson_oidc_authorization_callback_init(&callback);
  lonejson_error_init(&error);
  status = lonejson_oidc_authorization_callback_parse_query(
      query, len, expected_state, max_bytes, &callback, &error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_oidc_authorization_callback_cleanup(&callback);
    return ljlua_push_status_result(L, status, &error);
  }
  ljlua_auth_push_oidc_callback(L, &callback);
  lonejson_oidc_authorization_callback_cleanup(&callback);
  return 1;
}

static int ljlua_oidc_validate_bearer_token(lua_State *L) {
  lonejson *runtime;
  lonejson *owned_runtime;
  lonejson_oidc_jwks_cache cache;
  lonejson_oidc_jwks_cache_policy cache_policy;
  lonejson_jwt_claim_policy claim_policy;
  lonejson_oidc_bearer_validation_request request;
  lonejson_oidc_bearer_validation validation;
  lonejson_error error;
  lonejson_status status;
  const char *authorization_header;
  const char *jwks_json;
  const char *failure;
  size_t jwks_len;
  int arg;

  runtime = ljlua_auth_runtime_arg(L, &arg, &owned_runtime, &error);
  if (runtime == NULL) {
    return ljlua_push_status_result(L, LONEJSON_STATUS_INVALID_ARGUMENT,
                                    &error);
  }
  authorization_header = luaL_checkstring(L, arg);
  jwks_json = luaL_checklstring(L, arg + 1, &jwks_len);
  ljlua_auth_read_jwks_cache_policy(L, arg + 2, &cache_policy);
  ljlua_auth_read_policy(L, arg + 3, &claim_policy, 1);

  lonejson_oidc_jwks_cache_init(&cache);
  lonejson_oidc_bearer_validation_init(&validation);
  status = lonejson_oidc_jwks_cache_update_json(
      runtime, &cache, &cache_policy, jwks_json, jwks_len, &error);
  if (status == LONEJSON_STATUS_OK) {
    memset(&request, 0, sizeof(request));
    request.authorization_header = authorization_header;
    request.jwks_cache = &cache;
    request.jwks_policy = &cache_policy;
    request.claim_policy = &claim_policy;
    status =
        lonejson_oidc_validate_bearer_token(runtime, &request, &validation,
                                            &error);
  }
  if (owned_runtime != NULL) {
    lonejson_free(owned_runtime);
  }
  ljlua_auth_free_policy(&claim_policy);
  if (status != LONEJSON_STATUS_OK) {
    failure = lonejson_auth_failure_string(validation.failure);
    lonejson_oidc_bearer_validation_cleanup(&validation);
    lonejson_oidc_jwks_cache_cleanup(&cache);
    lua_pushnil(L);
    ljlua_push_error(L, &error);
    lua_pushstring(L, failure);
    return 3;
  }
  ljlua_auth_push_bearer_validation(L, &validation);
  lonejson_oidc_bearer_validation_cleanup(&validation);
  lonejson_oidc_jwks_cache_cleanup(&cache);
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

static int ljlua_jwt_validate_compact_signature(lua_State *L) {
  lonejson *runtime;
  lonejson *owned_runtime;
  lonejson_jwt_compact compact;
  lonejson_jwt_header header;
  lonejson_jwt_claims claims;
  lonejson_jwt_claim_policy limits;
  lonejson_jwk jwk;
  lonejson_error error;
  lonejson_status status;
  const char *token;
  const char *jwk_json;
  size_t token_len;
  size_t jwk_len;
  int arg;

  runtime = ljlua_auth_runtime_arg(L, &arg, &owned_runtime, &error);
  if (runtime == NULL) {
    return ljlua_push_status_result(L, LONEJSON_STATUS_INVALID_ARGUMENT,
                                    &error);
  }
  token = luaL_checklstring(L, arg, &token_len);
  jwk_json = luaL_checklstring(L, arg + 1, &jwk_len);
  ljlua_auth_read_policy(L, arg + 2, &limits, 0);
  lonejson_jwt_header_init(&header);
  lonejson_jwt_claims_init(&claims);
  lonejson_jwk_init(&jwk);
  status = lonejson_jwt_parse_compact(token, token_len, &compact, &error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_jwt_decode_compact(runtime, token, token_len, &limits,
                                         &header, &claims, &error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status =
        lonejson_jwk_parse_json(runtime, jwk_json, jwk_len, &jwk, &error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = owned_runtime == NULL
                 ? lonejson_jwt_validate_signature_with_runtime(
                       runtime, &compact, &header, &jwk, &error)
                 : lonejson_jwt_validate_signature(&compact, &header, &jwk,
                                                   &error);
  }
  if (owned_runtime != NULL) {
    lonejson_free(owned_runtime);
  }
  ljlua_auth_free_policy(&limits);
  lonejson_jwk_cleanup(&jwk);
  lonejson_jwt_header_cleanup(&header);
  lonejson_jwt_claims_cleanup(&claims);
  if (status != LONEJSON_STATUS_OK) {
    return ljlua_push_status_result(L, status, &error);
  }
  lua_pushboolean(L, 1);
  return 1;
}
#endif
