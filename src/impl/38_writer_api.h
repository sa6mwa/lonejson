typedef enum lonejson__writer_mode {
  LONEJSON__WRITER_MODE_SINK = 1,
  LONEJSON__WRITER_MODE_GENERATOR = 2
} lonejson__writer_mode;

typedef enum lonejson__writer_frame_kind {
  LONEJSON__WRITER_FRAME_OBJECT = 1,
  LONEJSON__WRITER_FRAME_ARRAY = 2
} lonejson__writer_frame_kind;

typedef struct lonejson__writer_frame {
  lonejson__writer_frame_kind kind;
  size_t count;
  int after_key;
} lonejson__writer_frame;

typedef enum lonejson__writer_event_kind {
  LONEJSON__WRITER_EVENT_NONE = 0,
  LONEJSON__WRITER_EVENT_STRING = 1,
  LONEJSON__WRITER_EVENT_STRING_CHUNK = 2,
  LONEJSON__WRITER_EVENT_KEY = 3,
  LONEJSON__WRITER_EVENT_BYTES = 4,
  LONEJSON__WRITER_EVENT_CHILD = 5,
  LONEJSON__WRITER_EVENT_SCALAR_BYTES = 6
} lonejson__writer_event_kind;

typedef struct lonejson__writer_state {
  lonejson__writer_mode mode;
  lonejson_write_options options;
  lonejson_allocator allocator;
  lonejson_sink_fn sink;
  void *sink_user;
  lonejson_error *external_error;
  lonejson__writer_frame *frames;
  size_t frame_count;
  size_t frame_capacity;
  int root_written;
  int finished;
  int string_open;
  lonejson__writer_event_kind event_kind;
  const void *event_data;
  const void *event_data2;
  size_t event_len;
  size_t event_off;
  unsigned event_phase;
  unsigned char event_buffer[64];
  lonejson_generator event_child;
  unsigned char event_child_buffer[512];
  int event_child_active;
  int string_reader_active;
  lonejson_reader_fn string_reader;
  void *string_reader_user;
  unsigned char string_reader_buffer[4096];
  size_t string_reader_buffer_len;
  size_t string_reader_buffer_off;
  int string_reader_eof;
} lonejson__writer_state;

static lonejson_status
lonejson__writer_set_error(lonejson_writer *writer, lonejson_error *error,
                           lonejson_status status, const char *message) {
  lonejson__writer_state *state;
  lonejson_error *target;

  state = writer != NULL ? (lonejson__writer_state *)writer->state : NULL;
  target = error != NULL ? error : (writer != NULL ? &writer->error : NULL);
  if (target == NULL && state != NULL) {
    target = state->external_error;
  }
  return lonejson__set_error(target, status, 0u, 0u, 0u, "%s", message);
}

static lonejson_status lonejson__writer_before_value(lonejson_writer *writer,
                                                     lonejson_error *error);

static lonejson_status lonejson__writer_emit(lonejson_writer *writer,
                                             const void *data, size_t len,
                                             lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson_status status;

  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  if (writer == NULL || writer->state == NULL || data == NULL) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "writer and data are required");
  }
  state = (lonejson__writer_state *)writer->state;
  status = state->sink(state->sink_user, data, len,
                       error != NULL ? error : &writer->error);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    if (error == NULL && writer->error.code == LONEJSON_STATUS_OK) {
      lonejson__set_error(&writer->error, status, 0u, 0u, 0u,
                          "writer sink failed");
    }
  }
  return status;
}

static lonejson_status lonejson__writer_emit_cstr(lonejson_writer *writer,
                                                  const char *text,
                                                  lonejson_error *error) {
  return lonejson__writer_emit(writer, text, LONEJSON__TEXT_LEN(text), error);
}

static int lonejson__writer_output_blocked(lonejson_writer *writer) {
  lonejson__writer_state *state;

  if (writer == NULL || writer->state == NULL) {
    return 0;
  }
  state = (lonejson__writer_state *)writer->state;
  if (state->mode != LONEJSON__WRITER_MODE_GENERATOR) {
    return 0;
  }
  return lonejson__writer_generator_output_blocked(state->sink_user);
}

static void lonejson__writer_clear_event(lonejson__writer_state *state) {
  if (state->event_child_active) {
    lonejson_generator_cleanup(&state->event_child);
    state->event_child_active = 0;
  }
  state->event_kind = LONEJSON__WRITER_EVENT_NONE;
  state->event_data = NULL;
  state->event_data2 = NULL;
  state->event_len = 0u;
  state->event_off = 0u;
  state->event_phase = 0u;
}

