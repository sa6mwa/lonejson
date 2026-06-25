typedef struct ljlua_path_visit_state {
  lua_State *L;
  int callbacks_ref;
} ljlua_path_visit_state;

typedef struct ljlua_candidate_visit_state {
  lua_State *L;
  int options_ref;
  int path_callbacks_ref;
  int payload_sink_ref;
  ljlua_path_visit_state path_state;
  lonejson_path_value_visitor path_visitor;
} ljlua_candidate_visit_state;

typedef enum ljlua_path_extra_kind {
  LJLUA_PATH_EXTRA_NONE = 0,
  LJLUA_PATH_EXTRA_CHUNK = 1,
  LJLUA_PATH_EXTRA_BOOL = 2
} ljlua_path_extra_kind;

static void ljlua_set_callback_error(lonejson_error *error,
                                     lonejson_status status,
                                     const char *message) {
  if (error == NULL) {
    return;
  }
  memset(error, 0, sizeof(*error));
  error->code = status;
  snprintf(error->message, sizeof(error->message), "%s", message);
}

static void ljlua_push_value_path(lua_State *L,
                                  const lonejson_value_path *path) {
  size_t i;

  if (path == NULL || path->segment_count == 0u) {
    lua_createtable(L, 0, 0);
    return;
  }
  lua_createtable(L, (int)path->segment_count, 0);
  for (i = 0u; i < path->segment_count; ++i) {
    lua_pushlstring(L, path->segments[i].data, path->segments[i].len);
    lua_rawseti(L, -2, (lua_Integer)i + 1);
  }
}

