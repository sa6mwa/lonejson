#ifdef LONEJSON_WITH_CURL

static int test_child_curl_upload_cleanup_default_allocator(void) {
  test_event event;
  lonejson_curl_upload upload;

  memset(&event, 0, sizeof(event));
  strcpy(event.id, "evt-1");
  event.ok = true;

  if (lonejson_curl_upload_init(&upload, &test_event_map, &event, NULL) !=
      LONEJSON_STATUS_OK) {
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
  test_allocator_state write_alloc;
  lonejson_write_options options = lonejson_default_write_options();
  lonejson_curl_upload upload;
  char buffer[128];
  size_t total;

  memset(&event, 0, sizeof(event));
  strcpy(event.id, "evt-1");
  event.ok = true;
  test_allocator_init(&write_alloc);
  options.allocator = &write_alloc.allocator;

  EXPECT(lonejson_curl_upload_init(&upload, &test_event_map, &event,
                                   &options) == LONEJSON_STATUS_OK);
  EXPECT(write_alloc.stats.alloc_calls > 0u);
  EXPECT(write_alloc.stats.bytes_live > 0u);
  total = lonejson_curl_read_callback(buffer, 1u, sizeof(buffer) - 1u, &upload);
  EXPECT(total != 0u);
  EXPECT(total != CURL_READFUNC_ABORT);
  buffer[total] = '\0';
  EXPECT(strstr(buffer, "\"id\":\"evt-1\"") != NULL);
  lonejson_curl_upload_cleanup(&upload);
  EXPECT(write_alloc.stats.bytes_live == 0u);
  EXPECT(write_alloc.stats.free_calls > 0u);
}

static void test_curl_upload_streaming_does_not_buffer_payload(void) {
  test_source_doc doc;
  test_allocator_state write_alloc;
  lonejson_write_options options = lonejson_default_write_options();
  lonejson_curl_upload upload;
  char path[] = "/tmp/lonejson-curl-upload-XXXXXX";
  char chunk[4096];
  int fd;
  FILE *fp;
  size_t total;
  size_t i;

  memset(&doc, 0, sizeof(doc));
  strcpy(doc.id, "evt-1");
  lonejson_source_init(&doc.text);
  lonejson_source_init(&doc.bytes);
  test_allocator_init(&write_alloc);
  options.allocator = &write_alloc.allocator;

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
  EXPECT(lonejson_curl_upload_init(&upload, &test_source_doc_map, &doc,
                                   &options) == LONEJSON_STATUS_OK);
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
  EXPECT(write_alloc.stats.bytes_live == 0u);
  lonejson_cleanup(&test_source_doc_map, &doc);
  unlink(path);
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
  EXPECT(lonejson_curl_array_parse_init(&parse, "items", &test_item_map, &item,
                                        NULL, test_curl_array_item,
                                        &seen) == LONEJSON_STATUS_OK);
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
  EXPECT(lonejson_curl_array_parse_init(&parse, "", &test_item_map, &item, NULL,
                                        test_curl_array_item,
                                        &seen) == LONEJSON_STATUS_OK);
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
  EXPECT(lonejson_curl_array_parse_init(&parse, "", NULL, &item, NULL,
                                        test_curl_array_item, &seen) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(parse.stream == NULL);
  EXPECT(parse.error.code == LONEJSON_STATUS_INVALID_ARGUMENT);
  lonejson_curl_array_parse_cleanup(&parse);

  memset(&seen, 0, sizeof(seen));
  EXPECT(lonejson_curl_array_parse_init(&parse, "", &test_item_map, &item, NULL,
                                        test_curl_array_item,
                                        &seen) == LONEJSON_STATUS_OK);
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
  EXPECT(lonejson_curl_array_parse_init(&parse, "items", &test_item_map, &item,
                                        NULL, test_curl_array_item,
                                        &seen) == LONEJSON_STATUS_OK);
  got = lonejson_curl_array_write_callback("{\"meta\":true}", 1u,
                                           strlen("{\"meta\":true}"), &parse);
  EXPECT(got == 0u);
  EXPECT(parse.error.code == LONEJSON_STATUS_MISSING_REQUIRED_FIELD);
  EXPECT(seen.count == 0u);
  lonejson_curl_array_parse_cleanup(&parse);

  memset(&seen, 0, sizeof(seen));
  EXPECT(lonejson_curl_array_parse_init(&parse, "items", &test_item_map, &item,
                                        NULL, test_curl_array_item,
                                        &seen) == LONEJSON_STATUS_OK);
  got = lonejson_curl_array_write_callback(
      "{\"items\":[{\"id\":41}],\"tail\":{bad}}", 1u,
      strlen("{\"items\":[{\"id\":41}],\"tail\":{bad}}"), &parse);
  EXPECT(got == 0u);
  EXPECT(parse.error.code == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(seen.count == 1u);
  EXPECT(seen.ids[0] == 41);
  lonejson_curl_array_parse_cleanup(&parse);
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
  EXPECT(lonejson_curl_string_array_parse_init(&parse, "keys", NULL, &handler,
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
  EXPECT(lonejson_curl_string_array_parse_init(&parse, "keys", NULL, &handler,
                                               &seen) == LONEJSON_STATUS_OK);
  got = lonejson_curl_string_array_write_callback(
      "{\"keys\":[1]}", 1u, strlen("{\"keys\":[1]}"), &parse);
  EXPECT(got == 0u);
  EXPECT(parse.error.code == LONEJSON_STATUS_TYPE_MISMATCH);
  lonejson_curl_string_array_parse_cleanup(&parse);
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
  EXPECT(lonejson_curl_string_items_parse_init(&parse, "keys", NULL,
                                               test_curl_string_item,
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
  EXPECT(lonejson_curl_string_items_parse_init(&parse, "keys", NULL,
                                               test_curl_string_item,
                                               &seen) == LONEJSON_STATUS_OK);
  got = lonejson_curl_string_items_write_callback(
      "{\"keys\":[1]}", 1u, strlen("{\"keys\":[1]}"), &parse);
  EXPECT(got == 0u);
  EXPECT(parse.error.code == LONEJSON_STATUS_TYPE_MISMATCH);
  lonejson_curl_string_items_parse_cleanup(&parse);
}
#endif

static int test_child_parse_poisoned_stream_and_json_value_fields(void) {
  test_alloc_parse_doc stream_doc;
  test_alloc_json_value_doc json_doc;

  memset(&stream_doc, 0xA5, sizeof(stream_doc));
  stream_doc.body._lonejson_magic = LONEJSON__SPOOLED_MAGIC;
  stream_doc.body.memory = (unsigned char *)1;
  stream_doc.body.spill_fp = (FILE *)1;
  lonejson__init_map(&test_alloc_parse_doc_map, &stream_doc);
  lonejson_cleanup(&test_alloc_parse_doc_map, &stream_doc);

  memset(&json_doc, 0xA5, sizeof(json_doc));
  json_doc.selector._lonejson_magic = LONEJSON__JSON_VALUE_MAGIC;
  json_doc.selector.json = (char *)1;
  json_doc.selector.path = (char *)1;
  lonejson__init_map(&test_alloc_json_value_doc_map, &json_doc);
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
  lonejson_parse_options options;
  lonejson_error error;
  lonejson_status status;

  memset(&run, 0, sizeof(run));
  run.results.items = run.result_storage;
  run.results.capacity =
      sizeof(run.result_storage) / sizeof(run.result_storage[0]);
  run.results.elem_size = sizeof(run.result_storage[0]);
  run.results.flags = LONEJSON_ARRAY_FIXED_CAPACITY;

  options = lonejson_default_parse_options();
  options.clear_destination = 0;
  status = lonejson_parse_cstr(&test_fixed_result_run_map, &run, json, &options,
                               &error);
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
