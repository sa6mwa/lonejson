typedef struct lonejson__source_cursor {
  const lonejson_source *source;
  FILE *fp;
  int close_fp;
  int use_fd;
} lonejson__source_cursor;

typedef struct lonejson__json_cursor {
  const lonejson_json_value *value;
  const unsigned char *buffer;
  size_t buffer_len;
  size_t buffer_off;
  unsigned char read_buffer[4096];
  size_t read_buffer_len;
  size_t read_buffer_off;
  lonejson_reader_fn reader;
  void *reader_user;
  FILE *fp;
  int close_fp;
  int use_fd;
} lonejson__json_cursor;

typedef struct lonejson__json_io {
  lonejson__json_cursor *cursor;
  lonejson_sink_fn sink;
  void *sink_user;
  const lonejson_value_visitor *visitor;
  void *visitor_user;
  lonejson_error *error;
  int pretty;
  size_t base_depth;
  size_t depth;
  size_t total_bytes;
  lonejson_value_limits limits;
  const lonejson_allocator *allocator;
  int has_pushback;
  int pushback;
} lonejson__json_io;

static lonejson_status
lonejson__source_cursor_open(const lonejson_source *value,
                             lonejson__source_cursor *cursor,
                             lonejson_error *error) {
  memset(cursor, 0, sizeof(*cursor));
  cursor->source = value;
  if (value->kind == LONEJSON_SOURCE_NONE) {
    return LONEJSON_STATUS_OK;
  }
  if (value->kind == LONEJSON_SOURCE_PATH) {
    cursor->fp = fopen(value->path, "rb");
    if (cursor->fp == NULL) {
      if (error != NULL) {
        error->system_errno = errno;
      }
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to open source path '%s'",
                                 value->path);
    }
    cursor->close_fp = 1;
    return LONEJSON_STATUS_OK;
  }
  if (value->kind == LONEJSON_SOURCE_FILE) {
    if (fseek(value->fp, 0L, SEEK_SET) != 0) {
      if (error != NULL) {
        error->system_errno = errno;
      }
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to rewind source FILE*; non-seekable "
                                 "sources are unsupported");
    }
    clearerr(value->fp);
    cursor->fp = value->fp;
    return LONEJSON_STATUS_OK;
  }
  if (lseek(value->fd, 0, SEEK_SET) < 0) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    return lonejson__set_error(
        error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
        "failed to rewind source fd; non-seekable sources are unsupported");
  }
  cursor->use_fd = 1;
  return LONEJSON_STATUS_OK;
}

static void lonejson__source_cursor_close(lonejson__source_cursor *cursor) {
  if (cursor->close_fp && cursor->fp != NULL) {
    fclose(cursor->fp);
  }
  cursor->fp = NULL;
  cursor->close_fp = 0;
  cursor->use_fd = 0;
}

static lonejson_read_result
lonejson__source_cursor_read(lonejson__source_cursor *cursor,
                             unsigned char *buffer, size_t capacity) {
  lonejson_read_result result;

  memset(&result, 0, sizeof(result));
  if (cursor->source->kind == LONEJSON_SOURCE_NONE) {
    result.eof = 1;
    return result;
  }
  if (cursor->use_fd) {
    ssize_t got = read(cursor->source->fd, buffer, capacity);
    if (got < 0) {
      result.error_code = errno;
      return result;
    }
    result.bytes_read = (size_t)got;
    result.eof = (got == 0) ? 1 : 0;
    return result;
  }
  result.bytes_read = fread(buffer, 1u, capacity, cursor->fp);
  if (result.bytes_read < capacity) {
    if (ferror(cursor->fp)) {
      result.error_code = errno;
      return result;
    }
    result.eof = 1;
  }
  return result;
}

