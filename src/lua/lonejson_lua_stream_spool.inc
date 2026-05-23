static int ljlua_schema_stream_string(lua_State *L) {
  ljlua_schema_ud *schema_ud = ljlua_check_schema(L, 1);
  size_t len;
  const char *json = luaL_checklstring(L, 2, &len);
  lonejson_parse_options options;
  lonejson_error error;
  ljlua_stream_ud *ud;
  int fixed_string_scratch_ref =
      ljlua_parse_options_fixed_string_scratch_ref(L, 3);

  ljlua_parse_parse_options(L, 3, &options);
  ud = (ljlua_stream_ud *)lua_newuserdatauv(L, sizeof(*ud), 0);
  memset(ud, 0, sizeof(*ud));
  ud->fixed_string_scratch_ref = LUA_NOREF;
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
    if (fixed_string_scratch_ref != LUA_NOREF) {
      luaL_unref(L, LUA_REGISTRYINDEX, fixed_string_scratch_ref);
    }
    return ljlua_push_error(L, &error);
  }
  ud->schema = schema_ud->schema;
  lua_pushvalue(L, 1);
  ud->schema_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  ud->fixed_string_scratch_ref = fixed_string_scratch_ref;
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
  if (ud->fixed_string_scratch_ref != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, ud->fixed_string_scratch_ref);
    ud->fixed_string_scratch_ref = LUA_NOREF;
  }
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
  int fixed_string_scratch_ref =
      ljlua_parse_options_fixed_string_scratch_ref(L, 4);

  ljlua_parse_parse_options(L, 4, &options);
  ud = (ljlua_array_stream_ud *)lua_newuserdatauv(L, sizeof(*ud), 0);
  memset(ud, 0, sizeof(*ud));
  ud->fixed_string_scratch_ref = LUA_NOREF;
  ud->clear_destination = options.clear_destination ? 1 : 0;
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    options.clear_destination = 0;
  }
  ud->magic = LJLUA_ARRAY_STREAM_MAGIC;
  ud->stream =
      lonejson_array_stream_open_path(array_path, path, &options, &error);
  if (ud->stream == NULL) {
    if (fixed_string_scratch_ref != LUA_NOREF) {
      luaL_unref(L, LUA_REGISTRYINDEX, fixed_string_scratch_ref);
    }
    return ljlua_push_error(L, &error);
  }
  ud->schema = schema_ud->schema;
  lua_pushvalue(L, 1);
  ud->schema_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  ud->fixed_string_scratch_ref = fixed_string_scratch_ref;
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
  int fixed_string_scratch_ref =
      ljlua_parse_options_fixed_string_scratch_ref(L, 4);

  ljlua_parse_parse_options(L, 4, &options);
  ud = (ljlua_array_stream_ud *)lua_newuserdatauv(L, sizeof(*ud), 0);
  memset(ud, 0, sizeof(*ud));
  ud->fixed_string_scratch_ref = LUA_NOREF;
  ud->clear_destination = options.clear_destination ? 1 : 0;
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    options.clear_destination = 0;
  }
  ud->magic = LJLUA_ARRAY_STREAM_MAGIC;
  ud->stream = lonejson_array_stream_open_fd(array_path, fd, &options, &error);
  if (ud->stream == NULL) {
    if (fixed_string_scratch_ref != LUA_NOREF) {
      luaL_unref(L, LUA_REGISTRYINDEX, fixed_string_scratch_ref);
    }
    return ljlua_push_error(L, &error);
  }
  ud->schema = schema_ud->schema;
  lua_pushvalue(L, 1);
  ud->schema_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  ud->fixed_string_scratch_ref = fixed_string_scratch_ref;
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
  int fixed_string_scratch_ref =
      ljlua_parse_options_fixed_string_scratch_ref(L, 4);

  ljlua_parse_parse_options(L, 4, &options);
  ud = (ljlua_array_stream_ud *)lua_newuserdatauv(L, sizeof(*ud), 0);
  memset(ud, 0, sizeof(*ud));
  ud->fixed_string_scratch_ref = LUA_NOREF;
  ud->clear_destination = options.clear_destination ? 1 : 0;
  if (ljlua_schema_has_json_value(schema_ud->schema)) {
    options.clear_destination = 0;
  }
  ud->magic = LJLUA_ARRAY_STREAM_MAGIC;
  ud->stream =
      lonejson_array_stream_open_filep(array_path, fp, &options, &error);
  if (ud->stream == NULL) {
    if (fixed_string_scratch_ref != LUA_NOREF) {
      luaL_unref(L, LUA_REGISTRYINDEX, fixed_string_scratch_ref);
    }
    return ljlua_push_error(L, &error);
  }
  ud->schema = schema_ud->schema;
  lua_pushvalue(L, 1);
  ud->schema_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  ud->fixed_string_scratch_ref = fixed_string_scratch_ref;
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
  int fixed_string_scratch_ref =
      ljlua_parse_options_fixed_string_scratch_ref(L, 4);

  ljlua_parse_parse_options(L, 4, &options);
  ud = (ljlua_array_stream_ud *)lua_newuserdatauv(L, sizeof(*ud), 0);
  memset(ud, 0, sizeof(*ud));
  ud->fixed_string_scratch_ref = LUA_NOREF;
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
    if (fixed_string_scratch_ref != LUA_NOREF) {
      luaL_unref(L, LUA_REGISTRYINDEX, fixed_string_scratch_ref);
    }
    return ljlua_push_error(L, &error);
  }
  ud->schema = schema_ud->schema;
  lua_pushvalue(L, 1);
  ud->schema_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  ud->fixed_string_scratch_ref = fixed_string_scratch_ref;
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