static lonejson_status
ljlua_call_path_callback(ljlua_path_visit_state *state, const char *name,
                         const lonejson_value_path *path,
                         ljlua_path_extra_kind extra_kind,
                         const char *chunk_data, size_t chunk_len,
                         int bool_value, lonejson_error *error) {
  lua_State *L = state->L;
  int rc;

  lua_rawgeti(L, LUA_REGISTRYINDEX, state->callbacks_ref);
  lua_getfield(L, -1, name);
  if (lua_isnil(L, -1)) {
    lua_pop(L, 2);
    return LONEJSON_STATUS_OK;
  }
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 2);
    ljlua_set_callback_error(error, LONEJSON_STATUS_CALLBACK_FAILED,
                             "path visitor callback is not a function");
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  lua_remove(L, -2);
  ljlua_push_value_path(L, path);
  if (extra_kind == LJLUA_PATH_EXTRA_CHUNK) {
    lua_pushlstring(L, chunk_data, chunk_len);
  } else if (extra_kind == LJLUA_PATH_EXTRA_BOOL) {
    lua_pushboolean(L, bool_value != 0);
  }
  rc = lua_pcall(L, extra_kind == LJLUA_PATH_EXTRA_NONE ? 1 : 2, 1, 0);
  if (rc != LUA_OK) {
    const char *message = lua_tostring(L, -1);
    ljlua_set_callback_error(error, LONEJSON_STATUS_CALLBACK_FAILED,
                             message != NULL ? message
                                             : "path visitor callback failed");
    lua_pop(L, 1);
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  if (lua_isboolean(L, -1) && !lua_toboolean(L, -1)) {
    lua_pop(L, 1);
    ljlua_set_callback_error(error, LONEJSON_STATUS_CALLBACK_FAILED,
                             "path visitor callback returned false");
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  lua_pop(L, 1);
  return LONEJSON_STATUS_OK;
}

static lonejson_status ljlua_path_event_named(void *user,
                                              const lonejson_value_path *path,
                                              lonejson_error *error,
                                              const char *name) {
  ljlua_path_visit_state *state = (ljlua_path_visit_state *)user;
  return ljlua_call_path_callback(state, name, path, LJLUA_PATH_EXTRA_NONE,
                                  NULL, 0u, 0, error);
}

static lonejson_status ljlua_object_begin_cb(void *user,
                                             const lonejson_value_path *path,
                                             lonejson_error *error) {
  return ljlua_path_event_named(user, path, error, "object_begin");
}

static lonejson_status ljlua_object_end_cb(void *user,
                                           const lonejson_value_path *path,
                                           lonejson_error *error) {
  return ljlua_path_event_named(user, path, error, "object_end");
}

static lonejson_status ljlua_key_begin_cb(void *user,
                                          const lonejson_value_path *path,
                                          lonejson_error *error) {
  return ljlua_path_event_named(user, path, error, "object_key_begin");
}

static lonejson_status ljlua_key_end_cb(void *user,
                                        const lonejson_value_path *path,
                                        lonejson_error *error) {
  return ljlua_path_event_named(user, path, error, "object_key_end");
}

static lonejson_status ljlua_array_begin_cb(void *user,
                                            const lonejson_value_path *path,
                                            lonejson_error *error) {
  return ljlua_path_event_named(user, path, error, "array_begin");
}

static lonejson_status ljlua_array_end_cb(void *user,
                                          const lonejson_value_path *path,
                                          lonejson_error *error) {
  return ljlua_path_event_named(user, path, error, "array_end");
}

static lonejson_status ljlua_string_begin_cb(void *user,
                                             const lonejson_value_path *path,
                                             lonejson_error *error) {
  return ljlua_path_event_named(user, path, error, "string_begin");
}

static lonejson_status ljlua_string_end_cb(void *user,
                                           const lonejson_value_path *path,
                                           lonejson_error *error) {
  return ljlua_path_event_named(user, path, error, "string_end");
}

static lonejson_status ljlua_number_begin_cb(void *user,
                                             const lonejson_value_path *path,
                                             lonejson_error *error) {
  return ljlua_path_event_named(user, path, error, "number_begin");
}

static lonejson_status ljlua_number_end_cb(void *user,
                                           const lonejson_value_path *path,
                                           lonejson_error *error) {
  return ljlua_path_event_named(user, path, error, "number_end");
}

static lonejson_status ljlua_null_cb(void *user,
                                     const lonejson_value_path *path,
                                     lonejson_error *error) {
  return ljlua_path_event_named(user, path, error, "null_value");
}

static lonejson_status
ljlua_chunk_named(void *user, const lonejson_value_path *path, const char *data,
                  size_t len, lonejson_error *error, const char *name) {
  ljlua_path_visit_state *state = (ljlua_path_visit_state *)user;
  return ljlua_call_path_callback(state, name, path, LJLUA_PATH_EXTRA_CHUNK,
                                  data, len, 0, error);
}

static lonejson_status ljlua_key_chunk_cb(void *user,
                                          const lonejson_value_path *path,
                                          const char *data, size_t len,
                                          lonejson_error *error) {
  return ljlua_chunk_named(user, path, data, len, error, "object_key_chunk");
}

static lonejson_status ljlua_string_chunk_cb(void *user,
                                             const lonejson_value_path *path,
                                             const char *data, size_t len,
                                             lonejson_error *error) {
  return ljlua_chunk_named(user, path, data, len, error, "string_chunk");
}

static lonejson_status ljlua_number_chunk_cb(void *user,
                                             const lonejson_value_path *path,
                                             const char *data, size_t len,
                                             lonejson_error *error) {
  return ljlua_chunk_named(user, path, data, len, error, "number_chunk");
}

static lonejson_status ljlua_bool_cb(void *user,
                                     const lonejson_value_path *path, int value,
                                     lonejson_error *error) {
  ljlua_path_visit_state *state = (ljlua_path_visit_state *)user;
  return ljlua_call_path_callback(state, "boolean_value", path,
                                  LJLUA_PATH_EXTRA_BOOL, NULL, 0u, value,
                                  error);
}

static lonejson_path_value_visitor ljlua_make_path_visitor(void) {
  lonejson_path_value_visitor visitor = lonejson_default_path_value_visitor();

  visitor.object_begin = ljlua_object_begin_cb;
  visitor.object_end = ljlua_object_end_cb;
  visitor.object_key_begin = ljlua_key_begin_cb;
  visitor.object_key_chunk = ljlua_key_chunk_cb;
  visitor.object_key_end = ljlua_key_end_cb;
  visitor.array_begin = ljlua_array_begin_cb;
  visitor.array_end = ljlua_array_end_cb;
  visitor.string_begin = ljlua_string_begin_cb;
  visitor.string_chunk = ljlua_string_chunk_cb;
  visitor.string_end = ljlua_string_end_cb;
  visitor.number_begin = ljlua_number_begin_cb;
  visitor.number_chunk = ljlua_number_chunk_cb;
  visitor.number_end = ljlua_number_end_cb;
  visitor.boolean_value = ljlua_bool_cb;
  visitor.null_value = ljlua_null_cb;
  return visitor;
}

static int ljlua_push_status_result(lua_State *L, lonejson_status status,
                                    const lonejson_error *error) {
  if (status == LONEJSON_STATUS_OK) {
    lua_pushboolean(L, 1);
    return 1;
  }
  lua_pushnil(L);
  ljlua_push_error(L, error);
  return 2;
}

static lonejson *ljlua_ensure_visit_runtime(lua_State *L, lonejson *runtime,
                                            lonejson **owned_runtime,
                                            lonejson_error *error) {
  (void)L;
  *owned_runtime = NULL;
  if (runtime != NULL) {
    return runtime;
  }
  *owned_runtime = lonejson_new(NULL, error);
  return *owned_runtime;
}

static int ljlua_visit_path_value_string(lua_State *L) {
  ljlua_runtime_ud *runtime_ud = NULL;
  lonejson *runtime = NULL;
  lonejson *owned_runtime = NULL;
  const char *json;
  size_t len;
  int callbacks_index;
  ljlua_path_visit_state state;
  lonejson_path_value_visitor visitor;
  lonejson_error error;
  lonejson_status status;

  if (lua_gettop(L) >= 1 && luaL_testudata(L, 1, LJLUA_RUNTIME_MT) != NULL) {
    runtime_ud = ljlua_check_runtime(L, 1);
    runtime = runtime_ud->runtime;
    json = luaL_checklstring(L, 2, &len);
    callbacks_index = 3;
  } else {
    json = luaL_checklstring(L, 1, &len);
    callbacks_index = 2;
  }
  luaL_checktype(L, callbacks_index, LUA_TTABLE);
  lua_pushvalue(L, callbacks_index);
  state.L = L;
  state.callbacks_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  visitor = ljlua_make_path_visitor();
  runtime = ljlua_ensure_visit_runtime(L, runtime, &owned_runtime, &error);
  if (runtime == NULL) {
    luaL_unref(L, LUA_REGISTRYINDEX, state.callbacks_ref);
    return ljlua_push_status_result(L, LONEJSON_STATUS_INVALID_ARGUMENT,
                                    &error);
  }
  status = lonejson_visit_path_value_buffer(runtime, json, len, &visitor,
                                            &state, &error);
  if (owned_runtime != NULL) {
    lonejson_free(owned_runtime);
  }
  luaL_unref(L, LUA_REGISTRYINDEX, state.callbacks_ref);
  return ljlua_push_status_result(L, status, &error);
}

static int ljlua_visit_path_value_path(lua_State *L) {
  ljlua_runtime_ud *runtime_ud = NULL;
  lonejson *runtime = NULL;
  lonejson *owned_runtime = NULL;
  const char *path;
  int callbacks_index;
  ljlua_path_visit_state state;
  lonejson_path_value_visitor visitor;
  lonejson_error error;
  lonejson_status status;

  if (lua_gettop(L) >= 1 && luaL_testudata(L, 1, LJLUA_RUNTIME_MT) != NULL) {
    runtime_ud = ljlua_check_runtime(L, 1);
    runtime = runtime_ud->runtime;
    path = luaL_checkstring(L, 2);
    callbacks_index = 3;
  } else {
    path = luaL_checkstring(L, 1);
    callbacks_index = 2;
  }
  luaL_checktype(L, callbacks_index, LUA_TTABLE);
  lua_pushvalue(L, callbacks_index);
  state.L = L;
  state.callbacks_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  visitor = ljlua_make_path_visitor();
  runtime = ljlua_ensure_visit_runtime(L, runtime, &owned_runtime, &error);
  if (runtime == NULL) {
    luaL_unref(L, LUA_REGISTRYINDEX, state.callbacks_ref);
    return ljlua_push_status_result(L, LONEJSON_STATUS_INVALID_ARGUMENT,
                                    &error);
  }
  status =
      lonejson_visit_path_value_path(runtime, path, &visitor, &state, &error);
  if (owned_runtime != NULL) {
    lonejson_free(owned_runtime);
  }
  luaL_unref(L, LUA_REGISTRYINDEX, state.callbacks_ref);
  return ljlua_push_status_result(L, status, &error);
}

static int ljlua_visit_path_value_file(lua_State *L) {
  ljlua_runtime_ud *runtime_ud = NULL;
  lonejson *runtime = NULL;
  lonejson *owned_runtime = NULL;
  FILE *fp;
  int callbacks_index;
  ljlua_path_visit_state state;
  lonejson_path_value_visitor visitor;
  lonejson_error error;
  lonejson_status status;

  if (lua_gettop(L) >= 1 && luaL_testudata(L, 1, LJLUA_RUNTIME_MT) != NULL) {
    runtime_ud = ljlua_check_runtime(L, 1);
    runtime = runtime_ud->runtime;
    fp = ljlua_check_file(L, 2);
    callbacks_index = 3;
  } else {
    fp = ljlua_check_file(L, 1);
    callbacks_index = 2;
  }
  luaL_checktype(L, callbacks_index, LUA_TTABLE);
  lua_pushvalue(L, callbacks_index);
  state.L = L;
  state.callbacks_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  visitor = ljlua_make_path_visitor();
  runtime = ljlua_ensure_visit_runtime(L, runtime, &owned_runtime, &error);
  if (runtime == NULL) {
    luaL_unref(L, LUA_REGISTRYINDEX, state.callbacks_ref);
    return ljlua_push_status_result(L, LONEJSON_STATUS_INVALID_ARGUMENT,
                                    &error);
  }
  status =
      lonejson_visit_path_value_filep(runtime, fp, &visitor, &state, &error);
  if (owned_runtime != NULL) {
    lonejson_free(owned_runtime);
  }
  luaL_unref(L, LUA_REGISTRYINDEX, state.callbacks_ref);
  return ljlua_push_status_result(L, status, &error);
}

static int ljlua_visit_path_value_fd(lua_State *L) {
  ljlua_runtime_ud *runtime_ud = NULL;
  lonejson *runtime = NULL;
  lonejson *owned_runtime = NULL;
  int fd;
  int callbacks_index;
  ljlua_path_visit_state state;
  lonejson_path_value_visitor visitor;
  lonejson_error error;
  lonejson_status status;

  if (lua_gettop(L) >= 1 && luaL_testudata(L, 1, LJLUA_RUNTIME_MT) != NULL) {
    runtime_ud = ljlua_check_runtime(L, 1);
    runtime = runtime_ud->runtime;
    fd = ljlua_check_fd_like(L, 2);
    callbacks_index = 3;
  } else {
    fd = ljlua_check_fd_like(L, 1);
    callbacks_index = 2;
  }
  luaL_checktype(L, callbacks_index, LUA_TTABLE);
  lua_pushvalue(L, callbacks_index);
  state.L = L;
  state.callbacks_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  visitor = ljlua_make_path_visitor();
  runtime = ljlua_ensure_visit_runtime(L, runtime, &owned_runtime, &error);
  if (runtime == NULL) {
    luaL_unref(L, LUA_REGISTRYINDEX, state.callbacks_ref);
    return ljlua_push_status_result(L, LONEJSON_STATUS_INVALID_ARGUMENT,
                                    &error);
  }
  status = lonejson_visit_path_value_fd(runtime, fd, &visitor, &state, &error);
  if (owned_runtime != NULL) {
    lonejson_free(owned_runtime);
  }
  luaL_unref(L, LUA_REGISTRYINDEX, state.callbacks_ref);
  return ljlua_push_status_result(L, status, &error);
}

static void ljlua_push_candidate_info(lua_State *L,
                                      const lonejson_candidate_info *info) {
  lua_createtable(L, 0, 8);
  ljlua_push_u64_plus_one(L, info->index);
  lua_setfield(L, -2, "index");
  ljlua_push_u64(L, info->index);
  lua_setfield(L, -2, "index0");
  ljlua_push_u64(L, info->stream_offset);
  lua_setfield(L, -2, "stream_offset");
  if (info->byte_size == LONEJSON_CANDIDATE_BYTE_SIZE_UNKNOWN) {
    lua_pushnil(L);
  } else {
    ljlua_push_u64(L, info->byte_size);
  }
  lua_setfield(L, -2, "byte_size");
  ljlua_push_u64(L, info->payload_size);
  lua_setfield(L, -2, "payload_size");
  if (info->payload != NULL) {
    lua_pushlstring(L, (const char *)info->payload, (size_t)info->payload_size);
    lua_setfield(L, -2, "payload");
  }
  if (info->payload_spool != NULL) {
    ljlua_push_spool(L, (lonejson_spooled *)info->payload_spool, 0, 0, 1);
    lua_setfield(L, -2, "payload_spool");
  }
}

#if defined(LONEJSON_TEST_LUA_CANDIDATE_INFO)
static int ljlua_test_candidate_info_u64(lua_State *L) {
  lonejson_candidate_info info;

  memset(&info, 0, sizeof(info));
  info.index = ((lonejson_uint64)1u << 53u) + 1u;
  info.stream_offset = ((lonejson_uint64)1u << 53u) + 2u;
  info.byte_size = ((lonejson_uint64)1u << 53u) + 3u;
  info.payload_size = ((lonejson_uint64)1u << 53u) + 4u;
  ljlua_push_candidate_info(L, &info);
  return 1;
}
#endif

static lonejson_candidate_callback_result ljlua_call_candidate_callback(
    ljlua_candidate_visit_state *state, const char *name,
    const lonejson_candidate_info *candidate, lonejson_error *error) {
  lua_State *L = state->L;
  int rc;
  const char *result;

  lua_rawgeti(L, LUA_REGISTRYINDEX, state->options_ref);
  lua_getfield(L, -1, name);
  lua_remove(L, -2);
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
    return LONEJSON_CANDIDATE_CONTINUE;
  }
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 1);
    ljlua_set_callback_error(error, LONEJSON_STATUS_CALLBACK_FAILED,
                             "candidate callback is not a function");
    return LONEJSON_CANDIDATE_ERROR;
  }
  ljlua_push_candidate_info(L, candidate);
  rc = lua_pcall(L, 1, 1, 0);
  if (rc != LUA_OK) {
    const char *message = lua_tostring(L, -1);
    ljlua_set_callback_error(error, LONEJSON_STATUS_CALLBACK_FAILED,
                             message != NULL ? message
                                             : "candidate callback failed");
    lua_pop(L, 1);
    return LONEJSON_CANDIDATE_ERROR;
  }
  if (lua_isboolean(L, -1)) {
    if (!lua_toboolean(L, -1)) {
      lua_pop(L, 1);
      ljlua_set_callback_error(error, LONEJSON_STATUS_CALLBACK_FAILED,
                               "candidate callback returned false");
      return LONEJSON_CANDIDATE_ERROR;
    }
    lua_pop(L, 1);
    return LONEJSON_CANDIDATE_CONTINUE;
  }
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
    return LONEJSON_CANDIDATE_CONTINUE;
  }
  result = lua_tostring(L, -1);
  if (result != NULL && strcmp(result, "stop") == 0) {
    lua_pop(L, 1);
    return LONEJSON_CANDIDATE_STOP;
  }
  if (result != NULL && strcmp(result, "continue") == 0) {
    lua_pop(L, 1);
    return LONEJSON_CANDIDATE_CONTINUE;
  }
  if (result != NULL && strcmp(result, "error") == 0) {
    lua_pop(L, 1);
    ljlua_set_callback_error(error, LONEJSON_STATUS_CALLBACK_FAILED,
                             "candidate callback returned error");
    return LONEJSON_CANDIDATE_ERROR;
  }
  lua_pop(L, 1);
  return LONEJSON_CANDIDATE_CONTINUE;
}