lonejson_status lonejson_source_write_to_sink(const lonejson_source *value,
                                              lonejson_sink_fn sink, void *user,
                                              lonejson_error *error) {
  unsigned char buffer[4096];
  lonejson__source_cursor cursor;
  lonejson_status status;
  lonejson_read_result chunk;

  if (value == NULL || sink == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "source handle and sink are required");
  }
  status = lonejson__source_cursor_open(value, &cursor, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  for (;;) {
    chunk = lonejson__source_cursor_read(&cursor, buffer, sizeof(buffer));
    if (chunk.error_code != 0) {
      if (error != NULL) {
        error->system_errno = chunk.error_code;
      }
      lonejson__source_cursor_close(&cursor);
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to read source data");
    }
    if (chunk.bytes_read != 0u) {
      status = sink(user, buffer, chunk.bytes_read, error);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        lonejson__source_cursor_close(&cursor);
        return status;
      }
    }
    if (chunk.eof) {
      break;
    }
  }
  lonejson__source_cursor_close(&cursor);
  return LONEJSON_STATUS_OK;
}

static int lonejson__is_valid_json_number(const char *value, size_t len);
static int lonejson__is_json_space(int ch);
static int lonejson__is_digit(int ch);
static int lonejson__hex_value(int ch);
static LONEJSON__INLINE int lonejson__utf8_append(lonejson_scratch *scratch,
                                                  lonejson_uint32 cp);
static lonejson_status lonejson__emit_escaped_fragment(
    lonejson_sink_fn sink, void *user, lonejson_error *error,
    const unsigned char *src, size_t len);

static lonejson_status lonejson__json_buffer_sink(void *user, const void *data,
                                                  size_t len,
                                                  lonejson_error *error) {
  lonejson_json_value *value = (lonejson_json_value *)user;
  size_t required;
  size_t capacity;
  size_t next_cap;
  char *next;

  (void)error;
  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  required = value->len + len + 1u;
  capacity = lonejson__owned_size(value->json);
  if (capacity < required) {
    next_cap = capacity != 0u ? capacity : 64u;
    while (next_cap < required) {
      size_t doubled = next_cap * 2u;
      if (doubled <= next_cap) {
        next_cap = required;
        break;
      }
      next_cap = doubled;
    }
    next = (char *)lonejson__owned_realloc(&value->allocator, value->json,
                                           next_cap);
    if (next == NULL) {
      return LONEJSON_STATUS_ALLOCATION_FAILED;
    }
    value->json = next;
  }
  memcpy(value->json + value->len, data, len);
  value->len += len;
  value->json[value->len] = '\0';
  return LONEJSON_STATUS_OK;
}

static LONEJSON__INLINE lonejson_status lonejson__json_emit(
    lonejson__json_io *io, const void *data, size_t len) {
  return io->sink(io->sink_user, data, len, io->error);
}

static LONEJSON__INLINE lonejson_status lonejson__json_emit_cstr(
    lonejson__json_io *io, const char *text) {
  return lonejson__json_emit(io, text, strlen(text));
}

