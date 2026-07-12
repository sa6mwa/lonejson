typedef struct ljlua_path_visit_state {
  lua_State *L;
  int callbacks_ref;
} ljlua_path_visit_state;

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
