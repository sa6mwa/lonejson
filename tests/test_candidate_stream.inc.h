typedef struct test_candidate_stream_state {
  size_t begin_count;
  size_t end_count;
  size_t paths_seen;
  size_t numbers_seen;
  size_t bools_seen;
  size_t nulls_seen;
  size_t stop_after_end;
  size_t fail_at_begin;
  lonejson_uint64 begin_indices[8];
  lonejson_uint64 end_indices[8];
  lonejson_uint64 begin_offsets[8];
  lonejson_uint64 begin_sizes[8];
  lonejson_uint64 end_offsets[8];
  lonejson_uint64 end_sizes[8];
  char payloads[8][128];
  lonejson_uint64 payload_sizes[8];
  int payload_spilled[8];
  char payload_spool_paths[8][LONEJSON_SPOOL_TEMP_PATH_CAPACITY];
  size_t fail_at_end;
  size_t decision_count;
  size_t stop_at_decision;
  size_t fail_at_decision;
  unsigned retain_mask;
  lonejson_uint64 decision_indices[8];
  lonejson_uint64 decision_offsets[8];
  lonejson_uint64 decision_sizes[8];
} test_candidate_stream_state;

static lonejson_candidate_callback_result
test_candidate_begin(void *user, const lonejson_candidate_info *candidate,
                     lonejson_error *error) {
  test_candidate_stream_state *state = (test_candidate_stream_state *)user;
  (void)error;
  if (state->begin_count <
      sizeof(state->begin_offsets) / sizeof(state->begin_offsets[0])) {
    state->begin_indices[state->begin_count] = candidate->index;
    state->begin_offsets[state->begin_count] = candidate->stream_offset;
    state->begin_sizes[state->begin_count] = candidate->byte_size;
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
    state->end_indices[state->end_count] = candidate->index;
    state->end_offsets[state->end_count] = candidate->stream_offset;
    state->end_sizes[state->end_count] = candidate->byte_size;
    if (candidate->payload != NULL) {
      size_t copy = (size_t)candidate->payload_size;
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
      state->payload_sizes[slot] =
          (lonejson_uint64)lonejson_spooled_size(candidate->payload_spool);
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

static lonejson_candidate_capture_decision
test_candidate_capture_decision(void *user,
                                const lonejson_candidate_info *candidate,
                                lonejson_error *error) {
  test_candidate_stream_state *state = (test_candidate_stream_state *)user;
  size_t slot;

  (void)error;
  slot = state->decision_count;
  if (slot < sizeof(state->decision_indices) / sizeof(state->decision_indices[0])) {
    state->decision_indices[slot] = candidate->index;
    state->decision_offsets[slot] = candidate->stream_offset;
    state->decision_sizes[slot] = candidate->byte_size;
  }
  ++state->decision_count;
  if (state->fail_at_decision != 0u &&
      state->decision_count == state->fail_at_decision) {
    return LONEJSON_CANDIDATE_DECISION_ERROR;
  }
  if (state->stop_at_decision != 0u &&
      state->decision_count == state->stop_at_decision) {
    return LONEJSON_CANDIDATE_DECISION_STOP;
  }
  if (candidate->index < 8u &&
      (state->retain_mask & (1u << (unsigned)candidate->index)) != 0u) {
    return LONEJSON_CANDIDATE_RETAIN;
  }
  return LONEJSON_CANDIDATE_DISCARD;
}

static lonejson_read_result test_candidate_eof_reader(void *user,
                                                      unsigned char *buffer,
                                                      size_t capacity) {
  lonejson_read_result result;

  (void)user;
  (void)buffer;
  (void)capacity;
  memset(&result, 0, sizeof(result));
  result.eof = 1;
  return result;
}

typedef struct test_candidate_once_reader_state {
  const unsigned char *data;
  size_t len;
  int used;
} test_candidate_once_reader_state;

static lonejson_read_result
test_candidate_once_reader(void *user, unsigned char *buffer,
                           size_t capacity) {
  test_candidate_once_reader_state *state =
      (test_candidate_once_reader_state *)user;
  lonejson_read_result result;

  memset(&result, 0, sizeof(result));
  if (state->used) {
    result.eof = 1;
    return result;
  }
  state->used = 1;
  if (state->len > capacity) {
    result.error_code = EOVERFLOW;
    return result;
  }
  memcpy(buffer, state->data, state->len);
  result.bytes_read = state->len;
  return result;
}

static void test_candidate_stream_public_ranges_are_u64(void) {
  lonejson_candidate_info info;
  lonejson_uint64 *index = &info.index;
  lonejson_uint64 *stream_offset = &info.stream_offset;
  lonejson_uint64 *byte_size = &info.byte_size;
  lonejson_uint64 *payload_size = &info.payload_size;
  lj_candidate_info alias_info;
  lj_uint64 *alias_index = &alias_info.index;
  lj_uint64 *alias_stream_offset = &alias_info.stream_offset;
  lj_uint64 *alias_byte_size = &alias_info.byte_size;
  lj_uint64 *alias_payload_size = &alias_info.payload_size;

  (void)index;
  (void)stream_offset;
  (void)byte_size;
  (void)payload_size;
  (void)alias_index;
  (void)alias_stream_offset;
  (void)alias_byte_size;
  (void)alias_payload_size;
  EXPECT(LONEJSON_CANDIDATE_BYTE_SIZE_UNKNOWN == LONEJSON_UINT64_MAX);
  EXPECT(LJ_CANDIDATE_BYTE_SIZE_UNKNOWN ==
         LONEJSON_CANDIDATE_BYTE_SIZE_UNKNOWN);
  EXPECT(sizeof(info.index) == sizeof(lonejson_uint64));
  EXPECT(sizeof(info.stream_offset) == sizeof(lonejson_uint64));
  EXPECT(sizeof(info.byte_size) == sizeof(lonejson_uint64));
  EXPECT(sizeof(info.payload_size) == sizeof(lonejson_uint64));
  EXPECT(sizeof(info.index) == 8u);
  EXPECT(sizeof(info.stream_offset) == 8u);
  EXPECT(sizeof(info.byte_size) == 8u);
  EXPECT(sizeof(info.payload_size) == 8u);
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
  static const char json[] = " \t{\"a\":1}\r\n";
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
  EXPECT(state.begin_indices[0] == 0u);
  EXPECT(state.end_indices[0] == 0u);
  EXPECT(state.begin_offsets[0] == 2u);
  EXPECT(state.begin_sizes[0] == LONEJSON_CANDIDATE_BYTE_SIZE_UNKNOWN);
  EXPECT(state.end_offsets[0] == 2u);
  EXPECT(state.end_sizes[0] == strlen("{\"a\":1}"));
  EXPECT(state.paths_seen == 1u);
}

static void test_candidate_stream_array_items(void) {
  static const char json[] = "[ \n {\"a\":1} ,\t2 , null \n]";
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
  EXPECT(state.begin_indices[0] == 0u);
  EXPECT(state.begin_indices[1] == 1u);
  EXPECT(state.begin_indices[2] == 2u);
  EXPECT(state.end_indices[0] == 0u);
  EXPECT(state.end_indices[1] == 1u);
  EXPECT(state.end_indices[2] == 2u);
  EXPECT(state.begin_offsets[0] == 4u);
  EXPECT(state.begin_sizes[0] == LONEJSON_CANDIDATE_BYTE_SIZE_UNKNOWN);
  EXPECT(state.end_sizes[0] == 7u);
  EXPECT(state.begin_offsets[1] == 14u);
  EXPECT(state.begin_sizes[1] == LONEJSON_CANDIDATE_BYTE_SIZE_UNKNOWN);
  EXPECT(state.end_sizes[1] == 1u);
  EXPECT(state.begin_offsets[2] == 18u);
  EXPECT(state.begin_sizes[2] == LONEJSON_CANDIDATE_BYTE_SIZE_UNKNOWN);
  EXPECT(state.end_sizes[2] == 4u);
  EXPECT(state.numbers_seen == 2u);
  EXPECT(state.nulls_seen == 1u);
}

static void test_candidate_stream_offsets_above_uint32(void) {
  static const unsigned char buffered[] = " true";
  const lonejson_uint64 base = ((lonejson_uint64)0xffffffffu) + 17u;
  test_candidate_stream_state state;
  lonejson_candidate_stream_options options;
  const lonejson_runtime *runtime_state =
      lonejson__runtime_const(test_default_runtime());
  lonejson__json_cursor cursor;
  lonejson_status status;
  lonejson_error error;

  memset(&state, 0, sizeof(state));
  memset(&cursor, 0, sizeof(cursor));
  memcpy(cursor.read_buffer, buffered, sizeof(buffered) - 1u);
  cursor.read_buffer_len = sizeof(buffered) - 1u;
  cursor.read_buffer_off = 0u;
  cursor.stream_offset = base + (lonejson_uint64)cursor.read_buffer_len;
  cursor.reader = test_candidate_eof_reader;

  options = lonejson_default_candidate_stream_options();
  options.framing = LONEJSON_CANDIDATE_FRAMING_NDJSON;
  options.candidate_begin = test_candidate_begin;
  options.candidate_end = test_candidate_end;
  options.candidate_user = &state;
  status = lonejson__visit_candidates_cursor_with_limits(
      &cursor, &options, runtime_state, NULL, runtime_state->config.allocator,
      &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.begin_count == 1u);
  EXPECT(state.end_count == 1u);
  EXPECT(state.begin_indices[0] == 0u);
  EXPECT(state.end_indices[0] == 0u);
  EXPECT(state.begin_offsets[0] == base + 1u);
  EXPECT(state.end_offsets[0] == base + 1u);
  EXPECT(state.begin_sizes[0] == LONEJSON_CANDIDATE_BYTE_SIZE_UNKNOWN);
  EXPECT(state.end_sizes[0] == 4u);
}

static void test_candidate_stream_offset_overflow_fails(void) {
  static const unsigned char space[] = " ";
  test_candidate_once_reader_state reader;
  lonejson_candidate_stream_options options;
  const lonejson_runtime *runtime_state =
      lonejson__runtime_const(test_default_runtime());
  lonejson__json_cursor cursor;
  lonejson_status status;
  lonejson_error error;

  memset(&reader, 0, sizeof(reader));
  reader.data = space;
  reader.len = sizeof(space) - 1u;
  memset(&cursor, 0, sizeof(cursor));
  cursor.stream_offset = LONEJSON_UINT64_MAX;
  cursor.reader = test_candidate_once_reader;
  cursor.reader_user = &reader;

  options = lonejson_default_candidate_stream_options();
  options.framing = LONEJSON_CANDIDATE_FRAMING_NDJSON;
  status = lonejson__visit_candidates_cursor_with_limits(
      &cursor, &options, runtime_state, NULL, runtime_state->config.allocator,
      &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(strstr(error.message, "uint64 range") != NULL);
}

static void test_candidate_stream_repeated_and_auto(void) {
  static const char json[] = " \n1 \t {\"x\":2}\r\n true ";
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
  EXPECT(state.begin_indices[0] == 0u);
  EXPECT(state.begin_indices[1] == 1u);
  EXPECT(state.begin_indices[2] == 2u);
  EXPECT(state.end_indices[0] == 0u);
  EXPECT(state.end_indices[1] == 1u);
  EXPECT(state.end_indices[2] == 2u);
  EXPECT(state.begin_offsets[0] == 2u);
  EXPECT(state.begin_offsets[1] == 6u);
  EXPECT(state.begin_offsets[2] == 16u);
  EXPECT(state.begin_sizes[0] == LONEJSON_CANDIDATE_BYTE_SIZE_UNKNOWN);
  EXPECT(state.begin_sizes[1] == LONEJSON_CANDIDATE_BYTE_SIZE_UNKNOWN);
  EXPECT(state.begin_sizes[2] == LONEJSON_CANDIDATE_BYTE_SIZE_UNKNOWN);
  EXPECT(state.end_offsets[0] == state.begin_offsets[0]);
  EXPECT(state.end_offsets[1] == state.begin_offsets[1]);
  EXPECT(state.end_offsets[2] == state.begin_offsets[2]);
  EXPECT(state.end_sizes[0] == 1u);
  EXPECT(state.end_sizes[1] == strlen("{\"x\":2}"));
  EXPECT(state.end_sizes[2] == strlen("true"));
  EXPECT(state.numbers_seen == 2u);
  EXPECT(state.bools_seen == 1u);
}

static void test_candidate_stream_index_above_uint32(void) {
  static const char json[] = "true";
  const lonejson_uint64 base_index = ((lonejson_uint64)0xffffffffu) + 19u;
  test_candidate_stream_state state;
  lonejson_candidate_stream_options options;
  const lonejson_runtime *runtime_state =
      lonejson__runtime_const(test_default_runtime());
  lonejson__json_cursor cursor;
  lonejson__candidate_scan scan;
  lonejson_status status;
  lonejson_error error;

  memset(&state, 0, sizeof(state));
  memset(&cursor, 0, sizeof(cursor));
  cursor.buffer = (const unsigned char *)json;
  cursor.buffer_len = strlen(json);

  options = lonejson_default_candidate_stream_options();
  options.candidate_begin = test_candidate_begin;
  options.candidate_end = test_candidate_end;
  options.candidate_user = &state;

  memset(&scan, 0, sizeof(scan));
  scan.cursor = &cursor;
  scan.options = &options;
  scan.runtime = runtime_state;
  scan.allocator = runtime_state->config.allocator;
  scan.error = &error;
  scan.next_index = base_index;

  status = lonejson__candidate_visit_one(&scan);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.begin_count == 1u);
  EXPECT(state.end_count == 1u);
  EXPECT(state.begin_indices[0] == base_index);
  EXPECT(state.end_indices[0] == base_index);
  EXPECT(scan.next_index == base_index + 1u);
}

static void test_candidate_stream_index_overflow_fails(void) {
  static const char json[] = "true";
  test_candidate_stream_state state;
  lonejson_candidate_stream_options options;
  const lonejson_runtime *runtime_state =
      lonejson__runtime_const(test_default_runtime());
  lonejson__json_cursor cursor;
  lonejson__candidate_scan scan;
  lonejson_status status;
  lonejson_error error;

  memset(&state, 0, sizeof(state));
  memset(&cursor, 0, sizeof(cursor));
  cursor.buffer = (const unsigned char *)json;
  cursor.buffer_len = strlen(json);

  options = lonejson_default_candidate_stream_options();
  options.candidate_begin = test_candidate_begin;
  options.candidate_end = test_candidate_end;
  options.candidate_user = &state;

  memset(&scan, 0, sizeof(scan));
  scan.cursor = &cursor;
  scan.options = &options;
  scan.runtime = runtime_state;
  scan.allocator = runtime_state->config.allocator;
  scan.error = &error;
  scan.next_index = LONEJSON_UINT64_MAX;

  status = lonejson__candidate_visit_one(&scan);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(state.begin_count == 1u);
  EXPECT(state.end_count == 1u);
  EXPECT(state.begin_indices[0] == LONEJSON_UINT64_MAX);
  EXPECT(state.end_indices[0] == LONEJSON_UINT64_MAX);
  EXPECT(scan.next_index == LONEJSON_UINT64_MAX);
  EXPECT(strstr(error.message, "candidate index exceeds uint64 range") != NULL);
}

static void test_candidate_stream_callback_error_above_uint32(void) {
  static const unsigned char buffered[] = " true";
  const lonejson_uint64 base = ((lonejson_uint64)0xffffffffu) + 91u;
  test_candidate_stream_state state;
  lonejson_candidate_stream_options options;
  const lonejson_runtime *runtime_state =
      lonejson__runtime_const(test_default_runtime());
  lonejson__json_cursor cursor;
  lonejson_status status;
  lonejson_error error;

  memset(&state, 0, sizeof(state));
  state.fail_at_begin = 1u;
  memset(&cursor, 0, sizeof(cursor));
  memcpy(cursor.read_buffer, buffered, sizeof(buffered) - 1u);
  cursor.read_buffer_len = sizeof(buffered) - 1u;
  cursor.read_buffer_off = 0u;
  cursor.stream_offset = base + (lonejson_uint64)cursor.read_buffer_len;
  cursor.reader = test_candidate_eof_reader;

  options = lonejson_default_candidate_stream_options();
  options.framing = LONEJSON_CANDIDATE_FRAMING_NDJSON;
  options.candidate_begin = test_candidate_begin;
  options.candidate_end = test_candidate_end;
  options.candidate_user = &state;
  error.code = LONEJSON_STATUS_INVALID_JSON;
  error.offset = 17u;
  snprintf(error.message, sizeof(error.message), "stale callback error");

  status = lonejson__visit_candidates_cursor_with_limits(
      &cursor, &options, runtime_state, NULL, runtime_state->config.allocator,
      &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(state.begin_count == 1u);
  EXPECT(state.end_count == 0u);
  EXPECT(state.begin_offsets[0] == base + 1u);
  if (base + 1u > (lonejson_uint64)SIZE_MAX) {
    EXPECT(error.offset == SIZE_MAX);
  } else {
    EXPECT(error.offset == (size_t)(base + 1u));
  }
  EXPECT(strstr(error.message, "candidate callback failed") != NULL);
}

static void test_candidate_stream_rejects_adjacent_repeated_values(void) {
  static const char *const invalid_json[] = {"truefalse", "1true", "{}\"x\""};
  static const char *const invalid_ndjson[] = {"1,2", "[]{}"};
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
    EXPECT(state.begin_count <= 1u);
    EXPECT(state.end_count <= 1u);
  }

  for (i = 0u; i < sizeof(invalid_ndjson) / sizeof(invalid_ndjson[0]); ++i) {
    memset(&state, 0, sizeof(state));
    options = lonejson_default_candidate_stream_options();
    options.framing = LONEJSON_CANDIDATE_FRAMING_NDJSON;
    options.candidate_begin = test_candidate_begin;
    options.candidate_end = test_candidate_end;
    options.candidate_user = &state;
    status = lonejson_visit_candidates_buffer(
        test_default_runtime(), invalid_ndjson[i], strlen(invalid_ndjson[i]),
        &options, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
    EXPECT(state.begin_count == 1u);
    EXPECT(state.end_count == 1u);
    EXPECT(strstr(error.message, "whitespace between repeated JSON values") !=
           NULL);
  }
}

static void test_candidate_stream_accepts_whitespace_separators(void) {
  static const char *const valid_json[] = {"1 2", "1\n2", "1\t2", "1\r\n2",
                                           "1 \n\t\r 2"};
  test_candidate_stream_state state;
  lonejson_candidate_stream_options options;
  lonejson_status status;
  lonejson_error error;
  size_t i;

  for (i = 0u; i < sizeof(valid_json) / sizeof(valid_json[0]); ++i) {
    memset(&state, 0, sizeof(state));
    options = lonejson_default_candidate_stream_options();
    options.framing = LONEJSON_CANDIDATE_FRAMING_NDJSON;
    options.candidate_begin = test_candidate_begin;
    options.candidate_end = test_candidate_end;
    options.candidate_user = &state;
    status = lonejson_visit_candidates_buffer(
        test_default_runtime(), valid_json[i], strlen(valid_json[i]), &options,
        &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(state.begin_count == 2u);
    EXPECT(state.end_count == 2u);
    EXPECT(state.begin_offsets[1] > state.begin_offsets[0]);
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

static void test_candidate_stream_malformed_offset_above_uint32(void) {
  static const unsigned char buffered[] = " {\"a\":";
  const lonejson_uint64 base = ((lonejson_uint64)0xffffffffu) + 123u;
  test_candidate_stream_state state;
  lonejson_candidate_stream_options options;
  const lonejson_runtime *runtime_state =
      lonejson__runtime_const(test_default_runtime());
  lonejson__json_cursor cursor;
  lonejson_status status;
  lonejson_error error;
  char expected_stream_offset[32];

  memset(&state, 0, sizeof(state));
  memset(&cursor, 0, sizeof(cursor));
  memcpy(cursor.read_buffer, buffered, sizeof(buffered) - 1u);
  cursor.read_buffer_len = sizeof(buffered) - 1u;
  cursor.read_buffer_off = 0u;
  cursor.stream_offset = base + (lonejson_uint64)cursor.read_buffer_len;
  cursor.reader = test_candidate_eof_reader;

  options = lonejson_default_candidate_stream_options();
  options.framing = LONEJSON_CANDIDATE_FRAMING_SINGLE_VALUE;
  options.candidate_begin = test_candidate_begin;
  options.candidate_end = test_candidate_end;
  options.candidate_user = &state;
  status = lonejson__visit_candidates_cursor_with_limits(
      &cursor, &options, runtime_state, NULL, runtime_state->config.allocator,
      &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(state.begin_count == 1u);
  EXPECT(state.begin_offsets[0] == base + 1u);
  EXPECT(state.end_count == 0u);
  (void)lonejson__format_u64_decimal(
      expected_stream_offset, sizeof(expected_stream_offset),
      base + (lonejson_uint64)(sizeof(buffered) - 1u));
  if (base + (lonejson_uint64)(sizeof(buffered) - 1u) >
      (lonejson_uint64)SIZE_MAX) {
    EXPECT(error.offset == SIZE_MAX);
  } else {
    EXPECT(error.offset ==
           (size_t)(base + (lonejson_uint64)(sizeof(buffered) - 1u)));
  }
  EXPECT(strstr(error.message, expected_stream_offset) != NULL);
  EXPECT(strstr(error.message, "stream offset") != NULL);
  EXPECT(strstr(error.message, "candidate offset") != NULL);
}

static void test_candidate_stream_counts_pushed_back_bytes_for_limits(void) {
  static const char json[] = "true";
  test_candidate_stream_state state;
  lonejson_candidate_stream_options options;
  lonejson_config config;
  lonejson *runtime;
  lonejson_status status;
  lonejson_error error;

  config = lonejson_default_config();
  config.json_value_max_total_bytes = 3u;
  runtime = lonejson_new(&config, &error);
  EXPECT(runtime != NULL);
  if (runtime == NULL) {
    return;
  }

  memset(&state, 0, sizeof(state));
  options = lonejson_default_candidate_stream_options();
  options.framing = LONEJSON_CANDIDATE_FRAMING_NDJSON;
  options.candidate_begin = test_candidate_begin;
  options.candidate_end = test_candidate_end;
  options.candidate_user = &state;
  status = lonejson_visit_candidates_buffer(runtime, json, strlen(json),
                                            &options, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(state.begin_count == 1u);
  EXPECT(state.end_count == 0u);
  EXPECT(strstr(error.message, "maximum total byte limit") != NULL);

  config = lonejson_default_config();
  config.json_value_max_total_bytes = 4u;
  runtime = lonejson_new(&config, &error);
  EXPECT(runtime != NULL);
  if (runtime != NULL) {
    memset(&state, 0, sizeof(state));
    options = lonejson_default_candidate_stream_options();
    options.framing = LONEJSON_CANDIDATE_FRAMING_NDJSON;
    options.candidate_begin = test_candidate_begin;
    options.candidate_end = test_candidate_end;
    options.candidate_user = &state;
    status = lonejson_visit_candidates_buffer(runtime, json, strlen(json),
                                              &options, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(state.begin_count == 1u);
    EXPECT(state.end_count == 1u);
    lonejson_free(runtime);
  }
}

static void
test_candidate_stream_counts_pushed_back_bytes_across_sources(void) {
  static const char json[] = "1 true";
  test_candidate_stream_state state;
  lonejson_candidate_stream_options options;
  lonejson_config config;
  lonejson *runtime;
  lonejson_status status;
  lonejson_error error;
  test_reader_state reader;

  config = lonejson_default_config();
  config.json_value_max_total_bytes = 3u;
  runtime = lonejson_new(&config, &error);
  EXPECT(runtime != NULL);
  if (runtime == NULL) {
    return;
  }

  memset(&state, 0, sizeof(state));
  options = lonejson_default_candidate_stream_options();
  options.framing = LONEJSON_CANDIDATE_FRAMING_NDJSON;
  options.candidate_begin = test_candidate_begin;
  options.candidate_end = test_candidate_end;
  options.candidate_user = &state;
  status = lonejson_visit_candidates_buffer(runtime, json, strlen(json),
                                            &options, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(state.begin_count == 2u);
  EXPECT(state.end_count == 1u);
  EXPECT(strstr(error.message, "maximum total byte limit") != NULL);

  memset(&state, 0, sizeof(state));
  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 1u;
  status = lonejson_visit_candidates_reader(runtime, test_state_reader, &reader,
                                            &options, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(state.begin_count == 2u);
  EXPECT(state.end_count == 1u);
  EXPECT(strstr(error.message, "maximum total byte limit") != NULL);
  lonejson_free(runtime);
}

static void test_candidate_stream_numeric_delimiters_do_not_count_as_payload(
    void) {
  static const char ndjson[] = "1 2";
  static const char array_items[] = "[1]";
  test_candidate_stream_state state;
  lonejson_candidate_stream_options options;
  lonejson_config config;
  lonejson *runtime;
  lonejson_status status;
  lonejson_error error;
  test_reader_state reader;

  config = lonejson_default_config();
  config.json_value_max_total_bytes = 1u;
  runtime = lonejson_new(&config, &error);
  EXPECT(runtime != NULL);
  if (runtime == NULL) {
    return;
  }

  memset(&state, 0, sizeof(state));
  options = lonejson_default_candidate_stream_options();
  options.framing = LONEJSON_CANDIDATE_FRAMING_NDJSON;
  options.candidate_begin = test_candidate_begin;
  options.candidate_end = test_candidate_end;
  options.candidate_user = &state;
  status = lonejson_visit_candidates_buffer(runtime, ndjson, strlen(ndjson),
                                            &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.begin_count == 2u);
  EXPECT(state.end_count == 2u);
  EXPECT(state.end_sizes[0] == 1u);
  EXPECT(state.end_sizes[1] == 1u);

  memset(&state, 0, sizeof(state));
  reader.json = ndjson;
  reader.offset = 0u;
  reader.chunk_size = 1u;
  status = lonejson_visit_candidates_reader(runtime, test_state_reader, &reader,
                                            &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.begin_count == 2u);
  EXPECT(state.end_count == 2u);
  EXPECT(state.end_sizes[0] == 1u);
  EXPECT(state.end_sizes[1] == 1u);

  memset(&state, 0, sizeof(state));
  options.framing = LONEJSON_CANDIDATE_FRAMING_ARRAY_ITEMS;
  status = lonejson_visit_candidates_buffer(runtime, array_items,
                                            strlen(array_items), &options,
                                            &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.begin_count == 1u);
  EXPECT(state.end_count == 1u);
  EXPECT(state.end_sizes[0] == 1u);

  lonejson_free(runtime);
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

  memset(&state, 0, sizeof(state));
  state.fail_at_end = 1u;
  error.code = LONEJSON_STATUS_INVALID_JSON;
  error.offset = 123u;
  snprintf(error.message, sizeof(error.message), "stale end candidate error");
  status = lonejson_visit_candidates_buffer(test_default_runtime(), json,
                                            strlen(json), &options, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(state.begin_count == 1u);
  EXPECT(state.end_count == 1u);
  EXPECT(error.code == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(error.offset == state.end_offsets[0]);
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

static void test_candidate_stream_capture_recursive_array_items(void) {
  static const char json[] =
      "[ { \"id\" : \"a\" }, [ { \"id\" : \"b\" }, [ { \"id\" : \"c\" } ] ], "
      "{ \"id\" : \"d\" } ]";
  test_candidate_stream_state state;
  lonejson_candidate_stream_options options;
  lonejson_status status;
  lonejson_error error;

  memset(&state, 0, sizeof(state));
  options = lonejson_default_candidate_stream_options();
  options.framing = LONEJSON_CANDIDATE_FRAMING_RECURSIVE_ARRAY_ITEMS;
  options.capture_mode = LONEJSON_CANDIDATE_CAPTURE_MEMORY;
  options.candidate_begin = test_candidate_begin;
  options.candidate_end = test_candidate_end;
  options.candidate_user = &state;
  status = lonejson_visit_candidates_buffer(test_default_runtime(), json,
                                            strlen(json), &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.begin_count == 4u);
  EXPECT(state.end_count == 4u);
  EXPECT(state.begin_indices[0] == 0u);
  EXPECT(state.begin_indices[1] == 1u);
  EXPECT(state.begin_indices[2] == 2u);
  EXPECT(state.begin_indices[3] == 3u);
  EXPECT(strcmp(state.payloads[0], "{\"id\":\"a\"}") == 0);
  EXPECT(strcmp(state.payloads[1], "{\"id\":\"b\"}") == 0);
  EXPECT(strcmp(state.payloads[2], "{\"id\":\"c\"}") == 0);
  EXPECT(strcmp(state.payloads[3], "{\"id\":\"d\"}") == 0);
  EXPECT(state.end_offsets[0] < state.end_offsets[1]);
  EXPECT(state.end_offsets[1] < state.end_offsets[2]);
  EXPECT(state.end_offsets[2] < state.end_offsets[3]);
  EXPECT(state.end_sizes[0] != LONEJSON_CANDIDATE_BYTE_SIZE_UNKNOWN);
  EXPECT(state.end_sizes[1] != LONEJSON_CANDIDATE_BYTE_SIZE_UNKNOWN);
  EXPECT(state.end_sizes[2] != LONEJSON_CANDIDATE_BYTE_SIZE_UNKNOWN);
  EXPECT(state.end_sizes[3] != LONEJSON_CANDIDATE_BYTE_SIZE_UNKNOWN);
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

static void test_candidate_stream_capture_gated_spooled(void) {
  static const char json[] = "[ { \"a\" : 1 }, { \"b\" : 2 }, null ]";
  test_candidate_stream_state state;
  lonejson_candidate_stream_options options;
  test_reader_state reader;
  lonejson_status status;
  lonejson_error error;

  memset(&state, 0, sizeof(state));
  state.retain_mask = 1u << 1u;
  options = lonejson_default_candidate_stream_options();
  options.framing = LONEJSON_CANDIDATE_FRAMING_ARRAY_ITEMS;
  options.capture_mode = LONEJSON_CANDIDATE_CAPTURE_GATED_SPOOLED;
  options.capture_decision = test_candidate_capture_decision;
  options.capture_decision_user = &state;
  options.candidate_begin = test_candidate_begin;
  options.candidate_end = test_candidate_end;
  options.candidate_user = &state;
  status = lonejson_visit_candidates_buffer(test_default_runtime(), json,
                                            strlen(json), &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.begin_count == 3u);
  EXPECT(state.decision_count == 3u);
  EXPECT(state.end_count == 3u);
  EXPECT(state.decision_indices[0] == 0u);
  EXPECT(state.decision_indices[1] == 1u);
  EXPECT(state.decision_indices[2] == 2u);
  EXPECT(state.decision_offsets[0] == state.end_offsets[0]);
  EXPECT(state.decision_offsets[1] == state.end_offsets[1]);
  EXPECT(state.decision_offsets[2] == state.end_offsets[2]);
  EXPECT(state.decision_sizes[0] == state.end_sizes[0]);
  EXPECT(state.decision_sizes[1] == state.end_sizes[1]);
  EXPECT(state.decision_sizes[2] == state.end_sizes[2]);
  EXPECT(state.payload_sizes[0] == 0u);
  EXPECT(state.payloads[0][0] == '\0');
  EXPECT(strcmp(state.payloads[1], "{\"b\":2}") == 0);
  EXPECT(state.payload_sizes[1] == strlen("{\"b\":2}"));
  EXPECT(state.payload_sizes[2] == 0u);
  EXPECT(state.payloads[2][0] == '\0');

  memset(&state, 0, sizeof(state));
  state.retain_mask = (1u << 0u) | (1u << 1u) | (1u << 2u);
  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 2u;
  status = lonejson_visit_candidates_reader(test_default_runtime(),
                                            test_state_reader, &reader,
                                            &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.decision_count == 3u);
  EXPECT(state.end_count == 3u);
  EXPECT(strcmp(state.payloads[0], "{\"a\":1}") == 0);
  EXPECT(strcmp(state.payloads[1], "{\"b\":2}") == 0);
  EXPECT(strcmp(state.payloads[2], "null") == 0);
}

static void test_candidate_stream_capture_gated_stop_and_failure(void) {
  static const char json[] = "[ 1, 2, 3 ]";
  test_candidate_stream_state state;
  lonejson_candidate_stream_options options;
  lonejson_status status;
  lonejson_error error;

  memset(&state, 0, sizeof(state));
  state.stop_at_decision = 2u;
  state.retain_mask = 7u;
  options = lonejson_default_candidate_stream_options();
  options.framing = LONEJSON_CANDIDATE_FRAMING_ARRAY_ITEMS;
  options.capture_mode = LONEJSON_CANDIDATE_CAPTURE_GATED_SPOOLED;
  options.capture_decision = test_candidate_capture_decision;
  options.capture_decision_user = &state;
  options.candidate_begin = test_candidate_begin;
  options.candidate_end = test_candidate_end;
  options.candidate_user = &state;
  status = lonejson_visit_candidates_buffer(test_default_runtime(), json,
                                            strlen(json), &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.begin_count == 2u);
  EXPECT(state.decision_count == 2u);
  EXPECT(state.end_count == 1u);
  EXPECT(strcmp(state.payloads[0], "1") == 0);

  memset(&state, 0, sizeof(state));
  state.fail_at_decision = 1u;
  status = lonejson_visit_candidates_buffer(test_default_runtime(), json,
                                            strlen(json), &options, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(state.begin_count == 1u);
  EXPECT(state.decision_count == 1u);
  EXPECT(state.end_count == 0u);

  options = lonejson_default_candidate_stream_options();
  options.capture_mode = LONEJSON_CANDIDATE_CAPTURE_GATED_SPOOLED;
  status = lonejson_visit_candidates_buffer(test_default_runtime(), "1", 1u,
                                            &options, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
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

typedef struct test_candidate_transform_state {
  size_t number_chunks;
  size_t transforms_after_number_chunks;
  size_t old_number_seen;
  size_t old_string_seen;
  size_t old_bool_seen;
  size_t transform_metadata_seen;
  size_t gated_metadata_seen;
  size_t insert_object_begin_seen;
  size_t insert_before_member_seen;
  size_t insert_after_member_seen;
  size_t insert_object_end_seen;
  size_t insert_array_object_begin_seen;
  size_t insert_array_object_end_seen;
  size_t insert_array_member_seen;
  int fail_replace;
} test_candidate_transform_state;

typedef struct test_candidate_transform_gate_policy {
  int output;
  int mutate;
} test_candidate_transform_gate_policy;

typedef struct test_candidate_transform_gate_state {
  test_buffer_sink *sink;
  size_t status_candidate;
  char status_values[8][16];
  size_t status_value_lengths[8];
  test_candidate_transform_gate_policy policies[8];
  size_t decision_count;
  size_t root_policy_seen;
  size_t status_policy_seen;
  size_t replace_policy_seen;
  size_t transform_count;
  size_t projected_replay_seen;
  size_t source_replay_seen;
  size_t root_relationship_seen;
  size_t object_member_relationship_seen;
  size_t projected_out_member_seen;
  size_t decision_sink_lengths[8];
  int matches_only;
  int expect_projected;
  int stop_at_decision;
  int fail_at_decision;
} test_candidate_transform_gate_state;

typedef struct test_candidate_transform_projected_state {
  test_candidate_transform_gate_state gate;
  size_t count_seen;
  size_t count_replaced;
  size_t root_hello_inserted;
  size_t synthetic_object_begin_seen;
  size_t synthetic_object_end_seen;
  size_t array_placeholder_seen;
  size_t array_placeholder_replaced;
  size_t array_relationship_seen;
  size_t old_scalar_policy_count;
  size_t old_scalar_complete_count;
  size_t streaming_string_seen;
  int insert_hello;
  int mutate_count;
  int mutate_placeholder;
} test_candidate_transform_projected_state;

typedef struct test_candidate_transform_failing_reader_state {
  const char *json;
  size_t offset;
  size_t chunk_size;
  size_t fail_after;
} test_candidate_transform_failing_reader_state;

static lonejson_read_result test_candidate_transform_failing_reader(
    void *user, unsigned char *buffer, size_t capacity) {
  test_candidate_transform_failing_reader_state *st =
      (test_candidate_transform_failing_reader_state *)user;
  lonejson_read_result rr;
  size_t remaining = strlen(st->json) - st->offset;
  size_t chunk = st->chunk_size;

  memset(&rr, 0, sizeof(rr));
  if (st->offset >= st->fail_after) {
    rr.error_code = EIO;
    return rr;
  }
  if (remaining == 0u) {
    rr.eof = 1;
    return rr;
  }
  if (chunk > remaining) {
    chunk = remaining;
  }
  if (chunk > capacity) {
    chunk = capacity;
  }
  if (st->offset + chunk > st->fail_after) {
    chunk = st->fail_after - st->offset;
  }
  memcpy(buffer, st->json + st->offset, chunk);
  st->offset += chunk;
  rr.bytes_read = chunk;
  return rr;
}

static int test_candidate_transform_path_is(
    const lonejson_candidate_transform_event *event, const char *name) {
  const lonejson_path_segment *segment;

  if (event == NULL || event->path == NULL ||
      event->path->segment_count == 0u) {
    return 0;
  }
  segment = &event->path->segments[event->path->segment_count - 1u];
  return strlen(name) == segment->len &&
         memcmp(segment->data, name, segment->len) == 0;
}

static int test_candidate_transform_path_depth_is(
    const lonejson_candidate_transform_event *event, size_t depth,
    const char *name) {
  const lonejson_path_segment *segment;

  if (event == NULL || event->path == NULL ||
      event->path->segment_count <= depth) {
    return 0;
  }
  segment = &event->path->segments[depth];
  return strlen(name) == segment->len &&
         memcmp(segment->data, name, segment->len) == 0;
}

static int test_candidate_transform_path_index_is(
    const lonejson_candidate_transform_event *event, size_t depth,
    const char *index) {
  const lonejson_path_segment *segment;

  if (event == NULL || event->path == NULL ||
      event->path->segment_count <= depth) {
    return 0;
  }
  segment = &event->path->segments[depth];
  return strlen(index) == segment->len &&
         memcmp(segment->data, index, segment->len) == 0;
}

static lonejson_status test_candidate_transform_gate_observe_status(
    void *user, const lonejson_value_path *path, const char *data, size_t len,
    lonejson_error *error) {
  test_candidate_transform_gate_state *state =
      (test_candidate_transform_gate_state *)user;
  size_t copy;

  (void)error;
  if (path == NULL || path->segment_count != 1u ||
      path->segments[0].len != 6u ||
      memcmp(path->segments[0].data, "status", 6u) != 0 ||
      state->status_candidate >=
          sizeof(state->status_values) / sizeof(state->status_values[0])) {
    return LONEJSON_STATUS_OK;
  }
  copy = len;
  if (copy >= sizeof(state->status_values[state->status_candidate]) -
                  state->status_value_lengths[state->status_candidate]) {
    copy = sizeof(state->status_values[state->status_candidate]) -
           state->status_value_lengths[state->status_candidate] - 1u;
  }
  memcpy(state->status_values[state->status_candidate] +
             state->status_value_lengths[state->status_candidate],
         data, copy);
  state->status_value_lengths[state->status_candidate] += copy;
  state->status_values[state->status_candidate]
                      [state->status_value_lengths[state->status_candidate]] =
      '\0';
  return LONEJSON_STATUS_OK;
}

static lonejson_candidate_transform_candidate_policy
test_candidate_transform_gate_decision(
    void *user, const lonejson_candidate_info *candidate,
    const lonejson_candidate_transform_candidate_info *transform_candidate,
    lonejson_error *error) {
  test_candidate_transform_gate_state *state =
      (test_candidate_transform_gate_state *)user;
  lonejson_candidate_transform_candidate_policy policy;
  test_candidate_transform_gate_policy *candidate_policy;
  size_t slot;
  int matched;

  memset(&policy, 0, sizeof(policy));
  slot = state->decision_count;
  if (slot < sizeof(state->decision_sink_lengths) /
                 sizeof(state->decision_sink_lengths[0])) {
    state->decision_sink_lengths[slot] =
        state->sink != NULL ? state->sink->length : 0u;
  }
  ++state->decision_count;
  if (state->fail_at_decision != 0 &&
      state->decision_count == (size_t)state->fail_at_decision) {
    policy.decision = LONEJSON_CANDIDATE_TRANSFORM_CANDIDATE_ERROR;
    (void)lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u,
                              0u, "test gated decision failed");
    return policy;
  }
  if (state->stop_at_decision != 0 &&
      state->decision_count == (size_t)state->stop_at_decision) {
    policy.decision = LONEJSON_CANDIDATE_TRANSFORM_CANDIDATE_STOP;
    return policy;
  }
  EXPECT(candidate != NULL);
  EXPECT(transform_candidate != NULL);
  EXPECT(transform_candidate->mode ==
         LONEJSON_CANDIDATE_TRANSFORM_MODE_GATED_SPOOLED);
  EXPECT(transform_candidate->logical_index == candidate->index);
  EXPECT(transform_candidate->byte_size !=
         LONEJSON_CANDIDATE_BYTE_SIZE_UNKNOWN);
  EXPECT(transform_candidate->bytes_spooled != 0u);
  EXPECT(transform_candidate->replay_count == 0u);
  if (candidate == NULL || candidate->index >= 8u) {
    policy.decision = LONEJSON_CANDIDATE_TRANSFORM_CANDIDATE_ERROR;
    return policy;
  }
  slot = (size_t)candidate->index;
  candidate_policy = &state->policies[slot];
  matched = strcmp(state->status_values[slot], "open") == 0;
  candidate_policy->mutate = matched;
  candidate_policy->output = matched || !state->matches_only;
  policy.candidate_policy = candidate_policy;
  policy.decision = candidate_policy->output
                        ? LONEJSON_CANDIDATE_TRANSFORM_CANDIDATE_EMIT
                        : LONEJSON_CANDIDATE_TRANSFORM_CANDIDATE_DROP;
  state->status_candidate++;
  return policy;
}

static lonejson_candidate_transform_action
test_candidate_transform_gate_transform(
    void *user, const lonejson_candidate_transform_event *event,
    lonejson_error *error) {
  test_candidate_transform_gate_state *state =
      (test_candidate_transform_gate_state *)user;
  test_candidate_transform_gate_policy *policy;

  (void)error;
  ++state->transform_count;
  EXPECT(event != NULL);
  EXPECT(event->candidate_policy != NULL);
  if (event != NULL) {
    if (state->expect_projected) {
      EXPECT(event->origin ==
             LONEJSON_CANDIDATE_TRANSFORM_EVENT_PROJECTED_SOURCE);
      EXPECT(event->phase ==
             LONEJSON_CANDIDATE_TRANSFORM_EVENT_PHASE_PROJECTED_REPLAY);
    } else {
      EXPECT(event->origin == LONEJSON_CANDIDATE_TRANSFORM_EVENT_REPLAY);
      EXPECT(event->phase == LONEJSON_CANDIDATE_TRANSFORM_EVENT_PHASE_REPLAY);
    }
  }
  policy = (test_candidate_transform_gate_policy *)event->candidate_policy;
  if (event != NULL && event->path != NULL &&
      event->path->segment_count == 0u) {
    ++state->root_policy_seen;
    EXPECT(event->relationship == LONEJSON_CANDIDATE_TRANSFORM_EVENT_ROOT);
    ++state->root_relationship_seen;
  } else if (event != NULL) {
    EXPECT(event->relationship ==
           LONEJSON_CANDIDATE_TRANSFORM_EVENT_OBJECT_MEMBER);
    ++state->object_member_relationship_seen;
  }
  if (event != NULL &&
      event->phase ==
          LONEJSON_CANDIDATE_TRANSFORM_EVENT_PHASE_PROJECTED_REPLAY) {
    ++state->projected_replay_seen;
  } else if (event != NULL &&
             event->phase == LONEJSON_CANDIDATE_TRANSFORM_EVENT_PHASE_REPLAY) {
    ++state->source_replay_seen;
  }
  if (test_candidate_transform_path_is(event, "big")) {
    ++state->projected_out_member_seen;
    if (state->expect_projected) {
      EXPECT(0 && "projected replay saw projected-out member");
      return LONEJSON_CANDIDATE_TRANSFORM_ERROR;
    }
  }
  if (test_candidate_transform_path_is(event, "status")) {
    ++state->status_policy_seen;
    if (policy != NULL && policy->mutate) {
      return LONEJSON_CANDIDATE_TRANSFORM_REPLACE;
    }
  }
  return LONEJSON_CANDIDATE_TRANSFORM_KEEP;
}

static lonejson_status test_candidate_transform_gate_replace(
    void *user, const lonejson_candidate_transform_event *event,
    lonejson_writer *writer, lonejson_error *error) {
  test_candidate_transform_gate_state *state =
      (test_candidate_transform_gate_state *)user;
  test_candidate_transform_gate_policy *policy;

  EXPECT(event != NULL);
  EXPECT(event->candidate_policy != NULL);
  policy = event != NULL
               ? (test_candidate_transform_gate_policy *)event->candidate_policy
               : NULL;
  EXPECT(policy != NULL && policy->mutate);
  ++state->replace_policy_seen;
  return lonejson_writer_string(writer, "done", 4u, error);
}

static lonejson_candidate_transform_candidate_policy
test_candidate_transform_projected_decision(
    void *user, const lonejson_candidate_info *candidate,
    const lonejson_candidate_transform_candidate_info *transform_candidate,
    lonejson_error *error) {
  test_candidate_transform_projected_state *state =
      (test_candidate_transform_projected_state *)user;

  return test_candidate_transform_gate_decision(
      &state->gate, candidate, transform_candidate, error);
}

static lonejson_candidate_transform_action
test_candidate_transform_projected_transform(
    void *user, const lonejson_candidate_transform_event *event,
    lonejson_error *error) {
  test_candidate_transform_projected_state *state =
      (test_candidate_transform_projected_state *)user;
  test_candidate_transform_gate_policy *policy;

  (void)error;
  EXPECT(event != NULL);
  EXPECT(event->phase ==
         LONEJSON_CANDIDATE_TRANSFORM_EVENT_PHASE_PROJECTED_REPLAY);
  EXPECT(event->candidate_policy != NULL);
  policy = event != NULL
               ? (test_candidate_transform_gate_policy *)event->candidate_policy
               : NULL;
  if (event != NULL && event->relationship ==
                           LONEJSON_CANDIDATE_TRANSFORM_EVENT_ARRAY_ELEMENT) {
    ++state->array_relationship_seen;
  }
  if (test_candidate_transform_path_is(event, "count")) {
    ++state->count_seen;
    EXPECT(event->origin ==
           LONEJSON_CANDIDATE_TRANSFORM_EVENT_PROJECTED_SOURCE);
    if (policy != NULL && policy->mutate && state->mutate_count) {
      return LONEJSON_CANDIDATE_TRANSFORM_REPLACE;
    }
  }
  if (event != NULL && event->value_type == LONEJSON_VALUE_STRING) {
    EXPECT(event->old_value == NULL);
    ++state->streaming_string_seen;
  }
  if (test_candidate_transform_path_depth_is(event, 0u, "missing")) {
    if (event->path != NULL && event->path->segment_count == 1u &&
        event->value_type == LONEJSON_VALUE_OBJECT) {
      ++state->synthetic_object_begin_seen;
      EXPECT(event->origin ==
             LONEJSON_CANDIDATE_TRANSFORM_EVENT_PROJECTED_SYNTHETIC);
    }
  }
  if (test_candidate_transform_path_depth_is(event, 0u, "arr") &&
      test_candidate_transform_path_index_is(event, 1u, "1") &&
      event->value_type == LONEJSON_VALUE_NULL) {
    ++state->array_placeholder_seen;
    EXPECT(event->origin ==
           LONEJSON_CANDIDATE_TRANSFORM_EVENT_PROJECTED_SYNTHETIC);
    if (state->mutate_placeholder) {
      return LONEJSON_CANDIDATE_TRANSFORM_REPLACE;
    }
  }
  return LONEJSON_CANDIDATE_TRANSFORM_KEEP;
}

static lonejson_candidate_transform_old_scalar_mode
test_candidate_transform_projected_old_scalar(
    void *user, const lonejson_candidate_transform_event *event,
    lonejson_error *error) {
  test_candidate_transform_projected_state *state =
      (test_candidate_transform_projected_state *)user;

  (void)error;
  EXPECT(event != NULL);
  EXPECT(event->old_value == NULL);
  ++state->old_scalar_policy_count;
  if (test_candidate_transform_path_is(event, "count")) {
    ++state->old_scalar_complete_count;
    return LONEJSON_CANDIDATE_TRANSFORM_OLD_SCALAR_COMPLETE;
  }
  return LONEJSON_CANDIDATE_TRANSFORM_OLD_SCALAR_NONE;
}

static lonejson_status test_candidate_transform_projected_replace(
    void *user, const lonejson_candidate_transform_event *event,
    lonejson_writer *writer, lonejson_error *error) {
  test_candidate_transform_projected_state *state =
      (test_candidate_transform_projected_state *)user;

  EXPECT(event != NULL);
  EXPECT(writer != NULL);
  if (test_candidate_transform_path_is(event, "count")) {
    ++state->count_replaced;
    EXPECT(event->origin ==
           LONEJSON_CANDIDATE_TRANSFORM_EVENT_PROJECTED_SOURCE);
    EXPECT(event->old_value != NULL);
    EXPECT(event->old_value->value_type == LONEJSON_VALUE_NUMBER);
    EXPECT(event->old_value->len == 1u);
    EXPECT(event->old_value->data != NULL && event->old_value->data[0] == '1');
    return lonejson_writer_number_text(writer, "2", 1u, error);
  }
  if (test_candidate_transform_path_depth_is(event, 0u, "arr") &&
      test_candidate_transform_path_index_is(event, 1u, "1")) {
    ++state->array_placeholder_replaced;
    EXPECT(event->origin ==
           LONEJSON_CANDIDATE_TRANSFORM_EVENT_PROJECTED_SYNTHETIC);
    EXPECT(event->relationship ==
           LONEJSON_CANDIDATE_TRANSFORM_EVENT_ARRAY_ELEMENT);
    return lonejson_writer_string(writer, "filled", 6u, error);
  }
  return lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u, 0u,
                             "unexpected projected replacement path");
}

static lonejson_status test_candidate_transform_projected_insert(
    void *user, const lonejson_candidate_transform_event *event,
    lonejson_writer *writer, lonejson_error *error) {
  test_candidate_transform_projected_state *state =
      (test_candidate_transform_projected_state *)user;
  test_candidate_transform_gate_policy *policy;

  EXPECT(event != NULL);
  EXPECT(writer != NULL);
  policy = event != NULL
               ? (test_candidate_transform_gate_policy *)event->candidate_policy
               : NULL;
  if (event != NULL &&
      event->insert_phase ==
          LONEJSON_CANDIDATE_TRANSFORM_INSERT_OBJECT_END &&
      event->path != NULL && event->path->segment_count == 0u &&
      policy != NULL && policy->mutate && state->insert_hello) {
    ++state->root_hello_inserted;
    EXPECT(lonejson_writer_key(writer, "hello", 5u, error) ==
           LONEJSON_STATUS_OK);
    return lonejson_writer_string(writer, "world", 5u, error);
  }
  if (event != NULL &&
      event->insert_phase ==
          LONEJSON_CANDIDATE_TRANSFORM_INSERT_OBJECT_END &&
      test_candidate_transform_path_depth_is(event, 0u, "missing")) {
    ++state->synthetic_object_end_seen;
    EXPECT(event->origin ==
           LONEJSON_CANDIDATE_TRANSFORM_EVENT_PROJECTED_SYNTHETIC);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_candidate_transform_action test_candidate_transform_decide(
    void *user, const lonejson_candidate_transform_event *event,
    lonejson_error *error) {
  (void)error;
  if (user != NULL && event != NULL && event->transform_candidate != NULL) {
    test_candidate_transform_state *state =
        (test_candidate_transform_state *)user;
    EXPECT(event->transform_candidate->mode ==
               LONEJSON_CANDIDATE_TRANSFORM_MODE_STREAMING ||
           event->transform_candidate->mode ==
               LONEJSON_CANDIDATE_TRANSFORM_MODE_GATED_SPOOLED);
    EXPECT(event->transform_candidate->physical_index ==
           event->candidate->index);
    EXPECT(event->transform_candidate->logical_index ==
           event->candidate->index);
    if (event->transform_candidate->mode ==
        LONEJSON_CANDIDATE_TRANSFORM_MODE_STREAMING) {
      EXPECT(event->transform_candidate->byte_size ==
             LONEJSON_CANDIDATE_BYTE_SIZE_UNKNOWN);
      EXPECT(event->transform_candidate->gated_spooled == 0);
    } else {
      EXPECT(event->transform_candidate->byte_size !=
             LONEJSON_CANDIDATE_BYTE_SIZE_UNKNOWN);
      EXPECT(event->transform_candidate->gated_spooled != 0);
      EXPECT(event->transform_candidate->bytes_spooled != 0u);
      EXPECT(event->transform_candidate->replay_count == 1u);
      ++state->gated_metadata_seen;
    }
    ++state->transform_metadata_seen;
  }
  if (event->value_type == LONEJSON_VALUE_NUMBER) {
    test_candidate_transform_state *state =
        (test_candidate_transform_state *)user;
    if (state != NULL && event->old_value != NULL &&
        event->old_value->value_type == LONEJSON_VALUE_NUMBER &&
        event->old_value->data != NULL && event->old_value->len != 0u) {
      ++state->old_number_seen;
    }
    if (state != NULL && state->number_chunks != 0u) {
      ++state->transforms_after_number_chunks;
    }
  }
  if (test_candidate_transform_path_is(event, "drop")) {
    return LONEJSON_CANDIDATE_TRANSFORM_DROP;
  }
  if (event->value_type == LONEJSON_VALUE_NUMBER) {
    return LONEJSON_CANDIDATE_TRANSFORM_REPLACE;
  }
  if (event->value_type == LONEJSON_VALUE_STRING &&
      event->old_value != NULL &&
      event->old_value->value_type == LONEJSON_VALUE_STRING &&
      event->old_value->data != NULL && event->old_value->len == 1u &&
      event->old_value->data[0] == 'x') {
    test_candidate_transform_state *state =
        (test_candidate_transform_state *)user;
    ++state->old_string_seen;
  }
  if (event->value_type == LONEJSON_VALUE_BOOL &&
      event->old_value != NULL &&
      event->old_value->value_type == LONEJSON_VALUE_BOOL &&
      event->old_value->boolean_value) {
    test_candidate_transform_state *state =
        (test_candidate_transform_state *)user;
    ++state->old_bool_seen;
  }
  return LONEJSON_CANDIDATE_TRANSFORM_KEEP;
}

static lonejson_candidate_transform_action test_candidate_transform_drop_only(
    void *user, const lonejson_candidate_transform_event *event,
    lonejson_error *error) {
  (void)user;
  (void)error;
  if (test_candidate_transform_path_is(event, "drop")) {
    return LONEJSON_CANDIDATE_TRANSFORM_DROP;
  }
  return LONEJSON_CANDIDATE_TRANSFORM_KEEP;
}

static lonejson_candidate_transform_action
test_candidate_transform_drop_array_index_one(
    void *user, const lonejson_candidate_transform_event *event,
    lonejson_error *error) {
  (void)user;
  (void)error;
  if (event->path != NULL && event->path->segment_count == 1u &&
      event->path->segments[0].len == 1u &&
      event->path->segments[0].data[0] == '1') {
    return LONEJSON_CANDIDATE_TRANSFORM_DROP;
  }
  return LONEJSON_CANDIDATE_TRANSFORM_KEEP;
}

static lonejson_candidate_transform_action test_candidate_transform_drop_id(
    void *user, const lonejson_candidate_transform_event *event,
    lonejson_error *error) {
  (void)user;
  (void)error;
  if (test_candidate_transform_path_is(event, "id")) {
    return LONEJSON_CANDIDATE_TRANSFORM_DROP;
  }
  return LONEJSON_CANDIDATE_TRANSFORM_KEEP;
}

static lonejson_candidate_transform_action test_candidate_transform_drop_a(
    void *user, const lonejson_candidate_transform_event *event,
    lonejson_error *error) {
  (void)user;
  (void)error;
  if (test_candidate_transform_path_is(event, "a")) {
    return LONEJSON_CANDIDATE_TRANSFORM_DROP;
  }
  return LONEJSON_CANDIDATE_TRANSFORM_KEEP;
}

static lonejson_candidate_transform_action test_candidate_transform_replace_a(
    void *user, const lonejson_candidate_transform_event *event,
    lonejson_error *error) {
  (void)user;
  (void)error;
  if (test_candidate_transform_path_is(event, "a")) {
    return LONEJSON_CANDIDATE_TRANSFORM_REPLACE;
  }
  return LONEJSON_CANDIDATE_TRANSFORM_KEEP;
}

static lonejson_candidate_transform_action test_candidate_transform_replace_root(
    void *user, const lonejson_candidate_transform_event *event,
    lonejson_error *error) {
  (void)user;
  (void)error;
  if (event->path != NULL && event->path->segment_count == 0u) {
    return LONEJSON_CANDIDATE_TRANSFORM_REPLACE;
  }
  return LONEJSON_CANDIDATE_TRANSFORM_KEEP;
}

static lonejson_candidate_transform_action
test_candidate_transform_stop_root_array(
    void *user, const lonejson_candidate_transform_event *event,
    lonejson_error *error) {
  (void)user;
  (void)error;
  if (event->path != NULL && event->path->segment_count == 0u &&
      event->value_type == LONEJSON_VALUE_ARRAY) {
    return LONEJSON_CANDIDATE_TRANSFORM_STOP;
  }
  return LONEJSON_CANDIDATE_TRANSFORM_KEEP;
}

static lonejson_candidate_transform_action
test_candidate_transform_stop_root_object(
    void *user, const lonejson_candidate_transform_event *event,
    lonejson_error *error) {
  (void)user;
  (void)error;
  if (event->path != NULL && event->path->segment_count == 0u &&
      event->value_type == LONEJSON_VALUE_OBJECT) {
    return LONEJSON_CANDIDATE_TRANSFORM_STOP;
  }
  return LONEJSON_CANDIDATE_TRANSFORM_KEEP;
}

static lonejson_candidate_transform_action test_candidate_transform_stop_at_a(
    void *user, const lonejson_candidate_transform_event *event,
    lonejson_error *error) {
  (void)user;
  (void)error;
  if (test_candidate_transform_path_is(event, "a") &&
      event->value_type == LONEJSON_VALUE_ARRAY) {
    return LONEJSON_CANDIDATE_TRANSFORM_STOP;
  }
  return LONEJSON_CANDIDATE_TRANSFORM_KEEP;
}

static lonejson_status test_candidate_transform_replace(
    void *user, const lonejson_candidate_transform_event *event,
    lonejson_writer *writer, lonejson_error *error) {
  test_candidate_transform_state *state =
      (test_candidate_transform_state *)user;
  char number[32];
  long old_number;

  if (state->fail_replace) {
    return lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u,
                               0u, "test transform replacement failed");
  }
  if (event->old_value == NULL ||
      event->old_value->value_type != LONEJSON_VALUE_NUMBER ||
      event->old_value->data == NULL || event->old_value->len >= 20u) {
    return lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u,
                               0u, "test transform old number missing");
  }
  memcpy(number, event->old_value->data, event->old_value->len);
  number[event->old_value->len] = '\0';
  old_number = strtol(number, NULL, 10);
  sprintf(number, "%ld", old_number + 1L);
  return lonejson_writer_number_text(writer, number, strlen(number), error);
}

static lonejson_status test_candidate_transform_observe_number(
    void *user, const lonejson_value_path *path, const char *data, size_t len,
    lonejson_error *error) {
  test_candidate_transform_state *state =
      (test_candidate_transform_state *)user;
  (void)path;
  (void)data;
  (void)len;
  (void)error;
  ++state->number_chunks;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_candidate_transform_replace_with_nine(
    void *user, const lonejson_candidate_transform_event *event,
    lonejson_writer *writer, lonejson_error *error) {
  (void)user;
  (void)event;
  return lonejson_writer_number_text(writer, "9", 1u, error);
}

static lonejson_candidate_transform_action test_candidate_transform_keep_stream(
    void *user, const lonejson_candidate_transform_event *event,
    lonejson_error *error) {
  test_candidate_transform_state *state =
      (test_candidate_transform_state *)user;
  (void)error;
  if (event->value_type == LONEJSON_VALUE_STRING) {
    EXPECT(event->old_value == NULL);
    ++state->transform_metadata_seen;
  }
  return LONEJSON_CANDIDATE_TRANSFORM_KEEP;
}

static lonejson_status test_candidate_transform_insert_members(
    void *user, const lonejson_candidate_transform_event *event,
    lonejson_writer *writer, lonejson_error *error) {
  test_candidate_transform_state *state =
      (test_candidate_transform_state *)user;

  EXPECT(event != NULL);
  EXPECT(writer != NULL);
  if (event == NULL || writer == NULL) {
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  if (event->insert_phase ==
      LONEJSON_CANDIDATE_TRANSFORM_INSERT_OBJECT_BEGIN) {
    ++state->insert_object_begin_seen;
    EXPECT(event->object_key == NULL);
    EXPECT(lonejson_writer_key(writer, "first", 5u, error) ==
           LONEJSON_STATUS_OK);
    return lonejson_writer_number_text(writer, "0", 1u, error);
  }
  if (event->insert_phase ==
      LONEJSON_CANDIDATE_TRANSFORM_INSERT_BEFORE_MEMBER) {
    if (event->object_key_len == 1u && event->object_key != NULL &&
        event->object_key[0] == 'a') {
      ++state->insert_before_member_seen;
      EXPECT(lonejson_writer_key(writer, "before_a", 8u, error) ==
             LONEJSON_STATUS_OK);
      return lonejson_writer_number_text(writer, "1", 1u, error);
    }
    return LONEJSON_STATUS_OK;
  }
  if (event->insert_phase ==
      LONEJSON_CANDIDATE_TRANSFORM_INSERT_AFTER_MEMBER) {
    if (event->object_key_len == 1u && event->object_key != NULL &&
        event->object_key[0] == 'a') {
      ++state->insert_after_member_seen;
      EXPECT(lonejson_writer_key(writer, "after_a", 7u, error) ==
             LONEJSON_STATUS_OK);
      return lonejson_writer_number_text(writer, "2", 1u, error);
    }
    return LONEJSON_STATUS_OK;
  }
  if (event->insert_phase ==
      LONEJSON_CANDIDATE_TRANSFORM_INSERT_OBJECT_END) {
    ++state->insert_object_end_seen;
    EXPECT(lonejson_writer_key(writer, "last", 4u, error) ==
           LONEJSON_STATUS_OK);
    EXPECT(lonejson_writer_begin_object(writer, error) == LONEJSON_STATUS_OK);
    EXPECT(lonejson_writer_key(writer, "nested", 6u, error) ==
           LONEJSON_STATUS_OK);
    EXPECT(lonejson_writer_bool(writer, 1, error) == LONEJSON_STATUS_OK);
    return lonejson_writer_end_object(writer, error);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_candidate_transform_insert_relationships(
    void *user, const lonejson_candidate_transform_event *event,
    lonejson_writer *writer, lonejson_error *error) {
  test_candidate_transform_state *state =
      (test_candidate_transform_state *)user;

  (void)writer;
  (void)error;
  EXPECT(event != NULL);
  if (event == NULL) {
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  if (event->insert_phase ==
      LONEJSON_CANDIDATE_TRANSFORM_INSERT_OBJECT_BEGIN) {
    ++state->insert_object_begin_seen;
    if (event->relationship ==
        LONEJSON_CANDIDATE_TRANSFORM_EVENT_ARRAY_ELEMENT) {
      ++state->insert_array_object_begin_seen;
    }
    return LONEJSON_STATUS_OK;
  }
  if (event->insert_phase ==
      LONEJSON_CANDIDATE_TRANSFORM_INSERT_BEFORE_MEMBER) {
    ++state->insert_before_member_seen;
    EXPECT(event->relationship ==
           LONEJSON_CANDIDATE_TRANSFORM_EVENT_OBJECT_MEMBER);
    ++state->insert_array_member_seen;
    return LONEJSON_STATUS_OK;
  }
  if (event->insert_phase ==
      LONEJSON_CANDIDATE_TRANSFORM_INSERT_AFTER_MEMBER) {
    ++state->insert_after_member_seen;
    EXPECT(event->relationship ==
           LONEJSON_CANDIDATE_TRANSFORM_EVENT_OBJECT_MEMBER);
    ++state->insert_array_member_seen;
    return LONEJSON_STATUS_OK;
  }
  if (event->insert_phase ==
      LONEJSON_CANDIDATE_TRANSFORM_INSERT_OBJECT_END) {
    ++state->insert_object_end_seen;
    if (event->relationship ==
        LONEJSON_CANDIDATE_TRANSFORM_EVENT_ARRAY_ELEMENT) {
      ++state->insert_array_object_end_seen;
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_candidate_transform_insert_before_each_member(
    void *user, const lonejson_candidate_transform_event *event,
    lonejson_writer *writer, lonejson_error *error) {
  test_candidate_transform_state *state =
      (test_candidate_transform_state *)user;

  EXPECT(event != NULL);
  EXPECT(writer != NULL);
  if (event == NULL || writer == NULL) {
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  if (event->insert_phase !=
      LONEJSON_CANDIDATE_TRANSFORM_INSERT_BEFORE_MEMBER) {
    return LONEJSON_STATUS_OK;
  }
  ++state->insert_before_member_seen;
  EXPECT(event->object_key != NULL);
  if (event->object_key_len == 4u &&
      memcmp(event->object_key, "drop", 4u) == 0) {
    EXPECT(0 && "insert callback saw projected-out member");
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  EXPECT(lonejson_writer_key(writer, "before", 6u, error) ==
         LONEJSON_STATUS_OK);
  return lonejson_writer_number_text(writer, "0", 1u, error);
}

static void test_candidate_stream_transform_pass_drop_replace(void) {
  static const char json[] =
      "{\"keep\":1,\"drop\":2,\"s\":\"x\"}\n[3,true]";
  unsigned char out[128];
  test_buffer_sink sink;
  test_candidate_transform_state state;
  lonejson_candidate_transform_options options;
  lonejson_candidate_transform_result result;
  lj_candidate_transform_result short_result;
  lj_candidate_transform_mode short_mode;
  lonejson_path_value_visitor observer;
  lonejson_status status;
  lonejson_error error;

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  memset(&result, 0xff, sizeof(result));
  memset(&short_result, 0, sizeof(short_result));
  short_mode = LJ_CANDIDATE_TRANSFORM_MODE_STREAMING;
  EXPECT(short_mode == LONEJSON_CANDIDATE_TRANSFORM_MODE_STREAMING);
  EXPECT(lj_status_string(LJ_STATUS_UNSUPPORTED) != NULL);
  sink.buffer = out;
  sink.capacity = sizeof(out);
  observer = lonejson_default_path_value_visitor();
  observer.number_chunk = test_candidate_transform_observe_number;
  memset(&options, 0, sizeof(options));
  options.framing = LONEJSON_CANDIDATE_FRAMING_NDJSON;
  options.output_framing = LONEJSON_CANDIDATE_TRANSFORM_OUTPUT_NDJSON;
  options.old_scalar_mode = LONEJSON_CANDIDATE_TRANSFORM_OLD_SCALAR_COMPLETE;
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.observer = &observer;
  options.observer_user = &state;
  options.transform = test_candidate_transform_decide;
  options.replace = test_candidate_transform_replace;
  options.transform_user = &state;
  options.result = &result;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out, "{\"keep\":2,\"s\":\"x\"}\n[4,true]\n") ==
         0);
  EXPECT(state.number_chunks == 3u);
  EXPECT(state.transforms_after_number_chunks == 3u);
  EXPECT(state.old_number_seen == 3u);
  EXPECT(state.old_string_seen == 1u);
  EXPECT(state.old_bool_seen == 1u);
  EXPECT(state.transform_metadata_seen != 0u);
  EXPECT(result.candidates_streamed == 2u);
  EXPECT(result.candidates_spooled == 0u);
  EXPECT(result.candidates_spilled == 0u);
  EXPECT(result.total_bytes_spooled == 0u);
  EXPECT(result.total_spill_bytes == 0u);
  EXPECT(result.candidates_replayed == 0u);
  EXPECT(result.last_candidate.mode ==
         LONEJSON_CANDIDATE_TRANSFORM_MODE_STREAMING);
  EXPECT(result.last_candidate.physical_index == 1u);
  EXPECT(result.last_candidate.logical_index == 1u);
  EXPECT(result.last_candidate.byte_size !=
         LONEJSON_CANDIDATE_BYTE_SIZE_UNKNOWN);
  EXPECT(result.last_candidate.gated_spooled == 0);
  EXPECT(short_result.candidates_streamed == 0u);
}

static void test_candidate_stream_transform_fragmented_reader(void) {
  static const char json[] =
      "{\"keep\":1,\"drop\":2,\"s\":\"x\"}\n[3,true]";
  unsigned char out[128];
  test_buffer_sink sink;
  test_reader_state reader;
  test_candidate_transform_state state;
  lonejson_candidate_transform_options options;
  lonejson_status status;
  lonejson_error error;

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 1u;
  memset(&options, 0, sizeof(options));
  options.framing = LONEJSON_CANDIDATE_FRAMING_NDJSON;
  options.output_framing = LONEJSON_CANDIDATE_TRANSFORM_OUTPUT_NDJSON;
  options.old_scalar_mode = LONEJSON_CANDIDATE_TRANSFORM_OLD_SCALAR_COMPLETE;
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_decide;
  options.replace = test_candidate_transform_replace;
  options.transform_user = &state;
  status = lonejson_transform_candidates_reader(test_default_runtime(),
                                                test_state_reader, &reader,
                                                &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out, "{\"keep\":2,\"s\":\"x\"}\n[4,true]\n") ==
         0);
  EXPECT(state.old_number_seen == 3u);
}

static void test_candidate_stream_transform_gated_spooled_replay(void) {
  static const char json[] = "[{\"keep\":1,\"drop\":2},{\"keep\":3}]";
  char dir_template[] = "/tmp/lonejson-transform-spool-XXXXXX";
  char *dir;
  unsigned char out[128];
  test_buffer_sink sink;
  test_candidate_transform_state state;
  lonejson_candidate_transform_options options;
  lonejson_candidate_transform_result result;
  lonejson_path_value_visitor observer;
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

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  memset(&result, 0, sizeof(result));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  observer = lonejson_default_path_value_visitor();
  observer.number_chunk = test_candidate_transform_observe_number;
  memset(&options, 0, sizeof(options));
  options.framing = LONEJSON_CANDIDATE_FRAMING_ARRAY_ITEMS;
  options.mode = LONEJSON_CANDIDATE_TRANSFORM_MODE_GATED_SPOOLED;
  options.old_scalar_mode = LONEJSON_CANDIDATE_TRANSFORM_OLD_SCALAR_COMPLETE;
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.observer = &observer;
  options.observer_user = &state;
  options.transform = test_candidate_transform_decide;
  options.replace = test_candidate_transform_replace;
  options.transform_user = &state;
  options.result = &result;
  status = lonejson_transform_candidates_buffer(runtime, json, strlen(json),
                                                &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out, "{\"keep\":2}\n{\"keep\":4}\n") == 0);
  EXPECT(state.number_chunks == 3u);
  EXPECT(state.old_number_seen == 3u);
  EXPECT(state.gated_metadata_seen != 0u);
  EXPECT(result.candidates_streamed == 0u);
  EXPECT(result.candidates_spooled == 2u);
  EXPECT(result.candidates_spilled == 2u);
  EXPECT(result.candidates_replayed == 2u);
  EXPECT(result.total_bytes_spooled != 0u);
  EXPECT(result.total_spill_bytes != 0u);
  EXPECT(result.last_candidate.mode ==
         LONEJSON_CANDIDATE_TRANSFORM_MODE_GATED_SPOOLED);
  EXPECT(result.last_candidate.physical_index == 1u);
  EXPECT(result.last_candidate.logical_index == 1u);
  EXPECT(result.last_candidate.gated_spooled != 0);
  EXPECT(result.last_candidate.spilled != 0);
  EXPECT(result.last_candidate.replay_count == 1u);
  EXPECT(result.last_candidate.stream_offset != 0u);
  EXPECT(result.last_candidate.byte_size !=
         LONEJSON_CANDIDATE_BYTE_SIZE_UNKNOWN);

  lonejson_free(runtime);
  rmdir(dir);
}

static void test_candidate_stream_transform_streams_large_string_keep(void) {
  char json[4099];
  test_counting_sink sink;
  test_candidate_transform_state state;
  lonejson_candidate_transform_options options;
  lonejson_config config;
  lonejson *runtime;
  lonejson_status status;
  lonejson_error error;
  size_t i;

  json[0] = '"';
  for (i = 1u; i < sizeof(json) - 2u; ++i) {
    json[i] = 'a';
  }
  json[sizeof(json) - 2u] = '"';
  json[sizeof(json) - 1u] = '\0';
  config = lonejson_default_config();
  config.max_alloc_bytes = 1024u;
  runtime = lonejson_new(&config, &error);
  EXPECT(runtime != NULL);
  if (runtime == NULL) {
    return;
  }

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  memset(&options, 0, sizeof(options));
  options.framing = LONEJSON_CANDIDATE_FRAMING_SINGLE_VALUE;
  options.sink = test_counting_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_keep_stream;
  options.transform_user = &state;
  status = lonejson_transform_candidates_buffer(runtime, json, strlen(json),
                                                &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(sink.total == strlen(json) + 1u);
  EXPECT(state.transform_metadata_seen == 1u);

  lonejson_free(runtime);
}

static void test_candidate_stream_transform_inserts_object_members(void) {
  static const char json[] = "{\"a\":3}";
  unsigned char out[160];
  test_buffer_sink sink;
  test_candidate_transform_state state;
  lonejson_candidate_transform_options options;
  lonejson_status status;
  lonejson_error error;
  lj_candidate_transform_insert_phase short_phase;

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  short_phase = LJ_CANDIDATE_TRANSFORM_INSERT_OBJECT_END;
  EXPECT(short_phase == LONEJSON_CANDIDATE_TRANSFORM_INSERT_OBJECT_END);
  memset(&options, 0, sizeof(options));
  options.framing = LONEJSON_CANDIDATE_FRAMING_SINGLE_VALUE;
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_keep_stream;
  options.insert = test_candidate_transform_insert_members;
  options.transform_user = &state;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out,
                "{\"first\":0,\"before_a\":1,\"a\":3,\"after_a\":2,"
                "\"last\":{\"nested\":true}}\n") == 0);
  EXPECT(state.insert_object_begin_seen == 1u);
  EXPECT(state.insert_before_member_seen == 1u);
  EXPECT(state.insert_after_member_seen == 1u);
  EXPECT(state.insert_object_end_seen == 1u);
}

static void
test_candidate_stream_transform_inserts_after_dropped_container_member(void) {
  static const char json[] = "{\"a\":{\"nested\":true}}";
  unsigned char out[160];
  test_buffer_sink sink;
  test_candidate_transform_state state;
  lonejson_candidate_transform_options options;
  lonejson_status status;
  lonejson_error error;

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  memset(&options, 0, sizeof(options));
  options.framing = LONEJSON_CANDIDATE_FRAMING_SINGLE_VALUE;
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_drop_a;
  options.insert = test_candidate_transform_insert_members;
  options.transform_user = &state;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out,
                "{\"first\":0,\"before_a\":1,\"after_a\":2,"
                "\"last\":{\"nested\":true}}\n") == 0);
  EXPECT(state.insert_after_member_seen == 1u);
}

static void
test_candidate_stream_transform_inserts_after_replaced_container_member(void) {
  static const char json[] = "{\"a\":{\"nested\":true}}";
  unsigned char out[160];
  test_buffer_sink sink;
  test_candidate_transform_state state;
  lonejson_candidate_transform_options options;
  lonejson_status status;
  lonejson_error error;

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  memset(&options, 0, sizeof(options));
  options.framing = LONEJSON_CANDIDATE_FRAMING_SINGLE_VALUE;
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_replace_a;
  options.replace = test_candidate_transform_replace_with_nine;
  options.insert = test_candidate_transform_insert_members;
  options.transform_user = &state;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out,
                "{\"first\":0,\"before_a\":1,\"a\":9,\"after_a\":2,"
                "\"last\":{\"nested\":true}}\n") == 0);
  EXPECT(state.insert_after_member_seen == 1u);
}

static void
test_candidate_stream_transform_insert_array_object_relationship(void) {
  static const char json[] = "[{\"a\":1}]";
  unsigned char out[64];
  test_buffer_sink sink;
  test_candidate_transform_state state;
  lonejson_candidate_transform_options options;
  lonejson_status status;
  lonejson_error error;

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  memset(&options, 0, sizeof(options));
  options.framing = LONEJSON_CANDIDATE_FRAMING_SINGLE_VALUE;
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_keep_stream;
  options.insert = test_candidate_transform_insert_relationships;
  options.transform_user = &state;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out, "[{\"a\":1}]\n") == 0);
  EXPECT(state.insert_object_begin_seen == 1u);
  EXPECT(state.insert_object_end_seen == 1u);
  EXPECT(state.insert_array_object_begin_seen == 1u);
  EXPECT(state.insert_array_object_end_seen == 1u);
  EXPECT(state.insert_before_member_seen == 1u);
  EXPECT(state.insert_after_member_seen == 1u);
  EXPECT(state.insert_array_member_seen == 2u);
}

static void test_candidate_stream_transform_recursive_array_items(void) {
  static const char json[] =
      "[{\"id\":\"a\"},[{\"id\":\"b\"},[{\"id\":\"c\"}]],{\"id\":\"d\"}]";
  char dir_template[] = "/tmp/lonejson-transform-recursive-XXXXXX";
  char *dir;
  unsigned char out[192];
  test_buffer_sink sink;
  test_candidate_transform_state state;
  lonejson_candidate_transform_options options;
  lonejson_candidate_transform_result result;
  lonejson_config config;
  lonejson *runtime;
  lonejson_status status;
  lonejson_error error;

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  memset(&result, 0, sizeof(result));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  memset(&options, 0, sizeof(options));
  options.framing = LONEJSON_CANDIDATE_FRAMING_RECURSIVE_ARRAY_ITEMS;
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_keep_stream;
  options.transform_user = &state;
  options.result = &result;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out,
                "{\"id\":\"a\"}\n{\"id\":\"b\"}\n{\"id\":\"c\"}\n"
                "{\"id\":\"d\"}\n") == 0);
  EXPECT(result.candidates_streamed == 4u);
  EXPECT(result.last_candidate.physical_index == 3u);
  EXPECT(result.last_candidate.logical_index == 3u);
  EXPECT(result.last_candidate.stream_offset !=
         LONEJSON_CANDIDATE_BYTE_SIZE_UNKNOWN);
  EXPECT(result.last_candidate.byte_size !=
         LONEJSON_CANDIDATE_BYTE_SIZE_UNKNOWN);

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

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  memset(&result, 0, sizeof(result));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  options.mode = LONEJSON_CANDIDATE_TRANSFORM_MODE_GATED_SPOOLED;
  options.result = &result;
  status = lonejson_transform_candidates_buffer(runtime, json, strlen(json),
                                                &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out,
                "{\"id\":\"a\"}\n{\"id\":\"b\"}\n{\"id\":\"c\"}\n"
                "{\"id\":\"d\"}\n") == 0);
  EXPECT(result.candidates_spooled == 4u);
  EXPECT(result.candidates_replayed == 4u);
  EXPECT(result.candidates_spilled == 4u);
  EXPECT(result.last_candidate.mode ==
         LONEJSON_CANDIDATE_TRANSFORM_MODE_GATED_SPOOLED);
  EXPECT(result.last_candidate.physical_index == 3u);
  EXPECT(result.last_candidate.logical_index == 3u);

  lonejson_free(runtime);
  rmdir(dir);
}

static void test_candidate_stream_transform_gated_decision_policy(void) {
  static const char json[] =
      "{\"big\":\"x\",\"id\":\"a\",\"status\":\"open\"}\n"
      "{\"big\":\"y\",\"id\":\"b\",\"status\":\"closed\"}";
  static const char recursive_json[] =
      "[{\"id\":\"a\",\"status\":\"open\"},"
      "[{\"id\":\"b\",\"status\":\"closed\"},"
      "{\"id\":\"c\",\"status\":\"open\"}]]";
  char dir_template[] = "/tmp/lonejson-transform-decision-XXXXXX";
  char *dir;
  unsigned char out[256];
  test_buffer_sink sink;
  test_reader_state reader;
  test_candidate_transform_gate_state state;
  lonejson_candidate_transform_options options;
  lonejson_candidate_transform_result result;
  lonejson_path_value_visitor observer;
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

  observer = lonejson_default_path_value_visitor();
  observer.string_chunk = test_candidate_transform_gate_observe_status;

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  memset(&result, 0, sizeof(result));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  state.sink = &sink;
  memset(&options, 0, sizeof(options));
  options.framing = LONEJSON_CANDIDATE_FRAMING_NDJSON;
  options.mode = LONEJSON_CANDIDATE_TRANSFORM_MODE_GATED_SPOOLED;
  options.old_scalar_mode = LONEJSON_CANDIDATE_TRANSFORM_OLD_SCALAR_COMPLETE;
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.observer = &observer;
  options.observer_user = &state;
  options.transform = test_candidate_transform_gate_transform;
  options.replace = test_candidate_transform_gate_replace;
  options.candidate_decision = test_candidate_transform_gate_decision;
  options.transform_user = &state;
  options.candidate_decision_user = &state;
  options.result = &result;
  status = lonejson_transform_candidates_buffer(runtime, json, strlen(json),
                                                &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out,
                "{\"big\":\"x\",\"id\":\"a\",\"status\":\"done\"}\n"
                "{\"big\":\"y\",\"id\":\"b\",\"status\":\"closed\"}\n") == 0);
  EXPECT(state.decision_count == 2u);
  EXPECT(state.root_policy_seen == 2u);
  EXPECT(state.status_policy_seen == 2u);
  EXPECT(state.replace_policy_seen == 1u);
  EXPECT(state.decision_sink_lengths[0] == 0u);
  EXPECT(state.decision_sink_lengths[1] ==
         strlen("{\"big\":\"x\",\"id\":\"a\",\"status\":\"done\"}\n"));
  EXPECT(result.candidates_spooled == 2u);
  EXPECT(result.candidates_replayed == 2u);
  EXPECT(result.candidates_dropped == 0u);
  EXPECT(result.candidates_spilled == 2u);

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  memset(&result, 0, sizeof(result));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  state.sink = &sink;
  state.matches_only = 1;
  options.observer_user = &state;
  options.transform_user = &state;
  options.candidate_decision_user = &state;
  options.result = &result;
  status = lonejson_transform_candidates_buffer(runtime, json, strlen(json),
                                                &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out,
                "{\"big\":\"x\",\"id\":\"a\",\"status\":\"done\"}\n") == 0);
  EXPECT(state.decision_count == 2u);
  EXPECT(state.root_policy_seen == 1u);
  EXPECT(state.status_policy_seen == 1u);
  EXPECT(state.replace_policy_seen == 1u);
  EXPECT(result.candidates_spooled == 2u);
  EXPECT(result.candidates_replayed == 1u);
  EXPECT(result.candidates_dropped == 1u);

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  memset(&result, 0, sizeof(result));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  state.sink = &sink;
  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 1u;
  options.framing = LONEJSON_CANDIDATE_FRAMING_NDJSON;
  options.observer_user = &state;
  options.transform_user = &state;
  options.candidate_decision_user = &state;
  options.result = &result;
  status = lonejson_transform_candidates_reader(runtime, test_state_reader,
                                                &reader, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out,
                "{\"big\":\"x\",\"id\":\"a\",\"status\":\"done\"}\n"
                "{\"big\":\"y\",\"id\":\"b\",\"status\":\"closed\"}\n") == 0);

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  memset(&result, 0, sizeof(result));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  state.sink = &sink;
  state.matches_only = 1;
  options.framing = LONEJSON_CANDIDATE_FRAMING_RECURSIVE_ARRAY_ITEMS;
  options.observer_user = &state;
  options.transform_user = &state;
  options.candidate_decision_user = &state;
  options.result = &result;
  status = lonejson_transform_candidates_buffer(runtime, recursive_json,
                                                strlen(recursive_json),
                                                &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out,
                "{\"id\":\"a\",\"status\":\"done\"}\n"
                "{\"id\":\"c\",\"status\":\"done\"}\n") == 0);
  EXPECT(state.decision_count == 3u);
  EXPECT(state.root_policy_seen == 2u);
  EXPECT(result.candidates_spooled == 3u);
  EXPECT(result.candidates_replayed == 2u);
  EXPECT(result.candidates_dropped == 1u);

  lonejson_free(runtime);
  rmdir(dir);
}

static void test_candidate_stream_transform_gated_decision_stop_and_error(void) {
  static const char json[] =
      "{\"id\":\"a\",\"status\":\"open\"}\n"
      "{\"id\":\"b\",\"status\":\"open\"}\n"
      "{\"id\":\"c\",\"status\":\"open\"}";
  unsigned char out[192];
  test_buffer_sink sink;
  test_candidate_transform_gate_state state;
  lonejson_candidate_transform_options options;
  lonejson_candidate_transform_result result;
  lonejson_path_value_visitor observer;
  lonejson_status status;
  lonejson_error error;

  observer = lonejson_default_path_value_visitor();
  observer.string_chunk = test_candidate_transform_gate_observe_status;

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  memset(&result, 0, sizeof(result));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  state.sink = &sink;
  state.stop_at_decision = 2;
  memset(&options, 0, sizeof(options));
  options.framing = LONEJSON_CANDIDATE_FRAMING_NDJSON;
  options.mode = LONEJSON_CANDIDATE_TRANSFORM_MODE_GATED_SPOOLED;
  options.old_scalar_mode = LONEJSON_CANDIDATE_TRANSFORM_OLD_SCALAR_COMPLETE;
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.observer = &observer;
  options.observer_user = &state;
  options.transform = test_candidate_transform_gate_transform;
  options.replace = test_candidate_transform_gate_replace;
  options.candidate_decision = test_candidate_transform_gate_decision;
  options.transform_user = &state;
  options.candidate_decision_user = &state;
  options.result = &result;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out, "{\"id\":\"a\",\"status\":\"done\"}\n") ==
         0);
  EXPECT(state.decision_count == 2u);
  EXPECT(result.candidates_spooled == 2u);
  EXPECT(result.candidates_replayed == 1u);
  EXPECT(result.candidates_stopped == 1u);

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  memset(&result, 0, sizeof(result));
  memset(&error, 0, sizeof(error));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  state.sink = &sink;
  state.fail_at_decision = 1;
  options.observer_user = &state;
  options.transform_user = &state;
  options.candidate_decision_user = &state;
  options.result = &result;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(error.code == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(sink.length == 0u);
  EXPECT(state.decision_count == 1u);
  EXPECT(result.candidates_spooled == 0u);
  EXPECT(result.candidates_replayed == 0u);
}

static void test_candidate_stream_transform_projected_composition_policy(void) {
  static const char json[] =
      "{\"big\":\"x\",\"id\":\"a\",\"status\":\"open\"}\n"
      "{\"big\":\"y\",\"id\":\"b\",\"status\":\"closed\"}";
  static const lonejson_candidate_transform_projection_segment segments[] = {
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "id", 2u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "status", 6u, 0u}};
  static const lonejson_candidate_transform_projection_path paths[] = {
      {&segments[0], 1u}, {&segments[1], 1u}};
  unsigned char out[256];
  test_buffer_sink sink;
  test_candidate_transform_gate_state state;
  lonejson_candidate_transform_options options;
  lonejson_candidate_transform_result result;
  lonejson_path_value_visitor observer;
  lonejson_status status;
  lonejson_error error;
  lj_candidate_transform_composition short_composition;
  lj_candidate_transform_candidate_decision short_decision;

  observer = lonejson_default_path_value_visitor();
  observer.string_chunk = test_candidate_transform_gate_observe_status;
  short_composition = LJ_CANDIDATE_TRANSFORM_COMPOSITION_PROJECT_THEN_TRANSFORM;
  EXPECT(short_composition ==
         LONEJSON_CANDIDATE_TRANSFORM_COMPOSITION_PROJECT_THEN_TRANSFORM);
  short_decision = LJ_CANDIDATE_TRANSFORM_CANDIDATE_DROP;
  EXPECT(short_decision == LONEJSON_CANDIDATE_TRANSFORM_CANDIDATE_DROP);

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  memset(&result, 0, sizeof(result));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  state.sink = &sink;
  state.expect_projected = 1;
  memset(&options, 0, sizeof(options));
  options.framing = LONEJSON_CANDIDATE_FRAMING_NDJSON;
  options.mode = LONEJSON_CANDIDATE_TRANSFORM_MODE_GATED_SPOOLED;
  options.composition =
      LONEJSON_CANDIDATE_TRANSFORM_COMPOSITION_PROJECT_THEN_TRANSFORM;
  options.old_scalar_mode = LONEJSON_CANDIDATE_TRANSFORM_OLD_SCALAR_COMPLETE;
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.observer = &observer;
  options.observer_user = &state;
  options.transform = test_candidate_transform_gate_transform;
  options.replace = test_candidate_transform_gate_replace;
  options.candidate_decision = test_candidate_transform_gate_decision;
  options.projection_paths = paths;
  options.projection_path_count = sizeof(paths) / sizeof(paths[0]);
  options.transform_user = &state;
  options.candidate_decision_user = &state;
  options.result = &result;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out,
                "{\"id\":\"a\",\"status\":\"done\"}\n"
                "{\"id\":\"b\",\"status\":\"closed\"}\n") == 0);
  EXPECT(state.decision_count == 2u);
  EXPECT(state.projected_replay_seen != 0u);
  EXPECT(state.source_replay_seen == 0u);
  EXPECT(state.projected_out_member_seen == 0u);
  EXPECT(state.root_relationship_seen == 2u);
  EXPECT(state.object_member_relationship_seen == 4u);
  EXPECT(result.candidates_spooled == 2u);
  EXPECT(result.candidates_projected == 2u);
  EXPECT(result.candidates_replayed == 4u);
  EXPECT(result.total_bytes_projected != 0u);
  EXPECT(result.last_candidate.replay_count == 2u);
  EXPECT(result.last_candidate.bytes_projected != 0u);

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  memset(&result, 0, sizeof(result));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  state.sink = &sink;
  state.expect_projected = 1;
  state.matches_only = 1;
  options.observer_user = &state;
  options.transform_user = &state;
  options.candidate_decision_user = &state;
  options.result = &result;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out, "{\"id\":\"a\",\"status\":\"done\"}\n") ==
         0);
  EXPECT(result.candidates_spooled == 2u);
  EXPECT(result.candidates_projected == 1u);
  EXPECT(result.candidates_replayed == 2u);
  EXPECT(result.candidates_dropped == 1u);

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  memset(&result, 0, sizeof(result));
  memset(&error, 0, sizeof(error));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  options.mode = LONEJSON_CANDIDATE_TRANSFORM_MODE_STREAMING;
  options.observer_user = &state;
  options.transform_user = &state;
  options.candidate_decision_user = &state;
  options.result = &result;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_UNSUPPORTED);
  EXPECT(sink.length == 0u);
}

static void test_candidate_stream_transform_projected_mutation_shapes(void) {
  static const char json[] =
      "{\"id\":\"a\",\"status\":\"open\",\"state\":{\"count\":1},"
      "\"uri\":\"u\",\"drop\":true}\n"
      "{\"id\":\"b\",\"status\":\"closed\",\"state\":{\"count\":1},"
      "\"uri\":\"v\",\"drop\":true}";
  static const lonejson_candidate_transform_projection_segment segments[] = {
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "id", 2u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "state", 5u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "count", 5u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "uri", 3u, 0u}};
  static const lonejson_candidate_transform_projection_path paths[] = {
      {&segments[0], 1u}, {&segments[1], 2u}, {&segments[3], 1u}};
  unsigned char out[256];
  test_buffer_sink sink;
  test_candidate_transform_projected_state state;
  lonejson_candidate_transform_options options;
  lonejson_candidate_transform_result result;
  lonejson_path_value_visitor observer;
  lonejson_status status;
  lonejson_error error;

  observer = lonejson_default_path_value_visitor();
  observer.string_chunk = test_candidate_transform_gate_observe_status;
  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  memset(&result, 0, sizeof(result));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  state.gate.sink = &sink;
  state.gate.expect_projected = 1;
  state.insert_hello = 1;
  state.mutate_count = 1;
  memset(&options, 0, sizeof(options));
  options.framing = LONEJSON_CANDIDATE_FRAMING_NDJSON;
  options.mode = LONEJSON_CANDIDATE_TRANSFORM_MODE_GATED_SPOOLED;
  options.composition =
      LONEJSON_CANDIDATE_TRANSFORM_COMPOSITION_PROJECT_THEN_TRANSFORM;
  options.old_scalar = test_candidate_transform_projected_old_scalar;
  options.old_scalar_user = &state;
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.observer = &observer;
  options.observer_user = &state.gate;
  options.transform = test_candidate_transform_projected_transform;
  options.replace = test_candidate_transform_projected_replace;
  options.insert = test_candidate_transform_projected_insert;
  options.candidate_decision = test_candidate_transform_projected_decision;
  options.projection_paths = paths;
  options.projection_path_count = sizeof(paths) / sizeof(paths[0]);
  options.transform_user = &state;
  options.candidate_decision_user = &state;
  options.result = &result;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out,
                "{\"id\":\"a\",\"state\":{\"count\":2},\"uri\":\"u\","
                "\"hello\":\"world\"}\n"
                "{\"id\":\"b\",\"state\":{\"count\":1},\"uri\":\"v\"}\n") ==
         0);
  EXPECT(state.count_seen == 2u);
  EXPECT(state.count_replaced == 1u);
  EXPECT(state.root_hello_inserted == 1u);
  EXPECT(state.old_scalar_complete_count == 2u);
  EXPECT(state.streaming_string_seen == 4u);
  EXPECT(result.candidates_spooled == 2u);
  EXPECT(result.candidates_projected == 2u);
  EXPECT(result.candidates_replayed == 4u);
}

