static void
lonejson__json_pull_clear_pending(lonejson__json_pull_state *state) {
  state->pending_kind = LONEJSON__JSON_PULL_PENDING_NONE;
  state->pending_ptr = NULL;
  state->pending_len = 0u;
  state->pending_off = 0u;
  state->pending_repeat_count = 0u;
}

static lonejson_status
lonejson__json_pull_set_pending_bytes(lonejson__json_pull_state *state,
                                      const void *data, size_t len) {
  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  state->pending_kind = LONEJSON__JSON_PULL_PENDING_BYTES;
  state->pending_ptr = (const unsigned char *)data;
  state->pending_len = len;
  state->pending_off = 0u;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__json_pull_set_pending_repeat(lonejson__json_pull_state *state,
                                       unsigned char byte, size_t count) {
  if (count == 0u) {
    return LONEJSON_STATUS_OK;
  }
  state->pending_kind = LONEJSON__JSON_PULL_PENDING_REPEAT;
  state->pending_repeat_byte = byte;
  state->pending_repeat_count = count;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__json_pull_set_pending_char(lonejson__json_pull_state *state,
                                     unsigned char ch) {
  state->scratch[0] = ch;
  return lonejson__json_pull_set_pending_bytes(state, state->scratch, 1u);
}

static lonejson_status
lonejson__json_pull_flush_pending(lonejson__json_pull_state *state) {
  size_t writable;
  size_t take;

  if (state->pending_kind == LONEJSON__JSON_PULL_PENDING_NONE ||
      state->out_length >= state->out_capacity) {
    return LONEJSON_STATUS_OK;
  }
  writable = state->out_capacity - state->out_length;
  if (state->pending_kind == LONEJSON__JSON_PULL_PENDING_BYTES) {
    take = state->pending_len - state->pending_off;
    if (take > writable) {
      take = writable;
    }
    memcpy(state->out + state->out_length,
           state->pending_ptr + state->pending_off, take);
    state->out_length += take;
    state->pending_off += take;
    if (state->pending_off == state->pending_len) {
      lonejson__json_pull_clear_pending(state);
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
    lonejson__json_pull_clear_pending(state);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__json_pull_push_frame(lonejson__json_pull_state *state,
                               lonejson__json_pull_frame_kind kind,
                               lonejson__json_pull_frame_phase phase,
                               size_t depth, lonejson_error *error) {
  lonejson__json_pull_frame *next;
  size_t next_cap;

  if (state->frame_count == state->frame_capacity) {
    next_cap =
        (state->frame_capacity == 0u) ? 8u : (state->frame_capacity * 2u);
    next = (lonejson__json_pull_frame *)lonejson__buffer_realloc(
        &state->allocator, state->frames,
        state->frame_capacity * sizeof(*state->frames),
        next_cap * sizeof(*state->frames));
    if (next == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u,
                                 "failed to grow JSON pull frame stack");
    }
    state->frames = next;
    state->frame_capacity = next_cap;
  }
  memset(&state->frames[state->frame_count], 0, sizeof(*state->frames));
  state->frames[state->frame_count].kind = kind;
  state->frames[state->frame_count].phase = phase;
  state->frames[state->frame_count].depth = depth;
  state->frame_count++;
  return LONEJSON_STATUS_OK;
}

static void lonejson__json_pull_pop_frame(lonejson__json_pull_state *state) {
  if (state->frame_count != 0u) {
    state->frame_count--;
  }
}

static LONEJSON__INLINE int
lonejson__json_pull_getc(lonejson__json_pull_state *state) {
  unsigned char byte = 0u;
  lonejson_read_result result;
  ssize_t got;

  if (state->has_pushback) {
    state->has_pushback = 0;
    return state->pushback;
  }
  if (state->cursor.buffer != NULL) {
    if (state->cursor.buffer_off >= state->cursor.buffer_len) {
      return EOF;
    }
    byte = state->cursor.buffer[state->cursor.buffer_off++];
    goto counted;
  }
  if (state->cursor.read_buffer_off < state->cursor.read_buffer_len) {
    byte = state->cursor.read_buffer[state->cursor.read_buffer_off++];
    goto counted;
  }
  if (state->cursor.reader != NULL) {
    for (;;) {
      result = state->cursor.reader(state->cursor.reader_user,
                                    state->cursor.read_buffer,
                                    sizeof(state->cursor.read_buffer));
      if (result.error_code != 0) {
        state->error.system_errno = result.error_code;
        lonejson__set_error(&state->error, LONEJSON_STATUS_CALLBACK_FAILED, 0u,
                            0u, 0u, "reader callback failed");
        return -2;
      }
      if (result.bytes_read != 0u) {
        state->cursor.read_buffer_len = result.bytes_read;
        state->cursor.read_buffer_off = 0u;
        byte = state->cursor.read_buffer[state->cursor.read_buffer_off++];
        goto counted;
      }
      if (result.eof) {
        return EOF;
      }
      if (!result.would_block) {
        return EOF;
      }
    }
  }
  if (state->cursor.use_fd) {
    got = read(state->cursor.value->fd, state->cursor.read_buffer,
               sizeof(state->cursor.read_buffer));
    if (got < 0) {
      state->error.system_errno = errno;
      lonejson__set_error(&state->error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                          "failed to read JSON value fd");
      return -2;
    }
    if (got == 0) {
      return EOF;
    }
    state->cursor.read_buffer_len = (size_t)got;
    state->cursor.read_buffer_off = 0u;
    byte = state->cursor.read_buffer[state->cursor.read_buffer_off++];
    goto counted;
  }
  got = (ssize_t)fread(state->cursor.read_buffer, 1u,
                       sizeof(state->cursor.read_buffer), state->cursor.fp);
  if (got <= 0) {
    if (ferror(state->cursor.fp)) {
      state->error.system_errno = errno;
      lonejson__set_error(&state->error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                          "failed to read JSON value stream");
      return -2;
    }
    return EOF;
  }
  state->cursor.read_buffer_len = (size_t)got;
  state->cursor.read_buffer_off = 0u;
  byte = state->cursor.read_buffer[state->cursor.read_buffer_off++];
counted:
  state->total_bytes++;
  if (state->limits.max_total_bytes != 0u &&
      state->total_bytes > state->limits.max_total_bytes) {
    lonejson__set_error(&state->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                        "JSON value exceeds maximum total byte limit");
    return -2;
  }
  return (int)byte;
}

static LONEJSON__INLINE void
lonejson__json_pull_ungetc(lonejson__json_pull_state *state, int ch) {
  state->has_pushback = 1;
  state->pushback = ch;
}

static LONEJSON__INLINE int
lonejson__json_pull_peek_nonspace(lonejson__json_pull_state *state) {
  int ch;

  do {
    ch = lonejson__json_pull_getc(state);
  } while (ch >= 0 && lonejson__is_json_space(ch));
  if (ch >= 0) {
    lonejson__json_pull_ungetc(state, ch);
  }
  return ch;
}

static lonejson_status
lonejson__json_pull_schedule_indent(lonejson__json_pull_state *state,
                                    size_t depth) {
  return lonejson__json_pull_set_pending_repeat(
      state, ' ', (state->base_depth + depth) * 2u);
}

static void
lonejson__json_pull_value_complete(lonejson__json_pull_state *state) {
  lonejson__json_pull_frame *frame;

  if (state->frame_count == 0u) {
    state->root_finished = 1;
    return;
  }
  frame = &state->frames[state->frame_count - 1u];
  switch (frame->phase) {
  case LONEJSON__JSON_PULL_OBJECT_VALUE:
    frame->phase = LONEJSON__JSON_PULL_OBJECT_COMMA_OR_END;
    frame->count++;
    break;
  case LONEJSON__JSON_PULL_ARRAY_VALUE:
    frame->phase = LONEJSON__JSON_PULL_ARRAY_COMMA_OR_END;
    frame->count++;
    break;
  default:
    break;
  }
}

static lonejson_status
lonejson__json_pull_begin_number(lonejson__json_pull_state *state, int first) {
  state->lex_mode = LONEJSON_LEX_NUMBER;
  state->number_len = 0u;
  state->number_buf[state->number_len++] = (char)first;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__json_pull_begin_literal(lonejson__json_pull_state *state, int first,
                                  const char *rest) {
  state->lex_mode = (first == 't')   ? LONEJSON_LEX_TRUE
                    : (first == 'f') ? LONEJSON_LEX_FALSE
                                     : LONEJSON_LEX_NULL;
  state->literal_buf[0] = (char)first;
  state->literal_len = 1u;
  state->literal_pos = 0u;
  state->literal_rest = rest;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__json_pull_begin_string(lonejson__json_pull_state *state, int is_key) {
  state->lex_mode = LONEJSON_LEX_STRING;
  state->lex_is_key = is_key;
  state->lex_escape = 0;
  state->unicode_pending_high = 0u;
  state->unicode_accum = 0u;
  state->unicode_digits_needed = 0;
  state->unicode_expect_low_slash = 0;
  state->unicode_expect_low_u = 0;
  state->unicode_parsing_low = 0;
  return lonejson__json_pull_set_pending_bytes(state, "\"", 1u);
}

static lonejson_status
lonejson__json_pull_start_value(lonejson__json_pull_state *state,
                                size_t depth) {
  int ch;

  state->root_started = 1;
  ch = lonejson__json_pull_peek_nonspace(state);
  if (ch == -2) {
    return state->error.code;
  }
  if (ch == EOF) {
    return lonejson__set_error(&state->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                               0u, 0u, "expected JSON value");
  }
  ch = lonejson__json_pull_getc(state);
  switch (ch) {
  case '"':
    return lonejson__json_pull_begin_string(state, 0);
  case '{':
    if (lonejson__json_pull_push_frame(state, LONEJSON__JSON_PULL_FRAME_OBJECT,
                                       LONEJSON__JSON_PULL_OBJECT_KEY_OR_END,
                                       depth,
                                       &state->error) != LONEJSON_STATUS_OK) {
      return state->error.code;
    }
    return lonejson__json_pull_set_pending_bytes(state, "{", 1u);
  case '[':
    if (lonejson__json_pull_push_frame(state, LONEJSON__JSON_PULL_FRAME_ARRAY,
                                       LONEJSON__JSON_PULL_ARRAY_VALUE_OR_END,
                                       depth,
                                       &state->error) != LONEJSON_STATUS_OK) {
      return state->error.code;
    }
    return lonejson__json_pull_set_pending_bytes(state, "[", 1u);
  case 't':
    return lonejson__json_pull_begin_literal(state, ch, "rue");
  case 'f':
    return lonejson__json_pull_begin_literal(state, ch, "alse");
  case 'n':
    return lonejson__json_pull_begin_literal(state, ch, "ull");
  default:
    if (ch == '-' || lonejson__is_digit(ch)) {
      return lonejson__json_pull_begin_number(state, ch);
    }
    return lonejson__set_error(&state->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                               0u, 0u, "expected JSON value");
  }
}

static LONEJSON__NOINLINE LONEJSON__COLD lonejson_status
lonejson__json_pull_step_number(lonejson__json_pull_state *state) {
  int ch;

  while (state->number_len < sizeof(state->number_buf)) {
    ch = lonejson__json_pull_getc(state);
    if (ch == -2) {
      return state->error.code;
    }
    if (!(ch >= 0 && (lonejson__is_digit(ch) || ch == '+' || ch == '-' ||
                      ch == '.' || ch == 'e' || ch == 'E'))) {
      if (ch >= 0) {
        lonejson__json_pull_ungetc(state, ch);
      }
      if (!lonejson__is_valid_json_number(state->number_buf,
                                          state->number_len)) {
        return lonejson__set_error(&state->error, LONEJSON_STATUS_INVALID_JSON,
                                   0u, 0u, 0u, "invalid JSON number");
      }
      state->lex_mode = LONEJSON_LEX_NONE;
      lonejson__json_pull_value_complete(state);
      return lonejson__json_pull_set_pending_bytes(state, state->number_buf,
                                                   state->number_len);
    }
    state->number_buf[state->number_len++] = (char)ch;
  }
  return lonejson__set_error(&state->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u,
                             0u, "JSON number exceeds maximum byte limit");
}

static LONEJSON__NOINLINE LONEJSON__COLD lonejson_status
lonejson__json_pull_step_literal(lonejson__json_pull_state *state) {
  while (state->literal_rest[state->literal_pos] != '\0') {
    int ch = lonejson__json_pull_getc(state);
    if (ch == -2) {
      return state->error.code;
    }
    if (ch != (unsigned char)state->literal_rest[state->literal_pos]) {
      return lonejson__set_error(&state->error, LONEJSON_STATUS_INVALID_JSON,
                                 0u, 0u, 0u, "invalid JSON literal");
    }
    state->literal_buf[state->literal_len++] = (char)ch;
    state->literal_pos++;
  }
  state->lex_mode = LONEJSON_LEX_NONE;
  lonejson__json_pull_value_complete(state);
  return lonejson__json_pull_set_pending_bytes(state, state->literal_buf,
                                               state->literal_len);
}

static LONEJSON__NOINLINE LONEJSON__COLD lonejson_status
lonejson__json_pull_step_string(lonejson__json_pull_state *state) {
  while (state->pending_kind == LONEJSON__JSON_PULL_PENDING_NONE) {
    int ch;
    if (state->unicode_expect_low_slash) {
      ch = lonejson__json_pull_getc(state);
      if (ch == -2) {
        return state->error.code;
      }
      if (ch != '\\') {
        return lonejson__set_error(&state->error, LONEJSON_STATUS_INVALID_JSON,
                                   0u, 0u, 0u,
                                   "invalid unicode surrogate pair");
      }
      state->unicode_expect_low_slash = 0;
      state->unicode_expect_low_u = 1;
      return lonejson__json_pull_set_pending_char(state, '\\');
    }
    if (state->unicode_expect_low_u) {
      ch = lonejson__json_pull_getc(state);
      if (ch == -2) {
        return state->error.code;
      }
      if (ch != 'u') {
        return lonejson__set_error(&state->error, LONEJSON_STATUS_INVALID_JSON,
                                   0u, 0u, 0u,
                                   "invalid unicode surrogate pair");
      }
      state->unicode_expect_low_u = 0;
      state->unicode_digits_needed = 4;
      state->unicode_accum = 0u;
      state->unicode_parsing_low = 1;
      return lonejson__json_pull_set_pending_char(state, 'u');
    }
    if (state->unicode_digits_needed != 0) {
      int hv;
      ch = lonejson__json_pull_getc(state);
      if (ch == -2) {
        return state->error.code;
      }
      if (ch == EOF) {
        return lonejson__set_error(&state->error, LONEJSON_STATUS_INVALID_JSON,
                                   0u, 0u, 0u, "invalid unicode escape");
      }
      hv = lonejson__hex_value(ch);
      if (hv < 0) {
        return lonejson__set_error(&state->error, LONEJSON_STATUS_INVALID_JSON,
                                   0u, 0u, 0u, "invalid unicode escape");
      }
      state->unicode_accum = (state->unicode_accum << 4u) | (lonejson_uint32)hv;
      state->unicode_digits_needed--;
      if (state->unicode_digits_needed == 0) {
        if (state->unicode_parsing_low) {
          if (state->unicode_accum < 0xDC00u ||
              state->unicode_accum > 0xDFFFu) {
            return lonejson__set_error(&state->error,
                                       LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                                       "invalid unicode surrogate pair");
          }
          state->unicode_pending_high = 0u;
          state->unicode_parsing_low = 0;
        } else if (state->unicode_accum >= 0xD800u &&
                   state->unicode_accum <= 0xDBFFu) {
          state->unicode_pending_high = state->unicode_accum;
          state->unicode_expect_low_slash = 1;
        } else if (state->unicode_accum >= 0xDC00u &&
                   state->unicode_accum <= 0xDFFFu) {
          return lonejson__set_error(&state->error,
                                     LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                                     "unexpected low surrogate");
        }
      }
      return lonejson__json_pull_set_pending_char(state, (unsigned char)ch);
    }
    if (state->lex_escape) {
      ch = lonejson__json_pull_getc(state);
      if (ch == -2) {
        return state->error.code;
      }
      if (ch == EOF) {
        return lonejson__set_error(&state->error, LONEJSON_STATUS_INVALID_JSON,
                                   0u, 0u, 0u, "unterminated escape sequence");
      }
      switch (ch) {
      case '"':
      case '\\':
      case '/':
      case 'b':
      case 'f':
      case 'n':
      case 'r':
      case 't':
        state->lex_escape = 0;
        return lonejson__json_pull_set_pending_char(state, (unsigned char)ch);
      case 'u':
        state->lex_escape = 0;
        state->unicode_digits_needed = 4;
        state->unicode_accum = 0u;
        state->unicode_parsing_low = 0;
        return lonejson__json_pull_set_pending_char(state, 'u');
      default:
        return lonejson__set_error(&state->error, LONEJSON_STATUS_INVALID_JSON,
                                   0u, 0u, 0u, "invalid string escape");
      }
    }
    ch = lonejson__json_pull_getc(state);
    if (ch == -2) {
      return state->error.code;
    }
    if (ch == EOF) {
      return lonejson__set_error(&state->error, LONEJSON_STATUS_INVALID_JSON,
                                 0u, 0u, 0u, "unterminated JSON string");
    }
    if (ch == '"') {
      state->lex_mode = LONEJSON_LEX_NONE;
      if (state->lex_is_key) {
        if (state->frame_count == 0u ||
            state->frames[state->frame_count - 1u].kind !=
                LONEJSON__JSON_PULL_FRAME_OBJECT) {
          return lonejson__set_error(&state->error,
                                     LONEJSON_STATUS_INTERNAL_ERROR, 0u, 0u, 0u,
                                     "object key outside object frame");
        }
        state->frames[state->frame_count - 1u].phase =
            LONEJSON__JSON_PULL_OBJECT_COLON;
      } else {
        lonejson__json_pull_value_complete(state);
      }
      return lonejson__json_pull_set_pending_bytes(state, "\"", 1u);
    }
    if (ch < 0x20) {
      return lonejson__set_error(&state->error, LONEJSON_STATUS_INVALID_JSON,
                                 0u, 0u, 0u, "control character in string");
    }
    if (ch == '\\') {
      state->lex_escape = 1;
    }
    return lonejson__json_pull_set_pending_char(state, (unsigned char)ch);
  }
  return LONEJSON_STATUS_OK;
}

static LONEJSON__NOINLINE LONEJSON__COLD lonejson_status
lonejson__json_pull_step_frame(lonejson__json_pull_state *state) {
  lonejson__json_pull_frame *frame;
  int ch;

  if (state->frame_count == 0u) {
    if (!state->root_started) {
      return lonejson__json_pull_start_value(state, 0u);
    }
    state->root_finished = 1;
    return LONEJSON_STATUS_OK;
  }

  frame = &state->frames[state->frame_count - 1u];
  switch (frame->phase) {
  case LONEJSON__JSON_PULL_OBJECT_KEY_OR_END:
    ch = lonejson__json_pull_peek_nonspace(state);
    if (ch == -2) {
      return state->error.code;
    }
    if (ch == EOF) {
      return lonejson__set_error(&state->error, LONEJSON_STATUS_INVALID_JSON,
                                 0u, 0u, 0u, "unterminated JSON object");
    }
    if (ch == '}') {
      (void)lonejson__json_pull_getc(state);
      if (state->pretty && frame->count != 0u) {
        frame->phase = LONEJSON__JSON_PULL_OBJECT_PRE_CLOSE_NL;
        return LONEJSON_STATUS_OK;
      }
      frame->phase = LONEJSON__JSON_PULL_OBJECT_CLOSE;
      return LONEJSON_STATUS_OK;
    }
    frame->phase = state->pretty ? LONEJSON__JSON_PULL_OBJECT_PRE_MEMBER_NL
                                 : LONEJSON__JSON_PULL_OBJECT_KEY;
    return LONEJSON_STATUS_OK;
  case LONEJSON__JSON_PULL_OBJECT_PRE_MEMBER_NL:
    frame->phase = LONEJSON__JSON_PULL_OBJECT_PRE_MEMBER_INDENT;
    return lonejson__json_pull_set_pending_bytes(state, "\n", 1u);
  case LONEJSON__JSON_PULL_OBJECT_PRE_MEMBER_INDENT:
    frame->phase = LONEJSON__JSON_PULL_OBJECT_KEY;
    return lonejson__json_pull_schedule_indent(state, frame->depth + 1u);
  case LONEJSON__JSON_PULL_OBJECT_KEY:
    ch = lonejson__json_pull_peek_nonspace(state);
    if (ch == -2) {
      return state->error.code;
    }
    if (ch != '"') {
      return lonejson__set_error(&state->error, LONEJSON_STATUS_INVALID_JSON,
                                 0u, 0u, 0u, "expected object key");
    }
    (void)lonejson__json_pull_getc(state);
    return lonejson__json_pull_begin_string(state, 1);
  case LONEJSON__JSON_PULL_OBJECT_COLON:
    ch = lonejson__json_pull_peek_nonspace(state);
    if (ch == -2) {
      return state->error.code;
    }
    if (ch != ':') {
      return lonejson__set_error(&state->error, LONEJSON_STATUS_INVALID_JSON,
                                 0u, 0u, 0u, "expected ':' after object key");
    }
    (void)lonejson__json_pull_getc(state);
    frame->phase = LONEJSON__JSON_PULL_OBJECT_VALUE;
    return lonejson__json_pull_set_pending_bytes(
        state, state->pretty ? ": " : ":", state->pretty ? 2u : 1u);
  case LONEJSON__JSON_PULL_OBJECT_VALUE:
    return lonejson__json_pull_start_value(state, frame->depth + 1u);
  case LONEJSON__JSON_PULL_OBJECT_COMMA_OR_END:
    ch = lonejson__json_pull_peek_nonspace(state);
    if (ch == -2) {
      return state->error.code;
    }
    if (ch == ',') {
      (void)lonejson__json_pull_getc(state);
      frame->phase = state->pretty ? LONEJSON__JSON_PULL_OBJECT_PRE_MEMBER_NL
                                   : LONEJSON__JSON_PULL_OBJECT_KEY;
      return lonejson__json_pull_set_pending_bytes(state, ",", 1u);
    }
    if (ch == '}') {
      (void)lonejson__json_pull_getc(state);
      if (state->pretty) {
        frame->phase = LONEJSON__JSON_PULL_OBJECT_PRE_CLOSE_NL;
      } else {
        frame->phase = LONEJSON__JSON_PULL_OBJECT_CLOSE;
      }
      return LONEJSON_STATUS_OK;
    }
    return lonejson__set_error(&state->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                               0u, 0u, "expected ',' or '}' in object");
  case LONEJSON__JSON_PULL_OBJECT_PRE_CLOSE_NL:
    frame->phase = LONEJSON__JSON_PULL_OBJECT_PRE_CLOSE_INDENT;
    return lonejson__json_pull_set_pending_bytes(state, "\n", 1u);
  case LONEJSON__JSON_PULL_OBJECT_PRE_CLOSE_INDENT:
    frame->phase = LONEJSON__JSON_PULL_OBJECT_CLOSE;
    return lonejson__json_pull_schedule_indent(state, frame->depth);
  case LONEJSON__JSON_PULL_OBJECT_CLOSE:
    lonejson__json_pull_pop_frame(state);
    lonejson__json_pull_value_complete(state);
    return lonejson__json_pull_set_pending_bytes(state, "}", 1u);

  case LONEJSON__JSON_PULL_ARRAY_VALUE_OR_END:
    ch = lonejson__json_pull_peek_nonspace(state);
    if (ch == -2) {
      return state->error.code;
    }
    if (ch == EOF) {
      return lonejson__set_error(&state->error, LONEJSON_STATUS_INVALID_JSON,
                                 0u, 0u, 0u, "unterminated JSON array");
    }
    if (ch == ']') {
      (void)lonejson__json_pull_getc(state);
      if (state->pretty && frame->count != 0u) {
        frame->phase = LONEJSON__JSON_PULL_ARRAY_PRE_CLOSE_NL;
      } else {
        frame->phase = LONEJSON__JSON_PULL_ARRAY_CLOSE;
      }
      return LONEJSON_STATUS_OK;
    }
    frame->phase = state->pretty ? LONEJSON__JSON_PULL_ARRAY_PRE_VALUE_NL
                                 : LONEJSON__JSON_PULL_ARRAY_VALUE;
    return LONEJSON_STATUS_OK;
  case LONEJSON__JSON_PULL_ARRAY_PRE_VALUE_NL:
    frame->phase = LONEJSON__JSON_PULL_ARRAY_PRE_VALUE_INDENT;
    return lonejson__json_pull_set_pending_bytes(state, "\n", 1u);
  case LONEJSON__JSON_PULL_ARRAY_PRE_VALUE_INDENT:
    frame->phase = LONEJSON__JSON_PULL_ARRAY_VALUE;
    return lonejson__json_pull_schedule_indent(state, frame->depth + 1u);
  case LONEJSON__JSON_PULL_ARRAY_VALUE:
    return lonejson__json_pull_start_value(state, frame->depth + 1u);
  case LONEJSON__JSON_PULL_ARRAY_COMMA_OR_END:
    ch = lonejson__json_pull_peek_nonspace(state);
    if (ch == -2) {
      return state->error.code;
    }
    if (ch == ',') {
      (void)lonejson__json_pull_getc(state);
      frame->phase = state->pretty ? LONEJSON__JSON_PULL_ARRAY_PRE_VALUE_NL
                                   : LONEJSON__JSON_PULL_ARRAY_VALUE;
      return lonejson__json_pull_set_pending_bytes(state, ",", 1u);
    }
    if (ch == ']') {
      (void)lonejson__json_pull_getc(state);
      if (state->pretty) {
        frame->phase = LONEJSON__JSON_PULL_ARRAY_PRE_CLOSE_NL;
      } else {
        frame->phase = LONEJSON__JSON_PULL_ARRAY_CLOSE;
      }
      return LONEJSON_STATUS_OK;
    }
    return lonejson__set_error(&state->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                               0u, 0u, "expected ',' or ']' in array");
  case LONEJSON__JSON_PULL_ARRAY_PRE_CLOSE_NL:
    frame->phase = LONEJSON__JSON_PULL_ARRAY_PRE_CLOSE_INDENT;
    return lonejson__json_pull_set_pending_bytes(state, "\n", 1u);
  case LONEJSON__JSON_PULL_ARRAY_PRE_CLOSE_INDENT:
    frame->phase = LONEJSON__JSON_PULL_ARRAY_CLOSE;
    return lonejson__json_pull_schedule_indent(state, frame->depth);
  case LONEJSON__JSON_PULL_ARRAY_CLOSE:
    lonejson__json_pull_pop_frame(state);
    lonejson__json_pull_value_complete(state);
    return lonejson__json_pull_set_pending_bytes(state, "]", 1u);
  default:
    return lonejson__set_error(&state->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               0u, 0u, 0u, "invalid JSON pull frame phase");
  }
}

static LONEJSON__NOINLINE LONEJSON__COLD lonejson_status
lonejson__json_pull_init(lonejson__json_pull_state *state,
                         const lonejson_json_value *value, int pretty,
                         size_t base_depth, lonejson_error *error) {
  lonejson_status status;
  lonejson_value_limits defaults;

  memset(state, 0, sizeof(*state));
  defaults = lonejson_default_value_limits();
  state->pretty = pretty;
  state->base_depth = base_depth;
  state->allocator =
      lonejson__allocator_resolve(value ? &value->allocator : NULL);
  state->limits = defaults;
  if (value != NULL && value->parse_visitor_limits.max_depth != 0u) {
    state->limits = value->parse_visitor_limits;
    if (state->limits.max_depth == 0u) {
      state->limits.max_depth = defaults.max_depth;
    }
    if (state->limits.max_string_bytes == 0u) {
      state->limits.max_string_bytes = defaults.max_string_bytes;
    }
    if (state->limits.max_number_bytes == 0u) {
      state->limits.max_number_bytes = defaults.max_number_bytes;
    }
    if (state->limits.max_key_bytes == 0u) {
      state->limits.max_key_bytes = defaults.max_key_bytes;
    }
  }
  if (value == NULL || value->kind == LONEJSON_JSON_VALUE_NULL) {
    state->initialized = 1;
    state->root_started = 1;
    state->root_finished = 1;
    state->synthetic_null = 1;
    return lonejson__json_pull_set_pending_bytes(state, "null", 4u);
  }
  status = lonejson__json_cursor_open(value, &state->cursor, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  state->initialized = 1;
  lonejson__clear_error(&state->error);
  return LONEJSON_STATUS_OK;
}

static LONEJSON__NOINLINE LONEJSON__COLD lonejson_status
lonejson__json_pull_read(lonejson__json_pull_state *state,
                         unsigned char *buffer, size_t capacity,
                         size_t *out_len, int *out_eof, lonejson_error *error) {
  lonejson_status status = LONEJSON_STATUS_OK;
  int ch;

  if (out_len != NULL) {
    *out_len = 0u;
  }
  if (out_eof != NULL) {
    *out_eof = 0;
  }
  if (state == NULL || !state->initialized) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JSON pull state is not initialized");
  }
  state->out = buffer;
  state->out_capacity = capacity;
  state->out_length = 0u;
  while (state->out_length < state->out_capacity) {
    status = lonejson__json_pull_flush_pending(state);
    if (status != LONEJSON_STATUS_OK) {
      break;
    }
    if (state->pending_kind != LONEJSON__JSON_PULL_PENDING_NONE ||
        state->out_length == state->out_capacity) {
      break;
    }
    if (state->lex_mode == LONEJSON_LEX_STRING) {
      status = lonejson__json_pull_step_string(state);
    } else if (state->lex_mode == LONEJSON_LEX_NUMBER) {
      status = lonejson__json_pull_step_number(state);
    } else if (state->lex_mode == LONEJSON_LEX_TRUE ||
               state->lex_mode == LONEJSON_LEX_FALSE ||
               state->lex_mode == LONEJSON_LEX_NULL) {
      status = lonejson__json_pull_step_literal(state);
    } else if (state->root_finished &&
               state->pending_kind == LONEJSON__JSON_PULL_PENDING_NONE &&
               state->frame_count == 0u) {
      if (state->synthetic_null) {
        state->finished = 1;
        break;
      }
      ch = lonejson__json_pull_peek_nonspace(state);
      if (ch == -2) {
        status = state->error.code;
      } else if (ch != EOF) {
        status = lonejson__set_error(
            &state->error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
            "JSON value must contain exactly one value");
      } else {
        state->finished = 1;
        break;
      }
    } else {
      status = lonejson__json_pull_step_frame(state);
    }
    if (status != LONEJSON_STATUS_OK) {
      break;
    }
  }
  if (error != NULL) {
    *error = state->error;
  }
  if (out_len != NULL) {
    *out_len = state->out_length;
  }
  if (out_eof != NULL) {
    *out_eof = state->finished &&
               state->pending_kind == LONEJSON__JSON_PULL_PENDING_NONE;
  }
  return status;
}

static void lonejson__json_pull_cleanup(lonejson__json_pull_state *state) {
  if (state == NULL) {
    return;
  }
  if (state->initialized) {
    lonejson__json_cursor_close(&state->cursor);
  }
  if (state->frames != NULL) {
    lonejson__buffer_free(&state->allocator, state->frames,
                          state->frame_capacity * sizeof(*state->frames));
  }
  memset(state, 0, sizeof(*state));
}
