static int ljlua_schema_stream_string(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  size_t len;
  const char *json = luaL_checklstring(L, 2, &len);
  lonejson_parse_options options;
  lonejson_error error;
  ljlua_stream_ud *ud;

  ljlua_parse_parse_options(L, 3, &options);
  ud = (ljlua_stream_ud *)lua_newuserdatauv(L, sizeof(*ud), 0);
  memset(ud, 0, sizeof(*ud));
  ud->clear_destination = options.clear_destination ? 1 : 0;
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    options.clear_destination = 0;
  }
  ud->magic = LJLUA_STREAM_MAGIC;
  ud->owned_input = (unsigned char *)malloc(len);
  if (ud->owned_input == NULL) {
    return luaL_error(L, "failed to allocate stream input");
  }
  memcpy(ud->owned_input, json, len);
  ud->owned_input_len = len;
  ud->stream = lonejson_stream_open_reader(
      &schema_ud->schema->map, ljlua_mem_stream_read, ud, &options, &error);
  if (ud->stream == NULL) {
    free(ud->owned_input);
    ud->owned_input = NULL;
    return ljlua_push_error(L, &error);
  }
  ud->schema = schema_ud->schema;
  lua_pushvalue(L, 1);
  ud->schema_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  luaL_setmetatable(L, LJLUA_STREAM_MT);
  return 1;
}

static int ljlua_array_stream_gc(lua_State *L) {
  ljlua_array_stream_ud *ud = ljlua_check_array_stream(L, 1);
  if (ud->stream != NULL) {
    lonejson_array_stream_close(ud->stream);
    ud->stream = NULL;
  }
  ljlua_array_stream_clear_pending(L, ud);
  free(ud->owned_input);
  ud->owned_input = NULL;
  ud->owned_input_len = 0u;
  ud->owned_input_offset = 0u;
  if (ud->schema_ref != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, ud->schema_ref);
    ud->schema_ref = LUA_NOREF;
  }
  return 0;
}

static int ljlua_array_stream_close(lua_State *L) {
  return ljlua_array_stream_gc(L);
}

static int ljlua_schema_array_stream_path(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  const char *array_path = luaL_checkstring(L, 2);
  const char *path = luaL_checkstring(L, 3);
  lonejson_parse_options options;
  lonejson_error error;
  ljlua_array_stream_ud *ud;

  ljlua_parse_parse_options(L, 4, &options);
  ud = (ljlua_array_stream_ud *)lua_newuserdatauv(L, sizeof(*ud), 0);
  memset(ud, 0, sizeof(*ud));
  ud->clear_destination = options.clear_destination ? 1 : 0;
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    options.clear_destination = 0;
  }
  ud->magic = LJLUA_ARRAY_STREAM_MAGIC;
  ud->stream =
      lonejson_array_stream_open_path(array_path, path, &options, &error);
  if (ud->stream == NULL) {
    return ljlua_push_error(L, &error);
  }
  ud->schema = schema_ud->schema;
  lua_pushvalue(L, 1);
  ud->schema_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  luaL_setmetatable(L, LJLUA_ARRAY_STREAM_MT);
  return 1;
}

static int ljlua_schema_array_stream_fd(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  const char *array_path = luaL_checkstring(L, 2);
  int fd = ljlua_check_fd_like(L, 3);
  lonejson_parse_options options;
  lonejson_error error;
  ljlua_array_stream_ud *ud;

  ljlua_parse_parse_options(L, 4, &options);
  ud = (ljlua_array_stream_ud *)lua_newuserdatauv(L, sizeof(*ud), 0);
  memset(ud, 0, sizeof(*ud));
  ud->clear_destination = options.clear_destination ? 1 : 0;
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    options.clear_destination = 0;
  }
  ud->magic = LJLUA_ARRAY_STREAM_MAGIC;
  ud->stream = lonejson_array_stream_open_fd(array_path, fd, &options, &error);
  if (ud->stream == NULL) {
    return ljlua_push_error(L, &error);
  }
  ud->schema = schema_ud->schema;
  lua_pushvalue(L, 1);
  ud->schema_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  luaL_setmetatable(L, LJLUA_ARRAY_STREAM_MT);
  return 1;
}

