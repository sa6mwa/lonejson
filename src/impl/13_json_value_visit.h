
static lonejson_status lonejson__json_visit_event(lonejson__json_io *io,
                                                  lonejson_value_event_fn fn) {
  if (fn == NULL) {
    return LONEJSON_STATUS_OK;
  }
  return fn(io->visitor_user, io->error);
}

static lonejson_status lonejson__json_visit_chunk(lonejson__json_io *io,
                                                  lonejson_value_chunk_fn fn,
                                                  const char *data,
                                                  size_t len) {
  if (len == 0u || fn == NULL) {
    return LONEJSON_STATUS_OK;
  }
  return fn(io->visitor_user, data, len, io->error);
}

static lonejson_status lonejson__json_visit_bool(lonejson__json_io *io,
                                                 int value) {
  if (io->visitor == NULL || io->visitor->boolean_value == NULL) {
    return LONEJSON_STATUS_OK;
  }
  return io->visitor->boolean_value(io->visitor_user, value, io->error);
}

static const unsigned char *
lonejson__json_cursor_plain_span(lonejson__json_io *io, size_t *available,
                                 int *uses_read_buffer) {
  if (available != NULL) {
    *available = 0u;
  }
  if (uses_read_buffer != NULL) {
    *uses_read_buffer = 0;
  }
  if (io->has_pushback) {
    return NULL;
  }
  if (io->cursor->buffer != NULL &&
      io->cursor->buffer_off < io->cursor->buffer_len) {
    if (available != NULL) {
      *available = io->cursor->buffer_len - io->cursor->buffer_off;
    }
    return io->cursor->buffer + io->cursor->buffer_off;
  }
  if (io->cursor->read_buffer_off < io->cursor->read_buffer_len) {
    if (available != NULL) {
      *available = io->cursor->read_buffer_len - io->cursor->read_buffer_off;
    }
    if (uses_read_buffer != NULL) {
      *uses_read_buffer = 1;
    }
    return io->cursor->read_buffer + io->cursor->read_buffer_off;
  }
  return NULL;
}

