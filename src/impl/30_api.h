lonejson_parse_options lonejson_default_parse_options(void) {
  lonejson_parse_options options;
  options.clear_destination = 1;
  options.reject_duplicate_keys = 1;
  options.max_depth = 64u;
  options.allocator = NULL;
  return options;
}

lonejson_value_limits lonejson_default_value_limits(void) {
  lonejson_value_limits limits;
  limits.max_depth = 64u;
  limits.max_string_bytes = 1024u * 1024u;
  limits.max_number_bytes = 256u;
  limits.max_key_bytes = 64u * 1024u;
  limits.max_total_bytes = 0u;
  return limits;
}

lonejson_write_options lonejson_default_write_options(void) {
  lonejson_write_options options;
  options.overflow_policy = LONEJSON_OVERFLOW_FAIL;
  options.pretty = 0;
  options.allocator = NULL;
  options.max_output_bytes = LONEJSON_WRITE_MAX_OUTPUT_BYTES;
  return options;
}

static size_t
lonejson__write_max_output_bytes(const lonejson_write_options *options) {
  if (options != NULL && options->max_output_bytes != 0u) {
    return options->max_output_bytes;
  }
  return LONEJSON_WRITE_MAX_OUTPUT_BYTES;
}

lonejson_owned_buffer lonejson_default_owned_buffer(void) {
  lonejson_owned_buffer buffer;
  buffer.data = NULL;
  buffer.len = 0u;
  buffer.alloc_size = 0u;
  buffer.allocator = lonejson_default_allocator();
  return buffer;
}

static int lonejson__string_array_stream_is_initialized(
    const lonejson_string_array_stream *stream) {
  return stream != NULL &&
         stream->_lonejson_magic ==
             lonejson__init_cookie(stream, LONEJSON__STRING_ARRAY_STREAM_MAGIC);
}

static int lonejson__mapped_array_stream_is_initialized(
    const lonejson_mapped_array_stream *stream) {
  return stream != NULL &&
         stream->_lonejson_magic ==
             lonejson__init_cookie(stream, LONEJSON__MAPPED_ARRAY_STREAM_MAGIC);
}

void lonejson_string_array_stream_init(lonejson_string_array_stream *stream) {
  if (stream == NULL) {
    return;
  }
  memset(stream, 0, sizeof(*stream));
  stream->_lonejson_magic =
      lonejson__init_cookie(stream, LONEJSON__STRING_ARRAY_STREAM_MAGIC);
}

lonejson_status lonejson_string_array_stream_set_handler(
    lonejson_string_array_stream *stream,
    const lonejson_array_stream_string_handler *handler, void *user,
    lonejson_error *error) {
  if (stream == NULL || handler == NULL || handler->chunk == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "string array stream and chunk handler are "
                               "required");
  }
  if (!lonejson__string_array_stream_is_initialized(stream)) {
    lonejson_string_array_stream_init(stream);
  }
  stream->handler = *handler;
  stream->user = user;
  stream->active = 0;
  lonejson__clear_error(error);
  return LONEJSON_STATUS_OK;
}

void lonejson_mapped_array_stream_init(lonejson_mapped_array_stream *stream) {
  if (stream == NULL) {
    return;
  }
  memset(stream, 0, sizeof(*stream));
  stream->_lonejson_magic =
      lonejson__init_cookie(stream, LONEJSON__MAPPED_ARRAY_STREAM_MAGIC);
}

lonejson_status lonejson_mapped_array_stream_set_handler(
    lonejson_mapped_array_stream *stream,
    const lonejson_mapped_array_stream_handler *handler,
    lonejson_error *error) {
  if (stream == NULL || handler == NULL || handler->item_map == NULL ||
      handler->item_dst == NULL || handler->item == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "mapped array stream, item map, destination, "
                               "and callback are required");
  }
  if (!lonejson__map_layout_is_valid(handler->item_map)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "mapped array stream item map is invalid");
  }
  if (!lonejson__mapped_array_stream_is_initialized(stream)) {
    lonejson_mapped_array_stream_init(stream);
  }
  stream->handler = *handler;
  stream->active = 0;
  lonejson__clear_error(error);
  return LONEJSON_STATUS_OK;
}

void lonejson_owned_buffer_init(lonejson_owned_buffer *buffer) {
  if (buffer == NULL) {
    return;
  }
  *buffer = lonejson_default_owned_buffer();
}

const char *lonejson_status_string(lonejson_status status) {
  switch (status) {
  case LONEJSON_STATUS_OK:
    return "ok";
  case LONEJSON_STATUS_INVALID_ARGUMENT:
    return "invalid_argument";
  case LONEJSON_STATUS_INVALID_JSON:
    return "invalid_json";
  case LONEJSON_STATUS_TYPE_MISMATCH:
    return "type_mismatch";
  case LONEJSON_STATUS_MISSING_REQUIRED_FIELD:
    return "missing_required_field";
  case LONEJSON_STATUS_DUPLICATE_FIELD:
    return "duplicate_field";
  case LONEJSON_STATUS_OVERFLOW:
    return "overflow";
  case LONEJSON_STATUS_TRUNCATED:
    return "truncated";
  case LONEJSON_STATUS_ALLOCATION_FAILED:
    return "allocation_failed";
  case LONEJSON_STATUS_CALLBACK_FAILED:
    return "callback_failed";
  case LONEJSON_STATUS_IO_ERROR:
    return "io_error";
  case LONEJSON_STATUS_INTERNAL_ERROR:
    return "internal_error";
  default:
    return "unknown";
  }
}