static int ljlua_schema_array_stream_file(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  const char *array_path = luaL_checkstring(L, 2);
  FILE *fp = ljlua_check_file(L, 3);
  lonejson_parse_options options;
  lonejson_error error;
  ljlua_array_stream_ud *ud;

  ljlua_parse_parse_options(L, 4, &options);
  ud = (ljlua_array_stream_ud *)lua_newuserdatauv(L, sizeof(*ud), 0);
  memset(ud, 0, sizeof(*ud));
  ud->clear_destination = options.clear_destination ? 1 : 0;
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    options.clear_destination = 0;
  }
  ud->magic = LJLUA_ARRAY_STREAM_MAGIC;
  ud->stream =
      lonejson_array_stream_open_filep(array_path, fp, &options, &error);
  if (ud->stream == NULL) {
    return ljlua_push_error(L, &error);
  }
  ud->schema = schema_ud->schema;
  lua_pushvalue(L, 1);
  ud->schema_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  luaL_setmetatable(L, LJLUA_ARRAY_STREAM_MT);
  return 1;
}

static int ljlua_schema_array_stream_string(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  const char *array_path = luaL_checkstring(L, 2);
  size_t len;
  const char *json = luaL_checklstring(L, 3, &len);
  lonejson_parse_options options;
  lonejson_error error;
  ljlua_array_stream_ud *ud;

  ljlua_parse_parse_options(L, 4, &options);
  ud = (ljlua_array_stream_ud *)lua_newuserdatauv(L, sizeof(*ud), 0);
  memset(ud, 0, sizeof(*ud));
  ud->clear_destination = options.clear_destination ? 1 : 0;
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    options.clear_destination = 0;
  }
  ud->magic = LJLUA_ARRAY_STREAM_MAGIC;
  ud->owned_input = (unsigned char *)malloc(len == 0u ? 1u : len);
  if (ud->owned_input == NULL) {
    return luaL_error(L, "failed to allocate array stream input");
  }
  if (len != 0u) {
    memcpy(ud->owned_input, json, len);
  }
  ud->owned_input_len = len;
  ud->stream = lonejson_array_stream_open_reader(
      array_path, ljlua_array_mem_stream_read, ud, &options, &error);
  if (ud->stream == NULL) {
    free(ud->owned_input);
    ud->owned_input = NULL;
    return ljlua_push_error(L, &error);
  }
  ud->schema = schema_ud->schema;
  lua_pushvalue(L, 1);
  ud->schema_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  luaL_setmetatable(L, LJLUA_ARRAY_STREAM_MT);
  return 1;
}

static int ljlua_stream_close(lua_State *L) { return ljlua_stream_gc(L); }

static lonejson_read_result
ljlua_mem_stream_read(void *user, unsigned char *buffer, size_t capacity) {
  ljlua_stream_ud *ud = (ljlua_stream_ud *)user;
  lonejson_read_result rr;
  size_t remaining;

  memset(&rr, 0, sizeof(rr));
  if (ud->owned_input_offset >= ud->owned_input_len) {
    rr.eof = 1;
    return rr;
  }
  remaining = ud->owned_input_len - ud->owned_input_offset;
  if (remaining > capacity) {
    remaining = capacity;
  }
  memcpy(buffer, ud->owned_input + ud->owned_input_offset, remaining);
  ud->owned_input_offset += remaining;
  rr.bytes_read = remaining;
  rr.eof = ud->owned_input_offset >= ud->owned_input_len;
  return rr;
}

