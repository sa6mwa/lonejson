static int ljlua_schema_new(lua_State *L) {
  ljlua_runtime_ud *runtime_ud = ljlua_check_runtime(L, 1);
  const char *name = luaL_checkstring(L, 2);
  size_t count;
  size_t i;
  ljlua_schema *schema;
  ljlua_schema_ud *ud;

  luaL_checktype(L, 3, LUA_TTABLE);
  count = (size_t)lua_rawlen(L, 3);
  schema = (ljlua_schema *)calloc(1u, sizeof(*schema));
  if (schema == NULL) {
    return luaL_error(L, "failed to allocate schema");
  }
  schema->runtime = runtime_ud->runtime;
  schema->name = ljlua_strdup(name);
  schema->field_count = count;
  schema->metas = (ljlua_field_meta *)calloc(count, sizeof(schema->metas[0]));
  if (schema->metas == NULL) {
    ljlua_schema_destroy(schema);
    return luaL_error(L, "failed to allocate schema fields");
  }
  schema->fields = (lonejson_field *)calloc(count, sizeof(schema->fields[0]));
  if (schema->fields == NULL) {
    ljlua_schema_destroy(schema);
    return luaL_error(L, "failed to allocate runtime map fields");
  }
  for (i = 0u; i < count; ++i) {
    lua_rawgeti(L, 3, (lua_Integer)i + 1);
    ljlua_compile_field(L, schema->runtime, lua_gettop(L), &schema->metas[i]);
    lua_pop(L, 1);
  }
  ljlua_finalize_schema(schema);
  if (schema->record_size != 0u) {
    schema->scratch_record = (unsigned char *)malloc(schema->record_size);
    if (schema->scratch_record == NULL) {
      ljlua_schema_destroy(schema);
      return luaL_error(L, "failed to allocate schema scratch record");
    }
  }
  ud = (ljlua_schema_ud *)ljlua_newuserdata_slots(L, sizeof(*ud), 0);
  memset(ud, 0, sizeof(*ud));
  ud->magic = LJLUA_SCHEMA_MAGIC;
  ud->schema = schema;
  lua_pushvalue(L, 1);
  ud->runtime_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  luaL_setmetatable(L, LJLUA_SCHEMA_MT);
  return 1;
}

static int ljlua_schema_gc(lua_State *L) {
  ljlua_schema_ud *ud = ljlua_check_schema(L, 1);
  ljlua_schema_destroy(ud->schema);
  ud->schema = NULL;
  if (ud->runtime_ref != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, ud->runtime_ref);
    ud->runtime_ref = LUA_NOREF;
  }
  return 0;
}

static int ljlua_schema_new_record(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  ljlua_record_ud *record_ud;

  record_ud = (ljlua_record_ud *)ljlua_newuserdata_slots(
      L, sizeof(*record_ud) + schema_ud->schema->record_size, 0);
  memset(record_ud, 0, sizeof(*record_ud) + schema_ud->schema->record_size);
  record_ud->magic = LJLUA_RECORD_MAGIC;
  record_ud->schema = schema_ud->schema;
  lua_pushvalue(L, 1);
  record_ud->schema_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  record_ud->cleared = 1;
  luaL_setmetatable(L, LJLUA_RECORD_MT);
  ljlua_prepare_record_storage(record_ud->schema, record_ud->data);
  return 1;
}

static int ljlua_record_gc(lua_State *L) {
  ljlua_record_ud *ud = ljlua_check_record(L, 1);
  if (ud->schema != NULL) {
    ljlua_cleanup_record_storage(ud->schema, ud->data);
  }
  if (ud->schema_ref != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, ud->schema_ref);
    ud->schema_ref = LUA_NOREF;
  }
  return 0;
}

