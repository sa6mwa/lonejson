static int ljlua_parse_parse_options(lua_State *L, int index,
                                     lonejson_parse_options *options) {
  *options = lonejson_default_parse_options();
  if (index == 0 || lua_isnoneornil(L, index)) {
    return 1;
  }
  luaL_checktype(L, index, LUA_TTABLE);
  lua_getfield(L, index, "clear_destination");
  if (!lua_isnil(L, -1)) {
    options->clear_destination = lua_toboolean(L, -1);
  }
  lua_pop(L, 1);
  lua_getfield(L, index, "reject_duplicate_keys");
  if (!lua_isnil(L, -1)) {
    options->reject_duplicate_keys = lua_toboolean(L, -1);
  }
  lua_pop(L, 1);
  lua_getfield(L, index, "max_depth");
  if (lua_isinteger(L, -1) && lua_tointeger(L, -1) > 0) {
    options->max_depth = (size_t)lua_tointeger(L, -1);
  }
  lua_pop(L, 1);
  return 1;
}

static int ljlua_schema_compile_path(lua_State *L) {
  ljlua_schema_ud *schema_ud;
  const char *path;
  ljlua_compiled_path *compiled;
  ljlua_path_ud *path_ud;

  schema_ud = ljlua_check_schema(L, 1);
  path = luaL_checkstring(L, 2);
  compiled = ljlua_find_compiled_path(schema_ud->schema, path);
  if (compiled == NULL) {
    compiled = ljlua_compile_path(L, schema_ud->schema, path);
  }
  path_ud = (ljlua_path_ud *)lua_newuserdatauv(L, sizeof(*path_ud), 0);
  memset(path_ud, 0, sizeof(*path_ud));
  path_ud->magic = LJLUA_PATH_MAGIC;
  path_ud->schema = schema_ud->schema;
  path_ud->compiled = compiled;
  lua_pushvalue(L, 1);
  path_ud->schema_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  luaL_setmetatable(L, LJLUA_PATH_MT);
  return 1;
}

static int ljlua_schema_compile_get(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  const char *path = luaL_checkstring(L, 2);
  ljlua_path_ud *path_ud;
  ljlua_compiled_path *compiled;

  compiled = ljlua_find_compiled_path(schema_ud->schema, path);
  if (compiled == NULL) {
    compiled = ljlua_compile_path(L, schema_ud->schema, path);
  }
  path_ud = (ljlua_path_ud *)lua_newuserdatauv(L, sizeof(*path_ud), 0);
  memset(path_ud, 0, sizeof(*path_ud));
  path_ud->magic = LJLUA_PATH_MAGIC;
  path_ud->schema = schema_ud->schema;
  path_ud->compiled = compiled;
  lua_pushvalue(L, 1);
  path_ud->schema_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  luaL_setmetatable(L, LJLUA_PATH_MT);
  lua_pushvalue(L, -1);
  lua_pushcclosure(L, ljlua_getter_call, 1);
  return 1;
}

static int ljlua_schema_compile_count(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  const char *path = luaL_checkstring(L, 2);
  ljlua_path_ud *path_ud;
  ljlua_compiled_path *compiled;

  compiled = ljlua_find_compiled_path(schema_ud->schema, path);
  if (compiled == NULL) {
    compiled = ljlua_compile_path(L, schema_ud->schema, path);
  }
  path_ud = (ljlua_path_ud *)lua_newuserdatauv(L, sizeof(*path_ud), 0);
  memset(path_ud, 0, sizeof(*path_ud));
  path_ud->magic = LJLUA_PATH_MAGIC;
  path_ud->schema = schema_ud->schema;
  path_ud->compiled = compiled;
  lua_pushvalue(L, 1);
  path_ud->schema_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  luaL_setmetatable(L, LJLUA_PATH_MT);
  lua_pushvalue(L, -1);
  lua_pushcclosure(L, ljlua_counter_call, 1);
  return 1;
}