static lonejson_read_result
ljlua_array_mem_stream_read(void *user, unsigned char *buffer,
                            size_t capacity) {
  ljlua_array_stream_ud *ud = (ljlua_array_stream_ud *)user;
  lonejson_read_result rr;
  size_t remaining;

  memset(&rr, 0, sizeof(rr));
  if (ud->owned_input_offset >= ud->owned_input_len) {
    rr.eof = 1;
    return rr;
  }
  remaining = ud->owned_input_len - ud->owned_input_offset;
  if (remaining > capacity) {
    remaining = capacity;
  }
  memcpy(buffer, ud->owned_input + ud->owned_input_offset, remaining);
  ud->owned_input_offset += remaining;
  rr.bytes_read = remaining;
  rr.eof = ud->owned_input_offset >= ud->owned_input_len;
  return rr;
}

static int ljlua_array_stream_next(lua_State *L) {
  ljlua_array_stream_ud *ud = ljlua_check_array_stream(L, 1);
  lonejson_error error;
  lonejson_array_stream_result result;
  ljlua_record_ud *record_ud = ljlua_test_record(L, 2);

  if (ud->pending_record != NULL && record_ud != NULL) {
    return luaL_error(
        L, "array stream has a pending table item; call next() without record");
  }
  if (record_ud != NULL) {
    if (record_ud->schema != ud->schema) {
      return luaL_error(L, "record belongs to a different schema");
    }
    if (ljlua_schema_has_json_value(ud->schema)) {
      if (ud->clear_destination) {
        lonejson_reset(&ud->schema->map, record_ud->data);
      }
      ljlua_prepare_record_json_value_capture(L, ud->schema, record_ud->data,
                                              ud->clear_destination ? 0 : 1);
    }
    result = lonejson_array_stream_next(ud->stream, &ud->schema->map,
                                        record_ud->data, &error);
    if (result == LONEJSON_ARRAY_STREAM_ITEM) {
      record_ud->cleared = 0;
      lua_pushvalue(L, 2);
      lua_pushnil(L);
      lua_pushstring(L, "item");
      return 3;
    }
  } else {
    unsigned char *record = ud->pending_record;
    ljlua_decode_context *ctx = &ud->pending_ctx;
    if (record == NULL) {
      record = (unsigned char *)calloc(1u, ud->schema->record_size);
      if (record == NULL) {
        return luaL_error(L, "failed to allocate decode buffer");
      }
      memset(ctx, 0, sizeof(*ctx));
      ljlua_prepare_record_storage(ud->schema, record);
      if (ljlua_schema_has_json_value(ud->schema)) {
        ljlua_decode_context_prepare_table_mode(L, ud->schema, record, ctx);
      }
      ud->pending_record = record;
    } else {
      ctx->L = L;
    }
    result =
        lonejson_array_stream_next(ud->stream, &ud->schema->map, record, &error);
    if (result == LONEJSON_ARRAY_STREAM_ITEM) {
      if (ljlua_schema_has_json_value(ud->schema)) {
        ljlua_record_to_table_with_overrides(L, ud->schema, record, ctx);
      } else {
        ljlua_record_to_table(L, ud->schema, record);
      }
      ljlua_array_stream_clear_pending(L, ud);
      lua_pushnil(L);
      lua_pushstring(L, "item");
      return 3;
    }
    if (result != LONEJSON_ARRAY_STREAM_WOULD_BLOCK) {
      ljlua_array_stream_clear_pending(L, ud);
    }
  }
  if (result == LONEJSON_ARRAY_STREAM_EOF) {
    lua_pushnil(L);
    lua_pushnil(L);
    lua_pushstring(L, "eof");
    return 3;
  }
  if (result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK) {
    lua_pushnil(L);
    lua_pushnil(L);
    lua_pushstring(L, "would_block");
    return 3;
  }
  lua_pushnil(L);
  ljlua_push_error(L, &error);
  lua_pushstring(L, "error");
  return 3;
}