static int ljlua_record_index(lua_State *L) {
  ljlua_record_ud *ud = ljlua_check_record(L, 1);
  const char *key = luaL_checkstring(L, 2);
  ljlua_compiled_path *compiled;
  if (strcmp(key, "get") == 0) {
    lua_pushcfunction(L, ljlua_record_get);
    return 1;
  }
  if (strcmp(key, "to_table") == 0) {
    lua_pushcfunction(L, ljlua_record_to_table_method);
    return 1;
  }
  if (strcmp(key, "count") == 0) {
    lua_pushcfunction(L, ljlua_record_count);
    return 1;
  }
  if (strcmp(key, "clear") == 0) {
    lua_pushcfunction(L, ljlua_record_clear);
    return 1;
  }
  compiled = ljlua_find_compiled_path(ud->schema, key);
  if (compiled == NULL) {
    compiled = ljlua_compile_path(L, ud->schema, key);
  }
  if (compiled != NULL && compiled->step_count == 1u &&
      compiled->steps[0].index == 0u) {
    return ljlua_push_compiled_path_value(L, ud->data, compiled, 1);
  }
  lua_pushnil(L);
  return 1;
}

static int ljlua_record_to_table_method(lua_State *L) {
  ljlua_record_ud *ud = ljlua_check_record(L, 1);
  return ljlua_record_to_table(L, ud->schema, ud->data);
}

static int ljlua_record_get(lua_State *L) {
  ljlua_record_ud *ud = ljlua_check_record(L, 1);
  ljlua_compiled_path *compiled = NULL;

  if (lua_type(L, 2) == LUA_TSTRING) {
    const char *path = lua_tostring(L, 2);
    compiled = ljlua_find_compiled_path(ud->schema, path);
    if (compiled == NULL) {
      compiled = ljlua_compile_path(L, ud->schema, path);
    }
  } else {
    ljlua_path_ud *path_ud = ljlua_check_path(L, 2);
    if (path_ud->schema != ud->schema) {
      return luaL_error(L, "path belongs to a different schema");
    }
    compiled = path_ud->compiled;
  }
  return ljlua_push_compiled_path_value(L, ud->data, compiled, 1);
}

static int ljlua_record_count(lua_State *L) {
  ljlua_record_ud *ud = ljlua_check_record(L, 1);
  ljlua_field_meta *meta = NULL;
  void *ptr;

  if (lua_type(L, 2) == LUA_TSTRING) {
    const char *key = lua_tostring(L, 2);
    ljlua_compiled_path *compiled = ljlua_find_compiled_path(ud->schema, key);
    if (compiled == NULL) {
      compiled = ljlua_compile_path(L, ud->schema, key);
    }
    if (compiled == NULL || compiled->step_count != 1u ||
        compiled->steps[0].index != 0u) {
      return luaL_error(L, "unknown field '%s'", key);
    }
    meta = compiled->steps[0].meta;
  } else {
    ljlua_path_ud *path_ud = ljlua_check_path(L, 2);
    if (path_ud->schema != ud->schema) {
      return luaL_error(L, "path belongs to a different schema");
    }
    if (path_ud->compiled->step_count != 1u ||
        path_ud->compiled->steps[0].index != 0u) {
      return luaL_error(L, "count path must reference a top-level array field");
    }
    meta = path_ud->compiled->steps[0].meta;
  }
  ptr = (unsigned char *)ud->data + meta->field.struct_offset;
  switch (meta->lua_kind) {
  case LJLUA_FIELD_STRING_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_string_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_I64_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_i64_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_U64_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_u64_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_F64_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_f64_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_BOOL_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_bool_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_OBJECT_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_object_array *)ptr)->count);
    return 1;
  default:
    return luaL_error(L, "field is not an array");
  }
}

static int ljlua_record_clear(lua_State *L) {
  ljlua_record_ud *ud = ljlua_check_record(L, 1);
  lonejson_reset(ud->schema->runtime, &ud->schema->map, ud->data);
  ud->cleared = 1;
  return 0;
}

static int ljlua_path_gc(lua_State *L) {
  ljlua_path_ud *ud = ljlua_check_path(L, 1);
  if (ud->schema_ref != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, ud->schema_ref);
    ud->schema_ref = LUA_NOREF;
  }
  ud->schema = NULL;
  ud->compiled = NULL;
  return 0;
}

static int ljlua_path_get(lua_State *L) {
  ljlua_path_ud *path_ud = ljlua_check_path(L, 1);
  ljlua_record_ud *record_ud = ljlua_check_record(L, 2);

  if (path_ud->schema != record_ud->schema) {
    return luaL_error(L, "path belongs to a different schema");
  }
  return ljlua_push_compiled_path_value(L, record_ud->data, path_ud->compiled,
                                        2);
}

