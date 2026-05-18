static void test_writer_dynamic_object_and_values(void) {
  unsigned char out[512];
  test_buffer_sink sink;
  lonejson_writer writer;
  lonejson_json_value raw;
  test_event event;
  lonejson_error error;
  lonejson_status status;

  memset(&sink, 0, sizeof(sink));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  lonejson_json_value_init(&raw);
  EXPECT(lonejson_json_value_set_buffer(&raw, "{\"z\":[true,null]}", 17u,
                                        &error) == LONEJSON_STATUS_OK);
  memset(&event, 0, sizeof(event));
  strcpy(event.id, "e1");
  event.ok = true;

  status = lonejson_writer_init_sink(&writer, test_buffer_sink_write, &sink,
                                     NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(lonejson_writer_begin_object(&writer, &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_writer_key(&writer, "a\"b", 3u, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_writer_string(&writer, "x\ny\\z", 5u, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_writer_key(&writer, "n", 1u, &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_writer_i64(&writer, -42, &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_writer_key(&writer, "raw", 3u, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_writer_json_value(&writer, &raw, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_writer_key(&writer, "mapped", 6u, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_writer_mapped(&writer, &test_event_map, &event, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_writer_end_object(&writer, &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_writer_finish(&writer, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)out,
                "{\"a\\\"b\":\"x\\ny\\\\z\",\"n\":-42,"
                "\"raw\":{\"z\":[true,null]},"
                "\"mapped\":{\"id\":\"e1\",\"ok\":true}}") == 0);
  lonejson_writer_cleanup(&writer);
  lonejson_json_value_cleanup(&raw);
}

static void test_writer_primitive_string_sources(void) {
  unsigned char out[512];
  test_buffer_sink sink;
  test_reader_state reader;
  lonejson_spooled spool;
  lonejson_spool_options spool_options;
  lonejson_source source;
  lonejson_error error;
  lonejson_status status;
  lonejson_writer writer;
  char bytes_path[] = "/tmp/lonejson-writer-source-bytes-XXXXXX";
  int bytes_fd;
  static const unsigned char bytes_payload[] = {0x01u, 0x02u, 0x03u};

  memset(&sink, 0, sizeof(sink));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  reader.json = "ab\"\\\ncd";
  reader.offset = 0u;
  reader.chunk_size = 2u;
  status = lonejson_write_json_string_sink(test_state_reader, &reader,
                                           test_buffer_sink_write, &sink, NULL,
                                           &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)out, "\"ab\\\"\\\\\\ncd\"") == 0);

  memset(&sink, 0, sizeof(sink));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  memset(&spool_options, 0, sizeof(spool_options));
  spool_options.memory_limit = 4u;
  spool_options.temp_dir = "/tmp";
  lonejson_spooled_init(&spool, &spool_options);
  EXPECT(lonejson_spooled_append(&spool, "hello\nworld", 11u, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_spooled_spilled(&spool) != 0);
  status = lonejson_write_json_string_spooled_sink(
      &spool, test_buffer_sink_write, &sink, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)out, "\"hello\\nworld\"") == 0);

  bytes_fd = mkstemp(bytes_path);
  EXPECT(bytes_fd >= 0);
  if (bytes_fd >= 0) {
    EXPECT(write(bytes_fd, bytes_payload, sizeof(bytes_payload)) ==
           (ssize_t)sizeof(bytes_payload));
    close(bytes_fd);
    lonejson_source_init(&source);
    EXPECT(lonejson_source_set_path(&source, bytes_path, &error) ==
           LONEJSON_STATUS_OK);
    memset(&sink, 0, sizeof(sink));
    sink.buffer = out;
    sink.capacity = sizeof(out);
    EXPECT(lonejson_writer_init_sink(&writer, test_buffer_sink_write, &sink,
                                     NULL, &error) == LONEJSON_STATUS_OK);
    EXPECT(lonejson_writer_begin_array(&writer, &error) ==
           LONEJSON_STATUS_OK);
    EXPECT(lonejson_writer_spooled_base64(&writer, &spool, &error) ==
           LONEJSON_STATUS_OK);
    EXPECT(lonejson_writer_source_base64(&writer, &source, &error) ==
           LONEJSON_STATUS_OK);
    EXPECT(lonejson_writer_end_array(&writer, &error) == LONEJSON_STATUS_OK);
    EXPECT(lonejson_writer_finish(&writer, &error) == LONEJSON_STATUS_OK);
    EXPECT(strcmp((const char *)out, "[\"aGVsbG8Kd29ybGQ=\",\"AQID\"]") ==
           0);
    lonejson_writer_cleanup(&writer);
    lonejson_source_cleanup(&source);
    unlink(bytes_path);
  }
  lonejson_spooled_cleanup(&spool);
}

static void test_writer_invalid_state_and_sink_failure(void) {
  unsigned char out[32];
  test_buffer_sink sink;
  test_failing_sink failing;
  lonejson_writer writer;
  lonejson_error error;

  memset(&sink, 0, sizeof(sink));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  EXPECT(lonejson_writer_init_sink(&writer, test_buffer_sink_write, &sink,
                                   NULL, &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_writer_key(&writer, "x", 1u, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  lonejson_writer_cleanup(&writer);

  memset(&failing, 0, sizeof(failing));
  failing.fail_after = 2u;
  EXPECT(lonejson_writer_init_sink(&writer, test_failing_sink_write, &failing,
                                   NULL, &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_writer_string(&writer, "abc", 3u, &error) ==
         LONEJSON_STATUS_CALLBACK_FAILED);
  lonejson_writer_cleanup(&writer);
}

static void test_writer_number_text_rejects_non_numbers(void) {
  static const char *invalid_values[] = {"true", "null", "\"x\"", "[]", "{}",
                                         "01",   "1x"};
  unsigned char out[64];
  test_buffer_sink sink;
  lonejson_writer writer;
  lonejson_error error;
  size_t i;

  for (i = 0u; i < sizeof(invalid_values) / sizeof(invalid_values[0]); i++) {
    memset(&sink, 0, sizeof(sink));
    sink.buffer = out;
    sink.capacity = sizeof(out);
    EXPECT(lonejson_writer_init_sink(&writer, test_buffer_sink_write, &sink,
                                     NULL, &error) == LONEJSON_STATUS_OK);
    EXPECT(lonejson_writer_number_text(&writer, invalid_values[i],
                                       strlen(invalid_values[i]), &error) ==
           LONEJSON_STATUS_INVALID_JSON);
    EXPECT(sink.length == 0u);
    lonejson_writer_cleanup(&writer);
  }

  memset(&sink, 0, sizeof(sink));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  EXPECT(lonejson_writer_init_sink(&writer, test_buffer_sink_write, &sink, NULL,
                                   &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_writer_number_text(&writer, "-12.5e+2", 8u, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_writer_finish(&writer, &error) == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out, "-12.5e+2") == 0);
  lonejson_writer_cleanup(&writer);
}

typedef struct test_writer_generator_ctx {
  int step;
} test_writer_generator_ctx;

typedef struct test_writer_primitive_generator_ctx {
  int step;
  int calls[9];
} test_writer_primitive_generator_ctx;

typedef struct test_writer_string_reader_generator_ctx {
  int step;
  test_reader_state reader;
} test_writer_string_reader_generator_ctx;

enum { TEST_WRITER_STREAM_PAYLOAD_LEN = 16384 };

typedef struct test_writer_large_mapped_doc {
  char payload[TEST_WRITER_STREAM_PAYLOAD_LEN + 1u];
} test_writer_large_mapped_doc;

static const lonejson_field test_writer_large_mapped_doc_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(test_writer_large_mapped_doc, payload,
                                    "payload", LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(test_writer_large_mapped_doc_map,
                    test_writer_large_mapped_doc,
                    test_writer_large_mapped_doc_fields);

typedef struct test_writer_source_generator_ctx {
  int step;
  int source_calls;
  lonejson_source source;
} test_writer_source_generator_ctx;

typedef struct test_writer_json_value_generator_ctx {
  int step;
  int value_calls;
  lonejson_json_value value;
} test_writer_json_value_generator_ctx;

typedef struct test_writer_mapped_generator_ctx {
  int step;
  int mapped_calls;
  test_writer_large_mapped_doc doc;
} test_writer_mapped_generator_ctx;

typedef struct test_writer_retry_mismatch_ctx {
  int calls;
  int kind;
  int step;
} test_writer_retry_mismatch_ctx;

typedef struct test_writer_child_retry_mismatch_ctx {
  int calls;
  int use_mapped;
  lonejson_json_value first;
  lonejson_json_value second;
  test_writer_large_mapped_doc first_doc;
  test_writer_large_mapped_doc second_doc;
} test_writer_child_retry_mismatch_ctx;

typedef struct test_writer_root_string_generator_ctx {
  int step;
  int string_calls;
} test_writer_root_string_generator_ctx;

typedef struct test_writer_object_generator_ctx {
  int step;
  int calls[6];
} test_writer_object_generator_ctx;

typedef struct test_writer_backpressure_ctx {
  int step;
  int calls[16];
  lonejson_source source;
  lonejson_json_value json_value;
  test_reader_state json_reader;
  test_writer_large_mapped_doc mapped;
} test_writer_backpressure_ctx;

typedef struct test_writer_equivalence_ctx {
  int step;
  int calls[40];
  lonejson_source text_source;
  lonejson_source b64_source;
  lonejson_spooled text_spool;
  lonejson_spooled b64_spool;
  lonejson_json_value json_value;
  test_writer_large_mapped_doc mapped;
} test_writer_equivalence_ctx;

typedef struct test_writer_equivalence_init {
  const char *text_source_path;
  const char *b64_source_path;
} test_writer_equivalence_init;

typedef lonejson_status (*test_writer_model_producer_fn)(
    lonejson_writer *writer, void *user, lonejson_error *error);
typedef void (*test_writer_model_init_fn)(void *ctx, void *user);
typedef void (*test_writer_model_cleanup_fn)(void *ctx);
typedef const int *(*test_writer_model_calls_fn)(const void *ctx);
typedef int (*test_writer_model_step_fn)(const void *ctx);

typedef struct test_writer_generator_model {
  const char *name;
  size_t ctx_size;
  test_writer_model_init_fn init;
  test_writer_model_cleanup_fn cleanup;
  test_writer_model_producer_fn producer;
  test_writer_model_calls_fn calls;
  test_writer_model_step_fn step;
  void *init_user;
  size_t completed_step;
  const size_t *retry_steps;
  size_t retry_step_count;
  size_t output_capacity;
} test_writer_generator_model;

static size_t test_writer_generator_read_all(lonejson_generator *generator,
                                             size_t chunk_size,
                                             unsigned char *all,
                                             size_t all_capacity) {
  unsigned char chunk[32];
  size_t all_len;
  size_t out_len;
  int eof;
  lonejson_status status;

  EXPECT(chunk_size <= sizeof(chunk));
  all_len = 0u;
  eof = 0;
  while (!eof) {
    status = lonejson_generator_read(generator, chunk, chunk_size, &out_len,
                                     &eof);
    EXPECT(status == LONEJSON_STATUS_OK);
    if (status != LONEJSON_STATUS_OK) {
      break;
    }
    EXPECT(out_len != 0u || eof);
    if (out_len == 0u && !eof) {
      break;
    }
    EXPECT(all_len + out_len < all_capacity);
    if (all_len + out_len >= all_capacity) {
      break;
    }
    memcpy(all + all_len, chunk, out_len);
    all_len += out_len;
  }
  all[all_len] = '\0';
  return all_len;
}

static size_t test_writer_generator_read_all_pattern(
    lonejson_generator *generator, const size_t *capacities,
    size_t capacity_count, unsigned char *all, size_t all_capacity) {
  unsigned char chunk[32];
  size_t all_len;
  size_t out_len;
  size_t i;
  int eof;
  lonejson_status status;

  EXPECT(capacity_count != 0u);
  all_len = 0u;
  eof = 0;
  i = 0u;
  while (!eof) {
    size_t capacity = capacities[i % capacity_count];
    EXPECT(capacity <= sizeof(chunk));
    status = lonejson_generator_read(generator, chunk, capacity, &out_len,
                                     &eof);
    EXPECT(status == LONEJSON_STATUS_OK);
    if (status != LONEJSON_STATUS_OK) {
      break;
    }
    EXPECT(out_len <= capacity);
    if (capacity == 0u) {
      EXPECT(out_len == 0u);
      EXPECT(eof == 0);
      i++;
      continue;
    }
    EXPECT(out_len != 0u || eof);
    if (out_len == 0u && !eof) {
      break;
    }
    EXPECT(all_len + out_len < all_capacity);
    if (all_len + out_len >= all_capacity) {
      break;
    }
    memcpy(all + all_len, chunk, out_len);
    all_len += out_len;
    i++;
  }
  all[all_len] = '\0';
  return all_len;
}

static lonejson_status test_writer_generator_producer(lonejson_writer *writer,
                                                      void *user,
                                                      lonejson_error *error) {
  test_writer_generator_ctx *ctx;
  lonejson_status status;

  ctx = (test_writer_generator_ctx *)user;
  for (;;) {
    switch (ctx->step) {
    case 0:
      status = lonejson_writer_begin_array(writer, error);
      break;
    case 1:
      status = lonejson_writer_bool(writer, 1, error);
      break;
    case 2:
      status = lonejson_writer_null(writer, error);
      break;
    case 3:
      status = lonejson_writer_i64(writer, 12345, error);
      break;
    case 4:
      status = lonejson_writer_end_array(writer, error);
      break;
    default:
      return LONEJSON_STATUS_OK;
    }
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    ctx->step++;
  }
}

static lonejson_status test_writer_primitive_generator_producer(
    lonejson_writer *writer, void *user, lonejson_error *error) {
  test_writer_primitive_generator_ctx *ctx;
  lonejson_status status;

  ctx = (test_writer_primitive_generator_ctx *)user;
  for (;;) {
    switch (ctx->step) {
    case 0:
      ctx->calls[0]++;
      status = lonejson_writer_begin_array(writer, error);
      break;
    case 1:
      ctx->calls[1]++;
      status = lonejson_writer_i64(writer, 0, error);
      break;
    case 2:
      ctx->calls[2]++;
      status = lonejson_writer_i64(writer, -12, error);
      break;
    case 3:
      ctx->calls[3]++;
      status = lonejson_writer_i64(writer, -1234567890, error);
      break;
    case 4:
      ctx->calls[4]++;
      status = lonejson_writer_u64(
          writer, (lonejson_uint64)0u - (lonejson_uint64)1u, error);
      break;
    case 5:
      ctx->calls[5]++;
      status = lonejson_writer_f64(writer, -12.5, error);
      break;
    case 6:
      ctx->calls[6]++;
      status = lonejson_writer_bool(writer, 1, error);
      break;
    case 7:
      ctx->calls[7]++;
      status = lonejson_writer_null(writer, error);
      break;
    case 8:
      ctx->calls[8]++;
      status = lonejson_writer_end_array(writer, error);
      break;
    default:
      return LONEJSON_STATUS_OK;
    }
    if (status == LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    ctx->step++;
  }
}

static lonejson_status test_writer_root_string_generator_producer(
    lonejson_writer *writer, void *user, lonejson_error *error) {
  test_writer_root_string_generator_ctx *ctx;
  lonejson_status status;

  ctx = (test_writer_root_string_generator_ctx *)user;
  for (;;) {
    switch (ctx->step) {
    case 0:
      ctx->string_calls++;
      status = lonejson_writer_string(writer, "ab\"\\\ncd", 7u, error);
      break;
    default:
      return LONEJSON_STATUS_OK;
    }
    if (status == LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    ctx->step++;
  }
}

static lonejson_status test_writer_object_generator_producer(
    lonejson_writer *writer, void *user, lonejson_error *error) {
  test_writer_object_generator_ctx *ctx;
  lonejson_status status;

  ctx = (test_writer_object_generator_ctx *)user;
  for (;;) {
    switch (ctx->step) {
    case 0:
      ctx->calls[0]++;
      status = lonejson_writer_begin_object(writer, error);
      break;
    case 1:
      ctx->calls[1]++;
      status = lonejson_writer_key(writer, "a\"b", 3u, error);
      break;
    case 2:
      ctx->calls[2]++;
      status = lonejson_writer_string(writer, "x\ny\\z", 5u, error);
      break;
    case 3:
      ctx->calls[3]++;
      status = lonejson_writer_key(writer, "n", 1u, error);
      break;
    case 4:
      ctx->calls[4]++;
      status = lonejson_writer_number_text(writer, "-12.5e+2", 8u, error);
      break;
    case 5:
      ctx->calls[5]++;
      status = lonejson_writer_end_object(writer, error);
      break;
    default:
      return LONEJSON_STATUS_OK;
    }
    if (status == LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    ctx->step++;
  }
}

static lonejson_status test_writer_source_generator_producer(
    lonejson_writer *writer, void *user, lonejson_error *error) {
  test_writer_source_generator_ctx *ctx;
  lonejson_status status;

  ctx = (test_writer_source_generator_ctx *)user;
  for (;;) {
    switch (ctx->step) {
    case 0:
      status = lonejson_writer_begin_array(writer, error);
      break;
    case 1:
      ctx->source_calls++;
      status = lonejson_writer_source_text(writer, &ctx->source, error);
      break;
    case 2:
      status = lonejson_writer_end_array(writer, error);
      break;
    default:
      return LONEJSON_STATUS_OK;
    }
    if (status == LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    ctx->step++;
  }
}

static lonejson_status test_writer_json_value_generator_producer(
    lonejson_writer *writer, void *user, lonejson_error *error) {
  test_writer_json_value_generator_ctx *ctx;
  lonejson_status status;

  ctx = (test_writer_json_value_generator_ctx *)user;
  for (;;) {
    switch (ctx->step) {
    case 0:
      status = lonejson_writer_begin_array(writer, error);
      break;
    case 1:
      ctx->value_calls++;
      status = lonejson_writer_json_value(writer, &ctx->value, error);
      break;
    case 2:
      status = lonejson_writer_end_array(writer, error);
      break;
    default:
      return LONEJSON_STATUS_OK;
    }
    if (status == LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    ctx->step++;
  }
}

static lonejson_status test_writer_mapped_generator_producer(
    lonejson_writer *writer, void *user, lonejson_error *error) {
  test_writer_mapped_generator_ctx *ctx;
  lonejson_status status;

  ctx = (test_writer_mapped_generator_ctx *)user;
  for (;;) {
    switch (ctx->step) {
    case 0:
      status = lonejson_writer_begin_array(writer, error);
      break;
    case 1:
      ctx->mapped_calls++;
      status = lonejson_writer_mapped(
          writer, &test_writer_large_mapped_doc_map, &ctx->doc, error);
      break;
    case 2:
      status = lonejson_writer_end_array(writer, error);
      break;
    default:
      return LONEJSON_STATUS_OK;
    }
    if (status == LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    ctx->step++;
  }
}

static lonejson_status test_writer_child_retry_mismatch_producer(
    lonejson_writer *writer, void *user, lonejson_error *error) {
  test_writer_child_retry_mismatch_ctx *ctx;

  ctx = (test_writer_child_retry_mismatch_ctx *)user;
  ctx->calls++;
  if (ctx->use_mapped) {
    return lonejson_writer_mapped(
        writer, &test_writer_large_mapped_doc_map,
        ctx->calls == 1 ? &ctx->first_doc : &ctx->second_doc, error);
  }
  return lonejson_writer_json_value(writer,
                                    ctx->calls == 1 ? &ctx->first
                                                    : &ctx->second,
                                    error);
}

static lonejson_status test_writer_retry_mismatch_producer(
    lonejson_writer *writer, void *user, lonejson_error *error) {
  test_writer_retry_mismatch_ctx *ctx;
  lonejson_status status;

  ctx = (test_writer_retry_mismatch_ctx *)user;
  if (ctx->kind == 1) {
    for (;;) {
      switch (ctx->step) {
      case 0:
        status = lonejson_writer_begin_object(writer, error);
        break;
      case 1:
        ctx->calls++;
        status = lonejson_writer_key(
            writer,
            ctx->calls == 1 ? "abcdefghijklmnopqrstuvwxyz"
                            : "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
            26u, error);
        break;
      default:
        return LONEJSON_STATUS_OK;
      }
      if (status == LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      ctx->step++;
    }
  }
  ctx->calls++;
  switch (ctx->kind) {
  case 0:
    return lonejson_writer_string(
        writer,
        ctx->calls == 1 ? "abcdefghijklmnopqrstuvwxyz"
                        : "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
        26u, error);
  case 2:
    return lonejson_writer_number_text(
        writer, ctx->calls == 1 ? "-1234567890" : "12345678901", 11u, error);
  case 3:
    return lonejson_writer_i64(writer, ctx->calls == 1 ? -1234567890 : -12,
                               error);
  case 4:
    return lonejson_writer_bool(writer, ctx->calls == 1, error);
  default:
    break;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_writer_backpressure_producer(
    lonejson_writer *writer, void *user, lonejson_error *error) {
  test_writer_backpressure_ctx *ctx;
  lonejson_status status;

  ctx = (test_writer_backpressure_ctx *)user;
  for (;;) {
    switch (ctx->step) {
    case 0:
      ctx->calls[0]++;
      status = lonejson_writer_begin_object(writer, error);
      break;
    case 1:
      ctx->calls[1]++;
      status = lonejson_writer_key(writer, "s", 1u, error);
      break;
    case 2:
      ctx->calls[2]++;
      status = lonejson_writer_string(writer, "ab\"\\\ncd", 7u, error);
      break;
    case 3:
      ctx->calls[3]++;
      status = lonejson_writer_key(writer, "n", 1u, error);
      break;
    case 4:
      ctx->calls[4]++;
      status = lonejson_writer_number_text(writer, "-12.5e+2", 8u, error);
      break;
    case 5:
      ctx->calls[5]++;
      status = lonejson_writer_key(writer, "src", 3u, error);
      break;
    case 6:
      ctx->calls[6]++;
      status = lonejson_writer_source_text(writer, &ctx->source, error);
      break;
    case 7:
      ctx->calls[7]++;
      status = lonejson_writer_key(writer, "raw", 3u, error);
      break;
    case 8: {
      ctx->calls[8]++;
      status = lonejson_writer_json_value(writer, &ctx->json_value, error);
      break;
    }
    case 9:
      ctx->calls[9]++;
      status = lonejson_writer_key(writer, "mapped", 6u, error);
      break;
    case 10:
      ctx->calls[10]++;
      status = lonejson_writer_mapped(
          writer, &test_writer_large_mapped_doc_map, &ctx->mapped, error);
      break;
    case 11:
      ctx->calls[11]++;
      status = lonejson_writer_key(writer, "arr", 3u, error);
      break;
    case 12:
      ctx->calls[12]++;
      status = lonejson_writer_begin_array(writer, error);
      break;
    case 13:
      ctx->calls[13]++;
      status = lonejson_writer_bool(writer, 1, error);
      break;
    case 14:
      ctx->calls[14]++;
      status = lonejson_writer_null(writer, error);
      break;
    case 15:
      ctx->calls[15]++;
      status = lonejson_writer_end_array(writer, error);
      break;
    case 16:
      status = lonejson_writer_end_object(writer, error);
      break;
    default:
      return LONEJSON_STATUS_OK;
    }
    if (status == LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    ctx->step++;
  }
}

static lonejson_status test_writer_string_reader_generator_producer(
    lonejson_writer *writer, void *user, lonejson_error *error) {
  test_writer_string_reader_generator_ctx *ctx;
  lonejson_status status;

  ctx = (test_writer_string_reader_generator_ctx *)user;
  for (;;) {
    switch (ctx->step) {
    case 0:
      status = lonejson_writer_begin_array(writer, error);
      break;
    case 1:
      status = lonejson_writer_string_reader(writer, test_state_reader,
                                             &ctx->reader, error);
      break;
    case 2:
      status = lonejson_writer_bool(writer, 1, error);
      break;
    case 3:
      status = lonejson_writer_end_array(writer, error);
      break;
    default:
      return LONEJSON_STATUS_OK;
    }
    if (status == LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    ctx->step++;
  }
}

static lonejson_status test_writer_equivalence_producer(
    lonejson_writer *writer, void *user, lonejson_error *error) {
  test_writer_equivalence_ctx *ctx;
  lonejson_status status;

  ctx = (test_writer_equivalence_ctx *)user;
  for (;;) {
    ctx->calls[ctx->step]++;
    switch (ctx->step) {
    case 0:
      status = lonejson_writer_begin_object(writer, error);
      break;
    case 1:
      status = lonejson_writer_key(writer, "str", 3u, error);
      break;
    case 2:
      status = lonejson_writer_string(writer, "ab\"\\\ncd", 7u, error);
      break;
    case 3:
      status = lonejson_writer_key(writer, "txtnum", 6u, error);
      break;
    case 4:
      status = lonejson_writer_number_text(writer, "-12.5e+2", 8u, error);
      break;
    case 5:
      status = lonejson_writer_key(writer, "i0", 2u, error);
      break;
    case 6:
      status = lonejson_writer_i64(writer, 0, error);
      break;
    case 7:
      status = lonejson_writer_key(writer, "ineg", 4u, error);
      break;
    case 8:
      status = lonejson_writer_i64(writer, -1234567890, error);
      break;
    case 9:
      status = lonejson_writer_key(writer, "u", 1u, error);
      break;
    case 10:
      status = lonejson_writer_u64(
          writer, (lonejson_uint64)0u - (lonejson_uint64)1u, error);
      break;
    case 11:
      status = lonejson_writer_key(writer, "f", 1u, error);
      break;
    case 12:
      status = lonejson_writer_f64(writer, -12.5, error);
      break;
    case 13:
      status = lonejson_writer_key(writer, "b", 1u, error);
      break;
    case 14:
      status = lonejson_writer_bool(writer, 0, error);
      break;
    case 15:
      status = lonejson_writer_key(writer, "z", 1u, error);
      break;
    case 16:
      status = lonejson_writer_null(writer, error);
      break;
    case 17:
      status = lonejson_writer_key(writer, "spool_text", 10u, error);
      break;
    case 18:
      status = lonejson_writer_string_spooled(writer, &ctx->text_spool, error);
      break;
    case 19:
      status = lonejson_writer_key(writer, "source_text", 11u, error);
      break;
    case 20:
      status = lonejson_writer_source_text(writer, &ctx->text_source, error);
      break;
    case 21:
      status = lonejson_writer_key(writer, "spool_b64", 9u, error);
      break;
    case 22:
      status = lonejson_writer_spooled_base64(writer, &ctx->b64_spool, error);
      break;
    case 23:
      status = lonejson_writer_key(writer, "source_b64", 10u, error);
      break;
    case 24:
      status = lonejson_writer_source_base64(writer, &ctx->b64_source, error);
      break;
    case 25:
      status = lonejson_writer_key(writer, "raw", 3u, error);
      break;
    case 26:
      status = lonejson_writer_json_value(writer, &ctx->json_value, error);
      break;
    case 27:
      status = lonejson_writer_key(writer, "mapped", 6u, error);
      break;
    case 28:
      status = lonejson_writer_mapped(
          writer, &test_writer_large_mapped_doc_map, &ctx->mapped, error);
      break;
    case 29:
      status = lonejson_writer_key(writer, "arr", 3u, error);
      break;
    case 30:
      status = lonejson_writer_begin_array(writer, error);
      break;
    case 31:
      status = lonejson_writer_i64(writer, -12, error);
      break;
    case 32:
      status = lonejson_writer_string(writer, "", 0u, error);
      break;
    case 33:
      status = lonejson_writer_number_text(writer, "0", 1u, error);
      break;
    case 34:
      status = lonejson_writer_end_array(writer, error);
      break;
    case 35:
      status = lonejson_writer_end_object(writer, error);
      break;
    default:
      return LONEJSON_STATUS_OK;
    }
    if (status == LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    ctx->step++;
  }
}

static void test_writer_equivalence_write_file(char *path_template,
                                               const void *data, size_t len) {
  int fd;

  fd = mkstemp(path_template);
  EXPECT(fd >= 0);
  if (fd < 0) {
    return;
  }
  EXPECT(write(fd, data, len) == (ssize_t)len);
  close(fd);
}

static void test_writer_equivalence_ctx_init(void *raw_ctx, void *user) {
  static const char raw_json[] = "{\"nested\":[true,null,-1]}";
  static const char text_payload[] = "source text line\nwith quote \"";
  static const char b64_payload[] = "abc";
  test_writer_equivalence_ctx *ctx;
  test_writer_equivalence_init *init;
  lonejson_spool_options spool_options;
  size_t i;

  ctx = (test_writer_equivalence_ctx *)raw_ctx;
  init = (test_writer_equivalence_init *)user;
  memset(ctx, 0, sizeof(*ctx));
  lonejson_source_init(&ctx->text_source);
  lonejson_source_init(&ctx->b64_source);
  memset(&spool_options, 0, sizeof(spool_options));
  spool_options.memory_limit = 4u;
  spool_options.temp_dir = "/tmp";
  lonejson_spooled_init(&ctx->text_spool, &spool_options);
  lonejson_spooled_init(&ctx->b64_spool, &spool_options);
  EXPECT(lonejson_source_set_path(&ctx->text_source, init->text_source_path,
                                  NULL) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_source_set_path(&ctx->b64_source, init->b64_source_path,
                                  NULL) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_spooled_append(&ctx->text_spool, text_payload,
                                 sizeof(text_payload) - 1u,
                                 NULL) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_spooled_append(&ctx->b64_spool, b64_payload,
                                 sizeof(b64_payload) - 1u,
                                 NULL) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_spooled_spilled(&ctx->text_spool) != 0);
  lonejson_json_value_init(&ctx->json_value);
  EXPECT(lonejson_json_value_set_buffer(&ctx->json_value, raw_json,
                                        sizeof(raw_json) - 1u,
                                        NULL) == LONEJSON_STATUS_OK);
  for (i = 0u; i < TEST_WRITER_STREAM_PAYLOAD_LEN; i++) {
    ctx->mapped.payload[i] = (char)('a' + (int)(i % 26u));
  }
  ctx->mapped.payload[TEST_WRITER_STREAM_PAYLOAD_LEN] = '\0';
}

static void test_writer_equivalence_ctx_cleanup(void *raw_ctx) {
  test_writer_equivalence_ctx *ctx;

  ctx = (test_writer_equivalence_ctx *)raw_ctx;
  lonejson_source_cleanup(&ctx->text_source);
  lonejson_source_cleanup(&ctx->b64_source);
  lonejson_spooled_cleanup(&ctx->text_spool);
  lonejson_spooled_cleanup(&ctx->b64_spool);
  lonejson_json_value_cleanup(&ctx->json_value);
}

static const int *test_writer_equivalence_calls(const void *raw_ctx) {
  const test_writer_equivalence_ctx *ctx;

  ctx = (const test_writer_equivalence_ctx *)raw_ctx;
  return ctx->calls;
}

static int test_writer_equivalence_step(const void *raw_ctx) {
  const test_writer_equivalence_ctx *ctx;

  ctx = (const test_writer_equivalence_ctx *)raw_ctx;
  return ctx->step;
}

static size_t test_writer_generator_model_render_sink(
    const test_writer_generator_model *model, void *ctx, unsigned char *all,
    size_t all_capacity) {
  test_buffer_sink sink;
  lonejson_writer writer;
  lonejson_error error;

  memset(&sink, 0, sizeof(sink));
  sink.buffer = all;
  sink.capacity = all_capacity;
  EXPECT(lonejson_writer_init_sink(&writer, test_buffer_sink_write, &sink,
                                   NULL, &error) == LONEJSON_STATUS_OK);
  EXPECT(model->producer(&writer, ctx, &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_writer_finish(&writer, &error) == LONEJSON_STATUS_OK);
  lonejson_writer_cleanup(&writer);
  return sink.length;
}

static void
test_writer_generator_model_check(const test_writer_generator_model *model) {
  static const size_t patterns[][8] = {
      {1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u},
      {0u, 1u, 0u, 2u, 1u, 3u, 1u, 4u},
      {2u, 3u, 5u, 1u, 8u, 1u, 13u, 1u},
      {31u, 1u, 7u, 0u, 2u, 29u, 1u, 3u}};
  unsigned char expected[TEST_WRITER_STREAM_PAYLOAD_LEN + 2048u];
  int max_calls[64];
  size_t expected_len;
  size_t p;
  size_t i;

  EXPECT(model != NULL);
  EXPECT(model->ctx_size != 0u);
  EXPECT(model->ctx_size <= 65536u);
  EXPECT(model->output_capacity <= sizeof(expected));
  memset(max_calls, 0, sizeof(max_calls));

  {
    void *ctx = malloc(model->ctx_size);

    EXPECT(ctx != NULL);
    if (ctx == NULL) {
      return;
    }
    model->init(ctx, model->init_user);
    expected_len =
        test_writer_generator_model_render_sink(model, ctx, expected,
                                                model->output_capacity);
    EXPECT(model->step(ctx) == (int)model->completed_step);
    EXPECT(lonejson_validate_buffer(expected, expected_len, NULL) ==
           LONEJSON_STATUS_OK);
    model->cleanup(ctx);
    free(ctx);
  }

  for (p = 0u; p < sizeof(patterns) / sizeof(patterns[0]); p++) {
    lonejson_generator generator;
    void *ctx;
    unsigned char all[TEST_WRITER_STREAM_PAYLOAD_LEN + 2048u];
    size_t all_len;
    const int *calls;
    test_realloc_limit_state realloc_limit;
    lonejson_allocator allocator;
    lonejson_write_options options;

    ctx = malloc(model->ctx_size);
    EXPECT(ctx != NULL);
    if (ctx == NULL) {
      return;
    }
    model->init(ctx, model->init_user);
    memset(&realloc_limit, 0, sizeof(realloc_limit));
    realloc_limit.max_realloc_size = 65536u;
    allocator = lonejson_default_allocator();
    allocator.malloc_fn = test_realloc_limit_malloc;
    allocator.realloc_fn = test_realloc_limit_realloc;
    allocator.free_fn = test_realloc_limit_free;
    allocator.ctx = &realloc_limit;
    options = lonejson_default_write_options();
    options.allocator = &allocator;
    EXPECT(lonejson_writer_generator_init(&generator, model->producer, ctx,
                                          &options) == LONEJSON_STATUS_OK);
    realloc_limit.observed_max_realloc_size = 0u;
    all_len = test_writer_generator_read_all_pattern(
        &generator, patterns[p], sizeof(patterns[p]) / sizeof(patterns[p][0]),
        all, model->output_capacity);
    EXPECT(all_len == expected_len);
    EXPECT(memcmp(all, expected, expected_len) == 0);
    EXPECT(lonejson_validate_buffer(all, all_len, NULL) == LONEJSON_STATUS_OK);
    EXPECT(model->step(ctx) == (int)model->completed_step);
    EXPECT(realloc_limit.observed_max_realloc_size < 12288u);
    calls = model->calls(ctx);
    for (i = 0u; i < model->completed_step; i++) {
      EXPECT(calls[i] >= 1);
      if (i < sizeof(max_calls) / sizeof(max_calls[0]) &&
          calls[i] > max_calls[i]) {
        max_calls[i] = calls[i];
      }
    }
    lonejson_generator_cleanup(&generator);
    model->cleanup(ctx);
    free(ctx);
  }

  for (p = 1u; p <= 32u; p++) {
    lonejson_generator generator;
    void *ctx;
    unsigned char all[TEST_WRITER_STREAM_PAYLOAD_LEN + 2048u];
    size_t all_len;

    ctx = malloc(model->ctx_size);
    EXPECT(ctx != NULL);
    if (ctx == NULL) {
      return;
    }
    model->init(ctx, model->init_user);
    EXPECT(lonejson_writer_generator_init(&generator, model->producer, ctx,
                                          NULL) == LONEJSON_STATUS_OK);
    all_len = test_writer_generator_read_all(&generator, p, all,
                                             model->output_capacity);
    EXPECT(all_len == expected_len);
    EXPECT(memcmp(all, expected, expected_len) == 0);
    EXPECT(lonejson_validate_buffer(all, all_len, NULL) == LONEJSON_STATUS_OK);
    EXPECT(model->step(ctx) == (int)model->completed_step);
    lonejson_generator_cleanup(&generator);
    model->cleanup(ctx);
    free(ctx);
  }

  for (i = 0u; i < model->retry_step_count; i++) {
    size_t step = model->retry_steps[i];

    EXPECT(step < sizeof(max_calls) / sizeof(max_calls[0]));
    if (step < sizeof(max_calls) / sizeof(max_calls[0])) {
      EXPECT(max_calls[step] > 1);
    }
  }
}

static void test_writer_generator_streams_producer(void) {
  lonejson_generator generator;
  test_writer_generator_ctx ctx;
  unsigned char all[64];
  unsigned char chunk[64];
  size_t all_len;
  size_t out_len;
  int eof;
  lonejson_status status;

  memset(&ctx, 0, sizeof(ctx));
  status = lonejson_writer_generator_init(&generator,
                                          test_writer_generator_producer, &ctx,
                                          NULL);
  EXPECT(status == LONEJSON_STATUS_OK);
  all_len = 0u;
  eof = 0;
  while (!eof) {
    status = lonejson_generator_read(&generator, chunk, sizeof(chunk), &out_len,
                                     &eof);
    EXPECT(status == LONEJSON_STATUS_OK);
    if (status != LONEJSON_STATUS_OK) {
      break;
    }
    EXPECT(out_len != 0u || eof);
    if (out_len == 0u && !eof) {
      break;
    }
    EXPECT(all_len + out_len < sizeof(all));
    if (all_len + out_len >= sizeof(all)) {
      break;
    }
    memcpy(all + all_len, chunk, out_len);
    all_len += out_len;
  }
  all[all_len] = '\0';
  EXPECT(strcmp((const char *)all, "[true,null,12345]") == 0);
  lonejson_generator_cleanup(&generator);

  memset(&ctx, 0, sizeof(ctx));
  status = lonejson_writer_generator_init(
      &generator, test_writer_generator_producer, &ctx, NULL);
  EXPECT(status == LONEJSON_STATUS_OK);
  all_len = 0u;
  eof = 0;
  while (!eof) {
    status = lonejson_generator_read(&generator, chunk, 2u, &out_len, &eof);
    EXPECT(status == LONEJSON_STATUS_OK);
    if (status != LONEJSON_STATUS_OK) {
      break;
    }
    EXPECT(out_len != 0u || eof);
    if (out_len == 0u && !eof) {
      break;
    }
    EXPECT(all_len + out_len < sizeof(all));
    if (all_len + out_len >= sizeof(all)) {
      break;
    }
    memcpy(all + all_len, chunk, out_len);
    all_len += out_len;
  }
  all[all_len] = '\0';
  EXPECT(strcmp((const char *)all, "[true,null,12345]") == 0);
  lonejson_generator_cleanup(&generator);
}

static void test_writer_generator_preserves_event_state_on_backpressure(void) {
  lonejson_generator generator;
  test_writer_root_string_generator_ctx root_ctx;
  test_writer_object_generator_ctx object_ctx;
  unsigned char all[128];
  lonejson_status status;
  size_t i;

  memset(&root_ctx, 0, sizeof(root_ctx));
  status = lonejson_writer_generator_init(
      &generator, test_writer_root_string_generator_producer, &root_ctx, NULL);
  EXPECT(status == LONEJSON_STATUS_OK);
  test_writer_generator_read_all(&generator, 2u, all, sizeof(all));
  EXPECT(strcmp((const char *)all, "\"ab\\\"\\\\\\ncd\"") == 0);
  EXPECT(root_ctx.step == 1);
  EXPECT(root_ctx.string_calls > 1);
  lonejson_generator_cleanup(&generator);

  memset(&object_ctx, 0, sizeof(object_ctx));
  status = lonejson_writer_generator_init(
      &generator, test_writer_object_generator_producer, &object_ctx, NULL);
  EXPECT(status == LONEJSON_STATUS_OK);
  test_writer_generator_read_all(&generator, 1u, all, sizeof(all));
  EXPECT(strcmp((const char *)all, "{\"a\\\"b\":\"x\\ny\\\\z\",\"n\":-12.5e+2}") ==
         0);
  EXPECT(object_ctx.step == 6);
  for (i = 0u; i < sizeof(object_ctx.calls) / sizeof(object_ctx.calls[0]);
       i++) {
    EXPECT(object_ctx.calls[i] >= 1);
  }
  lonejson_generator_cleanup(&generator);
}

static void test_writer_generator_rejects_retry_mismatch(void) {
  size_t kind;

  for (kind = 0u; kind < 5u; kind++) {
    lonejson_generator generator;
    test_writer_retry_mismatch_ctx ctx;
    unsigned char chunk[2];
    size_t out_len;
    int eof;
    lonejson_status status;

    memset(&ctx, 0, sizeof(ctx));
    ctx.kind = (int)kind;
    status = lonejson_writer_generator_init(
        &generator, test_writer_retry_mismatch_producer, &ctx, NULL);
    EXPECT(status == LONEJSON_STATUS_OK);
    status = lonejson_generator_read(&generator, chunk, sizeof(chunk),
                                     &out_len, &eof);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(out_len == sizeof(chunk));
    EXPECT(eof == 0);

    status = lonejson_generator_read(&generator, chunk, sizeof(chunk),
                                     &out_len, &eof);
    EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
    EXPECT(ctx.calls == 2);
    lonejson_generator_cleanup(&generator);
  }
}

static void test_writer_generator_rejects_child_retry_mismatch(void) {
  lonejson_generator generator;
  test_writer_child_retry_mismatch_ctx ctx;
  unsigned char chunk[2];
  size_t out_len;
  int eof;
  lonejson_status status;

  memset(&ctx, 0, sizeof(ctx));
  lonejson_json_value_init(&ctx.first);
  lonejson_json_value_init(&ctx.second);
  EXPECT(lonejson_json_value_set_buffer(
             &ctx.first, "\"abcdefghijklmnopqrstuvwxyz\"", 28u, NULL) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_json_value_set_buffer(
             &ctx.second, "\"ABCDEFGHIJKLMNOPQRSTUVWXYZ\"", 28u, NULL) ==
         LONEJSON_STATUS_OK);
  status = lonejson_writer_generator_init(
      &generator, test_writer_child_retry_mismatch_producer, &ctx, NULL);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(lonejson_generator_read(&generator, chunk, sizeof(chunk), &out_len,
                                 &eof) == LONEJSON_STATUS_OK);
  EXPECT(out_len == sizeof(chunk));
  EXPECT(eof == 0);
  status = lonejson_generator_read(&generator, chunk, sizeof(chunk), &out_len,
                                   &eof);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(ctx.calls == 2);
  lonejson_generator_cleanup(&generator);
  lonejson_json_value_cleanup(&ctx.first);
  lonejson_json_value_cleanup(&ctx.second);

  memset(&ctx, 0, sizeof(ctx));
  ctx.use_mapped = 1;
  strcpy(ctx.first_doc.payload, "abcdefghijklmnopqrstuvwxyz");
  strcpy(ctx.second_doc.payload, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
  status = lonejson_writer_generator_init(
      &generator, test_writer_child_retry_mismatch_producer, &ctx, NULL);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(lonejson_generator_read(&generator, chunk, sizeof(chunk), &out_len,
                                 &eof) == LONEJSON_STATUS_OK);
  EXPECT(out_len == sizeof(chunk));
  EXPECT(eof == 0);
  status = lonejson_generator_read(&generator, chunk, sizeof(chunk), &out_len,
                                   &eof);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(ctx.calls == 2);
  lonejson_generator_cleanup(&generator);
}

static void test_writer_generator_zero_capacity_read(void) {
  lonejson_generator generator;
  test_writer_generator_ctx ctx;
  unsigned char all[64];
  size_t out_len;
  int eof;
  lonejson_status status;

  memset(&ctx, 0, sizeof(ctx));
  status = lonejson_writer_generator_init(&generator,
                                          test_writer_generator_producer, &ctx,
                                          NULL);
  EXPECT(status == LONEJSON_STATUS_OK);
  out_len = 99u;
  eof = 1;
  status = lonejson_generator_read(&generator, NULL, 0u, &out_len, &eof);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(out_len == 0u);
  EXPECT(eof == 0);
  test_writer_generator_read_all(&generator, 2u, all, sizeof(all));
  EXPECT(strcmp((const char *)all, "[true,null,12345]") == 0);
  lonejson_generator_cleanup(&generator);
}

static void test_writer_generator_primitive_scalars_resume(void) {
  static const size_t chunk_sizes[] = {1u, 2u, 3u, 4u, 5u, 8u};
  static const char expected[] =
      "[0,-12,-1234567890,18446744073709551615,-12.5,true,null]";
  size_t i;

  for (i = 0u; i < sizeof(chunk_sizes) / sizeof(chunk_sizes[0]); i++) {
    lonejson_generator generator;
    test_writer_primitive_generator_ctx ctx;
    unsigned char all[160];
    size_t j;

    memset(&ctx, 0, sizeof(ctx));
    EXPECT(lonejson_writer_generator_init(
               &generator, test_writer_primitive_generator_producer, &ctx,
               NULL) == LONEJSON_STATUS_OK);
    test_writer_generator_read_all(&generator, chunk_sizes[i], all,
                                   sizeof(all));
    EXPECT(strcmp((const char *)all, expected) == 0);
    EXPECT(ctx.step == 9);
    if (chunk_sizes[i] == 3u) {
      EXPECT(ctx.calls[2] > 1);
    }
    for (j = 0u; j < sizeof(ctx.calls) / sizeof(ctx.calls[0]); j++) {
      EXPECT(ctx.calls[j] >= 1);
    }
    lonejson_generator_cleanup(&generator);
  }
}

static void test_writer_generator_matches_sink_under_backpressure(void) {
  static const size_t retry_steps[] = {2u,  4u,  8u,  10u, 12u, 18u, 20u,
                                       22u, 24u, 26u, 28u, 31u, 33u};
  static const char text_source_payload[] = "source text line\nwith quote \"";
  static const char b64_source_payload[] = "xyz";
  test_writer_equivalence_init init;
  test_writer_generator_model model;
  char text_source_path[] = "/tmp/lonejson-writer-equivalence-text-XXXXXX";
  char b64_source_path[] = "/tmp/lonejson-writer-equivalence-b64-XXXXXX";

  test_writer_equivalence_write_file(text_source_path, text_source_payload,
                                     sizeof(text_source_payload) - 1u);
  test_writer_equivalence_write_file(b64_source_path, b64_source_payload,
                                     sizeof(b64_source_payload) - 1u);
  init.text_source_path = text_source_path;
  init.b64_source_path = b64_source_path;
  memset(&model, 0, sizeof(model));
  model.name = "mixed writer public API";
  model.ctx_size = sizeof(test_writer_equivalence_ctx);
  model.init = test_writer_equivalence_ctx_init;
  model.cleanup = test_writer_equivalence_ctx_cleanup;
  model.producer = test_writer_equivalence_producer;
  model.calls = test_writer_equivalence_calls;
  model.step = test_writer_equivalence_step;
  model.init_user = &init;
  model.completed_step = 36u;
  model.retry_steps = retry_steps;
  model.retry_step_count = sizeof(retry_steps) / sizeof(retry_steps[0]);
  model.output_capacity = TEST_WRITER_STREAM_PAYLOAD_LEN + 2048u;
  test_writer_generator_model_check(&model);
  unlink(text_source_path);
  unlink(b64_source_path);
}

static void test_writer_child_sink_failures(void) {
  lonejson_json_value value;
  test_failing_sink failing;
  lonejson_writer writer;
  lonejson_error error;

  lonejson_json_value_init(&value);
  EXPECT(lonejson_json_value_set_buffer(
             &value, "\"abcdefghijklmnopqrstuvwxyz\"", 28u, &error) ==
         LONEJSON_STATUS_OK);
  memset(&failing, 0, sizeof(failing));
  failing.fail_after = 4u;
  EXPECT(lonejson_writer_init_sink(&writer, test_failing_sink_write, &failing,
                                   NULL, &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_writer_json_value(&writer, &value, &error) ==
         LONEJSON_STATUS_CALLBACK_FAILED);
  lonejson_writer_cleanup(&writer);
  lonejson_json_value_cleanup(&value);
}

static void test_writer_generator_backpressure_matrix(void) {
  static const size_t patterns[][6] = {
      {1u, 0u, 1u, 1u, 1u, 1u},  {2u, 3u, 1u, 4u, 0u, 5u},
      {7u, 1u, 8u, 2u, 3u, 0u},  {16u, 5u, 1u, 31u, 2u, 4u}};
  static const char raw_payload[] = "{\"k\":[1,true,null]}";
  static const char prefix[] =
      "{\"s\":\"ab\\\"\\\\\\ncd\",\"n\":-12.5e+2,\"src\":\"";
  static const char middle[] = "\",\"raw\":";
  static const char mapped_prefix[] = ",\"mapped\":{\"payload\":\"";
  static const char suffix[] = "\"},\"arr\":[true,null]}";
  unsigned char expected[TEST_WRITER_STREAM_PAYLOAD_LEN + 1024u];
  char source_payload[257];
  size_t expected_len;
  size_t p;
  size_t i;

  for (i = 0u; i < sizeof(source_payload) - 1u; i++) {
    source_payload[i] = (char)('A' + (int)(i % 26u));
  }
  source_payload[sizeof(source_payload) - 1u] = '\0';
  expected_len = 0u;
  memcpy(expected + expected_len, prefix, sizeof(prefix) - 1u);
  expected_len += sizeof(prefix) - 1u;
  memcpy(expected + expected_len, source_payload, sizeof(source_payload) - 1u);
  expected_len += sizeof(source_payload) - 1u;
  memcpy(expected + expected_len, middle, sizeof(middle) - 1u);
  expected_len += sizeof(middle) - 1u;
  memcpy(expected + expected_len, raw_payload, sizeof(raw_payload) - 1u);
  expected_len += sizeof(raw_payload) - 1u;
  memcpy(expected + expected_len, mapped_prefix, sizeof(mapped_prefix) - 1u);
  expected_len += sizeof(mapped_prefix) - 1u;
  for (i = 0u; i < TEST_WRITER_STREAM_PAYLOAD_LEN; i++) {
    expected[expected_len++] = (unsigned char)('a' + (int)(i % 26u));
  }
  memcpy(expected + expected_len, suffix, sizeof(suffix));
  expected_len += sizeof(suffix) - 1u;

  for (p = 0u; p < sizeof(patterns) / sizeof(patterns[0]); p++) {
    lonejson_generator generator;
    test_writer_backpressure_ctx ctx;
    unsigned char all[TEST_WRITER_STREAM_PAYLOAD_LEN + 1024u];
    char path[] = "/tmp/lonejson-writer-matrix-source-XXXXXX";
    size_t all_len;
    int fd;

    memset(&ctx, 0, sizeof(ctx));
    lonejson_source_init(&ctx.source);
    lonejson_json_value_init(&ctx.json_value);
    ctx.json_reader.json = raw_payload;
    ctx.json_reader.offset = 0u;
    ctx.json_reader.chunk_size = 3u;
    EXPECT(lonejson_json_value_set_reader(&ctx.json_value, test_state_reader,
                                          &ctx.json_reader, NULL) ==
           LONEJSON_STATUS_OK);
    for (i = 0u; i < TEST_WRITER_STREAM_PAYLOAD_LEN; i++) {
      ctx.mapped.payload[i] = (char)('a' + (int)(i % 26u));
    }
    ctx.mapped.payload[TEST_WRITER_STREAM_PAYLOAD_LEN] = '\0';

    fd = mkstemp(path);
    EXPECT(fd >= 0);
    if (fd < 0) {
      lonejson_json_value_cleanup(&ctx.json_value);
      continue;
    }
    EXPECT(write(fd, source_payload, sizeof(source_payload) - 1u) ==
           (ssize_t)(sizeof(source_payload) - 1u));
    EXPECT(lseek(fd, 0, SEEK_SET) == 0);
    EXPECT(lonejson_source_set_fd(&ctx.source, fd, NULL) ==
           LONEJSON_STATUS_OK);

    EXPECT(lonejson_writer_generator_init(
               &generator, test_writer_backpressure_producer, &ctx, NULL) ==
           LONEJSON_STATUS_OK);
    all_len = test_writer_generator_read_all_pattern(
        &generator, patterns[p], sizeof(patterns[p]) / sizeof(patterns[p][0]),
        all, sizeof(all));
    EXPECT(all_len == expected_len);
    EXPECT(strcmp((const char *)all, (const char *)expected) == 0);
    EXPECT(ctx.step == 17);
    for (i = 0u; i < sizeof(ctx.calls) / sizeof(ctx.calls[0]); i++) {
      EXPECT(ctx.calls[i] >= 1);
    }
    EXPECT(ctx.calls[6] > 1);
    EXPECT(ctx.calls[8] > 1);
    EXPECT(ctx.calls[10] > 1);
    lonejson_generator_cleanup(&generator);
    lonejson_source_cleanup(&ctx.source);
    lonejson_json_value_cleanup(&ctx.json_value);
    close(fd);
    unlink(path);
  }
}

static void test_writer_generator_streams_source_without_materializing(void) {
  lonejson_generator generator;
  test_writer_source_generator_ctx ctx;
  test_realloc_limit_state realloc_limit;
  lonejson_allocator allocator;
  lonejson_write_options options;
  unsigned char all[TEST_WRITER_STREAM_PAYLOAD_LEN + 8u];
  unsigned char expected[TEST_WRITER_STREAM_PAYLOAD_LEN + 8u];
  size_t all_len;
  char path[] = "/tmp/lonejson-writer-generator-source-XXXXXX";
  int fd;
  size_t i;

  memset(&ctx, 0, sizeof(ctx));
  lonejson_source_init(&ctx.source);
  fd = mkstemp(path);
  EXPECT(fd >= 0);
  if (fd < 0) {
    return;
  }
  for (i = 0u; i < TEST_WRITER_STREAM_PAYLOAD_LEN; i++) {
    char ch = (char)('a' + (int)(i % 26u));
    EXPECT(write(fd, &ch, 1u) == 1);
  }
  EXPECT(lseek(fd, 0, SEEK_SET) == 0);
  EXPECT(lonejson_source_set_fd(&ctx.source, fd, NULL) == LONEJSON_STATUS_OK);

  memset(&realloc_limit, 0, sizeof(realloc_limit));
  realloc_limit.max_realloc_size = 65536u;
  allocator = lonejson_default_allocator();
  allocator.malloc_fn = test_realloc_limit_malloc;
  allocator.realloc_fn = test_realloc_limit_realloc;
  allocator.free_fn = test_realloc_limit_free;
  allocator.ctx = &realloc_limit;
  options = lonejson_default_write_options();
  options.allocator = &allocator;

  EXPECT(lonejson_writer_generator_init(
             &generator, test_writer_source_generator_producer, &ctx,
             &options) == LONEJSON_STATUS_OK);
  realloc_limit.observed_max_realloc_size = 0u;
  all_len = test_writer_generator_read_all(&generator, 7u, all, sizeof(all));
  expected[0] = '[';
  expected[1] = '"';
  for (i = 0u; i < TEST_WRITER_STREAM_PAYLOAD_LEN; i++) {
    expected[2u + i] = (unsigned char)('a' + (int)(i % 26u));
  }
  expected[2u + TEST_WRITER_STREAM_PAYLOAD_LEN] = '"';
  expected[3u + TEST_WRITER_STREAM_PAYLOAD_LEN] = ']';
  expected[4u + TEST_WRITER_STREAM_PAYLOAD_LEN] = '\0';
  EXPECT(all_len == TEST_WRITER_STREAM_PAYLOAD_LEN + 4u);
  EXPECT(strcmp((const char *)all, (const char *)expected) == 0);
  EXPECT(realloc_limit.observed_max_realloc_size < 12288u);
  EXPECT(ctx.step == 3);
  EXPECT(ctx.source_calls > 1);
  lonejson_generator_cleanup(&generator);
  lonejson_source_cleanup(&ctx.source);
  close(fd);
  unlink(path);
}

static void
test_writer_generator_streams_json_value_without_materializing(void) {
  lonejson_generator generator;
  test_writer_json_value_generator_ctx ctx;
  test_realloc_limit_state realloc_limit;
  lonejson_allocator allocator;
  lonejson_write_options options;
  unsigned char json[TEST_WRITER_STREAM_PAYLOAD_LEN + 4u];
  unsigned char expected[TEST_WRITER_STREAM_PAYLOAD_LEN + 8u];
  unsigned char all[TEST_WRITER_STREAM_PAYLOAD_LEN + 8u];
  size_t all_len;
  size_t i;

  memset(&ctx, 0, sizeof(ctx));
  lonejson_json_value_init(&ctx.value);
  json[0] = '"';
  expected[0] = '[';
  expected[1] = '"';
  for (i = 0u; i < TEST_WRITER_STREAM_PAYLOAD_LEN; i++) {
    json[1u + i] = (unsigned char)('a' + (int)(i % 26u));
    expected[2u + i] = json[1u + i];
  }
  json[1u + TEST_WRITER_STREAM_PAYLOAD_LEN] = '"';
  expected[2u + TEST_WRITER_STREAM_PAYLOAD_LEN] = '"';
  expected[3u + TEST_WRITER_STREAM_PAYLOAD_LEN] = ']';
  expected[4u + TEST_WRITER_STREAM_PAYLOAD_LEN] = '\0';
  EXPECT(lonejson_json_value_set_buffer(
             &ctx.value, (const char *)json,
             TEST_WRITER_STREAM_PAYLOAD_LEN + 2u, NULL) ==
         LONEJSON_STATUS_OK);

  memset(&realloc_limit, 0, sizeof(realloc_limit));
  realloc_limit.max_realloc_size = 65536u;
  allocator = lonejson_default_allocator();
  allocator.malloc_fn = test_realloc_limit_malloc;
  allocator.realloc_fn = test_realloc_limit_realloc;
  allocator.free_fn = test_realloc_limit_free;
  allocator.ctx = &realloc_limit;
  options = lonejson_default_write_options();
  options.allocator = &allocator;

  EXPECT(lonejson_writer_generator_init(
             &generator, test_writer_json_value_generator_producer, &ctx,
             &options) == LONEJSON_STATUS_OK);
  realloc_limit.observed_max_realloc_size = 0u;
  all_len = test_writer_generator_read_all(&generator, 5u, all, sizeof(all));
  EXPECT(all_len == TEST_WRITER_STREAM_PAYLOAD_LEN + 4u);
  EXPECT(strcmp((const char *)all, (const char *)expected) == 0);
  EXPECT(realloc_limit.observed_max_realloc_size < 12288u);
  EXPECT(ctx.step == 3);
  EXPECT(ctx.value_calls > 1);
  lonejson_generator_cleanup(&generator);
  lonejson_json_value_cleanup(&ctx.value);
}

static void test_writer_generator_streams_mapped_without_materializing(void) {
  static const char prefix[] = "[{\"payload\":\"";
  static const char suffix[] = "\"}]";
  lonejson_generator generator;
  test_writer_mapped_generator_ctx ctx;
  test_realloc_limit_state realloc_limit;
  lonejson_allocator allocator;
  lonejson_write_options options;
  unsigned char expected[TEST_WRITER_STREAM_PAYLOAD_LEN + sizeof(prefix) +
                         sizeof(suffix)];
  unsigned char all[TEST_WRITER_STREAM_PAYLOAD_LEN + sizeof(prefix) +
                    sizeof(suffix)];
  size_t all_len;
  size_t i;

  memset(&ctx, 0, sizeof(ctx));
  for (i = 0u; i < TEST_WRITER_STREAM_PAYLOAD_LEN; i++) {
    ctx.doc.payload[i] = (char)('a' + (int)(i % 26u));
  }
  ctx.doc.payload[TEST_WRITER_STREAM_PAYLOAD_LEN] = '\0';

  memcpy(expected, prefix, sizeof(prefix) - 1u);
  memcpy(expected + sizeof(prefix) - 1u, ctx.doc.payload,
         TEST_WRITER_STREAM_PAYLOAD_LEN);
  memcpy(expected + sizeof(prefix) - 1u + TEST_WRITER_STREAM_PAYLOAD_LEN,
         suffix, sizeof(suffix));

  memset(&realloc_limit, 0, sizeof(realloc_limit));
  realloc_limit.max_realloc_size = 65536u;
  allocator = lonejson_default_allocator();
  allocator.malloc_fn = test_realloc_limit_malloc;
  allocator.realloc_fn = test_realloc_limit_realloc;
  allocator.free_fn = test_realloc_limit_free;
  allocator.ctx = &realloc_limit;
  options = lonejson_default_write_options();
  options.allocator = &allocator;

  EXPECT(lonejson_writer_generator_init(
             &generator, test_writer_mapped_generator_producer, &ctx,
             &options) == LONEJSON_STATUS_OK);
  realloc_limit.observed_max_realloc_size = 0u;
  all_len = test_writer_generator_read_all(&generator, 6u, all, sizeof(all));
  EXPECT(all_len == strlen((const char *)expected));
  EXPECT(strcmp((const char *)all, (const char *)expected) == 0);
  EXPECT(realloc_limit.observed_max_realloc_size < 12288u);
  EXPECT(ctx.step == 3);
  EXPECT(ctx.mapped_calls > 1);
  lonejson_generator_cleanup(&generator);
}

static void test_writer_generator_string_reader_preserves_chunks(void) {
  lonejson_generator generator;
  test_writer_string_reader_generator_ctx ctx;
  unsigned char all[128];
  lonejson_status status;

  memset(&ctx, 0, sizeof(ctx));
  ctx.reader.json = "ab\"\\\ncd";
  ctx.reader.offset = 0u;
  ctx.reader.chunk_size = 2u;
  status = lonejson_writer_generator_init(
      &generator, test_writer_string_reader_generator_producer, &ctx, NULL);
  EXPECT(status == LONEJSON_STATUS_OK);
  test_writer_generator_read_all(&generator, 3u, all, sizeof(all));
  EXPECT(strcmp((const char *)all, "[\"ab\\\"\\\\\\ncd\",true]") == 0);
  EXPECT(ctx.reader.offset == strlen(ctx.reader.json));
  lonejson_generator_cleanup(&generator);
}
