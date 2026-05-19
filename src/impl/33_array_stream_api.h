#ifndef LONEJSON__OMIT_PROTOCOL_FRAMING_IMPL
static int lonejson__ascii_ieq(const char *a, size_t a_len, const char *b) {
  size_t i;
  for (i = 0u; i < a_len && b[i] != '\0'; ++i) {
    unsigned char ac = (unsigned char)a[i];
    unsigned char bc = (unsigned char)b[i];
    if (ac >= 'A' && ac <= 'Z') {
      ac = (unsigned char)(ac + ('a' - 'A'));
    }
    if (bc >= 'A' && bc <= 'Z') {
      bc = (unsigned char)(bc + ('a' - 'A'));
    }
    if (ac != bc) {
      return 0;
    }
  }
  return i == a_len && b[i] == '\0';
}

static int lonejson__parse_sse_retry(const char *value, size_t value_len,
                                     unsigned long *out) {
  unsigned long acc;
  unsigned long digit;
  size_t i;

  if (value_len == 0u) {
    return 0;
  }
  acc = 0u;
  for (i = 0u; i < value_len; ++i) {
    if (value[i] < '0' || value[i] > '9') {
      return 0;
    }
    digit = (unsigned long)(value[i] - '0');
    if (acc > (ULONG_MAX - digit) / 10u) {
      return 0;
    }
    acc = (acc * 10u) + digit;
  }
  *out = acc;
  return 1;
}
#endif

static lonejson_status lonejson__byte_reserve(lonejson__byte_buffer *buffer,
                                              size_t needed, size_t limit,
                                              const lonejson_allocator *alloc,
                                              lonejson_error *error) {
  char *next;
  size_t next_cap;

  if (needed > limit) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "framing buffer limit exceeded");
  }
  if (needed <= buffer->cap) {
    return LONEJSON_STATUS_OK;
  }
  next_cap = buffer->cap == 0u ? 64u : buffer->cap;
  while (next_cap < needed) {
    if (next_cap > (SIZE_MAX / 2u)) {
      next_cap = needed;
      break;
    }
    next_cap *= 2u;
  }
  if (next_cap > limit) {
    next_cap = needed;
  }
  next = (char *)lonejson__buffer_realloc(alloc, buffer->data, buffer->cap,
                                          next_cap);
  if (next == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to allocate framing buffer");
  }
  buffer->data = next;
  buffer->cap = next_cap;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__byte_append(lonejson__byte_buffer *buffer,
                                             const void *data, size_t len,
                                             size_t limit,
                                             const lonejson_allocator *alloc,
                                             lonejson_error *error) {
  lonejson_status status;
  size_t needed;

  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  if (buffer->len > SIZE_MAX - len) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "framing buffer size overflow");
  }
  needed = buffer->len + len;
  if (needed > limit || needed == SIZE_MAX) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "framing buffer limit exceeded");
  }
  status =
      lonejson__byte_reserve(buffer, needed + 1u, limit + 1u, alloc, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  memcpy(buffer->data + buffer->len, data, len);
  buffer->len += len;
  buffer->data[buffer->len] = '\0';
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__byte_append_char(lonejson__byte_buffer *buffer,
                                                  char ch, size_t limit,
                                                  const lonejson_allocator *a,
                                                  lonejson_error *error) {
  return lonejson__byte_append(buffer, &ch, 1u, limit, a, error);
}

static void lonejson__byte_reset(lonejson__byte_buffer *buffer) {
  buffer->len = 0u;
  if (buffer->data != NULL) {
    buffer->data[0] = '\0';
  }
}

#ifndef LONEJSON__OMIT_PROTOCOL_FRAMING_IMPL
static void lonejson__byte_consume(lonejson__byte_buffer *buffer, size_t len) {
  if (len >= buffer->len) {
    lonejson__byte_reset(buffer);
    return;
  }
  memmove(buffer->data, buffer->data + len, buffer->len - len);
  buffer->len -= len;
  buffer->data[buffer->len] = '\0';
}
#endif

static void lonejson__byte_free(lonejson__byte_buffer *buffer,
                                const lonejson_allocator *allocator) {
  lonejson__buffer_free(allocator, buffer->data, buffer->cap);
  buffer->data = NULL;
  buffer->len = 0u;
  buffer->cap = 0u;
}

static char *lonejson__dup_range(const char *begin, size_t len,
                                 const lonejson_allocator *allocator,
                                 size_t *alloc_size, lonejson_error *error) {
  char *out;
  out = (char *)lonejson__buffer_alloc(allocator, len + 1u);
  if (out == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
                        "failed to allocate string");
    return NULL;
  }
  memcpy(out, begin, len);
  out[len] = '\0';
  if (alloc_size != NULL) {
    *alloc_size = len + 1u;
  }
  return out;
}

static lonejson_status
lonejson__array_stream_set_error(lonejson_array_stream *stream,
                                 lonejson_status code, const char *message) {
  lonejson_status status;
  status = lonejson__set_error(&stream->error, code, stream->offset,
                               stream->line, stream->column, "%s", message);
  stream->state = LONEJSON_ARRAY_STREAM_STATE_ERROR;
  return status;
}

static lonejson_array_stream_result
lonejson__array_stream_result_from_status(lonejson_array_stream *stream,
                                          lonejson_status status,
                                          lonejson_error *error) {
  if (status == LONEJSON_STATUS_TRUNCATED && stream->would_block) {
    if (error != NULL) {
      *error = stream->error;
    }
    return LONEJSON_ARRAY_STREAM_WOULD_BLOCK;
  }
  if (error != NULL) {
    *error = stream->error;
  }
  return LONEJSON_ARRAY_STREAM_ERROR;
}

static lonejson_read_result
lonejson__array_stream_read(lonejson_array_stream *stream) {
  lonejson_read_result result;

  memset(&result, 0, sizeof(result));
  switch (stream->source_kind) {
  case LONEJSON_ARRAY_STREAM_SOURCE_READER:
    return stream->reader(stream->reader_user, stream->io_buffer,
                          sizeof(stream->io_buffer));
  case LONEJSON_ARRAY_STREAM_SOURCE_FILE:
    return lonejson__file_reader(stream->fp, stream->io_buffer,
                                 sizeof(stream->io_buffer));
  case LONEJSON_ARRAY_STREAM_SOURCE_FD: {
    ssize_t rc = read(stream->fd, stream->io_buffer, sizeof(stream->io_buffer));
    if (rc > 0) {
      result.bytes_read = (size_t)rc;
    } else if (rc == 0) {
      result.eof = 1;
    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
      result.would_block = 1;
    } else {
      result.error_code = errno;
    }
    return result;
  }
  case LONEJSON_ARRAY_STREAM_SOURCE_PUSH:
    if (stream->push_eof) {
      result.eof = 1;
    } else {
      result.would_block = 1;
    }
    return result;
  default:
    result.error_code = EINVAL;
    return result;
  }
}

static int lonejson__array_stream_getc(lonejson_array_stream *stream) {
  unsigned char ch;

  if (stream->has_pushback) {
    stream->has_pushback = 0;
    return stream->pushback;
  }
  if (stream->buffered_start == stream->buffered_end) {
    for (;;) {
      lonejson_read_result chunk = lonejson__array_stream_read(stream);
      if (chunk.error_code != 0) {
        stream->error.system_errno = chunk.error_code;
        (void)lonejson__array_stream_set_error(stream, LONEJSON_STATUS_IO_ERROR,
                                               "array stream read failed");
        return -2;
      }
      if (chunk.would_block) {
        stream->would_block = 1;
        return -2;
      }
      if (chunk.bytes_read == 0u && chunk.eof) {
        return EOF;
      }
      if (chunk.bytes_read != 0u) {
        stream->buffered_start = 0u;
        stream->buffered_end = chunk.bytes_read;
        break;
      }
    }
  }
  ch = stream->io_buffer[stream->buffered_start++];
  stream->offset++;
  if (ch == '\n') {
    stream->line++;
    stream->column = 0u;
  } else {
    stream->column++;
  }
  return (int)ch;
}

static void lonejson__array_stream_advance(lonejson_array_stream *stream,
                                           const unsigned char *bytes,
                                           size_t len) {
  size_t i;

  for (i = 0u; i < len; ++i) {
    stream->offset++;
    if (bytes[i] == '\n') {
      stream->line++;
      stream->column = 0u;
    } else {
      stream->column++;
    }
  }
}

static void lonejson__array_stream_ungetc(lonejson_array_stream *stream,
                                          int ch) {
  if (ch < 0) {
    return;
  }
  stream->has_pushback = 1;
  stream->pushback = ch;
  if (stream->offset != 0u) {
    stream->offset--;
  }
  if (stream->column != 0u) {
    stream->column--;
  }
}

static int lonejson__array_stream_nonspace(lonejson_array_stream *stream) {
  int ch = 0;
  do {
    ch = lonejson__array_stream_getc(stream);
  } while (ch >= 0 && lonejson__is_json_space((unsigned char)ch));
  return ch;
}

static lonejson_status
lonejson__array_stream_append_key_codepoint(lonejson_array_stream *stream,
                                            lonejson__byte_buffer *decoded_key,
                                            lonejson_uint32 cp) {
  unsigned char utf8[4];
  size_t utf8_len;

  if (decoded_key == NULL) {
    return LONEJSON_STATUS_OK;
  }
  if (cp <= 0x7Fu) {
    utf8[0] = (unsigned char)cp;
    utf8_len = 1u;
  } else if (cp <= 0x7FFu) {
    utf8[0] = (unsigned char)(0xC0u | (cp >> 6u));
    utf8[1] = (unsigned char)(0x80u | (cp & 0x3Fu));
    utf8_len = 2u;
  } else if (cp <= 0xFFFFu) {
    utf8[0] = (unsigned char)(0xE0u | (cp >> 12u));
    utf8[1] = (unsigned char)(0x80u | ((cp >> 6u) & 0x3Fu));
    utf8[2] = (unsigned char)(0x80u | (cp & 0x3Fu));
    utf8_len = 3u;
  } else if (cp <= 0x10FFFFu) {
    utf8[0] = (unsigned char)(0xF0u | (cp >> 18u));
    utf8[1] = (unsigned char)(0x80u | ((cp >> 12u) & 0x3Fu));
    utf8[2] = (unsigned char)(0x80u | ((cp >> 6u) & 0x3Fu));
    utf8[3] = (unsigned char)(0x80u | (cp & 0x3Fu));
    utf8_len = 4u;
  } else {
    return lonejson__array_stream_set_error(
        stream, LONEJSON_STATUS_INVALID_JSON, "invalid unicode codepoint");
  }
  return lonejson__byte_append(decoded_key, utf8, utf8_len, SIZE_MAX - 1u,
                               &stream->allocator, &stream->error);
}