static int ljlua_path_count(lua_State *L) {
  ljlua_path_ud *path_ud = ljlua_check_path(L, 1);
  ljlua_record_ud *record_ud = ljlua_check_record(L, 2);
  ljlua_field_meta *meta;
  void *ptr;

  if (path_ud->schema != record_ud->schema) {
    return luaL_error(L, "path belongs to a different schema");
  }
  if (path_ud->compiled->step_count != 1u ||
      path_ud->compiled->steps[0].index != 0u) {
    return luaL_error(L, "count path must reference a top-level array field");
  }
  meta = path_ud->compiled->steps[0].meta;
  ptr = (unsigned char *)record_ud->data + meta->field.struct_offset;
  switch (meta->lua_kind) {
  case LJLUA_FIELD_STRING_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_string_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_I64_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_i64_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_U64_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_u64_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_F64_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_f64_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_BOOL_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_bool_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_OBJECT_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_object_array *)ptr)->count);
    return 1;
  default:
    return luaL_error(L, "field is not an array");
  }
}

static int ljlua_getter_call(lua_State *L) {
  ljlua_path_ud *path_ud =
      (ljlua_path_ud *)lua_touserdata(L, lua_upvalueindex(1));
  ljlua_record_ud *record_ud = ljlua_check_record(L, 1);

  if (path_ud == NULL || path_ud->schema != record_ud->schema) {
    return luaL_error(L, "path belongs to a different schema");
  }
  return ljlua_push_compiled_path_value(L, record_ud->data, path_ud->compiled,
                                        1);
}

static int ljlua_counter_call(lua_State *L) {
  ljlua_path_ud *path_ud =
      (ljlua_path_ud *)lua_touserdata(L, lua_upvalueindex(1));
  ljlua_record_ud *record_ud = ljlua_check_record(L, 1);
  ljlua_field_meta *meta;
  void *ptr;

  if (path_ud == NULL || path_ud->schema != record_ud->schema) {
    return luaL_error(L, "path belongs to a different schema");
  }
  if (path_ud->compiled->step_count != 1u ||
      path_ud->compiled->steps[0].index != 0u) {
    return luaL_error(L, "count path must reference a top-level array field");
  }
  meta = path_ud->compiled->steps[0].meta;
  ptr = (unsigned char *)record_ud->data + meta->field.struct_offset;
  switch (meta->lua_kind) {
  case LJLUA_FIELD_STRING_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_string_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_I64_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_i64_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_U64_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_u64_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_F64_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_f64_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_BOOL_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_bool_array *)ptr)->count);
    return 1;
  case LJLUA_FIELD_OBJECT_ARRAY:
    lua_pushinteger(L, (lua_Integer)((lonejson_object_array *)ptr)->count);
    return 1;
  default:
    return luaL_error(L, "field is not an array");
  }
}

static int
ljlua_schema_decode_into_buffer(lua_State *L, ljlua_schema *schema,
                                void *record, const char *json, size_t len,
                                const lonejson__parse_options *options,
                                const lonejson_runtime *runtime) {
  lonejson_error error;
  lonejson_status status;

  status = ljlua_parse_buffer_with_options(&schema->map, record, json, len,
                                           options, runtime, &error);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    ljlua_push_error(L, &error);
    return 0;
  }
  return 1;
}