static lonejson_read_result ljlua_array_mem_stream_read(void *user,
                                                        unsigned char *buffer,
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
    result = lonejson_array_stream_next(ud->stream, &ud->schema->map, record,
                                        &error);
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

typedef struct ljlua_array_rewrite_mem_reader {
  const unsigned char *data;
  size_t len;
  size_t offset;
} ljlua_array_rewrite_mem_reader;

typedef struct ljlua_array_rewrite_parent_state {
  ljlua_schema *schema;
  unsigned char *record;
} ljlua_array_rewrite_parent_state;

typedef struct ljlua_array_rewrite_ctx {
  lua_State *L;
  int item_ref;
  int append_ref;
  const char *selector;
  lonejson_json_value item_value;
  lonejson_json_value replacement_value;
  lonejson_json_value insert_value;
  lonejson_json_value append_value;
  lonejson_array_rewrite_parent *parents;
  ljlua_array_rewrite_parent_state *parent_states;
  size_t parent_count;
} ljlua_array_rewrite_ctx;

static int ljlua_array_rewrite_validate_json_value(lua_State *L, int index,
                                                   const void **visited,
                                                   size_t depth,
                                                   lonejson_error *error);

static lonejson_read_result ljlua_array_rewrite_mem_read(void *user,
                                                         unsigned char *buffer,
                                                         size_t capacity) {
  ljlua_array_rewrite_mem_reader *reader =
      (ljlua_array_rewrite_mem_reader *)user;
  lonejson_read_result rr;
  size_t remaining;

  memset(&rr, 0, sizeof(rr));
  if (reader->offset >= reader->len) {
    rr.eof = 1;
    return rr;
  }
  remaining = reader->len - reader->offset;
  if (remaining > capacity) {
    remaining = capacity;
  }
  memcpy(buffer, reader->data + reader->offset, remaining);
  reader->offset += remaining;
  rr.bytes_read = remaining;
  rr.eof = reader->offset >= reader->len;
  return rr;
}

static int ljlua_array_rewrite_validate_json_table(lua_State *L, int index,
                                                   const void **visited,
                                                   size_t depth,
                                                   lonejson_error *error) {
  const char *shape;
  size_t i;

  index = lua_absindex(L, index);
  shape = ljlua_json_shape(L, index);
  if (shape != NULL && strcmp(shape, "array") == 0) {
    size_t n = (size_t)lua_rawlen(L, index);

    for (i = 1u; i <= n; ++i) {
      int ok;

      lua_rawgeti(L, index, (lua_Integer)i);
      ok = ljlua_array_rewrite_validate_json_value(L, lua_gettop(L), visited,
                                                   depth + 1u, error);
      lua_pop(L, 1);
      if (!ok) {
        return 0;
      }
    }
    return 1;
  }
  if ((shape != NULL && strcmp(shape, "object") == 0) ||
      !ljlua_json_table_is_array(L, index)) {
    lua_pushnil(L);
    while (lua_next(L, index) != 0) {
      int ok;

      if (lua_type(L, -2) != LUA_TSTRING) {
        lua_pop(L, 2);
        lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u, 0u,
                            "JSON object keys must be strings");
        return 0;
      }
      ok = ljlua_array_rewrite_validate_json_value(L, lua_gettop(L), visited,
                                                   depth + 1u, error);
      lua_pop(L, 1);
      if (!ok) {
        lua_pop(L, 1);
        return 0;
      }
    }
    return 1;
  }
  {
    size_t n = (size_t)lua_rawlen(L, index);

    for (i = 1u; i <= n; ++i) {
      int ok;

      lua_rawgeti(L, index, (lua_Integer)i);
      ok = ljlua_array_rewrite_validate_json_value(L, lua_gettop(L), visited,
                                                   depth + 1u, error);
      lua_pop(L, 1);
      if (!ok) {
        return 0;
      }
    }
  }
  return 1;
}