static lonejson_candidate_callback_result
ljlua_candidate_begin_cb(void *user, const lonejson_candidate_info *candidate,
                         lonejson_error *error) {
  return ljlua_call_candidate_callback((ljlua_candidate_visit_state *)user,
                                       "candidate_begin", candidate, error);
}

static lonejson_candidate_callback_result
ljlua_candidate_end_cb(void *user, const lonejson_candidate_info *candidate,
                       lonejson_error *error) {
  return ljlua_call_candidate_callback((ljlua_candidate_visit_state *)user,
                                       "candidate_end", candidate, error);
}

static lonejson_status ljlua_candidate_payload_sink(void *user,
                                                    const void *data,
                                                    size_t len,
                                                    lonejson_error *error) {
  ljlua_candidate_visit_state *state = (ljlua_candidate_visit_state *)user;
  lua_State *L = state->L;
  int rc;

  lua_rawgeti(L, LUA_REGISTRYINDEX, state->payload_sink_ref);
  lua_pushlstring(L, (const char *)data, len);
  rc = lua_pcall(L, 1, 1, 0);
  if (rc != LUA_OK) {
    const char *message = lua_tostring(L, -1);
    ljlua_set_callback_error(error, LONEJSON_STATUS_CALLBACK_FAILED,
                             message != NULL ? message
                                             : "candidate payload sink failed");
    lua_pop(L, 1);
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  if (lua_isboolean(L, -1) && !lua_toboolean(L, -1)) {
    lua_pop(L, 1);
    ljlua_set_callback_error(error, LONEJSON_STATUS_CALLBACK_FAILED,
                             "candidate payload sink returned false");
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  lua_pop(L, 1);
  return LONEJSON_STATUS_OK;
}

static lonejson_candidate_framing ljlua_parse_candidate_framing(lua_State *L,
                                                                int index) {
  const char *name;

  if (lua_isnoneornil(L, index)) {
    return LONEJSON_CANDIDATE_FRAMING_AUTO;
  }
  name = luaL_checkstring(L, index);
  if (strcmp(name, "auto") == 0) {
    return LONEJSON_CANDIDATE_FRAMING_AUTO;
  }
  if (strcmp(name, "single_value") == 0) {
    return LONEJSON_CANDIDATE_FRAMING_SINGLE_VALUE;
  }
  if (strcmp(name, "ndjson") == 0) {
    return LONEJSON_CANDIDATE_FRAMING_NDJSON;
  }
  if (strcmp(name, "array_items") == 0) {
    return LONEJSON_CANDIDATE_FRAMING_ARRAY_ITEMS;
  }
  return (lonejson_candidate_framing)luaL_error(
      L, "unsupported candidate framing '%s'", name);
}

static lonejson_candidate_capture_mode
ljlua_parse_candidate_capture(lua_State *L, int index) {
  const char *name;

  if (lua_isnoneornil(L, index)) {
    return LONEJSON_CANDIDATE_CAPTURE_NONE;
  }
  name = luaL_checkstring(L, index);
  if (strcmp(name, "none") == 0) {
    return LONEJSON_CANDIDATE_CAPTURE_NONE;
  }
  if (strcmp(name, "sink") == 0) {
    return LONEJSON_CANDIDATE_CAPTURE_SINK;
  }
  if (strcmp(name, "memory") == 0) {
    return LONEJSON_CANDIDATE_CAPTURE_MEMORY;
  }
  if (strcmp(name, "spooled") == 0) {
    return LONEJSON_CANDIDATE_CAPTURE_SPOOLED;
  }
  return (lonejson_candidate_capture_mode)luaL_error(
      L, "unsupported candidate capture mode '%s'", name);
}

static void
ljlua_parse_candidate_options(lua_State *L, int options_index,
                              ljlua_candidate_visit_state *state,
                              lonejson_candidate_stream_options *options) {
  options_index = lua_absindex(L, options_index);
  *options = lonejson_default_candidate_stream_options();
  lua_getfield(L, options_index, "framing");
  options->framing = ljlua_parse_candidate_framing(L, -1);
  lua_pop(L, 1);
  lua_getfield(L, options_index, "capture");
  options->capture_mode = ljlua_parse_candidate_capture(L, -1);
  lua_pop(L, 1);
  lua_getfield(L, options_index, "capture_mode");
  if (!lua_isnil(L, -1)) {
    options->capture_mode = ljlua_parse_candidate_capture(L, -1);
  }
  lua_pop(L, 1);
  lua_getfield(L, options_index, "max_memory_payload_bytes");
  if (!lua_isnil(L, -1)) {
    options->max_memory_payload_bytes = (size_t)luaL_checkinteger(L, -1);
  }
  lua_pop(L, 1);
  lua_getfield(L, options_index, "payload_sink");
  if (!lua_isnil(L, -1)) {
    luaL_checktype(L, -1, LUA_TFUNCTION);
    lua_pushvalue(L, -1);
    state->payload_sink_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    options->payload_sink = ljlua_candidate_payload_sink;
    options->payload_sink_user = state;
  }
  lua_pop(L, 1);
  lua_getfield(L, options_index, "path_visitor");
  if (!lua_isnil(L, -1)) {
    luaL_checktype(L, -1, LUA_TTABLE);
    lua_pushvalue(L, -1);
    state->path_callbacks_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    state->path_state.L = L;
    state->path_state.callbacks_ref = state->path_callbacks_ref;
    state->path_visitor = ljlua_make_path_visitor();
    options->path_visitor = &state->path_visitor;
    options->visitor_user = &state->path_state;
  }
  lua_pop(L, 1);
  if (state->path_callbacks_ref != LUA_NOREF) {
    options->visitor_user = &state->path_state;
  }
  options->candidate_begin = ljlua_candidate_begin_cb;
  options->candidate_end = ljlua_candidate_end_cb;
  options->candidate_user = state;
}

static int ljlua_visit_candidates_string(lua_State *L) {
  ljlua_runtime_ud *runtime_ud = NULL;
  lonejson *runtime = NULL;
  lonejson *owned_runtime = NULL;
  const char *json;
  size_t len;
  int options_index;
  ljlua_candidate_visit_state state;
  lonejson_candidate_stream_options options;
  lonejson_error error;
  lonejson_status status;

  memset(&state, 0, sizeof(state));
  state.L = L;
  state.options_ref = LUA_NOREF;
  state.path_callbacks_ref = LUA_NOREF;
  state.payload_sink_ref = LUA_NOREF;
  if (lua_gettop(L) >= 1 && luaL_testudata(L, 1, LJLUA_RUNTIME_MT) != NULL) {
    runtime_ud = ljlua_check_runtime(L, 1);
    runtime = runtime_ud->runtime;
    json = luaL_checklstring(L, 2, &len);
    options_index = 3;
  } else {
    json = luaL_checklstring(L, 1, &len);
    options_index = 2;
  }
  luaL_checktype(L, options_index, LUA_TTABLE);
  lua_pushvalue(L, options_index);
  state.options_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  ljlua_parse_candidate_options(L, options_index, &state, &options);
  runtime = ljlua_ensure_visit_runtime(L, runtime, &owned_runtime, &error);
  if (runtime == NULL) {
    luaL_unref(L, LUA_REGISTRYINDEX, state.options_ref);
    if (state.path_callbacks_ref != LUA_NOREF) {
      luaL_unref(L, LUA_REGISTRYINDEX, state.path_callbacks_ref);
    }
    if (state.payload_sink_ref != LUA_NOREF) {
      luaL_unref(L, LUA_REGISTRYINDEX, state.payload_sink_ref);
    }
    return ljlua_push_status_result(L, LONEJSON_STATUS_INVALID_ARGUMENT,
                                    &error);
  }
  status =
      lonejson_visit_candidates_buffer(runtime, json, len, &options, &error);
  if (owned_runtime != NULL) {
    lonejson_free(owned_runtime);
  }
  luaL_unref(L, LUA_REGISTRYINDEX, state.options_ref);
  if (state.path_callbacks_ref != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, state.path_callbacks_ref);
  }
  if (state.payload_sink_ref != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, state.payload_sink_ref);
  }
  return ljlua_push_status_result(L, status, &error);
}