#ifdef LONEJSON_WITH_CURL
static lonejson_parser *
lonejson__parser_create_ex(const lonejson_map *map, void *dst,
                           const lonejson_parse_options *options,
                           lonejson_error *error, int validate_only) {
  lonejson_parser *parser;
  unsigned char *workspace;

  if ((!validate_only && (map == NULL || dst == NULL)) ||
      (validate_only && (map != NULL || dst != NULL))) {
    lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 1u, 0u,
        validate_only ? "validation parser does not accept mapping arguments"
                      : "map and destination are required");
    return NULL;
  }
  if (options != NULL &&
      !LONEJSON__ALLOCATOR_IS_VALID_CONFIG(options->allocator)) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "allocator must provide either all callbacks or none");
    return NULL;
  }
  parser = (lonejson_parser *)lonejson__buffer_alloc(
      options ? options->allocator : NULL,
      sizeof(*parser) + LONEJSON_PUSH_PARSER_BUFFER_SIZE +
          LONEJSON__PARSER_WORKSPACE_SLACK);
  if (parser == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 1u, 0u,
                        "failed to allocate parser");
    return NULL;
  }
  workspace = ((unsigned char *)parser) + sizeof(*parser);
  lonejson__parser_init_state(
      parser, map, dst, options, validate_only, workspace,
      LONEJSON_PUSH_PARSER_BUFFER_SIZE + LONEJSON__PARSER_WORKSPACE_SLACK);
  parser->self_alloc_size = sizeof(*parser) + LONEJSON_PUSH_PARSER_BUFFER_SIZE +
                            LONEJSON__PARSER_WORKSPACE_SLACK;
  parser->owns_self = 1;
  if (!validate_only && parser->options.clear_destination) {
    lonejson__init_map_with_allocator(map, dst, &parser->allocator);
  }
  lonejson__clear_error(error);
  return parser;
}
#endif

static lonejson_status lonejson_parser_feed(lonejson_parser *parser,
                                            const void *data, size_t len) {
  size_t consumed;

  return lonejson__parser_feed_bytes(parser, (const unsigned char *)data, len,
                                     &consumed, 0);
}

static lonejson_status lonejson_parser_finish(lonejson_parser *parser) {
  if (parser == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  if (parser->failed) {
    return parser->error.code;
  }
  if (parser->lex_mode == LONEJSON_LEX_NUMBER) {
    parser->lex_mode = LONEJSON_LEX_NONE;
    if (lonejson__deliver_token(parser, LONEJSON_LEX_NUMBER) !=
        LONEJSON_STATUS_OK) {
      return parser->error.code;
    }
  }
  if (parser->lex_mode != LONEJSON_LEX_NONE) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "incomplete JSON token at end of input");
  }
  if (!parser->root_started || !parser->root_finished ||
      parser->frame_count != 0u) {
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_INVALID_JSON, parser->error.offset,
        parser->error.line, parser->error.column, "incomplete JSON document");
  }
  return parser->error.truncated ? LONEJSON_STATUS_TRUNCATED
                                 : LONEJSON_STATUS_OK;
}

static void lonejson_parser_destroy(lonejson_parser *parser) {
  if (parser == NULL) {
    return;
  }
  lonejson__parser_unwind_active_mapped_array_streams(parser);
  while (parser->frame_count != 0u) {
    lonejson__pop_frame(parser);
  }
  if (parser->owns_self) {
    lonejson__buffer_free(&parser->allocator, parser, parser->self_alloc_size);
  }
}

lonejson_status lonejson_parse_buffer(const lonejson_map *map, void *dst,
                                      const void *data, size_t len,
                                      const lonejson_parse_options *options,
                                      lonejson_error *error) {
  lonejson_parser *parser;
  lonejson_status status;
  lonejson_parser parser_storage;
  unsigned char parser_workspace[LONEJSON_PARSER_BUFFER_SIZE +
                                 LONEJSON__PARSER_WORKSPACE_SLACK];

  if (map == NULL || dst == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "map and destination are required");
  }
  if (data == NULL && len != 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "buffer is required");
  }
  if (options != NULL &&
      !LONEJSON__ALLOCATOR_IS_VALID_CONFIG(options->allocator)) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "allocator must provide either all callbacks or none");
  }
  parser = &parser_storage;
  lonejson__parser_init_state(parser, map, dst, options, 0, parser_workspace,
                              sizeof(parser_workspace));
  if (parser->options.clear_destination) {
    lonejson__init_map_with_allocator(map, dst, &parser->allocator);
  }
  lonejson__clear_error(error);
  status = lonejson_parser_feed(parser, data, len);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    status = lonejson_parser_finish(parser);
  }
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    lonejson__parser_unwind_active_mapped_array_streams(parser);
  }
  if (error != NULL) {
    *error = parser->error;
  }
  return status;
}

lonejson_status lonejson_parse_cstr(const lonejson_map *map, void *dst,
                                    const char *json,
                                    const lonejson_parse_options *options,
                                    lonejson_error *error) {
  if (json == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "json string is required");
  }
  return lonejson_parse_buffer(map, dst, json, strlen(json), options, error);
}

static lonejson_read_result
lonejson__file_reader(void *user, unsigned char *buffer, size_t capacity) {
  FILE *fp = (FILE *)user;
  lonejson_read_result result;

  memset(&result, 0, sizeof(result));
  result.bytes_read = fread(buffer, 1u, capacity, fp);
  result.eof = feof(fp) ? 1 : 0;
  result.error_code = ferror(fp) ? errno : 0;
  return result;
}