static int ljlua_array_rewrite_validate_json_value(lua_State *L, int index,
                                                   const void **visited,
                                                   size_t depth,
                                                   lonejson_error *error) {
  int type;
  size_t i;

  if (depth > 128u) {
    lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u, 0u,
                        "JSON value nesting exceeds Lua binding limit");
    return 0;
  }
  index = lua_absindex(L, index);
  if (ljlua_is_json_null(L, index)) {
    return 1;
  }
  type = lua_type(L, index);
  switch (type) {
  case LUA_TNIL:
  case LUA_TBOOLEAN:
  case LUA_TSTRING:
    return 1;
  case LUA_TNUMBER:
#if defined(LUA_MAXINTEGER)
    if (lua_isinteger(L, index)) {
      return 1;
    }
#endif
    if (!lonejson__is_finite_f64((double)lua_tonumber(L, index))) {
      lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u, 0u,
                          "JSON numbers must be finite");
      return 0;
    }
    return 1;
  case LUA_TTABLE: {
    const void *ptr = lua_topointer(L, index);

    for (i = 0u; i < depth; ++i) {
      if (visited[i] == ptr) {
        lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u, 0u,
                            "cyclic Lua tables cannot be encoded as JSON");
        return 0;
      }
    }
    visited[depth] = ptr;
    return ljlua_array_rewrite_validate_json_table(L, index, visited, depth,
                                                   error);
  }
  default:
    lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u, 0u,
                        "unsupported Lua type for JSON value");
    return 0;
  }
}

static int ljlua_array_rewrite_encode_source(lua_State *L, int index,
                                             lonejson_json_value *value,
                                             lonejson_error *error) {
  ljlua_json_buf buf;
  ljlua_json_out out;
  lonejson_status status;
  const void *visited[129];

  memset(&buf, 0, sizeof(buf));
  out.sink = ljlua_json_buf_sink;
  out.user = &buf;
  memset(visited, 0, sizeof(visited));
  if (!ljlua_array_rewrite_validate_json_value(L, index, visited, 0u, error)) {
    return 0;
  }
  memset(visited, 0, sizeof(visited));
  if (ljlua_encode_json_value(L, index, &out, visited, 0u) == 0) {
    free(buf.data);
    return 0;
  }
  status = lonejson_json_value_set_buffer(value, buf.data, buf.len, error);
  free(buf.data);
  return status == LONEJSON_STATUS_OK;
}