static int ljlua_schema_decode_into(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  ljlua_record_ud *record_ud = ljlua_check_record(L, 2);
  lonejson__parse_options effective_options;
  const lonejson_runtime *runtime;
  size_t len;
  const char *json = luaL_checklstring(L, 3, &len);

  runtime = lonejson__runtime_const(schema_ud->schema->runtime);
  ljlua_reject_legacy_options(L, 4, "schema:decode_into");
  effective_options = runtime->parse_options;
  if (record_ud->schema != schema_ud->schema) {
    return luaL_error(L, "record belongs to a different schema");
  }
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    if (effective_options.clear_destination) {
      lonejson_reset(schema_ud->schema->runtime, &schema_ud->schema->map,
                     record_ud->data);
    }
    ljlua_prepare_record_json_value_capture(
        L, schema_ud->schema, record_ud->data,
        effective_options.clear_destination ? 0 : 1);
    effective_options.clear_destination = 0;
  } else {
    if (effective_options.clear_destination) {
      lonejson_reset(schema_ud->schema->runtime, &schema_ud->schema->map,
                     record_ud->data);
      effective_options.clear_destination = 0;
    } else if (record_ud->cleared) {
      effective_options.clear_destination = 0;
    }
  }
  if (!ljlua_schema_decode_into_buffer(L, schema_ud->schema, record_ud->data,
                                       json, len, &effective_options,
                                       runtime)) {
    return 2;
  }
  record_ud->cleared = 0;
  lua_pushvalue(L, 2);
  return 1;
}

static int ljlua_schema_decode_path(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  const char *path = luaL_checkstring(L, 2);
  lonejson__parse_options options;
  const lonejson_runtime *runtime;
  lonejson_error error;
  lonejson_status status;
  unsigned char *record;
  int owned_record;
  ljlua_decode_context ctx;
  runtime = lonejson__runtime_const(schema_ud->schema->runtime);
  ljlua_reject_legacy_options(L, 3, "schema:decode_path");
  options = runtime->parse_options;
  memset(&ctx, 0, sizeof(ctx));
  record = ljlua_schema_borrow_scratch(schema_ud->schema, &owned_record);
  if (record == NULL) {
    return luaL_error(L, "failed to allocate decode buffer");
  }
  ljlua_prepare_record_storage(schema_ud->schema, record);
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    options.clear_destination = 0;
    ljlua_decode_context_prepare_table_mode(L, schema_ud->schema, record, &ctx);
  } else {
    options.clear_destination = 1;
  }
  status = ljlua_parse_path_with_options(&schema_ud->schema->map, record, path,
                                         &options, runtime, &error);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    ljlua_decode_context_cleanup(&ctx);
    ljlua_cleanup_record_storage(schema_ud->schema, record);
    ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
    lua_pushnil(L);
    ljlua_push_error(L, &error);
    return 2;
  }
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    ljlua_record_to_table_with_overrides(L, schema_ud->schema, record, &ctx);
  } else {
    ljlua_record_to_table(L, schema_ud->schema, record);
  }
  ljlua_decode_context_cleanup(&ctx);
  ljlua_cleanup_record_storage(schema_ud->schema, record);
  ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
  return 1;
}

static int ljlua_schema_decode_file(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  FILE *fp = ljlua_check_file(L, 2);
  lonejson__parse_options options;
  const lonejson_runtime *runtime;
  lonejson_error error;
  lonejson_status status;
  unsigned char *record;
  int owned_record;
  ljlua_decode_context ctx;
  runtime = lonejson__runtime_const(schema_ud->schema->runtime);
  ljlua_reject_legacy_options(L, 3, "schema:decode_file");
  options = runtime->parse_options;
  memset(&ctx, 0, sizeof(ctx));
  record = ljlua_schema_borrow_scratch(schema_ud->schema, &owned_record);
  if (record == NULL) {
    return luaL_error(L, "failed to allocate decode buffer");
  }
  ljlua_prepare_record_storage(schema_ud->schema, record);
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    options.clear_destination = 0;
    ljlua_decode_context_prepare_table_mode(L, schema_ud->schema, record, &ctx);
  } else {
    options.clear_destination = 1;
  }
  status = ljlua_parse_filep_with_options(&schema_ud->schema->map, record, fp,
                                          &options, runtime, &error);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    ljlua_decode_context_cleanup(&ctx);
    ljlua_cleanup_record_storage(schema_ud->schema, record);
    ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
    lua_pushnil(L);
    ljlua_push_error(L, &error);
    return 2;
  }
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    ljlua_record_to_table_with_overrides(L, schema_ud->schema, record, &ctx);
  } else {
    ljlua_record_to_table(L, schema_ud->schema, record);
  }
  ljlua_decode_context_cleanup(&ctx);
  ljlua_cleanup_record_storage(schema_ud->schema, record);
  ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
  return 1;
}