static lonejson_status
lonejson__json_visit_plain_chunk(lonejson__json_io *io, int is_key,
                                 unsigned char *plain, size_t *plain_len,
                                 size_t *decoded_bytes, size_t limit,
                                 const unsigned char *data, size_t len) {
  lonejson_status status;
  size_t take;
  size_t offset = 0u;
  lonejson_value_chunk_fn fn =
      is_key ? io->visitor->object_key_chunk : io->visitor->string_chunk;

  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  if (io->limits.max_total_bytes != 0u &&
      io->total_bytes + len > io->limits.max_total_bytes) {
    return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "JSON value exceeds maximum total byte limit");
  }
  io->total_bytes += len;
  *decoded_bytes += len;
  if (limit != 0u && *decoded_bytes > limit) {
    return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               is_key ? "JSON object key exceeds maximum "
                                        "decoded byte limit"
                                      : "JSON string exceeds maximum decoded "
                                        "byte limit");
  }
  if (*plain_len == 0u) {
    return lonejson__json_visit_chunk(io, fn, (const char *)data, len);
  }
  while (offset < len) {
    take = len - offset;
    if (take > 256u) {
      take = 256u;
    }
    if (take > (256u - *plain_len)) {
      take = 256u - *plain_len;
    }
    memcpy(plain + *plain_len, data + offset, take);
    *plain_len += take;
    offset += take;
    if (*plain_len == sizeof(unsigned char[256])) {
      status =
          lonejson__json_visit_chunk(io, fn, (const char *)plain, *plain_len);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
      *plain_len = 0u;
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__json_visit_string_value(lonejson__json_io *io,
                                                         int is_key) {
  unsigned char plain[256];
  size_t plain_len = 0u;
  size_t decoded_bytes = 0u;
  int ch;
  lonejson_status status;
  size_t limit =
      is_key ? io->limits.max_key_bytes : io->limits.max_string_bytes;

  status = lonejson__json_visit_event(io, is_key ? io->visitor->object_key_begin
                                                 : io->visitor->string_begin);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  for (;;) {
    const unsigned char *span;
    size_t available;
    size_t plain_span = 0u;
    int uses_read_buffer = 0;

    span = lonejson__json_cursor_plain_span(io, &available, &uses_read_buffer);
    while (span != NULL && available != 0u) {
      while (plain_span < available) {
        unsigned char b = span[plain_span];
        if (b == '"' || b == '\\' || b < 0x20u) {
          break;
        }
        ++plain_span;
      }
      if (plain_span != 0u) {
        status = lonejson__json_visit_plain_chunk(io, is_key, plain, &plain_len,
                                                  &decoded_bytes, limit, span,
                                                  plain_span);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
        if (uses_read_buffer) {
          io->cursor->read_buffer_off += plain_span;
        } else {
          io->cursor->buffer_off += plain_span;
        }
        if (plain_span == available) {
          span = lonejson__json_cursor_plain_span(io, &available,
                                                  &uses_read_buffer);
          plain_span = 0u;
          continue;
        }
      }
      break;
    }
    ch = lonejson__json_cursor_getc(io);
    if (ch == -2) {
      return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
    }
    if (ch == EOF) {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "unterminated JSON string");
    }
    if (ch == '"') {
      if (plain_len != 0u) {
        status = lonejson__json_visit_chunk(
            io,
            is_key ? io->visitor->object_key_chunk : io->visitor->string_chunk,
            (const char *)plain, plain_len);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      return lonejson__json_visit_event(io, is_key ? io->visitor->object_key_end
                                                   : io->visitor->string_end);
    }
    if (ch < 0x20) {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "control character in string");
    }
    if (ch != '\\') {
      plain[plain_len++] = (unsigned char)ch;
      decoded_bytes++;
      if (limit != 0u && decoded_bytes > limit) {
        return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u,
                                   0u,
                                   is_key ? "JSON object key exceeds maximum "
                                            "decoded byte limit"
                                          : "JSON string exceeds maximum "
                                            "decoded byte limit");
      }
      if (plain_len == sizeof(plain)) {
        status = lonejson__json_visit_chunk(
            io,
            is_key ? io->visitor->object_key_chunk : io->visitor->string_chunk,
            (const char *)plain, plain_len);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
        plain_len = 0u;
      }
      continue;
    }

    if (plain_len != 0u) {
      status = lonejson__json_visit_chunk(io,
                                          is_key ? io->visitor->object_key_chunk
                                                 : io->visitor->string_chunk,
                                          (const char *)plain, plain_len);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
      plain_len = 0u;
    }
    ch = lonejson__json_cursor_getc(io);
    if (ch == -2) {
      return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
    }
    if (ch == EOF) {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "unterminated escape sequence");
    }
    switch (ch) {
    case '"':
    case '\\':
    case '/': {
      char out = (char)ch;
      decoded_bytes++;
      if (limit != 0u && decoded_bytes > limit) {
        return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u,
                                   0u, "decoded JSON text exceeds limit");
      }
      status = lonejson__json_visit_chunk(io,
                                          is_key ? io->visitor->object_key_chunk
                                                 : io->visitor->string_chunk,
                                          &out, 1u);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
      break;
    }
    case 'b':
    case 'f':
    case 'n':
    case 'r':
    case 't': {
      char out = (ch == 'b')   ? '\b'
                 : (ch == 'f') ? '\f'
                 : (ch == 'n') ? '\n'
                 : (ch == 'r') ? '\r'
                               : '\t';
      decoded_bytes++;
      if (limit != 0u && decoded_bytes > limit) {
        return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u,
                                   0u, "decoded JSON text exceeds limit");
      }
      status = lonejson__json_visit_chunk(io,
                                          is_key ? io->visitor->object_key_chunk
                                                 : io->visitor->string_chunk,
                                          &out, 1u);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
      break;
    }
    case 'u': {
      int i;
      lonejson_uint32 cp = 0u;
      /* lonejson__scratch_reserve keeps one byte of slack beyond the explicit
       * payload length, so a 4-byte UTF-8 sequence needs 6 bytes of storage
       * here rather than 5.
       */
      unsigned char utf8[6];
      lonejson_scratch scratch;
      for (i = 0; i < 4; ++i) {
        int hx = lonejson__json_cursor_getc(io);
        int hv;
        if (hx == -2) {
          return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
        }
        if (hx == EOF) {
          return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON,
                                     0u, 0u, 0u, "invalid unicode escape");
        }
        hv = lonejson__hex_value(hx);
        if (hv < 0) {
          return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON,
                                     0u, 0u, 0u, "invalid unicode escape");
        }
        cp = (cp << 4u) | (lonejson_uint32)hv;
      }
      if (cp >= 0xD800u && cp <= 0xDBFFu) {
        lonejson_uint32 low = 0u;
        int hx;
        if (lonejson__json_cursor_getc(io) != '\\' ||
            lonejson__json_cursor_getc(io) != 'u') {
          return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON,
                                     0u, 0u, 0u,
                                     "invalid unicode surrogate pair");
        }
        for (i = 0; i < 4; ++i) {
          int hv;
          hx = lonejson__json_cursor_getc(io);
          if (hx == -2) {
            return io->error ? io->error->code
                             : LONEJSON_STATUS_CALLBACK_FAILED;
          }
          if (hx == EOF) {
            return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON,
                                       0u, 0u, 0u,
                                       "invalid unicode surrogate pair");
          }
          hv = lonejson__hex_value(hx);
          if (hv < 0) {
            return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON,
                                       0u, 0u, 0u,
                                       "invalid unicode surrogate pair");
          }
          low = (low << 4u) | (lonejson_uint32)hv;
        }
        if (low < 0xDC00u || low > 0xDFFFu) {
          return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON,
                                     0u, 0u, 0u,
                                     "invalid unicode surrogate pair");
        }
        cp = 0x10000u + (((cp - 0xD800u) << 10u) | (low - 0xDC00u));
      } else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
        return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                   0u, 0u, "unexpected low surrogate");
      }
      memset(&scratch, 0, sizeof(scratch));
      scratch.data = (char *)utf8;
      scratch.cap = sizeof(utf8);
      if (!lonejson__utf8_append(&scratch, cp)) {
        return lonejson__set_error(io->error, LONEJSON_STATUS_INTERNAL_ERROR,
                                   0u, 0u, 0u,
                                   "failed to encode unicode escape");
      }
      decoded_bytes += scratch.len;
      if (limit != 0u && decoded_bytes > limit) {
        return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u,
                                   0u, "decoded JSON text exceeds limit");
      }
      status = lonejson__json_visit_chunk(io,
                                          is_key ? io->visitor->object_key_chunk
                                                 : io->visitor->string_chunk,
                                          (const char *)utf8, scratch.len);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
      break;
    }
    default:
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "invalid string escape");
    }
  }
}