static lonejson_status ljlua_array_rewrite_lua_error(lua_State *L,
                                                     lonejson_error *error,
                                                     const char *prefix) {
  const char *message = lua_tostring(L, -1);
  if (message == NULL) {
    message = "Lua callback failed";
  }
  return lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u, 0u,
                             "%s: %s", prefix, message);
}

static void ljlua_array_rewrite_push_context(lua_State *L,
                                             ljlua_array_rewrite_ctx *ctx) {
  size_t i;

  lua_newtable(L);
  lua_pushstring(L, ctx->selector);
  lua_setfield(L, -2, "selector");
  lua_newtable(L);
  for (i = 0u; i < ctx->parent_count; ++i) {
    lua_newtable(L);
    lua_pushstring(L, ctx->parents[i].segment);
    lua_setfield(L, -2, "segment");
    ljlua_record_to_table(L, ctx->parent_states[i].schema,
                          ctx->parent_states[i].record);
    lua_setfield(L, -2, "value");
    lua_rawseti(L, -2, (lua_Integer)i + 1);
  }
  lua_setfield(L, -2, "parents");
}

static lonejson_status
ljlua_array_rewrite_parse_action(lua_State *L, int index,
                                 lonejson_array_rewrite_action *out,
                                 lonejson_error *error) {
  const char *action = NULL;

  if (lua_isnoneornil(L, index)) {
    *out = LONEJSON_ARRAY_REWRITE_KEEP;
    return LONEJSON_STATUS_OK;
  }
  if (lua_istable(L, index)) {
    lua_getfield(L, index, "action");
    action = lua_tostring(L, -1);
    lua_pop(L, 1);
  } else {
    action = lua_tostring(L, index);
  }
  if (action == NULL) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u, 0u,
        "array rewrite action must be a string or table");
  }
  if (strcmp(action, "keep") == 0) {
    *out = LONEJSON_ARRAY_REWRITE_KEEP;
  } else if (strcmp(action, "drop") == 0) {
    *out = LONEJSON_ARRAY_REWRITE_DROP;
  } else if (strcmp(action, "replace") == 0) {
    *out = LONEJSON_ARRAY_REWRITE_REPLACE;
  } else if (strcmp(action, "insert_before") == 0) {
    *out = LONEJSON_ARRAY_REWRITE_INSERT_BEFORE;
  } else if (strcmp(action, "insert_after") == 0) {
    *out = LONEJSON_ARRAY_REWRITE_INSERT_AFTER;
  } else if (strcmp(action, "replace_and_insert_after") == 0) {
    *out = LONEJSON_ARRAY_REWRITE_REPLACE_AND_INSERT_AFTER;
  } else {
    return lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u,
                               0u, "unsupported array rewrite action '%s'",
                               action);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status ljlua_array_rewrite_item_callback(
    void *user, const lonejson_array_rewrite_context *context, void *item,
    lonejson_array_rewrite_result *result, lonejson_error *error) {
  ljlua_array_rewrite_ctx *ctx = (ljlua_array_rewrite_ctx *)user;
  lua_State *L = ctx->L;
  int top = lua_gettop(L);
  int callback_status;

  (void)context;
  lonejson_json_value_cleanup(&ctx->replacement_value);
  lonejson_json_value_cleanup(&ctx->insert_value);
  lonejson_json_value_init(&ctx->replacement_value);
  lonejson_json_value_init(&ctx->insert_value);

  lua_rawgeti(L, LUA_REGISTRYINDEX, ctx->item_ref);
  ljlua_push_json_value(L, (const lonejson_json_value *)item);
  ljlua_array_rewrite_push_context(L, ctx);
  callback_status = lua_pcall(L, 2, 1, 0);
  if (callback_status != LUA_OK) {
    lonejson_status status =
        ljlua_array_rewrite_lua_error(L, error, "array rewrite item callback");
    lua_settop(L, top);
    return status;
  }
  if (ljlua_array_rewrite_parse_action(L, -1, &result->action, error) !=
      LONEJSON_STATUS_OK) {
    lua_settop(L, top);
    return error->code;
  }
  if (lua_istable(L, -1)) {
    if (result->action == LONEJSON_ARRAY_REWRITE_REPLACE ||
        result->action == LONEJSON_ARRAY_REWRITE_REPLACE_AND_INSERT_AFTER) {
      lua_getfield(L, -1, "replacement");
      if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, -1, "value");
      }
      if (!ljlua_array_rewrite_encode_source(L, -1, &ctx->replacement_value,
                                             error)) {
        lua_settop(L, top);
        return error->code;
      }
      lua_pop(L, 1);
      result->replacement.json = &ctx->replacement_value;
    }
    if (result->action == LONEJSON_ARRAY_REWRITE_INSERT_BEFORE ||
        result->action == LONEJSON_ARRAY_REWRITE_INSERT_AFTER ||
        result->action == LONEJSON_ARRAY_REWRITE_REPLACE_AND_INSERT_AFTER) {
      lua_getfield(L, -1, "insert");
      if (!ljlua_array_rewrite_encode_source(L, -1, &ctx->insert_value,
                                             error)) {
        lua_settop(L, top);
        return error->code;
      }
      lua_pop(L, 1);
      result->insert.json = &ctx->insert_value;
    }
  }
  lua_settop(L, top);
  return LONEJSON_STATUS_OK;
}