static int ljlua_schema_decode_fd(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  int fd = ljlua_check_fd_like(L, 2);
  FILE *fp;
  lonejson__parse_options options;
  const lonejson_runtime *runtime;
  lonejson_error error;
  lonejson_status status;
  unsigned char *record;
  int owned_record;
  ljlua_decode_context ctx;
  runtime = lonejson__runtime_const(schema_ud->schema->runtime);
  ljlua_reject_legacy_options(L, 3, "schema:decode_fd");
  options = runtime->parse_options;
  memset(&ctx, 0, sizeof(ctx));
  record = ljlua_schema_borrow_scratch(schema_ud->schema, &owned_record);
  if (record == NULL) {
    return luaL_error(L, "failed to allocate decode buffer");
  }
  ljlua_prepare_record_storage(schema_ud->schema, record);
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    options.clear_destination = 0;
    ljlua_decode_context_prepare_table_mode(L, schema_ud->schema, record, &ctx);
  } else {
    options.clear_destination = 1;
  }
  fp = ljlua_dup_fd_to_file(L, fd, "rb");
  status = ljlua_parse_filep_with_options(&schema_ud->schema->map, record, fp,
                                          &options, runtime, &error);
  fclose(fp);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    ljlua_decode_context_cleanup(&ctx);
    ljlua_cleanup_record_storage(schema_ud->schema, record);
    ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
    lua_pushnil(L);
    ljlua_push_error(L, &error);
    return 2;
  }
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    ljlua_record_to_table_with_overrides(L, schema_ud->schema, record, &ctx);
  } else {
    ljlua_record_to_table(L, schema_ud->schema, record);
  }
  ljlua_decode_context_cleanup(&ctx);
  ljlua_cleanup_record_storage(schema_ud->schema, record);
  ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
  return 1;
}

static int ljlua_schema_decode(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  size_t len;
  const char *json = luaL_checklstring(L, 2, &len);
  unsigned char *record;
  int owned_record;
  ljlua_decode_context ctx;
  lonejson__parse_options options;
  const lonejson_runtime *runtime;
  record = ljlua_schema_borrow_scratch(schema_ud->schema, &owned_record);
  if (record == NULL) {
    return luaL_error(L, "failed to allocate decode buffer");
  }
  memset(&ctx, 0, sizeof(ctx));
  runtime = lonejson__runtime_const(schema_ud->schema->runtime);
  ljlua_reject_legacy_options(L, 3, "schema:decode");
  options = runtime->parse_options;
  ljlua_prepare_record_storage(schema_ud->schema, record);
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    options.clear_destination = 0;
    ljlua_decode_context_prepare_table_mode(L, schema_ud->schema, record, &ctx);
    {
      lonejson_error error;
      lonejson_status status =
          ljlua_parse_buffer_with_options(&schema_ud->schema->map, record, json,
                                          len, &options, runtime, &error);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        ljlua_decode_context_cleanup(&ctx);
        ljlua_cleanup_record_storage(schema_ud->schema, record);
        ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
        lua_pushnil(L);
        ljlua_push_error(L, &error);
        return 2;
      }
    }
  } else {
    options.clear_destination = 1;
    if (!ljlua_schema_decode_into_buffer(L, schema_ud->schema, record, json,
                                         len, &options, runtime)) {
      ljlua_cleanup_record_storage(schema_ud->schema, record);
      ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
      return 2;
    }
  }
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    ljlua_record_to_table_with_overrides(L, schema_ud->schema, record, &ctx);
  } else {
    ljlua_record_to_table(L, schema_ud->schema, record);
  }
  ljlua_decode_context_cleanup(&ctx);
  ljlua_cleanup_record_storage(schema_ud->schema, record);
  ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
  return 1;
}

static int ljlua_schema_assign(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  ljlua_record_ud *record_ud = ljlua_check_record(L, 2);
  luaL_checktype(L, 3, LUA_TTABLE);
  if (record_ud->schema != schema_ud->schema) {
    return luaL_error(L, "record belongs to a different schema");
  }
  lonejson_reset(schema_ud->schema->runtime, &schema_ud->schema->map,
                 record_ud->data);
  record_ud->cleared = 1;
  ljlua_assign_table_to_record(L, schema_ud->schema, record_ud->data, 3);
  record_ud->cleared = 0;
  lua_pushvalue(L, 2);
  return 1;
}