static void
test_candidate_stream_transform_projected_synthetic_shapes(void) {
  static const char json[] = "{\"status\":\"open\",\"arr\":[\"zero\"]}";
  static const lonejson_candidate_transform_projection_segment segments[] = {
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "missing", 7u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "child", 5u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "arr", 3u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_ARRAY_INDEX, NULL, 0u, 1u}};
  static const lonejson_candidate_transform_projection_path paths[] = {
      {&segments[0], 2u}, {&segments[2], 2u}};
  unsigned char out[192];
  test_buffer_sink sink;
  test_candidate_transform_projected_state state;
  lonejson_candidate_transform_options options;
  lonejson_candidate_transform_result result;
  lonejson_path_value_visitor observer;
  lonejson_status status;
  lonejson_error error;

  observer = lonejson_default_path_value_visitor();
  observer.string_chunk = test_candidate_transform_gate_observe_status;
  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  memset(&result, 0, sizeof(result));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  state.gate.sink = &sink;
  state.gate.expect_projected = 1;
  state.mutate_placeholder = 1;
  memset(&options, 0, sizeof(options));
  options.framing = LONEJSON_CANDIDATE_FRAMING_SINGLE_VALUE;
  options.mode = LONEJSON_CANDIDATE_TRANSFORM_MODE_GATED_SPOOLED;
  options.composition =
      LONEJSON_CANDIDATE_TRANSFORM_COMPOSITION_PROJECT_THEN_TRANSFORM;
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.observer = &observer;
  options.observer_user = &state.gate;
  options.transform = test_candidate_transform_projected_transform;
  options.replace = test_candidate_transform_projected_replace;
  options.insert = test_candidate_transform_projected_insert;
  options.candidate_decision = test_candidate_transform_projected_decision;
  options.projection_paths = paths;
  options.projection_path_count = sizeof(paths) / sizeof(paths[0]);
  options.transform_user = &state;
  options.candidate_decision_user = &state;
  options.result = &result;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out,
                "{\"arr\":[null,\"filled\"],"
                "\"missing\":{\"child\":null}}\n") == 0);
  EXPECT(state.synthetic_object_begin_seen == 1u);
  EXPECT(state.synthetic_object_end_seen == 1u);
  EXPECT(state.array_placeholder_seen == 1u);
  EXPECT(state.array_placeholder_replaced == 1u);
  EXPECT(state.array_relationship_seen != 0u);
  EXPECT(result.candidates_projected == 1u);
  EXPECT(result.last_candidate.bytes_projected != 0u);
}

