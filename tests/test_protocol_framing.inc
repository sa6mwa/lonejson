static void test_sse_incremental_events_and_json_selection(void) {
  const char *chunks[] = {
      "id: abc\r",     "event: update\r\n", ": ignored\n", "retry: 1500\n",
      "data: first\r", "data: second\n",    "\n"};
  const char *json_chunks[] = {
      "event: keep\nid: j1\ndata: {\"id\":\"evtjsn\",\"ok\":true}\n\n",
      "event: skip\ndata: {\"id\":\"bad\",\"ok\":false}\n\n"};
  const char *selected[] = {"keep"};
  lonejson_sse_json_options json_options;
  test_sse_state state;
  test_sse_json_state json_state;
  lonejson_sse_handler handler;
  lonejson_sse *sse;
  test_event event;
  lonejson_error error;
  lonejson_status status;
  size_t i;

  memset(&state, 0, sizeof(state));
  handler = test_sse_handler();
  sse = lonejson_sse_open(NULL, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    return;
  }
  for (i = 0u; i < sizeof(chunks) / sizeof(chunks[0]); ++i) {
    status = lonejson_sse_push(sse, chunks[i], strlen(chunks[i]), &handler,
                               &state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
  }
  status = lonejson_sse_finish(sse, &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.count == 1u);
  EXPECT(state.begin_count == 1u);
  EXPECT(state.data_count == 3u);
  EXPECT(strcmp(state.event, "update") == 0);
  EXPECT(strcmp(state.id, "abc") == 0);
  EXPECT(strcmp(state.data, "first\nsecond") == 0);
  EXPECT(state.data_len == strlen("first\nsecond"));
  EXPECT(state.has_retry);
  EXPECT(state.retry_ms == 1500u);
  lonejson_sse_close(sse);

  memset(&json_state, 0, sizeof(json_state));
  memset(&event, 0, sizeof(event));
  json_options.event_names = selected;
  json_options.event_name_count = 1u;
  json_options.parse_options = NULL;
  sse = lonejson_sse_open(NULL, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    return;
  }
  for (i = 0u; i < sizeof(json_chunks) / sizeof(json_chunks[0]); ++i) {
    status = lonejson_sse_push_json(
        sse, &test_event_map, &event, json_chunks[i], strlen(json_chunks[i]),
        &json_options, test_sse_json_event_cb, &json_state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
  }
  status =
      lonejson_sse_finish_json(sse, &test_event_map, &event, &json_options,
                               test_sse_json_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(json_state.count == 1u);
  EXPECT(json_state.invalid == 0);
  EXPECT(strcmp(json_state.event, "keep") == 0);
  EXPECT(strcmp(json_state.id, "j1") == 0);
  EXPECT(json_state.data_len == strlen("{\"id\":\"evtjsn\",\"ok\":true}"));
  lonejson_sse_close(sse);
}

static void test_sse_json_streams_data_without_event_buffering(void) {
  static const char chunk0[] = "event: keep\nid: j2\ndata: {\"id\":\"";
  static const char chunk1[] = "evtjsn\",\n";
  static const char chunk2[] = "data: \"ok\":true}\n\n";
  static const char late_event[] =
      "data: {\"id\":\"evtjsn\",\"ok\":true}\nevent: keep\n\n";
  const char *empty_selected[] = {""};
  const char *selected[] = {"keep"};
  lonejson_sse_options sse_options;
  lonejson_sse_json_options json_options;
  test_sse_json_state json_state;
  test_event event;
  lonejson_sse *sse;
  lonejson_error error;
  lonejson_status status;

  json_options.event_names = selected;
  json_options.event_name_count = 1u;
  json_options.parse_options = NULL;
  sse_options = lonejson_default_sse_options();
  sse_options.max_event_data_bytes = 64u;

  memset(&json_state, 0, sizeof(json_state));
  memset(&event, 0, sizeof(event));
  sse = lonejson_sse_open(&sse_options, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    return;
  }
  status = lonejson_sse_push_json(sse, &test_event_map, &event, chunk0,
                                  sizeof(chunk0) - 1u, &json_options,
                                  test_sse_json_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_sse_push_json(sse, &test_event_map, &event, chunk1,
                                  sizeof(chunk1) - 1u, &json_options,
                                  test_sse_json_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_sse_push_json(sse, &test_event_map, &event, chunk2,
                                  sizeof(chunk2) - 1u, &json_options,
                                  test_sse_json_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status =
      lonejson_sse_finish_json(sse, &test_event_map, &event, &json_options,
                               test_sse_json_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(json_state.count == 1u);
  EXPECT(json_state.invalid == 0);
  EXPECT(strcmp(json_state.event, "keep") == 0);
  EXPECT(strcmp(json_state.id, "j2") == 0);
  EXPECT(json_state.data_len == strlen("{\"id\":\"evtjsn\",\n\"ok\":true}"));
  lonejson_sse_close(sse);

  memset(&json_state, 0, sizeof(json_state));
  memset(&event, 0, sizeof(event));
  json_options.event_names = empty_selected;
  sse = lonejson_sse_open(NULL, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    return;
  }
  status = lonejson_sse_push_json(sse, &test_event_map, &event, late_event,
                                  sizeof(late_event) - 1u, &json_options,
                                  test_sse_json_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  lonejson_sse_close(sse);
}

static void test_sse_json_event_limits_and_boundary_errors(void) {
  static const char header[] = "event: keep\nid: ok\ndata: ";
  static const char payload[] = "{\"id\":\"evtjsn\",\"ok\":true}";
  static const char line_terminator[] = "\n";
  static const char event_terminator[] = "\n\n";
  static const char split_line[] = "data:\n";
  const char *selected[] = {"keep"};
  lonejson_sse_options sse_options;
  lonejson_sse_json_options json_options;
  test_sse_json_state json_state;
  test_event event;
  lonejson_sse *sse;
  lonejson_error error;
  lonejson_status status;
  size_t payload_len;

  payload_len = strlen(payload);
  json_options.event_names = selected;
  json_options.event_name_count = 1u;
  json_options.parse_options = NULL;

  memset(&event, 0, sizeof(event));
  memset(&json_state, 0, sizeof(json_state));
  sse_options = lonejson_default_sse_options();
  sse_options.max_event_data_bytes = payload_len - 1u;
  sse = lonejson_sse_open(&sse_options, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    return;
  }
  status = lonejson_sse_push_json(
      sse, &test_event_map, &event, header, strlen(header), &json_options,
      test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_sse_push_json(
      sse, &test_event_map, &event, payload, payload_len, &json_options,
      test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_sse_push_json(
      sse, &test_event_map, &event, event_terminator, strlen(event_terminator),
      &json_options, test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(error.code == LONEJSON_STATUS_OVERFLOW);
  EXPECT(json_state.count == 0u);
  lonejson_sse_close(sse);

  memset(&event, 0, sizeof(event));
  memset(&json_state, 0, sizeof(json_state));
  sse_options = lonejson_default_sse_options();
  sse_options.max_event_data_bytes = payload_len;
  sse = lonejson_sse_open(&sse_options, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    return;
  }
  status = lonejson_sse_push_json(
      sse, &test_event_map, &event, header, strlen(header), &json_options,
      test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_sse_push_json(
      sse, &test_event_map, &event, payload, payload_len, &json_options,
      test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_sse_push_json(
      sse, &test_event_map, &event, line_terminator, strlen(line_terminator),
      &json_options, test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_sse_finish_json(sse, &test_event_map, &event, &json_options,
                                    test_sse_json_collect_event_cb, &json_state,
                                    &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(json_state.count == 1u);
  EXPECT(json_state.data_len == payload_len);
  EXPECT(strcmp(json_state.event, "keep") == 0);
  EXPECT(strcmp(json_state.id, "ok") == 0);
  lonejson_sse_close(sse);

  memset(&event, 0, sizeof(event));
  memset(&json_state, 0, sizeof(json_state));
  sse_options = lonejson_default_sse_options();
  sse_options.max_event_data_bytes = payload_len;
  sse = lonejson_sse_open(&sse_options, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    return;
  }
  status = lonejson_sse_push_json(
      sse, &test_event_map, &event, header, strlen(header), &json_options,
      test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_sse_push_json(
      sse, &test_event_map, &event, payload, payload_len, &json_options,
      test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_sse_push_json(
      sse, &test_event_map, &event, split_line, strlen(split_line),
      &json_options, test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(error.code == LONEJSON_STATUS_OVERFLOW);
  EXPECT(json_state.count == 0u);
  lonejson_sse_close(sse);
}

static void test_sse_json_event_limits_alloc_guarded_e2e(void) {
  static const char header[] = "event: keep\nid: ok\ndata: ";
  static const char payload[] = "{\"name\":\"overflow\",\"tags\":[\"x\",\"y\","
                                "\"z\"],\"body\":\"payload\"}";
  static const char terminator[] = "\n\n";
  test_allocator_state parse_alloc;
  lonejson_parse_options parse_options;
  lonejson_sse_options sse_options;
  lonejson_sse_json_options json_options;
  test_sse_json_state json_state;
  test_alloc_parse_doc doc;
  lonejson_sse *sse;
  lonejson_error error;
  lonejson_status status;

  memset(&json_state, 0, sizeof(json_state));
  memset(&doc, 0, sizeof(doc));
  test_allocator_init(&parse_alloc);
  parse_options = lonejson_default_parse_options();
  parse_options.allocator = &parse_alloc.allocator;
  json_options.parse_options = &parse_options;
  json_options.event_names = NULL;
  json_options.event_name_count = 0u;

  sse_options = lonejson_default_sse_options();
  sse_options.max_event_data_bytes = strlen(payload) - 10u;
  sse = lonejson_sse_open(&sse_options, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    return;
  }
  status = lonejson_sse_push_json(
      sse, &test_alloc_parse_doc_map, &doc, header, strlen(header),
      &json_options, test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_sse_push_json(
      sse, &test_alloc_parse_doc_map, &doc, payload, strlen(payload),
      &json_options, test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_sse_push_json(
      sse, &test_alloc_parse_doc_map, &doc, terminator, strlen(terminator),
      &json_options, test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(error.code == LONEJSON_STATUS_OVERFLOW);
  EXPECT(json_state.count == 0u);
  EXPECT(parse_alloc.stats.alloc_calls == 0u);
  EXPECT(parse_alloc.stats.realloc_calls == 0u);
  EXPECT(parse_alloc.stats.bytes_live == 0u);
  lonejson_sse_close(sse);
}

static void test_sse_retry_and_json_metadata_only_events(void) {
  static const char valid_retry[] = "retry: 1500\n\n";
  static const char invalid_retries[] =
      "retry: -1\n\n"
      "retry: +1\n\n"
      "retry:  1\n\n"
      "retry: 1844674407370955161518446744073709551615\n\n";
  static const char data_event[] = "data: x\n\n";
  static const char json_metadata_only[] = "event: keep\nretry: 1500\n\n";
  static const char json_invalid_retry_only[] = "event: keep\nretry: -1\n\n";
  const char *selected[] = {"keep"};
  lonejson_sse_json_options json_options;
  test_sse_json_state json_state;
  test_sse_state state;
  lonejson_sse_handler handler;
  lonejson_sse *sse;
  test_event event;
  lonejson_error error;
  lonejson_status status;

  memset(&state, 0, sizeof(state));
  handler = test_sse_handler();
  sse = lonejson_sse_open(NULL, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    return;
  }
  status = lonejson_sse_push(sse, valid_retry, sizeof(valid_retry) - 1u,
                             &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.count == 1u);
  EXPECT(state.has_retry);
  EXPECT(state.retry_ms == 1500u);
  status = lonejson_sse_push(sse, invalid_retries, sizeof(invalid_retries) - 1u,
                             &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.count == 1u);
  status = lonejson_sse_push(sse, data_event, sizeof(data_event) - 1u, &handler,
                             &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.count == 2u);
  EXPECT(!state.has_retry);
  lonejson_sse_close(sse);

  memset(&json_state, 0, sizeof(json_state));
  memset(&event, 0, sizeof(event));
  json_options.event_names = selected;
  json_options.event_name_count = 1u;
  json_options.parse_options = NULL;
  sse = lonejson_sse_open(NULL, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    return;
  }
  status = lonejson_sse_push_json(
      sse, &test_event_map, &event, json_metadata_only,
      sizeof(json_metadata_only) - 1u, &json_options,
      test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_sse_push_json(
      sse, &test_event_map, &event, json_invalid_retry_only,
      sizeof(json_invalid_retry_only) - 1u, &json_options,
      test_sse_json_collect_event_cb, &json_state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_sse_finish_json(sse, &test_event_map, &event, &json_options,
                                    test_sse_json_collect_event_cb, &json_state,
                                    &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(json_state.count == 0u);
  lonejson_sse_close(sse);
}

static void test_sse_callback_failure_status(void) {
  test_sse_state state;
  lonejson_sse_handler handler;
  lonejson_sse *sse;
  lonejson_error error;
  lonejson_status status;
  static const char event[] = "data: x\n\n";

  memset(&state, 0, sizeof(state));
  handler = test_sse_handler();
  state.fail = 1;
  sse = lonejson_sse_open(NULL, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    return;
  }
  status = lonejson_sse_push(sse, event, sizeof(event) - 1u, &handler, &state,
                             &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(error.code == LONEJSON_STATUS_CALLBACK_FAILED);
  lonejson_sse_close(sse);
}

static void test_sse_limits_and_allocation_contract(void) {
  static const char event[] = "data: hello\n\n";
  lonejson_sse_options options;
  lonejson_allocator partial_allocator;
  test_allocator_state alloc;
  test_sse_state state;
  lonejson_sse_handler handler;
  lonejson_sse *sse;
  lonejson_error error;
  lonejson_status status;
  size_t alloc_calls_after_open;
  size_t realloc_calls_after_open;

  memset(&partial_allocator, 0, sizeof(partial_allocator));
  partial_allocator.malloc_fn = test_allocator_malloc;
  options = lonejson_default_sse_options();
  options.allocator = &partial_allocator;
  sse = lonejson_sse_open(&options, &error);
  EXPECT(sse == NULL);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);

  test_allocator_init(&alloc);
  options = lonejson_default_sse_options();
  options.allocator = &alloc.allocator;
  sse = lonejson_sse_open(&options, &error);
  EXPECT(sse != NULL);
  if (sse == NULL) {
    return;
  }
  status = lonejson_sse_push(sse, ": warmup\n", strlen(": warmup\n"), NULL,
                             NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  alloc_calls_after_open = alloc.stats.alloc_calls;
  realloc_calls_after_open = alloc.stats.realloc_calls;
  memset(&state, 0, sizeof(state));
  handler = test_sse_handler();
  status = lonejson_sse_push(sse, event, sizeof(event) - 1u, &handler, &state,
                             &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.count == 1u);
  EXPECT(alloc.stats.alloc_calls == alloc_calls_after_open);
  EXPECT(alloc.stats.realloc_calls == realloc_calls_after_open);
  status = lonejson_sse_finish(sse, &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_sse_push(sse, event, sizeof(event) - 1u, &handler, &state,
                             &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  lonejson_sse_close(sse);
  EXPECT(alloc.stats.bytes_live == 0u);
}

static void test_sse_limits_and_invalid_usage(void) {
  lonejson_sse_options options;
  lonejson_sse *sse;
  test_sse_state state;
  lonejson_sse_handler handler;
  lonejson_error error;
  lonejson_status status;

  memset(&state, 0, sizeof(state));
  handler = test_sse_handler();
  options = lonejson_default_sse_options();
  options.max_line_bytes = 4u;
  sse = lonejson_sse_open(&options, &error);
  EXPECT(sse != NULL);
  if (sse != NULL) {
    status = lonejson_sse_push(sse, "data: abc\n", strlen("data: abc\n"),
                               &handler, &state, &error);
    EXPECT(status == LONEJSON_STATUS_OVERFLOW);
    EXPECT(error.code == LONEJSON_STATUS_OVERFLOW);
    lonejson_sse_close(sse);
  }

  options = lonejson_default_sse_options();
  options.max_event_data_bytes = 3u;
  sse = lonejson_sse_open(&options, &error);
  EXPECT(sse != NULL);
  if (sse != NULL) {
    status = lonejson_sse_push(sse, "data: abcd\n\n", strlen("data: abcd\n\n"),
                               &handler, &state, &error);
    EXPECT(status == LONEJSON_STATUS_OVERFLOW);
    EXPECT(error.code == LONEJSON_STATUS_OVERFLOW);
    lonejson_sse_close(sse);
  }

  status = lonejson_sse_push(NULL, "", 0u, &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  options = lonejson_default_sse_options();
  sse = lonejson_sse_open(&options, &error);
  EXPECT(sse != NULL);
  if (sse != NULL) {
    status = lonejson_sse_push(sse, NULL, 1u, &handler, &state, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
    lonejson_sse_close(sse);
  }
}

static void test_multipart_incremental_parts_and_raw_payloads(void) {
  static const char payload[] = "raw\r\n--abc\r\nbytes\npayload";
  static const char prefix[] =
      "--abc\r\n"
      "Content-Disposition: form-data; name=\"meta\"\r\n"
      "Content-Type: application/json\r\n"
      "\r\n"
      "{\"ok\":true}\r\n"
      "--abc\r\n"
      "Content-Disposition: form-data; name=\"payload\"\r\n"
      "Content-Type: application/octet-stream\r\n"
      "Content-Length: 25\r\n"
      "\r\n";
  static const char suffix[] = "\r\n--abc--\r\n";
  lonejson_multipart_handler handler;
  test_multipart_state state;
  lonejson_multipart *mp;
  lonejson_error error;
  lonejson_status status;
  size_t i;

  memset(&handler, 0, sizeof(handler));
  handler.begin_part = test_multipart_begin_cb;
  handler.part_data = test_multipart_data_cb;
  handler.end_part = test_multipart_end_cb;
  memset(&state, 0, sizeof(state));
  mp = lonejson_multipart_open("Multipart/Related; boundary = \"abc\"", NULL,
                               &error);
  EXPECT(mp != NULL);
  if (mp == NULL) {
    return;
  }
  for (i = 0u; i < sizeof(prefix) - 1u; ++i) {
    status =
        lonejson_multipart_push(mp, prefix + i, 1u, &handler, &state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
  }
  for (i = 0u; i < sizeof(payload) - 1u; ++i) {
    status =
        lonejson_multipart_push(mp, payload + i, 1u, &handler, &state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
  }
  for (i = 0u; i < sizeof(suffix) - 1u; ++i) {
    status =
        lonejson_multipart_push(mp, suffix + i, 1u, &handler, &state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
  }
  status = lonejson_multipart_finish(mp, &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.begin_count == 2u);
  EXPECT(state.end_count == 2u);
  EXPECT(strcmp(state.names[0], "meta") == 0);
  EXPECT(strcmp(state.content_types[0], "application/json") == 0);
  EXPECT(state.lengths[0] == -1);
  EXPECT(strcmp(state.names[1], "payload") == 0);
  EXPECT(strcmp(state.content_types[1], "application/octet-stream") == 0);
  EXPECT(state.lengths[1] == 25);
  EXPECT(strcmp(state.data, "{\"ok\":true}raw\r\n--abc\r\nbytes\npayload") ==
         0);
  lonejson_multipart_close(mp);
}

static void test_multipart_no_length_preserves_body_bytes(void) {
  static const char body[] = "--b\n"
                             "Content-Disposition: form-data; name=text\n"
                             "\n"
                             "line1\r\nline2\n"
                             "--b--\n";
  lonejson_multipart_handler handler;
  test_multipart_state state;
  lonejson_multipart *mp;
  lonejson_error error;
  lonejson_status status;

  memset(&handler, 0, sizeof(handler));
  handler.begin_part = test_multipart_begin_cb;
  handler.part_data = test_multipart_data_cb;
  handler.end_part = test_multipart_end_cb;
  memset(&state, 0, sizeof(state));
  mp = lonejson_multipart_open("multipart/mixed; boundary=b", NULL, &error);
  EXPECT(mp != NULL);
  if (mp == NULL) {
    return;
  }
  status = lonejson_multipart_push(mp, body, sizeof(body) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_multipart_finish(mp, &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.begin_count == 1u);
  EXPECT(state.end_count == 1u);
  EXPECT(strcmp(state.names[0], "text") == 0);
  EXPECT(strcmp(state.data, "line1\r\nline2") == 0);
  lonejson_multipart_close(mp);
}

static void test_multipart_no_length_streams_long_lines_with_lookahead(void) {
  static const char prefix[] = "--x\r\n\r\n";
  static const char chunk0[] = "aaaaaaaaaaaaaaaa";
  static const char chunk1[] = "bbbbbbbbbbbbbbbb";
  static const char chunk2[] = "cccccccccccccccc";
  static const char suffix[] = "\r\n--x--\r\n";
  static const char false_boundary_body[] = "--xnot-a-delimiter";
  lonejson_multipart_options options;
  lonejson_multipart_handler handler;
  test_multipart_state state;
  lonejson_multipart *mp;
  lonejson_error error;
  lonejson_status status;

  memset(&handler, 0, sizeof(handler));
  handler.begin_part = test_multipart_begin_cb;
  handler.part_data = test_multipart_data_cb;
  handler.end_part = test_multipart_end_cb;
  options = lonejson_default_multipart_options();
  options.max_part_buffered_bytes = 9u;
  memset(&state, 0, sizeof(state));
  mp = lonejson_multipart_open("multipart/mixed; boundary=x", &options, &error);
  EXPECT(mp != NULL);
  if (mp == NULL) {
    return;
  }
  status = lonejson_multipart_push(mp, prefix, sizeof(prefix) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_multipart_push(mp, chunk0, sizeof(chunk0) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_multipart_push(mp, chunk1, sizeof(chunk1) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_multipart_push(mp, chunk2, sizeof(chunk2) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.data_len == strlen(chunk0) + strlen(chunk1) + strlen(chunk2));
  EXPECT(strcmp(state.data,
                "aaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbcccccccccccccccc") == 0);
  status = lonejson_multipart_push(mp, suffix, sizeof(suffix) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_multipart_finish(mp, &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.begin_count == 1u);
  EXPECT(state.end_count == 1u);
  lonejson_multipart_close(mp);

  memset(&state, 0, sizeof(state));
  mp = lonejson_multipart_open("multipart/mixed; boundary=x", &options, &error);
  EXPECT(mp != NULL);
  if (mp == NULL) {
    return;
  }
  status = lonejson_multipart_push(mp, prefix, sizeof(prefix) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_multipart_push(mp, false_boundary_body,
                                   sizeof(false_boundary_body) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(state.data, false_boundary_body) == 0);
  status = lonejson_multipart_push(mp, suffix, sizeof(suffix) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_multipart_finish(mp, &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.end_count == 1u);
  lonejson_multipart_close(mp);
}

static void
test_multipart_content_length_streams_body_without_body_allocations(void) {
  static const char prefix[] =
      "--stream\r\n"
      "Content-Disposition: form-data; name=\"payload\"\r\n"
      "Content-Type: application/octet-stream\r\n"
      "Content-Length: 17\r\n"
      "\r\n";
  static const char chunk0[] = "abc";
  static const char chunk1[] = "defgh";
  static const char chunk2[] = "ijklmnopq";
  static const char suffix[] = "\r\n--stream--\r\n";
  lonejson_multipart_options options;
  lonejson_multipart_handler handler;
  test_allocator_state alloc;
  test_multipart_state state;
  lonejson_multipart *mp;
  lonejson_error error;
  lonejson_status status;

  memset(&handler, 0, sizeof(handler));
  handler.begin_part = test_multipart_begin_cb;
  handler.part_data = test_multipart_data_cb;
  handler.end_part = test_multipart_end_cb;
  test_allocator_init(&alloc);
  options = lonejson_default_multipart_options();
  options.allocator = &alloc.allocator;
  memset(&state, 0, sizeof(state));
  state.stats = &alloc.stats;
  mp = lonejson_multipart_open("multipart/form-data; boundary=stream", &options,
                               &error);
  EXPECT(mp != NULL);
  if (mp == NULL) {
    return;
  }
  status = lonejson_multipart_push(mp, prefix, sizeof(prefix) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.begin_count == 1u);
  EXPECT(state.data_count == 0u);
  state.alloc_calls_after_headers = alloc.stats.alloc_calls;
  state.realloc_calls_after_headers = alloc.stats.realloc_calls;
  state.check_no_body_alloc = 1;

  status = lonejson_multipart_push(mp, chunk0, sizeof(chunk0) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_multipart_push(mp, chunk1, sizeof(chunk1) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_multipart_push(mp, chunk2, sizeof(chunk2) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(alloc.stats.alloc_calls == state.alloc_calls_after_headers);
  EXPECT(alloc.stats.realloc_calls == state.realloc_calls_after_headers);
  EXPECT(state.saw_body_alloc == 0);
  EXPECT(state.data_count == 3u);
  EXPECT(state.data_chunks[0] == sizeof(chunk0) - 1u);
  EXPECT(state.data_chunks[1] == sizeof(chunk1) - 1u);
  EXPECT(state.data_chunks[2] == sizeof(chunk2) - 1u);
  EXPECT(strcmp(state.data, "abcdefghijklmnopq") == 0);
  state.check_no_body_alloc = 0;

  status = lonejson_multipart_push(mp, suffix, sizeof(suffix) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_multipart_finish(mp, &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.end_count == 1u);
  EXPECT(strcmp(state.names[0], "payload") == 0);
  EXPECT(strcmp(state.content_types[0], "application/octet-stream") == 0);
  EXPECT(state.lengths[0] == 17);
  lonejson_multipart_close(mp);
  EXPECT(alloc.stats.bytes_live == 0u);
}

static void test_multipart_content_disposition_name_parameters(void) {
  static const char body[] =
      "--d\n"
      "Content-Disposition: form-data; filename=\"wrong\"; NAME=\"right\"\n"
      "\n"
      "one\n"
      "--d\n"
      "Content-Disposition: form-data; filename=\"wrong\"; "
      "name=\"pay\\\"load\"\n"
      "\n"
      "two\n"
      "--d--\n";
  lonejson_multipart_handler handler;
  test_multipart_state state;
  lonejson_multipart *mp;
  lonejson_error error;
  lonejson_status status;

  memset(&handler, 0, sizeof(handler));
  handler.begin_part = test_multipart_begin_cb;
  handler.part_data = test_multipart_data_cb;
  handler.end_part = test_multipart_end_cb;
  memset(&state, 0, sizeof(state));
  mp = lonejson_multipart_open("multipart/form-data; boundary=d", NULL, &error);
  EXPECT(mp != NULL);
  if (mp == NULL) {
    return;
  }
  status = lonejson_multipart_push(mp, body, sizeof(body) - 1u, &handler,
                                   &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_multipart_finish(mp, &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.begin_count == 2u);
  EXPECT(state.end_count == 2u);
  EXPECT(strcmp(state.names[0], "right") == 0);
  EXPECT(strcmp(state.names[1], "pay\"load") == 0);
  lonejson_multipart_close(mp);
}

static void test_multipart_limits_and_invalid_usage(void) {
  static const char invalid_header[] = "--x\nnot-a-header\n\nvalue\n--x--\n";
  static const char too_many_headers[] = "--x\nA: 1\nB: 2\n\nvalue\n--x--\n";
  static const char bad_length[] = "--x\nContent-Length: 12x\n\nvalue\n--x--\n";
  static const char huge_length[] =
      "--x\nContent-Length: 999999999999999999999999999999\n\nvalue\n--x--\n";
  static const char too_small_boundary_lookahead[] = "--x\n\n--x--\n";
  lonejson_multipart_options options;
  lonejson_multipart_handler handler;
  lonejson_allocator partial_allocator;
  test_multipart_state state;
  lonejson_multipart *mp;
  lonejson_error error;
  lonejson_status status;

  memset(&handler, 0, sizeof(handler));
  handler.begin_part = test_multipart_begin_cb;
  handler.part_data = test_multipart_data_cb;
  handler.end_part = test_multipart_end_cb;
  memset(&partial_allocator, 0, sizeof(partial_allocator));
  partial_allocator.malloc_fn = test_allocator_malloc;
  options = lonejson_default_multipart_options();
  options.allocator = &partial_allocator;
  mp = lonejson_multipart_open("multipart/mixed; boundary=x", &options, &error);
  EXPECT(mp == NULL);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);

  mp = lonejson_multipart_open("multipart/mixed", NULL, &error);
  EXPECT(mp == NULL);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);
  mp = lonejson_multipart_open("multipart/mixed; boundary=\"unterminated", NULL,
                               &error);
  EXPECT(mp == NULL);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);

  options = lonejson_default_multipart_options();
  options.max_boundary_bytes = 3u;
  mp = lonejson_multipart_open("multipart/mixed; boundary=abcd", &options,
                               &error);
  EXPECT(mp == NULL);
  EXPECT(error.code == LONEJSON_STATUS_OVERFLOW);

  memset(&state, 0, sizeof(state));
  mp = lonejson_multipart_open("multipart/mixed; boundary=x", NULL, &error);
  EXPECT(mp != NULL);
  if (mp != NULL) {
    status =
        lonejson_multipart_push(mp, invalid_header, sizeof(invalid_header) - 1u,
                                &handler, &state, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
    lonejson_multipart_close(mp);
  }

  options = lonejson_default_multipart_options();
  options.max_header_count = 1u;
  memset(&state, 0, sizeof(state));
  mp = lonejson_multipart_open("multipart/mixed; boundary=x", &options, &error);
  EXPECT(mp != NULL);
  if (mp != NULL) {
    status = lonejson_multipart_push(mp, too_many_headers,
                                     sizeof(too_many_headers) - 1u, &handler,
                                     &state, &error);
    EXPECT(status == LONEJSON_STATUS_OVERFLOW);
    lonejson_multipart_close(mp);
  }

  memset(&state, 0, sizeof(state));
  mp = lonejson_multipart_open("multipart/mixed; boundary=x", NULL, &error);
  EXPECT(mp != NULL);
  if (mp != NULL) {
    status = lonejson_multipart_push(mp, bad_length, sizeof(bad_length) - 1u,
                                     &handler, &state, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
    lonejson_multipart_close(mp);
  }

  memset(&state, 0, sizeof(state));
  mp = lonejson_multipart_open("multipart/mixed; boundary=x", NULL, &error);
  EXPECT(mp != NULL);
  if (mp != NULL) {
    status = lonejson_multipart_push(mp, huge_length, sizeof(huge_length) - 1u,
                                     &handler, &state, &error);
    EXPECT(status == LONEJSON_STATUS_OVERFLOW);
    lonejson_multipart_close(mp);
  }

  options = lonejson_default_multipart_options();
  options.max_part_buffered_bytes = 4u;
  memset(&state, 0, sizeof(state));
  mp = lonejson_multipart_open("multipart/mixed; boundary=x", &options, &error);
  EXPECT(mp != NULL);
  if (mp != NULL) {
    status = lonejson_multipart_push(mp, too_small_boundary_lookahead,
                                     sizeof(too_small_boundary_lookahead) - 1u,
                                     &handler, &state, &error);
    EXPECT(status == LONEJSON_STATUS_OVERFLOW);
    lonejson_multipart_close(mp);
  }

  memset(&state, 0, sizeof(state));
  mp = lonejson_multipart_open("multipart/mixed; boundary=x", NULL, &error);
  EXPECT(mp != NULL);
  if (mp != NULL) {
    status = lonejson_multipart_push(mp, "--x--\n", strlen("--x--\n"), &handler,
                                     &state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    status = lonejson_multipart_finish(mp, &handler, &state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    status = lonejson_multipart_push(mp, "--x--\n", strlen("--x--\n"), &handler,
                                     &state, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
    lonejson_multipart_close(mp);
  }
}

static void test_multipart_failures_are_reported(void) {
  static const char incomplete[] = "--x\nContent-Type: text/plain\n\nvalue\n";
  static const char with_data[] =
      "--x\nContent-Type: text/plain\nContent-Length: 5\n\nvalue\n--x--\n";
  static const char empty_part[] = "--x\nContent-Type: text/plain\n\n\n--x--\n";
  lonejson_multipart_handler handler;
  test_multipart_state state;
  lonejson_multipart *mp;
  lonejson_error error;
  lonejson_status status;

  memset(&handler, 0, sizeof(handler));
  handler.begin_part = test_multipart_begin_cb;
  handler.part_data = test_multipart_data_cb;
  handler.end_part = test_multipart_end_cb;
  memset(&state, 0, sizeof(state));
  mp = lonejson_multipart_open("multipart/mixed; boundary=x", NULL, &error);
  EXPECT(mp != NULL);
  if (mp == NULL) {
    return;
  }
  status = lonejson_multipart_push(mp, incomplete, sizeof(incomplete) - 1u,
                                   &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_multipart_finish(mp, &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
  lonejson_multipart_close(mp);

  memset(&state, 0, sizeof(state));
  state.fail_data = 1;
  mp = lonejson_multipart_open("multipart/mixed; boundary=x", NULL, &error);
  EXPECT(mp != NULL);
  if (mp == NULL) {
    return;
  }
  status = lonejson_multipart_push(mp, with_data, sizeof(with_data) - 1u,
                                   &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(error.code == LONEJSON_STATUS_CALLBACK_FAILED);
  lonejson_multipart_close(mp);

  memset(&state, 0, sizeof(state));
  state.fail_begin = 1;
  mp = lonejson_multipart_open("multipart/mixed; boundary=x", NULL, &error);
  EXPECT(mp != NULL);
  if (mp == NULL) {
    return;
  }
  status = lonejson_multipart_push(mp, empty_part, sizeof(empty_part) - 1u,
                                   &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(error.code == LONEJSON_STATUS_CALLBACK_FAILED);
  lonejson_multipart_close(mp);

  memset(&state, 0, sizeof(state));
  state.fail_end = 1;
  mp = lonejson_multipart_open("multipart/mixed; boundary=x", NULL, &error);
  EXPECT(mp != NULL);
  if (mp == NULL) {
    return;
  }
  status = lonejson_multipart_push(mp, empty_part, sizeof(empty_part) - 1u,
                                   &handler, &state, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(error.code == LONEJSON_STATUS_CALLBACK_FAILED);
  lonejson_multipart_close(mp);
}