static lonejson_status
lonejson__array_stream_read_key(lonejson_array_stream *stream) {
  int ch = 0;

  if (stream->key_phase == LONEJSON_ARRAY_STREAM_KEY_IDLE) {
    lonejson__byte_reset(&stream->key);
    stream->key_phase = LONEJSON_ARRAY_STREAM_KEY_BODY;
    stream->key_codepoint = 0u;
    stream->key_high_surrogate = 0u;
    stream->key_digits_needed = 0;
  }

  for (;;) {
    ch = lonejson__array_stream_getc(stream);
    if (ch == -2) {
      return stream->would_block ? LONEJSON_STATUS_TRUNCATED
                                 : stream->error.code;
    }
    if (ch == EOF) {
      return lonejson__array_stream_set_error(
          stream, LONEJSON_STATUS_INVALID_JSON, "unterminated JSON string");
    }

    switch (stream->key_phase) {
    case LONEJSON_ARRAY_STREAM_KEY_BODY:
      if (ch == '"') {
        stream->key_phase = LONEJSON_ARRAY_STREAM_KEY_IDLE;
        return LONEJSON_STATUS_OK;
      }
      if ((unsigned char)ch < 0x20u) {
        return lonejson__array_stream_set_error(
            stream, LONEJSON_STATUS_INVALID_JSON,
            "control character in JSON string");
      }
      if (ch == '\\') {
        stream->key_phase = LONEJSON_ARRAY_STREAM_KEY_ESCAPE;
        break;
      }
      if (lonejson__byte_append_char(&stream->key, (char)ch, SIZE_MAX - 1u,
                                     &stream->allocator,
                                     &stream->error) != LONEJSON_STATUS_OK) {
        stream->state = LONEJSON_ARRAY_STREAM_STATE_ERROR;
        return stream->error.code;
      }
      break;
    case LONEJSON_ARRAY_STREAM_KEY_ESCAPE:
      stream->key_phase = LONEJSON_ARRAY_STREAM_KEY_BODY;
      switch (ch) {
      case '"':
      case '\\':
      case '/':
        if (lonejson__byte_append_char(&stream->key, (char)ch, SIZE_MAX - 1u,
                                       &stream->allocator,
                                       &stream->error) != LONEJSON_STATUS_OK) {
          stream->state = LONEJSON_ARRAY_STREAM_STATE_ERROR;
          return stream->error.code;
        }
        break;
      case 'b':
        if (lonejson__byte_append_char(&stream->key, '\b', SIZE_MAX - 1u,
                                       &stream->allocator,
                                       &stream->error) != LONEJSON_STATUS_OK) {
          stream->state = LONEJSON_ARRAY_STREAM_STATE_ERROR;
          return stream->error.code;
        }
        break;
      case 'f':
        if (lonejson__byte_append_char(&stream->key, '\f', SIZE_MAX - 1u,
                                       &stream->allocator,
                                       &stream->error) != LONEJSON_STATUS_OK) {
          stream->state = LONEJSON_ARRAY_STREAM_STATE_ERROR;
          return stream->error.code;
        }
        break;
      case 'n':
        if (lonejson__byte_append_char(&stream->key, '\n', SIZE_MAX - 1u,
                                       &stream->allocator,
                                       &stream->error) != LONEJSON_STATUS_OK) {
          stream->state = LONEJSON_ARRAY_STREAM_STATE_ERROR;
          return stream->error.code;
        }
        break;
      case 'r':
        if (lonejson__byte_append_char(&stream->key, '\r', SIZE_MAX - 1u,
                                       &stream->allocator,
                                       &stream->error) != LONEJSON_STATUS_OK) {
          stream->state = LONEJSON_ARRAY_STREAM_STATE_ERROR;
          return stream->error.code;
        }
        break;
      case 't':
        if (lonejson__byte_append_char(&stream->key, '\t', SIZE_MAX - 1u,
                                       &stream->allocator,
                                       &stream->error) != LONEJSON_STATUS_OK) {
          stream->state = LONEJSON_ARRAY_STREAM_STATE_ERROR;
          return stream->error.code;
        }
        break;
      case 'u':
        stream->key_phase = LONEJSON_ARRAY_STREAM_KEY_UNICODE;
        stream->key_codepoint = 0u;
        stream->key_digits_needed = 4;
        break;
      default:
        return lonejson__array_stream_set_error(
            stream, LONEJSON_STATUS_INVALID_JSON, "invalid JSON string escape");
      }
      break;
    case LONEJSON_ARRAY_STREAM_KEY_UNICODE:
    case LONEJSON_ARRAY_STREAM_KEY_SURROGATE_LOW: {
      int hv = lonejson__hex_value(ch);
      lonejson_status status;
      if (hv < 0) {
        return lonejson__array_stream_set_error(
            stream, LONEJSON_STATUS_INVALID_JSON,
            "invalid unicode escape in JSON string");
      }
      stream->key_codepoint =
          (stream->key_codepoint << 4u) | (lonejson_uint32)hv;
      stream->key_digits_needed--;
      if (stream->key_digits_needed != 0) {
        break;
      }
      if (stream->key_phase == LONEJSON_ARRAY_STREAM_KEY_UNICODE &&
          stream->key_codepoint >= 0xD800u &&
          stream->key_codepoint <= 0xDBFFu) {
        stream->key_high_surrogate = stream->key_codepoint;
        stream->key_codepoint = 0u;
        stream->key_phase = LONEJSON_ARRAY_STREAM_KEY_SURROGATE_SLASH;
        break;
      }
      if (stream->key_phase == LONEJSON_ARRAY_STREAM_KEY_UNICODE &&
          stream->key_codepoint >= 0xDC00u &&
          stream->key_codepoint <= 0xDFFFu) {
        return lonejson__array_stream_set_error(
            stream, LONEJSON_STATUS_INVALID_JSON, "unexpected low surrogate");
      }
      if (stream->key_phase == LONEJSON_ARRAY_STREAM_KEY_SURROGATE_LOW) {
        if (stream->key_codepoint < 0xDC00u ||
            stream->key_codepoint > 0xDFFFu) {
          return lonejson__array_stream_set_error(
              stream, LONEJSON_STATUS_INVALID_JSON,
              "invalid unicode surrogate pair");
        }
        stream->key_codepoint =
            0x10000u + (((stream->key_high_surrogate - 0xD800u) << 10u) |
                        (stream->key_codepoint - 0xDC00u));
        stream->key_high_surrogate = 0u;
      }
      status = lonejson__array_stream_append_key_codepoint(
          stream, &stream->key, stream->key_codepoint);
      if (status != LONEJSON_STATUS_OK) {
        stream->state = LONEJSON_ARRAY_STREAM_STATE_ERROR;
        return status;
      }
      stream->key_phase = LONEJSON_ARRAY_STREAM_KEY_BODY;
      stream->key_codepoint = 0u;
      break;
    }
    case LONEJSON_ARRAY_STREAM_KEY_SURROGATE_SLASH:
      if (ch != '\\') {
        return lonejson__array_stream_set_error(
            stream, LONEJSON_STATUS_INVALID_JSON,
            "invalid unicode surrogate pair");
      }
      stream->key_phase = LONEJSON_ARRAY_STREAM_KEY_SURROGATE_U;
      break;
    case LONEJSON_ARRAY_STREAM_KEY_SURROGATE_U:
      if (ch != 'u') {
        return lonejson__array_stream_set_error(
            stream, LONEJSON_STATUS_INVALID_JSON,
            "invalid unicode surrogate pair");
      }
      stream->key_phase = LONEJSON_ARRAY_STREAM_KEY_SURROGATE_LOW;
      stream->key_codepoint = 0u;
      stream->key_digits_needed = 4;
      break;
    case LONEJSON_ARRAY_STREAM_KEY_IDLE:
      return lonejson__array_stream_set_error(
          stream, LONEJSON_STATUS_INTERNAL_ERROR,
          "invalid array stream key decoder state");
    }
  }
}

static int lonejson__array_stream_value_char_may_continue_number(int ch) {
  return lonejson__is_digit(ch) || ch == '+' || ch == '-' || ch == '.' ||
         ch == 'e' || ch == 'E';
}

static size_t
lonejson__array_stream_value_feed_len(const lonejson_array_stream *stream) {
  const unsigned char *begin;
  size_t available;
  size_t len;

  available = stream->buffered_end - stream->buffered_start;
  if (available <= 1u) {
    return available;
  }
  if (stream->value_parser.frame_count != 0u ||
      stream->value_parser.lex_mode == LONEJSON_LEX_STRING) {
    return available;
  }
  if (stream->value_parser.lex_mode == LONEJSON_LEX_NUMBER) {
    begin = stream->io_buffer + stream->buffered_start;
    len = 0u;
    while (len < available &&
           lonejson__array_stream_value_char_may_continue_number(begin[len])) {
      len++;
    }
    return len != 0u ? len : 1u;
  }
  if (stream->value_parser.lex_mode == LONEJSON_LEX_TRUE ||
      stream->value_parser.lex_mode == LONEJSON_LEX_FALSE ||
      stream->value_parser.lex_mode == LONEJSON_LEX_NULL) {
    return available;
  }
  return 1u;
}

static void lonejson__array_stream_dup_state_cleanup(
    lonejson__array_stream_dup_state *state);
static lonejson_status
lonejson__array_stream_dup_push_object(void *user, lonejson_error *error);
static lonejson_status
lonejson__array_stream_dup_pop_object(void *user, lonejson_error *error);
static lonejson_status
lonejson__array_stream_dup_key_begin(void *user, lonejson_error *error);
static lonejson_status
lonejson__array_stream_dup_key_chunk(void *user, const char *data, size_t len,
                                     lonejson_error *error);