static int ljlua_array_stream_next_value(lua_State *L) {
  ljlua_array_stream_ud *ud = ljlua_check_array_stream(L, 1);
  lonejson_json_value value;
  lonejson_error error;
  lonejson_array_stream_result result;

  lonejson_json_value_init(&value);
  result = lonejson_array_stream_next_value(ud->stream, &value, &error);
  if (result == LONEJSON_ARRAY_STREAM_ITEM) {
    ljlua_push_json_value(L, &value);
    lonejson_json_value_cleanup(&value);
    lua_pushnil(L);
    lua_pushstring(L, "item");
    return 3;
  }
  lonejson_json_value_cleanup(&value);
  if (result == LONEJSON_ARRAY_STREAM_EOF) {
    lua_pushnil(L);
    lua_pushnil(L);
    lua_pushstring(L, "eof");
    return 3;
  }
  if (result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK) {
    lua_pushnil(L);
    lua_pushnil(L);
    lua_pushstring(L, "would_block");
    return 3;
  }
  lua_pushnil(L);
  ljlua_push_error(L, &error);
  lua_pushstring(L, "error");
  return 3;
}

static int ljlua_stream_next(lua_State *L) {
  ljlua_stream_ud *ud = ljlua_check_stream(L, 1);
  lonejson_error error;
  lonejson_stream_result result;
  ljlua_record_ud *record_ud = ljlua_test_record(L, 2);

  if (record_ud != NULL) {
    if (record_ud->schema != ud->schema) {
      return luaL_error(L, "record belongs to a different schema");
    }
    if (ljlua_schema_has_json_value(ud->schema)) {
      if (ud->clear_destination) {
        lonejson_reset(&ud->schema->map, record_ud->data);
      }
      ljlua_prepare_record_json_value_capture(L, ud->schema, record_ud->data,
                                              ud->clear_destination ? 0 : 1);
    }
    result = lonejson_stream_next(ud->stream, record_ud->data, &error);
    if (result == LONEJSON_STREAM_OBJECT) {
      record_ud->cleared = 0;
      lua_pushvalue(L, 2);
      lua_pushnil(L);
      lua_pushstring(L, "object");
      return 3;
    }
  } else {
    unsigned char *record =
        (unsigned char *)calloc(1u, ud->schema->record_size);
    ljlua_decode_context ctx;
    if (record == NULL) {
      return luaL_error(L, "failed to allocate decode buffer");
    }
    memset(&ctx, 0, sizeof(ctx));
    ljlua_prepare_record_storage(ud->schema, record);
    if (ljlua_schema_has_json_value(ud->schema)) {
      ljlua_decode_context_prepare_table_mode(L, ud->schema, record, &ctx);
    }
    result = lonejson_stream_next(ud->stream, record, &error);
    if (result == LONEJSON_STREAM_OBJECT) {
      if (ljlua_schema_has_json_value(ud->schema)) {
        ljlua_record_to_table_with_overrides(L, ud->schema, record, &ctx);
      } else {
        ljlua_record_to_table(L, ud->schema, record);
      }
      ljlua_decode_context_cleanup(&ctx);
      ljlua_cleanup_record_storage(ud->schema, record);
      free(record);
      lua_pushnil(L);
      lua_pushstring(L, "object");
      return 3;
    }
    ljlua_decode_context_cleanup(&ctx);
    ljlua_cleanup_record_storage(ud->schema, record);
    free(record);
  }
  if (result == LONEJSON_STREAM_EOF) {
    lua_pushnil(L);
    lua_pushnil(L);
    lua_pushstring(L, "eof");
    return 3;
  }
  if (result == LONEJSON_STREAM_WOULD_BLOCK) {
    lua_pushnil(L);
    lua_pushnil(L);
    lua_pushstring(L, "would_block");
    return 3;
  }
  lua_pushnil(L);
  ljlua_push_error(L, &error);
  lua_pushstring(L, "error");
  return 3;
}

static int ljlua_spool_gc(lua_State *L) {
  ljlua_spool_ud *ud = ljlua_check_spool(L, 1);
  if (ud->owns_data) {
    lonejson_spooled_cleanup(&ud->owned);
  }
  if (ud->owner_ref != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, ud->owner_ref);
    ud->owner_ref = LUA_NOREF;
  }
  return 0;
}

static int ljlua_spool_size(lua_State *L) {
  ljlua_spool_ud *ud = ljlua_check_spool(L, 1);
  lua_pushinteger(L, (lua_Integer)lonejson_spooled_size(ud->spool));
  return 1;
}