static void
test_candidate_stream_transform_projected_recursive_and_fragmented(void) {
  static const char json[] =
      "[{\"id\":\"a\",\"status\":\"open\",\"state\":{\"count\":1}},"
      "[{\"id\":\"b\",\"status\":\"closed\",\"state\":{\"count\":1}},"
      "{\"id\":\"c\",\"status\":\"open\",\"state\":{\"count\":1}}]]";
  static const lonejson_candidate_transform_projection_segment segments[] = {
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "id", 2u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "state", 5u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "count", 5u, 0u}};
  static const lonejson_candidate_transform_projection_path paths[] = {
      {&segments[0], 1u}, {&segments[1], 2u}};
  unsigned char out[256];
  test_buffer_sink sink;
  test_reader_state reader;
  test_candidate_transform_projected_state state;
  lonejson_candidate_transform_options options;
  lonejson_candidate_transform_result result;
  lonejson_path_value_visitor observer;
  lonejson_status status;
  lonejson_error error;

  observer = lonejson_default_path_value_visitor();
  observer.string_chunk = test_candidate_transform_gate_observe_status;
  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  memset(&result, 0, sizeof(result));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 1u;
  state.gate.sink = &sink;
  state.gate.expect_projected = 1;
  state.mutate_count = 1;
  memset(&options, 0, sizeof(options));
  options.framing = LONEJSON_CANDIDATE_FRAMING_RECURSIVE_ARRAY_ITEMS;
  options.mode = LONEJSON_CANDIDATE_TRANSFORM_MODE_GATED_SPOOLED;
  options.composition =
      LONEJSON_CANDIDATE_TRANSFORM_COMPOSITION_PROJECT_THEN_TRANSFORM;
  options.old_scalar = test_candidate_transform_projected_old_scalar;
  options.old_scalar_user = &state;
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.observer = &observer;
  options.observer_user = &state.gate;
  options.transform = test_candidate_transform_projected_transform;
  options.replace = test_candidate_transform_projected_replace;
  options.candidate_decision = test_candidate_transform_projected_decision;
  options.projection_paths = paths;
  options.projection_path_count = sizeof(paths) / sizeof(paths[0]);
  options.transform_user = &state;
  options.candidate_decision_user = &state;
  options.result = &result;
  status = lonejson_transform_candidates_reader(test_default_runtime(),
                                                test_state_reader, &reader,
                                                &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out,
                "{\"id\":\"a\",\"state\":{\"count\":2}}\n"
                "{\"id\":\"b\",\"state\":{\"count\":1}}\n"
                "{\"id\":\"c\",\"state\":{\"count\":2}}\n") == 0);
  EXPECT(state.gate.decision_count == 3u);
  EXPECT(state.count_replaced == 2u);
  EXPECT(state.old_scalar_complete_count == 3u);
  EXPECT(state.streaming_string_seen == 3u);
  EXPECT(result.candidates_spooled == 3u);
  EXPECT(result.candidates_projected == 3u);
  EXPECT(result.candidates_replayed == 6u);
}