static lonejson_status
lonejson__array_stream_dup_key_end(void *user, lonejson_error *error);
static lonejson_status lonejson__array_stream_string_begin(void *user,
                                                           lonejson_error *e);
static lonejson_status lonejson__array_stream_string_chunk(void *user,
                                                           const char *data,
                                                           size_t len,
                                                           lonejson_error *e);
static lonejson_status lonejson__array_stream_string_end(void *user,
                                                         lonejson_error *e);
static lonejson_status
lonejson__array_stream_string_item_begin(void *user, lonejson_error *error);
static lonejson_status
lonejson__array_stream_string_item_chunk(void *user, const char *data,
                                         size_t len, lonejson_error *error);
static lonejson_status
lonejson__array_stream_string_item_end(void *user, lonejson_error *error);
static lonejson_status
lonejson__array_stream_string_type_error(lonejson_array_stream *stream,
                                         lonejson_error *error);
static lonejson_status
lonejson__array_stream_string_bad_event(void *user, lonejson_error *error);
static lonejson_status
lonejson__array_stream_string_bad_chunk(void *user, const char *data,
                                        size_t len, lonejson_error *error);
static lonejson_status
lonejson__array_stream_string_bad_bool(void *user, int value,
                                       lonejson_error *error);

static lonejson_status
lonejson__array_stream_start_value(lonejson_array_stream *stream,
                                   lonejson_array_stream_value_mode mode,
                                   const lonejson_map *map, void *dst,
                                   lonejson_sink_fn sink, void *sink_user) {
  lonejson_value_limits limits;

  if (stream->value_active) {
    if (stream->value_mode != mode || stream->active_map != map ||
        stream->active_dst != dst) {
      return lonejson__array_stream_set_error(
          stream, LONEJSON_STATUS_INVALID_ARGUMENT,
          "resume array stream item with the same destination");
    }
    return LONEJSON_STATUS_OK;
  }
  if (mode == LONEJSON_ARRAY_STREAM_VALUE_RAW) {
    lonejson__byte_reset(&stream->item);
  }
  lonejson__parser_init_state(
      &stream->value_parser, map, dst, &stream->options,
      mode == LONEJSON_ARRAY_STREAM_VALUE_MAPPED ? 0 : 1,
      stream->value_workspace, sizeof(stream->value_workspace));
  if ((mode == LONEJSON_ARRAY_STREAM_VALUE_SKIP ||
       mode == LONEJSON_ARRAY_STREAM_VALUE_SINK) &&
      stream->options.reject_duplicate_keys) {
    memset(&stream->skip_dup_state, 0, sizeof(stream->skip_dup_state));
    memset(&stream->skip_value, 0, sizeof(stream->skip_value));
    stream->skip_dup_state.allocator = &stream->allocator;
    stream->skip_visitor = lonejson_default_value_visitor();
    stream->skip_visitor.object_begin = lonejson__array_stream_dup_push_object;
    stream->skip_visitor.object_end = lonejson__array_stream_dup_pop_object;
    stream->skip_visitor.object_key_begin =
        lonejson__array_stream_dup_key_begin;
    stream->skip_visitor.object_key_chunk =
        lonejson__array_stream_dup_key_chunk;
    stream->skip_visitor.object_key_end = lonejson__array_stream_dup_key_end;
    limits = lonejson_default_value_limits();
    limits.max_depth = stream->options.max_depth == 0u
                           ? lonejson_default_parse_options().max_depth
                           : stream->options.max_depth;
    limits.max_string_bytes = 0u;
    limits.max_number_bytes = 0u;
    limits.max_key_bytes = 0u;
    limits.max_total_bytes = 0u;
    stream->skip_value.parse_mode = LONEJSON_JSON_VALUE_PARSE_VISITOR;
    stream->skip_value.parse_visitor = &stream->skip_visitor;
    stream->skip_value.parse_visitor_user = &stream->skip_dup_state;
    stream->skip_value.parse_visitor_limits = limits;
    stream->skip_value.allocator = stream->allocator;
    lonejson__parser_set_json_stream_value(&stream->value_parser,
                                           &stream->skip_value);
    stream->skip_dup_active = 1;
  }
  if (mode == LONEJSON_ARRAY_STREAM_VALUE_STRING) {
    limits = lonejson_default_value_limits();
    limits.max_depth = stream->options.max_depth == 0u
                           ? lonejson_default_parse_options().max_depth
                           : stream->options.max_depth;
    limits.max_string_bytes = 0u;
    limits.max_number_bytes = 0u;
    limits.max_key_bytes = 0u;
    limits.max_total_bytes = 0u;
    memset(&stream->string_value, 0, sizeof(stream->string_value));
    stream->string_visitor = lonejson_default_value_visitor();
    stream->string_visitor.string_begin = lonejson__array_stream_string_begin;
    stream->string_visitor.string_chunk = lonejson__array_stream_string_chunk;
    stream->string_visitor.string_end = lonejson__array_stream_string_end;
    stream->string_visitor.object_begin =
        lonejson__array_stream_string_bad_event;
    stream->string_visitor.object_end = lonejson__array_stream_string_bad_event;
    stream->string_visitor.array_begin =
        lonejson__array_stream_string_bad_event;
    stream->string_visitor.array_end = lonejson__array_stream_string_bad_event;
    stream->string_visitor.object_key_begin =
        lonejson__array_stream_string_bad_event;
    stream->string_visitor.object_key_chunk =
        lonejson__array_stream_string_bad_chunk;
    stream->string_visitor.object_key_end =
        lonejson__array_stream_string_bad_event;
    stream->string_visitor.number_begin =
        lonejson__array_stream_string_bad_event;
    stream->string_visitor.number_chunk =
        lonejson__array_stream_string_bad_chunk;
    stream->string_visitor.number_end = lonejson__array_stream_string_bad_event;
    stream->string_visitor.boolean_value =
        lonejson__array_stream_string_bad_bool;
    stream->string_visitor.null_value = lonejson__array_stream_string_bad_event;
    stream->string_value.parse_mode = LONEJSON_JSON_VALUE_PARSE_VISITOR;
    stream->string_value.parse_visitor = &stream->string_visitor;
    stream->string_value.parse_visitor_user = stream;
    stream->string_value.parse_visitor_limits = limits;
    stream->string_value.allocator = stream->allocator;
    stream->string_active = 1;
    stream->string_seen = 0;
    lonejson__parser_set_json_stream_value(&stream->value_parser,
                                           &stream->string_value);
  }
  if (mode == LONEJSON_ARRAY_STREAM_VALUE_MAPPED &&
      stream->value_parser.options.clear_destination) {
    lonejson__init_map_with_allocator(map, dst,
                                      &stream->value_parser.allocator);
    stream->value_parser.options.clear_destination = 0;
  }
  stream->value_mode = mode;
  stream->active_map = map;
  stream->active_dst = dst;
  stream->active_sink = sink;
  stream->active_sink_user = sink_user;
  stream->value_active = 1;
  return LONEJSON_STATUS_OK;
}

static void lonejson__array_stream_clear_value(lonejson_array_stream *stream) {
  if (stream->value_active) {
    lonejson_parser_destroy(&stream->value_parser);
  }
  if (stream->skip_dup_active) {
    lonejson__array_stream_dup_state_cleanup(&stream->skip_dup_state);
    stream->skip_dup_active = 0;
  }
  stream->string_active = 0;
  stream->value_active = 0;
  stream->value_mode = LONEJSON_ARRAY_STREAM_VALUE_NONE;
  stream->active_map = NULL;
  stream->active_dst = NULL;
}

static lonejson_status
lonejson__array_stream_finish_value(lonejson_array_stream *stream) {
  lonejson_status status = lonejson_parser_finish(&stream->value_parser);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    if (stream->value_parser.error.code != LONEJSON_STATUS_OK) {
      stream->error = stream->value_parser.error;
    } else if (stream->error.code != status) {
      (void)lonejson__set_error(&stream->error, status, stream->offset,
                                stream->line, stream->column,
                                "array stream value parser failed");
    }
    stream->state = LONEJSON_ARRAY_STREAM_STATE_ERROR;
    lonejson__array_stream_clear_value(stream);
    return status;
  }
  stream->error = stream->value_parser.error;
  lonejson__array_stream_clear_value(stream);
  return status;
}

static void
lonejson__array_stream_mark_pending_item(lonejson_array_stream *stream,
                                         lonejson_array_stream_value_mode mode,
                                         const lonejson_map *map, void *dst) {
  stream->item_pending = 1;
  stream->pending_value_mode = mode;
  stream->pending_map = map;
  stream->pending_dst = dst;
}

static void
lonejson__array_stream_clear_pending_item(lonejson_array_stream *stream) {
  stream->item_pending = 0;
  stream->pending_value_mode = LONEJSON_ARRAY_STREAM_VALUE_NONE;
  stream->pending_map = NULL;
  stream->pending_dst = NULL;
}