static int ljlua_array_rewrite_emit_lua(lua_State *L) {
  ljlua_array_rewrite_ctx *ctx =
      (ljlua_array_rewrite_ctx *)lua_touserdata(L, lua_upvalueindex(1));
  lonejson_array_rewrite_emit_fn emit =
      (lonejson_array_rewrite_emit_fn)lua_touserdata(L, lua_upvalueindex(2));
  void *emit_user = lua_touserdata(L, lua_upvalueindex(3));
  lonejson_error error;
  lonejson_array_rewrite_source source;

  memset(&error, 0, sizeof(error));
  memset(&source, 0, sizeof(source));
  lonejson_json_value_cleanup(&ctx->append_value);
  lonejson_json_value_init(&ctx->append_value);
  if (!ljlua_array_rewrite_encode_source(L, 1, &ctx->append_value, &error)) {
    lua_pushnil(L);
    ljlua_push_error(L, &error);
    return 2;
  }
  source.json = &ctx->append_value;
  if (emit(emit_user, &source, &error) != LONEJSON_STATUS_OK) {
    lua_pushnil(L);
    ljlua_push_error(L, &error);
    return 2;
  }
  lua_pushboolean(L, 1);
  return 1;
}

static lonejson_status ljlua_array_rewrite_append_callback(
    void *user, const lonejson_array_rewrite_context *context,
    lonejson_array_rewrite_emit_fn emit, void *emit_user,
    lonejson_error *error) {
  ljlua_array_rewrite_ctx *ctx = (ljlua_array_rewrite_ctx *)user;
  lua_State *L = ctx->L;
  int top = lua_gettop(L);
  int callback_status;

  (void)context;
  lua_rawgeti(L, LUA_REGISTRYINDEX, ctx->append_ref);
  ljlua_array_rewrite_push_context(L, ctx);
  lua_pushlightuserdata(L, ctx);
  lua_pushlightuserdata(L, (void *)emit);
  lua_pushlightuserdata(L, emit_user);
  lua_pushcclosure(L, ljlua_array_rewrite_emit_lua, 3);
  callback_status = lua_pcall(L, 2, 0, 0);
  if (callback_status != LUA_OK) {
    lonejson_status status = ljlua_array_rewrite_lua_error(
        L, error, "array rewrite append callback");
    lua_settop(L, top);
    return status;
  }
  lua_settop(L, top);
  return LONEJSON_STATUS_OK;
}

