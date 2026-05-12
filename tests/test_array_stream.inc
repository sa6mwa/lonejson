static void test_array_stream_mapped_items_from_root_key(void) {
  test_reader_state state = {
      "{\"version\":1,\"items\":[{\"id\":1,\"label\":\"one\"},"
      "{\"id\":2,\"label\":\"two\"}],\"tail\":{\"ok\":true}}",
      0u, 5u};
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  test_item item;

  stream = lonejson_array_stream_open_reader("items", test_state_reader, &state,
                                             NULL, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    return;
  }

  poison_bytes(&item, sizeof(item), 0xA5u);
  result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
  EXPECT(item.id == 1);
  EXPECT(strcmp(item.label, "one") == 0);
  lonejson_cleanup(&test_item_map, &item);

  poison_bytes(&item, sizeof(item), 0x5Au);
  result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
  EXPECT(item.id == 2);
  EXPECT(strcmp(item.label, "two") == 0);
  lonejson_cleanup(&test_item_map, &item);

  result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
  EXPECT(error.code == LONEJSON_STATUS_OK);
  lonejson_array_stream_close(stream);
}

static void test_array_stream_raw_values_from_root_array(void) {
  test_reader_state state = {"[ {\"keep\":[1,true,null]}, \"text\", 42 ]", 0u,
                             3u};
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_json_value value;
  lonejson_error error;
  test_buffer_sink sink;
  unsigned char raw[128];

  lonejson_json_value_init(&value);
  stream = lonejson_array_stream_open_reader("", test_state_reader, &state,
                                             NULL, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    lonejson_json_value_cleanup(&value);
    return;
  }

  result = lonejson_array_stream_next_value(stream, &value, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
  sink.buffer = raw;
  sink.capacity = sizeof(raw);
  sink.length = 0u;
  EXPECT(lonejson_json_value_write_to_sink(&value, test_buffer_sink_write,
                                           &sink, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(strcmp((char *)raw, "{\"keep\":[1,true,null]}") == 0);

  result = lonejson_array_stream_next_value(stream, &value, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
  sink.buffer = raw;
  sink.capacity = sizeof(raw);
  sink.length = 0u;
  EXPECT(lonejson_json_value_write_to_sink(&value, test_buffer_sink_write,
                                           &sink, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(strcmp((char *)raw, "\"text\"") == 0);

  result = lonejson_array_stream_next_value(stream, &value, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
  sink.buffer = raw;
  sink.capacity = sizeof(raw);
  sink.length = 0u;
  EXPECT(lonejson_json_value_write_to_sink(&value, test_buffer_sink_write,
                                           &sink, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(strcmp((char *)raw, "42") == 0);
  result = lonejson_array_stream_next_value(stream, &value, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
  EXPECT(error.code == LONEJSON_STATUS_OK);

  lonejson_array_stream_close(stream);
  lonejson_json_value_cleanup(&value);
}

static void test_array_stream_open_path_and_fd_sources(void) {
  char path[] = "/tmp/lonejson-array-stream-XXXXXX";
  const char *json = "{\"items\":[{\"id\":7,\"label\":\"path\"}]}";
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  test_item item;
  int fd;
  int pipe_fds[2];
  int flags;

  fd = write_temp_text_file(path, json);
  EXPECT(fd >= 0);
  if (fd < 0) {
    return;
  }
  close(fd);
  stream = lonejson_array_stream_open_path("items", path, NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
    EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
    EXPECT(item.id == 7);
    lonejson_cleanup(&test_item_map, &item);
    result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
    EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
    EXPECT(error.code == LONEJSON_STATUS_OK);
    lonejson_array_stream_close(stream);
  }

  fd = open(path, O_RDONLY);
  EXPECT(fd >= 0);
  if (fd >= 0) {
    stream = lonejson_array_stream_open_fd("items", fd, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(item.id == 7);
      lonejson_cleanup(&test_item_map, &item);
      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      lonejson_array_stream_close(stream);
    }
    close(fd);
  }

  if (pipe(pipe_fds) == 0) {
    flags = fcntl(pipe_fds[0], F_GETFL, 0);
    EXPECT(flags >= 0);
    if (flags >= 0) {
      EXPECT(fcntl(pipe_fds[0], F_SETFL, flags | O_NONBLOCK) == 0);
    }

    stream = lonejson_array_stream_open_fd("items", pipe_fds[0], NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK);
      EXPECT(error.code == LONEJSON_STATUS_OK);

      EXPECT(write(pipe_fds[1], json, strlen(json)) == (ssize_t)strlen(json));
      close(pipe_fds[1]);
      pipe_fds[1] = -1;

      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      if (result == LONEJSON_ARRAY_STREAM_ITEM) {
        EXPECT(item.id == 7);
        lonejson_cleanup(&test_item_map, &item);
      }
      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      lonejson_array_stream_close(stream);
    }
    if (pipe_fds[1] >= 0) {
      close(pipe_fds[1]);
    }
    close(pipe_fds[0]);
  } else {
    EXPECT(0);
  }
  unlink(path);
}

static void test_array_stream_failure_modes(void) {
  const char *cases[] = {
      "{\"items\":{}}",
      "{\"boards\":[{\"items\":[]}]}",
      "{\"items\":[{\"id\":1,\"id\":2}]}",
      "{\"items\":[{\"label\":\"missing id\"}]}",
      "{\"items\":[{\"id\":1}],\"items\":[]}",
      "{\"items\":[{\"id\":1}],\"tail\":}",
      "{\"items\":[{\"id\":1},]}",
      NULL};
  lonejson_status expected[] = {
      LONEJSON_STATUS_TYPE_MISMATCH,
      LONEJSON_STATUS_MISSING_REQUIRED_FIELD,
      LONEJSON_STATUS_DUPLICATE_FIELD,
      LONEJSON_STATUS_MISSING_REQUIRED_FIELD,
      LONEJSON_STATUS_DUPLICATE_FIELD,
      LONEJSON_STATUS_INVALID_JSON,
      LONEJSON_STATUS_INVALID_JSON};
  size_t i;
  lonejson_error error;
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  test_item item;

  {
    test_reader_state dotted_state = {"{}", 0u, 8u};
    stream = lonejson_array_stream_open_reader(
        "boards.items", test_state_reader, &dotted_state, NULL, &error);
  }
  EXPECT(stream == NULL);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);

  for (i = 0u; cases[i] != NULL; ++i) {
    test_reader_state state;
    state.json = cases[i];
    state.offset = 0u;
    state.chunk_size = 4u;
    stream = lonejson_array_stream_open_reader("items", test_state_reader,
                                               &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream == NULL) {
      continue;
    }
    result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
    if (result == LONEJSON_ARRAY_STREAM_ITEM) {
      lonejson_cleanup(&test_item_map, &item);
      result =
          lonejson_array_stream_next(stream, &test_item_map, &item, &error);
    }
    EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
    EXPECT(error.code == expected[i]);
    lonejson_array_stream_close(stream);
  }
}

static lonejson_read_result test_error_after_prefix_reader(void *user,
                                                           unsigned char *buffer,
                                                           size_t capacity) {
  test_reader_state *st = (test_reader_state *)user;
  lonejson_read_result rr;
  size_t remaining = strlen(st->json) - st->offset;

  memset(&rr, 0, sizeof(rr));
  if (st->offset >= st->chunk_size) {
    rr.error_code = EIO;
    return rr;
  }
  if (remaining == 0u) {
    rr.eof = 1;
    return rr;
  }
  if (remaining > capacity) {
    remaining = capacity;
  }
  if (remaining > st->chunk_size - st->offset) {
    remaining = st->chunk_size - st->offset;
  }
  memcpy(buffer, st->json + st->offset, remaining);
  st->offset += remaining;
  rr.bytes_read = remaining;
  return rr;
}

static void test_array_stream_streaming_invariants(void) {
  const char *json =
      "{\"items\":[{\"id\":1,\"label\":\"one\"},{bad}],"
      "\"tail\":\"this must not be parsed before the first item\"}";
  test_reader_state state;
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  test_item item;
  size_t after_first_item;

  state.json = json;
  state.offset = 0u;
  state.chunk_size = 1u;
  stream = lonejson_array_stream_open_reader("items", test_state_reader, &state,
                                             NULL, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    return;
  }

  result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
  EXPECT(item.id == 1);
  EXPECT(strcmp(item.label, "one") == 0);
  after_first_item = state.offset;
  EXPECT(after_first_item < strlen(json));
  EXPECT(strstr(json + after_first_item, "{bad}") != NULL);
  EXPECT(lonejson_array_stream_error(stream)->code == LONEJSON_STATUS_OK);
  lonejson_cleanup(&test_item_map, &item);

  result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_JSON);
  lonejson_array_stream_close(stream);
}

static void test_array_stream_delimiter_validation_before_yield(void) {
  static const char *raw_root_cases[] = {
      "[1x]",
      "[true?]",
      "[{}foo]",
  };
  size_t i;

  for (i = 0u; i < sizeof(raw_root_cases) / sizeof(raw_root_cases[0]); ++i) {
    test_reader_state state;
    lonejson_array_stream *stream;
    lonejson_array_stream_result result;
    lonejson_error error;
    lonejson_json_value value;

    state.json = raw_root_cases[i];
    state.offset = 0u;
    state.chunk_size = 1u;
    lonejson_json_value_init(&value);
    stream = lonejson_array_stream_open_reader("", test_state_reader, &state,
                                               NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_INVALID_JSON);
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
  }

  {
    test_reader_state state = {"{\"items\":[{}foo]}", 0u, 1u};
    lonejson_array_stream *stream;
    lonejson_array_stream_result result;
    lonejson_error error;
    lonejson_json_value value;

    lonejson_json_value_init(&value);
    stream = lonejson_array_stream_open_reader("items", test_state_reader,
                                               &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_INVALID_JSON);
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
  }

  {
    test_reader_state state = {"{\"items\":[{\"id\":1}foo]}", 0u, 1u};
    lonejson_array_stream *stream;
    lonejson_array_stream_result result;
    lonejson_error error;
    test_item item;

    stream = lonejson_array_stream_open_reader("items", test_state_reader,
                                               &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_INVALID_JSON);
      lonejson_array_stream_close(stream);
    }
  }
}

static void test_array_stream_mapped_items_do_not_materialize_item(void) {
  test_reader_state state = {
      "[{\"id\":9,\"label\":\"large-label-that-truncates\"}]", 0u, 1u};
  lonejson_allocator allocator = lonejson_default_allocator();
  lonejson_parse_options options = lonejson_default_parse_options();
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  test_item item;

  allocator.malloc_fn = test_no_realloc_malloc;
  allocator.realloc_fn = test_no_realloc_realloc;
  allocator.free_fn = test_no_realloc_free;
  options.allocator = &allocator;
  stream = lonejson_array_stream_open_reader("", test_state_reader, &state,
                                             &options, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    return;
  }

  result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
  EXPECT(item.id == 9);
  EXPECT(strcmp(item.label, "large-label") == 0);
  lonejson_cleanup(&test_item_map, &item);

  result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
  lonejson_array_stream_close(stream);
}

static void test_array_stream_skips_do_not_materialize_values(void) {
  char json[1024];
  size_t pos = 0u;
  size_t i;
  test_reader_state state;
  test_realloc_limit_state limit = {128u};
  lonejson_allocator allocator = lonejson_default_allocator();
  lonejson_parse_options options = lonejson_default_parse_options();
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  test_item item;

  memcpy(json + pos, "{\"meta\":\"", 9u);
  pos += 9u;
  for (i = 0u; i < 700u; ++i) {
    json[pos++] = 'x';
  }
  memcpy(json + pos, "\",\"items\":[{\"id\":5,\"label\":\"ok\"}],\"tail\":\"", 42u);
  pos += 42u;
  for (i = 0u; i < 120u; ++i) {
    json[pos++] = 'y';
  }
  memcpy(json + pos, "\"}", 3u);

  allocator.ctx = &limit;
  allocator.malloc_fn = test_realloc_limit_malloc;
  allocator.realloc_fn = test_realloc_limit_realloc;
  allocator.free_fn = test_realloc_limit_free;
  options.allocator = &allocator;

  state.json = json;
  state.offset = 0u;
  state.chunk_size = 3u;
  stream = lonejson_array_stream_open_reader("items", test_state_reader, &state,
                                             &options, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    return;
  }

  result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
  EXPECT(item.id == 5);
  EXPECT(strcmp(item.label, "ok") == 0);
  lonejson_cleanup(&test_item_map, &item);

  result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
  EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
  EXPECT(error.code == LONEJSON_STATUS_OK);
  lonejson_array_stream_close(stream);
}

static void test_array_stream_escaped_key_path_matching(void) {
  static const struct {
    const char *path;
    const char *json;
    int id;
  } cases[] = {
      {"items", "{\"\\u0069tems\":[{\"id\":10,\"label\":\"u\"}]}", 10},
      {"items", "{\"it\\u0065ms\":[{\"id\":11,\"label\":\"u\"}]}", 11},
      {"a/b", "{\"a\\/b\":[{\"id\":12,\"label\":\"s\"}]}", 12},
      {"a\\b", "{\"a\\\\b\":[{\"id\":13,\"label\":\"b\"}]}", 13},
      {"it\tms", "{\"it\\tms\":[{\"id\":14,\"label\":\"t\"}]}", 14},
      {"\xF0\x9D\x84\x9E",
       "{\"\\uD834\\uDD1E\":[{\"id\":15,\"label\":\"p\"}]}",
       15},
  };
  size_t i;

  for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    test_reader_state state;
    lonejson_array_stream *stream;
    lonejson_array_stream_result result;
    lonejson_error error;
    test_item item;

    state.json = cases[i].json;
    state.offset = 0u;
    state.chunk_size = 1u;
    stream = lonejson_array_stream_open_reader(cases[i].path,
                                               test_state_reader, &state, NULL,
                                               &error);
    EXPECT(stream != NULL);
    if (stream == NULL) {
      continue;
    }
    result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
    EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
    EXPECT(item.id == cases[i].id);
    lonejson_cleanup(&test_item_map, &item);
    result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
    EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
    lonejson_array_stream_close(stream);
  }
}

static void test_array_stream_escaped_key_failure_modes(void) {
  static const char *invalid_keys[] = {
      "{\"\\uD834x\":0,\"items\":[]}",
      "{\"\\uDD1E\":0,\"items\":[]}",
      "{\"\\u12X4\":0,\"items\":[]}",
      "{\"bad\\q\":0,\"items\":[]}",
  };
  size_t i;

  {
    lonejson_parse_options options = lonejson_default_parse_options();
    test_reader_state state = {"{\"items\":[],\"it\\u0065ms\":[]}", 0u, 1u};
    lonejson_array_stream *stream;
    lonejson_array_stream_result result;
    lonejson_error error;
    test_item item;

    options.reject_duplicate_keys = 1;
    stream = lonejson_array_stream_open_reader("items", test_state_reader,
                                               &state, &options, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_DUPLICATE_FIELD);
      lonejson_array_stream_close(stream);
    }
  }

  for (i = 0u; i < sizeof(invalid_keys) / sizeof(invalid_keys[0]); ++i) {
    test_reader_state state;
    lonejson_array_stream *stream;
    lonejson_array_stream_result result;
    lonejson_error error;
    test_item item;

    state.json = invalid_keys[i];
    state.offset = 0u;
    state.chunk_size = 1u;
    stream = lonejson_array_stream_open_reader("items", test_state_reader,
                                               &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream == NULL) {
      continue;
    }
    result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
    EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
    EXPECT(error.code == LONEJSON_STATUS_INVALID_JSON);
    lonejson_array_stream_close(stream);
  }
}

static void test_array_stream_duplicate_rejection_invariants(void) {
  lonejson_parse_options options = lonejson_default_parse_options();
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  lonejson_json_value value;
  test_item item;

  options.reject_duplicate_keys = 1;

  {
    test_reader_state state = {"[{\"a\":1,\"\\u0061\":2}]", 0u, 1u};

    lonejson_json_value_init(&value);
    stream = lonejson_array_stream_open_reader("", test_state_reader, &state,
                                               &options, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_DUPLICATE_FIELD);
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
  }

  {
    test_reader_state state = {"[{\"\":1,\"\":2}]", 0u, 1u};

    lonejson_json_value_init(&value);
    stream = lonejson_array_stream_open_reader("", test_state_reader, &state,
                                               &options, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_DUPLICATE_FIELD);
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
  }

  {
    test_reader_state state = {"[{\"a\":1,\"nested\":{\"a\":2}}]", 0u, 1u};

    lonejson_json_value_init(&value);
    stream = lonejson_array_stream_open_reader("", test_state_reader, &state,
                                               &options, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
  }

  {
    test_reader_state state =
        {"{\"meta\":1,\"\\u006deta\":2,\"items\":[{\"id\":1}]}", 0u, 1u};

    stream = lonejson_array_stream_open_reader("items", test_state_reader,
                                               &state, &options, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_DUPLICATE_FIELD);
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_reader_state state =
        {"{\"meta\":{\"x\":1,\"\\u0078\":2},\"items\":[{\"id\":1}]}",
         0u, 1u};

    stream = lonejson_array_stream_open_reader("items", test_state_reader,
                                               &state, &options, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_DUPLICATE_FIELD);
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_reader_state state =
        {"{\"meta\":[{\"x\":1,\"x\":2}],\"items\":[{\"id\":1}]}",
         0u, 1u};

    stream = lonejson_array_stream_open_reader("items", test_state_reader,
                                               &state, &options, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_DUPLICATE_FIELD);
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_reader_state state =
        {"{\"\":1,\"\":2,\"items\":[{\"id\":1}]}", 0u, 1u};

    stream = lonejson_array_stream_open_reader("items", test_state_reader,
                                               &state, &options, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_DUPLICATE_FIELD);
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_reader_state state =
        {"{\"items\":[{\"id\":2}],\"meta\":{\"x\":1,\"x\":2}}", 0u, 1u};

    stream = lonejson_array_stream_open_reader("items", test_state_reader,
                                               &state, &options, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(item.id == 2);
      lonejson_cleanup(&test_item_map, &item);

      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_DUPLICATE_FIELD);
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_reader_state state =
        {"{\"items\":[{\"id\":2}],\"meta\":1,\"meta\":2}", 0u, 1u};

    stream = lonejson_array_stream_open_reader("items", test_state_reader,
                                               &state, &options, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(item.id == 2);
      lonejson_cleanup(&test_item_map, &item);

      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_DUPLICATE_FIELD);
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_reader_state state = {"[{\"a\":1,\"a\":2}]", 0u, 1u};

    options.reject_duplicate_keys = 0;
    lonejson_json_value_init(&value);
    stream = lonejson_array_stream_open_reader("", test_state_reader, &state,
                                               &options, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
  }
}

static void test_array_stream_raw_duplicate_check_allows_large_values(void) {
  static const size_t big_string_len = 1024u * 1024u + 1024u;
  static const size_t big_number_len = 300u;
  static const size_t big_key_len = 64u * 1024u + 128u;
  char *json;
  test_reader_state state;
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  lonejson_json_value value;
  size_t pos;

  json = (char *)malloc(big_string_len + 5u);
  EXPECT(json != NULL);
  if (json != NULL) {
    json[0] = '[';
    json[1] = '"';
    memset(json + 2u, 's', big_string_len);
    json[big_string_len + 2u] = '"';
    json[big_string_len + 3u] = ']';
    json[big_string_len + 4u] = '\0';

    state.json = json;
    state.offset = 0u;
    state.chunk_size = 4096u;
    lonejson_json_value_init(&value);
    stream = lonejson_array_stream_open_reader("", test_state_reader, &state,
                                               NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      EXPECT(value.kind == LONEJSON_JSON_VALUE_BUFFER);
      EXPECT(value.len == big_string_len + 2u);
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
    free(json);
  }

  json = (char *)malloc(big_number_len + 3u);
  EXPECT(json != NULL);
  if (json != NULL) {
    json[0] = '[';
    json[1] = '1';
    memset(json + 2u, '2', big_number_len - 1u);
    json[big_number_len + 1u] = ']';
    json[big_number_len + 2u] = '\0';

    state.json = json;
    state.offset = 0u;
    state.chunk_size = 17u;
    lonejson_json_value_init(&value);
    stream = lonejson_array_stream_open_reader("", test_state_reader, &state,
                                               NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      EXPECT(value.kind == LONEJSON_JSON_VALUE_BUFFER);
      EXPECT(value.len == big_number_len);
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
    free(json);
  }

  json = (char *)malloc(big_key_len + 9u);
  EXPECT(json != NULL);
  if (json != NULL) {
    pos = 0u;
    json[pos++] = '[';
    json[pos++] = '{';
    json[pos++] = '"';
    memset(json + pos, 'k', big_key_len);
    pos += big_key_len;
    json[pos++] = '"';
    json[pos++] = ':';
    json[pos++] = '1';
    json[pos++] = '}';
    json[pos++] = ']';
    json[pos] = '\0';

    state.json = json;
    state.offset = 0u;
    state.chunk_size = 97u;
    lonejson_json_value_init(&value);
    stream = lonejson_array_stream_open_reader("", test_state_reader, &state,
                                               NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      EXPECT(value.kind == LONEJSON_JSON_VALUE_BUFFER);
      EXPECT(value.len == big_key_len + 6u);
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
    free(json);
  }
}

static void test_array_stream_reader_tolerates_empty_reads(void) {
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  lonejson_json_value value;
  test_item item;

  {
    test_empty_read_state state = {{"[]", 0u, 1u}, 2u};

    lonejson_json_value_init(&value);
    stream = lonejson_array_stream_open_reader("", test_empty_then_state_reader,
                                               &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      EXPECT(state.empty_reads_remaining == 0u);
      EXPECT(state.inner.offset == 2u);
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
  }

  {
    test_empty_read_state state = {
        {"{\"items\":[{\"id\":42,\"label\":\"ok\"}]}", 0u, 1u}, 3u};

    stream = lonejson_array_stream_open_reader("items",
                                               test_empty_then_state_reader,
                                               &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      EXPECT(item.id == 42);
      EXPECT(strcmp(item.label, "ok") == 0);
      EXPECT(state.empty_reads_remaining == 0u);
      lonejson_cleanup(&test_item_map, &item);

      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_intermittent_empty_read_state state = {
        {"[\"ab\",{\"k\":[1,2]},true]", 0u, 1u}, 1, 0u};

    lonejson_json_value_init(&value);
    stream = lonejson_array_stream_open_reader("",
                                               test_intermittent_empty_reader,
                                               &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      EXPECT(value.len == 4u);
      lonejson_json_value_cleanup(&value);

      lonejson_json_value_init(&value);
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      EXPECT(value.len == 11u);
      lonejson_json_value_cleanup(&value);

      lonejson_json_value_init(&value);
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      EXPECT(value.len == 4u);
      lonejson_json_value_cleanup(&value);

      lonejson_json_value_init(&value);
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      EXPECT(state.inner.offset == strlen(state.inner.json));
      EXPECT(state.empty_reads > 10u);
      lonejson_array_stream_close(stream);
      lonejson_json_value_cleanup(&value);
    } else {
      lonejson_json_value_cleanup(&value);
    }
  }

  {
    test_intermittent_empty_read_state state = {
        {"{\"meta\":{\"skip\":[1,2,3]},"
         "\"items\":[{\"id\":7,\"label\":\"ok\"},"
         "{\"id\":8,\"label\":\"two\"}],"
         "\"tail\":{\"done\":true}}",
         0u, 1u},
        1,
        0u};

    stream = lonejson_array_stream_open_reader("items",
                                               test_intermittent_empty_reader,
                                               &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      EXPECT(item.id == 7);
      EXPECT(strcmp(item.label, "ok") == 0);
      lonejson_cleanup(&test_item_map, &item);

      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      EXPECT(item.id == 8);
      EXPECT(strcmp(item.label, "two") == 0);
      lonejson_cleanup(&test_item_map, &item);

      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      EXPECT(state.inner.offset == strlen(state.inner.json));
      EXPECT(state.empty_reads > 20u);
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_intermittent_empty_read_state state = {{"[1x]", 0u, 1u}, 1, 0u};

    lonejson_json_value_init(&value);
    stream = lonejson_array_stream_open_reader("",
                                               test_intermittent_empty_reader,
                                               &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_INVALID_JSON);
      EXPECT(state.empty_reads > 0u);
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
  }
}

static void test_array_stream_additional_failure_modes(void) {
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  lonejson_json_value value;
  test_item item;

  {
    test_reader_state state = {"{\"meta\":{bad},\"items\":[]}", 0u, 2u};
    stream = lonejson_array_stream_open_reader("items", test_state_reader,
                                               &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_INVALID_JSON);
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_reader_state state = {"{\"items\":[01]}", 0u, 1u};
    lonejson_json_value_init(&value);
    stream = lonejson_array_stream_open_reader("items", test_state_reader,
                                               &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_INVALID_JSON);
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
  }

  {
    test_reader_state state = {"{\"items\":[{\"id\":1}", 0u, 8u};
    stream = lonejson_array_stream_open_reader("items",
                                               test_error_after_prefix_reader,
                                               &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_IO_ERROR);
      EXPECT(error.system_errno == EIO);
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_reader_state state = {"[{\"id\":1}]", 0u, 6u};

    stream = lonejson_array_stream_open_reader("",
                                               test_error_after_prefix_reader,
                                               &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_IO_ERROR);
      EXPECT(error.system_errno == EIO);
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_reader_state state = {"[{\"id\":1}]", 0u, 6u};

    lonejson_json_value_init(&value);
    stream = lonejson_array_stream_open_reader("",
                                               test_error_after_prefix_reader,
                                               &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_IO_ERROR);
      EXPECT(error.system_errno == EIO);
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
  }

  {
    test_reader_state state = {"{\"items\":[{\"id\":1}]}", 0u, 16u};
    stream = lonejson_array_stream_open_reader("items", test_state_reader,
                                               &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result =
          lonejson_array_stream_next(stream, &test_item_map, NULL, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);
      lonejson_array_stream_close(stream);
    }
  }
}

static void test_array_stream_would_block_boundaries(void) {
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  lonejson_json_value value;
  test_item item;
  size_t block_at;

  {
    test_would_block_read_state state = {"[{\"id\":1}]", 0u, 8u, 0u, 0};

    stream = lonejson_array_stream_open_reader("", test_would_block_once_reader,
                                               &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      item.id = 99;
      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      EXPECT(item.id == 99);
      EXPECT(state.blocked);
      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      if (result == LONEJSON_ARRAY_STREAM_ITEM) {
        EXPECT(item.id == 1);
        lonejson_cleanup(&test_item_map, &item);
      }
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_would_block_read_state state = {"[{\"id\":2}]", 0u, 8u, 0u, 0};

    lonejson_json_value_init(&value);
    stream = lonejson_array_stream_open_reader("", test_would_block_once_reader,
                                               &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
  }

  {
    test_would_block_read_state state =
        {"{\"meta\":1,\"items\":[{\"id\":1}]}", 0u, 4u, 8u, 0};

    stream = lonejson_array_stream_open_reader("items",
                                               test_would_block_once_reader,
                                               &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      if (result == LONEJSON_ARRAY_STREAM_ITEM) {
        EXPECT(item.id == 1);
        lonejson_cleanup(&test_item_map, &item);
      }
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_would_block_read_state state = {"[{\"id\":1}]", 0u, 4u, 6u, 0};

    stream = lonejson_array_stream_open_reader("", test_would_block_once_reader,
                                               &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      if (result == LONEJSON_ARRAY_STREAM_ITEM) {
        EXPECT(item.id == 1);
        lonejson_cleanup(&test_item_map, &item);
      }
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_would_block_read_state state = {"[{\"id\":1}]", 0u, 32u, 9u, 0};
    test_item first;
    test_item second;

    first.id = 99;
    second.id = 77;
    stream = lonejson_array_stream_open_reader("", test_would_block_once_reader,
                                               &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result =
          lonejson_array_stream_next(stream, &test_item_map, &first, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      EXPECT(first.id == 1);
      EXPECT(second.id == 77);
      result =
          lonejson_array_stream_next(stream, &test_item_map, &second, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
      EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);
      EXPECT(second.id == 77);
      lonejson_cleanup(&test_item_map, &first);
      lonejson_array_stream_close(stream);
    }
  }

  {
    test_would_block_read_state state = {"[1,2]", 0u, 4u, 2u, 0};

    lonejson_json_value_init(&value);
    stream = lonejson_array_stream_open_reader("", test_would_block_once_reader,
                                               &state, NULL, &error);
    EXPECT(stream != NULL);
    if (stream != NULL) {
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      result = lonejson_array_stream_next_value(stream, &value, &error);
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      EXPECT(value.kind == LONEJSON_JSON_VALUE_BUFFER);
      EXPECT(value.len == 1u);
      if (value.json != NULL) {
        EXPECT(memcmp(value.json, "1", 1u) == 0);
      }
      lonejson_array_stream_close(stream);
    }
    lonejson_json_value_cleanup(&value);
  }

  {
    const char *json =
        "{\"meta\":{\"nested\":[1,2,{\"x\":\"y\"}]},"
        "\"items\":[{\"id\":7,\"label\":\"ok\"},"
        "{\"id\":8,\"label\":\"two\"}],\"tail\":true}";

    for (block_at = 0u; block_at <= strlen(json); ++block_at) {
      test_would_block_read_state state;
      state.json = json;
      state.offset = 0u;
      state.chunk_size = 3u;
      state.block_after = block_at;
      state.blocked = 0;
      stream = lonejson_array_stream_open_reader(
          "items", test_would_block_once_reader, &state, NULL, &error);
      EXPECT(stream != NULL);
      if (stream == NULL) {
        continue;
      }

      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      while (result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK) {
        EXPECT(error.code == LONEJSON_STATUS_OK);
        result =
            lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      }
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      if (result == LONEJSON_ARRAY_STREAM_ITEM) {
        EXPECT(item.id == 7);
        EXPECT(strcmp(item.label, "ok") == 0);
        lonejson_cleanup(&test_item_map, &item);
      }

      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      while (result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK) {
        EXPECT(error.code == LONEJSON_STATUS_OK);
        result =
            lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      }
      EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
      if (result == LONEJSON_ARRAY_STREAM_ITEM) {
        EXPECT(item.id == 8);
        EXPECT(strcmp(item.label, "two") == 0);
        lonejson_cleanup(&test_item_map, &item);
      }

      result = lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      while (result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK) {
        EXPECT(error.code == LONEJSON_STATUS_OK);
        result =
            lonejson_array_stream_next(stream, &test_item_map, &item, &error);
      }
      EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      lonejson_array_stream_close(stream);
    }
  }

  {
    const char *json = "[{\"keep\":[1,true,null]},\"text\",42]";
    const size_t expected_lens[] = {22u, 6u, 2u};
    size_t item_index;

    for (block_at = 0u; block_at <= strlen(json); ++block_at) {
      test_would_block_read_state state;
      state.json = json;
      state.offset = 0u;
      state.chunk_size = 2u;
      state.block_after = block_at;
      state.blocked = 0;
      lonejson_json_value_init(&value);
      stream = lonejson_array_stream_open_reader(
          "", test_would_block_once_reader, &state, NULL, &error);
      EXPECT(stream != NULL);
      if (stream == NULL) {
        lonejson_json_value_cleanup(&value);
        continue;
      }

      for (item_index = 0u; item_index < 3u; ++item_index) {
        lonejson_json_value_reset(&value);
        result = lonejson_array_stream_next_value(stream, &value, &error);
        while (result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK) {
          EXPECT(error.code == LONEJSON_STATUS_OK);
          result = lonejson_array_stream_next_value(stream, &value, &error);
        }
        EXPECT(result == LONEJSON_ARRAY_STREAM_ITEM);
        EXPECT(value.kind == LONEJSON_JSON_VALUE_BUFFER);
        EXPECT(value.len == expected_lens[item_index]);
      }

      result = lonejson_array_stream_next_value(stream, &value, &error);
      while (result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK) {
        EXPECT(error.code == LONEJSON_STATUS_OK);
        result = lonejson_array_stream_next_value(stream, &value, &error);
      }
      EXPECT(result == LONEJSON_ARRAY_STREAM_EOF);
      EXPECT(error.code == LONEJSON_STATUS_OK);
      lonejson_array_stream_close(stream);
      lonejson_json_value_cleanup(&value);
    }
  }
}

static void test_array_stream_depth_limit_and_argument_guards(void) {
  test_reader_state state = {"[[[[]]]]", 0u, 2u};
  lonejson_parse_options options = lonejson_default_parse_options();
  lonejson_array_stream *stream;
  lonejson_array_stream_result result;
  lonejson_error error;
  lonejson_json_value value;
  test_item item;

  options.max_depth = 2u;
  lonejson_json_value_init(&value);
  stream = lonejson_array_stream_open_reader("", test_state_reader, &state,
                                             &options, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    result = lonejson_array_stream_next_value(stream, &value, &error);
    EXPECT(result == LONEJSON_ARRAY_STREAM_ERROR);
    EXPECT(error.code == LONEJSON_STATUS_OVERFLOW);
    lonejson_array_stream_close(stream);
  }

  EXPECT(lonejson_array_stream_next(NULL, &test_item_map, &item, &error) ==
         LONEJSON_ARRAY_STREAM_ERROR);
  EXPECT(lonejson_array_stream_next_value(NULL, &value, &error) ==
         LONEJSON_ARRAY_STREAM_ERROR);
  lonejson_json_value_cleanup(&value);
}

typedef struct test_array_stream_push_seen {
  int ids[8];
  char labels[8][12];
  size_t count;
} test_array_stream_push_seen;

typedef struct test_array_stream_string_seen {
  char items[8][64];
  size_t item_lens[8];
  size_t begins;
  size_t chunks;
  size_t ends;
  size_t current;
  size_t fail_after_chunks;
  size_t null_items;
  size_t unterminated_items;
} test_array_stream_string_seen;

static lonejson_status test_array_stream_push_item(void *user, void *dst) {
  test_array_stream_push_seen *seen = (test_array_stream_push_seen *)user;
  test_item *item = (test_item *)dst;

  if (seen->count >= sizeof(seen->ids) / sizeof(seen->ids[0])) {
    return LONEJSON_STATUS_OVERFLOW;
  }
  seen->ids[seen->count] = (int)item->id;
  strcpy(seen->labels[seen->count], item->label);
  seen->count++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_array_stream_push_fail_item(void *user,
                                                        void *dst) {
  (void)user;
  (void)dst;
  return LONEJSON_STATUS_INVALID_ARGUMENT;
}

static lonejson_status test_array_stream_push_alloc_item(void *user,
                                                         void *dst) {
  test_array_stream_push_seen *seen = (test_array_stream_push_seen *)user;
  test_alloc_parse_doc *doc = (test_alloc_parse_doc *)dst;

  seen->count++;
  if (doc->name != NULL && seen->count <= sizeof(seen->labels) /
                                      sizeof(seen->labels[0])) {
    snprintf(seen->labels[seen->count - 1u],
             sizeof(seen->labels[seen->count - 1u]), "%s", doc->name);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_array_stream_push_alloc_fail_item(void *user,
                                                              void *dst) {
  (void)user;
  (void)dst;
  return LONEJSON_STATUS_IO_ERROR;
}

static lonejson_status test_array_stream_string_begin(void *user,
                                                      lonejson_error *error) {
  test_array_stream_string_seen *seen = (test_array_stream_string_seen *)user;
  (void)error;
  if (seen->begins >= sizeof(seen->items) / sizeof(seen->items[0])) {
    return LONEJSON_STATUS_OVERFLOW;
  }
  seen->current = seen->begins;
  seen->item_lens[seen->current] = 0u;
  seen->items[seen->current][0] = '\0';
  seen->begins++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_array_stream_string_chunk(void *user,
                                                      const char *data,
                                                      size_t len,
                                                      lonejson_error *error) {
  test_array_stream_string_seen *seen = (test_array_stream_string_seen *)user;
  size_t room;
  (void)error;
  seen->chunks++;
  if (seen->fail_after_chunks != 0u &&
      seen->chunks >= seen->fail_after_chunks) {
    return LONEJSON_STATUS_IO_ERROR;
  }
  if (seen->begins == 0u) {
    return LONEJSON_STATUS_INTERNAL_ERROR;
  }
  room = sizeof(seen->items[seen->current]) - 1u - seen->item_lens[seen->current];
  if (len > room) {
    len = room;
  }
  memcpy(seen->items[seen->current] + seen->item_lens[seen->current], data, len);
  seen->item_lens[seen->current] += len;
  seen->items[seen->current][seen->item_lens[seen->current]] = '\0';
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_array_stream_string_end(void *user,
                                                    lonejson_error *error) {
  test_array_stream_string_seen *seen = (test_array_stream_string_seen *)user;
  (void)error;
  seen->ends++;
  return LONEJSON_STATUS_OK;
}

static lonejson_array_stream_string_handler test_array_stream_string_handler(
    void) {
  lonejson_array_stream_string_handler handler;
  memset(&handler, 0, sizeof(handler));
  handler.begin = test_array_stream_string_begin;
  handler.chunk = test_array_stream_string_chunk;
  handler.end = test_array_stream_string_end;
  return handler;
}

static lonejson_status test_array_stream_string_item(void *user,
                                                     const char *data,
                                                     size_t len,
                                                     lonejson_error *error) {
  test_array_stream_string_seen *seen = (test_array_stream_string_seen *)user;
  size_t room;
  (void)error;

  if (seen->ends >= sizeof(seen->items) / sizeof(seen->items[0])) {
    return LONEJSON_STATUS_OVERFLOW;
  }
  if (data == NULL) {
    seen->null_items++;
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (data[len] != '\0') {
    seen->unterminated_items++;
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (seen->fail_after_chunks != 0u &&
      seen->ends + 1u >= seen->fail_after_chunks) {
    return LONEJSON_STATUS_IO_ERROR;
  }
  room = sizeof(seen->items[seen->ends]) - 1u;
  if (len > room) {
    len = room;
  }
  memcpy(seen->items[seen->ends], data, len);
  seen->items[seen->ends][len] = '\0';
  seen->item_lens[seen->ends] = len;
  seen->ends++;
  return LONEJSON_STATUS_OK;
}

static void test_array_stream_push_api(void) {
  static const char json[] =
      "{\"meta\":{\"skip\":[1,2,{\"x\":\"y\"}]},"
      "\"items\":[{\"id\":1,\"label\":\"one\"},"
      "{\"id\":2,\"label\":\"two\"}],\"tail\":true}";
  lonejson_array_stream *stream;
  lonejson_error error;
  lonejson_status status;
  test_item item;
  test_array_stream_push_seen seen;
  size_t i;

  memset(&seen, 0, sizeof(seen));
  stream = lonejson_array_stream_open_push("items", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    for (i = 0u; i < strlen(json); ++i) {
      status = lonejson_array_stream_push(stream, &test_item_map, &item,
                                          json + i, 1u,
                                          test_array_stream_push_item, &seen,
                                          &error);
      EXPECT(status == LONEJSON_STATUS_OK);
    }
    EXPECT(seen.count == 2u);
    EXPECT(seen.ids[0] == 1);
    EXPECT(strcmp(seen.labels[0], "one") == 0);
    EXPECT(seen.ids[1] == 2);
    EXPECT(strcmp(seen.labels[1], "two") == 0);
    status = lonejson_array_stream_finish(stream, &test_item_map, &item,
                                          test_array_stream_push_item, &seen,
                                          &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(error.code == LONEJSON_STATUS_OK);
    EXPECT(seen.count == 2u);
    lonejson_array_stream_close(stream);
  }

  memset(&seen, 0, sizeof(seen));
  stream = lonejson_array_stream_open_push("", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    status = lonejson_array_stream_push(stream, &test_item_map, &item,
                                        "[{\"id\":3,\"label\":\"thr\"}",
                                        strlen("[{\"id\":3,\"label\":\"thr\"}"),
                                        test_array_stream_push_item, &seen,
                                        &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(seen.count == 0u);
    status = lonejson_array_stream_push(stream, &test_item_map, &item, "x]",
                                        2u, test_array_stream_push_item, &seen,
                                        &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
    EXPECT(seen.count == 0u);
    lonejson_array_stream_close(stream);
  }

  memset(&seen, 0, sizeof(seen));
  stream = lonejson_array_stream_open_push("", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    status = lonejson_array_stream_push(stream, &test_item_map, &item,
                                        "[{\"id\":4,\"label\":\"cb\"}]",
                                        strlen("[{\"id\":4,\"label\":\"cb\"}]"),
                                        test_array_stream_push_fail_item, &seen,
                                        &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
    EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);
    status = lonejson_array_stream_push(stream, &test_item_map, &item, "[]", 2u,
                                        test_array_stream_push_item, &seen,
                                        &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
    lonejson_array_stream_close(stream);
  }

  stream = lonejson_array_stream_open_push("", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    status = lonejson_array_stream_finish(stream, &test_item_map, &item,
                                          test_array_stream_push_item, &seen,
                                          &error);
    EXPECT(status == LONEJSON_STATUS_TYPE_MISMATCH);
    lonejson_array_stream_close(stream);
  }

  stream = lonejson_array_stream_open_push("boards.items", NULL, &error);
  EXPECT(stream == NULL);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);

  stream = lonejson_array_stream_open_reader("", test_state_reader, NULL, NULL,
                                             &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    status = lonejson_array_stream_push(stream, &test_item_map, &item, "[]", 2u,
                                        test_array_stream_push_item, &seen,
                                        &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
    lonejson_array_stream_close(stream);
  }

  stream = lonejson_array_stream_open_push("", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    status = lonejson_array_stream_push(stream, &test_item_map, &item, "[]", 2u,
                                        NULL, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
    status = lonejson_array_stream_push(stream, &test_item_map, &item, NULL, 1u,
                                        test_array_stream_push_item, &seen,
                                        &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
    lonejson_array_stream_close(stream);
  }
}

static void test_array_stream_push_invariant_failure_modes(void) {
  lonejson_parse_options options = lonejson_default_parse_options();
  lonejson_array_stream *stream;
  lonejson_error error;
  lonejson_status status;
  test_array_stream_push_seen seen;
  test_item item;

  options.reject_duplicate_keys = 1;
  memset(&seen, 0, sizeof(seen));
  stream = lonejson_array_stream_open_push("items", &options, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    static const char json[] =
        "{\"meta\":{\"x\":1,\"\\u0078\":2},\"items\":[{\"id\":1}]}";
    status = lonejson_array_stream_push(stream, &test_item_map, &item, json,
                                        strlen(json),
                                        test_array_stream_push_item, &seen,
                                        &error);
    EXPECT(status == LONEJSON_STATUS_DUPLICATE_FIELD);
    EXPECT(seen.count == 0u);
    lonejson_array_stream_close(stream);
  }

  memset(&seen, 0, sizeof(seen));
  stream = lonejson_array_stream_open_push("items", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    static const char json[] = "{\"meta\":true}";
    status = lonejson_array_stream_push(stream, &test_item_map, &item, json,
                                        strlen(json),
                                        test_array_stream_push_item, &seen,
                                        &error);
    EXPECT(status == LONEJSON_STATUS_MISSING_REQUIRED_FIELD);
    EXPECT(seen.count == 0u);
    lonejson_array_stream_close(stream);
  }

  memset(&seen, 0, sizeof(seen));
  stream = lonejson_array_stream_open_push("items", &options, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    static const char json[] =
        "{\"items\":[{\"id\":2}],\"tail\":1,\"\\u0074ail\":2}";
    status = lonejson_array_stream_push(stream, &test_item_map, &item, json,
                                        strlen(json),
                                        test_array_stream_push_item, &seen,
                                        &error);
    EXPECT(status == LONEJSON_STATUS_DUPLICATE_FIELD);
    EXPECT(seen.count == 1u);
    EXPECT(seen.ids[0] == 2);
    lonejson_array_stream_close(stream);
  }

  memset(&seen, 0, sizeof(seen));
  stream = lonejson_array_stream_open_push("items", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    static const char json[] = "{\"items\":[{\"id\":3}],\"tail\":{bad}}";
    status = lonejson_array_stream_push(stream, &test_item_map, &item, json,
                                        strlen(json),
                                        test_array_stream_push_item, &seen,
                                        &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
    EXPECT(seen.count == 1u);
    EXPECT(seen.ids[0] == 3);
    lonejson_array_stream_close(stream);
  }

  memset(&seen, 0, sizeof(seen));
  stream = lonejson_array_stream_open_push("items", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    static const char json[] =
        "{\"\\u0069tems\":[{\"id\":6,\"label\":\"esc\"}]}";
    status = lonejson_array_stream_push(stream, &test_item_map, &item, json,
                                        strlen(json),
                                        test_array_stream_push_item, &seen,
                                        &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    status = lonejson_array_stream_finish(stream, &test_item_map, &item,
                                          test_array_stream_push_item, &seen,
                                          &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(seen.count == 1u);
    EXPECT(seen.ids[0] == 6);
    lonejson_array_stream_close(stream);
  }

  memset(&seen, 0, sizeof(seen));
  stream = lonejson_array_stream_open_push("", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    status = lonejson_array_stream_push(stream, &test_item_map, &item,
                                        "[{\"id\":7}", strlen("[{\"id\":7}"),
                                        test_array_stream_push_item, &seen,
                                        &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(seen.count == 0u);
    status = lonejson_array_stream_finish(stream, &test_item_map, &item,
                                          test_array_stream_push_item, &seen,
                                          &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
    EXPECT(seen.count == 0u);
    lonejson_array_stream_close(stream);
  }

  memset(&seen, 0, sizeof(seen));
  stream = lonejson_array_stream_open_push("", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    status = lonejson_array_stream_push(
        stream, &test_item_map, &item,
        "[{\"id\":8,\"label\":\"label-that-is-too-long\"}]",
        strlen("[{\"id\":8,\"label\":\"label-that-is-too-long\"}]"),
        test_array_stream_push_item, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_TRUNCATED);
    EXPECT(error.truncated);
    EXPECT(seen.count == 1u);
    EXPECT(seen.ids[0] == 8);
    EXPECT(strcmp(seen.labels[0], "label-that-") == 0);
    status = lonejson_array_stream_finish(stream, &test_item_map, &item,
                                          test_array_stream_push_item, &seen,
                                          &error);
    EXPECT(status == LONEJSON_STATUS_TRUNCATED);
    EXPECT(error.truncated);
    lonejson_array_stream_close(stream);
  }
}

static void test_array_stream_push_does_not_materialize_response(void) {
  char json[1024];
  size_t pos = 0u;
  size_t i;
  lonejson_allocator allocator = lonejson_default_allocator();
  lonejson_parse_options options = lonejson_default_parse_options();
  test_realloc_limit_state limit = {128u};
  lonejson_array_stream *stream;
  lonejson_error error;
  lonejson_status status;
  test_array_stream_push_seen seen;
  test_item item;
  static const char suffix[] = "\",\"items\":[{\"id\":5,\"label\":\"ok\"}]}";

  memcpy(json + pos, "{\"meta\":\"", 9u);
  pos += 9u;
  for (i = 0u; i < 700u; ++i) {
    json[pos++] = 'x';
  }
  memcpy(json + pos, suffix, sizeof(suffix) - 1u);
  pos += sizeof(suffix) - 1u;

  allocator.ctx = &limit;
  allocator.malloc_fn = test_realloc_limit_malloc;
  allocator.realloc_fn = test_realloc_limit_realloc;
  allocator.free_fn = test_realloc_limit_free;
  options.allocator = &allocator;

  memset(&seen, 0, sizeof(seen));
  stream = lonejson_array_stream_open_push("items", &options, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    return;
  }
  for (i = 0u; i < pos; ++i) {
    status = lonejson_array_stream_push(stream, &test_item_map, &item, json + i,
                                        1u, test_array_stream_push_item, &seen,
                                        &error);
    EXPECT(status == LONEJSON_STATUS_OK);
  }
  status = lonejson_array_stream_finish(stream, &test_item_map, &item,
                                        test_array_stream_push_item, &seen,
                                        &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(seen.count == 1u);
  EXPECT(seen.ids[0] == 5);
  lonejson_array_stream_close(stream);
}

static void test_array_stream_push_cleans_reused_destination(void) {
  static const char json[] =
      "[{\"name\":\"one\",\"tags\":[\"a\",\"b\"]},"
      "{\"name\":\"two\",\"tags\":[\"c\",\"d\"]}]";
  lonejson_parse_options options = lonejson_default_parse_options();
  test_allocator_state alloc_state;
  lonejson_array_stream *stream;
  lonejson_error error;
  lonejson_status status;
  test_alloc_parse_doc doc;
  test_array_stream_push_seen seen;

  test_allocator_init(&alloc_state);
  options.allocator = &alloc_state.allocator;
  memset(&seen, 0, sizeof(seen));
  stream = lonejson_array_stream_open_push("", &options, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    return;
  }

  status = lonejson_array_stream_push(stream, &test_alloc_parse_doc_map, &doc,
                                      json, strlen(json),
                                      test_array_stream_push_alloc_item, &seen,
                                      &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_array_stream_finish(stream, &test_alloc_parse_doc_map, &doc,
                                        test_array_stream_push_alloc_item,
                                        &seen, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(seen.count == 2u);
  EXPECT(strcmp(seen.labels[0], "one") == 0);
  EXPECT(strcmp(seen.labels[1], "two") == 0);
  lonejson_array_stream_close(stream);
  EXPECT(alloc_state.stats.bytes_live == 0u);

  memset(&seen, 0, sizeof(seen));
  stream = lonejson_array_stream_open_push("", &options, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    return;
  }
  status = lonejson_array_stream_push(
      stream, &test_alloc_parse_doc_map, &doc,
      "[{\"name\":\"fail\",\"tags\":[\"owned\"]}]",
      strlen("[{\"name\":\"fail\",\"tags\":[\"owned\"]}]"),
      test_array_stream_push_alloc_fail_item, &seen, &error);
  EXPECT(status == LONEJSON_STATUS_IO_ERROR);
  lonejson_array_stream_close(stream);
  EXPECT(alloc_state.stats.bytes_live == 0u);
}

static void test_array_stream_push_string_items(void) {
  static const char json[] =
      "{\"cursor\":\"c1\",\"keys\":[\"alpha\",\"br\\u0061vo\",\"line\\nkey\"],"
      "\"index_seq\":7}";
  lonejson_array_stream_string_handler handler =
      test_array_stream_string_handler();
  lonejson_array_stream *stream;
  lonejson_error error;
  lonejson_status status;
  test_array_stream_string_seen seen;
  size_t i;

  memset(&seen, 0, sizeof(seen));
  stream = lonejson_array_stream_open_push("keys", NULL, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    return;
  }
  for (i = 0u; i < strlen(json); ++i) {
    status = lonejson_array_stream_push_string(stream, json + i, 1u, &handler,
                                               &seen, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
  }
  status =
      lonejson_array_stream_finish_string(stream, &handler, &seen, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(seen.begins == 3u);
  EXPECT(seen.ends == 3u);
  EXPECT(seen.chunks >= 3u);
  EXPECT(strcmp(seen.items[0], "alpha") == 0);
  EXPECT(strcmp(seen.items[1], "bravo") == 0);
  EXPECT(strcmp(seen.items[2], "line\nkey") == 0);
  lonejson_array_stream_close(stream);
}

static void test_array_stream_push_string_item_callback(void) {
  static const char json[] =
      "{\"cursor\":\"c1\",\"keys\":[\"\",\"alpha\",\"\",\"br\\u0061vo\","
      "\"line\\nkey\"],"
      "\"index_seq\":7}";
  lonejson_array_stream *stream;
  lonejson_error error;
  lonejson_status status;
  test_array_stream_string_seen seen;
  size_t i;

  memset(&seen, 0, sizeof(seen));
  stream = lonejson_array_stream_open_push("keys", NULL, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    return;
  }
  for (i = 0u; i < strlen(json); ++i) {
    status = lonejson_array_stream_push_string_items(
        stream, json + i, 1u, test_array_stream_string_item, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
  }
  status = lonejson_array_stream_finish_string_items(
      stream, test_array_stream_string_item, &seen, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(seen.ends == 5u);
  EXPECT(seen.null_items == 0u);
  EXPECT(seen.unterminated_items == 0u);
  EXPECT(strcmp(seen.items[0], "") == 0);
  EXPECT(strcmp(seen.items[1], "alpha") == 0);
  EXPECT(strcmp(seen.items[2], "") == 0);
  EXPECT(strcmp(seen.items[3], "bravo") == 0);
  EXPECT(strcmp(seen.items[4], "line\nkey") == 0);
  lonejson_array_stream_close(stream);

  memset(&seen, 0, sizeof(seen));
  seen.fail_after_chunks = 1u;
  stream = lonejson_array_stream_open_push("keys", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    status = lonejson_array_stream_push_string_items(
        stream, "{\"keys\":[\"fail\"]}", strlen("{\"keys\":[\"fail\"]}"),
        test_array_stream_string_item, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_IO_ERROR);
    lonejson_array_stream_close(stream);
  }
}

static void test_array_stream_push_string_item_callback_bounds(void) {
  const char prefix[] = "{\"keys\":[\"";
  const char suffix[] = "\"]}";
  lonejson_array_stream *stream;
  lonejson_error error;
  lonejson_status status;
  test_array_stream_string_seen seen;
  lonejson_value_limits limits;
  char *json;
  size_t string_len;
  size_t total;

  limits = lonejson_default_value_limits();
  string_len = limits.max_string_bytes + 1u;
  total = strlen(prefix) + string_len + strlen(suffix);
  json = (char *)malloc(total + 1u);
  EXPECT(json != NULL);
  if (json == NULL) {
    return;
  }
  memcpy(json, prefix, strlen(prefix));
  memset(json + strlen(prefix), 'a', string_len);
  memcpy(json + strlen(prefix) + string_len, suffix, strlen(suffix));
  json[total] = '\0';

  memset(&seen, 0, sizeof(seen));
  stream = lonejson_array_stream_open_push("keys", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    status = lonejson_array_stream_push_string_items(
        stream, json, total, test_array_stream_string_item, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_OVERFLOW);
    EXPECT(seen.ends == 0u);
    lonejson_array_stream_close(stream);
  }
  free(json);
}

static void test_array_stream_push_string_failure_modes(void) {
  lonejson_array_stream_string_handler handler =
      test_array_stream_string_handler();
  lonejson_array_stream *stream;
  lonejson_error error;
  lonejson_status status;
  test_array_stream_string_seen seen;

  memset(&seen, 0, sizeof(seen));
  stream = lonejson_array_stream_open_push("keys", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    static const char json[] = "{\"keys\":[\"ok\",42]}";
    status = lonejson_array_stream_push_string(stream, json, strlen(json),
                                               &handler, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_TYPE_MISMATCH);
    EXPECT(seen.ends == 1u);
    lonejson_array_stream_close(stream);
  }

  memset(&seen, 0, sizeof(seen));
  stream = lonejson_array_stream_open_push("keys", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    static const char json[] = "{\"keys\":[\"ok\"x]}";
    status = lonejson_array_stream_push_string(stream, json, strlen(json),
                                               &handler, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
    EXPECT(seen.begins == 1u);
    EXPECT(seen.ends == 0u);
    lonejson_array_stream_close(stream);
  }

  memset(&seen, 0, sizeof(seen));
  stream = lonejson_array_stream_open_push("keys", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    static const char json[] = "{\"meta\":true}";
    status = lonejson_array_stream_push_string(stream, json, strlen(json),
                                               &handler, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_MISSING_REQUIRED_FIELD);
    EXPECT(seen.begins == 0u);
    lonejson_array_stream_close(stream);
  }

  memset(&seen, 0, sizeof(seen));
  seen.fail_after_chunks = 1u;
  stream = lonejson_array_stream_open_push("keys", NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    static const char json[] = "{\"keys\":[\"fail\"]}";
    status = lonejson_array_stream_push_string(stream, json, strlen(json),
                                               &handler, &seen, &error);
    EXPECT(status == LONEJSON_STATUS_IO_ERROR);
    lonejson_array_stream_close(stream);
  }
}