static lonejson_status
lonejson__array_stream_check_pending_item(lonejson_array_stream *stream,
                                          lonejson_array_stream_value_mode mode,
                                          const lonejson_map *map, void *dst) {
  if (!stream->item_pending) {
    return LONEJSON_STATUS_OK;
  }
  if (stream->pending_value_mode != mode || stream->pending_map != map ||
      stream->pending_dst != dst) {
    return lonejson__array_stream_set_error(
        stream, LONEJSON_STATUS_INVALID_ARGUMENT,
        "resume array stream item with the same destination");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__array_stream_step_value(lonejson_array_stream *stream,
                                  lonejson_array_stream_value_mode mode,
                                  const lonejson_map *map, void *dst,
                                  lonejson_sink_fn sink, void *sink_user) {
  lonejson_status status;

  status = lonejson__array_stream_start_value(stream, mode, map, dst, sink,
                                              sink_user);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }

  for (;;) {
    if (lonejson__parser_root_complete(&stream->value_parser)) {
      return lonejson__array_stream_finish_value(stream);
    }
    if (stream->has_pushback) {
      unsigned char byte = (unsigned char)stream->pushback;
      size_t consumed = 0u;

      if (stream->value_parser.frame_count == 0u &&
          stream->value_parser.lex_mode == LONEJSON_LEX_NUMBER &&
          !lonejson__array_stream_value_char_may_continue_number(byte)) {
        return lonejson__array_stream_finish_value(stream);
      }
      stream->has_pushback = 0;
      status = lonejson__parser_feed_bytes(&stream->value_parser, &byte, 1u,
                                           &consumed, 1);
      if (consumed != 0u) {
        if (mode == LONEJSON_ARRAY_STREAM_VALUE_RAW &&
            lonejson__byte_append(&stream->item, &byte, 1u, SIZE_MAX - 1u,
                                  &stream->allocator,
                                  &stream->error) != LONEJSON_STATUS_OK) {
          stream->state = LONEJSON_ARRAY_STREAM_STATE_ERROR;
          lonejson__array_stream_clear_value(stream);
          return stream->error.code;
        }
        if (mode == LONEJSON_ARRAY_STREAM_VALUE_SINK) {
          status = sink(sink_user, &byte, 1u, &stream->error);
          if (status != LONEJSON_STATUS_OK) {
            stream->state = LONEJSON_ARRAY_STREAM_STATE_ERROR;
            lonejson__array_stream_clear_value(stream);
            return status;
          }
        }
        lonejson__array_stream_advance(stream, &byte, 1u);
      } else {
        stream->has_pushback = 1;
      }
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        if (stream->value_parser.error.code != LONEJSON_STATUS_OK) {
          stream->error = stream->value_parser.error;
        } else if (stream->error.code != status) {
          (void)lonejson__set_error(&stream->error, status, stream->offset,
                                    stream->line, stream->column,
                                    "array stream value parser failed");
        }
        stream->state = LONEJSON_ARRAY_STREAM_STATE_ERROR;
        lonejson__array_stream_clear_value(stream);
        return status;
      }
      if (lonejson__parser_root_complete(&stream->value_parser)) {
        return lonejson__array_stream_finish_value(stream);
      }
      if (consumed == 0u) {
        return lonejson__array_stream_set_error(
            stream, LONEJSON_STATUS_INTERNAL_ERROR,
            "array stream value parser made no progress");
      }
      continue;
    }
    if (stream->buffered_start == stream->buffered_end) {
      lonejson_read_result chunk = lonejson__array_stream_read(stream);
      if (chunk.error_code != 0) {
        stream->error.system_errno = chunk.error_code;
        return lonejson__array_stream_set_error(
            stream, LONEJSON_STATUS_IO_ERROR, "array stream read failed");
      }
      if (chunk.would_block) {
        stream->would_block = 1;
        return LONEJSON_STATUS_TRUNCATED;
      }
      if (chunk.bytes_read == 0u && chunk.eof) {
        if (stream->value_parser.lex_mode == LONEJSON_LEX_NUMBER) {
          return lonejson__array_stream_finish_value(stream);
        }
        return lonejson__array_stream_set_error(
            stream, LONEJSON_STATUS_INVALID_JSON, "unterminated JSON value");
      }
      if (chunk.bytes_read == 0u) {
        continue;
      }
      stream->buffered_start = 0u;
      stream->buffered_end = chunk.bytes_read;
    }

    if (stream->value_parser.frame_count == 0u &&
        stream->value_parser.lex_mode == LONEJSON_LEX_NUMBER &&
        !lonejson__array_stream_value_char_may_continue_number(
            stream->io_buffer[stream->buffered_start])) {
      return lonejson__array_stream_finish_value(stream);
    }

    {
      size_t consumed = 0u;
      const unsigned char *begin = stream->io_buffer + stream->buffered_start;
      size_t available = lonejson__array_stream_value_feed_len(stream);

      status = lonejson__parser_feed_bytes(&stream->value_parser, begin,
                                           available, &consumed, 1);
      if (consumed != 0u) {
        if (mode == LONEJSON_ARRAY_STREAM_VALUE_RAW &&
            lonejson__byte_append(&stream->item, begin, consumed, SIZE_MAX - 1u,
                                  &stream->allocator,
                                  &stream->error) != LONEJSON_STATUS_OK) {
          stream->state = LONEJSON_ARRAY_STREAM_STATE_ERROR;
          lonejson__array_stream_clear_value(stream);
          return stream->error.code;
        }
        if (mode == LONEJSON_ARRAY_STREAM_VALUE_SINK) {
          status = sink(sink_user, begin, consumed, &stream->error);
          if (status != LONEJSON_STATUS_OK) {
            stream->state = LONEJSON_ARRAY_STREAM_STATE_ERROR;
            lonejson__array_stream_clear_value(stream);
            return status;
          }
        }
        lonejson__array_stream_advance(stream, begin, consumed);
        stream->buffered_start += consumed;
      }
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        if (stream->value_parser.error.code != LONEJSON_STATUS_OK) {
          stream->error = stream->value_parser.error;
        } else if (stream->error.code != status) {
          (void)lonejson__set_error(&stream->error, status, stream->offset,
                                    stream->line, stream->column,
                                    "array stream value parser failed");
        }
        stream->state = LONEJSON_ARRAY_STREAM_STATE_ERROR;
        lonejson__array_stream_clear_value(stream);
        return status;
      }
      if (lonejson__parser_root_complete(&stream->value_parser)) {
        return lonejson__array_stream_finish_value(stream);
      }
      if (consumed == 0u) {
        return lonejson__array_stream_set_error(
            stream, LONEJSON_STATUS_INTERNAL_ERROR,
            "array stream value parser made no progress");
      }
    }
  }
}

static lonejson_status
lonejson__array_stream_note_root_key(lonejson_array_stream *stream) {
  size_t pos = 0u;
  lonejson_status status;

  if (!stream->options.reject_duplicate_keys) {
    return LONEJSON_STATUS_OK;
  }
  while (pos + sizeof(size_t) <= stream->root_keys.len) {
    size_t len;

    memcpy(&len, stream->root_keys.data + pos, sizeof(len));
    pos += sizeof(len);
    if (len > stream->root_keys.len - pos) {
      return lonejson__array_stream_set_error(
          stream, LONEJSON_STATUS_INTERNAL_ERROR,
          "corrupt array stream root key set");
    }
    if (len == stream->key.len &&
        (len == 0u ||
         memcmp(stream->root_keys.data + pos, stream->key.data, len) == 0)) {
      return lonejson__array_stream_set_error(stream,
                                              LONEJSON_STATUS_DUPLICATE_FIELD,
                                              "duplicate root object field");
    }
    pos += len;
  }
  if (pos != stream->root_keys.len) {
    return lonejson__array_stream_set_error(
        stream, LONEJSON_STATUS_INTERNAL_ERROR,
        "corrupt array stream root key set");
  }
  status = lonejson__byte_append(&stream->root_keys, &stream->key.len,
                                 sizeof(stream->key.len), SIZE_MAX - 1u,
                                 &stream->allocator, &stream->error);
  if (status != LONEJSON_STATUS_OK) {
    stream->state = LONEJSON_ARRAY_STREAM_STATE_ERROR;
    return status;
  }
  status = lonejson__byte_append(&stream->root_keys, stream->key.data,
                                 stream->key.len, SIZE_MAX - 1u,
                                 &stream->allocator, &stream->error);
  if (status != LONEJSON_STATUS_OK) {
    stream->state = LONEJSON_ARRAY_STREAM_STATE_ERROR;
  }
  return status;
}

static void lonejson__array_stream_dup_state_cleanup(
    lonejson__array_stream_dup_state *state) {
  size_t i;

  for (i = 0u; i < state->frame_count; ++i) {
    lonejson__byte_free(&state->frames[i].keys, state->allocator);
  }
  lonejson__buffer_free(state->allocator, state->frames,
                        state->frame_capacity * sizeof(state->frames[0]));
  lonejson__byte_free(&state->key, state->allocator);
  memset(state, 0, sizeof(*state));
}

