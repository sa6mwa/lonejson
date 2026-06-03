#ifdef LONEJSON_WITH_CURL

static int test_child_curl_upload_cleanup_default_allocator(void) {
  test_event event;
  lonejson_curl_upload upload;

  memset(&event, 0, sizeof(event));
  strcpy(event.id, "evt-1");
  event.ok = true;

  if (lonejson_curl_upload_init(&upload, test_default_runtime(),
                                &test_event_map,
                                &event) != LONEJSON_STATUS_OK) {
    return 2;
  }
  lonejson_curl_upload_cleanup(&upload);
  return 0;
}

static void test_curl_upload_cleanup_default_allocator(void) {
  pid_t pid;
  int status;

  pid = fork();
  EXPECT(pid >= 0);
  if (pid == 0) {
    _exit(test_child_curl_upload_cleanup_default_allocator());
  }
  if (pid < 0) {
    return;
  }
  EXPECT(waitpid(pid, &status, 0) == pid);
  EXPECT(WIFEXITED(status));
  if (WIFEXITED(status)) {
    EXPECT(WEXITSTATUS(status) == 0);
  }
}

static void test_curl_upload_custom_allocator_balance(void) {
  test_event event;
  lonejson_config config;
  test_allocator_state write_alloc;
  lonejson *runtime;
  lonejson_curl_upload upload;
  char buffer[128];
  size_t total;
  size_t runtime_bytes_live;

  memset(&event, 0, sizeof(event));
  strcpy(event.id, "evt-1");
  event.ok = true;
  test_allocator_init(&write_alloc);
  config = lonejson_default_config();
  config.allocator = &write_alloc.allocator;
  runtime = lonejson_new(&config, NULL);
  EXPECT(runtime != NULL);
  if (runtime == NULL) {
    return;
  }
  runtime_bytes_live = write_alloc.stats.bytes_live;

  EXPECT(lonejson_curl_upload_init(&upload, runtime, &test_event_map, &event) ==
         LONEJSON_STATUS_OK);
  EXPECT(write_alloc.stats.alloc_calls > 0u);
  EXPECT(write_alloc.stats.bytes_live > 0u);
  total = lonejson_curl_read_callback(buffer, 1u, sizeof(buffer) - 1u, &upload);
  EXPECT(total != 0u);
  EXPECT(total != CURL_READFUNC_ABORT);
  buffer[total] = '\0';
  EXPECT(strstr(buffer, "\"id\":\"evt-1\"") != NULL);
  lonejson_curl_upload_cleanup(&upload);
  EXPECT(write_alloc.stats.bytes_live == runtime_bytes_live);
  EXPECT(write_alloc.stats.free_calls > 0u);
  lonejson_free(runtime);
  EXPECT(write_alloc.stats.bytes_live == 0u);
}

static void test_curl_upload_streaming_does_not_buffer_payload(void) {
  test_source_doc doc;
  lonejson_config config;
  test_allocator_state write_alloc;
  lonejson *runtime;
  lonejson_curl_upload upload;
  char path[] = "/tmp/lonejson-curl-upload-XXXXXX";
  char chunk[4096];
  int fd;
  FILE *fp;
  size_t total;
  size_t i;
  size_t runtime_bytes_live;

  memset(&doc, 0, sizeof(doc));
  strcpy(doc.id, "evt-1");
  lonejson_source_init(&doc.text);
  lonejson_source_init(&doc.bytes);
  test_allocator_init(&write_alloc);
  config = lonejson_default_config();
  config.allocator = &write_alloc.allocator;
  runtime = lonejson_new(&config, NULL);
  EXPECT(runtime != NULL);
  if (runtime == NULL) {
    lonejson_cleanup(&test_source_doc_map, &doc);
    return;
  }
  runtime_bytes_live = write_alloc.stats.bytes_live;

  fd = mkstemp(path);
  EXPECT(fd >= 0);
  if (fd < 0) {
    return;
  }
  fp = fdopen(fd, "wb");
  EXPECT(fp != NULL);
  if (fp == NULL) {
    close(fd);
    unlink(path);
    return;
  }
  for (i = 0u; i < 262144u; ++i) {
    fputc('a', fp);
  }
  fclose(fp);

  EXPECT(lonejson_source_set_path(&doc.text, path, NULL) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_curl_upload_init(&upload, runtime, &test_source_doc_map,
                                   &doc) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_curl_upload_size(&upload) == (curl_off_t)-1);
  EXPECT(write_alloc.stats.peak_bytes_live < 65536u);

  total = 0u;
  for (;;) {
    size_t got = lonejson_curl_read_callback(chunk, 1u, sizeof(chunk), &upload);
    if (got == 0u) {
      break;
    }
    EXPECT(got != CURL_READFUNC_ABORT);
    total += got;
  }
  EXPECT(upload.generator.error.code == LONEJSON_STATUS_OK ||
         upload.generator.error.code == LONEJSON_STATUS_TRUNCATED);
  EXPECT(total > 262144u);
  EXPECT(write_alloc.stats.peak_bytes_live < 65536u);
  lonejson_curl_upload_cleanup(&upload);
  EXPECT(write_alloc.stats.bytes_live == runtime_bytes_live);
  lonejson_free(runtime);
  EXPECT(write_alloc.stats.bytes_live == 0u);
  lonejson_cleanup(&test_source_doc_map, &doc);
  unlink(path);
}

