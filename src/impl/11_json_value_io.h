static int lonejson__is_valid_json_number(const char *value, size_t len);
static int lonejson__is_json_space(int ch);
static int lonejson__is_digit(int ch);
static int lonejson__hex_value(int ch);
static LONEJSON__INLINE int lonejson__utf8_append(lonejson_scratch *scratch,
                                                  lonejson_uint32 cp);
static lonejson_status lonejson__emit_escaped_fragment(lonejson_sink_fn sink,
                                                       void *user,
                                                       lonejson_error *error,
                                                       const unsigned char *src,
                                                       size_t len);

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

static LONEJSON__INLINE lonejson_status
lonejson__json_emit(lonejson__json_io *io, const void *data, size_t len) {
  return io->sink(io->sink_user, data, len, io->error);
}

static LONEJSON__INLINE lonejson_status
lonejson__json_emit_cstr(lonejson__json_io *io, const char *text) {
  return lonejson__json_emit(io, text, strlen(text));
}

static LONEJSON__INLINE lonejson_status
lonejson__json_emit_indent(lonejson__json_io *io, size_t depth) {
  static const char prefix[] = "\n" LONEJSON__SPACES_64;
  size_t spaces;
  lonejson_status status;

  if (depth > (SIZE_MAX / 2u)) {
    return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "JSON indentation depth is too large");
  }
  spaces = depth * 2u;
  if (spaces <= 64u) {
    return lonejson__json_emit(io, prefix, spaces + 1u);
  }
  status = lonejson__json_emit(io, prefix, sizeof(prefix) - 1u);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  spaces -= 64u;
  while (spaces > 64u) {
    status = lonejson__json_emit(io, LONEJSON__SPACES_64, 64u);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    spaces -= 64u;
  }
  if (spaces != 0u) {
    status = lonejson__json_emit(io, LONEJSON__SPACES_64, spaces);
  }
  return status;
}

static lonejson_status
lonejson__json_cursor_open(const lonejson_json_value *value,
                           lonejson__json_cursor *cursor,
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

static LONEJSON__INLINE lonejson_status
lonejson__json_enforce_total_limit(lonejson__json_io *io) {
  if (io->limits.max_total_bytes != 0u &&
      io->total_bytes > io->limits.max_total_bytes) {
    return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "JSON value exceeds maximum total byte limit");
  }
  return LONEJSON_STATUS_OK;
}

static LONEJSON__INLINE LONEJSON__HOT int
lonejson__json_cursor_getc(lonejson__json_io *io) {
  unsigned char byte = 0u;
  lonejson_read_result result;
  ssize_t got;

  if (io->has_pushback) {
    io->has_pushback = 0;
    if (io->pushback_counted) {
      io->pushback_counted = 0;
      io->last_getc_counted = 1;
      io->total_bytes++;
      if (lonejson__json_enforce_total_limit(io) != LONEJSON_STATUS_OK) {
        return -2;
      }
    } else {
      io->last_getc_counted = 0;
    }
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
      result =
          io->cursor->reader(io->cursor->reader_user, io->cursor->read_buffer,
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
        io->cursor->stream_offset += result.bytes_read;
        byte = io->cursor->read_buffer[io->cursor->read_buffer_off++];
        goto counted;
      }
      if (result.eof) {
        return EOF;
      }
      if (result.would_block) {
        lonejson__set_error(io->error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u,
                            0u, "reader would block");
        return -2;
      }
      return EOF;
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
    io->cursor->stream_offset += (size_t)got;
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
  io->cursor->stream_offset += (size_t)got;
  byte = io->cursor->read_buffer[io->cursor->read_buffer_off++];
counted:
  io->last_getc_counted = 1;
  io->total_bytes++;
  if (lonejson__json_enforce_total_limit(io) != LONEJSON_STATUS_OK) {
    return -2;
  }
  return (int)byte;
}

static LONEJSON__INLINE int
lonejson__json_cursor_getc_lookahead(lonejson__json_io *io) {
  unsigned char byte = 0u;
  lonejson_read_result result;
  ssize_t got;

  if (io->has_pushback) {
    io->has_pushback = 0;
    if (io->pushback_counted) {
      io->pushback_counted = 0;
      io->last_getc_counted = 1;
      io->total_bytes++;
    } else {
      io->last_getc_counted = 0;
    }
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
      result =
          io->cursor->reader(io->cursor->reader_user, io->cursor->read_buffer,
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
        io->cursor->stream_offset += result.bytes_read;
        byte = io->cursor->read_buffer[io->cursor->read_buffer_off++];
        goto counted;
      }
      if (result.eof) {
        return EOF;
      }
      if (result.would_block) {
        lonejson__set_error(io->error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u,
                            0u, "reader would block");
        return -2;
      }
      return EOF;
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
    io->cursor->stream_offset += (size_t)got;
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
  io->cursor->stream_offset += (size_t)got;
  byte = io->cursor->read_buffer[io->cursor->read_buffer_off++];
counted:
  io->last_getc_counted = 1;
  io->total_bytes++;
  return (int)byte;
}

static LONEJSON__INLINE void lonejson__json_cursor_ungetc(lonejson__json_io *io,
                                                          int ch) {
  io->has_pushback = 1;
  io->pushback_counted = io->last_getc_counted;
  if (io->last_getc_counted && io->total_bytes != 0u) {
    --io->total_bytes;
  }
  io->last_getc_counted = 0;
  io->pushback = ch;
}

static LONEJSON__INLINE lonejson_status
lonejson__json_cursor_advance_span(lonejson__json_io *io, size_t len) {
  (void)io;
  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  return LONEJSON_STATUS_OK;
}

static LONEJSON__INLINE size_t
lonejson__json_cursor_next_offset(const lonejson__json_cursor *cursor) {
  if (cursor == NULL) {
    return 0u;
  }
  if (cursor->buffer != NULL) {
    return cursor->buffer_off;
  }
  if (cursor->stream_offset >=
      cursor->read_buffer_len - cursor->read_buffer_off) {
    return cursor->stream_offset - cursor->read_buffer_len +
           cursor->read_buffer_off;
  }
  return 0u;
}

static LONEJSON__INLINE size_t
lonejson__json_cursor_last_offset(const lonejson__json_cursor *cursor) {
  size_t next = lonejson__json_cursor_next_offset(cursor);
  return next == 0u ? 0u : next - 1u;
}

static LONEJSON__INLINE int
lonejson__json_peek_nonspace(lonejson__json_io *io) {
  int ch;

  do {
    ch = lonejson__json_cursor_getc(io);
  } while (ch >= 0 && lonejson__is_json_space(ch));
  if (ch >= 0) {
    lonejson__json_cursor_ungetc(io, ch);
  }
  return ch;
}