lonejson_status lonejson_parse_reader(const lonejson_map *map, void *dst,
                                      lonejson_reader_fn reader, void *user,
                                      const lonejson_parse_options *options,
                                      lonejson_error *error) {
  lonejson_parser *parser;
  unsigned char buffer[LONEJSON_READER_BUFFER_SIZE];
  lonejson_status status = LONEJSON_STATUS_OK;
  lonejson_parser parser_storage;
  unsigned char parser_workspace[LONEJSON_PARSER_BUFFER_SIZE +
                                 LONEJSON__PARSER_WORKSPACE_SLACK];

  if (reader == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "reader callback is required");
  }
  if (map == NULL || dst == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "map and destination are required");
  }
  if (options != NULL &&
      !LONEJSON__ALLOCATOR_IS_VALID_CONFIG(options->allocator)) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "allocator must provide either all callbacks or none");
  }
  parser = &parser_storage;
  lonejson__parser_init_state(parser, map, dst, options, 0, parser_workspace,
                              sizeof(parser_workspace));
  if (parser->options.clear_destination) {
    lonejson__init_map_with_allocator(map, dst, &parser->allocator);
  }
  lonejson__clear_error(error);
  for (;;) {
    lonejson_read_result chunk = reader(user, buffer, sizeof(buffer));
    if (chunk.error_code != 0) {
      parser->error.system_errno = chunk.error_code;
      status = lonejson__set_error(
          &parser->error, LONEJSON_STATUS_IO_ERROR, parser->error.offset,
          parser->error.line, parser->error.column, "reader callback failed");
      break;
    }
    if (chunk.bytes_read != 0u) {
      status = lonejson_parser_feed(parser, buffer, chunk.bytes_read);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        break;
      }
    }
    if (chunk.eof) {
      status = lonejson_parser_finish(parser);
      break;
    }
  }
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    lonejson__parser_unwind_active_mapped_array_streams(parser);
  }
  if (error != NULL) {
    *error = parser->error;
  }
  return status;
}

lonejson_status lonejson_parse_filep(const lonejson_map *map, void *dst,
                                     FILE *fp,
                                     const lonejson_parse_options *options,
                                     lonejson_error *error) {
  if (fp == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "file pointer is required");
  }
  return lonejson_parse_reader(map, dst, lonejson__file_reader, fp, options,
                               error);
}

lonejson_status lonejson_parse_path(const lonejson_map *map, void *dst,
                                    const char *path,
                                    const lonejson_parse_options *options,
                                    lonejson_error *error) {
  FILE *fp;
  lonejson_status status;

  if (path == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "path is required");
  }
  fp = fopen(path, "rb");
  if (fp == NULL) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                               "failed to open '%s'", path);
  }
  status = lonejson_parse_filep(map, dst, fp, options, error);
  fclose(fp);
  return status;
}

static void lonejson__stream_prepare_parser(lonejson_stream *stream,
                                            void *dst) {
  lonejson__parser_restart_stream(stream->parser, dst);
  if (stream->parser->options.clear_destination) {
    if (stream->prepared_dst == dst) {
      lonejson__reset_map(stream->map, dst);
    } else {
      lonejson__init_map_with_allocator(stream->map, dst, &stream->allocator);
      stream->prepared_dst = dst;
    }
  }
  stream->current_dst = dst;
  stream->object_in_progress = 1;
}

static lonejson_read_result lonejson__stream_read(lonejson_stream *stream) {
  lonejson_read_result result;

  memset(&result, 0, sizeof(result));
  switch (stream->source_kind) {
  case LONEJSON_STREAM_SOURCE_READER:
    return stream->reader(stream->reader_user, stream->io_buffer,
                          sizeof(stream->io_buffer));
  case LONEJSON_STREAM_SOURCE_FILE:
    return lonejson__file_reader(stream->fp, stream->io_buffer,
                                 sizeof(stream->io_buffer));
  case LONEJSON_STREAM_SOURCE_FD: {
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
  default:
    result.error_code = EINVAL;
    return result;
  }
}

static lonejson_stream *
lonejson__stream_open_common(const lonejson_map *map,
                             const lonejson_parse_options *options,
                             lonejson_error *error) {
  lonejson_stream *stream;
  size_t parser_bytes;
  unsigned char *workspace;

  if (map == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "map is required");
    return NULL;
  }
  if (options != NULL &&
      !LONEJSON__ALLOCATOR_IS_VALID_CONFIG(options->allocator)) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "allocator must provide either all callbacks or none");
    return NULL;
  }
  parser_bytes = sizeof(*stream->parser) + LONEJSON_PUSH_PARSER_BUFFER_SIZE +
                 LONEJSON__PARSER_WORKSPACE_SLACK;
  stream = (lonejson_stream *)lonejson__buffer_alloc(
      options ? options->allocator : NULL, sizeof(*stream) + parser_bytes);
  if (stream == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
                        "failed to allocate stream");
    return NULL;
  }
  stream->map = map;
  stream->options = options ? *options : lonejson_default_parse_options();
  stream->allocator = lonejson__allocator_resolve(stream->options.allocator);
  stream->self_alloc_size = sizeof(*stream) + parser_bytes;
  lonejson__clear_error(&stream->error);
  stream->reader = NULL;
  stream->reader_user = NULL;
  stream->fp = NULL;
  stream->fd = -1;
  stream->owns_fp = 0;
  stream->owns_fd = 0;
  stream->saw_eof = 0;
  stream->object_in_progress = 0;
  stream->buffered_start = 0u;
  stream->buffered_end = 0u;
  stream->source_kind = 0;
  stream->current_dst = NULL;
  stream->prepared_dst = NULL;
  stream->parser =
      (lonejson_parser *)(((unsigned char *)stream) + sizeof(*stream));
  workspace = ((unsigned char *)stream->parser) + sizeof(*stream->parser);
  memset(stream->parser, 0, sizeof(*stream->parser));
  workspace = lonejson__align_pointer(workspace,
                                      LONEJSON__PARSER_WORKSPACE_ALIGNMENT);
  stream->parser->root_map = map;
  stream->parser->options = stream->options;
  stream->parser->allocator = stream->allocator;
  stream->parser->workspace = workspace;
  stream->parser->workspace_size =
      (size_t)(((unsigned char *)stream) + sizeof(*stream) + parser_bytes -
               workspace);
  stream->parser->workspace_top = stream->parser->workspace_size;
  stream->parser->frames = (lonejson_frame *)workspace;
  stream->parser->self_alloc_size = parser_bytes;
  stream->parser->owns_self = 0;
  lonejson__clear_error(error);
  return stream;
}

lonejson_stream *
lonejson_stream_open_reader(const lonejson_map *map, lonejson_reader_fn reader,
                            void *user, const lonejson_parse_options *options,
                            lonejson_error *error) {
  lonejson_stream *stream;

  if (reader == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "reader callback is required");
    return NULL;
  }
  stream = lonejson__stream_open_common(map, options, error);
  if (stream == NULL) {
    return NULL;
  }
  stream->source_kind = LONEJSON_STREAM_SOURCE_READER;
  stream->reader = reader;
  stream->reader_user = user;
  return stream;
}

