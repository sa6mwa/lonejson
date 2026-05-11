typedef enum lonejson__generator_pending_kind {
  LONEJSON__GEN_PENDING_NONE = 0,
  LONEJSON__GEN_PENDING_BYTES = 1,
  LONEJSON__GEN_PENDING_REPEAT = 2
} lonejson__generator_pending_kind;

typedef enum lonejson__generator_frame_kind {
  LONEJSON__GEN_FRAME_MAP = 0,
  LONEJSON__GEN_FRAME_STRING = 1,
  LONEJSON__GEN_FRAME_STRING_ARRAY = 2,
  LONEJSON__GEN_FRAME_I64_ARRAY = 3,
  LONEJSON__GEN_FRAME_U64_ARRAY = 4,
  LONEJSON__GEN_FRAME_F64_ARRAY = 5,
  LONEJSON__GEN_FRAME_BOOL_ARRAY = 6,
  LONEJSON__GEN_FRAME_OBJECT_ARRAY = 7,
  LONEJSON__GEN_FRAME_SOURCE_TEXT = 8,
  LONEJSON__GEN_FRAME_SOURCE_BASE64 = 9,
  LONEJSON__GEN_FRAME_SPOOLED_TEXT = 10,
  LONEJSON__GEN_FRAME_SPOOLED_BASE64 = 11,
  LONEJSON__GEN_FRAME_JSON_VALUE = 12
} lonejson__generator_frame_kind;

typedef struct lonejson__generator_frame {
  lonejson__generator_frame_kind kind;
  unsigned phase;
  size_t depth;
  size_t index;
  size_t aux;
  size_t emitted;
  union {
    struct {
      const lonejson_map *map;
      const void *src;
    } map;
    struct {
      const unsigned char *text;
      size_t offset;
    } string;
    struct {
      const lonejson_string_array *arr;
    } string_array;
    struct {
      const lonejson_i64_array *arr;
    } i64_array;
    struct {
      const lonejson_u64_array *arr;
    } u64_array;
    struct {
      const lonejson_f64_array *arr;
    } f64_array;
    struct {
      const lonejson_bool_array *arr;
    } bool_array;
    struct {
      const lonejson_object_array *arr;
      const lonejson_field *field;
    } object_array;
    struct {
      const lonejson_source *value;
      lonejson__source_cursor cursor;
      int opened;
      unsigned char carry[3];
      size_t carry_len;
    } source;
    struct {
      lonejson_spooled cursor;
      int rewound;
      unsigned char carry[3];
      size_t carry_len;
    } spooled;
    struct {
      const lonejson_json_value *value;
    } json_value;
  } u;
} lonejson__generator_frame;

typedef struct lonejson__generator_state {
  lonejson_allocator allocator;
  lonejson_write_options options;
  lonejson__generator_frame *frames;
  size_t frame_count;
  size_t frame_capacity;
  unsigned char *out;
  size_t out_capacity;
  size_t out_length;
  lonejson__generator_pending_kind pending_kind;
  const unsigned char *pending_ptr;
  size_t pending_len;
  size_t pending_off;
  unsigned char pending_repeat_byte;
  size_t pending_repeat_count;
  unsigned char scratch[128];
  unsigned char io_buffer[4096];
  size_t io_buffer_len;
  size_t io_buffer_off;
  int io_eof;
  size_t io_owner_index;
  int io_owner_valid;
  lonejson__json_pull_state json_pull;
  size_t json_owner_index;
  const lonejson_json_value *json_owner_value;
  int json_owner_valid;
  int finished;
} lonejson__generator_state;

static void
lonejson__generator_clear_pending(lonejson__generator_state *state) {
  state->pending_kind = LONEJSON__GEN_PENDING_NONE;
  state->pending_ptr = NULL;
  state->pending_len = 0u;
  state->pending_off = 0u;
  state->pending_repeat_count = 0u;
}