static int ljlua_visit_candidates_path(lua_State *L) {
  ljlua_runtime_ud *runtime_ud = NULL;
  lonejson *runtime = NULL;
  lonejson *owned_runtime = NULL;
  const char *path;
  int options_index;
  ljlua_candidate_visit_state state;
  lonejson_candidate_stream_options options;
  lonejson_error error;
  lonejson_status status;

  memset(&state, 0, sizeof(state));
  state.L = L;
  state.options_ref = LUA_NOREF;
  state.path_callbacks_ref = LUA_NOREF;
  state.payload_sink_ref = LUA_NOREF;
  if (lua_gettop(L) >= 1 && luaL_testudata(L, 1, LJLUA_RUNTIME_MT) != NULL) {
    runtime_ud = ljlua_check_runtime(L, 1);
    runtime = runtime_ud->runtime;
    path = luaL_checkstring(L, 2);
    options_index = 3;
  } else {
    path = luaL_checkstring(L, 1);
    options_index = 2;
  }
  luaL_checktype(L, options_index, LUA_TTABLE);
  lua_pushvalue(L, options_index);
  state.options_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  ljlua_parse_candidate_options(L, options_index, &state, &options);
  runtime = ljlua_ensure_visit_runtime(L, runtime, &owned_runtime, &error);
  if (runtime == NULL) {
    luaL_unref(L, LUA_REGISTRYINDEX, state.options_ref);
    if (state.path_callbacks_ref != LUA_NOREF) {
      luaL_unref(L, LUA_REGISTRYINDEX, state.path_callbacks_ref);
    }
    if (state.payload_sink_ref != LUA_NOREF) {
      luaL_unref(L, LUA_REGISTRYINDEX, state.payload_sink_ref);
    }
    return ljlua_push_status_result(L, LONEJSON_STATUS_INVALID_ARGUMENT,
                                    &error);
  }
  status = lonejson_visit_candidates_path(runtime, path, &options, &error);
  if (owned_runtime != NULL) {
    lonejson_free(owned_runtime);
  }
  luaL_unref(L, LUA_REGISTRYINDEX, state.options_ref);
  if (state.path_callbacks_ref != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, state.path_callbacks_ref);
  }
  if (state.payload_sink_ref != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, state.payload_sink_ref);
  }
  return ljlua_push_status_result(L, status, &error);
}