static char *ljlua_serialize_value(lua_State *L, ljlua_schema *schema,
                                   void *record, size_t *out_len) {
  const lonejson_runtime *runtime;
  const lonejson__write_options *options;
  lonejson_error error;
  lonejson_status status;
  size_t needed = 0u;
  size_t capacity;
  size_t serialize_capacity;
  size_t max_output_bytes;
  size_t max_buffer_capacity;
  char *buffer;

  runtime = lonejson__runtime_const(schema->runtime);
  options = &runtime->write_options;
  max_output_bytes = options->max_output_bytes != 0u
                         ? options->max_output_bytes
                         : LONEJSON_WRITE_MAX_OUTPUT_BYTES;
  if (max_output_bytes == SIZE_MAX) {
    luaL_error(L, "max_output_bytes is too large");
  }
  max_buffer_capacity = max_output_bytes + 1u;
  capacity = schema->encode_capacity;
  if (capacity == 0u) {
    capacity = 256u;
  }
  if (capacity > max_buffer_capacity) {
    capacity = max_buffer_capacity;
  }
  if (schema->encode_buffer == NULL) {
    schema->encode_buffer = (char *)malloc(capacity);
    if (schema->encode_buffer == NULL) {
      luaL_error(L, "failed to allocate encode buffer");
    }
    schema->encode_capacity = capacity;
  }
  for (;;) {
    serialize_capacity = schema->encode_capacity;
    if (serialize_capacity > max_buffer_capacity) {
      serialize_capacity = max_buffer_capacity;
    }
    status = ljlua_serialize_buffer_with_options(
        &schema->map, record, schema->encode_buffer, serialize_capacity,
        &needed, options, &error);
    if (status != LONEJSON_STATUS_OVERFLOW) {
      break;
    }
    if (needed > max_output_bytes) {
      luaL_error(L, "serializer-owned output exceeds max_output_bytes");
    }
    capacity = schema->encode_capacity * 2u;
    if (capacity < needed + 1u) {
      capacity = needed + 1u;
    }
    if (capacity > max_buffer_capacity) {
      capacity = max_buffer_capacity;
    }
    if (capacity < 256u && max_output_bytes >= 256u) {
      capacity = 256u;
    }
    buffer = (char *)realloc(schema->encode_buffer, capacity);
    if (buffer == NULL) {
      luaL_error(L, "failed to grow encode buffer");
    }
    schema->encode_buffer = buffer;
    schema->encode_capacity = capacity;
  }
  if (needed > max_output_bytes) {
    luaL_error(L, "serializer-owned output exceeds max_output_bytes");
  }
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    luaL_error(L, "%s", error.message);
  }
  if (out_len != NULL) {
    *out_len = needed;
  }
  return schema->encode_buffer;
}

static int ljlua_schema_encode(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  size_t out_len = 0u;
  char *json;

  if (luaL_testudata(L, 2, LJLUA_RECORD_MT) != NULL) {
    ljlua_reject_legacy_options(L, 3, "schema:encode");
    ljlua_record_ud *record_ud = ljlua_check_record(L, 2);
    if (record_ud->schema != schema_ud->schema) {
      return luaL_error(L, "record belongs to a different schema");
    }
    json =
        ljlua_serialize_value(L, schema_ud->schema, record_ud->data, &out_len);
  } else {
    int owned_record;
    unsigned char *record =
        ljlua_schema_borrow_scratch(schema_ud->schema, &owned_record);
    if (record == NULL) {
      return luaL_error(L, "failed to allocate encode buffer");
    }
    ljlua_prepare_record_storage(schema_ud->schema, record);
    ljlua_assign_table_to_record(L, schema_ud->schema, record, 2);
    ljlua_reject_legacy_options(L, 3, "schema:encode");
    json = ljlua_serialize_value(L, schema_ud->schema, record, &out_len);
    ljlua_cleanup_record_storage(schema_ud->schema, record);
    ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
  }
  lua_pushlstring(L, json, out_len);
  return 1;
}