static lonejson_status lonejson__json_visit_number(lonejson__json_io *io,
                                                   int first) {
  char stack_buf[256];
  char *buffer = stack_buf;
  size_t capacity = io->limits.max_number_bytes + 1u;
  size_t len = 0u;
  int ch = first;
  lonejson_status status;

  if (capacity > sizeof(stack_buf)) {
    buffer = (char *)lonejson__owned_malloc(io->allocator, capacity);
    if (buffer == NULL) {
      return lonejson__set_error(io->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                                 0u, 0u, 0u,
                                 "failed to allocate JSON number buffer");
    }
  }
  status = lonejson__json_visit_event(io, io->visitor->number_begin);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    if (buffer != stack_buf) {
      lonejson__owned_free(buffer);
    }
    return status;
  }
  if (len >= io->limits.max_number_bytes) {
    if (buffer != stack_buf) {
      lonejson__owned_free(buffer);
    }
    return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "JSON number exceeds maximum byte limit");
  }
  buffer[len++] = (char)first;
  ch = lonejson__json_cursor_getc(io);
  while (ch >= 0 && (lonejson__is_digit(ch) || ch == '+' || ch == '-' ||
                     ch == '.' || ch == 'e' || ch == 'E')) {
    if (len >= io->limits.max_number_bytes) {
      if (buffer != stack_buf) {
        lonejson__owned_free(buffer);
      }
      return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u,
                                 0u, "JSON number exceeds maximum byte limit");
    }
    buffer[len++] = (char)ch;
    ch = lonejson__json_cursor_getc(io);
  }
  if (ch >= 0) {
    lonejson__json_cursor_ungetc(io, ch);
  }
  if (ch == -2) {
    if (buffer != stack_buf) {
      lonejson__owned_free(buffer);
    }
    return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
  }
  if (!lonejson__is_valid_json_number(buffer, len)) {
    if (buffer != stack_buf) {
      lonejson__owned_free(buffer);
    }
    return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u,
                               0u, "invalid JSON number");
  }
  status =
      lonejson__json_visit_chunk(io, io->visitor->number_chunk, buffer, len);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    status = lonejson__json_visit_event(io, io->visitor->number_end);
  }
  if (buffer != stack_buf) {
    lonejson__owned_free(buffer);
  }
  return status;
}