static lonejson_status
lonejson__array_stream_dup_push_object(void *user, lonejson_error *error) {
  lonejson__array_stream_dup_state *state =
      (lonejson__array_stream_dup_state *)user;
  lonejson__array_stream_dup_frame *next;
  size_t next_cap;

  if (state->frame_count == state->frame_capacity) {
    next_cap = state->frame_capacity == 0u ? 8u : state->frame_capacity * 2u;
    next = (lonejson__array_stream_dup_frame *)lonejson__buffer_realloc(
        state->allocator, state->frames,
        state->frame_capacity * sizeof(state->frames[0]),
        next_cap * sizeof(state->frames[0]));
    if (next == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u,
                                 "failed to allocate duplicate-key tracker");
    }
    state->frames = next;
    state->frame_capacity = next_cap;
  }
  memset(&state->frames[state->frame_count], 0, sizeof(state->frames[0]));
  state->frame_count++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__array_stream_dup_pop_object(void *user, lonejson_error *error) {
  lonejson__array_stream_dup_state *state =
      (lonejson__array_stream_dup_state *)user;

  (void)error;
  if (state->frame_count != 0u) {
    state->frame_count--;
    lonejson__byte_free(&state->frames[state->frame_count].keys,
                        state->allocator);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__array_stream_dup_key_begin(void *user, lonejson_error *error) {
  lonejson__array_stream_dup_state *state =
      (lonejson__array_stream_dup_state *)user;

  (void)error;
  lonejson__byte_reset(&state->key);
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__array_stream_dup_key_chunk(void *user, const char *data, size_t len,
                                     lonejson_error *error) {
  lonejson__array_stream_dup_state *state =
      (lonejson__array_stream_dup_state *)user;

  return lonejson__byte_append(&state->key, data, len, SIZE_MAX - 1u,
                               state->allocator, error);
}

static lonejson_status
lonejson__array_stream_dup_key_end(void *user, lonejson_error *error) {
  lonejson__array_stream_dup_state *state =
      (lonejson__array_stream_dup_state *)user;
  lonejson__array_stream_dup_frame *frame;
  size_t pos = 0u;
  lonejson_status status;

  if (state->frame_count == 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INTERNAL_ERROR, 0u, 0u,
                               0u, "object key outside object");
  }
  frame = &state->frames[state->frame_count - 1u];
  while (pos + sizeof(size_t) <= frame->keys.len) {
    size_t len;

    memcpy(&len, frame->keys.data + pos, sizeof(len));
    pos += sizeof(len);
    if (len > frame->keys.len - pos) {
      return lonejson__set_error(error, LONEJSON_STATUS_INTERNAL_ERROR, 0u, 0u,
                                 0u, "corrupt duplicate-key tracker");
    }
    if (len == state->key.len &&
        (len == 0u ||
         memcmp(frame->keys.data + pos, state->key.data, len) == 0)) {
      return lonejson__set_error(error, LONEJSON_STATUS_DUPLICATE_FIELD, 0u, 0u,
                                 0u, "duplicate object field");
    }
    pos += len;
  }
  if (pos != frame->keys.len) {
    return lonejson__set_error(error, LONEJSON_STATUS_INTERNAL_ERROR, 0u, 0u,
                               0u, "corrupt duplicate-key tracker");
  }
  status = lonejson__byte_append(&frame->keys, &state->key.len,
                                 sizeof(state->key.len), SIZE_MAX - 1u,
                                 state->allocator, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__byte_append(&frame->keys, state->key.data, state->key.len,
                               SIZE_MAX - 1u, state->allocator, error);
}

static lonejson_status
lonejson__array_stream_string_type_error(lonejson_array_stream *stream,
                                         lonejson_error *error) {
  lonejson_status status =
      lonejson__array_stream_set_error(stream, LONEJSON_STATUS_TYPE_MISMATCH,
                                       "selected array item is not a string");
  if (error != NULL) {
    *error = stream->error;
  }
  return status;
}

static lonejson_status
lonejson__array_stream_string_bad_event(void *user, lonejson_error *error) {
  return lonejson__array_stream_string_type_error((lonejson_array_stream *)user,
                                                  error);
}

static lonejson_status
lonejson__array_stream_string_bad_chunk(void *user, const char *data,
                                        size_t len, lonejson_error *error) {
  (void)data;
  (void)len;
  return lonejson__array_stream_string_type_error((lonejson_array_stream *)user,
                                                  error);
}

static lonejson_status
lonejson__array_stream_string_bad_bool(void *user, int value,
                                       lonejson_error *error) {
  (void)value;
  return lonejson__array_stream_string_type_error((lonejson_array_stream *)user,
                                                  error);
}

static lonejson_status lonejson__array_stream_string_begin(void *user,
                                                           lonejson_error *e) {
  lonejson_array_stream *stream = (lonejson_array_stream *)user;
  if (stream->string_seen) {
    return lonejson__array_stream_string_type_error(stream, e);
  }
  stream->string_seen = 1;
  if (stream->string_handler != NULL && stream->string_handler->begin != NULL) {
    return stream->string_handler->begin(stream->string_user, e);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__array_stream_string_chunk(void *user,
                                                           const char *data,
                                                           size_t len,
                                                           lonejson_error *e) {
  lonejson_array_stream *stream = (lonejson_array_stream *)user;
  if (stream->string_handler != NULL && stream->string_handler->chunk != NULL) {
    return stream->string_handler->chunk(stream->string_user, data, len, e);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__array_stream_string_end(void *user,
                                                         lonejson_error *e) {
  (void)user;
  (void)e;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__array_stream_string_item_begin(void *user, lonejson_error *error) {
  lonejson_array_stream *stream = (lonejson_array_stream *)user;
  (void)error;
  lonejson__byte_reset(&stream->string_item);
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__array_stream_string_item_chunk(void *user, const char *data,
                                         size_t len, lonejson_error *error) {
  lonejson_array_stream *stream = (lonejson_array_stream *)user;
  return lonejson__byte_append(&stream->string_item, data, len,
                               stream->string_item_max_bytes,
                               &stream->allocator, error);
}

static lonejson_status
lonejson__array_stream_string_item_end(void *user, lonejson_error *error) {
  lonejson_array_stream *stream = (lonejson_array_stream *)user;
  lonejson_status status;

  if (stream->string_callback == NULL) {
    return LONEJSON_STATUS_OK;
  }
  status = stream->string_callback(
      stream->string_callback_user,
      stream->string_item.data != NULL ? stream->string_item.data : "",
      stream->string_item.len, error);
  lonejson__byte_reset(&stream->string_item);
  return status;
}

static lonejson_status
lonejson__array_stream_validate_duplicate_keys(lonejson_array_stream *stream,
                                               const void *data, size_t len) {
  lonejson__array_stream_dup_state state;
  lonejson_value_visitor visitor;
  lonejson_value_limits limits;
  lonejson_status status;

  if (!stream->options.reject_duplicate_keys) {
    return LONEJSON_STATUS_OK;
  }
  memset(&state, 0, sizeof(state));
  state.allocator = &stream->allocator;
  visitor = lonejson_default_value_visitor();
  visitor.object_begin = lonejson__array_stream_dup_push_object;
  visitor.object_end = lonejson__array_stream_dup_pop_object;
  visitor.object_key_begin = lonejson__array_stream_dup_key_begin;
  visitor.object_key_chunk = lonejson__array_stream_dup_key_chunk;
  visitor.object_key_end = lonejson__array_stream_dup_key_end;
  limits = lonejson_default_value_limits();
  limits.max_depth = stream->options.max_depth == 0u
                         ? lonejson_default_parse_options().max_depth
                         : stream->options.max_depth;
  limits.max_string_bytes = len == 0u ? 1u : len;
  limits.max_number_bytes = len == 0u ? 1u : len;
  limits.max_key_bytes = len == 0u ? 1u : len;
  limits.max_total_bytes = 0u;
  status = lonejson_visit_value_buffer(data, len, &visitor, &state, &limits,
                                       &stream->error);
  lonejson__array_stream_dup_state_cleanup(&state);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    stream->state = LONEJSON_ARRAY_STREAM_STATE_ERROR;
  }
  return status;
}

static lonejson_status
lonejson__array_stream_getc_status(lonejson_array_stream *stream, int *out) {
  int ch = lonejson__array_stream_nonspace(stream);
  if (ch == -2) {
    return stream->would_block ? LONEJSON_STATUS_TRUNCATED : stream->error.code;
  }
  *out = ch;
  return LONEJSON_STATUS_OK;
}

static lonejson_array_stream *
lonejson__array_stream_open_common(const char *path,
                                   const lonejson_parse_options *options,
                                   lonejson_error *error) {
  lonejson_array_stream *stream;
  lonejson_parse_options resolved;
  size_t path_len;

  resolved = options ? *options : lonejson_default_parse_options();
  if (!LONEJSON__ALLOCATOR_IS_VALID_CONFIG(resolved.allocator)) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "allocator must provide either all callbacks or none");
    return NULL;
  }
  path_len = path == NULL ? 0u : strlen(path);
  if (path_len != 0u && strchr(path, '.') != NULL) {
    lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "array stream v1 supports only one direct path segment");
    return NULL;
  }
  stream = (lonejson_array_stream *)lonejson__buffer_alloc(resolved.allocator,
                                                           sizeof(*stream));
  if (stream == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
                        "failed to allocate array stream");
    return NULL;
  }
  memset(stream, 0, sizeof(*stream));
  stream->fd = -1;
  stream->line = 1u;
  stream->options = resolved;
  stream->allocator = lonejson__allocator_resolve(resolved.allocator);
  stream->self_alloc_size = sizeof(*stream);
  stream->state = LONEJSON_ARRAY_STREAM_STATE_INIT;
  if (path_len != 0u) {
    stream->path = lonejson__dup_range(path, path_len, &stream->allocator,
                                       &stream->path_alloc_size, error);
    if (stream->path == NULL) {
      lonejson__buffer_free(&stream->allocator, stream,
                            stream->self_alloc_size);
      return NULL;
    }
    stream->path_len = path_len;
  }
  lonejson__clear_error(&stream->error);
  lonejson__clear_error(error);
  return stream;
}

lonejson_array_stream *lonejson_array_stream_open_reader(
    const char *path, lonejson_reader_fn reader, void *user,
    const lonejson_parse_options *options, lonejson_error *error) {
  lonejson_array_stream *stream;

  if (reader == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "reader callback is required");
    return NULL;
  }
  stream = lonejson__array_stream_open_common(path, options, error);
  if (stream == NULL) {
    return NULL;
  }
  stream->source_kind = LONEJSON_ARRAY_STREAM_SOURCE_READER;
  stream->reader = reader;
  stream->reader_user = user;
  return stream;
}

lonejson_array_stream *
lonejson_array_stream_open_filep(const char *path, FILE *fp,
                                 const lonejson_parse_options *options,
                                 lonejson_error *error) {
  lonejson_array_stream *stream;

  if (fp == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "file pointer is required");
    return NULL;
  }
  stream = lonejson__array_stream_open_common(path, options, error);
  if (stream == NULL) {
    return NULL;
  }
  stream->source_kind = LONEJSON_ARRAY_STREAM_SOURCE_FILE;
  stream->fp = fp;
  return stream;
}

lonejson_array_stream *
lonejson_array_stream_open_path(const char *array_path, const char *path,
                                const lonejson_parse_options *options,
                                lonejson_error *error) {
  lonejson_array_stream *stream;
  FILE *fp;

  if (path == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "path is required");
    return NULL;
  }
  fp = fopen(path, "rb");
  if (fp == NULL) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 1u, 0u,
                        "failed to open '%s'", path);
    return NULL;
  }
  stream = lonejson_array_stream_open_filep(array_path, fp, options, error);
  if (stream == NULL) {
    fclose(fp);
    return NULL;
  }
  stream->owns_fp = 1;
  return stream;
}

lonejson_array_stream *
lonejson_array_stream_open_fd(const char *path, int fd,
                              const lonejson_parse_options *options,
                              lonejson_error *error) {
  lonejson_array_stream *stream;

  if (fd < 0) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "fd must be non-negative");
    return NULL;
  }
  stream = lonejson__array_stream_open_common(path, options, error);
  if (stream == NULL) {
    return NULL;
  }
  stream->source_kind = LONEJSON_ARRAY_STREAM_SOURCE_FD;
  stream->fd = fd;
  return stream;
}