static int ljlua_schema_write_path(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  const char *path = luaL_checkstring(L, 3);
  const lonejson_runtime *runtime;
  const lonejson__write_options *options;
  lonejson_error error;
  lonejson_status status;

  runtime = lonejson__runtime_const(schema_ud->schema->runtime);
  ljlua_reject_legacy_options(L, 4, "schema:write_path");
  options = &runtime->write_options;
  if (luaL_testudata(L, 2, LJLUA_RECORD_MT) != NULL) {
    ljlua_record_ud *record_ud = ljlua_check_record(L, 2);
    status = ljlua_serialize_path_with_options(
        &schema_ud->schema->map, record_ud->data, path, options, &error);
  } else {
    int owned_record;
    unsigned char *record =
        ljlua_schema_borrow_scratch(schema_ud->schema, &owned_record);
    if (record == NULL) {
      return luaL_error(L, "failed to allocate encode buffer");
    }
    ljlua_prepare_record_storage(schema_ud->schema, record);
    ljlua_assign_table_to_record(L, schema_ud->schema, record, 2);
    status = ljlua_serialize_path_with_options(&schema_ud->schema->map, record,
                                               path, options, &error);
    ljlua_cleanup_record_storage(schema_ud->schema, record);
    ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
  }
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return ljlua_push_error(L, &error);
  }
  lua_pushboolean(L, 1);
  return 1;
}

static int ljlua_schema_write_file(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  FILE *fp = ljlua_check_file(L, 3);
  const lonejson_runtime *runtime;
  const lonejson__write_options *options;
  lonejson_error error;
  lonejson_status status;

  runtime = lonejson__runtime_const(schema_ud->schema->runtime);
  ljlua_reject_legacy_options(L, 4, "schema:write_file");
  options = &runtime->write_options;
  if (luaL_testudata(L, 2, LJLUA_RECORD_MT) != NULL) {
    ljlua_record_ud *record_ud = ljlua_check_record(L, 2);
    status = ljlua_serialize_filep_with_options(
        &schema_ud->schema->map, record_ud->data, fp, options, &error);
  } else {
    int owned_record;
    unsigned char *record =
        ljlua_schema_borrow_scratch(schema_ud->schema, &owned_record);
    if (record == NULL) {
      return luaL_error(L, "failed to allocate encode buffer");
    }
    ljlua_prepare_record_storage(schema_ud->schema, record);
    ljlua_assign_table_to_record(L, schema_ud->schema, record, 2);
    status = ljlua_serialize_filep_with_options(&schema_ud->schema->map, record,
                                                fp, options, &error);
    ljlua_cleanup_record_storage(schema_ud->schema, record);
    ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
  }
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return ljlua_push_error(L, &error);
  }
  lua_pushboolean(L, 1);
  return 1;
}

static int ljlua_schema_write_fd(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  int fd = ljlua_check_fd_like(L, 3);
  FILE *fp = NULL;
  const lonejson_runtime *runtime;
  const lonejson__write_options *options;
  lonejson_error error;
  lonejson_status status;

  runtime = lonejson__runtime_const(schema_ud->schema->runtime);
  ljlua_reject_legacy_options(L, 4, "schema:write_fd");
  options = &runtime->write_options;
  fp = ljlua_dup_fd_to_file(L, fd, "wb");
  if (luaL_testudata(L, 2, LJLUA_RECORD_MT) != NULL) {
    ljlua_record_ud *record_ud = ljlua_check_record(L, 2);
    status = ljlua_serialize_filep_with_options(
        &schema_ud->schema->map, record_ud->data, fp, options, &error);
  } else {
    int owned_record;
    unsigned char *record =
        ljlua_schema_borrow_scratch(schema_ud->schema, &owned_record);
    if (record == NULL) {
      return luaL_error(L, "failed to allocate encode buffer");
    }
    ljlua_prepare_record_storage(schema_ud->schema, record);
    ljlua_assign_table_to_record(L, schema_ud->schema, record, 2);
    status = ljlua_serialize_filep_with_options(&schema_ud->schema->map, record,
                                                fp, options, &error);
    ljlua_cleanup_record_storage(schema_ud->schema, record);
    ljlua_schema_release_scratch(schema_ud->schema, record, owned_record);
  }
  fclose(fp);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return ljlua_push_error(L, &error);
  }
  lua_pushboolean(L, 1);
  return 1;
}