static lonejson_status lonejson__writer_begin_event(
    lonejson_writer *writer, lonejson__writer_event_kind kind,
    const void *data, size_t len, lonejson_error *error) {
  lonejson__writer_state *state;

  if (writer == NULL || writer->state == NULL) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "writer is required");
  }
  state = (lonejson__writer_state *)writer->state;
  if (state->event_kind != LONEJSON__WRITER_EVENT_NONE) {
    if (state->event_kind != kind || state->event_data != data ||
        state->event_len != len) {
      return lonejson__writer_set_error(
          writer, error, LONEJSON_STATUS_INVALID_ARGUMENT,
          "active writer event must be resumed with the same arguments");
    }
    return LONEJSON_STATUS_OK;
  }
  state->event_kind = kind;
  state->event_data = data;
  state->event_data2 = NULL;
  state->event_len = len;
  state->event_off = 0u;
  state->event_phase = 0u;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__writer_string_reader_emit_byte(lonejson_writer *writer,
                                         unsigned char byte,
                                         lonejson_error *error) {
  char escape_buf[7];

  if (byte >= 0x20u && byte != '"' && byte != '\\') {
    return lonejson__writer_emit(writer, &byte, 1u, error);
  }
  switch (byte) {
  case '"':
    return lonejson__writer_emit_cstr(writer, "\\\"", error);
  case '\\':
    return lonejson__writer_emit_cstr(writer, "\\\\", error);
  case '\b':
    return lonejson__writer_emit_cstr(writer, "\\b", error);
  case '\f':
    return lonejson__writer_emit_cstr(writer, "\\f", error);
  case '\n':
    return lonejson__writer_emit_cstr(writer, "\\n", error);
  case '\r':
    return lonejson__writer_emit_cstr(writer, "\\r", error);
  case '\t':
    return lonejson__writer_emit_cstr(writer, "\\t", error);
  default:
    escape_buf[0] = '\\';
    escape_buf[1] = 'u';
    escape_buf[2] = '0';
    escape_buf[3] = '0';
    escape_buf[4] = "0123456789abcdef"[(byte >> 4u) & 0x0Fu];
    escape_buf[5] = "0123456789abcdef"[byte & 0x0Fu];
    escape_buf[6] = '\0';
    return lonejson__writer_emit(writer, escape_buf, 6u, error);
  }
}