lonejson_stream *
lonejson_stream_open_filep(const lonejson_map *map, FILE *fp,
                           const lonejson_parse_options *options,
                           lonejson_error *error) {
  lonejson_stream *stream;

  if (fp == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "file pointer is required");
    return NULL;
  }
  stream = lonejson__stream_open_common(map, options, error);
  if (stream == NULL) {
    return NULL;
  }
  stream->source_kind = LONEJSON_STREAM_SOURCE_FILE;
  stream->fp = fp;
  return stream;
}

lonejson_stream *
lonejson_stream_open_path(const lonejson_map *map, const char *path,
                          const lonejson_parse_options *options,
                          lonejson_error *error) {
  lonejson_stream *stream;
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
  stream = lonejson_stream_open_filep(map, fp, options, error);
  if (stream == NULL) {
    fclose(fp);
    return NULL;
  }
  stream->owns_fp = 1;
  return stream;
}

lonejson_stream *lonejson_stream_open_fd(const lonejson_map *map, int fd,
                                         const lonejson_parse_options *options,
                                         lonejson_error *error) {
  lonejson_stream *stream;

  if (fd < 0) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "fd must be non-negative");
    return NULL;
  }
  stream = lonejson__stream_open_common(map, options, error);
  if (stream == NULL) {
    return NULL;
  }
  stream->source_kind = LONEJSON_STREAM_SOURCE_FD;
  stream->fd = fd;
  return stream;
}

lonejson_stream_result lonejson_stream_next(lonejson_stream *stream, void *dst,
                                            lonejson_error *error) {
  lonejson_status status;

  if (stream == NULL || dst == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "stream and destination are required");
    return LONEJSON_STREAM_ERROR;
  }
  if (!stream->object_in_progress) {
    lonejson__stream_prepare_parser(stream, dst);
  } else if (stream->current_dst != dst) {
    lonejson__set_error(&stream->error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u,
                        0u, 0u, "resume the stream with the same destination");
    if (error != NULL) {
      *error = stream->error;
    }
    return LONEJSON_STREAM_ERROR;
  }

  for (;;) {
    if (lonejson__parser_root_complete(stream->parser)) {
      if (stream->parser->error.code == LONEJSON_STATUS_OK &&
          !stream->parser->error.truncated) {
        stream->error.code = LONEJSON_STATUS_OK;
        stream->error.line = 0u;
        stream->error.column = 0u;
        stream->error.offset = 0u;
        stream->error.system_errno = 0;
        stream->error.truncated = 0;
        stream->error.message[0] = '\0';
      } else {
        stream->error = stream->parser->error;
      }
      stream->object_in_progress = 0;
      stream->current_dst = NULL;
      if (error != NULL) {
        *error = stream->error;
      }
      return LONEJSON_STREAM_OBJECT;
    }

    if (stream->buffered_start == stream->buffered_end) {
      lonejson_read_result chunk = lonejson__stream_read(stream);
      if (chunk.error_code != 0) {
        stream->error.system_errno = chunk.error_code;
        lonejson__set_error(&stream->error, LONEJSON_STATUS_IO_ERROR, 0u, 0u,
                            0u, "stream read failed");
        if (error != NULL) {
          *error = stream->error;
        }
        return LONEJSON_STREAM_ERROR;
      }
      if (chunk.would_block) {
        if (error != NULL) {
          *error = stream->error;
        }
        return LONEJSON_STREAM_WOULD_BLOCK;
      }
      if (chunk.bytes_read == 0u && chunk.eof) {
        stream->saw_eof = 1;
        if (!stream->parser->root_started) {
          if (error != NULL) {
            *error = stream->error;
          }
          return LONEJSON_STREAM_EOF;
        }
        stream->error.code = lonejson_parser_finish(stream->parser);
        stream->error = stream->parser->error;
        stream->object_in_progress = 0;
        stream->current_dst = NULL;
        if (error != NULL) {
          *error = stream->error;
        }
        return LONEJSON_STREAM_ERROR;
      }
      stream->buffered_start = 0u;
      stream->buffered_end = chunk.bytes_read;
      continue;
    }

    if (!stream->parser->root_started) {
      size_t start = stream->buffered_start;
      while (start < stream->buffered_end &&
             lonejson__is_json_space(stream->io_buffer[start])) {
        start++;
      }
      stream->buffered_start = start;
      if (stream->buffered_start == stream->buffered_end) {
        continue;
      }
      if (stream->io_buffer[stream->buffered_start] != '{') {
        lonejson__set_error(&stream->error, LONEJSON_STATUS_INVALID_JSON, 0u,
                            0u, 0u, "expected top-level object");
        if (error != NULL) {
          *error = stream->error;
        }
        return LONEJSON_STREAM_ERROR;
      }
    }

    {
      size_t consumed;

      status = lonejson__parser_feed_bytes(
          stream->parser, stream->io_buffer + stream->buffered_start,
          stream->buffered_end - stream->buffered_start, &consumed, 1);
      stream->buffered_start += consumed;
    }
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      stream->error = stream->parser->error;
      stream->object_in_progress = 0;
      stream->current_dst = NULL;
      if (error != NULL) {
        *error = stream->error;
      }
      return LONEJSON_STREAM_ERROR;
    }
    if (lonejson__parser_root_complete(stream->parser)) {
      if (stream->parser->error.code == LONEJSON_STATUS_OK &&
          !stream->parser->error.truncated) {
        stream->error.code = LONEJSON_STATUS_OK;
        stream->error.line = 0u;
        stream->error.column = 0u;
        stream->error.offset = 0u;
        stream->error.system_errno = 0;
        stream->error.truncated = 0;
        stream->error.message[0] = '\0';
      } else {
        stream->error = stream->parser->error;
      }
      stream->object_in_progress = 0;
      stream->current_dst = NULL;
      if (error != NULL) {
        *error = stream->error;
      }
      return LONEJSON_STREAM_OBJECT;
    }
  }
}