static int ljlua_stream_gc(lua_State *L) {
  ljlua_stream_ud *ud = ljlua_check_stream(L, 1);
  if (ud->stream != NULL) {
    lonejson_stream_close(ud->stream);
    ud->stream = NULL;
  }
  ud->input = NULL;
  ud->input_len = 0u;
  ud->input_offset = 0u;
  return 0;
}

static int ljlua_schema_stream_path(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  const char *path = luaL_checkstring(L, 2);
  lonejson__parse_options options;
  const lonejson_runtime *runtime;
  lonejson_error error;
  ljlua_stream_ud *ud;

  runtime = lonejson__runtime_const(schema_ud->schema->runtime);
  ljlua_reject_legacy_options(L, 3, "schema:stream_path");
  options = runtime->parse_options;
  ud = (ljlua_stream_ud *)ljlua_newuserdata_slots(L, sizeof(*ud), 2);
  memset(ud, 0, sizeof(*ud));
  ud->clear_destination = options.clear_destination ? 1 : 0;
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    options.clear_destination = 0;
  }
  ud->magic = LJLUA_STREAM_MAGIC;
  ud->stream = ljlua_stream_open_path_with_options(
      &schema_ud->schema->map, path, &options, runtime, &error);
  if (ud->stream == NULL) {
    return ljlua_push_error(L, &error);
  }
  ud->schema = schema_ud->schema;
  ljlua_set_uservalue_ref(L, -1, 1, 1);
  luaL_setmetatable(L, LJLUA_STREAM_MT);
  return 1;
}

static int ljlua_schema_stream_fd(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  int fd = ljlua_check_fd_like(L, 2);
  lonejson__parse_options options;
  const lonejson_runtime *runtime;
  lonejson_error error;
  ljlua_stream_ud *ud;

  runtime = lonejson__runtime_const(schema_ud->schema->runtime);
  ljlua_reject_legacy_options(L, 3, "schema:stream_fd");
  options = runtime->parse_options;
  ud = (ljlua_stream_ud *)ljlua_newuserdata_slots(L, sizeof(*ud), 2);
  memset(ud, 0, sizeof(*ud));
  ud->clear_destination = options.clear_destination ? 1 : 0;
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    options.clear_destination = 0;
  }
  ud->magic = LJLUA_STREAM_MAGIC;
  ud->stream = ljlua_stream_open_fd_with_options(&schema_ud->schema->map, fd,
                                                 &options, runtime, &error);
  if (ud->stream == NULL) {
    return ljlua_push_error(L, &error);
  }
  ud->schema = schema_ud->schema;
  ljlua_set_uservalue_ref(L, -1, 1, 1);
  luaL_setmetatable(L, LJLUA_STREAM_MT);
  return 1;
}

static int ljlua_schema_stream_file(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  FILE *fp = ljlua_check_file(L, 2);
  lonejson__parse_options options;
  const lonejson_runtime *runtime;
  lonejson_error error;
  ljlua_stream_ud *ud;

  runtime = lonejson__runtime_const(schema_ud->schema->runtime);
  ljlua_reject_legacy_options(L, 3, "schema:stream_file");
  options = runtime->parse_options;
  ud = (ljlua_stream_ud *)ljlua_newuserdata_slots(L, sizeof(*ud), 2);
  memset(ud, 0, sizeof(*ud));
  ud->clear_destination = options.clear_destination ? 1 : 0;
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    options.clear_destination = 0;
  }
  ud->magic = LJLUA_STREAM_MAGIC;
  ud->stream = ljlua_stream_open_filep_with_options(&schema_ud->schema->map, fp,
                                                    &options, runtime, &error);
  if (ud->stream == NULL) {
    return ljlua_push_error(L, &error);
  }
  ud->schema = schema_ud->schema;
  ljlua_set_uservalue_ref(L, -1, 1, 1);
  luaL_setmetatable(L, LJLUA_STREAM_MT);
  return 1;
}