static lonejson_status lonejson__json_visit_literal(lonejson__json_io *io,
                                                    int first, const char *rest,
                                                    int kind) {
  size_t i;
  for (i = 0u; rest[i] != '\0'; ++i) {
    int ch = lonejson__json_cursor_getc(io);
    if (ch == -2) {
      return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
    }
    if (ch != (unsigned char)rest[i]) {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "invalid JSON literal");
    }
  }
  if (kind == 1) {
    return lonejson__json_visit_bool(io, first == 't');
  }
  return lonejson__json_visit_event(io, io->visitor->null_value);
}

static LONEJSON__HOT lonejson_status
lonejson__json_visit_value(lonejson__json_io *io);

static lonejson_status lonejson__json_visit_array(lonejson__json_io *io) {
  int ch;
  int first = 1;
  lonejson_status status;

  if (io->depth >= io->limits.max_depth) {
    return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "JSON value nesting exceeds maximum depth");
  }
  status = lonejson__json_visit_event(io, io->visitor->array_begin);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  for (;;) {
    ch = lonejson__json_peek_nonspace(io);
    if (ch == -2) {
      return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
    }
    if (ch == EOF) {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "unterminated JSON array");
    }
    if (ch == ']') {
      (void)lonejson__json_cursor_getc(io);
      return lonejson__json_visit_event(io, io->visitor->array_end);
    }
    if (!first) {
      ch = lonejson__json_cursor_getc(io);
      if (ch != ',') {
        return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                   0u, 0u, "expected ',' in array");
      }
    }
    ++io->depth;
    status = lonejson__json_visit_value(io);
    --io->depth;
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    first = 0;
  }
}

static lonejson_status lonejson__json_visit_object(lonejson__json_io *io) {
  int ch;
  int first = 1;
  lonejson_status status;

  if (io->depth >= io->limits.max_depth) {
    return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "JSON value nesting exceeds maximum depth");
  }
  status = lonejson__json_visit_event(io, io->visitor->object_begin);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  for (;;) {
    ch = lonejson__json_peek_nonspace(io);
    if (ch == -2) {
      return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
    }
    if (ch == EOF) {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "unterminated JSON object");
    }
    if (ch == '}') {
      (void)lonejson__json_cursor_getc(io);
      return lonejson__json_visit_event(io, io->visitor->object_end);
    }
    if (!first) {
      ch = lonejson__json_cursor_getc(io);
      if (ch != ',') {
        return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                   0u, 0u, "expected ',' in object");
      }
    }
    ch = lonejson__json_peek_nonspace(io);
    if (ch != '"') {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "expected object key");
    }
    (void)lonejson__json_cursor_getc(io);
    status = lonejson__json_visit_string_value(io, 1);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    ch = lonejson__json_peek_nonspace(io);
    if (ch != ':') {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "expected ':' after object key");
    }
    (void)lonejson__json_cursor_getc(io);
    ++io->depth;
    status = lonejson__json_visit_value(io);
    --io->depth;
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    first = 0;
  }
}

static LONEJSON__HOT lonejson_status
lonejson__json_visit_value(lonejson__json_io *io) {
  int ch = lonejson__json_peek_nonspace(io);

  if (ch == -2) {
    return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
  }
  if (ch == EOF) {
    return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u,
                               0u, "expected JSON value");
  }
  ch = lonejson__json_cursor_getc(io);
  switch (ch) {
  case '"':
    return lonejson__json_visit_string_value(io, 0);
  case '{':
    return lonejson__json_visit_object(io);
  case '[':
    return lonejson__json_visit_array(io);
  case 't':
    return lonejson__json_visit_literal(io, ch, "rue", 1);
  case 'f':
    return lonejson__json_visit_literal(io, ch, "alse", 1);
  case 'n':
    return lonejson__json_visit_literal(io, ch, "ull", 0);
  default:
    if (ch == '-' || lonejson__is_digit(ch)) {
      return lonejson__json_visit_number(io, ch);
    }
    return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u,
                               0u, "expected JSON value");
  }
}