static void
test_candidate_stream_transform_projected_stop_and_read_failure(void) {
  static const char json[] =
      "{\"id\":\"a\",\"status\":\"open\"}\n"
      "{\"id\":\"b\",\"status\":\"open\"}\n"
      "{\"id\":\"c\",\"status\":\"open\"}";
  static const lonejson_candidate_transform_projection_segment segments[] = {
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "id", 2u, 0u}};
  static const lonejson_candidate_transform_projection_path paths[] = {
      {segments, 1u}};
  unsigned char out[192];
  test_buffer_sink sink;
  test_candidate_transform_failing_reader_state reader;
  test_candidate_transform_projected_state state;
  lonejson_candidate_transform_options options;
  lonejson_candidate_transform_result result;
  lonejson_path_value_visitor observer;
  lonejson_status status;
  lonejson_error error;

  observer = lonejson_default_path_value_visitor();
  observer.string_chunk = test_candidate_transform_gate_observe_status;
  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  memset(&result, 0, sizeof(result));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  state.gate.sink = &sink;
  state.gate.expect_projected = 1;
  state.gate.stop_at_decision = 2;
  memset(&options, 0, sizeof(options));
  options.framing = LONEJSON_CANDIDATE_FRAMING_NDJSON;
  options.mode = LONEJSON_CANDIDATE_TRANSFORM_MODE_GATED_SPOOLED;
  options.composition =
      LONEJSON_CANDIDATE_TRANSFORM_COMPOSITION_PROJECT_THEN_TRANSFORM;
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.observer = &observer;
  options.observer_user = &state.gate;
  options.transform = test_candidate_transform_projected_transform;
  options.candidate_decision = test_candidate_transform_projected_decision;
  options.projection_paths = paths;
  options.projection_path_count = 1u;
  options.transform_user = &state;
  options.candidate_decision_user = &state;
  options.result = &result;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out, "{\"id\":\"a\"}\n") == 0);
  EXPECT(state.gate.decision_count == 2u);
  EXPECT(result.candidates_spooled == 2u);
  EXPECT(result.candidates_projected == 1u);
  EXPECT(result.candidates_replayed == 2u);
  EXPECT(result.candidates_stopped == 1u);

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  memset(&result, 0, sizeof(result));
  memset(&error, 0, sizeof(error));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 1u;
  reader.fail_after = strlen("{\"id\":\"a\",\"status\":\"open\"}\n");
  state.gate.sink = &sink;
  state.gate.expect_projected = 1;
  options.observer_user = &state.gate;
  options.transform_user = &state;
  options.candidate_decision_user = &state;
  options.result = &result;
  status = lonejson_transform_candidates_reader(
      test_default_runtime(), test_candidate_transform_failing_reader, &reader,
      &options, &error);
  EXPECT(status != LONEJSON_STATUS_OK);
  EXPECT(error.code == status);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out, "{\"id\":\"a\"}\n") == 0);
  EXPECT(result.candidates_projected == 1u);
  EXPECT(result.candidates_replayed == 2u);
}