const lonejson_error *lonejson_stream_error(const lonejson_stream *stream) {
  return stream ? &stream->error : NULL;
}

void lonejson_stream_close(lonejson_stream *stream) {
  if (stream == NULL) {
    return;
  }
  lonejson_parser_destroy(stream->parser);
  if (stream->owns_fp && stream->fp != NULL) {
    fclose(stream->fp);
  }
  if (stream->owns_fd && stream->fd >= 0) {
    close(stream->fd);
  }
  lonejson__buffer_free(&stream->allocator, stream, stream->self_alloc_size);
}

lonejson_status lonejson_validate_buffer(const void *data, size_t len,
                                         lonejson_error *error) {
  lonejson_parser *parser;
  lonejson_status status;
  lonejson_parser parser_storage;
  unsigned char parser_workspace[LONEJSON_PARSER_BUFFER_SIZE +
                                 LONEJSON__PARSER_WORKSPACE_SLACK];

  if (data == NULL && len != 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "buffer is required");
  }
  parser = &parser_storage;
  lonejson__parser_init_state(parser, NULL, NULL, NULL, 1, parser_workspace,
                              sizeof(parser_workspace));
  lonejson__clear_error(error);
  status = lonejson_parser_feed(parser, data, len);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    status = lonejson_parser_finish(parser);
  }
  if (error != NULL) {
    *error = parser->error;
  }
  return status;
}

lonejson_status lonejson_validate_cstr(const char *json,
                                       lonejson_error *error) {
  if (json == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "json string is required");
  }
  return lonejson_validate_buffer(json, strlen(json), error);
}

lonejson_status lonejson_validate_reader(lonejson_reader_fn reader, void *user,
                                         lonejson_error *error) {
  lonejson_parser *parser;
  unsigned char buffer[LONEJSON_READER_BUFFER_SIZE];
  lonejson_status status = LONEJSON_STATUS_OK;
  lonejson_parser parser_storage;
  unsigned char parser_workspace[LONEJSON_PARSER_BUFFER_SIZE +
                                 LONEJSON__PARSER_WORKSPACE_SLACK];

  if (reader == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "reader callback is required");
  }
  parser = &parser_storage;
  lonejson__parser_init_state(parser, NULL, NULL, NULL, 1, parser_workspace,
                              sizeof(parser_workspace));
  lonejson__clear_error(error);
  for (;;) {
    lonejson_read_result chunk = reader(user, buffer, sizeof(buffer));
    if (chunk.error_code != 0) {
      parser->error.system_errno = chunk.error_code;
      status = lonejson__set_error(
          &parser->error, LONEJSON_STATUS_IO_ERROR, parser->error.offset,
          parser->error.line, parser->error.column, "reader callback failed");
      break;
    }
    if (chunk.bytes_read != 0u) {
      status = lonejson_parser_feed(parser, buffer, chunk.bytes_read);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        break;
      }
    }
    if (chunk.eof) {
      status = lonejson_parser_finish(parser);
      break;
    }
  }
  if (error != NULL) {
    *error = parser->error;
  }
  return status;
}

lonejson_status lonejson_validate_filep(FILE *fp, lonejson_error *error) {
  if (fp == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "file pointer is required");
  }
  return lonejson_validate_reader(lonejson__file_reader, fp, error);
}

lonejson_status lonejson_validate_path(const char *path,
                                       lonejson_error *error) {
  FILE *fp;
  lonejson_status status;

  if (path == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "path is required");
  }
  fp = fopen(path, "rb");
  if (fp == NULL) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 1u, 0u,
                               "failed to open '%s'", path);
  }
  status = lonejson_validate_filep(fp, error);
  fclose(fp);
  return status;
}

static lonejson_read_result
lonejson__fd_reader(void *user, unsigned char *buffer, size_t capacity) {
  lonejson_read_result result;
  int fd = *(const int *)user;
  ssize_t got;

  memset(&result, 0, sizeof(result));
  got = read(fd, buffer, capacity);
  if (got < 0) {
    result.error_code = errno;
    return result;
  }
  result.bytes_read = (size_t)got;
  result.eof = (got == 0) ? 1 : 0;
  return result;
}

lonejson_status lonejson_visit_value_buffer(
    const void *data, size_t len, const lonejson_value_visitor *visitor,
    void *user, const lonejson_value_limits *limits, lonejson_error *error) {
  lonejson__json_cursor cursor;

  if (data == NULL || visitor == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "buffer and visitor are required");
  }
  memset(&cursor, 0, sizeof(cursor));
  cursor.buffer = (const unsigned char *)data;
  cursor.buffer_len = len;
  return lonejson__json_visit_cursor(&cursor, NULL, visitor, user, limits,
                                     error);
}

lonejson_status lonejson_visit_value_cstr(const char *json,
                                          const lonejson_value_visitor *visitor,
                                          void *user,
                                          const lonejson_value_limits *limits,
                                          lonejson_error *error) {
  if (json == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "json string and visitor are required");
  }
  return lonejson_visit_value_buffer(json, strlen(json), visitor, user, limits,
                                     error);
}

lonejson_status
lonejson_visit_value_reader(lonejson_reader_fn reader, void *reader_user,
                            const lonejson_value_visitor *visitor, void *user,
                            const lonejson_value_limits *limits,
                            lonejson_error *error) {
  lonejson_json_value value;

  if (reader == NULL || visitor == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "reader and visitor are required");
  }
  lonejson_json_value_init(&value);
  value.kind = LONEJSON_JSON_VALUE_READER;
  value.reader = reader;
  value.reader_user = reader_user;
  return lonejson__json_visit(&value, visitor, user, limits, error);
}

