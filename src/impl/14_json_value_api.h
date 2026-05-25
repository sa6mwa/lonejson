
static int lonejson__value_limits_equal(const lonejson__value_limits *a,
                                        const lonejson__value_limits *b) {
  if (a == NULL || b == NULL) {
    return a == b;
  }
  return a->max_depth == b->max_depth &&
         a->max_string_bytes == b->max_string_bytes &&
         a->max_number_bytes == b->max_number_bytes &&
         a->max_key_bytes == b->max_key_bytes &&
         a->max_total_bytes == b->max_total_bytes;
}

static void lonejson__json_value_note_limits_mode_change(
    lonejson_json_value *value) {
  if (value == NULL) {
    return;
  }
  value->parse_limits_follow_runtime =
      lonejson__value_limits_equal(&value->parse_visitor_limits,
                                   &value->runtime_parse_visitor_limits);
}

static lonejson_status lonejson__visit_value_buffer_with_limits(
    const void *data, size_t len, const lonejson_value_visitor *visitor,
    void *user, const lonejson__value_limits *limits,
    const lonejson_allocator *allocator, lonejson_error *error);

int lonejson_json_value_is_rewindable(const lonejson_json_value *value) {
  if (value == NULL) {
    return 0;
  }
  switch (value->kind) {
  case LONEJSON_JSON_VALUE_NULL:
  case LONEJSON_JSON_VALUE_BUFFER:
  case LONEJSON_JSON_VALUE_PATH:
    return 1;
  case LONEJSON_JSON_VALUE_READER:
    return 0;
  case LONEJSON_JSON_VALUE_FILE: {
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
  case LONEJSON_JSON_VALUE_FD: {
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

lonejson_status lonejson_json_value_set_buffer(lonejson_json_value *value,
                                               const void *data, size_t len,
                                               lonejson_error *error) {
  lonejson_json_value input;
  lonejson_json_value compact;
  lonejson__value_limits limits;
  lonejson__value_limits runtime_limits;
  int follow_runtime;
  lonejson_status status;

  if (value == NULL || (data == NULL && len != 0u)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JSON value handle and buffer are required");
  }
  limits = value->parse_visitor_limits;
  runtime_limits = value->runtime_parse_visitor_limits;
  follow_runtime = value->parse_limits_follow_runtime;
  lonejson_json_value_init_with_allocator(&input, NULL);
  lonejson_json_value_init_with_allocator(&compact, &value->allocator);
  input.kind = LONEJSON_JSON_VALUE_BUFFER;
  memcpy(&input.json, &data, sizeof(input.json));
  input.len = len;
  {
    lonejson__json_cursor cursor;
    status = lonejson__json_cursor_open(&input, &cursor, error);
    if (status == LONEJSON_STATUS_OK) {
      status = lonejson__json_transcode_cursor(&cursor, &value->allocator,
                                               lonejson__json_buffer_sink,
                                               &compact, error, 0, 0u);
      lonejson__json_cursor_close(&cursor);
    }
  }
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    lonejson_json_value_cleanup(&compact);
    return status;
  }
  lonejson_json_value_cleanup(value);
  *value = compact;
  value->kind = LONEJSON_JSON_VALUE_BUFFER;
  value->parse_visitor_limits = limits;
  value->runtime_parse_visitor_limits = runtime_limits;
  value->parse_limits_follow_runtime = follow_runtime;
  value->_lonejson_magic =
      lonejson__init_cookie(value, LONEJSON__JSON_VALUE_MAGIC);
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
  lonejson__json_value_note_limits_mode_change(value);
  lonejson__clear_error(error);
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_json_value_set_parse_visitor(
    lonejson_json_value *value, const lonejson_value_visitor *visitor,
    void *user, lonejson_error *error) {
  if (value == NULL || visitor == NULL) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "JSON value handle and parse visitor are required");
  }
  lonejson__json_value_clear_runtime(value);
  lonejson__json_value_apply_allocator(value, &value->allocator);
  value->parse_mode = LONEJSON_JSON_VALUE_PARSE_VISITOR;
  value->parse_sink = NULL;
  value->parse_sink_user = NULL;
  value->parse_visitor = visitor;
  value->parse_visitor_user = user;
  lonejson__json_value_note_limits_mode_change(value);
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
  lonejson__json_value_note_limits_mode_change(value);
  lonejson__clear_error(error);
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_json_value_set_reader(lonejson_json_value *value,
                                               lonejson_reader_fn reader,
                                               void *user,
                                               lonejson_error *error) {
  if (value == NULL || reader == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JSON value handle and reader are required");
  }
  lonejson_json_value_cleanup(value);
  value->kind = LONEJSON_JSON_VALUE_READER;
  value->reader = reader;
  value->reader_user = user;
  lonejson__clear_error(error);
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_json_value_set_file(lonejson_json_value *value,
                                             FILE *fp, lonejson_error *error) {
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
