static void test_value_rewrite_replace_drop_and_root(void) {
  unsigned char out[512];
  test_buffer_sink sink;
  test_reader_state reader;
  lonejson_json_value replacement;
  lonejson_value_rewrite_options options;
  lonejson_error error;
  const char *path_user_name[] = {"user", "name"};
  const char *path_items_one[] = {"items", "1"};
  lonejson_status status;

  sink.length = 0u;
  out[0] = '\0';
  sink.buffer = out;
  sink.capacity = sizeof(out);
  lonejson_json_value_init(test_default_runtime(), &replacement);
  EXPECT(lonejson_json_value_set_buffer(&replacement, "\"Ada\"", 5u, &error) ==
         LONEJSON_STATUS_OK);
  memset(&options, 0, sizeof(options));
  options.target_segments = path_user_name;
  options.target_segment_count = 2u;
  options.action = LONEJSON_VALUE_REWRITE_REPLACE;
  options.replacement.json = &replacement;
  reader.json = "{\"user\":{\"name\":\"old\",\"id\":7},\"items\":[1,2,3]}";
  reader.offset = 0u;
  reader.chunk_size = 3u;
  status = test_value_rewrite_reader(
      test_state_reader, &reader, test_buffer_sink_write, &sink, &options,
      &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)out,
                "{\"user\":{\"name\":\"Ada\",\"id\":7},\"items\":[1,2,3]}") ==
         0);

  sink.length = 0u;
  out[0] = '\0';
  options.target_segments = path_items_one;
  options.target_segment_count = 2u;
  options.action = LONEJSON_VALUE_REWRITE_DROP;
  memset(&options.replacement, 0, sizeof(options.replacement));
  reader.json = "{\"items\":[1,2,3],\"tail\":true}";
  reader.offset = 0u;
  reader.chunk_size = 2u;
  status = test_value_rewrite_reader(
      test_state_reader, &reader, test_buffer_sink_write, &sink, &options,
      &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)out, "{\"items\":[1,3],\"tail\":true}") == 0);

  sink.length = 0u;
  out[0] = '\0';
  EXPECT(lonejson_json_value_set_buffer(&replacement, "{\"ok\":true}", 11u,
                                        &error) == LONEJSON_STATUS_OK);
  memset(&options, 0, sizeof(options));
  options.action = LONEJSON_VALUE_REWRITE_REPLACE;
  options.replacement.json = &replacement;
  reader.json = "[1,{\"ignored\":\"value\"}]";
  reader.offset = 0u;
  reader.chunk_size = 4u;
  status = test_value_rewrite_reader(
      test_state_reader, &reader, test_buffer_sink_write, &sink, &options,
      &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)out, "{\"ok\":true}") == 0);
  lonejson_json_value_cleanup(&replacement);
}

static void test_value_rewrite_runtime_json_value_limits(void) {
  static const char json[] = "{\"payload\":\"ab\",\"tail\":1}";
  lonejson_config config;
  lonejson *limited_lj;
  lonejson *default_lj;
  unsigned char out[128];
  test_buffer_sink sink;
  test_reader_state reader;
  lonejson_value_rewrite_options options;
  lonejson_error error;
  const char *path_payload[] = {"payload"};
  lonejson_status status;

  config = lonejson_default_config();
  config.json_value_max_string_bytes = 1u;
  limited_lj = lonejson_new(&config, &error);
  EXPECT(limited_lj != NULL);
  default_lj = lonejson_new(NULL, &error);
  EXPECT(default_lj != NULL);
  if (limited_lj == NULL || default_lj == NULL) {
    lonejson_free(limited_lj);
    lonejson_free(default_lj);
    return;
  }

  memset(&sink, 0, sizeof(sink));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  memset(&options, 0, sizeof(options));
  options.target_segments = path_payload;
  options.target_segment_count = 1u;
  options.action = LONEJSON_VALUE_REWRITE_DROP;
  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 2u;

  status = lonejson_value_rewrite_reader(
      limited_lj, test_state_reader, &reader, test_buffer_sink_write, &sink,
      &options, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);

  reader.offset = 0u;
  sink.length = 0u;
  out[0] = '\0';
  status = lonejson_value_rewrite_reader(
      default_lj, test_state_reader, &reader, test_buffer_sink_write, &sink,
      &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)out, "{\"tail\":1}") == 0);

  lonejson_free(limited_lj);
  lonejson_free(default_lj);
}

typedef struct test_value_rewrite_increment {
  lonejson_int64 delta;
} test_value_rewrite_increment;

static lonejson_status
test_value_rewrite_replace_with_increment(lonejson_writer *writer,
                                          const lonejson_value_rewrite_old_value
                                              *old_value,
                                          void *user, lonejson_error *error);
static lonejson_status
test_value_rewrite_replace_with_object(lonejson_writer *writer,
                                       const lonejson_value_rewrite_old_value
                                           *old_value,
                                       void *user, lonejson_error *error);
typedef struct test_value_rewrite_alloc_budget {
  size_t remaining_allocs;
} test_value_rewrite_alloc_budget;

static void *test_value_rewrite_fail_malloc(void *ctx, size_t size);
static void *test_value_rewrite_fail_realloc(void *ctx, void *ptr, size_t size);
static void test_value_rewrite_fail_free(void *ctx, void *ptr);