static void test_curl_parse_survives_runtime_free(void) {
  static const char chunk0[] =
      "{\"type\":\"query\",\"response\":{\"status\":\"ok\",";
  static const char chunk1[] = "\"payload\":{\"value\":\"abc\"}}}";
  lonejson_config config;
  lonejson *runtime;
  lonejson_curl_parse parse;
  test_nested_json_value_doc doc;
  lonejson_error error;
  lonejson_status status;
  test_buffer_sink sink;
  unsigned char sink_bytes[128];
  size_t wrote;

  memset(&doc, 0, sizeof(doc));
  config = lonejson_default_config();
  config.clear_destination_by_default = 0;
  runtime = lonejson_new(&config, &error);
  EXPECT(runtime != NULL);
  if (runtime == NULL) {
    return;
  }

  lonejson_init(runtime, &test_nested_json_value_doc_map, &doc);
  EXPECT(lonejson_json_value_enable_parse_capture(
             &doc.response.payload, &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_curl_parse_init(&parse, runtime,
                                  &test_nested_json_value_doc_map,
                                  &doc) == LONEJSON_STATUS_OK);
  lonejson_free(runtime);

  wrote =
      lonejson_curl_write_callback((char *)chunk0, 1u, strlen(chunk0), &parse);
  EXPECT(wrote == strlen(chunk0));
  wrote =
      lonejson_curl_write_callback((char *)chunk1, 1u, strlen(chunk1), &parse);
  EXPECT(wrote == strlen(chunk1));
  status = lonejson_curl_parse_finish(&parse);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(doc.type, "query") == 0);
  EXPECT(strcmp(doc.response.status, "ok") == 0);
  sink.buffer = sink_bytes;
  sink.capacity = sizeof(sink_bytes);
  sink.length = 0u;
  EXPECT(lonejson_json_value_write_to_sink(&doc.response.payload,
                                           test_buffer_sink_write, &sink,
                                           &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp((char *)sink_bytes, "{\"value\":\"abc\"}") == 0);

  lonejson_curl_parse_cleanup(&parse);
  lonejson_cleanup(&test_nested_json_value_doc_map, &doc);
}

static void test_curl_parse_reinit_releases_previous_parser(void) {
  lonejson_config config;
  lonejson_allocator allocator = {test_runtime_allocator_malloc,
                                  test_runtime_allocator_realloc,
                                  test_runtime_allocator_free, NULL, NULL};
  lonejson *runtime;
  lonejson_curl_parse parse;
  test_nested_json_value_doc doc0;
  test_nested_json_value_doc doc1;
  lonejson_error error;
  size_t alloc_after_first;

  memset(&parse, 0, sizeof(parse));
  memset(&doc0, 0, sizeof(doc0));
  memset(&doc1, 0, sizeof(doc1));
  reset_lonejson_alloc_stats();

  config = lonejson_default_config();
  config.allocator = &allocator;
  config.clear_destination_by_default = 0;
  runtime = lonejson_new(&config, &error);
  EXPECT(runtime != NULL);
  if (runtime == NULL) {
    return;
  }

  lonejson_init(runtime, &test_nested_json_value_doc_map, &doc0);
  EXPECT(lonejson_json_value_enable_parse_capture(
             &doc0.response.payload, &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_curl_parse_init(&parse, runtime,
                                  &test_nested_json_value_doc_map,
                                  &doc0) == LONEJSON_STATUS_OK);
  alloc_after_first = g_lonejson_alloc_calls;
  EXPECT(alloc_after_first > 0u);

  lonejson_init(runtime, &test_nested_json_value_doc_map, &doc1);
  EXPECT(lonejson_json_value_enable_parse_capture(
             &doc1.response.payload, &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_curl_parse_init(&parse, runtime,
                                  &test_nested_json_value_doc_map,
                                  &doc1) == LONEJSON_STATUS_OK);
  EXPECT(g_lonejson_free_calls > 0u);
  EXPECT(g_alloc_record_count > 0u);

  lonejson_curl_parse_cleanup(&parse);
  lonejson_cleanup(&test_nested_json_value_doc_map, &doc0);
  lonejson_cleanup(&test_nested_json_value_doc_map, &doc1);
  lonejson_free(runtime);
  EXPECT(g_alloc_record_count == 0u);
}

static void test_curl_parse_init_accepts_poisoned_stack_state(void) {
  static const char json[] = "{\"id\":\"evt-1\",\"ok\":true}";
  lonejson_curl_parse parse;
  test_event doc;
  size_t wrote;

  memset(&parse, 0xA5, sizeof(parse));
  memset(&doc, 0, sizeof(doc));
  lonejson_init(test_default_runtime(), &test_event_map, &doc);

  EXPECT(lonejson_curl_parse_init(&parse, test_default_runtime(),
                                  &test_event_map, &doc) == LONEJSON_STATUS_OK);
  wrote = lonejson_curl_write_callback((char *)json, 1u, strlen(json), &parse);
  EXPECT(wrote == strlen(json));
  EXPECT(lonejson_curl_parse_finish(&parse) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(doc.id, "evt-1") == 0);
  EXPECT(doc.ok);

  lonejson_curl_parse_cleanup(&parse);
  lonejson_cleanup(&test_event_map, &doc);
}

typedef struct test_curl_array_seen {
  int ids[8];
  size_t count;
} test_curl_array_seen;

typedef struct test_curl_string_seen {
  char items[8][32];
  size_t lens[8];
  size_t begins;
  size_t ends;
  size_t current;
  size_t null_items;
  size_t unterminated_items;
} test_curl_string_seen;

static lonejson_status test_curl_array_item(void *user, void *dst) {
  test_curl_array_seen *seen = (test_curl_array_seen *)user;
  test_item *item = (test_item *)dst;

  if (seen->count >= sizeof(seen->ids) / sizeof(seen->ids[0])) {
    return LONEJSON_STATUS_OVERFLOW;
  }
  seen->ids[seen->count++] = (int)item->id;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_curl_string_begin(void *user,
                                              lonejson_error *error) {
  test_curl_string_seen *seen = (test_curl_string_seen *)user;
  (void)error;
  seen->current = seen->begins++;
  seen->lens[seen->current] = 0u;
  seen->items[seen->current][0] = '\0';
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_curl_string_chunk(void *user, const char *data,
                                              size_t len,
                                              lonejson_error *error) {
  test_curl_string_seen *seen = (test_curl_string_seen *)user;
  size_t room;
  (void)error;
  room = sizeof(seen->items[seen->current]) - 1u - seen->lens[seen->current];
  if (len > room) {
    len = room;
  }
  memcpy(seen->items[seen->current] + seen->lens[seen->current], data, len);
  seen->lens[seen->current] += len;
  seen->items[seen->current][seen->lens[seen->current]] = '\0';
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_curl_string_end(void *user, lonejson_error *error) {
  test_curl_string_seen *seen = (test_curl_string_seen *)user;
  (void)error;
  seen->ends++;
  return LONEJSON_STATUS_OK;
}

static lonejson_array_stream_string_handler test_curl_string_handler(void) {
  lonejson_array_stream_string_handler handler;
  memset(&handler, 0, sizeof(handler));
  handler.begin = test_curl_string_begin;
  handler.chunk = test_curl_string_chunk;
  handler.end = test_curl_string_end;
  return handler;
}

static lonejson_status test_curl_string_item(void *user, const char *data,
                                             size_t len,
                                             lonejson_error *error) {
  test_curl_string_seen *seen = (test_curl_string_seen *)user;
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
  room = sizeof(seen->items[seen->ends]) - 1u;
  if (len > room) {
    len = room;
  }
  memcpy(seen->items[seen->ends], data, len);
  seen->items[seen->ends][len] = '\0';
  seen->lens[seen->ends] = len;
  seen->ends++;
  return LONEJSON_STATUS_OK;
}

static void test_curl_array_parse_streams_selected_arrays(void) {
  static const char json[] =
      "{\"meta\":{\"ignored\":true},\"items\":[{\"id\":11,\"label\":\"a\"},"
      "{\"id\":12,\"label\":\"b\"}]}";
  lonejson_curl_array_parse parse;
  test_curl_array_seen seen;
  test_item item;
  size_t i;
  size_t got;

  memset(&seen, 0, sizeof(seen));
  EXPECT(lonejson_curl_array_parse_init(
             &parse, test_default_runtime(), "items", &test_item_map, &item,
             test_curl_array_item, &seen) == LONEJSON_STATUS_OK);
  for (i = 0u; i < strlen(json); ++i) {
    got = lonejson_curl_array_write_callback((char *)json + i, 1u, 1u, &parse);
    EXPECT(got == 1u);
  }
  EXPECT(seen.count == 2u);
  EXPECT(seen.ids[0] == 11);
  EXPECT(seen.ids[1] == 12);
  EXPECT(lonejson_curl_array_parse_finish(&parse) == LONEJSON_STATUS_OK);
  lonejson_curl_array_parse_cleanup(&parse);

  memset(&seen, 0, sizeof(seen));
  EXPECT(lonejson_curl_array_parse_init(
             &parse, test_default_runtime(), "", &test_item_map, &item,
             test_curl_array_item, &seen) == LONEJSON_STATUS_OK);
  got = lonejson_curl_array_write_callback(
      "[{\"id\":21,\"label\":\"root\"}]", 1u,
      strlen("[{\"id\":21,\"label\":\"root\"}]"), &parse);
  EXPECT(got == strlen("[{\"id\":21,\"label\":\"root\"}]"));
  EXPECT(lonejson_curl_array_parse_finish(&parse) == LONEJSON_STATUS_OK);
  EXPECT(seen.count == 1u);
  EXPECT(seen.ids[0] == 21);
  lonejson_curl_array_parse_cleanup(&parse);
}

static void test_curl_array_parse_failure_cleanup_and_truncation(void) {
  lonejson_curl_array_parse parse;
  test_curl_array_seen seen;
  test_item item;
  size_t got;

  memset(&parse, 0xA5, sizeof(parse));
  EXPECT(lonejson_curl_array_parse_init(
             &parse, test_default_runtime(), "", NULL, &item,
             test_curl_array_item, &seen) == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(parse.stream == NULL);
  EXPECT(parse.error.code == LONEJSON_STATUS_INVALID_ARGUMENT);
  lonejson_curl_array_parse_cleanup(&parse);

  memset(&seen, 0, sizeof(seen));
  EXPECT(lonejson_curl_array_parse_init(
             &parse, test_default_runtime(), "", &test_item_map, &item,
             test_curl_array_item, &seen) == LONEJSON_STATUS_OK);
  got = lonejson_curl_array_write_callback(
      "[{\"id\":31,\"label\":\"label-that-is-too-long\"}]", 1u,
      strlen("[{\"id\":31,\"label\":\"label-that-is-too-long\"}]"), &parse);
  EXPECT(got == strlen("[{\"id\":31,\"label\":\"label-that-is-too-long\"}]"));
  EXPECT(parse.error.code == LONEJSON_STATUS_TRUNCATED);
  EXPECT(parse.error.truncated);
  EXPECT(seen.count == 1u);
  EXPECT(seen.ids[0] == 31);
  EXPECT(lonejson_curl_array_parse_finish(&parse) == LONEJSON_STATUS_TRUNCATED);
  EXPECT(parse.error.truncated);
  lonejson_curl_array_parse_cleanup(&parse);

  memset(&seen, 0, sizeof(seen));
  EXPECT(lonejson_curl_array_parse_init(
             &parse, test_default_runtime(), "items", &test_item_map, &item,
             test_curl_array_item, &seen) == LONEJSON_STATUS_OK);
  got = lonejson_curl_array_write_callback("{\"meta\":true}", 1u,
                                           strlen("{\"meta\":true}"), &parse);
  EXPECT(got == 0u);
  EXPECT(parse.error.code == LONEJSON_STATUS_MISSING_REQUIRED_FIELD);
  EXPECT(seen.count == 0u);
  lonejson_curl_array_parse_cleanup(&parse);

  memset(&seen, 0, sizeof(seen));
  EXPECT(lonejson_curl_array_parse_init(
             &parse, test_default_runtime(), "items", &test_item_map, &item,
             test_curl_array_item, &seen) == LONEJSON_STATUS_OK);
  got = lonejson_curl_array_write_callback(
      "{\"items\":[{\"id\":41}],\"tail\":{bad}}", 1u,
      strlen("{\"items\":[{\"id\":41}],\"tail\":{bad}}"), &parse);
  EXPECT(got == 0u);
  EXPECT(parse.error.code == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(seen.count == 1u);
  EXPECT(seen.ids[0] == 41);
  lonejson_curl_array_parse_cleanup(&parse);
}

static void test_curl_array_parse_reinit_releases_previous_stream(void) {
  lonejson_config config;
  lonejson_allocator allocator = {test_runtime_allocator_malloc,
                                  test_runtime_allocator_realloc,
                                  test_runtime_allocator_free, NULL, NULL};
  lonejson *runtime;
  lonejson_curl_array_parse parse;
  test_curl_array_seen seen;
  test_item item;

  memset(&parse, 0xA5, sizeof(parse));
  memset(&seen, 0, sizeof(seen));
  memset(&item, 0, sizeof(item));
  reset_lonejson_alloc_stats();

  config = lonejson_default_config();
  config.allocator = &allocator;
  runtime = lonejson_new(&config, NULL);
  EXPECT(runtime != NULL);
  if (runtime == NULL) {
    return;
  }

  EXPECT(lonejson_curl_array_parse_init(
             &parse, runtime, "items", &test_item_map, &item,
             test_curl_array_item, &seen) == LONEJSON_STATUS_OK);
  EXPECT(g_lonejson_alloc_calls > 0u);
  EXPECT(lonejson_curl_array_parse_init(&parse, runtime, "", &test_item_map,
                                        &item, test_curl_array_item,
                                        &seen) == LONEJSON_STATUS_OK);
  EXPECT(g_lonejson_free_calls > 0u);

  lonejson_curl_array_parse_cleanup(&parse);
  lonejson_free(runtime);
  EXPECT(g_alloc_record_count == 0u);
}

static void test_curl_string_array_parse_streams_keys(void) {
  static const char json[] =
      "{\"cursor\":\"c\",\"keys\":[\"k1\",\"k\\u0032\"],\"index_seq\":9}";
  lonejson_curl_string_array_parse parse;
  lonejson_array_stream_string_handler handler = test_curl_string_handler();
  test_curl_string_seen seen;
  size_t i;
  size_t got;

  memset(&seen, 0, sizeof(seen));
  EXPECT(lonejson_curl_string_array_parse_init(&parse, test_default_runtime(),
                                               "keys", &handler,
                                               &seen) == LONEJSON_STATUS_OK);
  for (i = 0u; i < strlen(json); ++i) {
    got = lonejson_curl_string_array_write_callback((char *)json + i, 1u, 1u,
                                                    &parse);
    EXPECT(got == 1u);
  }
  EXPECT(lonejson_curl_string_array_parse_finish(&parse) == LONEJSON_STATUS_OK);
  EXPECT(seen.begins == 2u);
  EXPECT(seen.ends == 2u);
  EXPECT(strcmp(seen.items[0], "k1") == 0);
  EXPECT(strcmp(seen.items[1], "k2") == 0);
  lonejson_curl_string_array_parse_cleanup(&parse);

  memset(&seen, 0, sizeof(seen));
  EXPECT(lonejson_curl_string_array_parse_init(&parse, test_default_runtime(),
                                               "keys", &handler,
                                               &seen) == LONEJSON_STATUS_OK);
  got = lonejson_curl_string_array_write_callback(
      "{\"keys\":[1]}", 1u, strlen("{\"keys\":[1]}"), &parse);
  EXPECT(got == 0u);
  EXPECT(parse.error.code == LONEJSON_STATUS_TYPE_MISMATCH);
  lonejson_curl_string_array_parse_cleanup(&parse);
}

static void test_curl_string_array_parse_reinit_releases_previous_stream(void) {
  lonejson_config config;
  lonejson_allocator allocator = {test_runtime_allocator_malloc,
                                  test_runtime_allocator_realloc,
                                  test_runtime_allocator_free, NULL, NULL};
  lonejson *runtime;
  lonejson_curl_string_array_parse parse;
  lonejson_array_stream_string_handler handler = test_curl_string_handler();
  test_curl_string_seen seen;

  memset(&parse, 0xA5, sizeof(parse));
  memset(&seen, 0, sizeof(seen));
  reset_lonejson_alloc_stats();

  config = lonejson_default_config();
  config.allocator = &allocator;
  runtime = lonejson_new(&config, NULL);
  EXPECT(runtime != NULL);
  if (runtime == NULL) {
    return;
  }

  EXPECT(lonejson_curl_string_array_parse_init(
             &parse, runtime, "keys", &handler, &seen) == LONEJSON_STATUS_OK);
  EXPECT(g_lonejson_alloc_calls > 0u);
  EXPECT(lonejson_curl_string_array_parse_init(&parse, runtime, "", &handler,
                                               &seen) == LONEJSON_STATUS_OK);
  EXPECT(g_lonejson_free_calls > 0u);

  lonejson_curl_string_array_parse_cleanup(&parse);
  lonejson_free(runtime);
  EXPECT(g_alloc_record_count == 0u);
}

static void test_curl_string_items_parse_streams_keys(void) {
  static const char json[] =
      "{\"cursor\":\"c\",\"keys\":[\"\",\"k1\",\"\",\"k\\u0032\"],"
      "\"index_seq\":9}";
  lonejson_curl_string_items_parse parse;
  test_curl_string_seen seen;
  size_t i;
  size_t got;

  memset(&seen, 0, sizeof(seen));
  EXPECT(lonejson_curl_string_items_parse_init(&parse, test_default_runtime(),
                                               "keys", test_curl_string_item,
                                               &seen) == LONEJSON_STATUS_OK);
  for (i = 0u; i < strlen(json); ++i) {
    got = lonejson_curl_string_items_write_callback((char *)json + i, 1u, 1u,
                                                    &parse);
    EXPECT(got == 1u);
  }
  EXPECT(lonejson_curl_string_items_parse_finish(&parse) == LONEJSON_STATUS_OK);
  EXPECT(seen.ends == 4u);
  EXPECT(seen.null_items == 0u);
  EXPECT(seen.unterminated_items == 0u);
  EXPECT(strcmp(seen.items[0], "") == 0);
  EXPECT(strcmp(seen.items[1], "k1") == 0);
  EXPECT(strcmp(seen.items[2], "") == 0);
  EXPECT(strcmp(seen.items[3], "k2") == 0);
  lonejson_curl_string_items_parse_cleanup(&parse);

  memset(&seen, 0, sizeof(seen));
  EXPECT(lonejson_curl_string_items_parse_init(&parse, test_default_runtime(),
                                               "keys", test_curl_string_item,
                                               &seen) == LONEJSON_STATUS_OK);
  got = lonejson_curl_string_items_write_callback(
      "{\"keys\":[1]}", 1u, strlen("{\"keys\":[1]}"), &parse);
  EXPECT(got == 0u);
  EXPECT(parse.error.code == LONEJSON_STATUS_TYPE_MISMATCH);
  lonejson_curl_string_items_parse_cleanup(&parse);
}

static void test_curl_string_items_parse_reinit_releases_previous_stream(void) {
  lonejson_config config;
  lonejson_allocator allocator = {test_runtime_allocator_malloc,
                                  test_runtime_allocator_realloc,
                                  test_runtime_allocator_free, NULL, NULL};
  lonejson *runtime;
  lonejson_curl_string_items_parse parse;
  test_curl_string_seen seen;

  memset(&parse, 0xA5, sizeof(parse));
  memset(&seen, 0, sizeof(seen));
  reset_lonejson_alloc_stats();

  config = lonejson_default_config();
  config.allocator = &allocator;
  runtime = lonejson_new(&config, NULL);
  EXPECT(runtime != NULL);
  if (runtime == NULL) {
    return;
  }

  EXPECT(lonejson_curl_string_items_parse_init(&parse, runtime, "keys",
                                               test_curl_string_item,
                                               &seen) == LONEJSON_STATUS_OK);
  EXPECT(g_lonejson_alloc_calls > 0u);
  EXPECT(lonejson_curl_string_items_parse_init(&parse, runtime, "",
                                               test_curl_string_item,
                                               &seen) == LONEJSON_STATUS_OK);
  EXPECT(g_lonejson_free_calls > 0u);

  lonejson_curl_string_items_parse_cleanup(&parse);
  lonejson_free(runtime);
  EXPECT(g_alloc_record_count == 0u);
}

static void test_curl_upload_reinit_releases_previous_generator(void) {
  test_event event;
  lonejson_config config;
  lonejson_allocator allocator = {test_runtime_allocator_malloc,
                                  test_runtime_allocator_realloc,
                                  test_runtime_allocator_free, NULL, NULL};
  lonejson *runtime;
  lonejson_curl_upload upload;

  memset(&event, 0, sizeof(event));
  strcpy(event.id, "evt-1");
  event.ok = true;
  memset(&upload, 0xA5, sizeof(upload));
  reset_lonejson_alloc_stats();

  config = lonejson_default_config();
  config.allocator = &allocator;
  runtime = lonejson_new(&config, NULL);
  EXPECT(runtime != NULL);
  if (runtime == NULL) {
    return;
  }

  EXPECT(lonejson_curl_upload_init(&upload, runtime, &test_event_map, &event) ==
         LONEJSON_STATUS_OK);
  EXPECT(g_lonejson_alloc_calls > 0u);
  EXPECT(lonejson_curl_upload_init(&upload, runtime, &test_event_map, &event) ==
         LONEJSON_STATUS_OK);
  EXPECT(g_lonejson_free_calls > 0u);

  lonejson_curl_upload_cleanup(&upload);
  lonejson_free(runtime);
  EXPECT(g_alloc_record_count == 0u);
}
#endif

static int test_child_parse_poisoned_stream_and_json_value_fields(void) {
  test_alloc_parse_doc stream_doc;
  test_alloc_json_value_doc json_doc;

  memset(&stream_doc, 0xA5, sizeof(stream_doc));
  stream_doc.body._lonejson_magic = LONEJSON__SPOOLED_MAGIC;
  stream_doc.body.memory = (unsigned char *)1;
  stream_doc.body.spill_fp = (FILE *)1;
  test_init_map(&test_alloc_parse_doc_map, &stream_doc);
  lonejson_cleanup(&test_alloc_parse_doc_map, &stream_doc);

  memset(&json_doc, 0xA5, sizeof(json_doc));
  json_doc.selector._lonejson_magic = LONEJSON__JSON_VALUE_MAGIC;
  json_doc.selector.json = (char *)1;
  json_doc.selector.path = (char *)1;
  test_init_map(&test_alloc_json_value_doc_map, &json_doc);
  lonejson_cleanup(&test_alloc_json_value_doc_map, &json_doc);
  return 0;
}

static void
test_clear_destination_ignores_poisoned_stream_and_json_value_state(void) {
  pid_t pid;
  int status;

  pid = fork();
  EXPECT(pid >= 0);
  if (pid == 0) {
    _exit(test_child_parse_poisoned_stream_and_json_value_fields());
  }
  if (pid < 0) {
    return;
  }
  EXPECT(waitpid(pid, &status, 0) == pid);
  EXPECT(WIFEXITED(status));
  EXPECT(WEXITSTATUS(status) == 0);
}

static void test_prepared_fixed_object_array_parse_preserves_storage(void) {
  static const char json[] =
      "{\"results\":["
      "{\"name\":\"parse/buffer_fixed/lonejson\",\"group\":\"parse\"},"
      "{\"name\":\"serialize/sink/lonejson\",\"group\":\"serialize\"}]}";
  test_fixed_result_run run;
  lonejson__parse_options options;
  lonejson_error error;
  lonejson_status status;

  memset(&run, 0, sizeof(run));
  run.results.items = run.result_storage;
  run.results.capacity =
      sizeof(run.result_storage) / sizeof(run.result_storage[0]);
  run.results.elem_size = sizeof(run.result_storage[0]);
  run.results.flags = LONEJSON_ARRAY_FIXED_CAPACITY;

  options = lonejson__default_parse_options();
  options.clear_destination = 0;
  status =
      test_parse_cstr(&test_fixed_result_run_map, &run, json, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(run.results.items == run.result_storage);
  EXPECT(run.results.count == 2u);
  EXPECT(strcmp(run.result_storage[0].name, "parse/buffer_fixed/lonejson") ==
         0);
  EXPECT(strcmp(run.result_storage[0].group, "parse") == 0);
  EXPECT(strcmp(run.result_storage[1].name, "serialize/sink/lonejson") == 0);
  EXPECT(strcmp(run.result_storage[1].group, "serialize") == 0);
  lonejson_cleanup(&test_fixed_result_run_map, &run);
}