lonejson_status
lonejson_visit_value_filep(FILE *fp, const lonejson_value_visitor *visitor,
                           void *user, const lonejson_value_limits *limits,
                           lonejson_error *error) {
  if (fp == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "file pointer and visitor are required");
  }
  return lonejson_visit_value_reader(lonejson__file_reader, fp, visitor, user,
                                     limits, error);
}

lonejson_status lonejson_visit_value_path(const char *path,
                                          const lonejson_value_visitor *visitor,
                                          void *user,
                                          const lonejson_value_limits *limits,
                                          lonejson_error *error) {
  FILE *fp;
  lonejson_status status;

  if (path == NULL || visitor == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "path and visitor are required");
  }
  fp = fopen(path, "rb");
  if (fp == NULL) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 1u, 0u,
                               "failed to open '%s'", path);
  }
  status = lonejson_visit_value_filep(fp, visitor, user, limits, error);
  fclose(fp);
  return status;
}

lonejson_status lonejson_visit_value_fd(int fd,
                                        const lonejson_value_visitor *visitor,
                                        void *user,
                                        const lonejson_value_limits *limits,
                                        lonejson_error *error) {
  if (fd < 0 || visitor == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "fd and visitor are required");
  }
  return lonejson_visit_value_reader(lonejson__fd_reader, &fd, visitor, user,
                                     limits, error);
}

lonejson_status lonejson_serialize_sink(const lonejson_map *map,
                                        const void *src, lonejson_sink_fn sink,
                                        void *user,
                                        const lonejson_write_options *options,
                                        lonejson_error *error) {
  lonejson_status status;
  lonejson__write_state state;

  if (map == NULL || src == NULL || sink == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "map, source, and sink are required");
  }
  if (!lonejson__map_layout_is_valid(map)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "invalid map layout");
  }
  lonejson__clear_error(error);
  if (options != NULL && options->pretty) {
    state.sink = sink;
    state.user = user;
    state.error = error;
    state.pretty = 1;
    state.depth = 0u;
    status = lonejson__serialize_map_pretty(map, src, &state);
  } else {
    status = lonejson__serialize_map_compact(map, src, sink, user, error);
  }
  return status;
}

lonejson_status lonejson_serialize_buffer(const lonejson_map *map,
                                          const void *src, char *buffer,
                                          size_t capacity, size_t *needed,
                                          const lonejson_write_options *options,
                                          lonejson_error *error) {
  lonejson_buffer_sink sink;
  lonejson_write_options local_options;
  lonejson_status status;

  if (buffer == NULL || capacity == 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "output buffer and capacity are required");
  }
  local_options = options ? *options : lonejson_default_write_options();
  sink.buffer = buffer;
  sink.capacity = capacity;
  sink.length = 0u;
  sink.needed = 0u;
  sink.alloc_size = 0u;
  sink.max_bytes = 0u;
  sink.policy = local_options.overflow_policy;
  sink.truncated = 0;
  buffer[0] = '\0';
  if (local_options.overflow_policy == LONEJSON_OVERFLOW_FAIL &&
      !local_options.pretty) {
    if (map == NULL || src == NULL) {
      status =
          lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                              0u, "map, source, and sink are required");
    } else if (!lonejson__map_layout_is_valid(map)) {
      status = lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u,
                                   0u, 0u, "invalid map layout");
    } else {
      lonejson__clear_error(error);
      status =
          lonejson__serialize_map_compact_buffer_exact(map, src, &sink, error);
    }
  } else {
    status = lonejson_serialize_sink(
        map, src,
        (local_options.overflow_policy == LONEJSON_OVERFLOW_FAIL)
            ? lonejson__sink_buffer_exact
            : lonejson__sink_buffer,
        &sink, &local_options, error);
  }
  sink.buffer[(sink.length < sink.capacity) ? sink.length
                                            : (sink.capacity - 1u)] = '\0';
  if (needed != NULL) {
    *needed = sink.needed;
  }
  if (status == LONEJSON_STATUS_OK && sink.truncated &&
      local_options.overflow_policy == LONEJSON_OVERFLOW_TRUNCATE) {
    status = LONEJSON_STATUS_TRUNCATED;
  }
  return status;
}

lonejson_status lonejson_serialize_owned(const lonejson_map *map,
                                         const void *src,
                                         lonejson_owned_buffer *out,
                                         const lonejson_write_options *options,
                                         lonejson_error *error) {
  lonejson_buffer_sink sink;
  lonejson_status status;
  lonejson_allocator resolved;

  if (out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "output buffer handle is required");
  }
  lonejson_owned_buffer_init(out);
  memset(&sink, 0, sizeof(sink));
  if (options != NULL &&
      !LONEJSON__ALLOCATOR_IS_VALID_CONFIG(options->allocator)) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "allocator must provide either all callbacks or none");
  }
  sink.allocator = options ? options->allocator : NULL;
  sink.max_bytes = lonejson__write_max_output_bytes(options);
  if ((options == NULL || !options->pretty)) {
    if (map == NULL || src == NULL) {
      status =
          lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                              0u, "map, source, and sink are required");
    } else if (!lonejson__map_layout_is_valid(map)) {
      status = lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u,
                                   0u, 0u, "invalid map layout");
    } else {
      lonejson__clear_error(error);
      status =
          lonejson__serialize_map_compact_buffer_grow(map, src, &sink, error);
    }
  } else {
    status = lonejson_serialize_sink(map, src, lonejson__sink_grow, &sink,
                                     options, error);
  }
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    lonejson__buffer_free(sink.allocator, sink.buffer, sink.alloc_size);
    return status;
  }
  resolved = lonejson__allocator_resolve(sink.allocator);
  out->data = sink.buffer;
  out->len = sink.length;
  out->alloc_size = sink.alloc_size;
  out->allocator = resolved;
  if (out->data != NULL) {
    out->data[out->len] = '\0';
  }
  return status;
}

void lonejson_owned_buffer_free(lonejson_owned_buffer *buffer) {
  if (buffer == NULL) {
    return;
  }
  if (buffer->data != NULL) {
    lonejson__buffer_free(&buffer->allocator, buffer->data, buffer->alloc_size);
  }
  lonejson_owned_buffer_init(buffer);
}

