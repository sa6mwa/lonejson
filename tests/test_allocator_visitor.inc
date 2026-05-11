static void test_value_visitor_success_and_limits(void) {
  const char *json = "{\"name\":\"a\\n\\u20ac\",\"items\":[1,-2.5e3,true,false,"
                     "null,{\"x\":[]}]}";
  lonejson_value_visitor visitor = test_value_visitor();
  lonejson_value_limits limits = lonejson_default_value_limits();
  lonejson_error error;
  lonejson_status status;
  test_visit_state state;
  test_reader_state reader;
  char path[] = "/tmp/lonejson-value-visitor-XXXXXX";
  int fd;
  FILE *fp;

  memset(&state, 0, sizeof(state));
  status = lonejson_visit_value_cstr(json, &visitor, &state, &limits, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strstr(state.log, "{K(name)S(a\n") != NULL);
  EXPECT(strstr(state.log, "K(items)[N(1)N(-2.5e3)TFZ{K(x)[]}]") != NULL);

  memset(&state, 0, sizeof(state));
  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 2u;
  status = lonejson_visit_value_reader(test_state_reader, &reader, &visitor,
                                       &state, &limits, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(
      strcmp(state.log,
             "{K(name)S(a\n\xE2\x82\xAC)K(items)[N(1)N(-2.5e3)TFZ{K(x)[]}]}") ==
      0);

  strcpy(path, "/tmp/lonejson-value-visitor-XXXXXX");
  fd = write_temp_text_file(path, json);
  EXPECT(fd >= 0);
  if (fd >= 0) {
    close(fd);
    memset(&state, 0, sizeof(state));
    status = lonejson_visit_value_path(path, &visitor, &state, &limits, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    unlink(path);
  }

  limits.max_depth = 2u;
  status = lonejson_visit_value_cstr(json, &visitor, &state, &limits, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);

  limits = lonejson_default_value_limits();
  limits.max_key_bytes = 3u;
  status = lonejson_visit_value_cstr(json, &visitor, &state, &limits, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);

  limits = lonejson_default_value_limits();
  limits.max_string_bytes = 2u;
  status = lonejson_visit_value_cstr("{\"k\":\"abcd\"}", &visitor, &state,
                                     &limits, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);

  limits = lonejson_default_value_limits();
  limits.max_number_bytes = 3u;
  status =
      lonejson_visit_value_cstr("12345", &visitor, &state, &limits, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);

  limits = lonejson_default_value_limits();
  limits.max_total_bytes = 5u;
  status =
      lonejson_visit_value_cstr("{\"k\":1}", &visitor, &state, &limits, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);

  status = lonejson_visit_value_cstr("{\"k\":1} trailing", &visitor, &state,
                                     NULL, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);

  status =
      lonejson_visit_value_cstr("{\"k\":tru}", &visitor, &state, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);

  strcpy(path, "/tmp/lonejson-value-visitor-XXXXXX");
  fd = write_temp_text_file(path, json);
  EXPECT(fd >= 0);
  if (fd >= 0) {
    fp = fdopen(fd, "rb");
    EXPECT(fp != NULL);
    if (fp != NULL) {
      EXPECT(fseek(fp, 0L, SEEK_SET) == 0);
      memset(&state, 0, sizeof(state));
      status = lonejson_visit_value_filep(fp, &visitor, &state, NULL, &error);
      EXPECT(status == LONEJSON_STATUS_OK);
      fclose(fp);
    } else {
      close(fd);
    }
    unlink(path);
  }

  strcpy(path, "/tmp/lonejson-value-visitor-XXXXXX");
  fd = write_temp_text_file(path, json);
  EXPECT(fd >= 0);
  if (fd >= 0) {
    EXPECT(lseek(fd, 0, SEEK_SET) == 0);
    memset(&state, 0, sizeof(state));
    status = lonejson_visit_value_fd(fd, &visitor, &state, NULL, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    close(fd);
    unlink(path);
  }
}

static void test_value_visitor_chunking_unicode_and_failures(void) {
  const char *json =
      "{\"\\u20acuro_key\":\"line\\n\\uD834\\uDD1E\","
      "\"items\":[\"alpha\",\"beta\",-12345.678e-9,true,false,null]}";
  static const char *invalid_cases[] = {
      "{\"bad\":\"\\uD834x\"}", "{\"bad\":\"\\uD834\\u0000\"}",
      "{\"bad\":\"\\uDD1E\"}",  "{\"bad\":\"\\u12\"}",
      "{\"bad\":\"\\x41\"}",    "{\"bad\":[1,2,}",
      "{\"bad\":1} trailing"};
  lonejson_value_visitor visitor = test_counting_value_visitor();
  lonejson_error error;
  lonejson_status status;
  test_visit_chunk_state state;
  test_reader_state reader;
  lonejson_value_limits limits;
  size_t i;

  memset(&state, 0, sizeof(state));
  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 1u;
  status = lonejson_visit_value_reader(test_state_reader, &reader, &visitor,
                                       &state, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.object_count == 1u);
  EXPECT(state.array_count == 1u);
  EXPECT(state.key_begin_count == 2u);
  EXPECT(state.key_chunk_count > state.key_begin_count);
  EXPECT(state.string_begin_count == 3u);
  EXPECT(state.string_chunk_count > state.string_begin_count);
  EXPECT(state.number_begin_count == 1u);
  EXPECT(state.number_chunk_count >= 1u);
  EXPECT(state.bool_count == 2u);
  EXPECT(state.null_count == 1u);
  EXPECT(strcmp(state.last_number, "-12345.678e-9") == 0);

  memset(&state, 0, sizeof(state));
  limits = lonejson_default_value_limits();
  limits.max_total_bytes = strlen(json);
  status = lonejson_visit_value_cstr(json, &visitor, &state, &limits, &error);
  EXPECT(status == LONEJSON_STATUS_OK);

  memset(&state, 0, sizeof(state));
  state.fail_after = 6u;
  status = lonejson_visit_value_cstr(json, &visitor, &state, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(state.callback_count == 6u);
  EXPECT(state.key_begin_count > 0u);

  status = lonejson_visit_value_cstr(json, NULL, &state, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);

  memset(&state, 0, sizeof(state));
  status = lonejson_visit_value_cstr(json, &visitor, &state, NULL, NULL);
  EXPECT(status == LONEJSON_STATUS_OK);

  for (i = 0u; i < sizeof(invalid_cases) / sizeof(invalid_cases[0]); ++i) {
    memset(&state, 0, sizeof(state));
    status = lonejson_visit_value_cstr(invalid_cases[i], &visitor, &state, NULL,
                                       &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
  }
}

static void test_custom_allocator_parse_cleanup_and_stream(void) {
  static const char json[] =
      "{\"name\":\"alpha\",\"body\":\"hello world\",\"tags\":[\"x\",\"y\"]}";
  test_allocator_state alloc;
  lonejson_parse_options options = lonejson_default_parse_options();
  lonejson_error error;
  lonejson_status status;
  test_alloc_parse_doc doc;
  lonejson_stream *stream;
  test_reader_state reader;

  test_allocator_init(&alloc);
  memset(&doc, 0, sizeof(doc));
  options.allocator = &alloc.allocator;
  status = lonejson_parse_cstr(&test_alloc_parse_doc_map, &doc, json, &options,
                               &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(alloc.stats.alloc_calls > 0u);
  EXPECT(alloc.stats.bytes_live > 0u);
  lonejson_cleanup(&test_alloc_parse_doc_map, &doc);
  EXPECT(alloc.stats.bytes_live == 0u);
  EXPECT(alloc.stats.free_calls > 0u);

  test_allocator_init(&alloc);
  options = lonejson_default_parse_options();
  options.allocator = &alloc.allocator;
  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 7u;
  stream = lonejson_stream_open_reader(
      &test_alloc_parse_doc_map, test_state_reader, &reader, &options, &error);
  EXPECT(stream != NULL);
  EXPECT(alloc.stats.alloc_calls > 0u);
  if (stream != NULL) {
    memset(&doc, 0, sizeof(doc));
    EXPECT(lonejson_stream_next(stream, &doc, &error) ==
           LONEJSON_STREAM_OBJECT);
    lonejson_cleanup(&test_alloc_parse_doc_map, &doc);
    lonejson_stream_close(stream);
  }
  EXPECT(alloc.stats.bytes_live == 0u);
}

#ifndef NDEBUG
static void test_custom_allocator_misaligned_owned_alloc_is_rejected(void) {
  static const char json[] = "{\"name\":\"alpha\"}";
  lonejson_parse_options options = lonejson_default_parse_options();
  lonejson_allocator allocator = lonejson_default_allocator();
  lonejson_error error;
  lonejson_status status;
  test_alloc_parse_doc doc;

  memset(&doc, 0, sizeof(doc));
  allocator.malloc_fn = test_misaligned_allocator_malloc;
  allocator.realloc_fn = test_misaligned_allocator_realloc;
  allocator.free_fn = test_misaligned_allocator_free;
  options.allocator = &allocator;
  status = lonejson_parse_cstr(&test_alloc_parse_doc_map, &doc, json, &options,
                               &error);
  EXPECT(status == LONEJSON_STATUS_ALLOCATION_FAILED);
  EXPECT(error.code == LONEJSON_STATUS_ALLOCATION_FAILED);
  EXPECT(strstr(error.message, "failed to allocate string") != NULL);
  lonejson_cleanup(&test_alloc_parse_doc_map, &doc);
}
#endif

static void test_custom_allocator_json_value_capture_and_serialize_alloc(void) {
  static const char json[] =
      "{\"id\":\"q1\",\"selector\":{\"a\":[1,true,null]}}";
  test_allocator_state parse_alloc;
  test_allocator_state write_alloc;
  lonejson_parse_options parse_options = lonejson_default_parse_options();
  lonejson_write_options write_options = lonejson_default_write_options();
  lonejson_error error;
  lonejson_status status;
  test_alloc_json_value_doc doc;
  test_event event;
  lonejson_owned_buffer serialized;

  test_allocator_init(&parse_alloc);
  test_allocator_init(&write_alloc);
  lonejson_json_value_init_with_allocator(&doc.selector,
                                          &parse_alloc.allocator);
  status = lonejson_json_value_enable_parse_capture(&doc.selector, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  memset(doc.id, 0, sizeof(doc.id));
  parse_options.clear_destination = 0;
  parse_options.allocator = &parse_alloc.allocator;
  status = lonejson_parse_cstr(&test_alloc_json_value_doc_map, &doc, json,
                               &parse_options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(parse_alloc.stats.alloc_calls > 0u);
  EXPECT(parse_alloc.stats.bytes_live > 0u);
  EXPECT(strcmp(doc.selector.json, "{\"a\":[1,true,null]}") == 0);
  lonejson_cleanup(&test_alloc_json_value_doc_map, &doc);
  EXPECT(parse_alloc.stats.bytes_live == 0u);

  write_options.allocator = &write_alloc.allocator;
  memset(&event, 0, sizeof(event));
  memcpy(event.id, "evt-1", 6u);
  event.ok = true;
  lonejson_owned_buffer_init(&serialized);
  status = lonejson_serialize_owned(&test_event_map, &event, &serialized,
                                    &write_options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(serialized.data != NULL);
  EXPECT(write_alloc.stats.alloc_calls > 0u);
  EXPECT(write_alloc.stats.bytes_live > 0u);
  if (serialized.data != NULL) {
    EXPECT(strcmp(serialized.data, "{\"id\":\"evt-1\",\"ok\":true}") == 0);
    lonejson_owned_buffer_free(&serialized);
  }
  EXPECT(write_alloc.stats.bytes_live == 0u);
}

static void test_custom_allocator_raw_serialize_alloc_is_rejected(void) {
  test_allocator_state write_alloc;
  lonejson_write_options write_options = lonejson_default_write_options();
  lonejson_error error;
  test_event event;
  char *serialized;

  test_allocator_init(&write_alloc);
  write_options.allocator = &write_alloc.allocator;
  memset(&event, 0, sizeof(event));
  memcpy(event.id, "evt-raw", 8u);
  event.ok = true;
  serialized = lonejson_serialize_alloc(&test_event_map, &event, NULL,
                                        &write_options, &error);
  EXPECT(serialized == NULL);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(strstr(error.message,
                "serialize_alloc uses the default allocator only") != NULL);
  EXPECT(write_alloc.stats.alloc_calls == 0u);
  EXPECT(write_alloc.stats.realloc_calls == 0u);
  EXPECT(write_alloc.stats.free_calls == 0u);
  EXPECT(write_alloc.stats.bytes_live == 0u);
}

static void
test_explicit_default_allocator_raw_serialize_alloc_is_allowed(void) {
  lonejson_write_options options = lonejson_default_write_options();
  lonejson_allocator allocator = lonejson_default_allocator();
  lonejson_error error;
  test_event event;
  char *serialized;

  options.allocator = &allocator;
  memset(&event, 0, sizeof(event));
  memcpy(event.id, "evt-def", 8u);
  event.ok = true;
  serialized =
      lonejson_serialize_alloc(&test_event_map, &event, NULL, &options, &error);
  EXPECT(serialized != NULL);
  if (serialized != NULL) {
    EXPECT(strcmp(serialized, "{\"id\":\"evt-def\",\"ok\":true}") == 0);
    LONEJSON_FREE(serialized);
  }
}

static void
test_explicit_default_allocator_raw_serialize_jsonl_alloc_is_allowed(void) {
  lonejson_write_options options = lonejson_default_write_options();
  lonejson_allocator allocator = lonejson_default_allocator();
  lonejson_error error;
  test_event events[2];
  char *serialized;

  options.allocator = &allocator;
  memset(events, 0, sizeof(events));
  memcpy(events[0].id, "evt-1", 6u);
  events[0].ok = true;
  memcpy(events[1].id, "evt-2", 6u);
  events[1].ok = false;
  serialized = lonejson_serialize_jsonl_alloc(&test_event_map, events, 2u, 0u,
                                              NULL, &options, &error);
  EXPECT(serialized != NULL);
  if (serialized != NULL) {
    EXPECT(strcmp(serialized, "{\"id\":\"evt-1\",\"ok\":true}\n"
                              "{\"id\":\"evt-2\",\"ok\":false}\n") == 0);
    LONEJSON_FREE(serialized);
  }
}

static void
test_custom_allocator_json_value_capture_preserves_handle_allocator(void) {
  static const char json[] = "{\"id\":\"q2\",\"selector\":{\"k\":[1,2,3]}}";
  test_allocator_state parse_alloc;
  lonejson_parse_options parse_options = lonejson_default_parse_options();
  lonejson_error error;
  lonejson_status status;
  test_alloc_json_value_doc doc;

  memset(&doc, 0, sizeof(doc));
  test_allocator_init(&parse_alloc);
  lonejson_json_value_init_with_allocator(&doc.selector,
                                          &parse_alloc.allocator);
  status = lonejson_json_value_enable_parse_capture(&doc.selector, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  parse_options.clear_destination = 0;
  parse_options.allocator = NULL;
  status = lonejson_parse_cstr(&test_alloc_json_value_doc_map, &doc, json,
                               &parse_options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(parse_alloc.stats.alloc_calls > 0u);
  EXPECT(parse_alloc.stats.bytes_live > 0u);
  EXPECT(strcmp(doc.selector.json, "{\"k\":[1,2,3]}") == 0);
  lonejson_cleanup(&test_alloc_json_value_doc_map, &doc);
  EXPECT(parse_alloc.stats.bytes_live == 0u);
}

static void test_partial_allocator_parse_is_rejected(void) {
  static const char json[] = "{\"name\":\"alpha\",\"body\":\"hello\"}";
  test_allocator_state alloc;
  lonejson_parse_options options = lonejson_default_parse_options();
  lonejson_error error;
  lonejson_status status;
  test_alloc_parse_doc doc;

  memset(&doc, 0, sizeof(doc));
  test_allocator_init(&alloc);
  alloc.allocator.realloc_fn = NULL;
  alloc.allocator.free_fn = NULL;
  options.allocator = &alloc.allocator;
  status = lonejson_parse_cstr(&test_alloc_parse_doc_map, &doc, json, &options,
                               &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(strstr(error.message,
                "allocator must provide either all callbacks or none") != NULL);
  EXPECT(alloc.stats.alloc_calls == 0u);
  EXPECT(alloc.stats.realloc_calls == 0u);
  EXPECT(alloc.stats.free_calls == 0u);
  EXPECT(alloc.stats.bytes_live == 0u);
}

static void test_partial_allocator_parse_reader_is_rejected(void) {
  static const char json[] = "{\"name\":\"alpha\",\"body\":\"hello\"}";
  test_allocator_state alloc;
  lonejson_parse_options options = lonejson_default_parse_options();
  lonejson_error error;
  lonejson_status status;
  test_alloc_parse_doc doc;
  test_reader_state reader;

  memset(&doc, 0, sizeof(doc));
  test_allocator_init(&alloc);
  alloc.allocator.realloc_fn = NULL;
  alloc.allocator.free_fn = NULL;
  options.allocator = &alloc.allocator;
  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 5u;
  status = lonejson_parse_reader(&test_alloc_parse_doc_map, &doc,
                                 test_state_reader, &reader, &options, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(strstr(error.message,
                "allocator must provide either all callbacks or none") != NULL);
  EXPECT(alloc.stats.alloc_calls == 0u);
  EXPECT(alloc.stats.realloc_calls == 0u);
  EXPECT(alloc.stats.free_calls == 0u);
  EXPECT(alloc.stats.bytes_live == 0u);
}

static void test_partial_allocator_serialize_alloc_is_rejected(void) {
  test_allocator_state alloc;
  lonejson_write_options options = lonejson_default_write_options();
  lonejson_error error;
  test_event event;
  char *serialized;

  memset(&event, 0, sizeof(event));
  memcpy(event.id, "evt-2", 6u);
  event.ok = true;
  test_allocator_init(&alloc);
  alloc.allocator.realloc_fn = NULL;
  alloc.allocator.free_fn = NULL;
  options.allocator = &alloc.allocator;
  serialized =
      lonejson_serialize_alloc(&test_event_map, &event, NULL, &options, &error);
  EXPECT(serialized == NULL);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(strstr(error.message,
                "allocator must provide either all callbacks or none") != NULL);
  EXPECT(alloc.stats.alloc_calls == 0u);
  EXPECT(alloc.stats.realloc_calls == 0u);
  EXPECT(alloc.stats.free_calls == 0u);
  EXPECT(alloc.stats.bytes_live == 0u);
}

static void test_partial_allocator_generator_init_is_rejected(void) {
  test_allocator_state alloc;
  lonejson_write_options options = lonejson_default_write_options();
  lonejson_generator generator;
  test_event event;
  lonejson_status status;

  memset(&event, 0, sizeof(event));
  memcpy(event.id, "evt-3", 6u);
  event.ok = true;
  test_allocator_init(&alloc);
  alloc.allocator.realloc_fn = NULL;
  alloc.allocator.free_fn = NULL;
  options.allocator = &alloc.allocator;
  status =
      lonejson_generator_init(&generator, &test_event_map, &event, &options);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(generator.state == NULL);
  EXPECT(generator.error.code == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(strstr(generator.error.message,
                "allocator must provide either all callbacks or none") != NULL);
  EXPECT(alloc.stats.alloc_calls == 0u);
  EXPECT(alloc.stats.realloc_calls == 0u);
  EXPECT(alloc.stats.free_calls == 0u);
  EXPECT(alloc.stats.bytes_live == 0u);
}

