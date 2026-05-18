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
  lonejson_json_value_init(&replacement);
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
  status = lonejson_value_rewrite_reader(
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
  status = lonejson_value_rewrite_reader(
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
  status = lonejson_value_rewrite_reader(
      test_state_reader, &reader, test_buffer_sink_write, &sink, &options,
      &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)out, "{\"ok\":true}") == 0);
  lonejson_json_value_cleanup(&replacement);
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
  status = lonejson_value_rewrite_reader(
      test_state_reader, &reader, test_buffer_sink_write, &sink, &options,
      &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)out,
                "{\"payload\":{\"label\":\"generated\",\"values\":[1,true]},"
                "\"tail\":false}") == 0);
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
  status = lonejson_value_rewrite_selector_reader(
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
  lonejson_json_value_init(&replacement);
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
  status = lonejson_value_rewrite_reader(
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
  status = lonejson_value_rewrite_reader(
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
  EXPECT(lonejson_value_rewrite_reader(NULL, NULL, test_buffer_sink_write,
                                       &sink, &options, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);

  EXPECT(lonejson_value_rewrite_selector_reader(
             test_state_reader, &reader, test_buffer_sink_write, &sink, NULL,
             &error) == LONEJSON_STATUS_INVALID_ARGUMENT);

  options.action = LONEJSON_VALUE_REWRITE_DROP;
  reader.json = "true";
  reader.offset = 0u;
  reader.chunk_size = 8u;
  EXPECT(lonejson_value_rewrite_reader(
             test_state_reader, &reader, test_buffer_sink_write, &sink,
             &options, &error) == LONEJSON_STATUS_INVALID_ARGUMENT);

  memset(&options, 0, sizeof(options));
  options.action = LONEJSON_VALUE_REWRITE_REPLACE;
  options.replacement.emit = test_value_rewrite_emit_dynamic;
  options.replacement.json = (const lonejson_json_value *)1;
  EXPECT(lonejson_value_rewrite_reader(test_state_reader, &reader,
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
  status = lonejson_value_rewrite_reader(
      test_state_reader, &reader, test_buffer_sink_write, &sink, &options,
      &error);
  EXPECT(status == LONEJSON_STATUS_TYPE_MISMATCH);
}