static lonejson_status
lonejson__writer_emit_escaped_event(lonejson_writer *writer,
                                    lonejson_error *error) {
  lonejson__writer_state *state;
  const unsigned char *data;
  lonejson_status status;

  state = (lonejson__writer_state *)writer->state;
  data = (const unsigned char *)state->event_data;
  while (state->event_off < state->event_len) {
    if (lonejson__writer_output_blocked(writer)) {
      return LONEJSON_STATUS_TRUNCATED;
    }
    status = lonejson__writer_string_reader_emit_byte(
        writer, data[state->event_off], error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    state->event_off++;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__writer_emit_bytes_event(lonejson_writer *writer,
                                  lonejson_error *error) {
  lonejson__writer_state *state;
  const unsigned char *data;
  lonejson_status status;

  state = (lonejson__writer_state *)writer->state;
  data = (const unsigned char *)state->event_data;
  while (state->event_off < state->event_len) {
    if (lonejson__writer_output_blocked(writer)) {
      return LONEJSON_STATUS_TRUNCATED;
    }
    status = lonejson__writer_emit(writer, data + state->event_off, 1u, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    state->event_off++;
  }
  return LONEJSON_STATUS_OK;
}

static size_t
lonejson__writer_format_u64(char *buffer, size_t capacity,
                            lonejson_uint64 value) {
  size_t idx;

  idx = capacity;
  do {
    lonejson_uint64 digit = value % 10u;
    value /= 10u;
    --idx;
    buffer[idx] = (char)('0' + (int)digit);
  } while (value != 0u);
  memmove(buffer, buffer + idx, capacity - idx);
  return capacity - idx;
}

static size_t
lonejson__writer_format_i64(char *buffer, size_t capacity,
                            lonejson_int64 value) {
  lonejson_uint64 magnitude;
  size_t len;

  if (value >= 0) {
    return lonejson__writer_format_u64(buffer, capacity,
                                       (lonejson_uint64)value);
  }
  buffer[0] = '-';
  magnitude = (lonejson_uint64)(-(value + 1)) + 1u;
  len = lonejson__writer_format_u64(buffer + 1u, capacity - 1u, magnitude);
  return len + 1u;
}

static lonejson_status lonejson__writer_emit_scalar_bytes(
    lonejson_writer *writer, const void *data, size_t len,
    lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson_status status;

  if (writer == NULL || writer->state == NULL ||
      (data == NULL && len != 0u) || len > 64u) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "writer scalar bytes are invalid");
  }
  state = (lonejson__writer_state *)writer->state;
  if (state->event_kind == LONEJSON__WRITER_EVENT_SCALAR_BYTES) {
    if (state->event_len != len ||
        memcmp(state->event_buffer, data, len) != 0) {
      return lonejson__writer_set_error(
          writer, error, LONEJSON_STATUS_INVALID_ARGUMENT,
          "active writer event must be resumed with the same arguments");
    }
  } else if (state->event_kind == LONEJSON__WRITER_EVENT_NONE) {
    memcpy(state->event_buffer, data, len);
    status = lonejson__writer_begin_event(
        writer, LONEJSON__WRITER_EVENT_SCALAR_BYTES, state->event_buffer, len,
        error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  } else {
    return lonejson__writer_begin_event(
        writer, LONEJSON__WRITER_EVENT_SCALAR_BYTES, state->event_buffer, len,
        error);
  }
  if (state->event_phase == 0u) {
    status = lonejson__writer_before_value(writer, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    state->event_phase = 1u;
  }
  status = lonejson__writer_emit_bytes_event(writer, error);
  if (status == LONEJSON_STATUS_OK) {
    lonejson__writer_clear_event(state);
  }
  return status;
}

static lonejson_status
lonejson__writer_push_frame(lonejson_writer *writer,
                            lonejson__writer_frame_kind kind,
                            lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson__writer_frame *next;
  size_t next_capacity;

  state = (lonejson__writer_state *)writer->state;
  if (state->frame_count == state->frame_capacity) {
    next_capacity = state->frame_capacity == 0u ? 8u : state->frame_capacity * 2u;
    next = (lonejson__writer_frame *)lonejson__buffer_realloc(
        &state->allocator, state->frames,
        state->frame_capacity * sizeof(*state->frames),
        next_capacity * sizeof(*state->frames));
    if (next == NULL) {
      return lonejson__writer_set_error(writer, error,
                                        LONEJSON_STATUS_ALLOCATION_FAILED,
                                        "failed to grow writer stack");
    }
    state->frames = next;
    state->frame_capacity = next_capacity;
  }
  memset(&state->frames[state->frame_count], 0,
         sizeof(state->frames[state->frame_count]));
  state->frames[state->frame_count].kind = kind;
  state->frame_count++;
  return LONEJSON_STATUS_OK;
}

static lonejson__writer_frame *
lonejson__writer_top(lonejson__writer_state *state) {
  if (state->frame_count == 0u) {
    return NULL;
  }
  return &state->frames[state->frame_count - 1u];
}

static lonejson_status lonejson__writer_before_value(lonejson_writer *writer,
                                                     lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson__writer_frame *frame;
  lonejson_status status;

  if (writer == NULL || writer->state == NULL) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "writer is required");
  }
  state = (lonejson__writer_state *)writer->state;
  if (state->string_open) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer string is still open");
  }
  if (state->finished) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "writer is already finished");
  }
  if (lonejson__writer_output_blocked(writer)) {
    return LONEJSON_STATUS_TRUNCATED;
  }
  frame = lonejson__writer_top(state);
  if (frame == NULL) {
    if (state->root_written) {
      return lonejson__writer_set_error(writer, error,
                                        LONEJSON_STATUS_INVALID_JSON,
                                        "writer already has a root value");
    }
    state->root_written = 1;
    return LONEJSON_STATUS_OK;
  }
  if (frame->kind == LONEJSON__WRITER_FRAME_ARRAY) {
    if (frame->count != 0u) {
      status = lonejson__writer_emit_cstr(writer, ",", error);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
    frame->count++;
    return LONEJSON_STATUS_OK;
  }
  if (!frame->after_key) {
    return lonejson__writer_set_error(
        writer, error, LONEJSON_STATUS_INVALID_JSON,
        "object values must be preceded by a writer key");
  }
  frame->after_key = 0;
  frame->count++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__writer_emit_value_bytes(
    lonejson_writer *writer, const void *data, size_t len,
    lonejson_error *error) {
  lonejson_status status;

  status = lonejson__writer_before_value(writer, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__writer_emit(writer, data, len, error);
}

lonejson_status lonejson_writer_init_sink(
    lonejson_writer *writer, lonejson_sink_fn sink, void *sink_user,
    const lonejson_write_options *options, lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson_allocator allocator;

  if (writer == NULL || sink == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "writer and sink are required");
  }
  memset(writer, 0, sizeof(*writer));
  if (options != NULL &&
      !LONEJSON__ALLOCATOR_IS_VALID_CONFIG(options->allocator)) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "allocator must provide either all callbacks or none");
  }
  allocator = lonejson__allocator_resolve(options ? options->allocator : NULL);
  state = (lonejson__writer_state *)lonejson__buffer_alloc(&allocator,
                                                           sizeof(*state));
  if (state == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to allocate writer state");
  }
  memset(state, 0, sizeof(*state));
  state->mode = sink == lonejson__writer_generator_sink
                    ? LONEJSON__WRITER_MODE_GENERATOR
                    : LONEJSON__WRITER_MODE_SINK;
  state->options = options ? *options : lonejson_default_write_options();
  state->allocator = allocator;
  state->sink = sink;
  state->sink_user = sink_user;
  state->external_error = error;
  writer->state = state;
  lonejson__clear_error(&writer->error);
  return LONEJSON_STATUS_OK;
}