static lonejson_status lonejson__json_visit_cursor(
    lonejson__json_cursor *cursor, const lonejson_allocator *allocator,
    const lonejson_value_visitor *visitor, void *user,
    const lonejson__value_limits *limits, lonejson_error *error) {
  lonejson__json_io io;
  lonejson__value_limits defaults;
  lonejson_status status;
  int ch;

  if (cursor == NULL || visitor == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "JSON value source and visitor are required");
  }
  memset(&io, 0, sizeof(io));
  io.cursor = cursor;
  io.visitor = visitor;
  io.visitor_user = user;
  io.error = error;
  io.allocator = allocator;
  if (limits != NULL && limits->max_depth != 0u &&
      limits->max_string_bytes != 0u &&
      limits->max_number_bytes != 0u &&
      limits->max_key_bytes != 0u) {
    io.limits = *limits;
  } else {
    defaults = lonejson__default_value_limits();
    io.limits = limits ? *limits : defaults;
    if (io.limits.max_depth == 0u) {
      io.limits.max_depth = defaults.max_depth;
    }
    if (io.limits.max_string_bytes == 0u) {
      io.limits.max_string_bytes = defaults.max_string_bytes;
    }
    if (io.limits.max_number_bytes == 0u) {
      io.limits.max_number_bytes = defaults.max_number_bytes;
    }
    if (io.limits.max_key_bytes == 0u) {
      io.limits.max_key_bytes = defaults.max_key_bytes;
    }
  }
  status = lonejson__json_visit_value(&io);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    ch = lonejson__json_peek_nonspace(&io);
    if (ch != EOF) {
      status =
          lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                              "visited JSON must contain exactly one value");
    }
  }
  return status;
}

static lonejson_status lonejson__json_visit(
    const lonejson_json_value *value, const lonejson_value_visitor *visitor,
    void *user, const lonejson__value_limits *limits, lonejson_error *error) {
  lonejson__json_cursor cursor;
  lonejson_status status;

  if (value == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "JSON value source and visitor are required");
  }
  status = lonejson__json_cursor_open(value, &cursor, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__json_visit_cursor(&cursor, &value->allocator, visitor,
                                       user, limits, error);
  lonejson__json_cursor_close(&cursor);
  return status;
}

static lonejson_status lonejson__json_transcode_cursor(
    lonejson__json_cursor *cursor, const lonejson_allocator *allocator,
    lonejson_sink_fn sink, void *user, lonejson_error *error, int pretty,
    size_t base_depth) {
  lonejson__json_io io;
  lonejson_status status;
  int ch;

  if (cursor == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JSON value handle is required");
  }
  memset(&io, 0, sizeof(io));
  io.cursor = cursor;
  io.sink = sink;
  io.sink_user = user;
  io.error = error;
  io.pretty = pretty;
  io.base_depth = base_depth;
  io.allocator = allocator;
  status = lonejson__json_parse_value(&io);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    ch = lonejson__json_peek_nonspace(&io);
    if (ch != EOF) {
      status =
          lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                              "embedded JSON must contain exactly one value");
    }
  }
  return status;
}

static lonejson_status
lonejson__json_transcode(const lonejson_json_value *value,
                         lonejson_sink_fn sink, void *user,
                         lonejson_error *error, int pretty, size_t base_depth) {
  lonejson__json_cursor cursor;
  lonejson_status status;

  if (value == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JSON value handle is required");
  }
  if (value->kind == LONEJSON_JSON_VALUE_NULL) {
    return sink(user, "null", 4u, error);
  }
  if (!pretty && value->kind == LONEJSON_JSON_VALUE_BUFFER) {
    return sink(user, value->json, value->len, error);
  }
  status = lonejson__json_cursor_open(value, &cursor, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__json_transcode_cursor(&cursor, &value->allocator, sink,
                                           user, error, pretty, base_depth);
  lonejson__json_cursor_close(&cursor);
  return status;
}