static void test_value_rewrite_callbacks_use_runtime_writer_policy(void) {
  static const char json[] = "{\"count\":1}";
  test_value_rewrite_alloc_budget budget;
  lonejson_allocator failing_allocator;
  lonejson_config config;
  lonejson *failing_lj;
  lonejson *default_lj;
  unsigned char out[128];
  test_buffer_sink sink;
  lonejson_value_rewrite_options options;
  lonejson_error error;
  const char *path_count[] = {"count"};
  lonejson_status status;

  memset(&options, 0, sizeof(options));
  options.target_segments = path_count;
  options.target_segment_count = 1u;
  options.action = LONEJSON_VALUE_REWRITE_REPLACE_WITH;
  options.replace = test_value_rewrite_replace_with_object;

  sink.buffer = out;
  sink.capacity = sizeof(out);
  sink.length = 0u;

  budget.remaining_allocs = 2u;
  memset(&failing_allocator, 0, sizeof(failing_allocator));
  failing_allocator.malloc_fn = test_value_rewrite_fail_malloc;
  failing_allocator.realloc_fn = test_value_rewrite_fail_realloc;
  failing_allocator.free_fn = test_value_rewrite_fail_free;
  failing_allocator.ctx = &budget;
  config = lonejson_default_config();
  config.allocator = &failing_allocator;
  failing_lj = lonejson_new(&config, &error);
  EXPECT(failing_lj != NULL);
  default_lj = lonejson_new(NULL, &error);
  EXPECT(default_lj != NULL);
  if (failing_lj == NULL || default_lj == NULL) {
    lonejson_free(failing_lj);
    lonejson_free(default_lj);
    return;
  }

  out[0] = '\0';
  status = lonejson_value_rewrite_buffer(failing_lj, json, strlen(json),
                                         test_buffer_sink_write, &sink,
                                         &options, &error);
  EXPECT(status == LONEJSON_STATUS_ALLOCATION_FAILED);

  sink.length = 0u;
  out[0] = '\0';
  status = lonejson_value_rewrite_buffer(default_lj, json, strlen(json),
                                         test_buffer_sink_write, &sink,
                                         &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)out, "{\"count\":{\"msg\":\"ok\"}}") == 0);

  lonejson_free(failing_lj);
  lonejson_free(default_lj);
}