static lonejson_status
lonejson__generator_set_pending_bytes(lonejson__generator_state *state,
                                      const void *data, size_t len) {
  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  state->pending_kind = LONEJSON__GEN_PENDING_BYTES;
  state->pending_ptr = (const unsigned char *)data;
  state->pending_len = len;
  state->pending_off = 0u;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__generator_set_pending_repeat(lonejson__generator_state *state,
                                       unsigned char byte, size_t count) {
  if (count == 0u) {
    return LONEJSON_STATUS_OK;
  }
  state->pending_kind = LONEJSON__GEN_PENDING_REPEAT;
  state->pending_repeat_byte = byte;
  state->pending_repeat_count = count;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__generator_set_pending_scratch(lonejson__generator_state *state,
                                        const void *data, size_t len) {
  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  memcpy(state->scratch, data, len);
  state->pending_kind = LONEJSON__GEN_PENDING_BYTES;
  state->pending_ptr = state->scratch;
  state->pending_len = len;
  state->pending_off = 0u;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__generator_flush_pending(lonejson__generator_state *state) {
  size_t writable;
  size_t take;

  if (state->pending_kind == LONEJSON__GEN_PENDING_NONE ||
      state->out_length >= state->out_capacity) {
    return LONEJSON_STATUS_OK;
  }
  writable = state->out_capacity - state->out_length;
  if (state->pending_kind == LONEJSON__GEN_PENDING_BYTES) {
    take = state->pending_len - state->pending_off;
    if (take > writable) {
      take = writable;
    }
    memcpy(state->out + state->out_length,
           state->pending_ptr + state->pending_off, take);
    state->out_length += take;
    state->pending_off += take;
    if (state->pending_off == state->pending_len) {
      lonejson__generator_clear_pending(state);
    }
    return LONEJSON_STATUS_OK;
  }
  take = state->pending_repeat_count;
  if (take > writable) {
    take = writable;
  }
  memset(state->out + state->out_length, (int)state->pending_repeat_byte, take);
  state->out_length += take;
  state->pending_repeat_count -= take;
  if (state->pending_repeat_count == 0u) {
    lonejson__generator_clear_pending(state);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__generator_push_frame(lonejson__generator_state *state,
                               const lonejson__generator_frame *frame,
                               lonejson_error *error) {
  lonejson__generator_frame *next;
  size_t next_cap;

  if (state->frame_count == state->frame_capacity) {
    next_cap =
        (state->frame_capacity == 0u) ? 16u : (state->frame_capacity * 2u);
    next = (lonejson__generator_frame *)lonejson__buffer_realloc(
        &state->allocator, state->frames,
        state->frame_capacity * sizeof(*state->frames),
        next_cap * sizeof(*state->frames));
    if (next == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u,
                                 "failed to grow generator frame stack");
    }
    state->frames = next;
    state->frame_capacity = next_cap;
  }
  state->frames[state->frame_count++] = *frame;
  return LONEJSON_STATUS_OK;
}

static void lonejson__generator_pop_frame(lonejson__generator_state *state) {
  if (state->frame_count != 0u) {
    state->frame_count--;
    if (state->io_owner_valid && state->io_owner_index >= state->frame_count) {
      state->io_owner_valid = 0;
      state->io_buffer_len = 0u;
      state->io_buffer_off = 0u;
      state->io_eof = 0;
    }
    if (state->json_owner_valid &&
        state->json_owner_index >= state->frame_count) {
      lonejson__json_pull_cleanup(&state->json_pull);
      state->json_owner_valid = 0;
      state->json_owner_index = 0u;
      state->json_owner_value = NULL;
    }
  }
}

static lonejson_status
lonejson__generator_push_string(lonejson__generator_state *state,
                                const char *text, size_t depth,
                                lonejson_error *error) {
  lonejson__generator_frame frame;

  memset(&frame, 0, sizeof(frame));
  frame.kind = LONEJSON__GEN_FRAME_STRING;
  frame.depth = depth;
  frame.u.string.text = (const unsigned char *)((text != NULL) ? text : "");
  return lonejson__generator_push_frame(state, &frame, error);
}

static lonejson_status
lonejson__generator_format_f64(lonejson__generator_state *state, double value,
                               lonejson_error *error) {
  int written;

  if (!lonejson__is_finite_f64(value)) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_TYPE_MISMATCH, error ? error->offset : 0u,
        error ? error->line : 0u, error ? error->column : 0u,
        "non-finite double cannot be serialized");
  }
  written =
      snprintf((char *)state->scratch, sizeof(state->scratch), "%.17g", value);
  if (written < 0 || (size_t)written >= sizeof(state->scratch)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INTERNAL_ERROR, 0u, 0u,
                               0u, "failed to format number");
  }
  return lonejson__generator_set_pending_scratch(state, state->scratch,
                                                 (size_t)written);
}

static lonejson_status
lonejson__generator_format_u64(lonejson__generator_state *state,
                               lonejson_uint64 value) {
  size_t idx;

  idx = sizeof(state->scratch);
  do {
    lonejson_uint64 digit;
    digit = value % 10u;
    value /= 10u;
    --idx;
    state->scratch[idx] = (unsigned char)('0' + (int)digit);
  } while (value != 0u);
  return lonejson__generator_set_pending_bytes(state, state->scratch + idx,
                                               sizeof(state->scratch) - idx);
}

static lonejson_status
lonejson__generator_format_i64(lonejson__generator_state *state,
                               lonejson_int64 value) {
  lonejson_uint64 magnitude;
  size_t idx;

  idx = sizeof(state->scratch);
  magnitude = (value < 0) ? ((lonejson_uint64)(-(value + 1)) + 1u)
                          : (lonejson_uint64)value;
  do {
    lonejson_uint64 digit;
    digit = magnitude % 10u;
    magnitude /= 10u;
    --idx;
    state->scratch[idx] = (unsigned char)('0' + (int)digit);
  } while (magnitude != 0u);
  if (value < 0) {
    --idx;
    state->scratch[idx] = '-';
  }
  return lonejson__generator_set_pending_bytes(state, state->scratch + idx,
                                               sizeof(state->scratch) - idx);
}

static lonejson_status lonejson__generator_push_value_from_field(
    lonejson__generator_state *state, const lonejson_field *field,
    const void *ptr, size_t depth, lonejson_error *error) {
  lonejson__generator_frame frame;

  memset(&frame, 0, sizeof(frame));
  switch (field->kind) {
  case LONEJSON_FIELD_KIND_STRING:
    if (field->storage == LONEJSON_STORAGE_DYNAMIC) {
      const char *text = *(const char *const *)ptr;
      if (text == NULL) {
        return lonejson__generator_set_pending_bytes(state, "null", 4u);
      }
      return lonejson__generator_push_string(state, text, depth, error);
    }
    return lonejson__generator_push_string(state, (const char *)ptr, depth,
                                           error);
  case LONEJSON_FIELD_KIND_STRING_STREAM:
    frame.kind = LONEJSON__GEN_FRAME_SPOOLED_TEXT;
    frame.depth = depth;
    frame.u.spooled.cursor = *(const lonejson_spooled *)ptr;
    return lonejson__generator_push_frame(state, &frame, error);
  case LONEJSON_FIELD_KIND_BASE64_STREAM:
    frame.kind = LONEJSON__GEN_FRAME_SPOOLED_BASE64;
    frame.depth = depth;
    frame.u.spooled.cursor = *(const lonejson_spooled *)ptr;
    return lonejson__generator_push_frame(state, &frame, error);
  case LONEJSON_FIELD_KIND_STRING_SOURCE:
    if (((const lonejson_source *)ptr)->kind == LONEJSON_SOURCE_NONE) {
      return lonejson__generator_set_pending_bytes(state, "null", 4u);
    }
    frame.kind = LONEJSON__GEN_FRAME_SOURCE_TEXT;
    frame.depth = depth;
    frame.u.source.value = (const lonejson_source *)ptr;
    return lonejson__generator_push_frame(state, &frame, error);
  case LONEJSON_FIELD_KIND_BASE64_SOURCE:
    if (((const lonejson_source *)ptr)->kind == LONEJSON_SOURCE_NONE) {
      return lonejson__generator_set_pending_bytes(state, "null", 4u);
    }
    frame.kind = LONEJSON__GEN_FRAME_SOURCE_BASE64;
    frame.depth = depth;
    frame.u.source.value = (const lonejson_source *)ptr;
    return lonejson__generator_push_frame(state, &frame, error);
  case LONEJSON_FIELD_KIND_JSON_VALUE:
    frame.kind = LONEJSON__GEN_FRAME_JSON_VALUE;
    frame.depth = depth;
    frame.u.json_value.value = (const lonejson_json_value *)ptr;
    return lonejson__generator_push_frame(state, &frame, error);
  case LONEJSON_FIELD_KIND_I64:
    return lonejson__generator_format_i64(state, *(const lonejson_int64 *)ptr);
  case LONEJSON_FIELD_KIND_U64:
    return lonejson__generator_format_u64(state, *(const lonejson_uint64 *)ptr);
  case LONEJSON_FIELD_KIND_F64:
    return lonejson__generator_format_f64(state, *(const double *)ptr, error);
  case LONEJSON_FIELD_KIND_BOOL:
    return lonejson__generator_set_pending_bytes(
        state, *(const bool *)ptr ? "true" : "false",
        *(const bool *)ptr ? 4u : 5u);
  case LONEJSON_FIELD_KIND_OBJECT:
    frame.kind = LONEJSON__GEN_FRAME_MAP;
    frame.depth = depth;
    frame.u.map.map = field->submap;
    frame.u.map.src = ptr;
    return lonejson__generator_push_frame(state, &frame, error);
  case LONEJSON_FIELD_KIND_STRING_ARRAY:
    frame.kind = LONEJSON__GEN_FRAME_STRING_ARRAY;
    frame.depth = depth;
    frame.u.string_array.arr = (const lonejson_string_array *)ptr;
    return lonejson__generator_push_frame(state, &frame, error);
  case LONEJSON_FIELD_KIND_I64_ARRAY:
    frame.kind = LONEJSON__GEN_FRAME_I64_ARRAY;
    frame.depth = depth;
    frame.u.i64_array.arr = (const lonejson_i64_array *)ptr;
    return lonejson__generator_push_frame(state, &frame, error);
  case LONEJSON_FIELD_KIND_U64_ARRAY:
    frame.kind = LONEJSON__GEN_FRAME_U64_ARRAY;
    frame.depth = depth;
    frame.u.u64_array.arr = (const lonejson_u64_array *)ptr;
    return lonejson__generator_push_frame(state, &frame, error);
  case LONEJSON_FIELD_KIND_F64_ARRAY:
    frame.kind = LONEJSON__GEN_FRAME_F64_ARRAY;
    frame.depth = depth;
    frame.u.f64_array.arr = (const lonejson_f64_array *)ptr;
    return lonejson__generator_push_frame(state, &frame, error);
  case LONEJSON_FIELD_KIND_BOOL_ARRAY:
    frame.kind = LONEJSON__GEN_FRAME_BOOL_ARRAY;
    frame.depth = depth;
    frame.u.bool_array.arr = (const lonejson_bool_array *)ptr;
    return lonejson__generator_push_frame(state, &frame, error);
  case LONEJSON_FIELD_KIND_OBJECT_ARRAY:
    frame.kind = LONEJSON__GEN_FRAME_OBJECT_ARRAY;
    frame.depth = depth;
    frame.u.object_array.arr = (const lonejson_object_array *)ptr;
    frame.u.object_array.field = field;
    return lonejson__generator_push_frame(state, &frame, error);
  default:
    return lonejson__set_error(error, LONEJSON_STATUS_INTERNAL_ERROR, 0u, 0u,
                               0u, "unsupported field kind");
  }
}

static lonejson_status
lonejson__generator_emit_escape(lonejson__generator_state *state,
                                unsigned char ch) {
  static const char hex[] = "0123456789abcdef";
  char escape_buf[6];

  switch (ch) {
  case '"':
    return lonejson__generator_set_pending_bytes(state, "\\\"", 2u);
  case '\\':
    return lonejson__generator_set_pending_bytes(state, "\\\\", 2u);
  case '\b':
    return lonejson__generator_set_pending_bytes(state, "\\b", 2u);
  case '\f':
    return lonejson__generator_set_pending_bytes(state, "\\f", 2u);
  case '\n':
    return lonejson__generator_set_pending_bytes(state, "\\n", 2u);
  case '\r':
    return lonejson__generator_set_pending_bytes(state, "\\r", 2u);
  case '\t':
    return lonejson__generator_set_pending_bytes(state, "\\t", 2u);
  default:
    escape_buf[0] = '\\';
    escape_buf[1] = 'u';
    escape_buf[2] = '0';
    escape_buf[3] = '0';
    escape_buf[4] = hex[(ch >> 4u) & 0x0Fu];
    escape_buf[5] = hex[ch & 0x0Fu];
    return lonejson__generator_set_pending_scratch(state, escape_buf,
                                                   sizeof(escape_buf));
  }
}

static lonejson_status
lonejson__generator_step_string(lonejson__generator_state *state,
                                lonejson__generator_frame *frame) {
  const unsigned char *text;
  size_t start;

  if (frame->phase == 0u) {
    frame->phase = 1u;
    return lonejson__generator_set_pending_bytes(state, "\"", 1u);
  }
  if (frame->phase == 2u) {
    lonejson__generator_pop_frame(state);
    return lonejson__generator_set_pending_bytes(state, "\"", 1u);
  }
  text = frame->u.string.text;
  start = frame->u.string.offset;
  while (text[frame->u.string.offset] != '\0') {
    unsigned char ch = text[frame->u.string.offset];
    if (ch < 0x20u || ch == '"' || ch == '\\') {
      break;
    }
    frame->u.string.offset++;
  }
  if (frame->u.string.offset != start) {
    return lonejson__generator_set_pending_bytes(
        state, text + start, frame->u.string.offset - start);
  }
  if (text[frame->u.string.offset] == '\0') {
    frame->phase = 2u;
    return LONEJSON_STATUS_OK;
  }
  frame->u.string.offset++;
  return lonejson__generator_emit_escape(state, text[start]);
}

static lonejson_status
lonejson__generator_stream_refill_source(lonejson__generator_state *state,
                                         lonejson__generator_frame *frame,
                                         lonejson_error *error) {
  lonejson_read_result chunk;
  lonejson_status status;

  if (!frame->u.source.opened) {
    status = lonejson__source_cursor_open(frame->u.source.value,
                                          &frame->u.source.cursor, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    frame->u.source.opened = 1;
  }
  chunk = lonejson__source_cursor_read(
      &frame->u.source.cursor, state->io_buffer, sizeof(state->io_buffer));
  if (chunk.error_code != 0) {
    if (error != NULL) {
      error->system_errno = chunk.error_code;
    }
    return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                               "failed to read source data");
  }
  state->io_buffer_len = chunk.bytes_read;
  state->io_buffer_off = 0u;
  state->io_eof = chunk.eof;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__generator_stream_refill_spooled(lonejson__generator_state *state,
                                          lonejson__generator_frame *frame,
                                          lonejson_error *error) {
  lonejson_read_result chunk;
  lonejson_status status;

  if (!frame->u.spooled.rewound) {
    status = lonejson_spooled_rewind(&frame->u.spooled.cursor, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    frame->u.spooled.rewound = 1;
  }
  chunk = lonejson_spooled_read(&frame->u.spooled.cursor, state->io_buffer,
                                sizeof(state->io_buffer));
  if (chunk.error_code != 0) {
    if (error != NULL) {
      error->system_errno = chunk.error_code;
    }
    return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                               "failed to read spooled data");
  }
  state->io_buffer_len = chunk.bytes_read;
  state->io_buffer_off = 0u;
  state->io_eof = chunk.eof;
  return LONEJSON_STATUS_OK;
}

static void
lonejson__generator_prepare_stream_owner(lonejson__generator_state *state,
                                         size_t owner_index) {
  if (!state->io_owner_valid || state->io_owner_index != owner_index) {
    state->io_owner_valid = 1;
    state->io_owner_index = owner_index;
    state->io_buffer_len = 0u;
    state->io_buffer_off = 0u;
    state->io_eof = 0;
  }
}

static lonejson_status lonejson__generator_step_stream_text(
    lonejson__generator_state *state, lonejson__generator_frame *frame,
    size_t owner_index, int is_source, lonejson_error *error) {
  size_t start;
  lonejson_status status;

  lonejson__generator_prepare_stream_owner(state, owner_index);
  if (frame->phase == 0u) {
    frame->phase = 1u;
    return lonejson__generator_set_pending_bytes(state, "\"", 1u);
  }
  if (frame->phase == 2u) {
    if (is_source && frame->u.source.opened) {
      lonejson__source_cursor_close(&frame->u.source.cursor);
      frame->u.source.opened = 0;
    }
    lonejson__generator_pop_frame(state);
    return lonejson__generator_set_pending_bytes(state, "\"", 1u);
  }
  if (state->io_buffer_off == state->io_buffer_len) {
    if (state->io_eof) {
      frame->phase = 2u;
      return LONEJSON_STATUS_OK;
    }
    status =
        is_source
            ? lonejson__generator_stream_refill_source(state, frame, error)
            : lonejson__generator_stream_refill_spooled(state, frame, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    if (state->io_buffer_off == state->io_buffer_len && state->io_eof) {
      frame->phase = 2u;
      return LONEJSON_STATUS_OK;
    }
  }
  start = state->io_buffer_off;
  while (state->io_buffer_off < state->io_buffer_len) {
    unsigned char ch = state->io_buffer[state->io_buffer_off];
    if (ch < 0x20u || ch == '"' || ch == '\\') {
      break;
    }
    state->io_buffer_off++;
  }
  if (state->io_buffer_off != start) {
    return lonejson__generator_set_pending_bytes(
        state, state->io_buffer + start, state->io_buffer_off - start);
  }
  if (state->io_buffer_off < state->io_buffer_len) {
    unsigned char ch = state->io_buffer[state->io_buffer_off++];
    return lonejson__generator_emit_escape(state, ch);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__generator_emit_base64_quad(lonejson__generator_state *state,
                                     const unsigned char *data, size_t len) {
  static const char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  char out[4];

  out[0] = alphabet[(data[0] >> 2u) & 0x3Fu];
  out[1] =
      alphabet[((data[0] & 0x03u) << 4u) | (((len > 1u) ? data[1] : 0u) >> 4u)];
  out[2] = (len > 1u) ? alphabet[((data[1] & 0x0Fu) << 2u) |
                                 (((len > 2u) ? data[2] : 0u) >> 6u)]
                      : '=';
  out[3] = (len > 2u) ? alphabet[data[2] & 0x3Fu] : '=';
  return lonejson__generator_set_pending_scratch(state, out, sizeof(out));
}

static lonejson_status lonejson__generator_step_stream_base64(
    lonejson__generator_state *state, lonejson__generator_frame *frame,
    size_t owner_index, int is_source, lonejson_error *error) {
  unsigned char *carry;
  size_t *carry_len;
  lonejson_status status;

  lonejson__generator_prepare_stream_owner(state, owner_index);
  carry = is_source ? frame->u.source.carry : frame->u.spooled.carry;
  carry_len =
      is_source ? &frame->u.source.carry_len : &frame->u.spooled.carry_len;
  if (frame->phase == 0u) {
    frame->phase = 1u;
    return lonejson__generator_set_pending_bytes(state, "\"", 1u);
  }
  if (frame->phase == 2u) {
    if (is_source && frame->u.source.opened) {
      lonejson__source_cursor_close(&frame->u.source.cursor);
      frame->u.source.opened = 0;
    }
    lonejson__generator_pop_frame(state);
    return lonejson__generator_set_pending_bytes(state, "\"", 1u);
  }
  while (*carry_len < 3u) {
    if (state->io_buffer_off == state->io_buffer_len) {
      if (state->io_eof) {
        break;
      }
      status =
          is_source
              ? lonejson__generator_stream_refill_source(state, frame, error)
              : lonejson__generator_stream_refill_spooled(state, frame, error);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      if (state->io_buffer_off == state->io_buffer_len && state->io_eof) {
        break;
      }
    }
    carry[*carry_len] = state->io_buffer[state->io_buffer_off++];
    (*carry_len)++;
  }
  if (*carry_len == 3u) {
    *carry_len = 0u;
    return lonejson__generator_emit_base64_quad(state, carry, 3u);
  }
  if (state->io_eof) {
    if (*carry_len != 0u) {
      size_t final_len;
      final_len = *carry_len;
      *carry_len = 0u;
      return lonejson__generator_emit_base64_quad(state, carry, final_len);
    }
    frame->phase = 2u;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__generator_step_map(lonejson__generator_state *state,
                             lonejson__generator_frame *frame,
                             lonejson_error *error) {
  const lonejson_map *map;
  const lonejson_field *field;

  map = frame->u.map.map;
  switch (frame->phase) {
  case 0u:
    frame->phase = 1u;
    return lonejson__generator_set_pending_bytes(state, "{", 1u);
  case 1u:
    while (frame->index < map->field_count &&
           lonejson__field_should_omit(frame->u.map.src,
                                       &map->fields[frame->index])) {
      frame->index++;
    }
    if (frame->index >= map->field_count) {
      if (state->options.pretty && frame->emitted != 0u) {
        frame->phase = 9u;
      } else {
        frame->phase = 11u;
      }
      return LONEJSON_STATUS_OK;
    }
    if (frame->emitted != 0u) {
      frame->phase = 2u;
      return LONEJSON_STATUS_OK;
    }
    frame->phase = state->options.pretty ? 3u : 5u;
    return LONEJSON_STATUS_OK;
  case 2u:
    frame->phase = state->options.pretty ? 3u : 5u;
    return lonejson__generator_set_pending_bytes(state, ",", 1u);
  case 3u:
    frame->aux = (frame->depth + 1u) * 2u;
    frame->phase = 4u;
    return lonejson__generator_set_pending_bytes(state, "\n", 1u);
  case 4u:
    if (frame->aux == 0u) {
      frame->phase = 5u;
      return LONEJSON_STATUS_OK;
    }
    if (frame->aux > 64u) {
      frame->aux -= 64u;
      return lonejson__generator_set_pending_repeat(state, ' ', 64u);
    }
    frame->phase = 5u;
    return lonejson__generator_set_pending_repeat(state, ' ', frame->aux);
  case 5u:
    field = &map->fields[frame->index];
    frame->phase = 6u;
    return lonejson__generator_push_string(state, field->json_key,
                                           frame->depth + 1u, error);
  case 6u:
    frame->phase = 7u;
    return lonejson__generator_set_pending_bytes(
        state, state->options.pretty ? ": " : ":",
        state->options.pretty ? 2u : 1u);
  case 7u:
    field = &map->fields[frame->index];
    frame->phase = 8u;
    return lonejson__generator_push_value_from_field(
        state, field, lonejson__field_cptr(frame->u.map.src, field),
        frame->depth + 1u, error);
  case 8u:
    frame->index++;
    frame->emitted++;
    frame->phase = 1u;
    return LONEJSON_STATUS_OK;
  case 9u:
    frame->aux = frame->depth * 2u;
    frame->phase = 10u;
    return lonejson__generator_set_pending_bytes(state, "\n", 1u);
  case 10u:
    if (frame->aux == 0u) {
      frame->phase = 11u;
      return LONEJSON_STATUS_OK;
    }
    if (frame->aux > 64u) {
      frame->aux -= 64u;
      return lonejson__generator_set_pending_repeat(state, ' ', 64u);
    }
    frame->phase = 11u;
    return lonejson__generator_set_pending_repeat(state, ' ', frame->aux);
  case 11u:
    lonejson__generator_pop_frame(state);
    return lonejson__generator_set_pending_bytes(state, "}", 1u);
  default:
    return lonejson__set_error(error, LONEJSON_STATUS_INTERNAL_ERROR, 0u, 0u,
                               0u, "invalid generator map phase");
  }
}

#define LONEJSON__GEN_ARRAY_STEP_BEGIN(arr_count)                              \
  switch (frame->phase) {                                                      \
  case 0u:                                                                     \
    frame->phase = 1u;                                                         \
    return lonejson__generator_set_pending_bytes(state, "[", 1u);              \
  case 1u:                                                                     \
    if (frame->index >= (arr_count)) {                                         \
      if (state->options.pretty && (arr_count) != 0u) {                        \
        frame->phase = 7u;                                                     \
      } else {                                                                 \
        frame->phase = 9u;                                                     \
      }                                                                        \
      return LONEJSON_STATUS_OK;                                               \
    }                                                                          \
    if (frame->index != 0u) {                                                  \
      frame->phase = 2u;                                                       \
      return LONEJSON_STATUS_OK;                                               \
    }                                                                          \
    frame->phase = state->options.pretty ? 3u : 5u;                            \
    return LONEJSON_STATUS_OK;                                                 \
  case 2u:                                                                     \
    frame->phase = state->options.pretty ? 3u : 5u;                            \
    return lonejson__generator_set_pending_bytes(state, ",", 1u);              \
  case 3u:                                                                     \
    frame->aux = (frame->depth + 1u) * 2u;                                     \
    frame->phase = 4u;                                                         \
    return lonejson__generator_set_pending_bytes(state, "\n", 1u);             \
  case 4u:                                                                     \
    if (frame->aux == 0u) {                                                    \
      frame->phase = 5u;                                                       \
      return LONEJSON_STATUS_OK;                                               \
    }                                                                          \
    if (frame->aux > 64u) {                                                    \
      frame->aux -= 64u;                                                       \
      return lonejson__generator_set_pending_repeat(state, ' ', 64u);          \
    }                                                                          \
    frame->phase = 5u;                                                         \
    return lonejson__generator_set_pending_repeat(state, ' ', frame->aux);     \
  case 6u:                                                                     \
    frame->index++;                                                            \
    frame->phase = 1u;                                                         \
    return LONEJSON_STATUS_OK;                                                 \
  case 7u:                                                                     \
    frame->aux = frame->depth * 2u;                                            \
    frame->phase = 8u;                                                         \
    return lonejson__generator_set_pending_bytes(state, "\n", 1u);             \
  case 8u:                                                                     \
    if (frame->aux == 0u) {                                                    \
      frame->phase = 9u;                                                       \
      return LONEJSON_STATUS_OK;                                               \
    }                                                                          \
    if (frame->aux > 64u) {                                                    \
      frame->aux -= 64u;                                                       \
      return lonejson__generator_set_pending_repeat(state, ' ', 64u);          \
    }                                                                          \
    frame->phase = 9u;                                                         \
    return lonejson__generator_set_pending_repeat(state, ' ', frame->aux);     \
  case 9u:                                                                     \
    lonejson__generator_pop_frame(state);                                      \
    return lonejson__generator_set_pending_bytes(state, "]", 1u);              \
  default:                                                                     \
    return lonejson__set_error(error, LONEJSON_STATUS_INTERNAL_ERROR, 0u, 0u,  \
                               0u, "invalid generator array phase");           \
  }

static lonejson_status
lonejson__generator_step_string_array(lonejson__generator_state *state,
                                      lonejson__generator_frame *frame,
                                      lonejson_error *error) {
  const lonejson_string_array *arr;
  arr = frame->u.string_array.arr;
  switch (frame->phase) {
  case 5u:
    frame->phase = 6u;
    return lonejson__generator_push_string(
        state, arr->items[frame->index] ? arr->items[frame->index] : "",
        frame->depth + 1u, error);
  default:
    LONEJSON__GEN_ARRAY_STEP_BEGIN(arr->count);
  }
}

static lonejson_status
lonejson__generator_step_i64_array(lonejson__generator_state *state,
                                   lonejson__generator_frame *frame,
                                   lonejson_error *error) {
  const lonejson_i64_array *arr;
  (void)error;
  arr = frame->u.i64_array.arr;
  switch (frame->phase) {
  case 5u:
    frame->phase = 6u;
    return lonejson__generator_format_i64(state, arr->items[frame->index]);
  default:
    LONEJSON__GEN_ARRAY_STEP_BEGIN(arr->count);
  }
}

static lonejson_status
lonejson__generator_step_u64_array(lonejson__generator_state *state,
                                   lonejson__generator_frame *frame,
                                   lonejson_error *error) {
  const lonejson_u64_array *arr;
  (void)error;
  arr = frame->u.u64_array.arr;
  switch (frame->phase) {
  case 5u:
    frame->phase = 6u;
    return lonejson__generator_format_u64(state, arr->items[frame->index]);
  default:
    LONEJSON__GEN_ARRAY_STEP_BEGIN(arr->count);
  }
}

static lonejson_status
lonejson__generator_step_f64_array(lonejson__generator_state *state,
                                   lonejson__generator_frame *frame,
                                   lonejson_error *error) {
  const lonejson_f64_array *arr;
  arr = frame->u.f64_array.arr;
  switch (frame->phase) {
  case 5u:
    frame->phase = 6u;
    return lonejson__generator_format_f64(state, arr->items[frame->index],
                                          error);
  default:
    LONEJSON__GEN_ARRAY_STEP_BEGIN(arr->count);
  }
}

static lonejson_status
lonejson__generator_step_bool_array(lonejson__generator_state *state,
                                    lonejson__generator_frame *frame,
                                    lonejson_error *error) {
  const lonejson_bool_array *arr;
  (void)error;
  arr = frame->u.bool_array.arr;
  switch (frame->phase) {
  case 5u:
    frame->phase = 6u;
    return lonejson__generator_set_pending_bytes(
        state, arr->items[frame->index] ? "true" : "false",
        arr->items[frame->index] ? 4u : 5u);
  default:
    LONEJSON__GEN_ARRAY_STEP_BEGIN(arr->count);
  }
}

#undef LONEJSON__GEN_ARRAY_STEP_BEGIN

static lonejson_status
lonejson__generator_step_object_array(lonejson__generator_state *state,
                                      lonejson__generator_frame *frame,
                                      lonejson_error *error) {
  const lonejson_object_array *arr;
  arr = frame->u.object_array.arr;
  switch (frame->phase) {
  case 0u:
    frame->phase = 1u;
    return lonejson__generator_set_pending_bytes(state, "[", 1u);
  case 1u:
    if (frame->index >= arr->count) {
      if (state->options.pretty && arr->count != 0u) {
        frame->phase = 7u;
      } else {
        frame->phase = 9u;
      }
      return LONEJSON_STATUS_OK;
    }
    if (frame->index != 0u) {
      frame->phase = 2u;
      return LONEJSON_STATUS_OK;
    }
    frame->phase = state->options.pretty ? 3u : 5u;
    return LONEJSON_STATUS_OK;
  case 2u:
    frame->phase = state->options.pretty ? 3u : 5u;
    return lonejson__generator_set_pending_bytes(state, ",", 1u);
  case 3u:
    frame->aux = (frame->depth + 1u) * 2u;
    frame->phase = 4u;
    return lonejson__generator_set_pending_bytes(state, "\n", 1u);
  case 4u:
    if (frame->aux == 0u) {
      frame->phase = 5u;
      return LONEJSON_STATUS_OK;
    }
    if (frame->aux > 64u) {
      frame->aux -= 64u;
      return lonejson__generator_set_pending_repeat(state, ' ', 64u);
    }
    frame->phase = 5u;
    return lonejson__generator_set_pending_repeat(state, ' ', frame->aux);
  case 5u: {
    const void *elem;
    lonejson__generator_frame child;
    elem = (const unsigned char *)arr->items +
           (frame->index * frame->u.object_array.field->elem_size);
    memset(&child, 0, sizeof(child));
    child.kind = LONEJSON__GEN_FRAME_MAP;
    child.depth = frame->depth + 1u;
    child.u.map.map = frame->u.object_array.field->submap;
    child.u.map.src = elem;
    frame->phase = 6u;
    return lonejson__generator_push_frame(state, &child, error);
  }
  case 6u:
    frame->index++;
    frame->phase = 1u;
    return LONEJSON_STATUS_OK;
  case 7u:
    frame->aux = frame->depth * 2u;
    frame->phase = 8u;
    return lonejson__generator_set_pending_bytes(state, "\n", 1u);
  case 8u:
    if (frame->aux == 0u) {
      frame->phase = 9u;
      return LONEJSON_STATUS_OK;
    }
    if (frame->aux > 64u) {
      frame->aux -= 64u;
      return lonejson__generator_set_pending_repeat(state, ' ', 64u);
    }
    frame->phase = 9u;
    return lonejson__generator_set_pending_repeat(state, ' ', frame->aux);
  case 9u:
    lonejson__generator_pop_frame(state);
    return lonejson__generator_set_pending_bytes(state, "]", 1u);
  default:
    return lonejson__set_error(error, LONEJSON_STATUS_INTERNAL_ERROR, 0u, 0u,
                               0u, "invalid generator object-array phase");
  }
}

static LONEJSON__NOINLINE LONEJSON__COLD lonejson_status
lonejson__generator_prepare_json_owner(lonejson__generator_state *state,
                                       size_t owner_index,
                                       const lonejson_json_value *value,
                                       lonejson_error *error) {
  if (state->json_owner_valid && state->json_owner_index == owner_index &&
      state->json_owner_value == value) {
    return LONEJSON_STATUS_OK;
  }
  if (state->json_owner_valid) {
    lonejson__json_pull_cleanup(&state->json_pull);
    state->json_owner_valid = 0;
    state->json_owner_index = 0u;
    state->json_owner_value = NULL;
  }
  if (lonejson__json_pull_init(
          &state->json_pull, value, state->options.pretty,
          owner_index == 0u ? 0u : state->frames[owner_index].depth,
          error) != LONEJSON_STATUS_OK) {
    return error->code;
  }
  state->json_owner_valid = 1;
  state->json_owner_index = owner_index;
  state->json_owner_value = value;
  return LONEJSON_STATUS_OK;
}

static LONEJSON__NOINLINE LONEJSON__COLD lonejson_status
lonejson__generator_step_json_value(lonejson__generator_state *state,
                                    lonejson__generator_frame *frame,
                                    size_t owner_index, lonejson_error *error) {
  size_t out_len;
  int eof;
  lonejson_status status;

  status = lonejson__generator_prepare_json_owner(
      state, owner_index, frame->u.json_value.value, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status =
      lonejson__json_pull_read(&state->json_pull, state->io_buffer,
                               sizeof(state->io_buffer), &out_len, &eof, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (out_len != 0u) {
    return lonejson__generator_set_pending_bytes(state, state->io_buffer,
                                                 out_len);
  }
  if (eof) {
    lonejson__json_pull_cleanup(&state->json_pull);
    state->json_owner_valid = 0;
    state->json_owner_index = 0u;
    state->json_owner_value = NULL;
    lonejson__generator_pop_frame(state);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__generator_step_frame(lonejson__generator_state *state,
                               lonejson_error *error) {
  lonejson__generator_frame *frame;
  size_t owner_index;

  if (state->frame_count == 0u) {
    state->finished = 1;
    return LONEJSON_STATUS_OK;
  }
  owner_index = state->frame_count - 1u;
  frame = &state->frames[owner_index];
  switch (frame->kind) {
  case LONEJSON__GEN_FRAME_MAP:
    return lonejson__generator_step_map(state, frame, error);
  case LONEJSON__GEN_FRAME_STRING:
    return lonejson__generator_step_string(state, frame);
  case LONEJSON__GEN_FRAME_STRING_ARRAY:
    return lonejson__generator_step_string_array(state, frame, error);
  case LONEJSON__GEN_FRAME_I64_ARRAY:
    return lonejson__generator_step_i64_array(state, frame, error);
  case LONEJSON__GEN_FRAME_U64_ARRAY:
    return lonejson__generator_step_u64_array(state, frame, error);
  case LONEJSON__GEN_FRAME_F64_ARRAY:
    return lonejson__generator_step_f64_array(state, frame, error);
  case LONEJSON__GEN_FRAME_BOOL_ARRAY:
    return lonejson__generator_step_bool_array(state, frame, error);
  case LONEJSON__GEN_FRAME_OBJECT_ARRAY:
    return lonejson__generator_step_object_array(state, frame, error);
  case LONEJSON__GEN_FRAME_SOURCE_TEXT:
    return lonejson__generator_step_stream_text(state, frame, owner_index, 1,
                                                error);
  case LONEJSON__GEN_FRAME_SOURCE_BASE64:
    return lonejson__generator_step_stream_base64(state, frame, owner_index, 1,
                                                  error);
  case LONEJSON__GEN_FRAME_SPOOLED_TEXT:
    return lonejson__generator_step_stream_text(state, frame, owner_index, 0,
                                                error);
  case LONEJSON__GEN_FRAME_SPOOLED_BASE64:
    return lonejson__generator_step_stream_base64(state, frame, owner_index, 0,
                                                  error);
  case LONEJSON__GEN_FRAME_JSON_VALUE:
    return lonejson__generator_step_json_value(state, frame, owner_index,
                                               error);
  default:
    return lonejson__set_error(error, LONEJSON_STATUS_INTERNAL_ERROR, 0u, 0u,
                               0u, "unsupported generator frame kind");
  }
}

lonejson_status lonejson_generator_init(lonejson_generator *generator,
                                        const lonejson_map *map,
                                        const void *src,
                                        const lonejson_write_options *options) {
  lonejson__generator_state *state;
  lonejson_allocator allocator;
  lonejson__generator_frame root;

  if (generator == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  memset(generator, 0, sizeof(*generator));
  if (map == NULL || src == NULL) {
    return lonejson__set_error(
        &generator->error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "map and source are required for generator init");
  }
  if (!lonejson__map_layout_is_valid(map)) {
    return lonejson__set_error(&generator->error,
                               LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                               "invalid map layout");
  }
  if (options != NULL &&
      !LONEJSON__ALLOCATOR_IS_VALID_CONFIG(options->allocator)) {
    return lonejson__set_error(
        &generator->error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "allocator must provide either all callbacks or none");
  }
  allocator = lonejson__allocator_resolve(options ? options->allocator : NULL);
  state = (lonejson__generator_state *)lonejson__buffer_alloc(&allocator,
                                                              sizeof(*state));
  if (state == NULL) {
    return lonejson__set_error(&generator->error,
                               LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
                               "failed to allocate generator state");
  }
  memset(state, 0, sizeof(*state));
  state->allocator = allocator;
  state->options = options ? *options : lonejson_default_write_options();
  memset(&root, 0, sizeof(root));
  root.kind = LONEJSON__GEN_FRAME_MAP;
  root.u.map.map = map;
  root.u.map.src = src;
  if (lonejson__generator_push_frame(state, &root, &generator->error) !=
      LONEJSON_STATUS_OK) {
    lonejson__buffer_free(&allocator, state, sizeof(*state));
    return generator->error.code;
  }
  generator->state = state;
  generator->eof = 0;
  lonejson__clear_error(&generator->error);
  return LONEJSON_STATUS_OK;
}

typedef struct lonejson__measure_sink_state {
  size_t total;
} lonejson__measure_sink_state;

static lonejson_status lonejson__measure_sink(void *user, const void *data,
                                              size_t len,
                                              lonejson_error *error) {
  lonejson__measure_sink_state *state = (lonejson__measure_sink_state *)user;

  (void)data;
  if (SIZE_MAX - state->total < len) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "serialized output is too large to measure");
  }
  state->total += len;
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_generator_measure(
    const lonejson_map *map, const void *src, size_t *out_len,
    const lonejson_write_options *options, lonejson_error *error) {
  lonejson__measure_sink_state state;
  lonejson_status status;

  if (map == NULL || src == NULL || out_len == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "map, source, and output length are required");
  }
  if (!lonejson__map_layout_is_valid(map)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "invalid map layout");
  }
  if (!lonejson__map_is_rewindable_for_serialize(map, src)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "cannot measure non-rewindable JSON document");
  }
  state.total = 0u;
  status = lonejson_serialize_sink(map, src, lonejson__measure_sink, &state,
                                   options, error);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    *out_len = state.total;
  }
  return status;
}

lonejson_status lonejson_generator_read(lonejson_generator *generator,
                                        unsigned char *buffer, size_t capacity,
                                        size_t *out_len, int *out_eof) {
  lonejson__generator_state *state;
  lonejson_status status;

  if (out_len != NULL) {
    *out_len = 0u;
  }
  if (out_eof != NULL) {
    *out_eof = 0;
  }
  if (generator == NULL || generator->state == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (capacity != 0u && buffer == NULL) {
    return lonejson__set_error(&generator->error,
                               LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                               "generator read buffer is required");
  }
  state = (lonejson__generator_state *)generator->state;
  state->out = buffer;
  state->out_capacity = capacity;
  state->out_length = 0u;
  while (state->out_length < state->out_capacity) {
    status = lonejson__generator_flush_pending(state);
    if (status != LONEJSON_STATUS_OK) {
      generator->error.code = status;
      return status;
    }
    if (state->pending_kind != LONEJSON__GEN_PENDING_NONE ||
        state->out_length == state->out_capacity) {
      break;
    }
    if (state->finished || state->frame_count == 0u) {
      state->finished = 1;
      break;
    }
    status = lonejson__generator_step_frame(state, &generator->error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  generator->eof = (state->finished || state->frame_count == 0u) &&
                   state->pending_kind == LONEJSON__GEN_PENDING_NONE;
  if (out_len != NULL) {
    *out_len = state->out_length;
  }
  if (out_eof != NULL) {
    *out_eof = generator->eof;
  }
  return LONEJSON_STATUS_OK;
}

void lonejson_generator_cleanup(lonejson_generator *generator) {
  lonejson__generator_state *state;
  size_t i;

  if (generator == NULL) {
    return;
  }
  state = (lonejson__generator_state *)generator->state;
  if (state != NULL) {
    if (state->json_owner_valid) {
      lonejson__json_pull_cleanup(&state->json_pull);
    }
    for (i = 0u; i < state->frame_count; ++i) {
      if ((state->frames[i].kind == LONEJSON__GEN_FRAME_SOURCE_TEXT ||
           state->frames[i].kind == LONEJSON__GEN_FRAME_SOURCE_BASE64) &&
          state->frames[i].u.source.opened) {
        lonejson__source_cursor_close(&state->frames[i].u.source.cursor);
      }
    }
    if (state->frames != NULL) {
      lonejson__buffer_free(&state->allocator, state->frames,
                            state->frame_capacity * sizeof(*state->frames));
    }
    lonejson__buffer_free(&state->allocator, state, sizeof(*state));
  }
  memset(generator, 0, sizeof(*generator));
}