void lonejson_writer_cleanup(lonejson_writer *writer) {
  lonejson__writer_state *state;

  if (writer == NULL || writer->state == NULL) {
    return;
  }
  state = (lonejson__writer_state *)writer->state;
  lonejson__writer_clear_event(state);
  lonejson__buffer_free(&state->allocator, state->frames,
                        state->frame_capacity * sizeof(*state->frames));
  lonejson__buffer_free(&state->allocator, state, sizeof(*state));
  memset(writer, 0, sizeof(*writer));
}

lonejson_status lonejson_writer_begin_object(lonejson_writer *writer,
                                             lonejson_error *error) {
  lonejson_status status;

  status = lonejson__writer_before_value(writer, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__writer_emit_cstr(writer, "{", error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__writer_push_frame(writer, LONEJSON__WRITER_FRAME_OBJECT,
                                     error);
}

lonejson_status lonejson_writer_end_object(lonejson_writer *writer,
                                           lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson__writer_frame *frame;
  lonejson_status status;

  if (writer == NULL || writer->state == NULL) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "writer is required");
  }
  state = (lonejson__writer_state *)writer->state;
  if (state->string_open) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer string is still open");
  }
  frame = lonejson__writer_top(state);
  if (frame == NULL || frame->kind != LONEJSON__WRITER_FRAME_OBJECT) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer is not inside an object");
  }
  if (frame->after_key) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "object key is missing a value");
  }
  status = lonejson__writer_emit_cstr(writer, "}", error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  state->frame_count--;
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_writer_begin_array(lonejson_writer *writer,
                                            lonejson_error *error) {
  lonejson_status status;

  status = lonejson__writer_before_value(writer, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__writer_emit_cstr(writer, "[", error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__writer_push_frame(writer, LONEJSON__WRITER_FRAME_ARRAY,
                                     error);
}

lonejson_status lonejson_writer_end_array(lonejson_writer *writer,
                                          lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson__writer_frame *frame;
  lonejson_status status;

  if (writer == NULL || writer->state == NULL) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "writer is required");
  }
  state = (lonejson__writer_state *)writer->state;
  if (state->string_open) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer string is still open");
  }
  frame = lonejson__writer_top(state);
  if (frame == NULL || frame->kind != LONEJSON__WRITER_FRAME_ARRAY) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer is not inside an array");
  }
  status = lonejson__writer_emit_cstr(writer, "]", error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  state->frame_count--;
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_writer_key(lonejson_writer *writer, const char *key,
                                    size_t key_len, lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson__writer_frame *frame;
  lonejson_status status;

  if (writer == NULL || writer->state == NULL ||
      (key == NULL && key_len != 0u)) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "writer and key are required");
  }
  state = (lonejson__writer_state *)writer->state;
  status = lonejson__writer_begin_event(writer, LONEJSON__WRITER_EVENT_KEY, key,
                                        key_len, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (state->event_phase == 0u) {
    if (state->string_open) {
      return lonejson__writer_set_error(writer, error,
                                        LONEJSON_STATUS_INVALID_JSON,
                                        "writer string is still open");
    }
    if (lonejson__writer_output_blocked(writer)) {
      return LONEJSON_STATUS_TRUNCATED;
    }
    frame = lonejson__writer_top(state);
    if (frame == NULL || frame->kind != LONEJSON__WRITER_FRAME_OBJECT) {
      return lonejson__writer_set_error(writer, error,
                                        LONEJSON_STATUS_INVALID_JSON,
                                        "writer key requires an object");
    }
    if (frame->after_key) {
      return lonejson__writer_set_error(
          writer, error, LONEJSON_STATUS_INVALID_JSON,
          "previous object key is missing a value");
    }
    if (frame->count != 0u) {
      status = lonejson__writer_emit_cstr(writer, ",", error);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
    state->event_phase = 1u;
  }
  if (state->event_phase == 1u) {
    if (lonejson__writer_output_blocked(writer)) {
      return LONEJSON_STATUS_TRUNCATED;
    }
    status = lonejson__writer_emit_cstr(writer, "\"", error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    state->event_phase = 2u;
  }
  if (state->event_phase == 2u) {
    status = lonejson__writer_emit_escaped_event(writer, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    state->event_phase = 3u;
  }
  if (state->event_phase == 3u) {
    if (lonejson__writer_output_blocked(writer)) {
      return LONEJSON_STATUS_TRUNCATED;
    }
    status = lonejson__writer_emit_cstr(writer, "\":", error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  frame = lonejson__writer_top(state);
  frame->after_key = 1;
  lonejson__writer_clear_event(state);
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_writer_string(lonejson_writer *writer,
                                       const void *data, size_t len,
                                       lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson_status status;

  if (data == NULL && len != 0u) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "string data is required");
  }
  status = lonejson__writer_begin_event(writer, LONEJSON__WRITER_EVENT_STRING,
                                        data, len, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  state = (lonejson__writer_state *)writer->state;
  if (state->event_phase == 0u) {
    status = lonejson__writer_before_value(writer, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    state->event_phase = 1u;
  }
  if (state->event_phase == 1u) {
    if (lonejson__writer_output_blocked(writer)) {
      return LONEJSON_STATUS_TRUNCATED;
    }
    status = lonejson__writer_emit_cstr(writer, "\"", error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    state->event_phase = 2u;
  }
  if (state->event_phase == 2u) {
    status = lonejson__writer_emit_escaped_event(writer, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    state->event_phase = 3u;
  }
  if (lonejson__writer_output_blocked(writer)) {
    return LONEJSON_STATUS_TRUNCATED;
  }
  status = lonejson__writer_emit_cstr(writer, "\"", error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  lonejson__writer_clear_event(state);
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_writer_string_begin(lonejson_writer *writer,
                                             lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson_status status;

  status = lonejson__writer_before_value(writer, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  state = (lonejson__writer_state *)writer->state;
  status = lonejson__writer_emit_cstr(writer, "\"", error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  state->string_open = 1;
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_writer_string_chunk(lonejson_writer *writer,
                                             const void *data, size_t len,
                                             lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson_status status;

  if (writer == NULL || writer->state == NULL ||
      (data == NULL && len != 0u)) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "writer and string chunk are required");
  }
  state = (lonejson__writer_state *)writer->state;
  if (!state->string_open) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer string is not open");
  }
  status = lonejson__writer_begin_event(
      writer, LONEJSON__WRITER_EVENT_STRING_CHUNK, data, len, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__writer_emit_escaped_event(writer, error);
  if (status == LONEJSON_STATUS_OK) {
    lonejson__writer_clear_event(state);
  }
  return status;
}

lonejson_status lonejson_writer_string_end(lonejson_writer *writer,
                                           lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson_status status;

  if (writer == NULL || writer->state == NULL) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "writer is required");
  }
  state = (lonejson__writer_state *)writer->state;
  if (!state->string_open) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer string is not open");
  }
  status = lonejson__writer_emit_cstr(writer, "\"", error);
  if (status == LONEJSON_STATUS_OK) {
    state->string_open = 0;
  }
  return status;
}

lonejson_status lonejson_writer_string_reader(lonejson_writer *writer,
                                              lonejson_reader_fn reader,
                                              void *reader_user,
                                              lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson_read_result rr;
  lonejson_status status;

  if (reader == NULL) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "string reader is required");
  }
  if (writer == NULL || writer->state == NULL) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "writer is required");
  }
  state = (lonejson__writer_state *)writer->state;
  if (state->string_reader_active) {
    if (state->string_reader != reader ||
        state->string_reader_user != reader_user) {
      return lonejson__writer_set_error(
          writer, error, LONEJSON_STATUS_INVALID_ARGUMENT,
          "active string reader must be resumed with the same reader");
    }
  } else {
    status = lonejson_writer_string_begin(writer, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    state->string_reader_active = 1;
    state->string_reader = reader;
    state->string_reader_user = reader_user;
    state->string_reader_buffer_len = 0u;
    state->string_reader_buffer_off = 0u;
    state->string_reader_eof = 0;
  }
  for (;;) {
    while (state->string_reader_buffer_off <
           state->string_reader_buffer_len) {
      if (lonejson__writer_output_blocked(writer)) {
        return LONEJSON_STATUS_TRUNCATED;
      }
      status = lonejson__writer_string_reader_emit_byte(
          writer, state->string_reader_buffer[state->string_reader_buffer_off],
          error);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      state->string_reader_buffer_off++;
    }
    state->string_reader_buffer_len = 0u;
    state->string_reader_buffer_off = 0u;
    if (state->string_reader_eof) {
      break;
    }
    if (lonejson__writer_output_blocked(writer)) {
      return LONEJSON_STATUS_TRUNCATED;
    }
    rr = reader(reader_user, state->string_reader_buffer,
                sizeof(state->string_reader_buffer));
    if (rr.error_code != 0) {
      return lonejson__writer_set_error(writer, error,
                                        LONEJSON_STATUS_IO_ERROR,
                                        "failed to read JSON string source");
    }
    if (rr.would_block) {
      return lonejson__writer_set_error(writer, error,
                                        LONEJSON_STATUS_CALLBACK_FAILED,
                                        "string reader would block");
    }
    state->string_reader_buffer_len = rr.bytes_read;
    state->string_reader_eof = rr.eof;
  }
  if (lonejson__writer_output_blocked(writer)) {
    return LONEJSON_STATUS_TRUNCATED;
  }
  status = lonejson_writer_string_end(writer, error);
  if (status == LONEJSON_STATUS_OK) {
    state->string_reader_active = 0;
    state->string_reader = NULL;
    state->string_reader_user = NULL;
    state->string_reader_buffer_len = 0u;
    state->string_reader_buffer_off = 0u;
    state->string_reader_eof = 0;
  }
  return status;
}

static lonejson_status lonejson__writer_child_init(
    lonejson_writer *writer, const lonejson__generator_frame *root,
    lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson__generator_state *child;

  state = (lonejson__writer_state *)writer->state;
  child = (lonejson__generator_state *)lonejson__buffer_alloc(
      &state->allocator, sizeof(*child));
  if (child == NULL) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_ALLOCATION_FAILED,
                                      "failed to allocate writer child generator");
  }
  memset(child, 0, sizeof(*child));
  child->magic = LONEJSON__GENERATOR_MAGIC;
  child->allocator = state->allocator;
  child->options = state->options;
  if (lonejson__generator_push_frame(child, root,
                                     error != NULL ? error : &writer->error) !=
      LONEJSON_STATUS_OK) {
    lonejson__buffer_free(&state->allocator, child, sizeof(*child));
    return (error != NULL ? error->code : writer->error.code);
  }
  memset(&state->event_child, 0, sizeof(state->event_child));
  state->event_child.state = child;
  state->event_child.eof = 0;
  lonejson__clear_error(&state->event_child.error);
  state->event_child_active = 1;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__writer_child_event(
    lonejson_writer *writer, lonejson__writer_event_kind kind,
    const void *arg0, const void *arg1, size_t arg_len,
    const lonejson__generator_frame *root, lonejson_error *error) {
  lonejson__writer_state *state;
  size_t output_available;
  size_t read_capacity;
  size_t out_len;
  int eof;
  lonejson_status status;

  status = lonejson__writer_begin_event(writer, kind, arg0, arg_len, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  state = (lonejson__writer_state *)writer->state;
  if (state->event_phase == 0u) {
    status = lonejson__writer_before_value(writer, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    state->event_data2 = arg1;
    status = lonejson__writer_child_init(writer, root, error);
    if (status != LONEJSON_STATUS_OK) {
      lonejson__writer_clear_event(state);
      return status;
    }
    state->event_phase = 1u;
  } else if (state->event_data2 != arg1) {
    return lonejson__writer_set_error(
        writer, error, LONEJSON_STATUS_INVALID_ARGUMENT,
        "active writer event must be resumed with the same arguments");
  }
  for (;;) {
    if (lonejson__writer_output_blocked(writer)) {
      return LONEJSON_STATUS_TRUNCATED;
    }
    read_capacity = sizeof(state->event_child_buffer);
    if (state->mode == LONEJSON__WRITER_MODE_GENERATOR) {
      output_available =
          lonejson__writer_generator_output_available(state->sink_user);
      if (output_available == 0u) {
        return LONEJSON_STATUS_TRUNCATED;
      }
      if (read_capacity > output_available) {
        read_capacity = output_available;
      }
    }
    status = lonejson_generator_read(&state->event_child,
                                     state->event_child_buffer, read_capacity,
                                     &out_len, &eof);
    if (status != LONEJSON_STATUS_OK) {
      if (error != NULL) {
        *error = state->event_child.error;
      } else {
        writer->error = state->event_child.error;
      }
      return status;
    }
    if (out_len != 0u) {
      status = lonejson__writer_emit(writer, state->event_child_buffer, out_len,
                                     error);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      if (lonejson__writer_output_blocked(writer)) {
        return LONEJSON_STATUS_TRUNCATED;
      }
    }
    if (eof) {
      break;
    }
    if (out_len == 0u) {
      return LONEJSON_STATUS_TRUNCATED;
    }
  }
  lonejson__writer_clear_event(state);
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_writer_string_spooled(
    lonejson_writer *writer, const lonejson_spooled *value,
    lonejson_error *error) {
  lonejson__generator_frame root;

  if (value == NULL) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "spooled string value is required");
  }
  memset(&root, 0, sizeof(root));
  root.kind = LONEJSON__GEN_FRAME_SPOOLED_TEXT;
  root.u.spooled.cursor = *value;
  return lonejson__writer_child_event(
      writer, LONEJSON__WRITER_EVENT_CHILD, value, NULL, 1u, &root, error);
}

lonejson_status lonejson_writer_source_text(lonejson_writer *writer,
                                            const lonejson_source *value,
                                            lonejson_error *error) {
  lonejson__generator_frame root;

  if (value == NULL) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "source text value is required");
  }
  if (value->kind == LONEJSON_SOURCE_NONE) {
    return lonejson__writer_emit_value_bytes(writer, "null", 4u, error);
  }
  memset(&root, 0, sizeof(root));
  root.kind = LONEJSON__GEN_FRAME_SOURCE_TEXT;
  root.u.source.value = value;
  return lonejson__writer_child_event(
      writer, LONEJSON__WRITER_EVENT_CHILD, value, NULL, 2u, &root, error);
}

lonejson_status lonejson_writer_spooled_base64(
    lonejson_writer *writer, const lonejson_spooled *value,
    lonejson_error *error) {
  lonejson__generator_frame root;

  if (value == NULL) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "spooled base64 value is required");
  }
  memset(&root, 0, sizeof(root));
  root.kind = LONEJSON__GEN_FRAME_SPOOLED_BASE64;
  root.u.spooled.cursor = *value;
  return lonejson__writer_child_event(
      writer, LONEJSON__WRITER_EVENT_CHILD, value, NULL, 3u, &root, error);
}

lonejson_status lonejson_writer_source_base64(lonejson_writer *writer,
                                              const lonejson_source *value,
                                              lonejson_error *error) {
  lonejson__generator_frame root;

  if (value == NULL) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "source base64 value is required");
  }
  if (value->kind == LONEJSON_SOURCE_NONE) {
    return lonejson__writer_emit_value_bytes(writer, "null", 4u, error);
  }
  memset(&root, 0, sizeof(root));
  root.kind = LONEJSON__GEN_FRAME_SOURCE_BASE64;
  root.u.source.value = value;
  return lonejson__writer_child_event(
      writer, LONEJSON__WRITER_EVENT_CHILD, value, NULL, 4u, &root, error);
}