static void ljlua_array_rewrite_ctx_cleanup(ljlua_array_rewrite_ctx *ctx) {
  size_t i;

  if (ctx == NULL) {
    return;
  }
  if (ctx->item_ref != LUA_NOREF) {
    luaL_unref(ctx->L, LUA_REGISTRYINDEX, ctx->item_ref);
    ctx->item_ref = LUA_NOREF;
  }
  if (ctx->append_ref != LUA_NOREF) {
    luaL_unref(ctx->L, LUA_REGISTRYINDEX, ctx->append_ref);
    ctx->append_ref = LUA_NOREF;
  }
  lonejson_json_value_cleanup(&ctx->item_value);
  lonejson_json_value_cleanup(&ctx->replacement_value);
  lonejson_json_value_cleanup(&ctx->insert_value);
  lonejson_json_value_cleanup(&ctx->append_value);
  for (i = 0u; i < ctx->parent_count; ++i) {
    if (ctx->parent_states[i].schema != NULL &&
        ctx->parent_states[i].record != NULL) {
      ljlua_cleanup_record_storage(ctx->parent_states[i].schema,
                                   ctx->parent_states[i].record);
      free(ctx->parent_states[i].record);
    }
  }
  free(ctx->parent_states);
  free(ctx->parents);
}

static int ljlua_array_rewrite_parse_options(
    lua_State *L, int index, const char *selector, ljlua_array_rewrite_ctx *ctx,
    lonejson_parse_options *parse_opts,
    lonejson_array_rewrite_options *rewrite_opts) {
  int options_index = lua_absindex(L, index);

  memset(ctx, 0, sizeof(*ctx));
  memset(rewrite_opts, 0, sizeof(*rewrite_opts));
  ctx->L = L;
  ctx->item_ref = LUA_NOREF;
  ctx->append_ref = LUA_NOREF;
  ctx->selector = selector;
  lonejson_json_value_init(&ctx->item_value);
  lonejson_json_value_init(&ctx->replacement_value);
  lonejson_json_value_init(&ctx->insert_value);
  lonejson_json_value_init(&ctx->append_value);
  ljlua_parse_parse_options(L, options_index, parse_opts);
  rewrite_opts->parse_options = parse_opts;
  rewrite_opts->user = ctx;

  if (lua_isnoneornil(L, options_index)) {
    return 1;
  }
  luaL_checktype(L, options_index, LUA_TTABLE);

  lua_getfield(L, options_index, "item");
  if (!lua_isnil(L, -1)) {
    luaL_checktype(L, -1, LUA_TFUNCTION);
    ctx->item_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    rewrite_opts->item_value = &ctx->item_value;
    rewrite_opts->item = ljlua_array_rewrite_item_callback;
  } else {
    lua_pop(L, 1);
  }

  lua_getfield(L, options_index, "append");
  if (!lua_isnil(L, -1)) {
    luaL_checktype(L, -1, LUA_TFUNCTION);
    ctx->append_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    rewrite_opts->append = ljlua_array_rewrite_append_callback;
  } else {
    lua_pop(L, 1);
  }

  lua_getfield(L, options_index, "parents");
  if (!lua_isnil(L, -1)) {
    size_t i;
    size_t count;
    int parents_index;

    luaL_checktype(L, -1, LUA_TTABLE);
    parents_index = lua_absindex(L, -1);
    count = (size_t)lua_rawlen(L, parents_index);
    ctx->parents =
        (lonejson_array_rewrite_parent *)calloc(count, sizeof(ctx->parents[0]));
    ctx->parent_states = (ljlua_array_rewrite_parent_state *)calloc(
        count, sizeof(ctx->parent_states[0]));
    if ((count != 0u && ctx->parents == NULL) ||
        (count != 0u && ctx->parent_states == NULL)) {
      lua_pop(L, 1);
      return luaL_error(L, "failed to allocate array rewrite parents");
    }
    ctx->parent_count = count;
    for (i = 0u; i < count; ++i) {
      ljlua_schema_ud *schema_ud;

      lua_rawgeti(L, parents_index, (lua_Integer)i + 1);
      luaL_checktype(L, -1, LUA_TTABLE);
      lua_getfield(L, -1, "segment");
      ctx->parents[i].segment = luaL_checkstring(L, -1);
      lua_pop(L, 1);
      lua_getfield(L, -1, "schema");
      schema_ud = ljlua_check_schema(L, -1);
      lua_pop(L, 1);
      ctx->parent_states[i].schema = schema_ud->schema;
      ctx->parent_states[i].record =
          (unsigned char *)malloc(schema_ud->schema->record_size);
      if (ctx->parent_states[i].record == NULL) {
        lua_pop(L, 2);
        return luaL_error(L, "failed to allocate array rewrite parent record");
      }
      ljlua_prepare_record_storage(schema_ud->schema,
                                   ctx->parent_states[i].record);
      ctx->parents[i].map = &schema_ud->schema->map;
      ctx->parents[i].dst = ctx->parent_states[i].record;
      lua_pop(L, 1);
    }
    rewrite_opts->parents = ctx->parents;
    rewrite_opts->parent_count = ctx->parent_count;
  }
  lua_pop(L, 1);
  return 1;
}