static int ljlua_spool_spilled(lua_State *L) {
  ljlua_spool_ud *ud = ljlua_check_spool(L, 1);
  lua_pushboolean(L, lonejson_spooled_spilled(ud->spool));
  return 1;
}

static int ljlua_spool_path(lua_State *L) {
  ljlua_spool_ud *ud = ljlua_check_spool(L, 1);
  if (ud->spool->temp_path[0] == '\0') {
    lua_pushnil(L);
  } else {
    lua_pushstring(L, ud->spool->temp_path);
  }
  return 1;
}

static int ljlua_spool_rewind(lua_State *L) {
  ljlua_spool_ud *ud = ljlua_check_spool(L, 1);
  lonejson_error error;
  if (lonejson_spooled_rewind(ud->spool, &error) != LONEJSON_STATUS_OK) {
    return ljlua_push_error(L, &error);
  }
  lua_pushboolean(L, 1);
  return 1;
}

static int ljlua_spool_read(lua_State *L) {
  ljlua_spool_ud *ud = ljlua_check_spool(L, 1);
  size_t capacity = (size_t)luaL_optinteger(L, 2, 4096);
  unsigned char *buffer;
  lonejson_read_result rr;

  buffer = (unsigned char *)lua_newuserdatauv(L, capacity, 0);
  rr = lonejson_spooled_read(ud->spool, buffer, capacity);
  if (rr.error_code != 0) {
    lua_pop(L, 1);
    return luaL_error(L, "failed to read spool");
  }
  if (rr.bytes_read == 0u && rr.eof) {
    lua_pop(L, 1);
    lua_pushnil(L);
    return 1;
  }
  lua_pushlstring(L, (const char *)buffer, rr.bytes_read);
  return 1;
}

static int ljlua_spool_read_all(lua_State *L) {
  ljlua_spool_ud *ud = ljlua_check_spool(L, 1);
  luaL_Buffer buf;
  unsigned char chunk[4096];
  lonejson_error error;

  if (lonejson_spooled_rewind(ud->spool, &error) != LONEJSON_STATUS_OK) {
    return ljlua_push_error(L, &error);
  }
  luaL_buffinit(L, &buf);
  for (;;) {
    lonejson_read_result rr =
        lonejson_spooled_read(ud->spool, chunk, sizeof(chunk));
    if (rr.error_code != 0) {
      return luaL_error(L, "failed to read spool");
    }
    if (rr.bytes_read != 0u) {
      luaL_addlstring(&buf, (const char *)chunk, rr.bytes_read);
    }
    if (rr.eof) {
      break;
    }
  }
  luaL_pushresult(&buf);
  return 1;
}

static int ljlua_spool_write_to(lua_State *L) {
  ljlua_spool_ud *ud = ljlua_check_spool(L, 1);
  size_t capacity = (size_t)luaL_optinteger(L, 3, 4096);
  unsigned char *buffer;
  lonejson_error error;

  luaL_checktype(L, 2, LUA_TFUNCTION);
  buffer = (unsigned char *)malloc(capacity);
  if (buffer == NULL) {
    return luaL_error(L, "failed to allocate write buffer");
  }
  if (lonejson_spooled_rewind(ud->spool, &error) != LONEJSON_STATUS_OK) {
    free(buffer);
    return ljlua_push_error(L, &error);
  }
  for (;;) {
    lonejson_read_result rr =
        lonejson_spooled_read(ud->spool, buffer, capacity);
    if (rr.error_code != 0) {
      free(buffer);
      return luaL_error(L, "failed to read spool");
    }
    if (rr.bytes_read != 0u) {
      lua_pushvalue(L, 2);
      lua_pushlstring(L, (const char *)buffer, rr.bytes_read);
      lua_call(L, 1, 0);
    }
    if (rr.eof) {
      break;
    }
  }
  free(buffer);
  lua_pushboolean(L, 1);
  return 1;
}

