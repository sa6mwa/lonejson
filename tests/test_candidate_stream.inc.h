typedef struct test_candidate_stream_state {
  size_t begin_count;
  size_t end_count;
  size_t paths_seen;
  size_t numbers_seen;
  size_t bools_seen;
  size_t nulls_seen;
  size_t stop_after_end;
  size_t fail_at_begin;
  size_t begin_offsets[8];
  size_t end_offsets[8];
  size_t end_sizes[8];
  char payloads[8][128];
  size_t payload_sizes[8];
  int payload_spilled[8];
  char payload_spool_paths[8][LONEJSON_SPOOL_TEMP_PATH_CAPACITY];
  size_t fail_at_end;
} test_candidate_stream_state;

static lonejson_candidate_callback_result
test_candidate_begin(void *user, const lonejson_candidate_info *candidate,
                     lonejson_error *error) {
  test_candidate_stream_state *state = (test_candidate_stream_state *)user;
  (void)error;
  if (state->begin_count <
      sizeof(state->begin_offsets) / sizeof(state->begin_offsets[0])) {
    state->begin_offsets[state->begin_count] = candidate->stream_offset;
  }
  ++state->begin_count;
  if (state->fail_at_begin != 0u && state->begin_count == state->fail_at_begin) {
    return LONEJSON_CANDIDATE_ERROR;
  }
  return LONEJSON_CANDIDATE_CONTINUE;
}

static lonejson_candidate_callback_result
test_candidate_end(void *user, const lonejson_candidate_info *candidate,
                   lonejson_error *error) {
  test_candidate_stream_state *state = (test_candidate_stream_state *)user;
  (void)error;
  if (state->end_count <
      sizeof(state->end_offsets) / sizeof(state->end_offsets[0])) {
    size_t slot = state->end_count;
    state->end_offsets[state->end_count] = candidate->stream_offset;
    state->end_sizes[state->end_count] = candidate->byte_size;
    if (candidate->payload != NULL) {
      size_t copy = candidate->payload_size;
      if (copy >= sizeof(state->payloads[slot])) {
        copy = sizeof(state->payloads[slot]) - 1u;
      }
      memcpy(state->payloads[slot], candidate->payload, copy);
      state->payloads[slot][copy] = '\0';
      state->payload_sizes[slot] = candidate->payload_size;
    }
    if (candidate->payload_spool != NULL) {
      test_buffer_sink sink;
      char buffer[128];
      lonejson_error local_error;

      memset(buffer, 0, sizeof(buffer));
      memset(&sink, 0, sizeof(sink));
      sink.buffer = (unsigned char *)buffer;
      sink.capacity = sizeof(buffer);
      EXPECT(lonejson_spooled_write_to_sink(candidate->payload_spool,
                                            test_buffer_sink_write, &sink,
                                            &local_error) ==
             LONEJSON_STATUS_OK);
      memcpy(state->payloads[slot], buffer, sink.length + 1u);
      state->payload_sizes[slot] = lonejson_spooled_size(candidate->payload_spool);
      state->payload_spilled[slot] =
          lonejson_spooled_spilled(candidate->payload_spool);
      if (candidate->payload_spool->temp_path[0] != '\0') {
        test_copy_cstr(state->payload_spool_paths[slot],
                       sizeof(state->payload_spool_paths[slot]),
                       candidate->payload_spool->temp_path);
      }
    }
  }
  ++state->end_count;
  if (state->fail_at_end != 0u && state->end_count == state->fail_at_end) {
    return LONEJSON_CANDIDATE_ERROR;
  }
  if (state->stop_after_end != 0u &&
      state->end_count == state->stop_after_end) {
    return LONEJSON_CANDIDATE_STOP;
  }
  return LONEJSON_CANDIDATE_CONTINUE;
}