static LONEJSON__INLINE lonejson_status lonejson__json_emit_indent(
    lonejson__json_io *io, size_t depth) {
  size_t i;
  lonejson_status status;

  status = lonejson__json_emit_cstr(io, "\n");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  for (i = 0u; i < depth * 2u; ++i) {
    status = lonejson__json_emit(io, " ", 1u);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__json_cursor_open(
    const lonejson_json_value *value, lonejson__json_cursor *cursor,
    lonejson_error *error) {
  memset(cursor, 0, sizeof(*cursor));
  cursor->value = value;
  if (value == NULL || value->kind == LONEJSON_JSON_VALUE_NULL) {
    return LONEJSON_STATUS_OK;
  }
  switch (value->kind) {
  case LONEJSON_JSON_VALUE_BUFFER:
    cursor->buffer = (const unsigned char *)value->json;
    cursor->buffer_len = value->len;
    return LONEJSON_STATUS_OK;
  case LONEJSON_JSON_VALUE_READER:
    cursor->reader = value->reader;
    cursor->reader_user = value->reader_user;
    return LONEJSON_STATUS_OK;
  case LONEJSON_JSON_VALUE_FILE:
    if (fseek(value->fp, 0L, SEEK_SET) != 0) {
      if (error != NULL) {
        error->system_errno = errno;
      }
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to rewind JSON value FILE*");
    }
    clearerr(value->fp);
    cursor->fp = value->fp;
    return LONEJSON_STATUS_OK;
  case LONEJSON_JSON_VALUE_FD:
    if (lseek(value->fd, 0, SEEK_SET) < 0) {
      if (error != NULL) {
        error->system_errno = errno;
      }
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to rewind JSON value fd");
    }
    cursor->use_fd = 1;
    return LONEJSON_STATUS_OK;
  case LONEJSON_JSON_VALUE_PATH:
    cursor->fp = fopen(value->path, "rb");
    if (cursor->fp == NULL) {
      if (error != NULL) {
        error->system_errno = errno;
      }
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to open JSON value path '%s'",
                                 value->path);
    }
    cursor->close_fp = 1;
    return LONEJSON_STATUS_OK;
  default:
    return lonejson__set_error(error, LONEJSON_STATUS_INTERNAL_ERROR, 0u, 0u,
                               0u, "unsupported JSON value kind");
  }
}

static void lonejson__json_cursor_close(lonejson__json_cursor *cursor) {
  if (cursor->close_fp && cursor->fp != NULL) {
    fclose(cursor->fp);
  }
  cursor->fp = NULL;
  cursor->close_fp = 0;
  cursor->use_fd = 0;
}

static LONEJSON__INLINE LONEJSON__HOT int
lonejson__json_cursor_getc(lonejson__json_io *io) {
  unsigned char byte = 0u;
  lonejson_read_result result;
  ssize_t got;

  if (io->has_pushback) {
    io->has_pushback = 0;
    return io->pushback;
  }
  if (io->cursor->buffer != NULL) {
    if (io->cursor->buffer_off >= io->cursor->buffer_len) {
      return EOF;
    }
    byte = io->cursor->buffer[io->cursor->buffer_off++];
    goto counted;
  }
  if (io->cursor->read_buffer_off < io->cursor->read_buffer_len) {
    byte = io->cursor->read_buffer[io->cursor->read_buffer_off++];
    goto counted;
  }
  if (io->cursor->reader != NULL) {
    for (;;) {
      result = io->cursor->reader(io->cursor->reader_user,
                                  io->cursor->read_buffer,
                                  sizeof(io->cursor->read_buffer));
      if (result.error_code != 0) {
        if (io->error != NULL) {
          io->error->system_errno = result.error_code;
        }
        lonejson__set_error(io->error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u,
                            0u, "reader callback failed");
        return -2;
      }
      if (result.bytes_read != 0u) {
        io->cursor->read_buffer_len = result.bytes_read;
        io->cursor->read_buffer_off = 0u;
        byte = io->cursor->read_buffer[io->cursor->read_buffer_off++];
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
  if (io->cursor->use_fd) {
    got = read(io->cursor->value->fd, io->cursor->read_buffer,
               sizeof(io->cursor->read_buffer));
    if (got < 0) {
      if (io->error != NULL) {
        io->error->system_errno = errno;
      }
      lonejson__set_error(io->error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                          "failed to read JSON value fd");
      return -2;
    }
    if (got == 0) {
      return EOF;
    }
    io->cursor->read_buffer_len = (size_t)got;
    io->cursor->read_buffer_off = 0u;
    byte = io->cursor->read_buffer[io->cursor->read_buffer_off++];
    goto counted;
  }
  got = (ssize_t)fread(io->cursor->read_buffer, 1u,
                       sizeof(io->cursor->read_buffer), io->cursor->fp);
  if (got <= 0) {
    if (ferror(io->cursor->fp)) {
      if (io->error != NULL) {
        io->error->system_errno = errno;
      }
      lonejson__set_error(io->error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                          "failed to read JSON value stream");
      return -2;
    }
    return EOF;
  }
  io->cursor->read_buffer_len = (size_t)got;
  io->cursor->read_buffer_off = 0u;
  byte = io->cursor->read_buffer[io->cursor->read_buffer_off++];
counted:
  io->total_bytes++;
  if (io->limits.max_total_bytes != 0u &&
      io->total_bytes > io->limits.max_total_bytes) {
    lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                        "JSON value exceeds maximum total byte limit");
    return -2;
  }
  return (int)byte;
}

static LONEJSON__INLINE void lonejson__json_cursor_ungetc(
    lonejson__json_io *io, int ch) {
  io->has_pushback = 1;
  io->pushback = ch;
}

static LONEJSON__INLINE int lonejson__json_peek_nonspace(
    lonejson__json_io *io) {
  int ch;

  do {
    ch = lonejson__json_cursor_getc(io);
  } while (ch >= 0 && lonejson__is_json_space(ch));
  if (ch >= 0) {
    lonejson__json_cursor_ungetc(io, ch);
  }
  return ch;
}

static LONEJSON__HOT lonejson_status lonejson__json_parse_value(
    lonejson__json_io *io);

static LONEJSON__HOT lonejson_status lonejson__json_parse_string(
    lonejson__json_io *io) {
  int ch;
  lonejson_status status;

  status = lonejson__json_emit(io, "\"", 1u);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  for (;;) {
    if (!io->has_pushback && io->cursor->buffer != NULL &&
        io->cursor->buffer_off < io->cursor->buffer_len) {
      const unsigned char *span = io->cursor->buffer + io->cursor->buffer_off;
      size_t available = io->cursor->buffer_len - io->cursor->buffer_off;
      size_t plain_span = 0u;

      while (plain_span < available) {
        unsigned char b = span[plain_span];
        if (b == '"' || b == '\\' || b < 0x20u) {
          break;
        }
        ++plain_span;
      }
      if (plain_span != 0u) {
        status = lonejson__json_emit(io, span, plain_span);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
        io->cursor->buffer_off += plain_span;
        continue;
      }
    } else if (!io->has_pushback &&
               io->cursor->read_buffer_off < io->cursor->read_buffer_len) {
      const unsigned char *span =
          io->cursor->read_buffer + io->cursor->read_buffer_off;
      size_t available = io->cursor->read_buffer_len - io->cursor->read_buffer_off;
      size_t plain_span = 0u;

      while (plain_span < available) {
        unsigned char b = span[plain_span];
        if (b == '"' || b == '\\' || b < 0x20u) {
          break;
        }
        ++plain_span;
      }
      if (plain_span != 0u) {
        status = lonejson__json_emit(io, span, plain_span);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
        io->cursor->read_buffer_off += plain_span;
        continue;
      }
    }
    ch = lonejson__json_cursor_getc(io);
    if (ch == -2) {
      return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
    }
    if (ch == EOF) {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "unterminated JSON string");
    }
    if (ch < 0x20) {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "control character in string");
    }
    status = lonejson__json_emit(io, &ch, 1u);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (ch == '"') {
      return LONEJSON_STATUS_OK;
    }
    if (ch == '\\') {
      int esc = lonejson__json_cursor_getc(io);
      if (esc == -2) {
        return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
      }
      if (esc == EOF) {
        return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                   0u, 0u, "unterminated escape sequence");
      }
      switch (esc) {
      case '"':
      case '\\':
      case '/':
      case 'b':
      case 'f':
      case 'n':
      case 'r':
      case 't':
        status = lonejson__json_emit(io, &esc, 1u);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
        break;
      case 'u': {
        int i;
        status = lonejson__json_emit(io, &esc, 1u);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
        for (i = 0; i < 4; ++i) {
          int hx = lonejson__json_cursor_getc(io);
          if (hx == -2) {
            return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
          }
          if (hx == EOF || lonejson__hex_value(hx) < 0) {
            return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON,
                                       0u, 0u, 0u,
                                       "invalid unicode escape");
          }
          status = lonejson__json_emit(io, &hx, 1u);
          if (status != LONEJSON_STATUS_OK &&
              status != LONEJSON_STATUS_TRUNCATED) {
            return status;
          }
        }
        break;
      }
      default:
        return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                   0u, 0u, "invalid string escape");
      }
    }
  }
}