static int ljlua_visit_candidates_file(lua_State *L) {
  ljlua_runtime_ud *runtime_ud = NULL;
  lonejson *runtime = NULL;
  lonejson *owned_runtime = NULL;
  FILE *fp;
  int options_index;
  ljlua_candidate_visit_state state;
  lonejson_candidate_stream_options options;
  lonejson_error error;
  lonejson_status status;

  memset(&state, 0, sizeof(state));
  state.L = L;
  state.options_ref = LUA_NOREF;
  state.path_callbacks_ref = LUA_NOREF;
  state.payload_sink_ref = LUA_NOREF;
  if (lua_gettop(L) >= 1 && luaL_testudata(L, 1, LJLUA_RUNTIME_MT) != NULL) {
    runtime_ud = ljlua_check_runtime(L, 1);
    runtime = runtime_ud->runtime;
    fp = ljlua_check_file(L, 2);
    options_index = 3;
  } else {
    fp = ljlua_check_file(L, 1);
    options_index = 2;
  }
  luaL_checktype(L, options_index, LUA_TTABLE);
  lua_pushvalue(L, options_index);
  state.options_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  ljlua_parse_candidate_options(L, options_index, &state, &options);
  runtime = ljlua_ensure_visit_runtime(L, runtime, &owned_runtime, &error);
  if (runtime == NULL) {
    luaL_unref(L, LUA_REGISTRYINDEX, state.options_ref);
    if (state.path_callbacks_ref != LUA_NOREF) {
      luaL_unref(L, LUA_REGISTRYINDEX, state.path_callbacks_ref);
    }
    if (state.payload_sink_ref != LUA_NOREF) {
      luaL_unref(L, LUA_REGISTRYINDEX, state.payload_sink_ref);
    }
    return ljlua_push_status_result(L, LONEJSON_STATUS_INVALID_ARGUMENT,
                                    &error);
  }
  status = lonejson_visit_candidates_filep(runtime, fp, &options, &error);
  if (owned_runtime != NULL) {
    lonejson_free(owned_runtime);
  }
  luaL_unref(L, LUA_REGISTRYINDEX, state.options_ref);
  if (state.path_callbacks_ref != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, state.path_callbacks_ref);
  }
  if (state.payload_sink_ref != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, state.payload_sink_ref);
  }
  return ljlua_push_status_result(L, status, &error);
}