static lonejson_status test_candidate_path_number(
    void *user, const lonejson_value_path *path, const char *data, size_t len,
    lonejson_error *error) {
  test_candidate_stream_state *state = (test_candidate_stream_state *)user;
  (void)data;
  (void)len;
  (void)error;
  if (path != NULL && path->segment_count != 0u) {
    ++state->paths_seen;
  }
  ++state->numbers_seen;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_candidate_number(void *user, const char *data,
                                             size_t len,
                                             lonejson_error *error) {
  test_candidate_stream_state *state = (test_candidate_stream_state *)user;
  (void)data;
  (void)len;
  (void)error;
  ++state->numbers_seen;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_candidate_bool(void *user, int value,
                                           lonejson_error *error) {
  test_candidate_stream_state *state = (test_candidate_stream_state *)user;
  (void)value;
  (void)error;
  ++state->bools_seen;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_candidate_null(void *user, lonejson_error *error) {
  test_candidate_stream_state *state = (test_candidate_stream_state *)user;
  (void)error;
  ++state->nulls_seen;
  return LONEJSON_STATUS_OK;
}

static void test_candidate_stream_single_object(void) {
  static const char json[] = "{\"a\":1}";
  test_candidate_stream_state state;
  lonejson_candidate_stream_options options;
  lonejson_path_value_visitor visitor;
  lonejson_error error;
  lonejson_status status;

  memset(&state, 0, sizeof(state));
  options = lonejson_default_candidate_stream_options();
  visitor = lonejson_default_path_value_visitor();
  visitor.number_chunk = test_candidate_path_number;
  options.framing = LONEJSON_CANDIDATE_FRAMING_SINGLE_VALUE;
  options.path_visitor = &visitor;
  options.visitor_user = &state;
  options.candidate_begin = test_candidate_begin;
  options.candidate_end = test_candidate_end;
  options.candidate_user = &state;
  status = lonejson_visit_candidates_buffer(test_default_runtime(), json,
                                            strlen(json), &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.begin_count == 1u);
  EXPECT(state.end_count == 1u);
  EXPECT(state.begin_offsets[0] == 0u);
  EXPECT(state.end_offsets[0] == 0u);
  EXPECT(state.end_sizes[0] == strlen(json));
  EXPECT(state.paths_seen == 1u);
}

static void test_candidate_stream_array_items(void) {
  static const char json[] = "[{\"a\":1}, 2, null]";
  test_candidate_stream_state state;
  lonejson_candidate_stream_options options;
  lonejson_value_visitor visitor;
  lonejson_status status;
  lonejson_error error;

  memset(&state, 0, sizeof(state));
  options = lonejson_default_candidate_stream_options();
  visitor = lonejson_default_value_visitor();
  visitor.number_chunk = test_candidate_number;
  visitor.null_value = test_candidate_null;
  options.framing = LONEJSON_CANDIDATE_FRAMING_ARRAY_ITEMS;
  options.visitor = &visitor;
  options.visitor_user = &state;
  options.candidate_begin = test_candidate_begin;
  options.candidate_end = test_candidate_end;
  options.candidate_user = &state;
  status = lonejson_visit_candidates_buffer(test_default_runtime(), json,
                                            strlen(json), &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.begin_count == 3u);
  EXPECT(state.end_count == 3u);
  EXPECT(state.begin_offsets[0] == 1u);
  EXPECT(state.end_sizes[0] == 7u);
  EXPECT(state.begin_offsets[1] == 10u);
  EXPECT(state.end_sizes[1] == 1u);
  EXPECT(state.begin_offsets[2] == 13u);
  EXPECT(state.end_sizes[2] == 4u);
  EXPECT(state.numbers_seen == 2u);
  EXPECT(state.nulls_seen == 1u);
}

static void test_candidate_stream_repeated_and_auto(void) {
  static const char json[] = "1\n{\"x\":2}\ntrue";
  test_candidate_stream_state state;
  lonejson_candidate_stream_options options;
  lonejson_value_visitor visitor;
  lonejson_status status;
  lonejson_error error;

  memset(&state, 0, sizeof(state));
  options = lonejson_default_candidate_stream_options();
  visitor = lonejson_default_value_visitor();
  visitor.number_chunk = test_candidate_number;
  visitor.boolean_value = test_candidate_bool;
  options.visitor = &visitor;
  options.visitor_user = &state;
  options.candidate_begin = test_candidate_begin;
  options.candidate_end = test_candidate_end;
  options.candidate_user = &state;
  status = lonejson_visit_candidates_buffer(test_default_runtime(), json,
                                            strlen(json), &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.begin_count == 3u);
  EXPECT(state.end_count == 3u);
  EXPECT(state.begin_offsets[0] == 0u);
  EXPECT(state.begin_offsets[1] == 2u);
  EXPECT(state.begin_offsets[2] == 10u);
  EXPECT(state.numbers_seen == 2u);
  EXPECT(state.bools_seen == 1u);
}

static void test_candidate_stream_rejects_adjacent_repeated_values(void) {
  static const char *const invalid_json[] = {"truefalse", "1true", "{}\"x\""};
  test_candidate_stream_state state;
  lonejson_candidate_stream_options options;
  lonejson_status status;
  lonejson_error error;
  size_t i;

  for (i = 0u; i < sizeof(invalid_json) / sizeof(invalid_json[0]); ++i) {
    memset(&state, 0, sizeof(state));
    options = lonejson_default_candidate_stream_options();
    options.framing = LONEJSON_CANDIDATE_FRAMING_NDJSON;
    options.candidate_begin = test_candidate_begin;
    options.candidate_end = test_candidate_end;
    options.candidate_user = &state;
    status = lonejson_visit_candidates_buffer(
        test_default_runtime(), invalid_json[i], strlen(invalid_json[i]),
        &options, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
    EXPECT(state.begin_count == 1u);
    EXPECT(state.end_count == 1u);
    EXPECT(strstr(error.message, "whitespace between repeated JSON values") !=
           NULL);

    memset(&state, 0, sizeof(state));
    options = lonejson_default_candidate_stream_options();
    options.candidate_begin = test_candidate_begin;
    options.candidate_end = test_candidate_end;
    options.candidate_user = &state;
    status = lonejson_visit_candidates_buffer(
        test_default_runtime(), invalid_json[i], strlen(invalid_json[i]),
        &options, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
    EXPECT(state.begin_count == 1u);
    EXPECT(state.end_count == 1u);
    EXPECT(strstr(error.message, "whitespace between repeated JSON values") !=
           NULL);
  }
}

static void test_candidate_stream_malformed_offset(void) {
  static const char json[] = " \n {\"a\":";
  test_candidate_stream_state state;
  lonejson_candidate_stream_options options;
  lonejson_status status;
  lonejson_error error;

  memset(&state, 0, sizeof(state));
  options = lonejson_default_candidate_stream_options();
  options.framing = LONEJSON_CANDIDATE_FRAMING_SINGLE_VALUE;
  options.candidate_begin = test_candidate_begin;
  options.candidate_end = test_candidate_end;
  options.candidate_user = &state;
  status = lonejson_visit_candidates_buffer(test_default_runtime(), json,
                                            strlen(json), &options, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(state.begin_count == 1u);
  EXPECT(state.end_count == 0u);
  EXPECT(error.offset >= state.begin_offsets[0]);
  EXPECT(strstr(error.message, "stream offset") != NULL);
  EXPECT(strstr(error.message, "candidate offset") != NULL);
}

static void test_candidate_stream_callback_stop_and_failure(void) {
  static const char json[] = "1 2 3";
  test_candidate_stream_state state;
  lonejson_candidate_stream_options options;
  lonejson_status status;
  lonejson_error error;

  memset(&state, 0, sizeof(state));
  options = lonejson_default_candidate_stream_options();
  options.framing = LONEJSON_CANDIDATE_FRAMING_NDJSON;
  options.candidate_begin = test_candidate_begin;
  options.candidate_end = test_candidate_end;
  options.candidate_user = &state;
  state.stop_after_end = 1u;
  status = lonejson_visit_candidates_buffer(test_default_runtime(), json,
                                            strlen(json), &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.begin_count == 1u);
  EXPECT(state.end_count == 1u);

  memset(&state, 0, sizeof(state));
  state.fail_at_begin = 2u;
  error.code = LONEJSON_STATUS_INVALID_JSON;
  error.offset = 99u;
  snprintf(error.message, sizeof(error.message), "stale candidate error");
  status = lonejson_visit_candidates_buffer(test_default_runtime(), json,
                                            strlen(json), &options, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(state.begin_count == 2u);
  EXPECT(state.end_count == 1u);
  EXPECT(error.code == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(error.offset == state.begin_offsets[1]);
  EXPECT(strstr(error.message, "candidate callback failed") != NULL);
}

static void test_candidate_stream_reader_and_runtime_methods(void) {
  static const char json[] = "[1,2,3]";
  test_reader_state reader;
  test_candidate_stream_state state;
  lonejson_candidate_stream_options options;
  lonejson_status status;
  lonejson_error error;

  memset(&state, 0, sizeof(state));
  options = lonejson_default_candidate_stream_options();
  options.framing = LONEJSON_CANDIDATE_FRAMING_ARRAY_ITEMS;
  options.candidate_begin = test_candidate_begin;
  options.candidate_end = test_candidate_end;
  options.candidate_user = &state;
  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 1u;
  status = test_default_runtime()->visit_candidates_reader(
      test_default_runtime(), test_state_reader, &reader, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.begin_count == 3u);
  EXPECT(state.end_count == 3u);
  EXPECT(reader.offset == strlen(json));
}

static void test_candidate_stream_file_path_fd_and_large_reader(void) {
  static const char file_json[] = "1 2";
  char path[] = "/tmp/lonejson-candidates-XXXXXX";
  test_candidate_stream_state state;
  lonejson_candidate_stream_options options;
  test_reader_state reader;
  lonejson_error error;
  lonejson_status status;
  char large_json[1025];
  int fd;
  FILE *fp;
  size_t i;

  options = lonejson_default_candidate_stream_options();
  options.framing = LONEJSON_CANDIDATE_FRAMING_NDJSON;
  options.candidate_begin = test_candidate_begin;
  options.candidate_end = test_candidate_end;
  options.candidate_user = &state;

  strcpy(path, "/tmp/lonejson-candidates-XXXXXX");
  fd = write_temp_text_file(path, file_json);
  EXPECT(fd >= 0);
  if (fd >= 0) {
    close(fd);

    memset(&state, 0, sizeof(state));
    fp = fopen(path, "rb");
    EXPECT(fp != NULL);
    if (fp != NULL) {
      status = lonejson_visit_candidates_filep(test_default_runtime(), fp,
                                               &options, &error);
      EXPECT(status == LONEJSON_STATUS_OK);
      EXPECT(state.end_count == 2u);
      fclose(fp);
    }

    memset(&state, 0, sizeof(state));
    status = lonejson_visit_candidates_path(test_default_runtime(), path,
                                            &options, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(state.end_count == 2u);

    fd = open(path, O_RDONLY);
    EXPECT(fd >= 0);
    if (fd >= 0) {
      memset(&state, 0, sizeof(state));
      status = lonejson_visit_candidates_fd(test_default_runtime(), fd,
                                            &options, &error);
      EXPECT(status == LONEJSON_STATUS_OK);
      EXPECT(state.end_count == 2u);
      close(fd);
    }
    unlink(path);
  }

  large_json[0] = '"';
  for (i = 1u; i + 1u < sizeof(large_json); ++i) {
    large_json[i] = 'x';
  }
  large_json[sizeof(large_json) - 2u] = '"';
  large_json[sizeof(large_json) - 1u] = '\0';
  reader.json = large_json;
  reader.offset = 0u;
  reader.chunk_size = 3u;
  memset(&state, 0, sizeof(state));
  options.framing = LONEJSON_CANDIDATE_FRAMING_SINGLE_VALUE;
  status = lonejson_visit_candidates_reader(test_default_runtime(),
                                            test_state_reader, &reader,
                                            &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.begin_count == 1u);
  EXPECT(state.end_count == 1u);
  EXPECT(state.end_sizes[0] == strlen(large_json));
  EXPECT(reader.offset == strlen(large_json));
}

static void test_candidate_stream_capture_sink_and_memory(void) {
  static const char json[] = "[ { \"a\" : 1 }, true ]";
  test_candidate_stream_state state;
  lonejson_candidate_stream_options options;
  test_buffer_sink sink;
  char sink_buffer[128];
  lonejson_status status;
  lonejson_error error;

  memset(&state, 0, sizeof(state));
  memset(&sink, 0, sizeof(sink));
  memset(sink_buffer, 0, sizeof(sink_buffer));
  sink.buffer = (unsigned char *)sink_buffer;
  sink.capacity = sizeof(sink_buffer);
  options = lonejson_default_candidate_stream_options();
  options.framing = LONEJSON_CANDIDATE_FRAMING_ARRAY_ITEMS;
  options.capture_mode = LONEJSON_CANDIDATE_CAPTURE_SINK;
  options.payload_sink = test_buffer_sink_write;
  options.payload_sink_user = &sink;
  options.candidate_begin = test_candidate_begin;
  options.candidate_end = test_candidate_end;
  options.candidate_user = &state;
  status = lonejson_visit_candidates_buffer(test_default_runtime(), json,
                                            strlen(json), &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.end_count == 2u);
  EXPECT(strcmp(sink_buffer, "{\"a\":1}true") == 0);
  EXPECT(state.payload_sizes[0] == 0u);

  memset(&state, 0, sizeof(state));
  options.capture_mode = LONEJSON_CANDIDATE_CAPTURE_MEMORY;
  options.payload_sink = NULL;
  options.payload_sink_user = NULL;
  options.max_memory_payload_bytes = 16u;
  status = lonejson_visit_candidates_buffer(test_default_runtime(), json,
                                            strlen(json), &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.end_count == 2u);
  EXPECT(strcmp(state.payloads[0], "{\"a\":1}") == 0);
  EXPECT(state.payload_sizes[0] == strlen("{\"a\":1}"));
  EXPECT(strcmp(state.payloads[1], "true") == 0);
  EXPECT(state.payload_sizes[1] == strlen("true"));
}

static void test_candidate_stream_capture_spooled_and_cleanup(void) {
  static const char json[] = "[ { \"long\" : \"abcdef\" }, 2 ]";
  char dir_template[] = "/tmp/lonejson-candidate-spool-XXXXXX";
  char *dir;
  test_candidate_stream_state state;
  lonejson_candidate_stream_options options;
  lonejson_config config;
  lonejson *runtime;
  lonejson_status status;
  lonejson_error error;

  dir = mkdtemp(dir_template);
  EXPECT(dir != NULL);
  if (dir == NULL) {
    return;
  }
  config = lonejson_default_config();
  config.spool_default.memory_limit = 1u;
  config.spool_default.temp_dir = dir;
  runtime = lonejson_new(&config, &error);
  EXPECT(runtime != NULL);
  if (runtime == NULL) {
    rmdir(dir);
    return;
  }

  memset(&state, 0, sizeof(state));
  options = lonejson_default_candidate_stream_options();
  options.framing = LONEJSON_CANDIDATE_FRAMING_ARRAY_ITEMS;
  options.capture_mode = LONEJSON_CANDIDATE_CAPTURE_SPOOLED;
  options.candidate_begin = test_candidate_begin;
  options.candidate_end = test_candidate_end;
  options.candidate_user = &state;
  status = lonejson_visit_candidates_buffer(runtime, json, strlen(json),
                                            &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.end_count == 2u);
  EXPECT(strcmp(state.payloads[0], "{\"long\":\"abcdef\"}") == 0);
  EXPECT(state.payload_spilled[0] != 0);
  EXPECT(state.payload_spool_paths[0][0] != '\0');
  EXPECT(access(state.payload_spool_paths[0], F_OK) != 0);

  memset(&state, 0, sizeof(state));
  state.fail_at_end = 1u;
  status = lonejson_visit_candidates_buffer(runtime, json, strlen(json),
                                            &options, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(state.end_count == 1u);
  EXPECT(state.payload_spool_paths[0][0] != '\0');
  EXPECT(access(state.payload_spool_paths[0], F_OK) != 0);

  lonejson_free(runtime);
  rmdir(dir);
}

static void test_candidate_stream_capture_failure_modes(void) {
  static const char json[] = "{\"a\":1}";
  test_candidate_stream_state state;
  lonejson_candidate_stream_options options;
  test_failing_sink failing_sink;
  lonejson_status status;
  lonejson_error error;

  memset(&state, 0, sizeof(state));
  options = lonejson_default_candidate_stream_options();
  options.capture_mode = LONEJSON_CANDIDATE_CAPTURE_MEMORY;
  options.max_memory_payload_bytes = 4u;
  options.candidate_begin = test_candidate_begin;
  options.candidate_end = test_candidate_end;
  options.candidate_user = &state;
  status = lonejson_visit_candidates_buffer(test_default_runtime(), json,
                                            strlen(json), &options, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(state.begin_count == 1u);
  EXPECT(state.end_count == 0u);

  memset(&state, 0, sizeof(state));
  memset(&failing_sink, 0, sizeof(failing_sink));
  failing_sink.fail_after = 2u;
  options = lonejson_default_candidate_stream_options();
  options.capture_mode = LONEJSON_CANDIDATE_CAPTURE_SINK;
  options.payload_sink = test_failing_sink_write;
  options.payload_sink_user = &failing_sink;
  options.candidate_begin = test_candidate_begin;
  options.candidate_end = test_candidate_end;
  options.candidate_user = &state;
  status = lonejson_visit_candidates_buffer(test_default_runtime(), json,
                                            strlen(json), &options, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(state.begin_count == 1u);
  EXPECT(state.end_count == 0u);

  options = lonejson_default_candidate_stream_options();
  options.capture_mode = LONEJSON_CANDIDATE_CAPTURE_SINK;
  status = lonejson_visit_candidates_buffer(test_default_runtime(), json,
                                            strlen(json), &options, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);

  options = lonejson_default_candidate_stream_options();
  options.capture_mode = (lonejson_candidate_capture_mode)99;
  status = lonejson_visit_candidates_buffer(test_default_runtime(), json,
                                            strlen(json), &options, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
}

static void test_candidate_stream_capture_path_visitor_user(void) {
  static const char json[] = "{\"a\":1}";
  test_candidate_stream_state state;
  lonejson_candidate_stream_options options;
  lonejson_path_value_visitor visitor;
  lonejson_status status;
  lonejson_error error;

  memset(&state, 0, sizeof(state));
  visitor = lonejson_default_path_value_visitor();
  visitor.number_chunk = test_candidate_path_number;
  options = lonejson_default_candidate_stream_options();
  options.framing = LONEJSON_CANDIDATE_FRAMING_SINGLE_VALUE;
  options.capture_mode = LONEJSON_CANDIDATE_CAPTURE_MEMORY;
  options.path_visitor = &visitor;
  options.visitor_user = &state;
  options.candidate_begin = test_candidate_begin;
  options.candidate_end = test_candidate_end;
  options.candidate_user = &state;
  status = lonejson_visit_candidates_buffer(test_default_runtime(), json,
                                            strlen(json), &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.end_count == 1u);
  EXPECT(strcmp(state.payloads[0], "{\"a\":1}") == 0);
  EXPECT(state.paths_seen == 1u);
  EXPECT(state.numbers_seen == 1u);
}