static lonejson_status lonejson__json_parse_number(lonejson__json_io *io,
                                                   int first) {
  char number[128];
  size_t len = 0u;
  int ch = first;
  lonejson_status status;

  do {
    if (len >= sizeof(number)) {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "JSON number token too long");
    }
    number[len++] = (char)ch;
    ch = lonejson__json_cursor_getc(io);
  } while (ch >= 0 && (lonejson__is_digit(ch) || ch == '+' || ch == '-' ||
                       ch == '.' || ch == 'e' || ch == 'E'));
  if (ch >= 0) {
    lonejson__json_cursor_ungetc(io, ch);
  } else if (ch == -2) {
    return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
  }
  if (!lonejson__is_valid_json_number(number, len)) {
    return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u,
                               0u, "invalid JSON number");
  }
  status = lonejson__json_emit(io, number, len);
  return status;
}

static lonejson_status lonejson__json_parse_literal(lonejson__json_io *io,
                                                    int first,
                                                    const char *rest) {
  size_t i;
  lonejson_status status;

  status = lonejson__json_emit(io, &first, 1u);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  for (i = 0u; rest[i] != '\0'; ++i) {
    int ch = lonejson__json_cursor_getc(io);
    if (ch == -2) {
      return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
    }
    if (ch != (unsigned char)rest[i]) {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "invalid JSON literal");
    }
    status = lonejson__json_emit(io, &ch, 1u);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__json_parse_array(lonejson__json_io *io) {
  int ch;
  int first = 1;
  lonejson_status status;

  status = lonejson__json_emit(io, "[", 1u);
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
      if (io->pretty && !first) {
        status = lonejson__json_emit_indent(io, io->base_depth + io->depth);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      return lonejson__json_emit(io, "]", 1u);
    }
    if (!first) {
      ch = lonejson__json_cursor_getc(io);
      if (ch != ',') {
        return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                   0u, 0u, "expected ',' in array");
      }
      status = lonejson__json_emit(io, ",", 1u);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    if (io->pretty) {
      status = lonejson__json_emit_indent(io, io->base_depth + io->depth + 1u);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    ++io->depth;
    status = lonejson__json_parse_value(io);
    --io->depth;
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    first = 0;
  }
}

static lonejson_status lonejson__json_parse_object(lonejson__json_io *io) {
  int ch;
  int first = 1;
  lonejson_status status;

  status = lonejson__json_emit(io, "{", 1u);
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
      if (io->pretty && !first) {
        status = lonejson__json_emit_indent(io, io->base_depth + io->depth);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      return lonejson__json_emit(io, "}", 1u);
    }
    if (!first) {
      ch = lonejson__json_cursor_getc(io);
      if (ch != ',') {
        return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                   0u, 0u, "expected ',' in object");
      }
      status = lonejson__json_emit(io, ",", 1u);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    if (io->pretty) {
      status = lonejson__json_emit_indent(io, io->base_depth + io->depth + 1u);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    ch = lonejson__json_peek_nonspace(io);
    if (ch == -2) {
      return io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
    }
    if (ch == EOF) {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "unterminated JSON object");
    }
    ch = lonejson__json_cursor_getc(io);
    if (ch != '"') {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "expected object key");
    }
    status = lonejson__json_parse_string(io);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    ch = lonejson__json_peek_nonspace(io);
    if (ch != ':') {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "expected ':' after object key");
    }
    (void)lonejson__json_cursor_getc(io);
    status = io->pretty ? lonejson__json_emit(io, ": ", 2u)
                        : lonejson__json_emit(io, ":", 1u);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    ++io->depth;
    status = lonejson__json_parse_value(io);
    --io->depth;
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    first = 0;
  }
}