static int ljlua_array_rewrite_string(lua_State *L) {
  const char *selector = luaL_checkstring(L, 1);
  size_t len;
  const char *json = luaL_checklstring(L, 2, &len);
  lonejson_parse_options parse_options;
  lonejson_array_rewrite_options options;
  ljlua_array_rewrite_ctx ctx;
  ljlua_array_rewrite_mem_reader reader;
  ljlua_json_buf output;
  lonejson_error error;
  lonejson_status status;

  ljlua_array_rewrite_parse_options(L, 3, selector, &ctx, &parse_options,
                                    &options);
  memset(&reader, 0, sizeof(reader));
  reader.data = (const unsigned char *)json;
  reader.len = len;
  memset(&output, 0, sizeof(output));
  status = lonejson_array_rewrite_reader(selector, ljlua_array_rewrite_mem_read,
                                         &reader, ljlua_json_buf_sink, &output,
                                         &options, &error);
  ljlua_array_rewrite_ctx_cleanup(&ctx);
  if (status != LONEJSON_STATUS_OK) {
    free(output.data);
    lua_pushnil(L);
    ljlua_push_error(L, &error);
    return 2;
  }
  lua_pushlstring(L, output.data, output.len);
  free(output.data);
  return 1;
}

static int ljlua_array_rewrite_path(lua_State *L) {
  const char *selector = luaL_checkstring(L, 1);
  const char *input_path = luaL_checkstring(L, 2);
  const char *output_path = luaL_checkstring(L, 3);
  lonejson_parse_options parse_options;
  lonejson_array_rewrite_options options;
  ljlua_array_rewrite_ctx ctx;
  lonejson_error error;
  lonejson_status status;

  ljlua_array_rewrite_parse_options(L, 4, selector, &ctx, &parse_options,
                                    &options);
  status = lonejson_array_rewrite_path(selector, input_path, output_path,
                                       &options, &error);
  ljlua_array_rewrite_ctx_cleanup(&ctx);
  if (status != LONEJSON_STATUS_OK) {
    lua_pushnil(L);
    ljlua_push_error(L, &error);
    return 2;
  }
  lua_pushboolean(L, 1);
  return 1;
}