lonejson_status lonejson_writer_number_text(lonejson_writer *writer,
                                            const char *data, size_t len,
                                            lonejson_error *error) {
  lonejson__writer_state *state;
  lonejson_status status;

  if (data == NULL || len == 0u) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "number text is required");
  }
  if (!lonejson__is_valid_json_number(data, len)) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "number text must be a JSON number");
  }
  status = lonejson__writer_begin_event(writer, LONEJSON__WRITER_EVENT_BYTES,
                                        data, len, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  state = (lonejson__writer_state *)writer->state;
  if (state->event_phase == 0u) {
    status = lonejson__writer_before_value(writer, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    state->event_phase = 1u;
  }
  status = lonejson__writer_emit_bytes_event(writer, error);
  if (status == LONEJSON_STATUS_OK) {
    lonejson__writer_clear_event(state);
  }
  return status;
}

lonejson_status lonejson_writer_i64(lonejson_writer *writer,
                                    lonejson_int64 value,
                                    lonejson_error *error) {
  char buffer[32];
  size_t len;

  len = lonejson__writer_format_i64(buffer, sizeof(buffer), value);
  return lonejson__writer_emit_scalar_bytes(writer, buffer, len, error);
}

lonejson_status lonejson_writer_u64(lonejson_writer *writer,
                                    lonejson_uint64 value,
                                    lonejson_error *error) {
  char buffer[32];
  size_t len;

  len = lonejson__writer_format_u64(buffer, sizeof(buffer), value);
  return lonejson__writer_emit_scalar_bytes(writer, buffer, len, error);
}

lonejson_status lonejson_writer_f64(lonejson_writer *writer, double value,
                                    lonejson_error *error) {
  char buffer[64];
  int len;

  if (!lonejson__is_finite_f64(value)) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "JSON numbers must be finite");
  }
  len = snprintf(buffer, sizeof(buffer), "%.17g", value);
  if (len < 0) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INTERNAL_ERROR,
                                      "failed to format number");
  }
  return lonejson__writer_emit_scalar_bytes(writer, buffer, (size_t)len, error);
}