static LONEJSON__HOT lonejson_status lonejson__json_parse_value(
    lonejson__json_io *io) {
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
    return lonejson__json_parse_string(io);
  case '{':
    return lonejson__json_parse_object(io);
  case '[':
    return lonejson__json_parse_array(io);
  case 't':
    return lonejson__json_parse_literal(io, ch, "rue");
  case 'f':
    return lonejson__json_parse_literal(io, ch, "alse");
  case 'n':
    return lonejson__json_parse_literal(io, ch, "ull");
  default:
    if (ch == '-' || lonejson__is_digit(ch)) {
      return lonejson__json_parse_number(io, ch);
    }
    return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u,
                               0u, "expected JSON value");
  }
}

static lonejson_status lonejson__json_visit_event(
    lonejson__json_io *io, lonejson_value_event_fn fn) {
  if (fn == NULL) {
    return LONEJSON_STATUS_OK;
  }
  return fn(io->visitor_user, io->error);
}

static lonejson_status lonejson__json_visit_chunk(
    lonejson__json_io *io, lonejson_value_chunk_fn fn, const char *data,
    size_t len) {
  if (len == 0u || fn == NULL) {
    return LONEJSON_STATUS_OK;
  }
  return fn(io->visitor_user, data, len, io->error);
}

static lonejson_status lonejson__json_visit_bool(
    lonejson__json_io *io, int value) {
  if (io->visitor == NULL || io->visitor->boolean_value == NULL) {
    return LONEJSON_STATUS_OK;
  }
  return io->visitor->boolean_value(io->visitor_user, value, io->error);
}