lonejson_status lonejson_owned_buffer_sink(void *user, const void *data,
                                           size_t len, lonejson_error *error) {
  lonejson_owned_buffer *buffer = (lonejson_owned_buffer *)user;
  char *next;
  size_t required;
  size_t next_cap;

  if (buffer == NULL || (data == NULL && len != 0u)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "owned buffer sink arguments are invalid");
  }
  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  required = buffer->len + len + 1u;
  if (required <= buffer->len) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "owned buffer sink size overflow");
  }
  if (buffer->alloc_size < required) {
    next_cap = buffer->alloc_size != 0u ? buffer->alloc_size : 64u;
    while (next_cap < required) {
      size_t doubled = next_cap * 2u;
      if (doubled <= next_cap) {
        next_cap = required;
        break;
      }
      next_cap = doubled;
    }
    next = (char *)lonejson__buffer_realloc(
        &buffer->allocator, buffer->data, buffer->alloc_size, next_cap);
    if (next == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u, "failed to grow owned buffer sink");
    }
    buffer->data = next;
    buffer->alloc_size = next_cap;
  }
  memcpy(buffer->data + buffer->len, data, len);
  buffer->len += len;
  buffer->data[buffer->len] = '\0';
  return LONEJSON_STATUS_OK;
}

char *lonejson_serialize_alloc(const lonejson_map *map, const void *src,
                               size_t *out_len,
                               const lonejson_write_options *options,
                               lonejson_error *error) {
  lonejson_owned_buffer buffer;
  lonejson_write_options local_options;
  lonejson_status status;

  if (options != NULL &&
      !LONEJSON__ALLOCATOR_IS_VALID_CONFIG(options->allocator)) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "allocator must provide either all callbacks or none");
    return NULL;
  }
  if (options != NULL && options->allocator != NULL &&
      !lonejson__allocator_is_default_family(options->allocator)) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "serialize_alloc uses the default allocator only; use "
                        "serialize_owned for custom allocators");
    return NULL;
  }
  local_options = options ? *options : lonejson_default_write_options();
  local_options.allocator = NULL;
  status = lonejson_serialize_owned(map, src, &buffer, &local_options, error);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return NULL;
  }
  if (out_len != NULL) {
    *out_len = buffer.len;
  }
  return buffer.data;
}

lonejson_status lonejson_serialize_filep(const lonejson_map *map,
                                         const void *src, FILE *fp,
                                         const lonejson_write_options *options,
                                         lonejson_error *error) {
  if (fp == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "file pointer is required");
  }
  return lonejson_serialize_sink(map, src, lonejson__sink_file, fp, options,
                                 error);
}

lonejson_status lonejson_serialize_path(const lonejson_map *map,
                                        const void *src, const char *path,
                                        const lonejson_write_options *options,
                                        lonejson_error *error) {
  FILE *fp;
  lonejson_status status;

  if (path == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "path is required");
  }
  fp = fopen(path, "wb");
  if (fp == NULL) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                               "failed to open '%s' for writing", path);
  }
  status = lonejson_serialize_filep(map, src, fp, options, error);
  fclose(fp);
  return status;
}

lonejson_status lonejson_serialize_jsonl_sink(
    const lonejson_map *map, const void *items, size_t count, size_t stride,
    lonejson_sink_fn sink, void *user, const lonejson_write_options *options,
    lonejson_error *error) {
  lonejson_status status;
  lonejson__write_state state;

  if (map == NULL || sink == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "map and sink are required");
  }
  if (count != 0u && items == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "items are required when count is non-zero");
  }
  if (stride == 0u) {
    stride = map->struct_size;
  }
  if (stride < map->struct_size) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "stride must be at least map->struct_size");
  }
  lonejson__clear_error(error);
  if (options != NULL && options->pretty) {
    state.sink = sink;
    state.user = user;
    state.error = error;
    state.pretty = 1;
    state.depth = 0u;
    status =
        lonejson__serialize_jsonl_records(map, items, count, stride, &state);
  } else {
    status = lonejson__serialize_jsonl_records_compact(
        map, items, count, stride, sink, user, error);
  }
  return status;
}

lonejson_status lonejson_serialize_jsonl_buffer(
    const lonejson_map *map, const void *items, size_t count, size_t stride,
    char *buffer, size_t capacity, size_t *needed,
    const lonejson_write_options *options, lonejson_error *error) {
  lonejson_buffer_sink sink;
  lonejson_write_options local_options;
  lonejson_status status;

  if (buffer == NULL || capacity == 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "output buffer and capacity are required");
  }
  local_options = options ? *options : lonejson_default_write_options();
  sink.buffer = buffer;
  sink.capacity = capacity;
  sink.length = 0u;
  sink.needed = 0u;
  sink.alloc_size = 0u;
  sink.max_bytes = 0u;
  sink.policy = local_options.overflow_policy;
  sink.truncated = 0;
  buffer[0] = '\0';
  if (local_options.overflow_policy == LONEJSON_OVERFLOW_FAIL &&
      !local_options.pretty) {
    size_t i;
    const unsigned char *base = (const unsigned char *)items;

    if (map == NULL) {
      status = lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u,
                                   0u, 0u, "map and sink are required");
    } else if (count != 0u && items == NULL) {
      status =
          lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                              0u, "items are required when count is non-zero");
    } else if ((stride != 0u && stride < map->struct_size)) {
      status =
          lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                              0u, "stride must be at least map->struct_size");
    } else if (!lonejson__map_layout_is_valid(map)) {
      status = lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u,
                                   0u, 0u, "invalid map layout");
    } else {
      lonejson__clear_error(error);
      if (stride == 0u) {
        stride = map->struct_size;
      }
      status = LONEJSON_STATUS_OK;
      for (i = 0u; i < count; ++i) {
        const void *record = (const void *)(base + (i * stride));
        status = lonejson__serialize_map_compact_buffer_exact(map, record,
                                                              &sink, error);
        if (status != LONEJSON_STATUS_OK) {
          break;
        }
        status = lonejson__buffer_emit_exact(&sink, error, "\n", 1u);
        if (status != LONEJSON_STATUS_OK) {
          break;
        }
      }
    }
  } else {
    status = lonejson_serialize_jsonl_sink(
        map, items, count, stride,
        (local_options.overflow_policy == LONEJSON_OVERFLOW_FAIL)
            ? lonejson__sink_buffer_exact
            : lonejson__sink_buffer,
        &sink, &local_options, error);
  }
  sink.buffer[(sink.length < sink.capacity) ? sink.length
                                            : (sink.capacity - 1u)] = '\0';
  if (needed != NULL) {
    *needed = sink.needed;
  }
  if (status == LONEJSON_STATUS_OK && sink.truncated &&
      local_options.overflow_policy == LONEJSON_OVERFLOW_TRUNCATE) {
    status = LONEJSON_STATUS_TRUNCATED;
  }
  return status;
}