lonejson_status lonejson_writer_bool(lonejson_writer *writer, int value,
                                     lonejson_error *error) {
  return lonejson__writer_emit_scalar_bytes(
      writer, value ? "true" : "false", value ? 4u : 5u, error);
}

lonejson_status lonejson_writer_null(lonejson_writer *writer,
                                     lonejson_error *error) {
  return lonejson__writer_emit_scalar_bytes(writer, "null", 4u, error);
}

lonejson_status lonejson_writer_json_value(
    lonejson_writer *writer, const lonejson_json_value *value,
    lonejson_error *error) {
  lonejson__generator_frame root;

  if (value == NULL) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "JSON value is required");
  }
  memset(&root, 0, sizeof(root));
  root.kind = LONEJSON__GEN_FRAME_JSON_VALUE;
  root.u.json_value.value = value;
  return lonejson__writer_child_event(
      writer, LONEJSON__WRITER_EVENT_CHILD, value, NULL, 5u, &root, error);
}

lonejson_status lonejson_writer_mapped(lonejson_writer *writer,
                                       const lonejson_map *map,
                                       const void *src,
                                       lonejson_error *error) {
  lonejson__generator_frame root;

  if (map == NULL || src == NULL) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "map and source are required");
  }
  memset(&root, 0, sizeof(root));
  root.kind = LONEJSON__GEN_FRAME_MAP;
  root.u.map.map = map;
  root.u.map.src = src;
  return lonejson__writer_child_event(
      writer, LONEJSON__WRITER_EVENT_CHILD, map, src, 6u, &root, error);
}