static const unsigned char *lonejson__json_cursor_plain_span(
    lonejson__json_io *io, size_t *available, int *uses_read_buffer) {
  if (available != NULL) {
    *available = 0u;
  }
  if (uses_read_buffer != NULL) {
    *uses_read_buffer = 0;
  }
  if (io->has_pushback) {
    return NULL;
  }
  if (io->cursor->buffer != NULL && io->cursor->buffer_off < io->cursor->buffer_len) {
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

static lonejson_status lonejson__json_visit_plain_chunk(
    lonejson__json_io *io, int is_key, unsigned char *plain, size_t *plain_len,
    size_t *decoded_bytes, size_t limit, const unsigned char *data, size_t len) {
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
      status = lonejson__json_visit_chunk(io, fn, (const char *)plain, *plain_len);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
      *plain_len = 0u;
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__json_visit_string_value(
    lonejson__json_io *io, int is_key) {
  unsigned char plain[256];
  size_t plain_len = 0u;
  size_t decoded_bytes = 0u;
  int ch;
  lonejson_status status;
  size_t limit =
      is_key ? io->limits.max_key_bytes : io->limits.max_string_bytes;

  status = lonejson__json_visit_event(
      io, is_key ? io->visitor->object_key_begin : io->visitor->string_begin);
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
        status = lonejson__json_visit_plain_chunk(
            io, is_key, plain, &plain_len, &decoded_bytes, limit, span, plain_span);
        if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
        if (uses_read_buffer) {
          io->cursor->read_buffer_off += plain_span;
        } else {
          io->cursor->buffer_off += plain_span;
        }
        if (plain_span == available) {
          span = lonejson__json_cursor_plain_span(io, &available, &uses_read_buffer);
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
      return lonejson__json_visit_event(
          io, is_key ? io->visitor->object_key_end : io->visitor->string_end);
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
      status = lonejson__json_visit_chunk(
          io, is_key ? io->visitor->object_key_chunk : io->visitor->string_chunk,
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
      status = lonejson__json_visit_chunk(
          io, is_key ? io->visitor->object_key_chunk : io->visitor->string_chunk,
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
      char out = (ch == 'b') ? '\b'
                 : (ch == 'f') ? '\f'
                 : (ch == 'n') ? '\n'
                 : (ch == 'r') ? '\r'
                               : '\t';
      decoded_bytes++;
      if (limit != 0u && decoded_bytes > limit) {
        return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u,
                                   0u, "decoded JSON text exceeds limit");
      }
      status = lonejson__json_visit_chunk(
          io, is_key ? io->visitor->object_key_chunk : io->visitor->string_chunk,
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
      status = lonejson__json_visit_chunk(
          io, is_key ? io->visitor->object_key_chunk : io->visitor->string_chunk,
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
    return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u,
                               0u, "JSON number exceeds maximum byte limit");
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
                                 0u,
                                 "JSON number exceeds maximum byte limit");
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
  status = lonejson__json_visit_chunk(io, io->visitor->number_chunk, buffer, len);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    status = lonejson__json_visit_event(io, io->visitor->number_end);
  }
  if (buffer != stack_buf) {
    lonejson__owned_free(buffer);
  }
  return status;
}

static lonejson_status lonejson__json_visit_literal(lonejson__json_io *io,
                                                    int first,
                                                    const char *rest,
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

static LONEJSON__HOT lonejson_status lonejson__json_visit_value(
    lonejson__json_io *io);

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
    ch = lonejson__json_cursor_getc(io);
    if (ch != '"') {
      return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "expected object key");
    }
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

static LONEJSON__HOT lonejson_status lonejson__json_visit_value(
    lonejson__json_io *io) {
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

static lonejson_status lonejson__json_visit(
    const lonejson_json_value *value, const lonejson_value_visitor *visitor,
    void *user, const lonejson_value_limits *limits, lonejson_error *error) {
  lonejson__json_cursor cursor;
  lonejson__json_io io;
  lonejson_status status;
  int ch;

  if (value == NULL || visitor == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JSON value source and visitor are required");
  }
  status = lonejson__json_cursor_open(value, &cursor, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  memset(&io, 0, sizeof(io));
  io.cursor = &cursor;
  io.visitor = visitor;
  io.visitor_user = user;
  io.error = error;
  io.allocator = &value->allocator;
  io.limits =
      limits ? *limits : lonejson_default_value_limits();
  if (io.limits.max_depth == 0u) {
    io.limits.max_depth = lonejson_default_value_limits().max_depth;
  }
  if (io.limits.max_string_bytes == 0u) {
    io.limits.max_string_bytes =
        lonejson_default_value_limits().max_string_bytes;
  }
  if (io.limits.max_number_bytes == 0u) {
    io.limits.max_number_bytes =
        lonejson_default_value_limits().max_number_bytes;
  }
  if (io.limits.max_key_bytes == 0u) {
    io.limits.max_key_bytes = lonejson_default_value_limits().max_key_bytes;
  }
  status = lonejson__json_visit_value(&io);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    ch = lonejson__json_peek_nonspace(&io);
    if (ch != EOF) {
      status = lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u,
                                   0u,
                                   "visited JSON must contain exactly one value");
    }
  }
  lonejson__json_cursor_close(&cursor);
  return status;
}

static lonejson_status lonejson__json_transcode(
    const lonejson_json_value *value, lonejson_sink_fn sink, void *user,
    lonejson_error *error, int pretty, size_t base_depth) {
  lonejson__json_cursor cursor;
  lonejson__json_io io;
  lonejson_status status;
  int ch;

  if (value == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JSON value handle is required");
  }
  if (value->kind == LONEJSON_JSON_VALUE_NULL) {
    return sink(user, "null", 4u, error);
  }
  status = lonejson__json_cursor_open(value, &cursor, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  memset(&io, 0, sizeof(io));
  io.cursor = &cursor;
  io.sink = sink;
  io.sink_user = user;
  io.error = error;
  io.pretty = pretty;
  io.base_depth = base_depth;
  io.allocator = &value->allocator;
  status = lonejson__json_parse_value(&io);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    ch = lonejson__json_peek_nonspace(&io);
    if (ch != EOF) {
      status =
          lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, 0u, 0u, 0u,
                              "embedded JSON must contain exactly one value");
    }
  }
  lonejson__json_cursor_close(&cursor);
  return status;
}

lonejson_status lonejson_json_value_set_buffer(lonejson_json_value *value,
                                               const void *data, size_t len,
                                               lonejson_error *error) {
  lonejson_json_value input;
  lonejson_json_value compact;
  lonejson_status status;

  if (value == NULL || (data == NULL && len != 0u)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JSON value handle and buffer are required");
  }
  lonejson_json_value_init(&input);
  lonejson_json_value_init_with_allocator(&compact, &value->allocator);
  input.kind = LONEJSON_JSON_VALUE_BUFFER;
  input.json = (char *)data;
  input.len = len;
  status = lonejson__json_transcode(&input, lonejson__json_buffer_sink,
                                    &compact, error, 0, 0u);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    lonejson_json_value_cleanup(&compact);
    return status;
  }
  lonejson_json_value_cleanup(value);
  *value = compact;
  value->kind = LONEJSON_JSON_VALUE_BUFFER;
  lonejson__clear_error(error);
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_json_value_set_parse_sink(lonejson_json_value *value,
                                                   lonejson_sink_fn sink,
                                                   void *user,
                                                   lonejson_error *error) {
  if (value == NULL || sink == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "JSON value handle and parse sink are required");
  }
  lonejson__json_value_clear_runtime(value);
  value->parse_mode = LONEJSON_JSON_VALUE_PARSE_SINK;
  value->parse_sink = sink;
  value->parse_sink_user = user;
  value->parse_visitor = NULL;
  value->parse_visitor_user = NULL;
  value->parse_visitor_limits = lonejson_default_value_limits();
  lonejson__clear_error(error);
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_json_value_set_parse_visitor(
    lonejson_json_value *value, const lonejson_value_visitor *visitor,
    void *user, const lonejson_value_limits *limits, lonejson_error *error) {
  lonejson_value_limits effective;

  if (value == NULL || visitor == NULL) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "JSON value handle and parse visitor are required");
  }
  effective = limits ? *limits : lonejson_default_value_limits();
  if (effective.max_depth == 0u) {
    effective.max_depth = lonejson_default_value_limits().max_depth;
  }
  if (effective.max_string_bytes == 0u) {
    effective.max_string_bytes = lonejson_default_value_limits().max_string_bytes;
  }
  if (effective.max_number_bytes == 0u) {
    effective.max_number_bytes = lonejson_default_value_limits().max_number_bytes;
  }
  if (effective.max_key_bytes == 0u) {
    effective.max_key_bytes = lonejson_default_value_limits().max_key_bytes;
  }
  lonejson__json_value_clear_runtime(value);
  lonejson__json_value_apply_allocator(value, &value->allocator);
  value->parse_mode = LONEJSON_JSON_VALUE_PARSE_VISITOR;
  value->parse_sink = NULL;
  value->parse_sink_user = NULL;
  value->parse_visitor = visitor;
  value->parse_visitor_user = user;
  value->parse_visitor_limits = effective;
  lonejson__clear_error(error);
  return LONEJSON_STATUS_OK;
}

lonejson_status
lonejson_json_value_enable_parse_capture(lonejson_json_value *value,
                                         lonejson_error *error) {
  if (value == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JSON value handle is required");
  }
  lonejson__json_value_clear_runtime(value);
  lonejson__json_value_apply_allocator(value, &value->allocator);
  value->parse_mode = LONEJSON_JSON_VALUE_PARSE_CAPTURE;
  value->parse_visitor = NULL;
  value->parse_visitor_user = NULL;
  value->parse_visitor_limits = lonejson_default_value_limits();
  lonejson__clear_error(error);
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_json_value_set_reader(lonejson_json_value *value,
                                               lonejson_reader_fn reader,
                                               void *user,
                                               lonejson_error *error) {
  if (value == NULL || reader == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "JSON value handle and reader are required");
  }
  lonejson_json_value_cleanup(value);
  value->kind = LONEJSON_JSON_VALUE_READER;
  value->reader = reader;
  value->reader_user = user;
  lonejson__clear_error(error);
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_json_value_set_file(lonejson_json_value *value,
                                             FILE *fp,
                                             lonejson_error *error) {
  if (value == NULL || fp == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JSON value handle and FILE* are required");
  }
  lonejson_json_value_cleanup(value);
  value->kind = LONEJSON_JSON_VALUE_FILE;
  value->fp = fp;
  lonejson__clear_error(error);
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_json_value_set_fd(lonejson_json_value *value, int fd,
                                           lonejson_error *error) {
  if (value == NULL || fd < 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "JSON value handle and non-negative fd are "
                               "required");
  }
  lonejson_json_value_cleanup(value);
  value->kind = LONEJSON_JSON_VALUE_FD;
  value->fd = fd;
  lonejson__clear_error(error);
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_json_value_set_path(lonejson_json_value *value,
                                             const char *path,
                                             lonejson_error *error) {
  size_t len;
  char *copy;

  if (value == NULL || path == NULL || path[0] == '\0') {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JSON value handle and path are required");
  }
  len = strlen(path);
  copy = (char *)lonejson__owned_malloc(&value->allocator, len + 1u);
  if (copy == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u, "failed to duplicate JSON value path");
  }
  memcpy(copy, path, len + 1u);
  lonejson_json_value_cleanup(value);
  value->kind = LONEJSON_JSON_VALUE_PATH;
  value->path = copy;
  lonejson__clear_error(error);
  return LONEJSON_STATUS_OK;
}
