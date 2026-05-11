
static LONEJSON__HOT lonejson_status
lonejson__json_parse_value(lonejson__json_io *io);

static LONEJSON__HOT lonejson_status
lonejson__json_parse_string(lonejson__json_io *io) {
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
      size_t available =
          io->cursor->read_buffer_len - io->cursor->read_buffer_off;
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
            return io->error ? io->error->code
                             : LONEJSON_STATUS_CALLBACK_FAILED;
          }
          if (hx == EOF || lonejson__hex_value(hx) < 0) {
            return lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON,
                                       0u, 0u, 0u, "invalid unicode escape");
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
  char stack_number[128];
  char *number = stack_number;
  size_t capacity = sizeof(stack_number);
  size_t len = 0u;
  int ch = first;
  lonejson_status status;

  do {
    if (len >= capacity) {
      size_t next_capacity = capacity * 2u;
      char *next;

      if (next_capacity <= capacity) {
        if (number != stack_number) {
          lonejson__owned_free(number);
        }
        return lonejson__set_error(io->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u,
                                   0u, "JSON number token too long");
      }
      if (number == stack_number) {
        next = (char *)lonejson__owned_malloc(io->allocator, next_capacity);
        if (next != NULL) {
          memcpy(next, number, len);
        }
      } else {
        next = (char *)lonejson__owned_realloc(io->allocator, number,
                                               next_capacity);
      }
      if (next == NULL) {
        if (number != stack_number) {
          lonejson__owned_free(number);
        }
        return lonejson__set_error(io->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                                   0u, 0u, 0u,
                                   "failed to allocate JSON number buffer");
      }
      number = next;
      capacity = next_capacity;
    }
    number[len++] = (char)ch;
    ch = lonejson__json_cursor_getc(io);
  } while (ch >= 0 && (lonejson__is_digit(ch) || ch == '+' || ch == '-' ||
                       ch == '.' || ch == 'e' || ch == 'E'));
  if (ch >= 0) {
    lonejson__json_cursor_ungetc(io, ch);
  } else if (ch == -2) {
    status = io->error ? io->error->code : LONEJSON_STATUS_CALLBACK_FAILED;
    if (number != stack_number) {
      lonejson__owned_free(number);
    }
    return status;
  }
  if (!lonejson__is_valid_json_number(number, len)) {
    status = lonejson__set_error(io->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                                 0u, 0u, "invalid JSON number");
    if (number != stack_number) {
      lonejson__owned_free(number);
    }
    return status;
  }
  status = lonejson__json_emit(io, number, len);
  if (number != stack_number) {
    lonejson__owned_free(number);
  }
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

static LONEJSON__HOT lonejson_status
lonejson__json_parse_value(lonejson__json_io *io) {
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