lonejson_status lonejson_writer_finish(lonejson_writer *writer,
                                       lonejson_error *error) {
  lonejson__writer_state *state;

  if (writer == NULL || writer->state == NULL) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_ARGUMENT,
                                      "writer is required");
  }
  state = (lonejson__writer_state *)writer->state;
  if (state->string_open) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer string is still open");
  }
  if (!state->root_written) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer emitted no JSON value");
  }
  if (state->frame_count != 0u) {
    return lonejson__writer_set_error(writer, error,
                                      LONEJSON_STATUS_INVALID_JSON,
                                      "writer has unclosed containers");
  }
  state->finished = 1;
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_write_json_string_sink(
    lonejson_reader_fn reader, void *reader_user, lonejson_sink_fn sink,
    void *sink_user, const lonejson_write_options *options,
    lonejson_error *error) {
  lonejson_writer writer;
  lonejson_status status;

  status = lonejson_writer_init_sink(&writer, sink, sink_user, options, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson_writer_string_reader(&writer, reader, reader_user, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_writer_finish(&writer, error);
  }
  lonejson_writer_cleanup(&writer);
  return status;
}

lonejson_status lonejson_write_json_string_buffer_sink(
    const void *data, size_t len, lonejson_sink_fn sink, void *sink_user,
    const lonejson_write_options *options, lonejson_error *error) {
  lonejson_writer writer;
  lonejson_status status;

  status = lonejson_writer_init_sink(&writer, sink, sink_user, options, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson_writer_string(&writer, data, len, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_writer_finish(&writer, error);
  }
  lonejson_writer_cleanup(&writer);
  return status;
}

lonejson_status lonejson_write_json_string_spooled_sink(
    const lonejson_spooled *value, lonejson_sink_fn sink, void *sink_user,
    const lonejson_write_options *options, lonejson_error *error) {
  lonejson_writer writer;
  lonejson_status status;

  status = lonejson_writer_init_sink(&writer, sink, sink_user, options, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson_writer_string_spooled(&writer, value, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_writer_finish(&writer, error);
  }
  lonejson_writer_cleanup(&writer);
  return status;
}