static void test_candidate_stream_transform_projects_structural_paths(void) {
  static const char json[] =
      "{\"id\":\"a\",\"drop\":1,\"nested\":{\"keep\":\"x\"},"
      "\"arr\":[\"zero\",\"one\",\"two\"]}";
  static const lonejson_candidate_transform_projection_segment segments[] = {
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "id", 2u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "nested", 6u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "keep", 4u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "missing", 7u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "a", 1u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "missing", 7u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "b", 1u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "arr", 3u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_ARRAY_INDEX, NULL, 0u, 2u}};
  static const lonejson_candidate_transform_projection_path paths[] = {
      {&segments[0], 1u}, {&segments[1], 2u}, {&segments[3], 2u},
      {&segments[5], 2u}, {&segments[7], 2u}};
  unsigned char out[192];
  test_buffer_sink sink;
  test_candidate_transform_state state;
  lonejson_candidate_transform_options options;
  lonejson_status status;
  lonejson_error error;

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  memset(&options, 0, sizeof(options));
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_keep_stream;
  options.transform_user = &state;
  options.projection_paths = paths;
  options.projection_path_count = sizeof(paths) / sizeof(paths[0]);
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out,
                "{\"id\":\"a\",\"nested\":{\"keep\":\"x\"},"
                "\"arr\":[null,null,\"two\"],"
                "\"missing\":{\"a\":null,\"b\":null}}\n") == 0);
}

