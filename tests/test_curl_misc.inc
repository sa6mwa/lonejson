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