lonejson_array_stream *
lonejson_array_stream_open_push(const char *path,
                                const lonejson_parse_options *options,
                                lonejson_error *error) {
  lonejson_array_stream *stream;

  stream = lonejson__array_stream_open_common(path, options, error);
  if (stream == NULL) {
    return NULL;
  }
  stream->source_kind = LONEJSON_ARRAY_STREAM_SOURCE_PUSH;
  return stream;
}

static lonejson_array_stream_result lonejson__array_stream_next_raw(
    lonejson_array_stream *stream, lonejson_array_stream_value_mode mode,
    const lonejson_map *map, void *dst, lonejson_sink_fn sink, void *sink_user,
    lonejson_error *error) {
  int ch = 0;
  lonejson_status status;

  if (stream == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "array stream is required");
    return LONEJSON_ARRAY_STREAM_ERROR;
  }
  if (stream->state == LONEJSON_ARRAY_STREAM_STATE_ERROR) {
    if (error != NULL) {
      *error = stream->error;
    }
    return LONEJSON_ARRAY_STREAM_ERROR;
  }
  if (stream->state == LONEJSON_ARRAY_STREAM_STATE_DONE) {
    if (error != NULL) {
      *error = stream->error;
    }
    return LONEJSON_ARRAY_STREAM_EOF;
  }
  stream->would_block = 0;

  for (;;) {
    switch (stream->phase) {
    case LONEJSON_ARRAY_STREAM_PHASE_START:
      status = lonejson__array_stream_getc_status(stream, &ch);
      if (status != LONEJSON_STATUS_OK) {
        return lonejson__array_stream_result_from_status(stream, status, error);
      }
      if (stream->path_len == 0u) {
        if (ch != '[') {
          (void)lonejson__array_stream_set_error(
              stream, LONEJSON_STATUS_TYPE_MISMATCH,
              "selected JSON value is not an array");
          if (error != NULL) {
            *error = stream->error;
          }
          return LONEJSON_ARRAY_STREAM_ERROR;
        }
        stream->state = LONEJSON_ARRAY_STREAM_STATE_IN_ARRAY;
        stream->root_array_selected = 1;
        stream->first_item = 1;
        stream->phase = LONEJSON_ARRAY_STREAM_PHASE_ARRAY_VALUE_OR_END;
        break;
      }
      if (ch != '{') {
        (void)lonejson__array_stream_set_error(
            stream, LONEJSON_STATUS_TYPE_MISMATCH,
            "array path requires a root object");
        if (error != NULL) {
          *error = stream->error;
        }
        return LONEJSON_ARRAY_STREAM_ERROR;
      }
      stream->phase = LONEJSON_ARRAY_STREAM_PHASE_ROOT_OBJECT_KEY_OR_END;
      break;

    case LONEJSON_ARRAY_STREAM_PHASE_ROOT_OBJECT_KEY_OR_END:
      if (stream->key_phase == LONEJSON_ARRAY_STREAM_KEY_IDLE) {
        status = lonejson__array_stream_getc_status(stream, &ch);
        if (status != LONEJSON_STATUS_OK) {
          return lonejson__array_stream_result_from_status(stream, status,
                                                           error);
        }
        if (ch == '}') {
          if (stream->root_object_after_comma) {
            (void)lonejson__array_stream_set_error(
                stream, LONEJSON_STATUS_INVALID_JSON, "expected object key");
            if (error != NULL) {
              *error = stream->error;
            }
            return LONEJSON_ARRAY_STREAM_ERROR;
          }
          if (!stream->selected_seen) {
            (void)lonejson__array_stream_set_error(
                stream, LONEJSON_STATUS_MISSING_REQUIRED_FIELD,
                "selected array field was not found");
            if (error != NULL) {
              *error = stream->error;
            }
            return LONEJSON_ARRAY_STREAM_ERROR;
          }
          stream->phase = LONEJSON_ARRAY_STREAM_PHASE_FINISH_DOCUMENT;
          break;
        }
        if (ch != '"') {
          (void)lonejson__array_stream_set_error(
              stream, LONEJSON_STATUS_INVALID_JSON, "expected object key");
          if (error != NULL) {
            *error = stream->error;
          }
          return LONEJSON_ARRAY_STREAM_ERROR;
        }
      }
      status = lonejson__array_stream_read_key(stream);
      if (status != LONEJSON_STATUS_OK) {
        return lonejson__array_stream_result_from_status(stream, status, error);
      }
      status = lonejson__array_stream_note_root_key(stream);
      if (status != LONEJSON_STATUS_OK) {
        return lonejson__array_stream_result_from_status(stream, status, error);
      }
      stream->root_object_after_comma = 0;
      stream->current_key_matched =
          stream->key.len == stream->path_len &&
          memcmp(stream->key.data, stream->path, stream->path_len) == 0;
      if (stream->current_key_matched) {
        stream->selected_seen = 1;
      }
      stream->phase = LONEJSON_ARRAY_STREAM_PHASE_ROOT_OBJECT_COLON;
      break;

    case LONEJSON_ARRAY_STREAM_PHASE_ROOT_OBJECT_COLON:
      status = lonejson__array_stream_getc_status(stream, &ch);
      if (status != LONEJSON_STATUS_OK) {
        return lonejson__array_stream_result_from_status(stream, status, error);
      }
      if (ch != ':') {
        (void)lonejson__array_stream_set_error(stream,
                                               LONEJSON_STATUS_INVALID_JSON,
                                               "expected ':' after object key");
        if (error != NULL) {
          *error = stream->error;
        }
        return LONEJSON_ARRAY_STREAM_ERROR;
      }
      stream->phase = LONEJSON_ARRAY_STREAM_PHASE_ROOT_OBJECT_VALUE;
      break;

    case LONEJSON_ARRAY_STREAM_PHASE_ROOT_OBJECT_VALUE:
      if (stream->current_key_matched) {
        status = lonejson__array_stream_getc_status(stream, &ch);
        if (status != LONEJSON_STATUS_OK) {
          return lonejson__array_stream_result_from_status(stream, status,
                                                           error);
        }
        if (ch != '[') {
          (void)lonejson__array_stream_set_error(
              stream, LONEJSON_STATUS_TYPE_MISMATCH,
              "selected JSON value is not an array");
          if (error != NULL) {
            *error = stream->error;
          }
          return LONEJSON_ARRAY_STREAM_ERROR;
        }
        stream->state = LONEJSON_ARRAY_STREAM_STATE_IN_ARRAY;
        stream->first_item = 1;
        stream->phase = LONEJSON_ARRAY_STREAM_PHASE_ARRAY_VALUE_OR_END;
        break;
      }
      status = lonejson__array_stream_step_value(
          stream, LONEJSON_ARRAY_STREAM_VALUE_SKIP, NULL, NULL, NULL, NULL);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return lonejson__array_stream_result_from_status(stream, status, error);
      }
      if (stream->would_block) {
        return lonejson__array_stream_result_from_status(
            stream, LONEJSON_STATUS_TRUNCATED, error);
      }
      stream->phase = LONEJSON_ARRAY_STREAM_PHASE_ROOT_OBJECT_COMMA_OR_END;
      break;

    case LONEJSON_ARRAY_STREAM_PHASE_ROOT_OBJECT_COMMA_OR_END:
      status = lonejson__array_stream_getc_status(stream, &ch);
      if (status != LONEJSON_STATUS_OK) {
        return lonejson__array_stream_result_from_status(stream, status, error);
      }
      if (ch == '}') {
        if (!stream->selected_seen) {
          (void)lonejson__array_stream_set_error(
              stream, LONEJSON_STATUS_MISSING_REQUIRED_FIELD,
              "selected array field was not found");
          if (error != NULL) {
            *error = stream->error;
          }
          return LONEJSON_ARRAY_STREAM_ERROR;
        }
        stream->phase = LONEJSON_ARRAY_STREAM_PHASE_FINISH_DOCUMENT;
        break;
      }
      if (ch != ',') {
        (void)lonejson__array_stream_set_error(stream,
                                               LONEJSON_STATUS_INVALID_JSON,
                                               "expected ',' or '}' in object");
        if (error != NULL) {
          *error = stream->error;
        }
        return LONEJSON_ARRAY_STREAM_ERROR;
      }
      stream->root_object_after_comma = 1;
      stream->phase = LONEJSON_ARRAY_STREAM_PHASE_ROOT_OBJECT_KEY_OR_END;
      break;

    case LONEJSON_ARRAY_STREAM_PHASE_ARRAY_VALUE_OR_END:
      if (stream->value_active) {
        status = lonejson__array_stream_step_value(stream, mode, map, dst,
                                                   stream->active_sink,
                                                   stream->active_sink_user);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return lonejson__array_stream_result_from_status(stream, status,
                                                           error);
        }
        if (stream->would_block) {
          return lonejson__array_stream_result_from_status(
              stream, LONEJSON_STATUS_TRUNCATED, error);
        }
        lonejson__array_stream_mark_pending_item(stream, mode, map, dst);
        stream->phase = LONEJSON_ARRAY_STREAM_PHASE_ITEM_AFTER_VALUE;
        break;
      }
      status = lonejson__array_stream_getc_status(stream, &ch);
      if (status != LONEJSON_STATUS_OK) {
        return lonejson__array_stream_result_from_status(stream, status, error);
      }
      if (ch == EOF) {
        (void)lonejson__array_stream_set_error(stream,
                                               LONEJSON_STATUS_INVALID_JSON,
                                               "unterminated selected array");
        if (error != NULL) {
          *error = stream->error;
        }
        return LONEJSON_ARRAY_STREAM_ERROR;
      }
      if (ch == ']') {
        if (!stream->first_item) {
          (void)lonejson__array_stream_set_error(
              stream, LONEJSON_STATUS_INVALID_JSON,
              "expected JSON value in selected array");
          if (error != NULL) {
            *error = stream->error;
          }
          return LONEJSON_ARRAY_STREAM_ERROR;
        }
        stream->phase =
            stream->root_array_selected
                ? LONEJSON_ARRAY_STREAM_PHASE_FINISH_DOCUMENT
                : LONEJSON_ARRAY_STREAM_PHASE_ROOT_OBJECT_COMMA_OR_END;
        break;
      }
      lonejson__array_stream_ungetc(stream, ch);
      status = lonejson__array_stream_step_value(stream, mode, map, dst, sink,
                                                 sink_user);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return lonejson__array_stream_result_from_status(stream, status, error);
      }
      if (stream->would_block) {
        return lonejson__array_stream_result_from_status(
            stream, LONEJSON_STATUS_TRUNCATED, error);
      }
      lonejson__array_stream_mark_pending_item(stream, mode, map, dst);
      stream->phase = LONEJSON_ARRAY_STREAM_PHASE_ITEM_AFTER_VALUE;
      break;

    case LONEJSON_ARRAY_STREAM_PHASE_ARRAY_COMMA_OR_END:
      status = lonejson__array_stream_getc_status(stream, &ch);
      if (status != LONEJSON_STATUS_OK) {
        return lonejson__array_stream_result_from_status(stream, status, error);
      }
      if (ch == ']') {
        stream->phase =
            stream->root_array_selected
                ? LONEJSON_ARRAY_STREAM_PHASE_FINISH_DOCUMENT
                : LONEJSON_ARRAY_STREAM_PHASE_ROOT_OBJECT_COMMA_OR_END;
        break;
      }
      if (ch != ',') {
        (void)lonejson__array_stream_set_error(
            stream, LONEJSON_STATUS_INVALID_JSON,
            "expected ',' or ']' in selected array");
        if (error != NULL) {
          *error = stream->error;
        }
        return LONEJSON_ARRAY_STREAM_ERROR;
      }
      stream->first_item = 0;
      stream->phase = LONEJSON_ARRAY_STREAM_PHASE_ARRAY_VALUE_OR_END;
      break;

    case LONEJSON_ARRAY_STREAM_PHASE_ITEM_AFTER_VALUE:
      status =
          lonejson__array_stream_check_pending_item(stream, mode, map, dst);
      if (status != LONEJSON_STATUS_OK) {
        return lonejson__array_stream_result_from_status(stream, status, error);
      }
      status = lonejson__array_stream_getc_status(stream, &ch);
      if (status != LONEJSON_STATUS_OK) {
        return lonejson__array_stream_result_from_status(stream, status, error);
      }
      if (ch == EOF) {
        (void)lonejson__array_stream_set_error(stream,
                                               LONEJSON_STATUS_INVALID_JSON,
                                               "unterminated selected array");
        if (error != NULL) {
          *error = stream->error;
        }
        return LONEJSON_ARRAY_STREAM_ERROR;
      }
      if (ch != ',' && ch != ']') {
        (void)lonejson__array_stream_set_error(
            stream, LONEJSON_STATUS_INVALID_JSON,
            "expected ',' or ']' in selected array");
        if (error != NULL) {
          *error = stream->error;
        }
        return LONEJSON_ARRAY_STREAM_ERROR;
      }
      lonejson__array_stream_ungetc(stream, ch);
      stream->first_item = 0;
      stream->phase = LONEJSON_ARRAY_STREAM_PHASE_ARRAY_COMMA_OR_END;
      lonejson__array_stream_clear_pending_item(stream);
      if (error != NULL) {
        *error = stream->error;
      }
      return LONEJSON_ARRAY_STREAM_ITEM;

    case LONEJSON_ARRAY_STREAM_PHASE_FINISH_DOCUMENT:
      status = lonejson__array_stream_getc_status(stream, &ch);
      if (status != LONEJSON_STATUS_OK) {
        return lonejson__array_stream_result_from_status(stream, status, error);
      }
      if (ch != EOF) {
        (void)lonejson__array_stream_set_error(
            stream, LONEJSON_STATUS_INVALID_JSON,
            stream->root_array_selected ? "unexpected data after root array"
                                        : "unexpected data after root object");
        if (error != NULL) {
          *error = stream->error;
        }
        return LONEJSON_ARRAY_STREAM_ERROR;
      }
      stream->state = LONEJSON_ARRAY_STREAM_STATE_DONE;
      if (error != NULL) {
        *error = stream->error;
      }
      return LONEJSON_ARRAY_STREAM_EOF;
    }
  }
}