static const lonejson_write_options *
ljlua_get_write_options(lua_State *L, int index,
                        lonejson_write_options *options) {
  if (index == 0 || lua_isnoneornil(L, index)) {
    return NULL;
  }
  luaL_checktype(L, index, LUA_TTABLE);
  *options = lonejson_default_write_options();
  lua_getfield(L, index, "overflow");
  if (!lua_isnil(L, -1)) {
    const char *name = luaL_checkstring(L, -1);
    if (strcmp(name, "fail") == 0) {
      options->overflow_policy = LONEJSON_OVERFLOW_FAIL;
    } else if (strcmp(name, "truncate") == 0) {
      options->overflow_policy = LONEJSON_OVERFLOW_TRUNCATE;
    } else if (strcmp(name, "truncate_silent") == 0) {
      options->overflow_policy = LONEJSON_OVERFLOW_TRUNCATE_SILENT;
    } else {
      luaL_error(L, "unsupported overflow policy '%s'", name);
    }
  }
  lua_pop(L, 1);
  lua_getfield(L, index, "pretty");
  if (!lua_isnil(L, -1)) {
    options->pretty = lua_toboolean(L, -1) ? 1 : 0;
  }
  lua_pop(L, 1);
  lua_getfield(L, index, "max_output_bytes");
  if (!lua_isnil(L, -1)) {
    lua_Integer max_output = luaL_checkinteger(L, -1);
    luaL_argcheck(L, max_output >= 0, index,
                  "max_output_bytes must be non-negative");
    options->max_output_bytes = (size_t)max_output;
  }
  lua_pop(L, 1);
  return options;
}

