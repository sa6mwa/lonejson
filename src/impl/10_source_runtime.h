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
  lonejson_uint64 stream_offset;
  lonejson_uint64 pushback_offset;
  int count_pushback;
  int has_pushback;
  int pushback;
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
  const lonejson_path_value_visitor *path_visitor;
  void *visitor_user;
  lonejson_error *error;
  int pretty;
  size_t base_depth;
  size_t depth;
  size_t total_bytes;
  lonejson__value_limits limits;
  const lonejson_allocator *allocator;
  int has_pushback;
  int pushback_counted;
  int last_getc_counted;
  int pushback;
  lonejson_path_segment *path_segments;
  lonejson__json_path_frame *path_frames;
  size_t path_depth;
  size_t path_capacity;
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

int lonejson_source_is_rewindable(const lonejson_source *value) {
  if (value == NULL) {
    return 0;
  }
  switch (value->kind) {
  case LONEJSON_SOURCE_NONE:
  case LONEJSON_SOURCE_PATH:
    return 1;
  case LONEJSON_SOURCE_FILE: {
    long pos;
    if (value->fp == NULL) {
      return 0;
    }
    pos = ftell(value->fp);
    if (pos < 0L || fseek(value->fp, 0L, SEEK_SET) != 0) {
      return 0;
    }
    return fseek(value->fp, pos, SEEK_SET) == 0;
  }
  case LONEJSON_SOURCE_FD: {
    off_t pos;
    if (value->fd < 0) {
      return 0;
    }
    pos = lseek(value->fd, 0, SEEK_CUR);
    if (pos < 0 || lseek(value->fd, 0, SEEK_SET) < 0) {
      return 0;
    }
    return lseek(value->fd, pos, SEEK_SET) >= 0;
  }
  default:
    return 0;
  }
}