lonejson_array_stream_result
lonejson_array_stream_next(lonejson_array_stream *stream,
                           const lonejson_map *map, void *dst,
                           lonejson_error *error) {
  lonejson_array_stream_result result;

  if (stream == NULL || map == NULL || dst == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "array stream, map, and destination are required");
    return LONEJSON_ARRAY_STREAM_ERROR;
  }
  result = lonejson__array_stream_next_raw(
      stream, LONEJSON_ARRAY_STREAM_VALUE_MAPPED, map, dst, NULL, NULL, error);
  if (result != LONEJSON_ARRAY_STREAM_ITEM) {
    return result;
  }
  if (error != NULL) {
    *error = stream->error;
  }
  return LONEJSON_ARRAY_STREAM_ITEM;
}

lonejson_array_stream_result
lonejson_array_stream_next_value(lonejson_array_stream *stream,
                                 lonejson_json_value *value,
                                 lonejson_error *error) {
  lonejson_array_stream_result result;
  lonejson_status status;

  if (value == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "JSON value destination is required");
    return LONEJSON_ARRAY_STREAM_ERROR;
  }
  result = lonejson__array_stream_next_raw(
      stream, LONEJSON_ARRAY_STREAM_VALUE_RAW, NULL, NULL, NULL, NULL, error);
  if (result != LONEJSON_ARRAY_STREAM_ITEM) {
    return result;
  }
  status = lonejson__array_stream_validate_duplicate_keys(
      stream, stream->item.data, stream->item.len);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    if (error != NULL) {
      *error = stream->error;
    }
    return LONEJSON_ARRAY_STREAM_ERROR;
  }
  status = lonejson_json_value_set_buffer(value, stream->item.data,
                                          stream->item.len, &stream->error);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    stream->state = LONEJSON_ARRAY_STREAM_STATE_ERROR;
    if (error != NULL) {
      *error = stream->error;
    }
    return LONEJSON_ARRAY_STREAM_ERROR;
  }
  if (error != NULL) {
    *error = stream->error;
  }
  return LONEJSON_ARRAY_STREAM_ITEM;
}

static lonejson_array_stream_result
lonejson__array_stream_next_sink(lonejson_array_stream *stream,
                                 lonejson_sink_fn sink, void *sink_user,
                                 lonejson_error *error) {
  lonejson_array_stream_result result;

  if (sink == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "array stream item sink is required");
    return LONEJSON_ARRAY_STREAM_ERROR;
  }
  result =
      lonejson__array_stream_next_raw(stream, LONEJSON_ARRAY_STREAM_VALUE_SINK,
                                      NULL, NULL, sink, sink_user, error);
  if (result != LONEJSON_ARRAY_STREAM_ITEM) {
    return result;
  }
  if (error != NULL) {
    *error = stream->error;
  }
  return LONEJSON_ARRAY_STREAM_ITEM;
}

static lonejson_status lonejson__array_stream_push_invoke(
    lonejson_array_stream *stream, lonejson_array_stream_item_fn callback,
    void *user, const lonejson_map *map, void *dst, lonejson_error *error) {
  lonejson_status status;

  status = callback(user, dst);
  lonejson_cleanup(map, dst);
  if (status != LONEJSON_STATUS_OK) {
    (void)lonejson__array_stream_set_error(stream, status,
                                           "array stream item callback failed");
    if (error != NULL) {
      *error = stream->error;
    }
    return status;
  }
  return LONEJSON_STATUS_OK;
}

static void
lonejson__array_stream_note_truncation(lonejson_array_stream *stream,
                                       lonejson_status *final_status,
                                       lonejson_error *final_error) {
  if (stream->error.truncated || stream->push_truncated) {
    stream->push_truncated = 1;
    *final_status = LONEJSON_STATUS_TRUNCATED;
    *final_error = stream->error;
    final_error->code = LONEJSON_STATUS_TRUNCATED;
    final_error->truncated = 1;
  }
}

static lonejson_status
lonejson__array_stream_return_error(lonejson_array_stream *stream,
                                    lonejson_error *error) {
  if (stream->error.code == LONEJSON_STATUS_OK) {
    (void)lonejson__set_error(&stream->error, LONEJSON_STATUS_INTERNAL_ERROR,
                              stream->offset, stream->line, stream->column,
                              "array stream failed without an error code");
  }
  if (error != NULL) {
    *error = stream->error;
  }
  return stream->error.code;
}

static lonejson_status
lonejson__array_stream_string_finish_item(lonejson_array_stream *stream,
                                          lonejson_error *error) {
  lonejson_status status = LONEJSON_STATUS_OK;

  if (!stream->string_seen) {
    return lonejson__array_stream_string_type_error(stream, error);
  }
  if (stream->string_handler != NULL && stream->string_handler->end != NULL) {
    status = stream->string_handler->end(stream->string_user, error);
  }
  stream->string_seen = 0;
  if (status != LONEJSON_STATUS_OK) {
    (void)lonejson__array_stream_set_error(
        stream, status, "array stream string callback failed");
    if (error != NULL) {
      *error = stream->error;
    }
  }
  return status;
}