static void
test_candidate_stream_transform_projection_inserts_only_emitted_members(void) {
  static const char json[] = "{\"drop\":1,\"keep\":2}";
  static const lonejson_candidate_transform_projection_segment segments[] = {
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "keep", 4u, 0u}};
  static const lonejson_candidate_transform_projection_path paths[] = {
      {segments, 1u}};
  unsigned char out[96];
  test_buffer_sink sink;
  test_candidate_transform_state state;
  lonejson_candidate_transform_options options;
  lonejson_status status;
  lonejson_error error;

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  memset(&options, 0, sizeof(options));
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_keep_stream;
  options.insert = test_candidate_transform_insert_before_each_member;
  options.transform_user = &state;
  options.projection_paths = paths;
  options.projection_path_count = 1u;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out, "{\"before\":0,\"keep\":2}\n") == 0);
  EXPECT(state.insert_before_member_seen == 1u);
}

static void test_candidate_stream_transform_projection_unsupported_shapes(void) {
  static const char json[] = "{\"id\":\"a\"}";
  static const lonejson_candidate_transform_projection_segment segments[] = {
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "id", 2u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_ARRAY_INDEX, NULL, 0u, 0u}};
  static const lonejson_candidate_transform_projection_path paths[] = {
      {&segments[0], 1u}, {&segments[1], 1u}};
  unsigned char out[64];
  test_buffer_sink sink;
  lonejson_candidate_transform_options options;
  lonejson_status status;
  lonejson_error error;

  memset(&sink, 0, sizeof(sink));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  memset(&options, 0, sizeof(options));
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_keep_stream;
  options.projection_paths = paths;
  options.projection_path_count = sizeof(paths) / sizeof(paths[0]);
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_UNSUPPORTED);
  EXPECT(sink.length == 0u);
}