lonejson_status lonejson_serialize_jsonl_owned(
    const lonejson_map *map, const void *items, size_t count, size_t stride,
    lonejson_owned_buffer *out, const lonejson_write_options *options,
    lonejson_error *error) {
  lonejson_buffer_sink sink;
  lonejson_status status;
  lonejson_allocator resolved;

  if (out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "output buffer handle is required");
  }
  lonejson_owned_buffer_init(out);
  memset(&sink, 0, sizeof(sink));
  if (options != NULL &&
      !LONEJSON__ALLOCATOR_IS_VALID_CONFIG(options->allocator)) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "allocator must provide either all callbacks or none");
  }
  sink.allocator = options ? options->allocator : NULL;
  sink.max_bytes = lonejson__write_max_output_bytes(options);
  if ((options == NULL || !options->pretty)) {
    size_t i;
    const unsigned char *base = (const unsigned char *)items;

    if (map == NULL) {
      status = lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u,
                                   0u, 0u, "map and sink are required");
    } else if (count != 0u && items == NULL) {
      status =
          lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                              0u, "items are required when count is non-zero");
    } else if ((stride != 0u && stride < map->struct_size)) {
      status =
          lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                              0u, "stride must be at least map->struct_size");
    } else if (!lonejson__map_layout_is_valid(map)) {
      status = lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u,
                                   0u, 0u, "invalid map layout");
    } else {
      lonejson__clear_error(error);
      if (stride == 0u) {
        stride = map->struct_size;
      }
      status = LONEJSON_STATUS_OK;
      for (i = 0u; i < count; ++i) {
        const void *record = (const void *)(base + (i * stride));
        status = lonejson__serialize_map_compact_buffer_grow(map, record, &sink,
                                                             error);
        if (status != LONEJSON_STATUS_OK) {
          break;
        }
        status = lonejson__buffer_emit_grow(&sink, error, "\n", 1u);
        if (status != LONEJSON_STATUS_OK) {
          break;
        }
      }
    }
  } else {
    status = lonejson_serialize_jsonl_sink(
        map, items, count, stride, lonejson__sink_grow, &sink, options, error);
  }
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    lonejson__buffer_free(sink.allocator, sink.buffer, sink.alloc_size);
    return status;
  }
  resolved = lonejson__allocator_resolve(sink.allocator);
  out->data = sink.buffer;
  out->len = sink.length;
  out->alloc_size = sink.alloc_size;
  out->allocator = resolved;
  if (out->data != NULL) {
    out->data[out->len] = '\0';
  }
  return status;
}

char *lonejson_serialize_jsonl_alloc(const lonejson_map *map, const void *items,
                                     size_t count, size_t stride,
                                     size_t *out_len,
                                     const lonejson_write_options *options,
                                     lonejson_error *error) {
  lonejson_owned_buffer buffer;
  lonejson_write_options local_options;
  lonejson_status status;

  if (options != NULL &&
      !LONEJSON__ALLOCATOR_IS_VALID_CONFIG(options->allocator)) {
    lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                        "allocator must provide either all callbacks or none");
    return NULL;
  }
  if (options != NULL && options->allocator != NULL &&
      !lonejson__allocator_is_default_family(options->allocator)) {
    lonejson__set_error(
        error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
        "serialize_jsonl_alloc uses the default allocator only; use "
        "serialize_jsonl_owned for custom allocators");
    return NULL;
  }
  local_options = options ? *options : lonejson_default_write_options();
  local_options.allocator = NULL;
  status = lonejson_serialize_jsonl_owned(map, items, count, stride, &buffer,
                                          &local_options, error);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return NULL;
  }
  if (out_len != NULL) {
    *out_len = buffer.len;
  }
  return buffer.data;
}

lonejson_status lonejson_serialize_jsonl_filep(
    const lonejson_map *map, const void *items, size_t count, size_t stride,
    FILE *fp, const lonejson_write_options *options, lonejson_error *error) {
  if (fp == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "file pointer is required");
  }
  return lonejson_serialize_jsonl_sink(map, items, count, stride,
                                       lonejson__sink_file, fp, options, error);
}

lonejson_status
lonejson_serialize_jsonl_path(const lonejson_map *map, const void *items,
                              size_t count, size_t stride, const char *path,
                              const lonejson_write_options *options,
                              lonejson_error *error) {
  FILE *fp;
  lonejson_status status;

  if (path == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "path is required");
  }
  fp = fopen(path, "wb");
  if (fp == NULL) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                               "failed to open '%s' for writing", path);
  }
  status = lonejson_serialize_jsonl_filep(map, items, count, stride, fp,
                                          options, error);
  fclose(fp);
  return status;
}

void lonejson_cleanup(const lonejson_map *map, void *value) {
  lonejson__cleanup_map_checked(map, value);
}

void lonejson_init(const lonejson_map *map, void *value) {
  lonejson__init_map(map, value);
}

void lonejson_reset(const lonejson_map *map, void *value) {
  lonejson__reset_map(map, value);
}