static void test_value_rewrite_path_rejects_invalid_runtime_without_truncating(void) {
  char input_path[] = "/tmp/lonejson-value-rewrite-in-XXXXXX";
  char output_path[] = "/tmp/lonejson-value-rewrite-out-XXXXXX";
  const char *input_json = "{\"keep\":1}";
  const char *sentinel = "keep me";
  lonejson_value_rewrite_options options;
  lonejson_error error;
  FILE *fp;
  char buffer[64];
  size_t n;
  int fd;

  fd = write_temp_text_file(input_path, input_json);
  EXPECT(fd >= 0);
  if (fd < 0) {
    return;
  }
  close(fd);

  fd = write_temp_text_file(output_path, sentinel);
  EXPECT(fd >= 0);
  if (fd < 0) {
    unlink(input_path);
    return;
  }
  close(fd);

  memset(&options, 0, sizeof(options));
  EXPECT(lonejson_value_rewrite_path(NULL, input_path, output_path, &options,
                                     &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);

  fp = fopen(output_path, "rb");
  EXPECT(fp != NULL);
  if (fp != NULL) {
    n = fread(buffer, 1u, sizeof(buffer) - 1u, fp);
    buffer[n] = '\0';
    fclose(fp);
    EXPECT(strcmp(buffer, sentinel) == 0);
  }

  unlink(input_path);
  unlink(output_path);
}

static lonejson_status
test_value_rewrite_emit_dynamic(lonejson_writer *writer, void *user,
                                lonejson_error *error) {
  const char *label = (const char *)user;
  lonejson_status status;

  status = lonejson_writer_begin_object(writer, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson_writer_key(writer, "label", 5u, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson_writer_string(writer, label, strlen(label), error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson_writer_key(writer, "values", 6u, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson_writer_begin_array(writer, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson_writer_i64(writer, 1, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson_writer_bool(writer, 1, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson_writer_end_array(writer, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson_writer_end_object(writer, error);
}

static lonejson_status
test_value_rewrite_emit_source_base64(lonejson_writer *writer, void *user,
                                      lonejson_error *error) {
  return lonejson_writer_source_base64(writer, (const lonejson_source *)user,
                                       error);
}

static void test_value_rewrite_callback_replacement(void) {
  unsigned char out[512];
  test_buffer_sink sink;
  test_reader_state reader;
  lonejson_value_rewrite_options options;
  lonejson_error error;
  const char *path_payload[] = {"payload"};
  lonejson_status status;

  sink.length = 0u;
  out[0] = '\0';
  sink.buffer = out;
  sink.capacity = sizeof(out);
  memset(&options, 0, sizeof(options));
  options.target_segments = path_payload;
  options.target_segment_count = 1u;
  options.action = LONEJSON_VALUE_REWRITE_REPLACE;
  options.replacement.emit = test_value_rewrite_emit_dynamic;
  options.replacement.emit_user = (void *)"generated";
  reader.json = "{\"payload\":null,\"tail\":false}";
  reader.offset = 0u;
  reader.chunk_size = 3u;
  status = test_value_rewrite_reader(
      test_state_reader, &reader, test_buffer_sink_write, &sink, &options,
      &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)out,
                "{\"payload\":{\"label\":\"generated\",\"values\":[1,true]},"
                "\"tail\":false}") == 0);
}

static lonejson_status
test_value_rewrite_replace_with_increment(lonejson_writer *writer,
                                          const lonejson_value_rewrite_old_value
                                              *old_value,
                                          void *user, lonejson_error *error) {
  test_value_rewrite_increment *ctx = (test_value_rewrite_increment *)user;
  char token[64];
  char *end = NULL;
  long old_number;

  if (!old_value->present) {
    return lonejson_writer_i64(writer, ctx->delta, error);
  }
  if (old_value->type != LONEJSON_VALUE_NUMBER) {
    if (error != NULL) {
      lonejson_error_init(error);
      error->code = LONEJSON_STATUS_TYPE_MISMATCH;
      strcpy(error->message, "increment target is not a number");
    }
    return LONEJSON_STATUS_TYPE_MISMATCH;
  }
  EXPECT(old_value->number_len < sizeof(token));
  if (old_value->number_len >= sizeof(token)) {
    return LONEJSON_STATUS_OVERFLOW;
  }
  memcpy(token, old_value->number, old_value->number_len);
  token[old_value->number_len] = '\0';
  old_number = strtol(token, &end, 10);
  EXPECT(end != token && *end == '\0');
  return lonejson_writer_i64(writer, (lonejson_int64)old_number + ctx->delta,
                             error);
}

typedef struct test_value_rewrite_visitor_counts {
  int objects;
  int keys;
} test_value_rewrite_visitor_counts;

static lonejson_status test_value_rewrite_count_object(void *user,
                                                       lonejson_error *error) {
  test_value_rewrite_visitor_counts *counts =
      (test_value_rewrite_visitor_counts *)user;
  (void)error;
  counts->objects++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_value_rewrite_count_key(void *user,
                                                    lonejson_error *error) {
  test_value_rewrite_visitor_counts *counts =
      (test_value_rewrite_visitor_counts *)user;
  (void)error;
  counts->keys++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
test_value_rewrite_replace_with_null(lonejson_writer *writer,
                                     const lonejson_value_rewrite_old_value
                                         *old_value,
                                     void *user, lonejson_error *error) {
  (void)user;
  EXPECT(old_value->present != 0);
  EXPECT(old_value->type == LONEJSON_VALUE_OBJECT);
  return lonejson_writer_null(writer, error);
}

static lonejson_status
test_value_rewrite_replace_with_object(lonejson_writer *writer,
                                       const lonejson_value_rewrite_old_value
                                           *old_value,
                                       void *user, lonejson_error *error) {
  lonejson_status status;
  (void)old_value;
  (void)user;

  status = lonejson_writer_begin_object(writer, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson_writer_key(writer, "msg", 3u, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson_writer_string(writer, "ok", 2u, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson_writer_end_object(writer, error);
}

static void *test_value_rewrite_fail_malloc(void *ctx, size_t size) {
  test_value_rewrite_alloc_budget *budget =
      (test_value_rewrite_alloc_budget *)ctx;

  if (budget == NULL || budget->remaining_allocs == 0u) {
    return NULL;
  }
  budget->remaining_allocs--;
  return LONEJSON_MALLOC(size);
}

static void *test_value_rewrite_fail_realloc(void *ctx, void *ptr, size_t size) {
  test_value_rewrite_alloc_budget *budget =
      (test_value_rewrite_alloc_budget *)ctx;

  if (ptr == NULL) {
    return test_value_rewrite_fail_malloc(ctx, size);
  }
  if (budget == NULL || budget->remaining_allocs == 0u) {
    return NULL;
  }
  budget->remaining_allocs--;
  return LONEJSON_REALLOC(ptr, size);
}

static void test_value_rewrite_fail_free(void *ctx, void *ptr) {
  (void)ctx;
  LONEJSON_FREE(ptr);
}

static void test_value_rewrite_replace_with_and_buffer_adapters(void) {
  lonejson_value_rewrite_options options;
  lonejson_value_rewrite_selector_options selector_options;
  lonejson_value_visitor visitor;
  lonejson_owned_buffer out;
  lonejson_error error;
  test_value_rewrite_increment inc;
  test_value_rewrite_visitor_counts counts;
  const char *path_count[] = {"count"};
  const char *path_meta_count[] = {"meta", "count"};
  const char *path_payload[] = {"payload"};
  lonejson_status status;

  inc.delta = 5;
  lonejson_owned_buffer_init(&out);
  memset(&options, 0, sizeof(options));
  options.target_segments = path_count;
  options.target_segment_count = 1u;
  options.action = LONEJSON_VALUE_REWRITE_REPLACE_WITH;
  options.replace = test_value_rewrite_replace_with_increment;
  options.replace_user = &inc;
  status = test_value_rewrite_buffer(
      "{\"count\":7,\"tail\":true}", strlen("{\"count\":7,\"tail\":true}"),
      lonejson_owned_buffer_sink, &out, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(out.data != NULL && strcmp(out.data, "{\"count\":12,\"tail\":true}") == 0);
  lonejson_owned_buffer_free(&out);

  lonejson_owned_buffer_init(&out);
  memset(&options, 0, sizeof(options));
  options.target_segments = path_meta_count;
  options.target_segment_count = 2u;
  options.action = LONEJSON_VALUE_REWRITE_REPLACE_WITH;
  options.replace = test_value_rewrite_replace_with_increment;
  options.replace_user = &inc;
  status = test_value_rewrite_buffer(
      "{\"meta\":{\"ok\":true}}", strlen("{\"meta\":{\"ok\":true}}"),
      lonejson_owned_buffer_sink, &out, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(out.data != NULL &&
         strcmp(out.data, "{\"meta\":{\"ok\":true,\"count\":5}}") == 0);
  lonejson_owned_buffer_free(&out);

  lonejson_owned_buffer_init(&out);
  memset(&options, 0, sizeof(options));
  options.target_segments = path_count;
  options.target_segment_count = 1u;
  options.action = LONEJSON_VALUE_REWRITE_REPLACE_WITH;
  options.replace = test_value_rewrite_replace_with_increment;
  options.replace_user = &inc;
  status = test_value_rewrite_buffer(
      "{\"count\":\"nope\"}", strlen("{\"count\":\"nope\"}"),
      lonejson_owned_buffer_sink, &out, &options, &error);
  EXPECT(status == LONEJSON_STATUS_TYPE_MISMATCH);
  lonejson_owned_buffer_free(&out);

  lonejson_owned_buffer_init(&out);
  counts.objects = 0;
  counts.keys = 0;
  visitor = lonejson_default_value_visitor();
  visitor.object_begin = test_value_rewrite_count_object;
  visitor.object_key_end = test_value_rewrite_count_key;
  memset(&options, 0, sizeof(options));
  options.target_segments = path_payload;
  options.target_segment_count = 1u;
  options.action = LONEJSON_VALUE_REWRITE_REPLACE_WITH;
  options.old_value_visitor = &visitor;
  options.old_value_user = &counts;
  options.replace = test_value_rewrite_replace_with_null;
  status = test_value_rewrite_buffer(
      "{\"payload\":{\"a\":1,\"b\":[2]},\"tail\":0}",
      strlen("{\"payload\":{\"a\":1,\"b\":[2]},\"tail\":0}"),
      lonejson_owned_buffer_sink, &out, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(out.data != NULL &&
         strcmp(out.data, "{\"payload\":null,\"tail\":0}") == 0);
  EXPECT(counts.objects == 1);
  EXPECT(counts.keys == 2);
  lonejson_owned_buffer_free(&out);

  lonejson_owned_buffer_init(&out);
  memset(&selector_options, 0, sizeof(selector_options));
  selector_options.selector = "count";
  selector_options.action = LONEJSON_VALUE_REWRITE_REPLACE_WITH;
  selector_options.replace = test_value_rewrite_replace_with_increment;
  selector_options.replace_user = &inc;
  status = test_value_rewrite_selector_buffer(
      "{\"count\":1}", strlen("{\"count\":1}"), lonejson_owned_buffer_sink,
      &out, &selector_options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(out.data != NULL && strcmp(out.data, "{\"count\":6}") == 0);
  lonejson_owned_buffer_free(&out);
}

typedef struct test_value_rewrite_old_value_expect {
  int present;
  lonejson_value_type type;
  int boolean;
  size_t min_number_len;
  int calls;
} test_value_rewrite_old_value_expect;

static lonejson_status
test_value_rewrite_expect_old_value(lonejson_writer *writer,
                                    const lonejson_value_rewrite_old_value *old,
                                    void *user, lonejson_error *error) {
  test_value_rewrite_old_value_expect *expect =
      (test_value_rewrite_old_value_expect *)user;

  expect->calls++;
  EXPECT(old->present == expect->present);
  EXPECT(old->type == expect->type);
  if (old->type == LONEJSON_VALUE_NUMBER) {
    EXPECT(old->number != NULL);
    EXPECT(old->number_len >= expect->min_number_len);
  } else {
    EXPECT(old->number == NULL);
    EXPECT(old->number_len == 0u);
  }
  if (old->type == LONEJSON_VALUE_BOOL) {
    EXPECT(old->boolean == expect->boolean);
  }
  return lonejson_writer_null(writer, error);
}

static void test_value_rewrite_replace_with_old_value_metadata(void) {
  static const struct {
    const char *input;
    int present;
    lonejson_value_type type;
    int boolean;
    size_t min_number_len;
  } cases[] = {
      {"{\"v\":\"x\"}", 1, LONEJSON_VALUE_STRING, 0, 0u},
      {"{\"v\":-12.5}", 1, LONEJSON_VALUE_NUMBER, 0, 5u},
      {"{\"v\":true}", 1, LONEJSON_VALUE_BOOL, 1, 0u},
      {"{\"v\":false}", 1, LONEJSON_VALUE_BOOL, 0, 0u},
      {"{\"v\":null}", 1, LONEJSON_VALUE_NULL, 0, 0u},
      {"{\"v\":[1]}", 1, LONEJSON_VALUE_ARRAY, 0, 0u},
      {"{\"v\":{\"a\":1}}", 1, LONEJSON_VALUE_OBJECT, 0, 0u},
      {"{}", 0, LONEJSON_VALUE_ABSENT, 0, 0u}};
  lonejson_value_rewrite_options options;
  lonejson_owned_buffer out;
  lonejson_error error;
  test_value_rewrite_old_value_expect expect;
  const char *path_v[] = {"v"};
  size_t i;
  lonejson_status status;

  for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    lonejson_owned_buffer_init(&out);
    memset(&expect, 0, sizeof(expect));
    expect.present = cases[i].present;
    expect.type = cases[i].type;
    expect.boolean = cases[i].boolean;
    expect.min_number_len = cases[i].min_number_len;
    memset(&options, 0, sizeof(options));
    options.target_segments = path_v;
    options.target_segment_count = 1u;
    options.action = LONEJSON_VALUE_REWRITE_REPLACE_WITH;
    options.replace = test_value_rewrite_expect_old_value;
    options.replace_user = &expect;
    status = test_value_rewrite_buffer(
        cases[i].input, strlen(cases[i].input), lonejson_owned_buffer_sink,
        &out, &options, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(expect.calls == 1);
    EXPECT(out.data != NULL);
    EXPECT(test_validate_buffer(out.data, out.len, &error) ==
           LONEJSON_STATUS_OK);
    lonejson_owned_buffer_free(&out);
  }
}

static lonejson_status test_value_rewrite_no_value(
    lonejson_writer *writer, const lonejson_value_rewrite_old_value *old_value,
    void *user, lonejson_error *error) {
  (void)writer;
  (void)old_value;
  (void)user;
  (void)error;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_value_rewrite_two_values(
    lonejson_writer *writer, const lonejson_value_rewrite_old_value *old_value,
    void *user, lonejson_error *error) {
  lonejson_status status;
  (void)old_value;
  (void)user;
  status = lonejson_writer_null(writer, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson_writer_null(writer, error);
}

static lonejson_status test_value_rewrite_partial_error(
    lonejson_writer *writer, const lonejson_value_rewrite_old_value *old_value,
    void *user, lonejson_error *error) {
  lonejson_status status;
  (void)old_value;
  (void)user;
  status = lonejson_writer_begin_object(writer, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u,
                             0u, "intentional replacement failure");
}

static lonejson_status test_value_rewrite_unclosed_container(
    lonejson_writer *writer, const lonejson_value_rewrite_old_value *old_value,
    void *user, lonejson_error *error) {
  (void)old_value;
  (void)user;
  return lonejson_writer_begin_array(writer, error);
}

static void test_value_rewrite_replace_with_callback_failures(void) {
  lonejson_value_rewrite_options options;
  lonejson_owned_buffer out;
  lonejson_error error;
  const char *path_v[] = {"v"};
  lonejson_status status;

  memset(&options, 0, sizeof(options));
  options.target_segments = path_v;
  options.target_segment_count = 1u;
  options.action = LONEJSON_VALUE_REWRITE_REPLACE_WITH;
  EXPECT(test_value_rewrite_buffer("{\"v\":1}", strlen("{\"v\":1}"),
                                       lonejson_owned_buffer_sink, &out,
                                       &options, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);

  lonejson_owned_buffer_init(&out);
  options.replace = test_value_rewrite_no_value;
  status = test_value_rewrite_buffer("{\"v\":1}", strlen("{\"v\":1}"),
                                         lonejson_owned_buffer_sink, &out,
                                         &options, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
  lonejson_owned_buffer_free(&out);

  lonejson_owned_buffer_init(&out);
  options.replace = test_value_rewrite_two_values;
  status = test_value_rewrite_buffer("{\"v\":1}", strlen("{\"v\":1}"),
                                         lonejson_owned_buffer_sink, &out,
                                         &options, &error);
  EXPECT(status != LONEJSON_STATUS_OK);
  lonejson_owned_buffer_free(&out);

  lonejson_owned_buffer_init(&out);
  options.replace = test_value_rewrite_partial_error;
  status = test_value_rewrite_buffer("{\"v\":1}", strlen("{\"v\":1}"),
                                         lonejson_owned_buffer_sink, &out,
                                         &options, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  lonejson_owned_buffer_free(&out);

  lonejson_owned_buffer_init(&out);
  options.replace = test_value_rewrite_unclosed_container;
  status = test_value_rewrite_buffer("{\"v\":1}", strlen("{\"v\":1}"),
                                         lonejson_owned_buffer_sink, &out,
                                         &options, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
  lonejson_owned_buffer_free(&out);
}

typedef struct test_value_rewrite_visit_counts {
  int object_begin;
  int object_end;
  int array_begin;
  int array_end;
  int key_begin;
  int key_chunk;
  int key_end;
  int string_begin;
  int string_chunk;
  int string_end;
  int number_begin;
  int number_chunk;
  int number_end;
  int boolean_value;
  int null_value;
} test_value_rewrite_visit_counts;

static lonejson_status test_value_rewrite_visit_object_begin(
    void *user, lonejson_error *error) {
  ((test_value_rewrite_visit_counts *)user)->object_begin++;
  (void)error;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_value_rewrite_visit_object_end(
    void *user, lonejson_error *error) {
  ((test_value_rewrite_visit_counts *)user)->object_end++;
  (void)error;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_value_rewrite_visit_array_begin(
    void *user, lonejson_error *error) {
  ((test_value_rewrite_visit_counts *)user)->array_begin++;
  (void)error;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_value_rewrite_visit_array_end(
    void *user, lonejson_error *error) {
  ((test_value_rewrite_visit_counts *)user)->array_end++;
  (void)error;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_value_rewrite_visit_key_begin(
    void *user, lonejson_error *error) {
  ((test_value_rewrite_visit_counts *)user)->key_begin++;
  (void)error;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_value_rewrite_visit_key_chunk(
    void *user, const char *data, size_t len, lonejson_error *error) {
  ((test_value_rewrite_visit_counts *)user)->key_chunk++;
  (void)data;
  (void)len;
  (void)error;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_value_rewrite_visit_key_end(
    void *user, lonejson_error *error) {
  ((test_value_rewrite_visit_counts *)user)->key_end++;
  (void)error;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_value_rewrite_visit_string_begin(
    void *user, lonejson_error *error) {
  ((test_value_rewrite_visit_counts *)user)->string_begin++;
  (void)error;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_value_rewrite_visit_string_chunk(
    void *user, const char *data, size_t len, lonejson_error *error) {
  ((test_value_rewrite_visit_counts *)user)->string_chunk++;
  (void)data;
  (void)len;
  (void)error;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_value_rewrite_visit_string_end(
    void *user, lonejson_error *error) {
  ((test_value_rewrite_visit_counts *)user)->string_end++;
  (void)error;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_value_rewrite_visit_number_begin(
    void *user, lonejson_error *error) {
  ((test_value_rewrite_visit_counts *)user)->number_begin++;
  (void)error;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_value_rewrite_visit_number_chunk(
    void *user, const char *data, size_t len, lonejson_error *error) {
  ((test_value_rewrite_visit_counts *)user)->number_chunk++;
  (void)data;
  (void)len;
  (void)error;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_value_rewrite_visit_number_end(
    void *user, lonejson_error *error) {
  ((test_value_rewrite_visit_counts *)user)->number_end++;
  (void)error;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_value_rewrite_visit_bool(
    void *user, int value, lonejson_error *error) {
  ((test_value_rewrite_visit_counts *)user)->boolean_value += value ? 1 : 2;
  (void)error;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_value_rewrite_visit_null(
    void *user, lonejson_error *error) {
  ((test_value_rewrite_visit_counts *)user)->null_value++;
  (void)error;
  return LONEJSON_STATUS_OK;
}

static void test_value_rewrite_old_value_visitor_balanced_events(void) {
  lonejson_value_rewrite_options options;
  lonejson_value_visitor visitor;
  lonejson_owned_buffer out;
  lonejson_error error;
  test_value_rewrite_old_value_expect expect;
  test_value_rewrite_visit_counts counts;
  const char *path_v[] = {"v"};
  lonejson_status status;

  memset(&expect, 0, sizeof(expect));
  expect.present = 1;
  expect.type = LONEJSON_VALUE_OBJECT;
  memset(&counts, 0, sizeof(counts));
  visitor = lonejson_default_value_visitor();
  visitor.object_begin = test_value_rewrite_visit_object_begin;
  visitor.object_end = test_value_rewrite_visit_object_end;
  visitor.array_begin = test_value_rewrite_visit_array_begin;
  visitor.array_end = test_value_rewrite_visit_array_end;
  visitor.object_key_begin = test_value_rewrite_visit_key_begin;
  visitor.object_key_chunk = test_value_rewrite_visit_key_chunk;
  visitor.object_key_end = test_value_rewrite_visit_key_end;
  visitor.string_begin = test_value_rewrite_visit_string_begin;
  visitor.string_chunk = test_value_rewrite_visit_string_chunk;
  visitor.string_end = test_value_rewrite_visit_string_end;
  visitor.number_begin = test_value_rewrite_visit_number_begin;
  visitor.number_chunk = test_value_rewrite_visit_number_chunk;
  visitor.number_end = test_value_rewrite_visit_number_end;
  visitor.boolean_value = test_value_rewrite_visit_bool;
  visitor.null_value = test_value_rewrite_visit_null;

  memset(&options, 0, sizeof(options));
  options.target_segments = path_v;
  options.target_segment_count = 1u;
  options.action = LONEJSON_VALUE_REWRITE_REPLACE_WITH;
  options.replace = test_value_rewrite_expect_old_value;
  options.replace_user = &expect;
  options.old_value_visitor = &visitor;
  options.old_value_user = &counts;

  lonejson_owned_buffer_init(&out);
  status = test_value_rewrite_buffer(
      "{\"v\":{\"s\":\"x\",\"n\":12,\"a\":[true,null]}}",
      strlen("{\"v\":{\"s\":\"x\",\"n\":12,\"a\":[true,null]}}"),
      lonejson_owned_buffer_sink, &out, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(counts.object_begin == counts.object_end);
  EXPECT(counts.object_begin == 1);
  EXPECT(counts.array_begin == counts.array_end);
  EXPECT(counts.array_begin == 1);
  EXPECT(counts.key_begin == counts.key_end);
  EXPECT(counts.key_begin == 3);
  EXPECT(counts.key_chunk >= 3);
  EXPECT(counts.string_begin == counts.string_end);
  EXPECT(counts.string_begin == 1);
  EXPECT(counts.string_chunk >= 1);
  EXPECT(counts.number_begin == counts.number_end);
  EXPECT(counts.number_begin == 1);
  EXPECT(counts.number_chunk >= 1);
  EXPECT(counts.boolean_value == 1);
  EXPECT(counts.null_value == 1);
  lonejson_owned_buffer_free(&out);
}

static lonejson_status test_value_rewrite_fail_event(void *user,
                                                     lonejson_error *error) {
  (void)user;
  return lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u, 0u,
                             "intentional visitor event failure");
}

static lonejson_status test_value_rewrite_fail_chunk(void *user,
                                                     const char *data,
                                                     size_t len,
                                                     lonejson_error *error) {
  (void)user;
  (void)data;
  (void)len;
  return lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u, 0u,
                             "intentional visitor chunk failure");
}

static lonejson_status test_value_rewrite_fail_bool(void *user, int value,
                                                    lonejson_error *error) {
  (void)user;
  (void)value;
  return lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u, 0u,
                             "intentional visitor bool failure");
}

static void test_value_rewrite_old_value_visitor_failures(void) {
  lonejson_value_rewrite_options options;
  lonejson_value_visitor visitor;
  lonejson_owned_buffer out;
  lonejson_error error;
  test_value_rewrite_old_value_expect expect;
  const char *path_v[] = {"v"};
  lonejson_status status;

  memset(&expect, 0, sizeof(expect));
  expect.present = 1;
  expect.type = LONEJSON_VALUE_OBJECT;
  memset(&options, 0, sizeof(options));
  options.target_segments = path_v;
  options.target_segment_count = 1u;
  options.action = LONEJSON_VALUE_REWRITE_REPLACE_WITH;
  options.replace = test_value_rewrite_expect_old_value;
  options.replace_user = &expect;
  visitor = lonejson_default_value_visitor();
  visitor.object_begin = test_value_rewrite_fail_event;
  options.old_value_visitor = &visitor;
  lonejson_owned_buffer_init(&out);
  status = test_value_rewrite_buffer("{\"v\":{\"a\":1}}",
                                         strlen("{\"v\":{\"a\":1}}"),
                                         lonejson_owned_buffer_sink, &out,
                                         &options, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  lonejson_owned_buffer_free(&out);

  visitor = lonejson_default_value_visitor();
  visitor.string_chunk = test_value_rewrite_fail_chunk;
  options.old_value_visitor = &visitor;
  lonejson_owned_buffer_init(&out);
  status = test_value_rewrite_buffer("{\"v\":\"abc\"}",
                                         strlen("{\"v\":\"abc\"}"),
                                         lonejson_owned_buffer_sink, &out,
                                         &options, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  lonejson_owned_buffer_free(&out);

  visitor = lonejson_default_value_visitor();
  visitor.number_end = test_value_rewrite_fail_event;
  options.old_value_visitor = &visitor;
  lonejson_owned_buffer_init(&out);
  status = test_value_rewrite_buffer("{\"v\":123}",
                                         strlen("{\"v\":123}"),
                                         lonejson_owned_buffer_sink, &out,
                                         &options, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  lonejson_owned_buffer_free(&out);

  visitor = lonejson_default_value_visitor();
  visitor.boolean_value = test_value_rewrite_fail_bool;
  options.old_value_visitor = &visitor;
  lonejson_owned_buffer_init(&out);
  status = test_value_rewrite_buffer("{\"v\":true}",
                                         strlen("{\"v\":true}"),
                                         lonejson_owned_buffer_sink, &out,
                                         &options, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  lonejson_owned_buffer_free(&out);

  visitor = lonejson_default_value_visitor();
  visitor.null_value = test_value_rewrite_fail_event;
  options.old_value_visitor = &visitor;
  lonejson_owned_buffer_init(&out);
  status = test_value_rewrite_buffer("{\"v\":null}",
                                         strlen("{\"v\":null}"),
                                         lonejson_owned_buffer_sink, &out,
                                         &options, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  lonejson_owned_buffer_free(&out);
}

static void test_value_rewrite_adapter_and_sink_failures(void) {
  lonejson_buffer_reader reader;
  lonejson_read_result rr;
  unsigned char buffer[4];
  lonejson_value_rewrite_options options;
  lonejson_owned_buffer owned;
  lonejson_error error;
  test_failing_sink failing_sink;
  const char *path_missing_array[] = {"items", "2"};
  lonejson_status status;

  lonejson_buffer_reader_init(&reader, "abc", 3u);
  rr = lonejson_buffer_reader_read(&reader, buffer, 2u);
  EXPECT(rr.bytes_read == 2u);
  EXPECT(rr.eof == 0);
  rr = lonejson_buffer_reader_read(&reader, buffer, 0u);
  EXPECT(rr.bytes_read == 0u);
  EXPECT(rr.eof == 0);
  rr = lonejson_buffer_reader_read(&reader, buffer, sizeof(buffer));
  EXPECT(rr.bytes_read == 1u);
  EXPECT(rr.eof != 0);
  lonejson_buffer_reader_init(&reader, NULL, 3u);
  rr = lonejson_buffer_reader_read(&reader, buffer, sizeof(buffer));
  EXPECT(rr.error_code != 0);
  lonejson_buffer_reader_init(&reader, NULL, 0u);
  rr = lonejson_buffer_reader_read(&reader, buffer, sizeof(buffer));
  EXPECT(rr.eof != 0);

  memset(&options, 0, sizeof(options));
  options.action = LONEJSON_VALUE_REWRITE_KEEP;
  EXPECT(test_value_rewrite_buffer(
             NULL, 3u, test_buffer_sink_write, &failing_sink, &options,
             &error) == LONEJSON_STATUS_INVALID_ARGUMENT);

  memset(&failing_sink, 0, sizeof(failing_sink));
  failing_sink.fail_after = 2u;
  EXPECT(test_value_rewrite_buffer("{\"v\":1}", strlen("{\"v\":1}"),
                                       test_failing_sink_write, &failing_sink,
                                       &options, &error) ==
         LONEJSON_STATUS_CALLBACK_FAILED);

  lonejson_owned_buffer_init(&owned);
  owned.allocator = lonejson_default_allocator();
  owned.allocator.malloc_fn = test_fail_malloc;
  EXPECT(lonejson_owned_buffer_sink(&owned, "x", 1u, &error) ==
         LONEJSON_STATUS_ALLOCATION_FAILED);
  lonejson_owned_buffer_free(&owned);

  memset(&options, 0, sizeof(options));
  options.target_segments = path_missing_array;
  options.target_segment_count = 2u;
  options.action = LONEJSON_VALUE_REWRITE_REPLACE_WITH;
  options.replace = test_value_rewrite_no_value;
  status = test_value_rewrite_buffer("{\"items\":[1]}",
                                         strlen("{\"items\":[1]}"),
                                         lonejson_owned_buffer_sink, &owned,
                                         &options, &error);
  EXPECT(status == LONEJSON_STATUS_TYPE_MISMATCH);
}

static void test_value_rewrite_selector_and_source_base64(void) {
  unsigned char out[512];
  test_buffer_sink sink;
  test_reader_state reader;
  lonejson_value_rewrite_selector_options options;
  lonejson_source source;
  lonejson_error error;
  char bytes_path[] = "/tmp/lonejson-value-rewrite-bytes-XXXXXX";
  int bytes_fd;
  static const unsigned char bytes_payload[] = {0x00u, 0x01u, 0x7fu};
  lonejson_status status;

  bytes_fd = mkstemp(bytes_path);
  EXPECT(bytes_fd >= 0);
  if (bytes_fd < 0) {
    return;
  }
  EXPECT(write(bytes_fd, bytes_payload, sizeof(bytes_payload)) ==
         (ssize_t)sizeof(bytes_payload));
  close(bytes_fd);
  lonejson_source_init(&source);
  EXPECT(lonejson_source_set_path(&source, bytes_path, &error) ==
         LONEJSON_STATUS_OK);
  sink.length = 0u;
  out[0] = '\0';
  sink.buffer = out;
  sink.capacity = sizeof(out);
  memset(&options, 0, sizeof(options));
  options.selector = "meta.payload\\.bytes";
  options.action = LONEJSON_VALUE_REWRITE_REPLACE;
  options.replacement.emit = test_value_rewrite_emit_source_base64;
  options.replacement.emit_user = &source;
  reader.json = "{\"meta\":{\"payload.bytes\":null},\"tail\":1}";
  reader.offset = 0u;
  reader.chunk_size = 2u;
  status = test_value_rewrite_selector_reader(
      test_state_reader, &reader, test_buffer_sink_write, &sink, &options,
      &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)out,
                "{\"meta\":{\"payload.bytes\":\"AAF/\"},\"tail\":1}") == 0);
  lonejson_source_cleanup(&source);
  unlink(bytes_path);
}

static void test_value_rewrite_missing_object_path_and_streaming_string(void) {
  unsigned char out[512];
  test_buffer_sink sink;
  test_reader_state reader;
  lonejson_json_value replacement;
  lonejson_value_rewrite_options options;
  lonejson_error error;
  const char *path_missing[] = {"meta", "request", "id"};
  const char *path_text[] = {"text"};
  lonejson_status status;

  sink.length = 0u;
  out[0] = '\0';
  sink.buffer = out;
  sink.capacity = sizeof(out);
  lonejson_json_value_init(test_default_runtime(), &replacement);
  EXPECT(lonejson_json_value_set_buffer(&replacement, "42", 2u, &error) ==
         LONEJSON_STATUS_OK);
  memset(&options, 0, sizeof(options));
  options.target_segments = path_missing;
  options.target_segment_count = 3u;
  options.action = LONEJSON_VALUE_REWRITE_REPLACE;
  options.replacement.json = &replacement;
  reader.json = "{\"meta\":{\"ok\":true},\"tail\":null}";
  reader.offset = 0u;
  reader.chunk_size = 1u;
  status = test_value_rewrite_reader(
      test_state_reader, &reader, test_buffer_sink_write, &sink, &options,
      &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)out,
                "{\"meta\":{\"ok\":true,\"request\":{\"id\":42}},"
                "\"tail\":null}") == 0);

  sink.length = 0u;
  out[0] = '\0';
  memset(&options, 0, sizeof(options));
  options.target_segments = path_text;
  options.target_segment_count = 1u;
  options.action = LONEJSON_VALUE_REWRITE_KEEP;
  reader.json = "{\"text\":\"ab\\ncd\\\"ef\",\"n\":123}";
  reader.offset = 0u;
  reader.chunk_size = 2u;
  status = test_value_rewrite_reader(
      test_state_reader, &reader, test_buffer_sink_write, &sink, &options,
      &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)out, "{\"text\":\"ab\\ncd\\\"ef\",\"n\":123}") ==
         0);
  lonejson_json_value_cleanup(&replacement);
}

static void test_value_rewrite_failures(void) {
  unsigned char out[128];
  test_buffer_sink sink;
  test_reader_state reader;
  lonejson_value_rewrite_options options;
  lonejson_error error;
  const char *path_missing_array[] = {"items", "2"};
  lonejson_status status;

  memset(&sink, 0, sizeof(sink));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  memset(&options, 0, sizeof(options));
  EXPECT(test_value_rewrite_reader(NULL, NULL, test_buffer_sink_write,
                                       &sink, &options, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);

  EXPECT(test_value_rewrite_selector_reader(
             test_state_reader, &reader, test_buffer_sink_write, &sink, NULL,
             &error) == LONEJSON_STATUS_INVALID_ARGUMENT);

  options.action = LONEJSON_VALUE_REWRITE_DROP;
  reader.json = "true";
  reader.offset = 0u;
  reader.chunk_size = 8u;
  EXPECT(test_value_rewrite_reader(
             test_state_reader, &reader, test_buffer_sink_write, &sink,
             &options, &error) == LONEJSON_STATUS_INVALID_ARGUMENT);

  memset(&options, 0, sizeof(options));
  options.action = LONEJSON_VALUE_REWRITE_REPLACE;
  options.replacement.emit = test_value_rewrite_emit_dynamic;
  options.replacement.json = (const lonejson_json_value *)1;
  EXPECT(test_value_rewrite_reader(test_state_reader, &reader,
                                       test_buffer_sink_write, &sink, &options,
                                       &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);

  memset(&options, 0, sizeof(options));
  options.target_segments = path_missing_array;
  options.target_segment_count = 2u;
  options.action = LONEJSON_VALUE_REWRITE_DROP;
  reader.json = "{\"items\":[1]}";
  reader.offset = 0u;
  reader.chunk_size = 8u;
  status = test_value_rewrite_reader(
      test_state_reader, &reader, test_buffer_sink_write, &sink, &options,
      &error);
  EXPECT(status == LONEJSON_STATUS_TYPE_MISMATCH);
}