static void test_candidate_stream_transform_projects_container_value(void) {
  static const char json[] =
      "{\"nested\":{\"keep\":\"x\",\"n\":1},\"id\":2}";
  static const lonejson_candidate_transform_projection_segment segments[] = {
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "nested", 6u, 0u}};
  static const lonejson_candidate_transform_projection_path paths[] = {
      {segments, 1u}};
  unsigned char out[128];
  test_buffer_sink sink;
  test_candidate_transform_state state;
  lonejson_candidate_transform_options options;
  lonejson_status status;
  lonejson_error error;

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  memset(&options, 0, sizeof(options));
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_keep_stream;
  options.transform_user = &state;
  options.projection_paths = paths;
  options.projection_path_count = 1u;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out,
                "{\"nested\":{\"keep\":\"x\",\"n\":1}}\n") == 0);
}

static void
test_candidate_stream_transform_projects_wrong_shape_parent(void) {
  static const char json[] = "{\"a\":1,\"arr\":1}";
  static const lonejson_candidate_transform_projection_segment segments[] = {
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "a", 1u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "b", 1u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "arr", 3u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_ARRAY_INDEX, NULL, 0u, 2u}};
  static const lonejson_candidate_transform_projection_path paths[] = {
      {&segments[0], 2u}, {&segments[2], 2u}};
  unsigned char out[128];
  test_buffer_sink sink;
  test_candidate_transform_state state;
  lonejson_candidate_transform_options options;
  lonejson_status status;
  lonejson_error error;

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  memset(&options, 0, sizeof(options));
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_keep_stream;
  options.transform_user = &state;
  options.projection_paths = paths;
  options.projection_path_count = sizeof(paths) / sizeof(paths[0]);
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out,
                "{\"a\":{\"b\":null},\"arr\":[null,null,null]}\n") == 0);
}

static void
test_candidate_stream_transform_projects_wrong_shape_container_parent(void) {
  static const char json[] = "{\"a\":[],\"arr\":{}}";
  static const lonejson_candidate_transform_projection_segment segments[] = {
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "a", 1u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "b", 1u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "arr", 3u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_ARRAY_INDEX, NULL, 0u, 2u}};
  static const lonejson_candidate_transform_projection_path paths[] = {
      {&segments[0], 2u}, {&segments[2], 2u}};
  unsigned char out[128];
  test_buffer_sink sink;
  test_candidate_transform_state state;
  lonejson_candidate_transform_options options;
  lonejson_status status;
  lonejson_error error;

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  memset(&options, 0, sizeof(options));
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_keep_stream;
  options.transform_user = &state;
  options.projection_paths = paths;
  options.projection_path_count = sizeof(paths) / sizeof(paths[0]);
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out,
                "{\"a\":{\"b\":null},\"arr\":[null,null,null]}\n") == 0);
}

static void
test_candidate_stream_transform_projects_wrong_shape_scalar_array_item(void) {
  static const char json[] = "[0,1,\"x\",3,\"ok\"]";
  static const lonejson_candidate_transform_projection_segment segments[] = {
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_ARRAY_INDEX, NULL, 0u, 2u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "a", 1u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_ARRAY_INDEX, NULL, 0u, 4u}};
  static const lonejson_candidate_transform_projection_path paths[] = {
      {&segments[0], 2u}, {&segments[2], 1u}};
  unsigned char out[128];
  test_buffer_sink sink;
  test_candidate_transform_state state;
  lonejson_candidate_transform_options options;
  lonejson_status status;
  lonejson_error error;

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  memset(&options, 0, sizeof(options));
  options.framing = LONEJSON_CANDIDATE_FRAMING_SINGLE_VALUE;
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_keep_stream;
  options.transform_user = &state;
  options.projection_paths = paths;
  options.projection_path_count = sizeof(paths) / sizeof(paths[0]);
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out,
                "[null,null,{\"a\":null},null,\"ok\"]\n") == 0);
}

static void
test_candidate_stream_transform_projects_wrong_shape_scalar_object_member(void) {
  static const char json[] = "{\"a\":1,\"z\":2}";
  static const lonejson_candidate_transform_projection_segment segments[] = {
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "a", 1u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "b", 1u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "z", 1u, 0u}};
  static const lonejson_candidate_transform_projection_path paths[] = {
      {&segments[0], 2u}, {&segments[2], 1u}};
  unsigned char out[128];
  test_buffer_sink sink;
  test_candidate_transform_state state;
  lonejson_candidate_transform_options options;
  lonejson_status status;
  lonejson_error error;

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  memset(&options, 0, sizeof(options));
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_keep_stream;
  options.transform_user = &state;
  options.projection_paths = paths;
  options.projection_path_count = sizeof(paths) / sizeof(paths[0]);
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out, "{\"a\":{\"b\":null},\"z\":2}\n") == 0);
}

static void
test_candidate_stream_transform_projection_drops_wrong_shape_scalar_ancestor(
    void) {
  static const char json[] = "{\"a\":1,\"z\":2}";
  static const lonejson_candidate_transform_projection_segment segments[] = {
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "a", 1u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "b", 1u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "z", 1u, 0u}};
  static const lonejson_candidate_transform_projection_path paths[] = {
      {&segments[0], 2u}, {&segments[2], 1u}};
  unsigned char out[64];
  test_buffer_sink sink;
  lonejson_candidate_transform_options options;
  lonejson_status status;
  lonejson_error error;

  memset(&sink, 0, sizeof(sink));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  memset(&options, 0, sizeof(options));
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_drop_a;
  options.projection_paths = paths;
  options.projection_path_count = sizeof(paths) / sizeof(paths[0]);
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out, "{\"z\":2}\n") == 0);
}

static void test_candidate_stream_transform_projection_drop_stays_dropped(void) {
  static const char json[] = "{\"id\":\"a\",\"x\":1}";
  static const lonejson_candidate_transform_projection_segment segments[] = {
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "id", 2u, 0u}};
  static const lonejson_candidate_transform_projection_path paths[] = {
      {segments, 1u}};
  unsigned char out[64];
  test_buffer_sink sink;
  lonejson_candidate_transform_options options;
  lonejson_status status;
  lonejson_error error;

  memset(&sink, 0, sizeof(sink));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  memset(&options, 0, sizeof(options));
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_drop_id;
  options.projection_paths = paths;
  options.projection_path_count = 1u;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out, "{}\n") == 0);
}