static int ljlua_push_spool(lua_State *L, lonejson_spooled *spool, int kind,
                            int owner_index, int copy_value) {
  ljlua_spool_ud *ud;
  lonejson_error error;
  unsigned char buffer[4096];

  ud = (ljlua_spool_ud *)lua_newuserdatauv(L, sizeof(*ud), 0);
  memset(ud, 0, sizeof(*ud));
  ud->magic = LJLUA_SPOOL_MAGIC;
  luaL_setmetatable(L, LJLUA_SPOOL_MT);
  ud->kind = kind;
  ud->owner_ref = LUA_NOREF;
  if (!copy_value) {
    ud->spool = spool;
    if (owner_index != 0) {
      lua_pushvalue(L, owner_index);
      ud->owner_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    return 1;
  }
  lonejson_spooled_init(&ud->owned, NULL);
  if (spool->memory_limit != 0u || spool->max_bytes != 0u ||
      spool->temp_dir != NULL) {
    lonejson_spool_options opts;
    opts.memory_limit = spool->memory_limit;
    opts.max_bytes = spool->max_bytes;
    opts.temp_dir = spool->temp_dir;
    lonejson_spooled_init(&ud->owned, &opts);
  }
  if (lonejson_spooled_rewind(spool, &error) != LONEJSON_STATUS_OK) {
    return luaL_error(L, "failed to rewind spool: %s", error.message);
  }
  for (;;) {
    lonejson_read_result rr =
        lonejson_spooled_read(spool, buffer, sizeof(buffer));
    if (rr.error_code != 0) {
      return luaL_error(L, "failed to clone spool");
    }
    if (rr.bytes_read != 0u &&
        lonejson__spooled_append(&ud->owned, buffer, rr.bytes_read, &error) !=
            LONEJSON_STATUS_OK) {
      return luaL_error(L, "failed to clone spool: %s", error.message);
    }
    if (rr.eof) {
      break;
    }
  }
  lonejson_spooled_rewind(spool, &error);
  ud->spool = &ud->owned;
  ud->owns_data = 1;
  return 1;
}

static int ljlua_push_value(lua_State *L, ljlua_schema *schema, void *record,
                            ljlua_field_meta *meta, int owner_index);

static int ljlua_schema_has_json_value(ljlua_schema *schema) {
  return schema != NULL && schema->has_json_value;
}

static void ljlua_decode_context_cleanup(ljlua_decode_context *ctx) {
  size_t i;

  if (ctx == NULL || ctx->json_values == NULL) {
    return;
  }
  for (i = 0u; i < ctx->schema->field_count; ++i) {
    if (ctx->json_values[i].active) {
      ljlua_json_builder_cleanup(ctx->L, &ctx->json_values[i].builder);
    }
  }
  free(ctx->json_values);
  ctx->json_values = NULL;
}

static void ljlua_array_stream_clear_pending(lua_State *L,
                                             ljlua_array_stream_ud *ud) {
  if (ud == NULL || ud->pending_record == NULL) {
    return;
  }
  ud->pending_ctx.L = L;
  ljlua_decode_context_cleanup(&ud->pending_ctx);
  ljlua_cleanup_record_storage(ud->schema, ud->pending_record);
  free(ud->pending_record);
  ud->pending_record = NULL;
  memset(&ud->pending_ctx, 0, sizeof(ud->pending_ctx));
}

static int ljlua_decode_context_prepare_table_mode(lua_State *L,
                                                   ljlua_schema *schema,
                                                   void *record,
                                                   ljlua_decode_context *ctx) {
  size_t i;

  memset(ctx, 0, sizeof(*ctx));
  ctx->L = L;
  ctx->schema = schema;
  ctx->json_values = (ljlua_json_value_decode_state *)calloc(
      schema->field_count, sizeof(*ctx->json_values));
  if (ctx->json_values == NULL) {
    return luaL_error(L, "failed to allocate Lua JSON decode state");
  }
  for (i = 0u; i < schema->field_count; ++i) {
    if (schema->metas[i].lua_kind == LJLUA_FIELD_JSON_VALUE) {
      lonejson_json_value *value =
          (lonejson_json_value *)((unsigned char *)record +
                                  schema->metas[i].field.struct_offset);
      ljlua_json_builder_init(&ctx->json_values[i].builder, L);
      memset(&ctx->json_values[i].visitor, 0,
             sizeof(ctx->json_values[i].visitor));
      ctx->json_values[i].visitor.object_begin = ljlua_json_value_object_begin;
      ctx->json_values[i].visitor.object_end = ljlua_json_value_object_end;
      ctx->json_values[i].visitor.object_key_begin = ljlua_json_value_key_begin;
      ctx->json_values[i].visitor.object_key_chunk = ljlua_json_value_key_chunk;
      ctx->json_values[i].visitor.object_key_end = ljlua_json_value_key_end;
      ctx->json_values[i].visitor.array_begin = ljlua_json_value_array_begin;
      ctx->json_values[i].visitor.array_end = ljlua_json_value_array_end;
      ctx->json_values[i].visitor.string_begin = ljlua_json_value_string_begin;
      ctx->json_values[i].visitor.string_chunk = ljlua_json_value_string_chunk;
      ctx->json_values[i].visitor.string_end = ljlua_json_value_string_end;
      ctx->json_values[i].visitor.number_begin = ljlua_json_value_number_begin;
      ctx->json_values[i].visitor.number_chunk = ljlua_json_value_number_chunk;
      ctx->json_values[i].visitor.number_end = ljlua_json_value_number_end;
      ctx->json_values[i].visitor.boolean_value = ljlua_json_value_boolean;
      ctx->json_values[i].visitor.null_value = ljlua_json_value_null;
      ctx->json_values[i].active = 1;
      if (lonejson_json_value_set_parse_visitor(
              value, &ctx->json_values[i].visitor, &ctx->json_values[i], NULL,
              NULL) != LONEJSON_STATUS_OK) {
        return luaL_error(L, "failed to configure Lua JSON parse visitor");
      }
    }
  }
  return 1;
}

static void ljlua_json_value_enable_parse_capture_preserving_runtime(
    lonejson_json_value *value) {
  if (value == NULL) {
    return;
  }
  value->parse_mode = LONEJSON_JSON_VALUE_PARSE_CAPTURE;
  value->parse_sink = NULL;
  value->parse_sink_user = NULL;
  value->parse_visitor = NULL;
  value->parse_visitor_user = NULL;
  value->parse_visitor_limits = lonejson_default_value_limits();
}

static int ljlua_prepare_record_json_value_capture(lua_State *L,
                                                   ljlua_schema *schema,
                                                   void *record,
                                                   int preserve_runtime) {
  size_t i;
  lonejson_error error;

  for (i = 0u; i < schema->field_count; ++i) {
    if (schema->metas[i].lua_kind == LJLUA_FIELD_JSON_VALUE) {
      lonejson_json_value *value =
          (lonejson_json_value *)((unsigned char *)record +
                                  schema->metas[i].field.struct_offset);
      if (preserve_runtime) {
        ljlua_json_value_enable_parse_capture_preserving_runtime(value);
      } else if (lonejson_json_value_enable_parse_capture(value, &error) !=
                 LONEJSON_STATUS_OK) {
        return luaL_error(L, "failed to enable JSON value capture: %s",
                          error.message);
      }
    }
  }
  return 1;
}

static int ljlua_record_to_table(lua_State *L, ljlua_schema *schema,
                                 void *record) {
  size_t i;

  lua_createtable(L, 0, (int)schema->field_count);
  for (i = 0u; i < schema->field_count; ++i) {
    ljlua_push_value(L, schema, record, &schema->metas[i], 0);
    lua_setfield(L, -2, schema->metas[i].name);
  }
  return 1;
}

static int
ljlua_record_to_table_with_overrides(lua_State *L, ljlua_schema *schema,
                                     void *record,
                                     const ljlua_decode_context *ctx) {
  size_t i;
  lua_createtable(L, 0, (int)schema->field_count);
  for (i = 0u; i < schema->field_count; ++i) {
    if (ctx != NULL && ctx->json_values != NULL && ctx->json_values[i].active) {
      int ref = ctx->json_values[i].builder.root_ref;
      if (ref == LUA_NOREF || ref == LUA_REFNIL) {
        lua_pushnil(L);
      } else {
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
      }
    } else {
      ljlua_push_value(L, schema, record, &schema->metas[i], 0);
    }
    lua_setfield(L, -2, schema->metas[i].name);
  }
  return 1;
}

static int ljlua_push_value(lua_State *L, ljlua_schema *schema, void *record,
                            ljlua_field_meta *meta, int owner_index) {
  void *ptr;
  size_t i;

  (void)schema;
  ptr = (unsigned char *)record + meta->field.struct_offset;
  switch (meta->lua_kind) {
  case LJLUA_FIELD_STRING:
    if (meta->field.storage == LONEJSON_STORAGE_DYNAMIC) {
      const char *text = *(const char *const *)ptr;
      if (text == NULL) {
        lua_pushnil(L);
      } else {
        lua_pushstring(L, text);
      }
    } else {
      lua_pushstring(L, (const char *)ptr);
    }
    return 1;
  case LJLUA_FIELD_SPOOLED_TEXT:
    return ljlua_push_spool(L, (lonejson_spooled *)ptr, meta->lua_kind,
                            owner_index, owner_index == 0);
  case LJLUA_FIELD_SPOOLED_BYTES:
    return ljlua_push_spool(L, (lonejson_spooled *)ptr, meta->lua_kind,
                            owner_index, owner_index == 0);
  case LJLUA_FIELD_JSON_VALUE:
    return ljlua_push_json_value(L, (const lonejson_json_value *)ptr);
  case LJLUA_FIELD_I64:
    lua_pushinteger(L, (lua_Integer) * (lonejson_int64 *)ptr);
    return 1;
  case LJLUA_FIELD_U64:
    return ljlua_push_u64(L, *(lonejson_uint64 *)ptr);
  case LJLUA_FIELD_F64:
    lua_pushnumber(L, *(double *)ptr);
    return 1;
  case LJLUA_FIELD_BOOL:
    lua_pushboolean(L, *(bool *)ptr);
    return 1;
  case LJLUA_FIELD_OBJECT:
    return ljlua_record_to_table(L, meta->subschema, ptr);
  case LJLUA_FIELD_STRING_ARRAY: {
    lonejson_string_array *arr = (lonejson_string_array *)ptr;
    lua_createtable(L, (int)arr->count, 0);
    for (i = 0u; i < arr->count; ++i) {
      lua_pushstring(L, arr->items[i] ? arr->items[i] : "");
      lua_rawseti(L, -2, (lua_Integer)i + 1);
    }
    return 1;
  }
  case LJLUA_FIELD_I64_ARRAY: {
    lonejson_i64_array *arr = (lonejson_i64_array *)ptr;
    lua_createtable(L, (int)arr->count, 0);
    for (i = 0u; i < arr->count; ++i) {
      lua_pushinteger(L, (lua_Integer)arr->items[i]);
      lua_rawseti(L, -2, (lua_Integer)i + 1);
    }
    return 1;
  }
  case LJLUA_FIELD_U64_ARRAY: {
    lonejson_u64_array *arr = (lonejson_u64_array *)ptr;
    lua_createtable(L, (int)arr->count, 0);
    for (i = 0u; i < arr->count; ++i) {
      ljlua_push_u64(L, arr->items[i]);
      lua_rawseti(L, -2, (lua_Integer)i + 1);
    }
    return 1;
  }
  case LJLUA_FIELD_F64_ARRAY: {
    lonejson_f64_array *arr = (lonejson_f64_array *)ptr;
    lua_createtable(L, (int)arr->count, 0);
    for (i = 0u; i < arr->count; ++i) {
      lua_pushnumber(L, arr->items[i]);
      lua_rawseti(L, -2, (lua_Integer)i + 1);
    }
    return 1;
  }
  case LJLUA_FIELD_BOOL_ARRAY: {
    lonejson_bool_array *arr = (lonejson_bool_array *)ptr;
    lua_createtable(L, (int)arr->count, 0);
    for (i = 0u; i < arr->count; ++i) {
      lua_pushboolean(L, arr->items[i]);
      lua_rawseti(L, -2, (lua_Integer)i + 1);
    }
    return 1;
  }
  case LJLUA_FIELD_OBJECT_ARRAY: {
    lonejson_object_array *arr = (lonejson_object_array *)ptr;
    lua_createtable(L, (int)arr->count, 0);
    for (i = 0u; i < arr->count; ++i) {
      void *elem =
          (unsigned char *)arr->items + (i * meta->subschema->record_size);
      ljlua_record_to_table(L, meta->subschema, elem);
      lua_rawseti(L, -2, (lua_Integer)i + 1);
    }
    return 1;
  }
  default:
    lua_pushnil(L);
    return 1;
  }
}

static void ljlua_cleanup_record_storage(ljlua_schema *schema, void *record) {
  if (schema == NULL || record == NULL) {
    return;
  }
  lonejson__cleanup_map(&schema->map, record);
}

static ljlua_field_meta *ljlua_find_meta(ljlua_schema *schema,
                                         const char *name) {
  size_t i;

  for (i = 0u; i < schema->field_count; ++i) {
    if (strcmp(schema->metas[i].name, name) == 0) {
      return &schema->metas[i];
    }
  }
  return NULL;
}

static ljlua_compiled_path *ljlua_find_compiled_path(ljlua_schema *schema,
                                                     const char *path) {
  ljlua_compiled_path *compiled;

  compiled = schema->path_cache;
  while (compiled != NULL) {
    if (strcmp(compiled->path, path) == 0) {
      return compiled;
    }
    compiled = compiled->next;
  }
  return NULL;
}

static ljlua_compiled_path *
ljlua_compile_path(lua_State *L, ljlua_schema *schema, const char *path) {
  const char *cursor;
  ljlua_schema *current_schema;
  ljlua_compiled_path *compiled;
  size_t capacity;

  cursor = path;
  current_schema = schema;
  compiled = (ljlua_compiled_path *)calloc(1u, sizeof(*compiled));
  if (compiled == NULL) {
    luaL_error(L, "failed to allocate record path cache");
  }
  compiled->path = ljlua_strdup(path);
  capacity = 4u;
  compiled->steps =
      (ljlua_path_step *)calloc(capacity, sizeof(compiled->steps[0]));
  if (compiled->steps == NULL) {
    free(compiled->path);
    free(compiled);
    luaL_error(L, "failed to allocate record path steps");
  }

  while (*cursor != '\0') {
    char segment[128];
    size_t seg_len;
    ljlua_field_meta *meta;
    size_t index;

    seg_len = 0u;
    while (*cursor != '\0' && *cursor != '.' && *cursor != '[') {
      if (seg_len + 1u >= sizeof(segment)) {
        luaL_error(L, "path segment too long");
      }
      segment[seg_len++] = *cursor++;
    }
    if (seg_len == 0u) {
      luaL_error(L, "malformed record path '%s'", path);
    }
    segment[seg_len] = '\0';
    meta = ljlua_find_meta(current_schema, segment);
    if (meta == NULL) {
      luaL_error(L, "unknown field '%s' in path '%s'", segment, path);
    }
    index = 0u;
    if (*cursor == '[') {
      ++cursor;
      while (*cursor >= '0' && *cursor <= '9') {
        index = index * 10u + (size_t)(*cursor - '0');
        ++cursor;
      }
      if (*cursor != ']') {
        luaL_error(L, "malformed array index in path '%s'", path);
      }
      ++cursor;
    }
    if (compiled->step_count == capacity) {
      ljlua_path_step *grown;
      capacity *= 2u;
      grown = (ljlua_path_step *)realloc(compiled->steps,
                                         capacity * sizeof(compiled->steps[0]));
      if (grown == NULL) {
        luaL_error(L, "failed to grow record path cache");
      }
      compiled->steps = grown;
    }
    compiled->steps[compiled->step_count].meta = meta;
    compiled->steps[compiled->step_count].index = index;
    ++compiled->step_count;

    if (*cursor == '.') {
      if (index != 0u) {
        if (meta->lua_kind != LJLUA_FIELD_OBJECT_ARRAY) {
          luaL_error(L, "field '%s' is not an object array in path '%s'",
                     segment, path);
        }
      } else if (meta->lua_kind != LJLUA_FIELD_OBJECT) {
        luaL_error(L, "field '%s' is not an object in path '%s'", segment,
                   path);
      }
      current_schema = meta->subschema;
      ++cursor;
      continue;
    }
    if (*cursor != '\0') {
      luaL_error(L, "malformed record path '%s'", path);
    }
    if (index != 0u && meta->lua_kind != LJLUA_FIELD_OBJECT_ARRAY &&
        meta->lua_kind != LJLUA_FIELD_STRING_ARRAY &&
        meta->lua_kind != LJLUA_FIELD_I64_ARRAY &&
        meta->lua_kind != LJLUA_FIELD_U64_ARRAY &&
        meta->lua_kind != LJLUA_FIELD_F64_ARRAY &&
        meta->lua_kind != LJLUA_FIELD_BOOL_ARRAY) {
      luaL_error(L, "field '%s' is not an array in path '%s'", segment, path);
    }
  }

  compiled->next = schema->path_cache;
  schema->path_cache = compiled;
  return compiled;
}

static int ljlua_push_compiled_path_value(lua_State *L, void *record,
                                          ljlua_compiled_path *compiled,
                                          int owner_index) {
  size_t i;
  ljlua_schema *schema;

  schema = NULL;
  for (i = 0u; i < compiled->step_count; ++i) {
    ljlua_field_meta *meta = compiled->steps[i].meta;
    size_t index = compiled->steps[i].index;
    void *ptr = (unsigned char *)record + meta->field.struct_offset;
    int final = (i + 1u == compiled->step_count);

    (void)schema;
    if (index != 0u) {
      switch (meta->lua_kind) {
      case LJLUA_FIELD_STRING_ARRAY: {
        lonejson_string_array *arr = (lonejson_string_array *)ptr;
        if (index > arr->count || arr->items[index - 1u] == NULL) {
          lua_pushnil(L);
        } else {
          lua_pushstring(L, arr->items[index - 1u]);
        }
        return 1;
      }
      case LJLUA_FIELD_I64_ARRAY: {
        lonejson_i64_array *arr = (lonejson_i64_array *)ptr;
        if (index > arr->count) {
          lua_pushnil(L);
        } else {
          lua_pushinteger(L, (lua_Integer)arr->items[index - 1u]);
        }
        return 1;
      }
      case LJLUA_FIELD_U64_ARRAY: {
        lonejson_u64_array *arr = (lonejson_u64_array *)ptr;
        if (index > arr->count) {
          lua_pushnil(L);
        } else {
          lua_pushinteger(L, (lua_Integer)arr->items[index - 1u]);
        }
        return 1;
      }
      case LJLUA_FIELD_F64_ARRAY: {
        lonejson_f64_array *arr = (lonejson_f64_array *)ptr;
        if (index > arr->count) {
          lua_pushnil(L);
        } else {
          lua_pushnumber(L, arr->items[index - 1u]);
        }
        return 1;
      }
      case LJLUA_FIELD_BOOL_ARRAY: {
        lonejson_bool_array *arr = (lonejson_bool_array *)ptr;
        if (index > arr->count) {
          lua_pushnil(L);
        } else {
          lua_pushboolean(L, arr->items[index - 1u]);
        }
        return 1;
      }
      case LJLUA_FIELD_OBJECT_ARRAY: {
        lonejson_object_array *arr = (lonejson_object_array *)ptr;
        if (index > arr->count) {
          lua_pushnil(L);
          return 1;
        }
        record = (unsigned char *)arr->items +
                 ((index - 1u) * meta->subschema->record_size);
        if (final) {
          return ljlua_record_to_table(L, meta->subschema, record);
        }
        break;
      }
      default:
        lua_pushnil(L);
        return 1;
      }
    } else {
      if (final) {
        return ljlua_push_value(L, NULL, record, meta, owner_index);
      }
      record = ptr;
    }
  }
  lua_pushnil(L);
  return 1;
}

static int ljlua_assign_spool_from_lua(lua_State *L, lonejson_spooled *spool,
                                       int kind, int index) {
  size_t len;
  const char *data;
  lonejson_error error;

  lonejson_spooled_reset(spool);
  if (lua_isuserdata(L, index)) {
    ljlua_spool_ud *src =
        (ljlua_spool_ud *)luaL_testudata(L, index, LJLUA_SPOOL_MT);
    unsigned char buffer[4096];

    if (src == NULL) {
      return luaL_error(L, "expected string or lonejson spool");
    }
    if (lonejson_spooled_rewind(src->spool, &error) != LONEJSON_STATUS_OK) {
      return luaL_error(L, "%s", error.message);
    }
    for (;;) {
      lonejson_read_result rr =
          lonejson_spooled_read(src->spool, buffer, sizeof(buffer));
      if (rr.error_code != 0) {
        return luaL_error(L, "failed to read spool source");
      }
      if (rr.bytes_read != 0u &&
          lonejson__spooled_append(spool, buffer, rr.bytes_read, &error) !=
              LONEJSON_STATUS_OK) {
        return luaL_error(L, "%s", error.message);
      }
      if (rr.eof) {
        break;
      }
    }
    return 1;
  }
  data = luaL_checklstring(L, index, &len);
  if (kind == LJLUA_FIELD_SPOOLED_TEXT) {
    if (lonejson__spooled_append(spool, (const unsigned char *)data, len,
                                 &error) != LONEJSON_STATUS_OK) {
      return luaL_error(L, "%s", error.message);
    }
  } else {
    if (lonejson__spooled_append(spool, (const unsigned char *)data, len,
                                 &error) != LONEJSON_STATUS_OK) {
      return luaL_error(L, "%s", error.message);
    }
  }
  return 1;
}

static int ljlua_assign_table_to_record(lua_State *L, ljlua_schema *schema,
                                        void *record, int table_index);

static void ljlua_init_helper_parser(lonejson_parser *parser) {
  static const lonejson_map helper_map = {"lua-helper", 0u, NULL, 0u};
  static unsigned char workspace[LONEJSON_PARSER_BUFFER_SIZE];

  lonejson__parser_init_state(parser, &helper_map, NULL, NULL, 0, workspace,
                              sizeof(workspace));
}

static int ljlua_assign_field_from_lua(lua_State *L, ljlua_schema *schema,
                                       void *record, ljlua_field_meta *meta,
                                       int value_index) {
  void *ptr;
  size_t i;

  (void)schema;
  ptr = (unsigned char *)record + meta->field.struct_offset;
  if (lua_isnil(L, value_index)) {
    lonejson_parser parser;

    ljlua_init_helper_parser(&parser);
    if (lonejson__assign_null(&parser, &meta->field, ptr) ==
        LONEJSON_STATUS_OK) {
      return 1;
    }
  }
  switch (meta->lua_kind) {
  case LJLUA_FIELD_STRING: {
    size_t len;
    const char *text = luaL_checklstring(L, value_index, &len);
    lonejson_error error;
    lonejson_parser parser;
    unsigned char workspace[LONEJSON_PARSER_BUFFER_SIZE];

    lonejson__parser_init_state(&parser, &schema->map, record, NULL, 0,
                                workspace, sizeof(workspace));
    if (lonejson__assign_string(&parser, &meta->field, ptr, text, len) !=
        LONEJSON_STATUS_OK) {
      return luaL_error(L, "%s", parser.error.message);
    }
    (void)error;
    return 1;
  }
  case LJLUA_FIELD_SPOOLED_TEXT:
  case LJLUA_FIELD_SPOOLED_BYTES:
    return ljlua_assign_spool_from_lua(L, (lonejson_spooled *)ptr,
                                       meta->lua_kind, value_index);
  case LJLUA_FIELD_JSON_VALUE:
    return ljlua_assign_json_value_from_lua(L, (lonejson_json_value *)ptr,
                                            value_index);
  case LJLUA_FIELD_I64:
    *(lonejson_int64 *)ptr = (lonejson_int64)luaL_checkinteger(L, value_index);
    return 1;
  case LJLUA_FIELD_U64:
    *(lonejson_uint64 *)ptr = ljlua_check_u64(L, value_index);
    return 1;
  case LJLUA_FIELD_F64:
    *(double *)ptr = (double)luaL_checknumber(L, value_index);
    return 1;
  case LJLUA_FIELD_BOOL:
    *(bool *)ptr = lua_toboolean(L, value_index) ? true : false;
    return 1;
  case LJLUA_FIELD_OBJECT:
    luaL_checktype(L, value_index, LUA_TTABLE);
    return ljlua_assign_table_to_record(L, meta->subschema, ptr, value_index);
  case LJLUA_FIELD_STRING_ARRAY: {
    lonejson_string_array *arr = (lonejson_string_array *)ptr;
    lonejson_parser parser;
    luaL_checktype(L, value_index, LUA_TTABLE);
    ljlua_init_helper_parser(&parser);
    for (i = 1u; i <= (size_t)lua_rawlen(L, value_index); ++i) {
      size_t len;
      const char *text;

      lua_rawgeti(L, value_index, (lua_Integer)i);
      text = luaL_checklstring(L, -1, &len);
      if (lonejson__array_append_string(&parser, &meta->field, arr, text,
                                        len) != LONEJSON_STATUS_OK) {
        lua_pop(L, 1);
        return luaL_error(L, "%s", parser.error.message);
      }
      lua_pop(L, 1);
    }
    return 1;
  }
  case LJLUA_FIELD_I64_ARRAY: {
    lonejson_i64_array *arr = (lonejson_i64_array *)ptr;
    lonejson_parser parser;
    luaL_checktype(L, value_index, LUA_TTABLE);
    ljlua_init_helper_parser(&parser);
    for (i = 1u; i <= (size_t)lua_rawlen(L, value_index); ++i) {
      lua_rawgeti(L, value_index, (lua_Integer)i);
      if (lonejson__array_append_i64(
              &parser, &meta->field, arr,
              (lonejson_int64)luaL_checkinteger(L, -1)) != LONEJSON_STATUS_OK) {
        lua_pop(L, 1);
        return luaL_error(L, "%s", parser.error.message);
      }
      lua_pop(L, 1);
    }
    return 1;
  }
  case LJLUA_FIELD_U64_ARRAY: {
    lonejson_u64_array *arr = (lonejson_u64_array *)ptr;
    lonejson_parser parser;
    luaL_checktype(L, value_index, LUA_TTABLE);
    ljlua_init_helper_parser(&parser);
    for (i = 1u; i <= (size_t)lua_rawlen(L, value_index); ++i) {
      lua_rawgeti(L, value_index, (lua_Integer)i);
      if (lonejson__array_append_u64(&parser, &meta->field, arr,
                                     ljlua_check_u64(L, -1)) !=
          LONEJSON_STATUS_OK) {
        lua_pop(L, 1);
        return luaL_error(L, "%s", parser.error.message);
      }
      lua_pop(L, 1);
    }
    return 1;
  }
  case LJLUA_FIELD_F64_ARRAY: {
    lonejson_f64_array *arr = (lonejson_f64_array *)ptr;
    lonejson_parser parser;
    luaL_checktype(L, value_index, LUA_TTABLE);
    ljlua_init_helper_parser(&parser);
    for (i = 1u; i <= (size_t)lua_rawlen(L, value_index); ++i) {
      lua_rawgeti(L, value_index, (lua_Integer)i);
      if (lonejson__array_append_f64(&parser, &meta->field, arr,
                                     (double)luaL_checknumber(L, -1)) !=
          LONEJSON_STATUS_OK) {
        lua_pop(L, 1);
        return luaL_error(L, "%s", parser.error.message);
      }
      lua_pop(L, 1);
    }
    return 1;
  }
  case LJLUA_FIELD_BOOL_ARRAY: {
    lonejson_bool_array *arr = (lonejson_bool_array *)ptr;
    lonejson_parser parser;
    luaL_checktype(L, value_index, LUA_TTABLE);
    ljlua_init_helper_parser(&parser);
    for (i = 1u; i <= (size_t)lua_rawlen(L, value_index); ++i) {
      lua_rawgeti(L, value_index, (lua_Integer)i);
      if (lonejson__array_append_bool(&parser, &meta->field, arr,
                                      lua_toboolean(L, -1) ? true : false) !=
          LONEJSON_STATUS_OK) {
        lua_pop(L, 1);
        return luaL_error(L, "%s", parser.error.message);
      }
      lua_pop(L, 1);
    }
    return 1;
  }
  case LJLUA_FIELD_OBJECT_ARRAY: {
    lonejson_object_array *arr = (lonejson_object_array *)ptr;
    size_t n = (size_t)lua_rawlen(L, value_index);
    lonejson_parser parser;
    luaL_checktype(L, value_index, LUA_TTABLE);
    ljlua_init_helper_parser(&parser);
    for (i = 1u; i <= n; ++i) {
      void *slot;
      lua_rawgeti(L, value_index, (lua_Integer)i);
      slot = lonejson__object_array_append_slot(&parser, &meta->field, arr);
      if (slot == NULL) {
        lua_pop(L, 1);
        return luaL_error(L, "%s", parser.error.message);
      }
      ljlua_prepare_record_storage(meta->subschema, slot);
      ljlua_assign_table_to_record(L, meta->subschema, slot, lua_gettop(L));
      lua_pop(L, 1);
    }
    return 1;
  }
  default:
    return luaL_error(L, "unsupported assignment kind");
  }
}

static int ljlua_assign_table_to_record(lua_State *L, ljlua_schema *schema,
                                        void *record, int table_index) {
  size_t i;

  if (table_index < 0) {
    table_index = lua_gettop(L) + table_index + 1;
  }
  for (i = 0u; i < schema->field_count; ++i) {
    lua_getfield(L, table_index, schema->metas[i].name);
    if (!lua_isnil(L, -1)) {
      ljlua_assign_field_from_lua(L, schema, record, &schema->metas[i],
                                  lua_gettop(L));
    }
    lua_pop(L, 1);
  }
  return 1;
}