static int ljlua_visit_candidates_fd(lua_State *L) {
  ljlua_runtime_ud *runtime_ud = NULL;
  lonejson *runtime = NULL;
  lonejson *owned_runtime = NULL;
  int fd;
  int options_index;
  ljlua_candidate_visit_state state;
  lonejson_candidate_stream_options options;
  lonejson_error error;
  lonejson_status status;

  memset(&state, 0, sizeof(state));
  state.L = L;
  state.options_ref = LUA_NOREF;
  state.path_callbacks_ref = LUA_NOREF;
  state.payload_sink_ref = LUA_NOREF;
  if (lua_gettop(L) >= 1 && luaL_testudata(L, 1, LJLUA_RUNTIME_MT) != NULL) {
    runtime_ud = ljlua_check_runtime(L, 1);
    runtime = runtime_ud->runtime;
    fd = ljlua_check_fd_like(L, 2);
    options_index = 3;
  } else {
    fd = ljlua_check_fd_like(L, 1);
    options_index = 2;
  }
  luaL_checktype(L, options_index, LUA_TTABLE);
  lua_pushvalue(L, options_index);
  state.options_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  ljlua_parse_candidate_options(L, options_index, &state, &options);
  runtime = ljlua_ensure_visit_runtime(L, runtime, &owned_runtime, &error);
  if (runtime == NULL) {
    luaL_unref(L, LUA_REGISTRYINDEX, state.options_ref);
    if (state.path_callbacks_ref != LUA_NOREF) {
      luaL_unref(L, LUA_REGISTRYINDEX, state.path_callbacks_ref);
    }
    if (state.payload_sink_ref != LUA_NOREF) {
      luaL_unref(L, LUA_REGISTRYINDEX, state.payload_sink_ref);
    }
    return ljlua_push_status_result(L, LONEJSON_STATUS_INVALID_ARGUMENT,
                                    &error);
  }
  status = lonejson_visit_candidates_fd(runtime, fd, &options, &error);
  if (owned_runtime != NULL) {
    lonejson_free(owned_runtime);
  }
  luaL_unref(L, LUA_REGISTRYINDEX, state.options_ref);
  if (state.path_callbacks_ref != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, state.path_callbacks_ref);
  }
  if (state.payload_sink_ref != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, state.payload_sink_ref);
  }
  return ljlua_push_status_result(L, status, &error);
}