static lonejson_status lonejson__array_stream_push_common(
    lonejson_array_stream *stream, const lonejson_map *map, void *dst,
    const void *bytes, size_t len, lonejson_array_stream_item_fn callback,
    void *user, int finish, lonejson_error *error) {
  const unsigned char *input = (const unsigned char *)bytes;
  size_t offset = 0u;
  lonejson_status final_status = LONEJSON_STATUS_OK;
  lonejson_error final_error;

  lonejson__clear_error(&final_error);

  if (stream == NULL || map == NULL || dst == NULL || callback == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "array stream, map, destination, and callback are "
                        "required");
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (stream->source_kind != LONEJSON_ARRAY_STREAM_SOURCE_PUSH) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "array stream is not push-fed");
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (len != 0u && input == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "input bytes are required");
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (stream->state == LONEJSON_ARRAY_STREAM_STATE_DONE) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "array stream is already finished");
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (stream->state == LONEJSON_ARRAY_STREAM_STATE_ERROR) {
    if (error != NULL) {
      *error = stream->error;
    }
    return stream->error.code;
  }

  while (offset < len) {
    size_t chunk_len = len - offset;
    if (chunk_len > sizeof(stream->io_buffer)) {
      chunk_len = sizeof(stream->io_buffer);
    }
    memcpy(stream->io_buffer, input + offset, chunk_len);
    stream->buffered_start = 0u;
    stream->buffered_end = chunk_len;
    stream->push_eof = 0;

    for (;;) {
      lonejson_array_stream_result result = lonejson__array_stream_next_raw(
          stream, LONEJSON_ARRAY_STREAM_VALUE_MAPPED, map, dst, NULL, NULL,
          error);
      if (result == LONEJSON_ARRAY_STREAM_ITEM) {
        lonejson_status status = lonejson__array_stream_push_invoke(
            stream, callback, user, map, dst, error);
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
        lonejson__array_stream_note_truncation(stream, &final_status,
                                               &final_error);
        continue;
      }
      if (result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK) {
        break;
      }
      if (result == LONEJSON_ARRAY_STREAM_ERROR) {
        return lonejson__array_stream_return_error(stream, error);
      }
      if (result == LONEJSON_ARRAY_STREAM_EOF) {
        if (stream->push_truncated && final_status == LONEJSON_STATUS_OK) {
          final_status = LONEJSON_STATUS_TRUNCATED;
          final_error = stream->error;
          final_error.code = LONEJSON_STATUS_TRUNCATED;
          final_error.truncated = 1;
        }
        if (final_status == LONEJSON_STATUS_TRUNCATED && error != NULL) {
          *error = final_error;
        }
        return final_status;
      }
    }
    offset += chunk_len;
  }

  if (finish) {
    stream->push_eof = 1;
    for (;;) {
      lonejson_array_stream_result result = lonejson__array_stream_next_raw(
          stream, LONEJSON_ARRAY_STREAM_VALUE_MAPPED, map, dst, NULL, NULL,
          error);
      if (result == LONEJSON_ARRAY_STREAM_ITEM) {
        lonejson_status status = lonejson__array_stream_push_invoke(
            stream, callback, user, map, dst, error);
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
        lonejson__array_stream_note_truncation(stream, &final_status,
                                               &final_error);
        continue;
      }
      if (result == LONEJSON_ARRAY_STREAM_EOF) {
        if (stream->push_truncated && final_status == LONEJSON_STATUS_OK) {
          final_status = LONEJSON_STATUS_TRUNCATED;
          final_error = stream->error;
          final_error.code = LONEJSON_STATUS_TRUNCATED;
          final_error.truncated = 1;
        }
        if (final_status == LONEJSON_STATUS_TRUNCATED && error != NULL) {
          *error = final_error;
        }
        return final_status;
      }
      if (result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK) {
        return lonejson__array_stream_set_error(
            stream, LONEJSON_STATUS_INTERNAL_ERROR,
            "push array stream blocked at EOF");
      }
      return lonejson__array_stream_return_error(stream, error);
    }
  }

  if (stream->push_truncated) {
    final_status = LONEJSON_STATUS_TRUNCATED;
    if (!final_error.truncated) {
      final_error = stream->error;
      final_error.code = LONEJSON_STATUS_TRUNCATED;
      final_error.truncated = 1;
    }
  }
  if (final_status == LONEJSON_STATUS_TRUNCATED) {
    if (error != NULL) {
      *error = final_error;
    }
    return final_status;
  }
  lonejson__clear_error(error);
  return final_status;
}

static lonejson_status lonejson__array_stream_push_string_common(
    lonejson_array_stream *stream, const void *bytes, size_t len,
    const lonejson_array_stream_string_handler *handler, void *user, int finish,
    lonejson_error *error) {
  const unsigned char *input = (const unsigned char *)bytes;
  size_t offset = 0u;

  if (stream == NULL || handler == NULL || handler->chunk == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "array stream and string chunk handler are required");
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (stream->source_kind != LONEJSON_ARRAY_STREAM_SOURCE_PUSH) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "array stream is not push-fed");
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (len != 0u && input == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "input bytes are required");
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (stream->state == LONEJSON_ARRAY_STREAM_STATE_DONE) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "array stream is already finished");
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (stream->state == LONEJSON_ARRAY_STREAM_STATE_ERROR) {
    if (error != NULL) {
      *error = stream->error;
    }
    return stream->error.code;
  }

  stream->string_handler = handler;
  stream->string_user = user;

  while (offset < len) {
    size_t chunk_len = len - offset;
    if (chunk_len > sizeof(stream->io_buffer)) {
      chunk_len = sizeof(stream->io_buffer);
    }
    memcpy(stream->io_buffer, input + offset, chunk_len);
    stream->buffered_start = 0u;
    stream->buffered_end = chunk_len;
    stream->push_eof = 0;

    for (;;) {
      lonejson_array_stream_result result = lonejson__array_stream_next_raw(
          stream, LONEJSON_ARRAY_STREAM_VALUE_STRING, NULL, NULL, NULL, NULL,
          error);
      if (result == LONEJSON_ARRAY_STREAM_ITEM) {
        lonejson_status status =
            lonejson__array_stream_string_finish_item(stream, error);
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
        continue;
      }
      if (result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK) {
        break;
      }
      if (result == LONEJSON_ARRAY_STREAM_ERROR) {
        return lonejson__array_stream_return_error(stream, error);
      }
      if (result == LONEJSON_ARRAY_STREAM_EOF) {
        return LONEJSON_STATUS_OK;
      }
    }
    offset += chunk_len;
  }

  if (finish) {
    stream->push_eof = 1;
    for (;;) {
      lonejson_array_stream_result result = lonejson__array_stream_next_raw(
          stream, LONEJSON_ARRAY_STREAM_VALUE_STRING, NULL, NULL, NULL, NULL,
          error);
      if (result == LONEJSON_ARRAY_STREAM_ITEM) {
        lonejson_status status =
            lonejson__array_stream_string_finish_item(stream, error);
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
        continue;
      }
      if (result == LONEJSON_ARRAY_STREAM_EOF) {
        return LONEJSON_STATUS_OK;
      }
      if (result == LONEJSON_ARRAY_STREAM_WOULD_BLOCK) {
        return lonejson__array_stream_set_error(
            stream, LONEJSON_STATUS_INTERNAL_ERROR,
            "push array stream blocked at EOF");
      }
      return lonejson__array_stream_return_error(stream, error);
    }
  }

  lonejson__clear_error(error);
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__array_stream_push_string_items_common(
    lonejson_array_stream *stream, const void *bytes, size_t len,
    lonejson_array_stream_string_fn callback, void *user, int finish,
    lonejson_error *error) {
  lonejson_array_stream_string_handler handler;
  lonejson_value_limits limits;

  if (stream == NULL || callback == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "array stream and string item callback are required");
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  limits = lonejson_default_value_limits();
  stream->string_item_max_bytes = limits.max_string_bytes;
  if (stream->string_item_max_bytes == 0u) {
    stream->string_item_max_bytes = SIZE_MAX - 1u;
  }
  stream->string_callback = callback;
  stream->string_callback_user = user;
  memset(&handler, 0, sizeof(handler));
  handler.begin = lonejson__array_stream_string_item_begin;
  handler.chunk = lonejson__array_stream_string_item_chunk;
  handler.end = lonejson__array_stream_string_item_end;
  return lonejson__array_stream_push_string_common(stream, bytes, len, &handler,
                                                   stream, finish, error);
}

lonejson_status lonejson_array_stream_push(
    lonejson_array_stream *stream, const lonejson_map *map, void *dst,
    const void *bytes, size_t len, lonejson_array_stream_item_fn callback,
    void *user, lonejson_error *error) {
  return lonejson__array_stream_push_common(stream, map, dst, bytes, len,
                                            callback, user, 0, error);
}

lonejson_status lonejson_array_stream_finish(
    lonejson_array_stream *stream, const lonejson_map *map, void *dst,
    lonejson_array_stream_item_fn callback, void *user, lonejson_error *error) {
  return lonejson__array_stream_push_common(stream, map, dst, NULL, 0u,
                                            callback, user, 1, error);
}

lonejson_status lonejson_array_stream_push_string(
    lonejson_array_stream *stream, const void *bytes, size_t len,
    const lonejson_array_stream_string_handler *handler, void *user,
    lonejson_error *error) {
  return lonejson__array_stream_push_string_common(stream, bytes, len, handler,
                                                   user, 0, error);
}

lonejson_status lonejson_array_stream_finish_string(
    lonejson_array_stream *stream,
    const lonejson_array_stream_string_handler *handler, void *user,
    lonejson_error *error) {
  return lonejson__array_stream_push_string_common(stream, NULL, 0u, handler,
                                                   user, 1, error);
}

lonejson_status lonejson_array_stream_push_string_items(
    lonejson_array_stream *stream, const void *bytes, size_t len,
    lonejson_array_stream_string_fn callback, void *user,
    lonejson_error *error) {
  return lonejson__array_stream_push_string_items_common(
      stream, bytes, len, callback, user, 0, error);
}

lonejson_status lonejson_array_stream_finish_string_items(
    lonejson_array_stream *stream, lonejson_array_stream_string_fn callback,
    void *user, lonejson_error *error) {
  return lonejson__array_stream_push_string_items_common(
      stream, NULL, 0u, callback, user, 1, error);
}

const lonejson_error *
lonejson_array_stream_error(const lonejson_array_stream *stream) {
  return stream ? &stream->error : NULL;
}

void lonejson_array_stream_close(lonejson_array_stream *stream) {
  if (stream == NULL) {
    return;
  }
  if (stream->owns_fp && stream->fp != NULL) {
    fclose(stream->fp);
  }
  if (stream->owns_fd && stream->fd >= 0) {
    close(stream->fd);
  }
  lonejson__array_stream_clear_value(stream);
  lonejson__byte_free(&stream->item, &stream->allocator);
  lonejson__byte_free(&stream->string_item, &stream->allocator);
  lonejson__byte_free(&stream->key, &stream->allocator);
  lonejson__byte_free(&stream->root_keys, &stream->allocator);
  lonejson__buffer_free(&stream->allocator, stream->path,
                        stream->path_alloc_size);
  lonejson__buffer_free(&stream->allocator, stream, stream->self_alloc_size);
}