static void
test_candidate_stream_transform_projection_honors_ancestor_drop(void) {
  static const char json[] = "{\"a\":{\"b\":1},\"x\":2}";
  static const lonejson_candidate_transform_projection_segment segments[] = {
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "a", 1u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "b", 1u, 0u}};
  static const lonejson_candidate_transform_projection_path paths[] = {
      {segments, 2u}};
  unsigned char out[64];
  test_buffer_sink sink;
  lonejson_candidate_transform_options options;
  lonejson_status status;
  lonejson_error error;

  memset(&sink, 0, sizeof(sink));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  memset(&options, 0, sizeof(options));
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_drop_a;
  options.projection_paths = paths;
  options.projection_path_count = 1u;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out, "{}\n") == 0);
}

static void
test_candidate_stream_transform_projection_honors_ancestor_replace(void) {
  static const char json[] = "{\"a\":{\"b\":1},\"x\":2}";
  static const lonejson_candidate_transform_projection_segment segments[] = {
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "a", 1u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "b", 1u, 0u}};
  static const lonejson_candidate_transform_projection_path paths[] = {
      {segments, 2u}};
  unsigned char out[64];
  test_buffer_sink sink;
  lonejson_candidate_transform_options options;
  lonejson_status status;
  lonejson_error error;

  memset(&sink, 0, sizeof(sink));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  memset(&options, 0, sizeof(options));
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_replace_a;
  options.replace = test_candidate_transform_replace_with_nine;
  options.projection_paths = paths;
  options.projection_path_count = 1u;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out, "{\"a\":9}\n") == 0);
}

static void
test_candidate_stream_transform_projection_honors_root_replace(void) {
  static const char json[] = "{\"a\":{\"b\":1},\"x\":2}";
  static const lonejson_candidate_transform_projection_segment segments[] = {
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "a", 1u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "b", 1u, 0u}};
  static const lonejson_candidate_transform_projection_path paths[] = {
      {segments, 2u}};
  unsigned char out[64];
  test_buffer_sink sink;
  lonejson_candidate_transform_options options;
  lonejson_status status;
  lonejson_error error;

  memset(&sink, 0, sizeof(sink));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  memset(&options, 0, sizeof(options));
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_replace_root;
  options.replace = test_candidate_transform_replace_with_nine;
  options.projection_paths = paths;
  options.projection_path_count = 1u;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out, "9\n") == 0);
}

static void test_candidate_stream_transform_projection_rejects_scalar_root(void) {
  static const char json[] = "1";
  static const lonejson_candidate_transform_projection_segment segments[] = {
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "id", 2u, 0u}};
  static const lonejson_candidate_transform_projection_path paths[] = {
      {segments, 1u}};
  unsigned char out[64];
  test_buffer_sink sink;
  lonejson_candidate_transform_options options;
  lonejson_status status;
  lonejson_error error;

  memset(&sink, 0, sizeof(sink));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  memset(&options, 0, sizeof(options));
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_keep_stream;
  options.projection_paths = paths;
  options.projection_path_count = 1u;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_UNSUPPORTED);
  EXPECT(sink.length == 0u);
}

static void test_candidate_stream_transform_projection_nul_key_seen(void) {
  static const char json[] = "{\"a\\u0000b\":1}";
  static const char key[] = {'a', '\0', 'b'};
  static const lonejson_candidate_transform_projection_segment segments[] = {
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, key, 3u, 0u}};
  static const lonejson_candidate_transform_projection_path paths[] = {
      {segments, 1u}};
  unsigned char out[128];
  test_buffer_sink sink;
  test_candidate_transform_state state;
  lonejson_candidate_transform_options options;
  lonejson_status status;
  lonejson_error error;

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  memset(&options, 0, sizeof(options));
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_keep_stream;
  options.transform_user = &state;
  options.projection_paths = paths;
  options.projection_path_count = 1u;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out, "{\"a\\u0000b\":1}\n") == 0);
}

static void test_candidate_stream_transform_projection_empty_key_seen(void) {
  static const char json[] = "{\"\":1}";
  static const lonejson_candidate_transform_projection_segment segments[] = {
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, NULL, 0u, 0u}};
  static const lonejson_candidate_transform_projection_path paths[] = {
      {segments, 1u}};
  unsigned char out[64];
  test_buffer_sink sink;
  test_candidate_transform_state state;
  lonejson_candidate_transform_options options;
  lonejson_status status;
  lonejson_error error;

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  memset(&options, 0, sizeof(options));
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_keep_stream;
  options.transform_user = &state;
  options.projection_paths = paths;
  options.projection_path_count = 1u;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out, "{\"\":1}\n") == 0);
}

static void test_candidate_stream_transform_projects_gated_spooled(void) {
  static const char json[] =
      "{\"drop\":1,\"nested\":{\"keep\":\"x\"},\"tail\":true}";
  static const lonejson_candidate_transform_projection_segment segments[] = {
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "nested", 6u, 0u},
      {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, "keep", 4u, 0u}};
  static const lonejson_candidate_transform_projection_path paths[] = {
      {segments, 2u}};
  char dir_template[] = "/tmp/lonejson-transform-project-XXXXXX";
  char *dir;
  unsigned char out[96];
  test_buffer_sink sink;
  test_candidate_transform_state state;
  lonejson_candidate_transform_options options;
  lonejson_candidate_transform_result result;
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

  memset(&sink, 0, sizeof(sink));
  memset(&state, 0, sizeof(state));
  memset(&result, 0, sizeof(result));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  memset(&options, 0, sizeof(options));
  options.mode = LONEJSON_CANDIDATE_TRANSFORM_MODE_GATED_SPOOLED;
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_keep_stream;
  options.transform_user = &state;
  options.projection_paths = paths;
  options.projection_path_count = 1u;
  options.result = &result;
  status = lonejson_transform_candidates_buffer(runtime, json, strlen(json),
                                                &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out, "{\"nested\":{\"keep\":\"x\"}}\n") == 0);
  EXPECT(result.candidates_spooled == 1u);
  EXPECT(result.candidates_replayed == 1u);
  EXPECT(result.candidates_spilled == 1u);

  lonejson_free(runtime);
  rmdir(dir);
}

static void test_candidate_stream_transform_stop_container(void) {
  static const char root_stop_json[] = "[1,{\"x\":2}]\n{\"later\":3}";
  static const char nested_stop_json[] = "{\"a\":[1],\"b\":2}\n{\"later\":3}";
  unsigned char out[128];
  test_buffer_sink sink;
  lonejson_candidate_transform_options options;
  lonejson_status status;
  lonejson_error error;

  memset(&sink, 0, sizeof(sink));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  memset(&options, 0, sizeof(options));
  options.framing = LONEJSON_CANDIDATE_FRAMING_NDJSON;
  options.output_framing = LONEJSON_CANDIDATE_TRANSFORM_OUTPUT_NDJSON;
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_stop_root_array;
  status = lonejson_transform_candidates_buffer(test_default_runtime(),
                                                root_stop_json,
                                                strlen(root_stop_json),
                                                &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(sink.length == 0u);

  memset(&sink, 0, sizeof(sink));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  options.transform = test_candidate_transform_stop_at_a;
  status = lonejson_transform_candidates_buffer(test_default_runtime(),
                                                nested_stop_json,
                                                strlen(nested_stop_json),
                                                &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out, "{}\n") == 0);
}

static void test_candidate_stream_transform_drop_array_element_compacts(void) {
  static const char json[] = "[1,2,3]";
  unsigned char out[64];
  test_buffer_sink sink;
  lonejson_candidate_transform_options options;
  lonejson_status status;
  lonejson_error error;

  memset(&sink, 0, sizeof(sink));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  memset(&options, 0, sizeof(options));
  options.framing = LONEJSON_CANDIDATE_FRAMING_SINGLE_VALUE;
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_drop_array_index_one;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out, "[1,3]\n") == 0);
}

static void test_candidate_stream_transform_gated_stop_propagates(void) {
  static const char json[] = "{\"a\":1}\n{\"a\":2}";
  char dir_template[] = "/tmp/lonejson-transform-stop-XXXXXX";
  char *dir;
  unsigned char out[64];
  test_buffer_sink sink;
  lonejson_candidate_transform_options options;
  lonejson_candidate_transform_result result;
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

  memset(&sink, 0, sizeof(sink));
  memset(&result, 0, sizeof(result));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  memset(&options, 0, sizeof(options));
  options.mode = LONEJSON_CANDIDATE_TRANSFORM_MODE_GATED_SPOOLED;
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_stop_root_object;
  options.result = &result;
  status = lonejson_transform_candidates_buffer(runtime, json, strlen(json),
                                                &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(sink.length == 0u);
  EXPECT(result.candidates_spooled == 1u);
  EXPECT(result.candidates_replayed == 1u);
  EXPECT(result.last_candidate.logical_index == 0u);

  lonejson_free(runtime);
  rmdir(dir);
}

static void test_candidate_stream_transform_failure_modes(void) {
  static const char json[] = "{\"keep\":1,\"drop\":2}";
  unsigned char out[64];
  test_buffer_sink sink;
  test_failing_sink failing_sink;
  test_candidate_transform_state state;
  lonejson_candidate_transform_options options;
  lonejson_status status;
  lonejson_error error;

  memset(&options, 0, sizeof(options));
  memset(&sink, 0, sizeof(sink));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_drop_only;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  out[sink.length] = '\0';
  EXPECT(strcmp((const char *)out, "{\"keep\":1}\n") == 0);

  memset(&options, 0, sizeof(options));
  memset(&sink, 0, sizeof(sink));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_decide;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);

  memset(&options, 0, sizeof(options));
  memset(&sink, 0, sizeof(sink));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.mode = (lonejson_candidate_transform_mode)99;
  options.transform = test_candidate_transform_drop_only;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);

  memset(&options, 0, sizeof(options));
  memset(&sink, 0, sizeof(sink));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.spool_class = (lonejson_spool_class)99;
  options.transform = test_candidate_transform_drop_only;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);

  memset(&options, 0, sizeof(options));
  memset(&sink, 0, sizeof(sink));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.mode = LONEJSON_CANDIDATE_TRANSFORM_MODE_UNSUPPORTED;
  options.transform = test_candidate_transform_drop_only;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_UNSUPPORTED);
  EXPECT(strcmp(lonejson_status_string(LONEJSON_STATUS_UNSUPPORTED),
                "unsupported") == 0);

  memset(&options, 0, sizeof(options));
  memset(&sink, 0, sizeof(sink));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.mode = LONEJSON_CANDIDATE_TRANSFORM_MODE_GATED_SPOOLED;
  options.max_spooled_candidate_bytes = 4u;
  options.transform = test_candidate_transform_drop_only;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);

  memset(&options, 0, sizeof(options));
  memset(&sink, 0, sizeof(sink));
  sink.buffer = out;
  sink.capacity = 1u;
  options.framing = LONEJSON_CANDIDATE_FRAMING_SINGLE_VALUE;
  options.output_framing = LONEJSON_CANDIDATE_TRANSFORM_OUTPUT_NDJSON;
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_drop_only;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), "true",
                                                4u, &options, &error);
  EXPECT(status == LONEJSON_STATUS_TRUNCATED);
  EXPECT(error.code == LONEJSON_STATUS_TRUNCATED);
  EXPECT(sink.length == 0u);

  memset(&options, 0, sizeof(options));
  memset(&sink, 0, sizeof(sink));
  sink.buffer = out;
  sink.capacity = 1u;
  options.framing = LONEJSON_CANDIDATE_FRAMING_SINGLE_VALUE;
  options.output_framing = LONEJSON_CANDIDATE_TRANSFORM_OUTPUT_NDJSON;
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  options.transform = test_candidate_transform_drop_only;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), "[1]",
                                                3u, &options, &error);
  EXPECT(status == LONEJSON_STATUS_TRUNCATED);
  EXPECT(error.code == LONEJSON_STATUS_TRUNCATED);
  EXPECT(sink.length == 0u);

  {
    static const lonejson_candidate_transform_projection_path paths[] = {
        {NULL, 0u}};
    memset(&options, 0, sizeof(options));
    memset(&sink, 0, sizeof(sink));
    sink.buffer = out;
    sink.capacity = sizeof(out);
    options.sink = test_buffer_sink_write;
    options.sink_user = &sink;
    options.transform = test_candidate_transform_drop_only;
    options.projection_paths = paths;
    options.projection_path_count = 1u;
    status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                  strlen(json), &options,
                                                  &error);
    EXPECT(status == LONEJSON_STATUS_UNSUPPORTED);
    EXPECT(sink.length == 0u);
  }

  {
    static const lonejson_candidate_transform_projection_path paths[] = {
        {NULL, 1u}};
    memset(&options, 0, sizeof(options));
    memset(&sink, 0, sizeof(sink));
    sink.buffer = out;
    sink.capacity = sizeof(out);
    options.sink = test_buffer_sink_write;
    options.sink_user = &sink;
    options.transform = test_candidate_transform_drop_only;
    options.projection_paths = paths;
    options.projection_path_count = 1u;
    status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                  strlen(json), &options,
                                                  &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
    EXPECT(sink.length == 0u);
  }

  {
    static const lonejson_candidate_transform_projection_segment segments[] = {
        {LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER, NULL, 1u, 0u}};
    static const lonejson_candidate_transform_projection_path paths[] = {
        {segments, 1u}};
    memset(&options, 0, sizeof(options));
    memset(&sink, 0, sizeof(sink));
    sink.buffer = out;
    sink.capacity = sizeof(out);
    options.sink = test_buffer_sink_write;
    options.sink_user = &sink;
    options.transform = test_candidate_transform_drop_only;
    options.projection_paths = paths;
    options.projection_path_count = 1u;
    status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                  strlen(json), &options,
                                                  &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
    EXPECT(sink.length == 0u);
  }

  {
    static const lonejson_candidate_transform_projection_segment segments[] = {
        {(lonejson_candidate_transform_projection_segment_kind)99, NULL, 0u,
         0u}};
    static const lonejson_candidate_transform_projection_path paths[] = {
        {segments, 1u}};
    memset(&options, 0, sizeof(options));
    memset(&sink, 0, sizeof(sink));
    sink.buffer = out;
    sink.capacity = sizeof(out);
    options.sink = test_buffer_sink_write;
    options.sink_user = &sink;
    options.transform = test_candidate_transform_drop_only;
    options.projection_paths = paths;
    options.projection_path_count = 1u;
    status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                  strlen(json), &options,
                                                  &error);
    EXPECT(status == LONEJSON_STATUS_UNSUPPORTED);
    EXPECT(sink.length == 0u);
  }

  memset(&failing_sink, 0, sizeof(failing_sink));
  memset(&state, 0, sizeof(state));
  failing_sink.fail_after = 2u;
  memset(&options, 0, sizeof(options));
  options.framing = LONEJSON_CANDIDATE_FRAMING_SINGLE_VALUE;
  options.output_framing = LONEJSON_CANDIDATE_TRANSFORM_OUTPUT_NDJSON;
  options.sink = test_failing_sink_write;
  options.sink_user = &failing_sink;
  options.old_scalar_mode = LONEJSON_CANDIDATE_TRANSFORM_OLD_SCALAR_COMPLETE;
  options.transform = test_candidate_transform_decide;
  options.replace = test_candidate_transform_replace;
  options.transform_user = &state;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);

  memset(&state, 0, sizeof(state));
  memset(&sink, 0, sizeof(sink));
  sink.buffer = out;
  sink.capacity = sizeof(out);
  state.fail_replace = 1;
  options.old_scalar_mode = LONEJSON_CANDIDATE_TRANSFORM_OLD_SCALAR_COMPLETE;
  options.sink = test_buffer_sink_write;
  options.sink_user = &sink;
  status = lonejson_transform_candidates_buffer(test_default_runtime(), json,
                                                strlen(json), &options,
                                                &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
}
