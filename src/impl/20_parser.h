lonejson_status
lonejson_json_value_write_to_sink(const lonejson_json_value *value,
                                  lonejson_sink_fn sink, void *user,
                                  lonejson_error *error) {
  if (sink == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "sink is required");
  }
  return lonejson__json_transcode(value, sink, user, error, 0, 0u);
}

static LONEJSON__INLINE int lonejson__scratch_reserve(lonejson_scratch *scratch,
                                                      size_t need) {
  if (need + 1u <= scratch->cap) {
    return 1;
  }
  return 0;
}

static LONEJSON__INLINE int lonejson__scratch_append(lonejson_scratch *scratch,
                                                     char ch) {
  if (!lonejson__scratch_reserve(scratch, scratch->len + 2u)) {
    return 0;
  }
  scratch->data[scratch->len++] = ch;
  return 1;
}

static LONEJSON__INLINE int
lonejson__scratch_append_bytes(lonejson_scratch *scratch,
                               const unsigned char *src, size_t len) {
  if (!lonejson__scratch_reserve(scratch, scratch->len + len + 1u)) {
    return 0;
  }
  memcpy(scratch->data + scratch->len, src, len);
  scratch->len += len;
  return 1;
}

static int lonejson__is_valid_json_number(const char *value, size_t len);
static int lonejson__is_json_space(int ch);
static int lonejson__is_digit(int ch);
static int lonejson__is_finite_f64(double value);
static int lonejson__is_exact_fixed_capacity(unsigned flags);
static size_t lonejson__frame_bytes(size_t frame_count);
static size_t lonejson__workspace_align(size_t size);
static unsigned char *lonejson__align_pointer(unsigned char *ptr,
                                              size_t alignment);
static unsigned char *lonejson__token_base(lonejson_parser *parser);
static size_t lonejson__token_limit(lonejson_parser *parser);
static unsigned char *lonejson__workspace_alloc_top(lonejson_parser *parser,
                                                    size_t size);
static int lonejson__mark_field_seen(lonejson_parser *parser,
                                     lonejson_frame *frame,
                                     const lonejson_field *field);
static lonejson_status
lonejson__complete_parent_after_value(lonejson_parser *parser);
static LONEJSON__INLINE void *lonejson__field_ptr(void *base,
                                                  const lonejson_field *field);
static lonejson_status lonejson__stream_value_begin(lonejson_parser *parser,
                                                    const lonejson_field *field,
                                                    void *ptr);
static void lonejson__parser_init_state(lonejson_parser *parser,
                                        const lonejson_map *map, void *dst,
                                        const lonejson_parse_options *options,
                                        int validate_only,
                                        unsigned char *workspace,
                                        size_t workspace_size);
#define lonejson__parser_peak_workspace_used(parser_ptr)                       \
  ((parser_ptr) != NULL ? (parser_ptr)->workspace_peak : 0u)
static LONEJSON__HOT lonejson_status
lonejson__parser_feed_bytes(lonejson_parser *parser, const unsigned char *bytes,
                            size_t len, size_t *consumed, int stop_at_root);

static void lonejson__scratch_reset(lonejson_scratch *scratch) {
  scratch->len = 0;
}

static const char *lonejson__scratch_cstr(lonejson_scratch *scratch) {
  if (scratch == NULL || scratch->data == NULL) {
    return "";
  }
  scratch->data[scratch->len] = '\0';
  return scratch->data;
}

static LONEJSON__INLINE int lonejson__is_json_space(int ch) {
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

static LONEJSON__INLINE int lonejson__is_digit(int ch) {
  return ch >= '0' && ch <= '9';
}

static int lonejson__is_finite_f64(double value) {
  return value == value && value <= DBL_MAX && value >= -DBL_MAX;
}

static int lonejson__is_exact_fixed_capacity(unsigned flags) {
  return flags == LONEJSON_ARRAY_FIXED_CAPACITY;
}

static size_t lonejson__frame_bytes(size_t frame_count) {
  return frame_count * sizeof(lonejson_frame);
}

static LONEJSON__INLINE unsigned char *
lonejson__align_pointer(unsigned char *ptr, size_t alignment) {
  uintptr_t value = (uintptr_t)ptr;
  uintptr_t mask = (uintptr_t)(alignment - 1u);

  if (alignment == 0u) {
    return ptr;
  }
  value = (value + mask) & ~mask;
  return (unsigned char *)value;
}

static LONEJSON__INLINE unsigned char *
lonejson__token_base(lonejson_parser *parser) {
  return parser->workspace + lonejson__frame_bytes(parser->frame_count);
}

static LONEJSON__INLINE size_t lonejson__token_limit(lonejson_parser *parser) {
  size_t frame_bytes = lonejson__frame_bytes(parser->frame_count);

  if (parser->workspace_top <= frame_bytes) {
    return 0u;
  }
  return parser->workspace_top - frame_bytes - 1u;
}

static LONEJSON__INLINE void
lonejson__parser_note_workspace_usage(lonejson_parser *parser) {
#if LONEJSON_TRACK_WORKSPACE_USAGE
  size_t frame_bytes;
  size_t reserved_top_bytes;
  size_t token_bytes;
  size_t used;

  frame_bytes = lonejson__frame_bytes(parser->frame_count);
  reserved_top_bytes = parser->workspace_size - parser->workspace_top;
  token_bytes = 0u;
  if (parser->lex_mode != LONEJSON_LEX_NONE || parser->token.len != 0u) {
    token_bytes = parser->token.len + 1u;
  }
  used = frame_bytes + reserved_top_bytes + token_bytes;
  if (used > parser->workspace_peak) {
    parser->workspace_peak = used;
  }
#else
  (void)parser;
#endif
}

static LONEJSON__INLINE unsigned char *
lonejson__workspace_alloc_top(lonejson_parser *parser, size_t size) {
  size_t frame_bytes = lonejson__frame_bytes(parser->frame_count);
  size_t aligned_size = lonejson__workspace_align(size);

  if (aligned_size > parser->workspace_top ||
      parser->workspace_top - aligned_size < frame_bytes) {
    return NULL;
  }
  parser->workspace_top -= aligned_size;
  return parser->workspace + parser->workspace_top;
}

static LONEJSON__INLINE size_t lonejson__workspace_align(size_t size) {
  return (size + (sizeof(void *) - 1u)) & ~(sizeof(void *) - 1u);
}

static LONEJSON__INLINE int lonejson__prepare_token(lonejson_parser *parser) {
  parser->token.data = (char *)lonejson__token_base(parser);
  parser->token.cap = lonejson__token_limit(parser);
  if (parser->token.cap == 0u) {
    parser->token.len = 0u;
    return 0;
  }
  lonejson__scratch_reset(&parser->token);
  return 1;
}

static int lonejson__field_is_streamed(const lonejson_field *field) {
  return field != NULL && (field->kind == LONEJSON_FIELD_KIND_STRING_STREAM ||
                           field->kind == LONEJSON_FIELD_KIND_BASE64_STREAM);
}

static LONEJSON__INLINE lonejson_status
lonejson__begin_string_lex(lonejson_parser *parser, int is_key) {
  if (lonejson__json_value_parse_visitor_active(parser)) {
    lonejson_status status = lonejson__json_value_string_begin(parser, is_key);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  parser->lex_mode = LONEJSON_LEX_STRING;
  parser->lex_is_key = is_key;
  parser->string_capture_mode = LONEJSON_STRING_CAPTURE_TOKEN;
  parser->lex_escape = 0;
  parser->unicode_pending_high = 0u;
  parser->unicode_digits_needed = 0;
  if (lonejson__json_value_parse_visitor_active(parser)) {
    parser->string_capture_mode = LONEJSON_STRING_CAPTURE_JSON_VISITOR;
    parser->token.data = NULL;
    parser->token.cap = 0u;
    parser->token.len = 0u;
    return LONEJSON_STATUS_OK;
  }
  return lonejson__prepare_token(parser)
             ? LONEJSON_STATUS_OK
             : lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "parser workspace exhausted by token");
}

static LONEJSON__INLINE lonejson_status lonejson__begin_string_value_lex(
    lonejson_parser *parser, const lonejson_field *field, void *ptr) {
  lonejson_status status;

  parser->lex_mode = LONEJSON_LEX_STRING;
  parser->lex_is_key = 0;
  parser->string_capture_mode = LONEJSON_STRING_CAPTURE_TOKEN;
  parser->lex_escape = 0;
  parser->unicode_pending_high = 0u;
  parser->unicode_digits_needed = 0;
  if ((field == NULL || field->kind == LONEJSON_FIELD_KIND_JSON_VALUE) &&
      lonejson__json_value_parse_visitor_active(parser)) {
    lonejson_status visitor_status =
        lonejson__json_value_string_begin(parser, 0);
    if (visitor_status != LONEJSON_STATUS_OK) {
      return visitor_status;
    }
    parser->string_capture_mode = LONEJSON_STRING_CAPTURE_JSON_VISITOR;
    parser->token.data = NULL;
    parser->token.cap = 0u;
    parser->token.len = 0u;
    return LONEJSON_STATUS_OK;
  }
  if (lonejson__field_is_streamed(field)) {
    status = lonejson__stream_value_begin(parser, field, ptr);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    parser->string_capture_mode = LONEJSON_STRING_CAPTURE_STREAM;
    return LONEJSON_STATUS_OK;
  }
  if (field != NULL && field->kind == LONEJSON_FIELD_KIND_STRING &&
      field->storage == LONEJSON_STORAGE_FIXED && field->fixed_capacity != 0u) {
    parser->direct_string_active = 1;
    parser->string_capture_mode = LONEJSON_STRING_CAPTURE_DIRECT;
    parser->direct_string_ptr = (char *)ptr;
    parser->direct_string_len = 0u;
    parser->direct_string_capacity = field->fixed_capacity;
    parser->direct_string_truncated = 0;
    parser->direct_string_overflow_policy = field->overflow_policy;
    parser->direct_string_field = field;
    return LONEJSON_STATUS_OK;
  }
  return lonejson__prepare_token(parser)
             ? LONEJSON_STATUS_OK
             : lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "parser workspace exhausted by token");
}

static LONEJSON__INLINE lonejson_status
lonejson__begin_number_lex(lonejson_parser *parser, unsigned char ch) {
  parser->lex_mode = LONEJSON_LEX_NUMBER;
  parser->lex_is_key = 0;
  if (!lonejson__prepare_token(parser)) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "parser workspace exhausted by token");
  }
  return lonejson__scratch_append(&parser->token, (char)ch)
             ? LONEJSON_STATUS_OK
             : lonejson__set_error(
                   &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                   parser->error.offset, parser->error.line,
                   parser->error.column, "failed to append number");
}

static LONEJSON__INLINE lonejson_status lonejson__begin_literal_lex(
    lonejson_parser *parser, lonejson_lex_mode mode, unsigned char ch) {
  parser->lex_mode = mode;
  parser->lex_is_key = 0;
  if (!lonejson__prepare_token(parser)) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "parser workspace exhausted by token");
  }
  return lonejson__scratch_append(&parser->token, (char)ch)
             ? LONEJSON_STATUS_OK
             : lonejson__set_error(
                   &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                   parser->error.offset, parser->error.line,
                   parser->error.column, "failed to append literal");
}

static int lonejson__base64_value(int ch) {
  if (ch >= 'A' && ch <= 'Z') {
    return ch - 'A';
  }
  if (ch >= 'a' && ch <= 'z') {
    return 26 + (ch - 'a');
  }
  if (ch >= '0' && ch <= '9') {
    return 52 + (ch - '0');
  }
  if (ch == '+') {
    return 62;
  }
  if (ch == '/') {
    return 63;
  }
  if (ch == '=') {
    return -2;
  }
  return -1;
}

static lonejson_status
lonejson__stream_value_append_decoded(lonejson_parser *parser,
                                      const unsigned char *data, size_t len) {
  if (!parser->stream_value_active || parser->stream_ptr == NULL) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "streamed field is not active");
  }
  return lonejson__spooled_append((lonejson_spooled *)parser->stream_ptr, data,
                                  len, &parser->error);
}

static lonejson_status
lonejson__stream_value_append_base64_char(lonejson_parser *parser,
                                          unsigned char ch) {
  int value;

  value = lonejson__base64_value(ch);
  if (value == -1) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "invalid base64 character in streamed field");
  }
  if (parser->stream_base64_saw_padding && ch != '=') {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "non-padding base64 data after padding");
  }
  parser->stream_base64_quad[parser->stream_base64_quad_len++] = ch;
  if (ch == '=') {
    parser->stream_base64_saw_padding = 1;
  }
  if (parser->stream_base64_quad_len == 4u) {
    unsigned char out[3];
    int a;
    int b;
    int c;
    int d;
    size_t out_len;
    lonejson_status status;

    a = lonejson__base64_value(parser->stream_base64_quad[0]);
    b = lonejson__base64_value(parser->stream_base64_quad[1]);
    c = lonejson__base64_value(parser->stream_base64_quad[2]);
    d = lonejson__base64_value(parser->stream_base64_quad[3]);
    if (a < 0 || b < 0 || c == -1 || d == -1) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "invalid base64 quartet in streamed field");
    }
    out[0] = (unsigned char)((a << 2) | ((b & 0x30) >> 4));
    out_len = 1u;
    if (c != -2) {
      out[1] = (unsigned char)(((b & 0x0Fu) << 4) | ((c & 0x3Cu) >> 2));
      out_len = 2u;
      if (d != -2) {
        out[2] = (unsigned char)(((c & 0x03u) << 6) | d);
        out_len = 3u;
      }
    } else if (d != -2) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "invalid base64 padding in streamed field");
    }
    status = lonejson__stream_value_append_decoded(parser, out, out_len);
    parser->stream_base64_quad_len = 0u;
    return status;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__stream_value_append(lonejson_parser *parser,
                                                     const unsigned char *data,
                                                     size_t len) {
  if (parser->stream_field->kind == LONEJSON_FIELD_KIND_STRING_STREAM) {
    return lonejson__stream_value_append_decoded(parser, data, len);
  }
  {
    size_t i;
    lonejson_status status;

    for (i = 0u; i < len; ++i) {
      status = lonejson__stream_value_append_base64_char(parser, data[i]);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__stream_value_begin(lonejson_parser *parser,
                                                    const lonejson_field *field,
                                                    void *ptr) {
  lonejson__spooled_apply_allocator((lonejson_spooled *)ptr,
                                    &parser->allocator);
  lonejson_spooled_reset((lonejson_spooled *)ptr);
  parser->stream_value_active = 1;
  parser->stream_field = field;
  parser->stream_ptr = ptr;
  parser->stream_base64_quad_len = 0u;
  parser->stream_base64_saw_padding = 0;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__stream_value_finish(lonejson_parser *parser) {
  lonejson_status status;

  status = LONEJSON_STATUS_OK;
  if (parser->stream_value_active &&
      parser->stream_field->kind == LONEJSON_FIELD_KIND_BASE64_STREAM &&
      parser->stream_base64_quad_len != 0u) {
    status = lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "incomplete base64 quartet in streamed field");
  }
  parser->stream_value_active = 0;
  parser->stream_field = NULL;
  parser->stream_ptr = NULL;
  parser->string_capture_mode = LONEJSON_STRING_CAPTURE_TOKEN;
  parser->stream_base64_quad_len = 0u;
  parser->stream_base64_saw_padding = 0;
  return status;
}

static LONEJSON__INLINE lonejson_status lonejson__direct_string_append_bytes(
    lonejson_parser *parser, const unsigned char *data, size_t len) {
  size_t limit;
  size_t remaining;
  size_t copy_len;

  if (!parser->direct_string_active || parser->direct_string_ptr == NULL ||
      parser->direct_string_capacity == 0u) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "direct string field is not active");
  }
  limit = parser->direct_string_capacity - 1u;
  remaining = (parser->direct_string_len < limit)
                  ? (limit - parser->direct_string_len)
                  : 0u;
  copy_len = len < remaining ? len : remaining;
  if (copy_len != 0u) {
    memcpy(parser->direct_string_ptr + parser->direct_string_len, data,
           copy_len);
    parser->direct_string_len += copy_len;
  }
  if (copy_len == len) {
    return LONEJSON_STATUS_OK;
  }
  if (parser->direct_string_overflow_policy == LONEJSON_OVERFLOW_FAIL) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "string field '%s' exceeds fixed capacity",
                               parser->direct_string_field != NULL
                                   ? parser->direct_string_field->json_key
                                   : "<unknown>");
  }
  parser->direct_string_truncated = 1;
  parser->error.truncated = 1;
  return LONEJSON_STATUS_OK;
}

static LONEJSON__INLINE lonejson_status
lonejson__direct_string_finish(lonejson_parser *parser) {
  lonejson_frame *frame;
  const lonejson_field *field;
  lonejson_status status;

  if (!parser->direct_string_active || parser->direct_string_ptr == NULL ||
      parser->direct_string_capacity == 0u) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "direct string field is not active");
  }
  parser->direct_string_ptr[parser->direct_string_len] = '\0';
  field = parser->direct_string_field;
  status = parser->direct_string_truncated &&
                   parser->direct_string_overflow_policy ==
                       LONEJSON_OVERFLOW_TRUNCATE
               ? LONEJSON_STATUS_TRUNCATED
               : LONEJSON_STATUS_OK;
  parser->direct_string_active = 0;
  parser->direct_string_ptr = NULL;
  parser->string_capture_mode = LONEJSON_STRING_CAPTURE_TOKEN;
  parser->direct_string_len = 0u;
  parser->direct_string_capacity = 0u;
  parser->direct_string_truncated = 0;
  parser->direct_string_overflow_policy = LONEJSON_OVERFLOW_FAIL;
  parser->direct_string_field = NULL;
  frame = (parser->frame_count != 0u)
              ? &parser->frames[parser->frame_count - 1u]
              : NULL;
  if (frame == NULL || frame->kind != LONEJSON_CONTAINER_OBJECT) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "direct string outside object context");
  }
  if (!lonejson__mark_field_seen(parser, frame, field)) {
    return parser->error.code;
  }
  {
    lonejson_status complete = lonejson__complete_parent_after_value(parser);
    if (complete != LONEJSON_STATUS_OK &&
        complete != LONEJSON_STATUS_TRUNCATED) {
      return complete;
    }
  }
  return status;
}

static lonejson_uint64
lonejson__required_mask_for_map(lonejson_parser *parser,
                                const lonejson_map *map) {
  size_t i;
  size_t slot;
  lonejson_uint64 mask;

  if (parser == NULL || map == NULL || map->field_count > 64u) {
    return 0u;
  }
  slot = ((size_t)((const void *)map) >> 4u) % LONEJSON__MAP_MASK_CACHE_SIZE;
  if (parser->required_mask_maps[slot] == map) {
    return parser->required_masks[slot];
  }
  mask = 0u;
  for (i = 0u; i < map->field_count; ++i) {
    if ((map->fields[i].flags & LONEJSON_FIELD_REQUIRED) != 0u) {
      mask |= ((lonejson_uint64)1u) << i;
    }
  }
  parser->required_mask_maps[slot] = map;
  parser->required_masks[slot] = mask;
  return mask;
}

static size_t lonejson__required_count_for_map(lonejson_parser *parser,
                                               const lonejson_map *map) {
  size_t count;
  size_t i;

  if (parser == NULL || map == NULL) {
    return 0u;
  }
  if (map->field_count <= 64u) {
    lonejson_uint64 mask = lonejson__required_mask_for_map(parser, map);
    size_t count = 0u;

    while (mask != 0u) {
      count += (size_t)(mask & 1u);
      mask >>= 1u;
    }
    return count;
  }
  count = 0u;
  for (i = 0u; i < map->field_count; ++i) {
    if ((map->fields[i].flags & LONEJSON_FIELD_REQUIRED) != 0u) {
      count++;
    }
  }
  return count;
}

static void lonejson__parser_init_state(lonejson_parser *parser,
                                        const lonejson_map *map, void *dst,
                                        const lonejson_parse_options *options,
                                        int validate_only,
                                        unsigned char *workspace,
                                        size_t workspace_size) {
  unsigned char *aligned_workspace = workspace;
  size_t alignment_padding = 0u;

  memset(parser, 0, sizeof(*parser));
  parser->root_map = map;
  parser->root_dst = dst;
  parser->validate_only = validate_only;
  parser->options = options ? *options : lonejson_default_parse_options();
  parser->allocator = lonejson__allocator_resolve(parser->options.allocator);
  parser->error.line = 1u;
  parser->error.column = 0u;
  parser->error.code = LONEJSON_STATUS_OK;
  if (workspace != NULL && workspace_size != 0u) {
    aligned_workspace = lonejson__align_pointer(
        workspace, LONEJSON__PARSER_WORKSPACE_ALIGNMENT);
    alignment_padding = (size_t)(aligned_workspace - workspace);
    if (alignment_padding >= workspace_size) {
      workspace_size = 0u;
      aligned_workspace = workspace + workspace_size;
    } else {
      workspace_size -= alignment_padding;
    }
  }
  parser->workspace = aligned_workspace;
  parser->workspace_size = workspace_size;
  parser->workspace_top = workspace_size;
  parser->frames = (lonejson_frame *)aligned_workspace;
  parser->string_capture_mode = LONEJSON_STRING_CAPTURE_TOKEN;
  parser->stream_value_active = 0;
  parser->stream_field = NULL;
  parser->stream_ptr = NULL;
  parser->stream_base64_quad_len = 0u;
  parser->stream_base64_saw_padding = 0;
  parser->direct_string_active = 0;
  parser->direct_string_ptr = NULL;
  parser->direct_string_len = 0u;
  parser->direct_string_capacity = 0u;
  parser->direct_string_truncated = 0;
  parser->direct_string_overflow_policy = LONEJSON_OVERFLOW_FAIL;
  parser->direct_string_field = NULL;
  parser->json_stream_value = NULL;
  parser->json_stream_active = 0;
  parser->json_stream_visit_active = 0;
  parser->json_stream_sink_active = 0;
  parser->json_stream_depth = 0u;
  if (workspace_size != 0u) {
    parser->token.data = (char *)lonejson__token_base(parser);
    parser->token.cap = lonejson__token_limit(parser);
    parser->token.data[0] = '\0';
  }
  lonejson__parser_note_workspace_usage(parser);
}

static void lonejson__parser_restart_stream(lonejson_parser *parser,
                                            void *dst) {
  parser->root_dst = dst;
  memset(&parser->error, 0, sizeof(parser->error));
  parser->error.line = 1u;
  parser->error.column = 0u;
  parser->error.code = LONEJSON_STATUS_OK;
  parser->workspace_top = parser->workspace_size;
  parser->frame_count = 0u;
  parser->workspace_peak = 0u;
  parser->root_started = 0;
  parser->root_finished = 0;
  parser->failed = 0;
  parser->lex_mode = LONEJSON_LEX_NONE;
  parser->string_capture_mode = LONEJSON_STRING_CAPTURE_TOKEN;
  parser->lex_is_key = 0;
  parser->lex_escape = 0;
  parser->unicode_pending_high = 0u;
  parser->unicode_digits_needed = 0;
  parser->unicode_accum = 0u;
  parser->stream_value_active = 0;
  parser->stream_field = NULL;
  parser->stream_ptr = NULL;
  parser->stream_base64_quad_len = 0u;
  parser->stream_base64_saw_padding = 0;
  parser->direct_string_active = 0;
  parser->direct_string_ptr = NULL;
  parser->direct_string_len = 0u;
  parser->direct_string_capacity = 0u;
  parser->direct_string_truncated = 0;
  parser->direct_string_overflow_policy = LONEJSON_OVERFLOW_FAIL;
  parser->direct_string_field = NULL;
  parser->json_stream_value = NULL;
  parser->json_stream_active = 0;
  parser->json_stream_visit_active = 0;
  parser->json_stream_sink_active = 0;
  parser->json_stream_depth = 0u;
  parser->token.len = 0u;
  parser->token.data = (char *)lonejson__token_base(parser);
  parser->token.cap = lonejson__token_limit(parser);
  parser->token.data[0] = '\0';
  lonejson__parser_note_workspace_usage(parser);
}

static LONEJSON__INLINE int lonejson__utf8_append(lonejson_scratch *scratch,
                                                  lonejson_uint32 cp) {
  char *dst;
  size_t len;

  if (scratch == NULL) {
    return 0;
  }
  if (cp <= 0x7Fu) {
    len = 1u;
  } else if (cp <= 0x7FFu) {
    len = 2u;
  } else if (cp <= 0xFFFFu) {
    len = 3u;
  } else if (cp <= 0x10FFFFu) {
    len = 4u;
  } else {
    return 0;
  }
  if (!lonejson__scratch_reserve(scratch, scratch->len + len + 1u)) {
    return 0;
  }
  dst = scratch->data + scratch->len;
  if (len == 1u) {
    dst[0] = (char)cp;
  } else if (len == 2u) {
    dst[0] = (char)(0xC0u | (cp >> 6u));
    dst[1] = (char)(0x80u | (cp & 0x3Fu));
  } else if (len == 3u) {
    dst[0] = (char)(0xE0u | (cp >> 12u));
    dst[1] = (char)(0x80u | ((cp >> 6u) & 0x3Fu));
    dst[2] = (char)(0x80u | (cp & 0x3Fu));
  } else {
    dst[0] = (char)(0xF0u | (cp >> 18u));
    dst[1] = (char)(0x80u | ((cp >> 12u) & 0x3Fu));
    dst[2] = (char)(0x80u | ((cp >> 6u) & 0x3Fu));
    dst[3] = (char)(0x80u | (cp & 0x3Fu));
  }
  scratch->len += len;
  return 1;
}

static LONEJSON__INLINE int
lonejson__field_key_matches(const lonejson_field *field, const char *key,
                            size_t key_len) {
  if (field == NULL || field->json_key_len != key_len) {
    return 0;
  }
  if (key_len == 0u) {
    return 1;
  }
  if (field->json_key_first != (unsigned char)key[0] ||
      field->json_key_last != (unsigned char)key[key_len - 1u]) {
    return 0;
  }
  switch (key_len) {
  case 1u:
  case 2u:
    return 1;
  case 3u:
    return field->json_key[1] == key[1];
  case 4u:
    return field->json_key[1] == key[1] && field->json_key[2] == key[2];
  case 5u:
    return field->json_key[1] == key[1] && field->json_key[2] == key[2] &&
           field->json_key[3] == key[3];
  case 6u:
    return field->json_key[1] == key[1] && field->json_key[2] == key[2] &&
           field->json_key[3] == key[3] && field->json_key[4] == key[4];
  case 7u:
    return field->json_key[1] == key[1] && field->json_key[2] == key[2] &&
           field->json_key[3] == key[3] && field->json_key[4] == key[4] &&
           field->json_key[5] == key[5];
  case 8u:
    return field->json_key[1] == key[1] && field->json_key[2] == key[2] &&
           field->json_key[3] == key[3] && field->json_key[4] == key[4] &&
           field->json_key[5] == key[5] && field->json_key[6] == key[6];
  case 9u:
    return field->json_key[1] == key[1] && field->json_key[2] == key[2] &&
           field->json_key[3] == key[3] && field->json_key[4] == key[4] &&
           field->json_key[5] == key[5] && field->json_key[6] == key[6] &&
           field->json_key[7] == key[7];
  case 10u:
    return field->json_key[1] == key[1] && field->json_key[2] == key[2] &&
           field->json_key[3] == key[3] && field->json_key[4] == key[4] &&
           field->json_key[5] == key[5] && field->json_key[6] == key[6] &&
           field->json_key[7] == key[7] && field->json_key[8] == key[8];
  case 11u:
    return field->json_key[1] == key[1] && field->json_key[2] == key[2] &&
           field->json_key[3] == key[3] && field->json_key[4] == key[4] &&
           field->json_key[5] == key[5] && field->json_key[6] == key[6] &&
           field->json_key[7] == key[7] && field->json_key[8] == key[8] &&
           field->json_key[9] == key[9];
  case 12u:
    return field->json_key[1] == key[1] && field->json_key[2] == key[2] &&
           field->json_key[3] == key[3] && field->json_key[4] == key[4] &&
           field->json_key[5] == key[5] && field->json_key[6] == key[6] &&
           field->json_key[7] == key[7] && field->json_key[8] == key[8] &&
           field->json_key[9] == key[9] && field->json_key[10] == key[10];
  case 13u:
    return field->json_key[1] == key[1] && field->json_key[2] == key[2] &&
           field->json_key[3] == key[3] && field->json_key[4] == key[4] &&
           field->json_key[5] == key[5] && field->json_key[6] == key[6] &&
           field->json_key[7] == key[7] && field->json_key[8] == key[8] &&
           field->json_key[9] == key[9] && field->json_key[10] == key[10] &&
           field->json_key[11] == key[11];
  case 14u:
    return field->json_key[1] == key[1] && field->json_key[2] == key[2] &&
           field->json_key[3] == key[3] && field->json_key[4] == key[4] &&
           field->json_key[5] == key[5] && field->json_key[6] == key[6] &&
           field->json_key[7] == key[7] && field->json_key[8] == key[8] &&
           field->json_key[9] == key[9] && field->json_key[10] == key[10] &&
           field->json_key[11] == key[11] && field->json_key[12] == key[12];
  case 15u:
    return field->json_key[1] == key[1] && field->json_key[2] == key[2] &&
           field->json_key[3] == key[3] && field->json_key[4] == key[4] &&
           field->json_key[5] == key[5] && field->json_key[6] == key[6] &&
           field->json_key[7] == key[7] && field->json_key[8] == key[8] &&
           field->json_key[9] == key[9] && field->json_key[10] == key[10] &&
           field->json_key[11] == key[11] && field->json_key[12] == key[12] &&
           field->json_key[13] == key[13];
  case 16u:
    return field->json_key[1] == key[1] && field->json_key[2] == key[2] &&
           field->json_key[3] == key[3] && field->json_key[4] == key[4] &&
           field->json_key[5] == key[5] && field->json_key[6] == key[6] &&
           field->json_key[7] == key[7] && field->json_key[8] == key[8] &&
           field->json_key[9] == key[9] && field->json_key[10] == key[10] &&
           field->json_key[11] == key[11] && field->json_key[12] == key[12] &&
           field->json_key[13] == key[13] && field->json_key[14] == key[14];
  default:
    return memcmp(field->json_key + 1u, key + 1u, key_len - 2u) == 0;
  }
}

static LONEJSON__INLINE size_t
lonejson__field_index(const lonejson_map *map, const lonejson_field *field) {
  if (map == NULL || field == NULL || field < map->fields ||
      field >= map->fields + map->field_count) {
    return SIZE_MAX;
  }
  return (size_t)(field - map->fields);
}

static const lonejson_field *lonejson__find_field(const lonejson_map *map,
                                                  lonejson_frame *frame,
                                                  const char *key,
                                                  size_t key_len) {
  size_t i;

  if (map == NULL) {
    return NULL;
  }
  if (key_len != 0u && frame != NULL &&
      frame->next_field_hint < map->field_count &&
      lonejson__field_key_matches(&map->fields[frame->next_field_hint], key,
                                  key_len)) {
    const lonejson_field *field = &map->fields[frame->next_field_hint];
    frame->next_field_hint++;
    return field;
  }
  for (i = 0; i < map->field_count; ++i) {
    const lonejson_field *field = &map->fields[i];
    if (lonejson__field_key_matches(field, key, key_len)) {
      if (frame != NULL) {
        frame->next_field_hint = i + 1u;
      }
      return field;
    }
  }
  return NULL;
}

static LONEJSON__INLINE void *lonejson__field_ptr(void *base,
                                                  const lonejson_field *field) {
  return (unsigned char *)base + field->struct_offset;
}

static LONEJSON__INLINE const void *
lonejson__field_cptr(const void *base, const lonejson_field *field) {
  return (const unsigned char *)base + field->struct_offset;
}

static size_t lonejson__field_storage_size(const lonejson_field *field) {
  if (field == NULL) {
    return 0u;
  }
  switch (field->kind) {
  case LONEJSON_FIELD_KIND_STRING:
    return (field->storage == LONEJSON_STORAGE_DYNAMIC) ? sizeof(char *)
                                                        : field->fixed_capacity;
  case LONEJSON_FIELD_KIND_STRING_STREAM:
  case LONEJSON_FIELD_KIND_BASE64_STREAM:
    return sizeof(lonejson_spooled);
  case LONEJSON_FIELD_KIND_STRING_SOURCE:
  case LONEJSON_FIELD_KIND_BASE64_SOURCE:
    return sizeof(lonejson_source);
  case LONEJSON_FIELD_KIND_JSON_VALUE:
    return sizeof(lonejson_json_value);
  case LONEJSON_FIELD_KIND_I64:
    return sizeof(lonejson_int64);
  case LONEJSON_FIELD_KIND_U64:
    return sizeof(lonejson_uint64);
  case LONEJSON_FIELD_KIND_F64:
    return sizeof(double);
  case LONEJSON_FIELD_KIND_BOOL:
    return sizeof(bool);
  case LONEJSON_FIELD_KIND_OBJECT:
    return field->submap ? field->submap->struct_size : 0u;
  case LONEJSON_FIELD_KIND_STRING_ARRAY:
    return sizeof(lonejson_string_array);
  case LONEJSON_FIELD_KIND_I64_ARRAY:
    return sizeof(lonejson_i64_array);
  case LONEJSON_FIELD_KIND_U64_ARRAY:
    return sizeof(lonejson_u64_array);
  case LONEJSON_FIELD_KIND_F64_ARRAY:
    return sizeof(lonejson_f64_array);
  case LONEJSON_FIELD_KIND_BOOL_ARRAY:
    return sizeof(lonejson_bool_array);
  case LONEJSON_FIELD_KIND_OBJECT_ARRAY:
    return sizeof(lonejson_object_array);
  default:
    return 0u;
  }
}

static int lonejson__field_fits_map(const lonejson_map *map,
                                    const lonejson_field *field) {
  size_t size;

  if (map == NULL || field == NULL) {
    return 0;
  }
  size = lonejson__field_storage_size(field);
  if (size == 0u) {
    return 0;
  }
  if (field->struct_offset > map->struct_size) {
    return 0;
  }
  return size <= (map->struct_size - field->struct_offset);
}

static lonejson_frame *lonejson__push_frame(lonejson_parser *parser,
                                            lonejson_container_kind kind) {
  lonejson_frame *frame;

  if (parser->options.max_depth != 0 &&
      parser->frame_count >= parser->options.max_depth) {
    lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                        parser->error.offset, parser->error.line,
                        parser->error.column, "maximum nesting depth exceeded");
    parser->failed = 1;
    return NULL;
  }
  if (lonejson__frame_bytes(parser->frame_count + 1u) > parser->workspace_top) {
    lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                        parser->error.offset, parser->error.line,
                        parser->error.column,
                        "parser workspace exhausted by nesting depth");
    parser->failed = 1;
    return NULL;
  }
  frame = &parser->frames[parser->frame_count++];
  memset(frame, 0, sizeof(*frame));
  frame->kind = kind;
  frame->state = (kind == LONEJSON_CONTAINER_OBJECT)
                     ? LONEJSON_FRAME_OBJECT_KEY_OR_END
                     : LONEJSON_FRAME_ARRAY_VALUE_OR_END;
  return frame;
}

static void lonejson__pop_frame(lonejson_parser *parser) {
  lonejson_frame *frame;

  if (parser->frame_count == 0) {
    return;
  }
  frame = &parser->frames[parser->frame_count - 1u];
  if (frame->seen_fields != NULL) {
    parser->workspace_top += frame->seen_bytes;
    frame->seen_fields = NULL;
    frame->seen_bytes = 0u;
  }
  parser->frame_count--;
}

static int lonejson__array_ensure_bytes(void **items_ptr, size_t *cap_ptr,
                                        size_t elem_size, unsigned *flags,
                                        size_t want,
                                        const lonejson_allocator *allocator,
                                        lonejson_overflow_policy policy,
                                        lonejson_error *error) {
  void *next;
  size_t cap;

  if (*cap_ptr >= want) {
    return 1;
  }
  if ((*flags & LONEJSON_ARRAY_FIXED_CAPACITY) != 0u) {
    if (policy == LONEJSON_OVERFLOW_FAIL) {
      lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, error->offset,
                          error->line, error->column,
                          "array capacity exceeded");
      return 0;
    }
    error->truncated = 1;
    return 0;
  }
  cap = (*cap_ptr != 0u) ? *cap_ptr : 4u;
  while (cap < want) {
    if (cap > (SIZE_MAX / 2u)) {
      lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, error->offset,
                          error->line, error->column, "array too large");
      return 0;
    }
    cap *= 2u;
  }
  next = lonejson__owned_realloc(allocator, *items_ptr, cap * elem_size);
  if (next == NULL) {
    lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, error->offset,
                        error->line, error->column, "failed to grow array");
    return 0;
  }
  *items_ptr = next;
  *cap_ptr = cap;
  *flags |= LONEJSON_ARRAY_OWNS_ITEMS;
  return 1;
}

static void lonejson__cleanup_value(const lonejson_field *field, void *ptr);
static void lonejson__reset_map(const lonejson_map *map, void *value);
static void
lonejson__init_map_with_allocator(const lonejson_map *map, void *value,
                                  const lonejson_allocator *allocator);
static void lonejson__init_map(const lonejson_map *map, void *value);
static void lonejson__cleanup_map_checked(const lonejson_map *map, void *value);

static int lonejson__map_layout_is_valid(const lonejson_map *map) {
  size_t i;

  if (map == NULL) {
    return 0;
  }
  for (i = 0u; i < map->field_count; ++i) {
    const lonejson_field *field = &map->fields[i];
    if (!lonejson__field_fits_map(map, field)) {
      return 0;
    }
    if ((field->kind == LONEJSON_FIELD_KIND_OBJECT ||
         field->kind == LONEJSON_FIELD_KIND_OBJECT_ARRAY) &&
        field->submap != NULL &&
        !lonejson__map_layout_is_valid(field->submap)) {
      return 0;
    }
  }
  return 1;
}

static int lonejson__map_may_allocate(const lonejson_map *map) {
  size_t i;

  if (map == NULL) {
    return 0;
  }
  for (i = 0u; i < map->field_count; ++i) {
    const lonejson_field *field = &map->fields[i];

    switch (field->kind) {
    case LONEJSON_FIELD_KIND_STRING:
      if (field->storage == LONEJSON_STORAGE_DYNAMIC) {
        return 1;
      }
      break;
    case LONEJSON_FIELD_KIND_STRING_STREAM:
    case LONEJSON_FIELD_KIND_BASE64_STREAM:
    case LONEJSON_FIELD_KIND_STRING_SOURCE:
    case LONEJSON_FIELD_KIND_BASE64_SOURCE:
    case LONEJSON_FIELD_KIND_JSON_VALUE:
    case LONEJSON_FIELD_KIND_STRING_ARRAY:
      return 1;
    case LONEJSON_FIELD_KIND_OBJECT:
      if (lonejson__map_may_allocate(field->submap)) {
        return 1;
      }
      break;
    case LONEJSON_FIELD_KIND_OBJECT_ARRAY:
      if (field->storage != LONEJSON_STORAGE_FIXED ||
          lonejson__map_may_allocate(field->submap)) {
        return 1;
      }
      break;
    case LONEJSON_FIELD_KIND_I64_ARRAY:
    case LONEJSON_FIELD_KIND_U64_ARRAY:
    case LONEJSON_FIELD_KIND_F64_ARRAY:
    case LONEJSON_FIELD_KIND_BOOL_ARRAY:
      if (field->storage != LONEJSON_STORAGE_FIXED) {
        return 1;
      }
      break;
    default:
      break;
    }
  }
  return 0;
}

static int lonejson__map_is_flat_zeroable(const lonejson_map *map) {
  size_t i;

  if (map == NULL) {
    return 0;
  }
  for (i = 0u; i < map->field_count; ++i) {
    const lonejson_field *field = &map->fields[i];

    switch (field->kind) {
    case LONEJSON_FIELD_KIND_I64:
    case LONEJSON_FIELD_KIND_U64:
    case LONEJSON_FIELD_KIND_F64:
    case LONEJSON_FIELD_KIND_BOOL:
      break;
    case LONEJSON_FIELD_KIND_STRING:
      if (field->storage != LONEJSON_STORAGE_FIXED) {
        return 0;
      }
      break;
    default:
      return 0;
    }
  }
  return 1;
}

static void lonejson__cleanup_map(const lonejson_map *map, void *value) {
  size_t i;

  if (map == NULL || value == NULL) {
    return;
  }
  for (i = 0; i < map->field_count; ++i) {
    const lonejson_field *field = &map->fields[i];
    lonejson__cleanup_value(field, lonejson__field_ptr(value, field));
  }
}

static void lonejson__cleanup_map_checked(const lonejson_map *map,
                                          void *value) {
  if (!lonejson__map_layout_is_valid(map)) {
    return;
  }
  lonejson__cleanup_map(map, value);
}

static void lonejson__reset_scalar_field(const lonejson_field *field,
                                         void *ptr) {
  switch (field->kind) {
  case LONEJSON_FIELD_KIND_I64:
    *(lonejson_int64 *)ptr = 0;
    break;
  case LONEJSON_FIELD_KIND_U64:
    *(lonejson_uint64 *)ptr = 0u;
    break;
  case LONEJSON_FIELD_KIND_F64:
    *(double *)ptr = 0.0;
    break;
  case LONEJSON_FIELD_KIND_BOOL:
    *(bool *)ptr = false;
    break;
  default:
    break;
  }
}

static void lonejson__reset_map(const lonejson_map *map, void *value) {
  size_t i;

  if (map == NULL || value == NULL) {
    return;
  }
  for (i = 0; i < map->field_count; ++i) {
    const lonejson_field *field = &map->fields[i];
    void *ptr;
    ptr = lonejson__field_ptr(value, field);
    switch (field->kind) {
    case LONEJSON_FIELD_KIND_I64:
    case LONEJSON_FIELD_KIND_U64:
    case LONEJSON_FIELD_KIND_F64:
    case LONEJSON_FIELD_KIND_BOOL:
      lonejson__reset_scalar_field(field, ptr);
      break;
    case LONEJSON_FIELD_KIND_STRING:
      if (field->storage == LONEJSON_STORAGE_FIXED &&
          field->fixed_capacity != 0u) {
        ((char *)ptr)[0] = '\0';
        break;
      }
      lonejson__cleanup_value(field, ptr);
      break;
    case LONEJSON_FIELD_KIND_OBJECT:
      if (field->submap != NULL && !lonejson__map_may_allocate(field->submap)) {
        lonejson__reset_map(field->submap, ptr);
        break;
      }
      lonejson__cleanup_value(field, ptr);
      break;
    case LONEJSON_FIELD_KIND_I64_ARRAY:
      if (lonejson__is_exact_fixed_capacity(
              ((lonejson_i64_array *)ptr)->flags)) {
        ((lonejson_i64_array *)ptr)->count = 0u;
        break;
      }
      lonejson__cleanup_value(field, ptr);
      break;
    case LONEJSON_FIELD_KIND_U64_ARRAY:
      if (lonejson__is_exact_fixed_capacity(
              ((lonejson_u64_array *)ptr)->flags)) {
        ((lonejson_u64_array *)ptr)->count = 0u;
        break;
      }
      lonejson__cleanup_value(field, ptr);
      break;
    case LONEJSON_FIELD_KIND_F64_ARRAY:
      if (lonejson__is_exact_fixed_capacity(
              ((lonejson_f64_array *)ptr)->flags)) {
        ((lonejson_f64_array *)ptr)->count = 0u;
        break;
      }
      lonejson__cleanup_value(field, ptr);
      break;
    case LONEJSON_FIELD_KIND_BOOL_ARRAY:
      if (lonejson__is_exact_fixed_capacity(
              ((lonejson_bool_array *)ptr)->flags)) {
        ((lonejson_bool_array *)ptr)->count = 0u;
        break;
      }
      lonejson__cleanup_value(field, ptr);
      break;
    case LONEJSON_FIELD_KIND_OBJECT_ARRAY:
      if (lonejson__is_exact_fixed_capacity(
              ((lonejson_object_array *)ptr)->flags) &&
          !lonejson__map_may_allocate(field->submap)) {
        ((lonejson_object_array *)ptr)->count = 0u;
        break;
      }
      lonejson__cleanup_value(field, ptr);
      break;
    default:
      lonejson__cleanup_value(field, ptr);
      break;
    }
  }
}

static void lonejson__init_value(const lonejson_field *field, void *ptr,
                                 const lonejson_allocator *allocator) {
  switch (field->kind) {
  case LONEJSON_FIELD_KIND_STRING:
    if (field->storage == LONEJSON_STORAGE_DYNAMIC) {
      *(char **)ptr = NULL;
    } else if (field->fixed_capacity != 0u) {
      ((char *)ptr)[0] = '\0';
    }
    break;
  case LONEJSON_FIELD_KIND_STRING_STREAM:
  case LONEJSON_FIELD_KIND_BASE64_STREAM:
    if (lonejson__spooled_is_initialized((const lonejson_spooled *)ptr)) {
      lonejson_spooled_cleanup((lonejson_spooled *)ptr);
    }
    lonejson_spooled_init_with_allocator((lonejson_spooled *)ptr,
                                         field->spool_options, allocator);
    break;
  case LONEJSON_FIELD_KIND_STRING_SOURCE:
  case LONEJSON_FIELD_KIND_BASE64_SOURCE:
    lonejson_source_init((lonejson_source *)ptr);
    break;
  case LONEJSON_FIELD_KIND_JSON_VALUE:
    if (lonejson__json_value_is_initialized((const lonejson_json_value *)ptr)) {
      lonejson_json_value_cleanup((lonejson_json_value *)ptr);
    }
    lonejson_json_value_init_with_allocator((lonejson_json_value *)ptr,
                                            allocator);
    break;
  case LONEJSON_FIELD_KIND_I64:
  case LONEJSON_FIELD_KIND_U64:
  case LONEJSON_FIELD_KIND_F64:
  case LONEJSON_FIELD_KIND_BOOL:
    lonejson__reset_scalar_field(field, ptr);
    break;
  case LONEJSON_FIELD_KIND_OBJECT:
    if (lonejson__map_is_flat_zeroable(field->submap)) {
      memset(ptr, 0, field->submap->struct_size);
      break;
    }
    lonejson__init_map_with_allocator(field->submap, ptr, allocator);
    break;
  case LONEJSON_FIELD_KIND_STRING_ARRAY: {
    lonejson_string_array *arr = (lonejson_string_array *)ptr;
    memset(arr, 0, sizeof(*arr));
    break;
  }
  case LONEJSON_FIELD_KIND_I64_ARRAY: {
    lonejson_i64_array *arr = (lonejson_i64_array *)ptr;
    memset(arr, 0, sizeof(*arr));
    break;
  }
  case LONEJSON_FIELD_KIND_U64_ARRAY: {
    lonejson_u64_array *arr = (lonejson_u64_array *)ptr;
    memset(arr, 0, sizeof(*arr));
    break;
  }
  case LONEJSON_FIELD_KIND_F64_ARRAY: {
    lonejson_f64_array *arr = (lonejson_f64_array *)ptr;
    memset(arr, 0, sizeof(*arr));
    break;
  }
  case LONEJSON_FIELD_KIND_BOOL_ARRAY: {
    lonejson_bool_array *arr = (lonejson_bool_array *)ptr;
    memset(arr, 0, sizeof(*arr));
    break;
  }
  case LONEJSON_FIELD_KIND_OBJECT_ARRAY: {
    lonejson_object_array *arr = (lonejson_object_array *)ptr;
    memset(arr, 0, sizeof(*arr));
    arr->elem_size = field->elem_size;
    break;
  }
  default:
    break;
  }
}

static void
lonejson__init_map_with_allocator(const lonejson_map *map, void *value,
                                  const lonejson_allocator *allocator) {
  size_t i;

  if (map == NULL || value == NULL) {
    return;
  }
  for (i = 0; i < map->field_count; ++i) {
    const lonejson_field *field = &map->fields[i];
    if (!lonejson__field_fits_map(map, field)) {
      continue;
    }
    lonejson__init_value(field, lonejson__field_ptr(value, field), allocator);
  }
}

static void lonejson__init_map(const lonejson_map *map, void *value) {
  lonejson__init_map_with_allocator(map, value, NULL);
}

static void lonejson__cleanup_value(const lonejson_field *field, void *ptr) {
  size_t i;

  switch (field->kind) {
  case LONEJSON_FIELD_KIND_STRING:
    if (field->storage == LONEJSON_STORAGE_DYNAMIC) {
      char **p = (char **)ptr;
      lonejson__owned_free(*p);
      *p = NULL;
    } else if (field->fixed_capacity != 0u) {
      ((char *)ptr)[0] = '\0';
    }
    break;
  case LONEJSON_FIELD_KIND_STRING_STREAM:
  case LONEJSON_FIELD_KIND_BASE64_STREAM:
    lonejson_spooled_reset((lonejson_spooled *)ptr);
    break;
  case LONEJSON_FIELD_KIND_STRING_SOURCE:
  case LONEJSON_FIELD_KIND_BASE64_SOURCE:
    lonejson_source_cleanup((lonejson_source *)ptr);
    lonejson_source_init((lonejson_source *)ptr);
    break;
  case LONEJSON_FIELD_KIND_JSON_VALUE:
    lonejson_json_value_cleanup((lonejson_json_value *)ptr);
    break;
  case LONEJSON_FIELD_KIND_OBJECT:
    lonejson__cleanup_map(field->submap, ptr);
    memset(ptr, 0, field->submap ? field->submap->struct_size : 0u);
    break;
  case LONEJSON_FIELD_KIND_STRING_ARRAY: {
    lonejson_string_array *arr = (lonejson_string_array *)ptr;
    char **items = arr->items;
    size_t capacity = arr->capacity;
    unsigned flags = arr->flags;
    for (i = 0; i < arr->count; ++i) {
      lonejson__owned_free(arr->items[i]);
    }
    if ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) != 0u) {
      lonejson__owned_free(arr->items);
    }
    memset(arr, 0, sizeof(*arr));
    if ((flags & LONEJSON_ARRAY_FIXED_CAPACITY) != 0u) {
      arr->items = items;
      arr->capacity = capacity;
      arr->flags = flags & LONEJSON_ARRAY_FIXED_CAPACITY;
    }
    break;
  }
  case LONEJSON_FIELD_KIND_I64_ARRAY: {
    lonejson_i64_array *arr = (lonejson_i64_array *)ptr;
    lonejson_int64 *items = arr->items;
    size_t capacity = arr->capacity;
    unsigned flags = arr->flags;
    if ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) != 0u) {
      lonejson__owned_free(arr->items);
    }
    memset(arr, 0, sizeof(*arr));
    if ((flags & LONEJSON_ARRAY_FIXED_CAPACITY) != 0u) {
      arr->items = items;
      arr->capacity = capacity;
      arr->flags = flags & LONEJSON_ARRAY_FIXED_CAPACITY;
    }
    break;
  }
  case LONEJSON_FIELD_KIND_U64_ARRAY: {
    lonejson_u64_array *arr = (lonejson_u64_array *)ptr;
    lonejson_uint64 *items = arr->items;
    size_t capacity = arr->capacity;
    unsigned flags = arr->flags;
    if ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) != 0u) {
      lonejson__owned_free(arr->items);
    }
    memset(arr, 0, sizeof(*arr));
    if ((flags & LONEJSON_ARRAY_FIXED_CAPACITY) != 0u) {
      arr->items = items;
      arr->capacity = capacity;
      arr->flags = flags & LONEJSON_ARRAY_FIXED_CAPACITY;
    }
    break;
  }
  case LONEJSON_FIELD_KIND_F64_ARRAY: {
    lonejson_f64_array *arr = (lonejson_f64_array *)ptr;
    double *items = arr->items;
    size_t capacity = arr->capacity;
    unsigned flags = arr->flags;
    if ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) != 0u) {
      lonejson__owned_free(arr->items);
    }
    memset(arr, 0, sizeof(*arr));
    if ((flags & LONEJSON_ARRAY_FIXED_CAPACITY) != 0u) {
      arr->items = items;
      arr->capacity = capacity;
      arr->flags = flags & LONEJSON_ARRAY_FIXED_CAPACITY;
    }
    break;
  }
  case LONEJSON_FIELD_KIND_BOOL_ARRAY: {
    lonejson_bool_array *arr = (lonejson_bool_array *)ptr;
    bool *items = arr->items;
    size_t capacity = arr->capacity;
    unsigned flags = arr->flags;
    if ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) != 0u) {
      lonejson__owned_free(arr->items);
    }
    memset(arr, 0, sizeof(*arr));
    if ((flags & LONEJSON_ARRAY_FIXED_CAPACITY) != 0u) {
      arr->items = items;
      arr->capacity = capacity;
      arr->flags = flags & LONEJSON_ARRAY_FIXED_CAPACITY;
    }
    break;
  }
  case LONEJSON_FIELD_KIND_OBJECT_ARRAY: {
    lonejson_object_array *arr = (lonejson_object_array *)ptr;
    void *items = arr->items;
    size_t capacity = arr->capacity;
    size_t elem_size = arr->elem_size;
    unsigned flags = arr->flags;
    if (field->submap != NULL && arr->items != NULL) {
      for (i = 0; i < arr->count; ++i) {
        void *elem = (unsigned char *)arr->items + (i * field->elem_size);
        lonejson__cleanup_map(field->submap, elem);
      }
    }
    if ((arr->flags & LONEJSON_ARRAY_OWNS_ITEMS) != 0u) {
      lonejson__owned_free(arr->items);
    }
    memset(arr, 0, sizeof(*arr));
    if ((flags & LONEJSON_ARRAY_FIXED_CAPACITY) != 0u) {
      arr->items = items;
      arr->capacity = capacity;
      arr->elem_size = elem_size;
      arr->flags = flags & LONEJSON_ARRAY_FIXED_CAPACITY;
    }
    break;
  }
  default:
    break;
  }
}

static lonejson_status lonejson__assign_string(lonejson_parser *parser,
                                               const lonejson_field *field,
                                               void *ptr, const char *value,
                                               size_t len) {
  if (memchr(value, '\0', len) != NULL) {
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
        parser->error.line, parser->error.column,
        "field '%s' contains embedded NUL unsupported by C strings",
        field->json_key);
  }
  if (field->storage == LONEJSON_STORAGE_DYNAMIC) {
    char **dst = (char **)ptr;
    char *copy = (char *)lonejson__owned_malloc(&parser->allocator, len + 1u);
    if (copy == NULL) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
          parser->error.offset, parser->error.line, parser->error.column,
          "failed to allocate string");
    }
    memcpy(copy, value, len);
    copy[len] = '\0';
    lonejson__owned_free(*dst);
    *dst = copy;
    return LONEJSON_STATUS_OK;
  }
  if (field->fixed_capacity == 0u) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_ARGUMENT,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "fixed string field has zero capacity");
  }
  if (len + 1u > field->fixed_capacity) {
    if (field->overflow_policy == LONEJSON_OVERFLOW_FAIL) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_OVERFLOW, parser->error.offset,
          parser->error.line, parser->error.column,
          "string field '%s' exceeds fixed capacity", field->json_key);
    }
    memcpy(ptr, value, field->fixed_capacity - 1u);
    ((char *)ptr)[field->fixed_capacity - 1u] = '\0';
    parser->error.truncated = 1;
    return (field->overflow_policy == LONEJSON_OVERFLOW_TRUNCATE)
               ? LONEJSON_STATUS_TRUNCATED
               : LONEJSON_STATUS_OK;
  }
  memcpy(ptr, value, len);
  ((char *)ptr)[len] = '\0';
  return LONEJSON_STATUS_OK;
}

static int lonejson__parse_i64_token(const char *value, lonejson_int64 *out) {
  const unsigned char *p;
  lonejson_uint64 max_u64;
  lonejson_uint64 limit;
  lonejson_uint64 acc;
  int negative;

  if (value == NULL || out == NULL || *value == '\0') {
    return 0;
  }
  p = (const unsigned char *)value;
  negative = (*p == '-');
  if (negative) {
    p++;
    if (*p == '\0') {
      return 0;
    }
  }
  if (*p == '0' && p[1] != '\0') {
    return 0;
  }
  max_u64 = (lonejson_uint64) ~(lonejson_uint64)0u;
  limit = negative ? ((max_u64 >> 1u) + 1u) : (max_u64 >> 1u);
  acc = 0u;
  while (*p != '\0') {
    lonejson_uint64 digit;

    if (*p < '0' || *p > '9') {
      return 0;
    }
    digit = (lonejson_uint64)(*p - '0');
    if (acc > (limit - digit) / 10u) {
      return 0;
    }
    acc = (acc * 10u) + digit;
    p++;
  }
  if (negative) {
    if (acc == ((max_u64 >> 1u) + 1u)) {
      *out = (lonejson_int64)(-((lonejson_int64)(acc - 1u)) - 1);
    } else {
      *out = (lonejson_int64)(-((lonejson_int64)acc));
    }
  } else {
    *out = (lonejson_int64)acc;
  }
  return 1;
}

static int lonejson__parse_u64_token(const char *value, lonejson_uint64 *out) {
  const unsigned char *p;
  lonejson_uint64 max_u64;
  lonejson_uint64 acc;

  if (value == NULL || out == NULL || *value == '\0' || *value == '-') {
    return 0;
  }
  p = (const unsigned char *)value;
  if (*p == '0' && p[1] != '\0') {
    return 0;
  }
  max_u64 = (lonejson_uint64) ~(lonejson_uint64)0u;
  acc = 0u;
  while (*p != '\0') {
    lonejson_uint64 digit;

    if (*p < '0' || *p > '9') {
      return 0;
    }
    digit = (lonejson_uint64)(*p - '0');
    if (acc > (max_u64 - digit) / 10u) {
      return 0;
    }
    acc = (acc * 10u) + digit;
    p++;
  }
  *out = acc;
  return 1;
}

static int lonejson__parse_f64_fast(const char *value, size_t len,
                                    double *out) {
  const unsigned char *p;
  const unsigned char *endp;
  double int_part;
  double frac_part;
  double scale;
  unsigned exponent;
  int exponent_negative;
  int negative;

  if (value == NULL || out == NULL || len == 0u || *value == '\0') {
    return 0;
  }
  p = (const unsigned char *)value;
  endp = p + len;
  negative = (*p == '-');
  if (negative) {
    p++;
    if (p == endp || *p == '\0') {
      return 0;
    }
  }
  if (p == endp) {
    return 0;
  }
  if (!lonejson__is_digit(*p)) {
    return 0;
  }
  if (*p == '0' && p + 1u < endp && p[1] != '.' && p[1] != 'e' && p[1] != 'E') {
    return 0;
  }
  int_part = 0.0;
  while (p < endp && lonejson__is_digit(*p)) {
    int_part = (int_part * 10.0) + (double)(*p - '0');
    p++;
  }
  frac_part = 0.0;
  if (p < endp && *p == '.') {
    unsigned frac_digits;

    p++;
    frac_digits = 0u;
    if (p == endp || !lonejson__is_digit(*p)) {
      return 0;
    }
    while (p < endp && lonejson__is_digit(*p)) {
      frac_part = (frac_part * 10.0) + (double)(*p - '0');
      frac_digits++;
      p++;
      if (frac_digits >= 17u && p < endp && lonejson__is_digit(*p)) {
        return 0;
      }
    }
    while (frac_digits-- != 0u) {
      frac_part /= 10.0;
    }
  }
  exponent = 0u;
  exponent_negative = 0;
  if (p < endp && (*p == 'e' || *p == 'E')) {
    p++;
    if (p == endp) {
      return 0;
    }
    if (*p == '+' || *p == '-') {
      exponent_negative = (*p == '-');
      p++;
      if (p == endp) {
        return 0;
      }
    }
    if (!lonejson__is_digit(*p)) {
      return 0;
    }
    while (p < endp && lonejson__is_digit(*p)) {
      if (exponent > 10000u) {
        return 0;
      }
      exponent = (exponent * 10u) + (unsigned)(*p - '0');
      p++;
    }
  }
  if (p != endp || *p != '\0') {
    return 0;
  }
  scale = int_part + frac_part;
  if (exponent != 0u) {
    double base;

    base = exponent_negative ? 0.1 : 10.0;
    while (exponent != 0u) {
      if ((exponent & 1u) != 0u) {
        scale *= base;
      }
      exponent >>= 1u;
      if (exponent != 0u) {
        base *= base;
      }
    }
    if (!lonejson__is_finite_f64(scale)) {
      return 0;
    }
  }
  *out = negative ? -scale : scale;
  return 1;
}

static lonejson_status lonejson__assign_i64(lonejson_parser *parser,
                                            const lonejson_field *field,
                                            void *ptr, const char *value) {
  lonejson_int64 parsed;

  (void)field;
  if (!lonejson__parse_i64_token(value, &parsed)) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "expected integer for '%s'", field->json_key);
  }
  *(lonejson_int64 *)ptr = (lonejson_int64)parsed;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__assign_u64(lonejson_parser *parser,
                                            const lonejson_field *field,
                                            void *ptr, const char *value) {
  lonejson_uint64 parsed;

  if (!lonejson__parse_u64_token(value, &parsed)) {
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
        parser->error.line, parser->error.column,
        "expected unsigned integer for '%s'", field->json_key);
  }
  *(lonejson_uint64 *)ptr = (lonejson_uint64)parsed;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__assign_f64(lonejson_parser *parser,
                                            const lonejson_field *field,
                                            void *ptr, const char *value,
                                            size_t len) {
  char *end = NULL;
  double parsed;

  if (!lonejson__parse_f64_fast(value, len, &parsed)) {
    if (!lonejson__is_valid_json_number(value, len)) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "expected number for '%s'", field->json_key);
    }
    errno = 0;
    parsed = strtod(value, &end);
    if (errno != 0 || end == value || *end != '\0') {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "expected number for '%s'", field->json_key);
    }
  }
  *(double *)ptr = parsed;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__assign_bool(lonejson_parser *parser,
                                             const lonejson_field *field,
                                             void *ptr, int value) {
  (void)parser;
  (void)field;
  *(bool *)ptr = value ? true : false;
  return LONEJSON_STATUS_OK;
}

static LONEJSON__INLINE int
lonejson__mark_field_seen(lonejson_parser *parser, lonejson_frame *frame,
                          const lonejson_field *field) {
  size_t idx;
  lonejson_uint64 bit;
  if (frame == NULL || frame->map == NULL || field == NULL) {
    return 1;
  }
  idx = lonejson__field_index(frame->map, field);
  if (idx == SIZE_MAX) {
    return 1;
  }
  if (frame->map->field_count <= 64u) {
    bit = ((lonejson_uint64)1u) << idx;
    if (parser->options.reject_duplicate_keys &&
        (frame->seen_mask & bit) != 0u) {
      lonejson__set_error(&parser->error, LONEJSON_STATUS_DUPLICATE_FIELD,
                          parser->error.offset, parser->error.line,
                          parser->error.column, "duplicate field '%s'",
                          field->json_key);
      parser->failed = 1;
      return 0;
    }
    if ((frame->seen_mask & bit) == 0u &&
        (field->flags & LONEJSON_FIELD_REQUIRED) != 0u &&
        frame->required_remaining != 0u) {
      frame->required_remaining--;
    }
    frame->seen_mask |= bit;
    return 1;
  }
  if (frame->seen_fields == NULL && frame->map->field_count != 0u) {
    frame->seen_fields =
        lonejson__workspace_alloc_top(parser, frame->map->field_count);
    if (frame->seen_fields == NULL) {
      lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                          parser->error.offset, parser->error.line,
                          parser->error.column,
                          "parser workspace exhausted by object bookkeeping");
      parser->failed = 1;
      return 0;
    }
    frame->seen_bytes = lonejson__workspace_align(frame->map->field_count);
    memset(frame->seen_fields, 0, frame->map->field_count);
  }
  if (parser->options.reject_duplicate_keys && frame->seen_fields[idx] != 0u) {
    lonejson__set_error(&parser->error, LONEJSON_STATUS_DUPLICATE_FIELD,
                        parser->error.offset, parser->error.line,
                        parser->error.column, "duplicate field '%s'",
                        field->json_key);
    parser->failed = 1;
    return 0;
  }
  if (frame->seen_fields[idx] == 0u &&
      (field->flags & LONEJSON_FIELD_REQUIRED) != 0u &&
      frame->required_remaining != 0u) {
    frame->required_remaining--;
  }
  frame->seen_fields[idx] = 1u;
  return 1;
}

static lonejson_status lonejson__assign_null(lonejson_parser *parser,
                                             const lonejson_field *field,
                                             void *ptr) {
  switch (field->kind) {
  case LONEJSON_FIELD_KIND_STRING:
  case LONEJSON_FIELD_KIND_STRING_STREAM:
  case LONEJSON_FIELD_KIND_BASE64_STREAM:
    if (field->storage == LONEJSON_STORAGE_DYNAMIC) {
      char **dst = (char **)ptr;
      lonejson__owned_free(*dst);
      *dst = NULL;
    } else if (field->kind == LONEJSON_FIELD_KIND_STRING &&
               field->fixed_capacity != 0u) {
      ((char *)ptr)[0] = '\0';
    } else {
      lonejson_spooled_reset((lonejson_spooled *)ptr);
    }
    return LONEJSON_STATUS_OK;
  case LONEJSON_FIELD_KIND_STRING_SOURCE:
  case LONEJSON_FIELD_KIND_BASE64_SOURCE:
    lonejson_source_reset((lonejson_source *)ptr);
    return LONEJSON_STATUS_OK;
  case LONEJSON_FIELD_KIND_JSON_VALUE:
    lonejson_json_value_reset((lonejson_json_value *)ptr);
    return LONEJSON_STATUS_OK;
  case LONEJSON_FIELD_KIND_OBJECT:
    if (field->submap != NULL) {
      lonejson__cleanup_map(field->submap, ptr);
      memset(ptr, 0, field->submap->struct_size);
    }
    return LONEJSON_STATUS_OK;
  case LONEJSON_FIELD_KIND_STRING_ARRAY:
  case LONEJSON_FIELD_KIND_I64_ARRAY:
  case LONEJSON_FIELD_KIND_U64_ARRAY:
  case LONEJSON_FIELD_KIND_F64_ARRAY:
  case LONEJSON_FIELD_KIND_BOOL_ARRAY:
  case LONEJSON_FIELD_KIND_OBJECT_ARRAY:
    lonejson__cleanup_value(field, ptr);
    return LONEJSON_STATUS_OK;
  default:
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "null is not allowed for '%s'", field->json_key);
  }
}

static lonejson_status lonejson__json_value_emit(lonejson_parser *parser,
                                                 const void *data, size_t len) {
  lonejson_json_value *value;

  value = parser->json_stream_value;
  if (value == NULL) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "JSON value stream has no destination");
  }
  if (value->parse_mode == LONEJSON_JSON_VALUE_PARSE_SINK) {
    return value->parse_sink(value->parse_sink_user, data, len, &parser->error);
  }
  if (value->parse_mode == LONEJSON_JSON_VALUE_PARSE_CAPTURE) {
    size_t required;
    size_t capacity;
    size_t next_cap;
    char *next;

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
        return lonejson__set_error(
            &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
            parser->error.offset, parser->error.line, parser->error.column,
            "failed to grow captured JSON value");
      }
      value->json = next;
    }
    memcpy(value->json + value->len, data, len);
    value->len += len;
    value->json[value->len] = '\0';
    value->kind = LONEJSON_JSON_VALUE_BUFFER;
    return LONEJSON_STATUS_OK;
  }
  return lonejson__set_error(
      &parser->error, LONEJSON_STATUS_INVALID_ARGUMENT, parser->error.offset,
      parser->error.line, parser->error.column,
      "JSON value field has no configured parse sink or capture mode");
}

static LONEJSON__INLINE int
lonejson__json_value_parse_visitor_active(const lonejson_parser *parser) {
  return parser != NULL && parser->json_stream_visit_active;
}

static LONEJSON__INLINE void
lonejson__parser_set_json_stream_value(lonejson_parser *parser,
                                       lonejson_json_value *value) {
  if (parser == NULL) {
    return;
  }
  parser->json_stream_value = value;
  parser->json_stream_active = (value != NULL);
  parser->json_stream_visit_active =
      value != NULL && value->parse_mode == LONEJSON_JSON_VALUE_PARSE_VISITOR &&
      value->parse_visitor != NULL;
  parser->json_stream_sink_active =
      value != NULL && value->parse_mode == LONEJSON_JSON_VALUE_PARSE_SINK;
}

static LONEJSON__INLINE void
lonejson__parser_clear_json_stream_value(lonejson_parser *parser) {
  if (parser == NULL) {
    return;
  }
  parser->json_stream_value = NULL;
  parser->json_stream_active = 0;
  parser->json_stream_visit_active = 0;
  parser->json_stream_sink_active = 0;
}

static lonejson_status
lonejson__json_value_visit_event(lonejson_parser *parser,
                                 lonejson_value_event_fn fn) {
  lonejson_json_value *value = parser->json_stream_value;

  if (!lonejson__json_value_parse_visitor_active(parser) || fn == NULL) {
    return LONEJSON_STATUS_OK;
  }
  return fn(value->parse_visitor_user, &parser->error);
}

static lonejson_status
lonejson__json_value_visit_chunk(lonejson_parser *parser,
                                 lonejson_value_chunk_fn fn, const char *data,
                                 size_t len, size_t *token_bytes,
                                 size_t token_limit, const char *limit_msg) {
  lonejson_json_value *value = parser->json_stream_value;

  if (!lonejson__json_value_parse_visitor_active(parser) || len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  if (token_limit != 0u && token_bytes != NULL &&
      *token_bytes + len > token_limit) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                               parser->error.offset, parser->error.line,
                               parser->error.column, "%s", limit_msg);
  }
  if (value->parse_visitor_limits.max_total_bytes != 0u &&
      parser->json_stream_total_bytes + len >
          value->parse_visitor_limits.max_total_bytes) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "JSON value exceeds maximum total byte limit");
  }
  if (token_bytes != NULL) {
    *token_bytes += len;
  }
  parser->json_stream_total_bytes += len;
  if (fn == NULL) {
    return LONEJSON_STATUS_OK;
  }
  return fn(value->parse_visitor_user, data, len, &parser->error);
}

static lonejson_status lonejson__json_value_visit_bool(lonejson_parser *parser,
                                                       int boolean_value) {
  lonejson_json_value *value = parser->json_stream_value;

  if (!lonejson__json_value_parse_visitor_active(parser)) {
    return LONEJSON_STATUS_OK;
  }
  if (value->parse_visitor->boolean_value == NULL) {
    return LONEJSON_STATUS_OK;
  }
  return value->parse_visitor->boolean_value(value->parse_visitor_user,
                                             boolean_value, &parser->error);
}

static lonejson_status
lonejson__json_value_string_begin(lonejson_parser *parser, int is_key) {
  lonejson_json_value *value = parser->json_stream_value;

  if (!lonejson__json_value_parse_visitor_active(parser)) {
    return LONEJSON_STATUS_OK;
  }
  parser->json_stream_text_bytes = 0u;
  parser->json_stream_text_is_key = is_key;
  return lonejson__json_value_visit_event(
      parser, is_key ? value->parse_visitor->object_key_begin
                     : value->parse_visitor->string_begin);
}

static lonejson_status
lonejson__json_value_string_chunk(lonejson_parser *parser, const char *data,
                                  size_t len) {
  lonejson_json_value *value = parser->json_stream_value;
  size_t limit;
  const char *msg;

  if (!lonejson__json_value_parse_visitor_active(parser)) {
    return LONEJSON_STATUS_OK;
  }
  limit = parser->json_stream_text_is_key
              ? value->parse_visitor_limits.max_key_bytes
              : value->parse_visitor_limits.max_string_bytes;
  msg = parser->json_stream_text_is_key
            ? "JSON object key exceeds maximum decoded byte limit"
            : "JSON string exceeds maximum decoded byte limit";
  return lonejson__json_value_visit_chunk(
      parser,
      parser->json_stream_text_is_key ? value->parse_visitor->object_key_chunk
                                      : value->parse_visitor->string_chunk,
      data, len, &parser->json_stream_text_bytes, limit, msg);
}

static lonejson_status
lonejson__json_value_string_end(lonejson_parser *parser) {
  lonejson_json_value *value = parser->json_stream_value;
  int is_key = parser->json_stream_text_is_key;

  if (!lonejson__json_value_parse_visitor_active(parser)) {
    return LONEJSON_STATUS_OK;
  }
  parser->json_stream_text_bytes = 0u;
  parser->json_stream_text_is_key = 0;
  return lonejson__json_value_visit_event(
      parser, is_key ? value->parse_visitor->object_key_end
                     : value->parse_visitor->string_end);
}

static lonejson_status lonejson__json_value_number(lonejson_parser *parser,
                                                   const char *text,
                                                   size_t len) {
  lonejson_json_value *value = parser->json_stream_value;
  lonejson_status status;
  size_t count = 0u;

  if (!lonejson__json_value_parse_visitor_active(parser)) {
    return LONEJSON_STATUS_OK;
  }
  status = lonejson__json_value_visit_event(parser,
                                            value->parse_visitor->number_begin);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  status = lonejson__json_value_visit_chunk(
      parser, value->parse_visitor->number_chunk, text, len, &count,
      value->parse_visitor_limits.max_number_bytes,
      "JSON number exceeds maximum byte limit");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  return lonejson__json_value_visit_event(parser,
                                          value->parse_visitor->number_end);
}

static lonejson_status lonejson__json_value_emit_string(lonejson_parser *parser,
                                                        const char *value,
                                                        size_t len) {
  lonejson_status status;

  status = lonejson__json_value_emit(parser, "\"", 1u);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__emit_escaped_fragment(
      parser->json_stream_sink_active ? parser->json_stream_value->parse_sink
                                      : lonejson__json_buffer_sink,
      parser->json_stream_sink_active
          ? parser->json_stream_value->parse_sink_user
          : parser->json_stream_value,
      &parser->error, (const unsigned char *)value, len);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  return lonejson__json_value_emit(parser, "\"", 1u);
}

static lonejson_status lonejson__assign_json_scalar(lonejson_parser *parser,
                                                    lonejson_json_value *value,
                                                    const char *text,
                                                    size_t len,
                                                    lonejson_lex_mode mode) {
  lonejson_status status;

  status = lonejson__json_value_prepare_parse(parser, value, &parser->error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  lonejson__parser_set_json_stream_value(parser, value);
  parser->json_stream_depth = 0u;
  parser->json_stream_total_bytes = 0u;
  parser->json_stream_text_bytes = 0u;
  parser->json_stream_text_is_key = 0;
  if (value->parse_mode == LONEJSON_JSON_VALUE_PARSE_VISITOR) {
    switch (mode) {
    case LONEJSON_LEX_STRING:
      status = lonejson__json_value_string_begin(parser, 0);
      if (status == LONEJSON_STATUS_OK) {
        status = lonejson__json_value_string_chunk(parser, text, len);
      }
      if (status == LONEJSON_STATUS_OK) {
        status = lonejson__json_value_string_end(parser);
      }
      break;
    case LONEJSON_LEX_NUMBER:
      status = lonejson__json_value_number(parser, text, len);
      break;
    case LONEJSON_LEX_TRUE:
      status = lonejson__json_value_visit_bool(parser, 1);
      break;
    case LONEJSON_LEX_FALSE:
      status = lonejson__json_value_visit_bool(parser, 0);
      break;
    case LONEJSON_LEX_NULL:
      status = lonejson__json_value_visit_event(
          parser, value->parse_visitor->null_value);
      break;
    default:
      status = lonejson__set_error(
          &parser->error, LONEJSON_STATUS_INTERNAL_ERROR, parser->error.offset,
          parser->error.line, parser->error.column,
          "unexpected JSON scalar mode");
      break;
    }
    lonejson__parser_clear_json_stream_value(parser);
    parser->json_stream_depth = 0u;
    return status;
  }
  switch (mode) {
  case LONEJSON_LEX_STRING:
    status = lonejson__json_value_emit_string(parser, text, len);
    break;
  case LONEJSON_LEX_NUMBER:
  case LONEJSON_LEX_TRUE:
  case LONEJSON_LEX_FALSE:
    status = lonejson__json_value_emit(parser, text, len);
    break;
  case LONEJSON_LEX_NULL:
    status = lonejson__json_value_emit(parser, "null", 4u);
    break;
  default:
    status = lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "unexpected JSON scalar mode");
    break;
  }
  lonejson__parser_clear_json_stream_value(parser);
  return status;
}

static lonejson_status lonejson__array_append_string(
    lonejson_parser *parser, const lonejson_field *field,
    lonejson_string_array *arr, const char *value, size_t len) {
  char *copy;

  if (memchr(value, '\0', len) != NULL) {
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
        parser->error.line, parser->error.column,
        "array '%s' contains embedded NUL unsupported by C strings",
        field->json_key);
  }
  if (!lonejson__array_ensure_bytes((void **)&arr->items, &arr->capacity,
                                    sizeof(char *), &arr->flags,
                                    arr->count + 1u, &parser->allocator,
                                    field->overflow_policy, &parser->error)) {
    return (field->overflow_policy == LONEJSON_OVERFLOW_FAIL)
               ? parser->error.code
               : LONEJSON_STATUS_OK;
  }
  if (arr->count >= arr->capacity) {
    parser->error.truncated = 1;
    return (field->overflow_policy == LONEJSON_OVERFLOW_TRUNCATE)
               ? LONEJSON_STATUS_TRUNCATED
               : LONEJSON_STATUS_OK;
  }
  copy = (char *)lonejson__owned_malloc(&parser->allocator, len + 1u);
  if (copy == NULL) {
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED, parser->error.offset,
        parser->error.line, parser->error.column,
        "failed to allocate array string");
  }
  memcpy(copy, value, len);
  copy[len] = '\0';
  arr->items[arr->count++] = copy;
  return LONEJSON_STATUS_OK;
}

#define LONEJSON__DEFINE_ARRAY_APPEND(name, array_type, elem_type)             \
  static lonejson_status name(lonejson_parser *parser,                         \
                              const lonejson_field *field, array_type *arr,    \
                              elem_type value) {                               \
    if (!lonejson__array_ensure_bytes(                                         \
            (void **)&arr->items, &arr->capacity, sizeof(elem_type),           \
            &arr->flags, arr->count + 1u, &parser->allocator,                  \
            field->overflow_policy, &parser->error)) {                         \
      return (field->overflow_policy == LONEJSON_OVERFLOW_FAIL)                \
                 ? parser->error.code                                          \
                 : LONEJSON_STATUS_OK;                                         \
    }                                                                          \
    if (arr->count >= arr->capacity) {                                         \
      parser->error.truncated = 1;                                             \
      return (field->overflow_policy == LONEJSON_OVERFLOW_TRUNCATE)            \
                 ? LONEJSON_STATUS_TRUNCATED                                   \
                 : LONEJSON_STATUS_OK;                                         \
    }                                                                          \
    arr->items[arr->count++] = value;                                          \
    return LONEJSON_STATUS_OK;                                                 \
  }

LONEJSON__DEFINE_ARRAY_APPEND(lonejson__array_append_i64, lonejson_i64_array,
                              lonejson_int64)
LONEJSON__DEFINE_ARRAY_APPEND(lonejson__array_append_u64, lonejson_u64_array,
                              lonejson_uint64)
LONEJSON__DEFINE_ARRAY_APPEND(lonejson__array_append_f64, lonejson_f64_array,
                              double)
LONEJSON__DEFINE_ARRAY_APPEND(lonejson__array_append_bool, lonejson_bool_array,
                              bool)

static void *lonejson__object_array_append_slot(lonejson_parser *parser,
                                                const lonejson_field *field,
                                                lonejson_object_array *arr) {
  void *slot;

  if (arr->elem_size == 0u) {
    arr->elem_size = field->elem_size;
  }
  if (!lonejson__array_ensure_bytes(&arr->items, &arr->capacity, arr->elem_size,
                                    &arr->flags, arr->count + 1u,
                                    &parser->allocator, field->overflow_policy,
                                    &parser->error)) {
    return NULL;
  }
  if (arr->count >= arr->capacity) {
    parser->error.truncated = 1;
    return NULL;
  }
  slot = (unsigned char *)arr->items + (arr->count * arr->elem_size);
  if (field->submap != NULL) {
    lonejson__init_map_with_allocator(field->submap, slot, &parser->allocator);
  } else {
    memset(slot, 0, arr->elem_size);
  }
  arr->count++;
  return slot;
}

static LONEJSON__INLINE lonejson_status
lonejson__complete_parent_after_value(lonejson_parser *parser) {
  lonejson_frame *parent;

  if (parser->frame_count == 0u) {
    parser->root_finished = 1;
    return LONEJSON_STATUS_OK;
  }
  parent = &parser->frames[parser->frame_count - 1u];
  if (parent->kind == LONEJSON_CONTAINER_OBJECT) {
    parent->state = LONEJSON_FRAME_OBJECT_COMMA_OR_END;
    parent->pending_field = NULL;
  } else {
    parent->state = LONEJSON_FRAME_ARRAY_COMMA_OR_END;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__handle_scalar_for_field(
    lonejson_parser *parser, lonejson_frame *frame, const lonejson_field *field,
    const char *value, size_t len, lonejson_lex_mode mode) {
  void *ptr;

  if (field == NULL) {
    return lonejson__complete_parent_after_value(parser);
  }
  if (frame != NULL && !lonejson__mark_field_seen(parser, frame, field)) {
    return parser->error.code;
  }
  ptr =
      lonejson__field_ptr(frame ? frame->object_ptr : parser->root_dst, field);
  switch (mode) {
  case LONEJSON_LEX_STRING:
    if (field->kind == LONEJSON_FIELD_KIND_JSON_VALUE) {
      return lonejson__assign_json_scalar(parser, (lonejson_json_value *)ptr,
                                          value, len, mode);
    }
    if (field->kind == LONEJSON_FIELD_KIND_STRING_STREAM ||
        field->kind == LONEJSON_FIELD_KIND_BASE64_STREAM) {
      return LONEJSON_STATUS_OK;
    }
    if (field->kind == LONEJSON_FIELD_KIND_STRING_SOURCE ||
        field->kind == LONEJSON_FIELD_KIND_BASE64_SOURCE) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
          parser->error.line, parser->error.column,
          "field '%s' is serialize-only and cannot be parsed from JSON",
          field->json_key);
    }
    if (field->kind != LONEJSON_FIELD_KIND_STRING) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "expected string field '%s'", field->json_key);
    }
    return lonejson__assign_string(parser, field, ptr, value, len);
  case LONEJSON_LEX_NUMBER:
    if (field->kind == LONEJSON_FIELD_KIND_JSON_VALUE) {
      return lonejson__assign_json_scalar(parser, (lonejson_json_value *)ptr,
                                          value, len, mode);
    }
    if (field->kind == LONEJSON_FIELD_KIND_I64) {
      return lonejson__assign_i64(parser, field, ptr, value);
    }
    if (field->kind == LONEJSON_FIELD_KIND_U64) {
      return lonejson__assign_u64(parser, field, ptr, value);
    }
    if (field->kind == LONEJSON_FIELD_KIND_F64) {
      return lonejson__assign_f64(parser, field, ptr, value, len);
    }
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
        parser->error.line, parser->error.column,
        "numeric value does not match field '%s'", field->json_key);
  case LONEJSON_LEX_TRUE:
  case LONEJSON_LEX_FALSE:
    if (field->kind == LONEJSON_FIELD_KIND_JSON_VALUE) {
      return lonejson__assign_json_scalar(parser, (lonejson_json_value *)ptr,
                                          value, len, mode);
    }
    if (field->kind != LONEJSON_FIELD_KIND_BOOL) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
          parser->error.line, parser->error.column,
          "boolean value does not match field '%s'", field->json_key);
    }
    return lonejson__assign_bool(parser, field, ptr, mode == LONEJSON_LEX_TRUE);
  case LONEJSON_LEX_NULL:
    if (field->kind == LONEJSON_FIELD_KIND_JSON_VALUE) {
      return lonejson__assign_json_scalar(parser, (lonejson_json_value *)ptr,
                                          value, len, mode);
    }
    return lonejson__assign_null(parser, field, ptr);
  default:
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               parser->error.offset, parser->error.line,
                               parser->error.column, "unexpected scalar mode");
  }
}

static lonejson_status
lonejson__handle_array_scalar(lonejson_parser *parser,
                              lonejson_frame *array_frame, const char *value,
                              size_t len, lonejson_lex_mode mode) {
  void *ptr = lonejson__field_ptr(array_frame->object_ptr, array_frame->field);

  switch (array_frame->field->kind) {
  case LONEJSON_FIELD_KIND_STRING_ARRAY:
    if (mode == LONEJSON_LEX_NULL) {
      return lonejson__array_append_string(
          parser, array_frame->field, (lonejson_string_array *)ptr, "", 0u);
    }
    if (mode != LONEJSON_LEX_STRING) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
          parser->error.line, parser->error.column,
          "array '%s' expects strings", array_frame->field->json_key);
    }
    return lonejson__array_append_string(
        parser, array_frame->field, (lonejson_string_array *)ptr, value, len);
  case LONEJSON_FIELD_KIND_I64_ARRAY:
    if (mode != LONEJSON_LEX_NUMBER) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
          parser->error.line, parser->error.column,
          "array '%s' expects integers", array_frame->field->json_key);
    } else {
      lonejson_int64 parsed = 0;
      lonejson_status status =
          lonejson__assign_i64(parser, array_frame->field, &parsed, value);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      return lonejson__array_append_i64(parser, array_frame->field,
                                        (lonejson_i64_array *)ptr, parsed);
    }
  case LONEJSON_FIELD_KIND_U64_ARRAY:
    if (mode != LONEJSON_LEX_NUMBER) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
          parser->error.line, parser->error.column,
          "array '%s' expects unsigned integers", array_frame->field->json_key);
    } else {
      lonejson_uint64 parsed = 0;
      lonejson_status status =
          lonejson__assign_u64(parser, array_frame->field, &parsed, value);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      return lonejson__array_append_u64(parser, array_frame->field,
                                        (lonejson_u64_array *)ptr, parsed);
    }
  case LONEJSON_FIELD_KIND_F64_ARRAY:
    if (mode != LONEJSON_LEX_NUMBER) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
          parser->error.line, parser->error.column,
          "array '%s' expects numbers", array_frame->field->json_key);
    } else {
      double parsed = 0.0;
      lonejson_status status =
          lonejson__assign_f64(parser, array_frame->field, &parsed, value, len);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      return lonejson__array_append_f64(parser, array_frame->field,
                                        (lonejson_f64_array *)ptr, parsed);
    }
  case LONEJSON_FIELD_KIND_BOOL_ARRAY:
    if (mode != LONEJSON_LEX_TRUE && mode != LONEJSON_LEX_FALSE) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
          parser->error.line, parser->error.column,
          "array '%s' expects booleans", array_frame->field->json_key);
    }
    return lonejson__array_append_bool(parser, array_frame->field,
                                       (lonejson_bool_array *)ptr,
                                       mode == LONEJSON_LEX_TRUE);
  default:
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "field '%s' does not accept scalar array values",
                               array_frame->field->json_key);
  }
}

static LONEJSON__INLINE lonejson_status
lonejson__begin_object_value(lonejson_parser *parser) {
  lonejson_frame *parent = (parser->frame_count != 0u)
                               ? &parser->frames[parser->frame_count - 1u]
                               : NULL;
  const lonejson_field *field = NULL;
  void *object_ptr = NULL;
  const lonejson_map *map = NULL;
  lonejson_frame *frame;
  int started_capture = 0;

  if (!parser->root_started) {
    parser->root_started = 1;
    map = parser->validate_only ? NULL : parser->root_map;
    object_ptr = parser->validate_only ? NULL : parser->root_dst;
  } else if (parent != NULL && parent->kind == LONEJSON_CONTAINER_OBJECT) {
    parent->after_comma = 0;
    field = parent->pending_field;
    if (field == NULL) {
      map = NULL;
      object_ptr = NULL;
    } else {
      if (!lonejson__mark_field_seen(parser, parent, field)) {
        return parser->error.code;
      }
      if (field->kind == LONEJSON_FIELD_KIND_JSON_VALUE) {
        lonejson_json_value *value = (lonejson_json_value *)lonejson__field_ptr(
            parent->object_ptr, field);
        if (lonejson__json_value_prepare_parse(parser, value, &parser->error) !=
            LONEJSON_STATUS_OK) {
          return parser->error.code;
        }
        lonejson__parser_set_json_stream_value(parser, value);
        parser->json_stream_depth = 1u;
        parser->json_stream_total_bytes = 0u;
        parser->json_stream_text_bytes = 0u;
        parser->json_stream_text_is_key = 0;
        if (value->parse_mode == LONEJSON_JSON_VALUE_PARSE_VISITOR) {
          if (value->parse_visitor_limits.max_depth != 0u &&
              parser->json_stream_depth >
                  value->parse_visitor_limits.max_depth) {
            lonejson__parser_clear_json_stream_value(parser);
            parser->json_stream_depth = 0u;
            return lonejson__set_error(
                &parser->error, LONEJSON_STATUS_OVERFLOW, parser->error.offset,
                parser->error.line, parser->error.column,
                "JSON value nesting exceeds maximum depth");
          }
          if (lonejson__json_value_visit_event(
                  parser, value->parse_visitor->object_begin) !=
              LONEJSON_STATUS_OK) {
            lonejson__parser_clear_json_stream_value(parser);
            parser->json_stream_depth = 0u;
            return parser->error.code;
          }
        } else if (lonejson__json_value_emit(parser, "{", 1u) !=
                   LONEJSON_STATUS_OK) {
          lonejson__parser_clear_json_stream_value(parser);
          parser->json_stream_depth = 0u;
          return parser->error.code;
        }
        map = NULL;
        object_ptr = NULL;
        started_capture = 1;
      } else {
        if (field->kind != LONEJSON_FIELD_KIND_OBJECT) {
          return lonejson__set_error(
              &parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
              parser->error.offset, parser->error.line, parser->error.column,
              "field '%s' expects non-object value", field->json_key);
        }
        map = field->submap;
        object_ptr = lonejson__field_ptr(parent->object_ptr, field);
        lonejson__init_map_with_allocator(map, object_ptr, &parser->allocator);
      }
    }
  } else if (parent != NULL && parent->kind == LONEJSON_CONTAINER_ARRAY) {
    parent->after_comma = 0;
    if (parent->field == NULL) {
      map = NULL;
      object_ptr = NULL;
    } else {
      lonejson_object_array *arr;
      if (parent->field->kind != LONEJSON_FIELD_KIND_OBJECT_ARRAY) {
        return lonejson__set_error(
            &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
            parser->error.line, parser->error.column,
            "array '%s' expects scalar elements", parent->field->json_key);
      }
      arr = (lonejson_object_array *)lonejson__field_ptr(parent->object_ptr,
                                                         parent->field);
      object_ptr =
          lonejson__object_array_append_slot(parser, parent->field, arr);
      if (object_ptr == NULL) {
        if (parent->field->overflow_policy == LONEJSON_OVERFLOW_FAIL) {
          return parser->error.code;
        }
        parser->error.truncated = 1;
        map = NULL;
      } else {
        map = parent->field->submap;
      }
    }
  }

  frame = lonejson__push_frame(parser, LONEJSON_CONTAINER_OBJECT);
  if (frame == NULL) {
    return parser->error.code;
  }
  frame->map = map;
  frame->object_ptr = object_ptr;
  frame->field = field;
  frame->required_remaining = lonejson__required_count_for_map(parser, map);
  if (!started_capture && parser->json_stream_active && map == NULL &&
      field == NULL && parent != NULL) {
    parser->json_stream_depth++;
    if (lonejson__json_value_parse_visitor_active(parser)) {
      if (parser->json_stream_value->parse_visitor_limits.max_depth != 0u &&
          parser->json_stream_depth >
              parser->json_stream_value->parse_visitor_limits.max_depth) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "JSON value nesting exceeds maximum depth");
      }
      return lonejson__json_value_visit_event(
          parser, parser->json_stream_value->parse_visitor->object_begin);
    }
    return lonejson__json_value_emit(parser, "{", 1u);
  }
  return LONEJSON_STATUS_OK;
}

static LONEJSON__INLINE lonejson_status
lonejson__begin_array_value(lonejson_parser *parser) {
  lonejson_frame *parent = (parser->frame_count != 0u)
                               ? &parser->frames[parser->frame_count - 1u]
                               : NULL;
  const lonejson_field *field = NULL;
  void *object_ptr = NULL;
  lonejson_frame *frame;
  int started_capture = 0;

  if (!parser->root_started) {
    if (!parser->validate_only) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "root value must be an object");
    }
    parser->root_started = 1;
  }
  if (parent != NULL && parent->kind == LONEJSON_CONTAINER_OBJECT) {
    parent->after_comma = 0;
    field = parent->pending_field;
    object_ptr = parent->object_ptr;
    if (field != NULL) {
      if (!lonejson__mark_field_seen(parser, parent, field)) {
        return parser->error.code;
      }
      if (field->kind == LONEJSON_FIELD_KIND_JSON_VALUE) {
        lonejson_json_value *value = (lonejson_json_value *)lonejson__field_ptr(
            parent->object_ptr, field);
        if (lonejson__json_value_prepare_parse(parser, value, &parser->error) !=
            LONEJSON_STATUS_OK) {
          return parser->error.code;
        }
        lonejson__parser_set_json_stream_value(parser, value);
        parser->json_stream_depth = 1u;
        parser->json_stream_total_bytes = 0u;
        parser->json_stream_text_bytes = 0u;
        parser->json_stream_text_is_key = 0;
        if (value->parse_mode == LONEJSON_JSON_VALUE_PARSE_VISITOR) {
          if (value->parse_visitor_limits.max_depth != 0u &&
              parser->json_stream_depth >
                  value->parse_visitor_limits.max_depth) {
            lonejson__parser_clear_json_stream_value(parser);
            parser->json_stream_depth = 0u;
            return lonejson__set_error(
                &parser->error, LONEJSON_STATUS_OVERFLOW, parser->error.offset,
                parser->error.line, parser->error.column,
                "JSON value nesting exceeds maximum depth");
          }
          if (lonejson__json_value_visit_event(
                  parser, value->parse_visitor->array_begin) !=
              LONEJSON_STATUS_OK) {
            lonejson__parser_clear_json_stream_value(parser);
            parser->json_stream_depth = 0u;
            return parser->error.code;
          }
        } else if (lonejson__json_value_emit(parser, "[", 1u) !=
                   LONEJSON_STATUS_OK) {
          lonejson__parser_clear_json_stream_value(parser);
          parser->json_stream_depth = 0u;
          return parser->error.code;
        }
        field = NULL;
        object_ptr = NULL;
        started_capture = 1;
      } else {
        switch (field->kind) {
        case LONEJSON_FIELD_KIND_STRING_ARRAY:
        case LONEJSON_FIELD_KIND_I64_ARRAY:
        case LONEJSON_FIELD_KIND_U64_ARRAY:
        case LONEJSON_FIELD_KIND_F64_ARRAY:
        case LONEJSON_FIELD_KIND_BOOL_ARRAY:
        case LONEJSON_FIELD_KIND_OBJECT_ARRAY:
          break;
        default:
          return lonejson__set_error(
              &parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
              parser->error.offset, parser->error.line, parser->error.column,
              "field '%s' is not an array", field->json_key);
        }
      }
    }
  } else if (parent != NULL && parent->kind == LONEJSON_CONTAINER_ARRAY) {
    parent->after_comma = 0;
    if (parent->field != NULL) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_TYPE_MISMATCH, parser->error.offset,
          parser->error.line, parser->error.column,
          "field '%s' does not accept nested arrays", parent->field->json_key);
    }
    field = NULL;
    object_ptr = NULL;
  }

  frame = lonejson__push_frame(parser, LONEJSON_CONTAINER_ARRAY);
  if (frame == NULL) {
    return parser->error.code;
  }
  frame->map = NULL;
  frame->object_ptr = object_ptr;
  frame->field = field;
  if (!started_capture && parser->json_stream_active && field == NULL &&
      parent != NULL) {
    parser->json_stream_depth++;
    if (lonejson__json_value_parse_visitor_active(parser)) {
      if (parser->json_stream_value->parse_visitor_limits.max_depth != 0u &&
          parser->json_stream_depth >
              parser->json_stream_value->parse_visitor_limits.max_depth) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "JSON value nesting exceeds maximum depth");
      }
      return lonejson__json_value_visit_event(
          parser, parser->json_stream_value->parse_visitor->array_begin);
    }
    return lonejson__json_value_emit(parser, "[", 1u);
  }
  return LONEJSON_STATUS_OK;
}

static LONEJSON__INLINE lonejson_status
lonejson__finalize_object(lonejson_parser *parser) {
  lonejson_frame *frame = &parser->frames[parser->frame_count - 1u];
  size_t i;
  lonejson_uint64 bit;
  lonejson_uint64 required_mask;

  if (frame->map != NULL && frame->required_remaining == 0u) {
    lonejson__pop_frame(parser);
    return lonejson__complete_parent_after_value(parser);
  }

  if (frame->map != NULL && frame->map->field_count <= 64u) {
    required_mask = lonejson__required_mask_for_map(parser, frame->map);
    if ((frame->seen_mask & required_mask) != required_mask) {
      for (i = 0u; i < frame->map->field_count; ++i) {
        bit = ((lonejson_uint64)1u) << i;
        if ((required_mask & bit) != 0u && (frame->seen_mask & bit) == 0u) {
          return lonejson__set_error(
              &parser->error, LONEJSON_STATUS_MISSING_REQUIRED_FIELD,
              parser->error.offset, parser->error.line, parser->error.column,
              "missing required field '%s'", frame->map->fields[i].json_key);
        }
      }
    }
  } else if (frame->map != NULL && frame->seen_fields != NULL) {
    for (i = 0u; i < frame->map->field_count; ++i) {
      if ((frame->map->fields[i].flags & LONEJSON_FIELD_REQUIRED) != 0u &&
          frame->seen_fields[i] == 0u) {
        return lonejson__set_error(
            &parser->error, LONEJSON_STATUS_MISSING_REQUIRED_FIELD,
            parser->error.offset, parser->error.line, parser->error.column,
            "missing required field '%s'", frame->map->fields[i].json_key);
      }
    }
  } else if (frame->map != NULL) {
    for (i = 0u; i < frame->map->field_count; ++i) {
      if ((frame->map->fields[i].flags & LONEJSON_FIELD_REQUIRED) != 0u) {
        return lonejson__set_error(
            &parser->error, LONEJSON_STATUS_MISSING_REQUIRED_FIELD,
            parser->error.offset, parser->error.line, parser->error.column,
            "missing required field '%s'", frame->map->fields[i].json_key);
      }
    }
  }

  if (parser->json_stream_active) {
    lonejson_status status =
        lonejson__json_value_parse_visitor_active(parser)
            ? lonejson__json_value_visit_event(
                  parser, parser->json_stream_value->parse_visitor->object_end)
            : lonejson__json_value_emit(parser, "}", 1u);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  lonejson__pop_frame(parser);
  if (parser->json_stream_active && parser->json_stream_depth != 0u) {
    parser->json_stream_depth--;
    if (parser->json_stream_depth == 0u) {
      lonejson__parser_clear_json_stream_value(parser);
    }
  }
  return lonejson__complete_parent_after_value(parser);
}

static LONEJSON__INLINE lonejson_status
lonejson__finalize_array(lonejson_parser *parser) {
  if (parser->json_stream_active) {
    lonejson_status status =
        lonejson__json_value_parse_visitor_active(parser)
            ? lonejson__json_value_visit_event(
                  parser, parser->json_stream_value->parse_visitor->array_end)
            : lonejson__json_value_emit(parser, "]", 1u);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  lonejson__pop_frame(parser);
  if (parser->json_stream_active && parser->json_stream_depth != 0u) {
    parser->json_stream_depth--;
    if (parser->json_stream_depth == 0u) {
      lonejson__parser_clear_json_stream_value(parser);
    }
  }
  return lonejson__complete_parent_after_value(parser);
}

static int lonejson__mapped_numeric_target(const lonejson_frame *frame) {
  const lonejson_field *field;

  if (frame == NULL) {
    return 0;
  }
  if (frame->kind == LONEJSON_CONTAINER_OBJECT) {
    field = frame->pending_field;
    return field != NULL && (field->kind == LONEJSON_FIELD_KIND_I64 ||
                             field->kind == LONEJSON_FIELD_KIND_U64 ||
                             field->kind == LONEJSON_FIELD_KIND_F64);
  }
  if (frame->kind == LONEJSON_CONTAINER_ARRAY) {
    field = frame->field;
    return field != NULL && (field->kind == LONEJSON_FIELD_KIND_I64_ARRAY ||
                             field->kind == LONEJSON_FIELD_KIND_U64_ARRAY ||
                             field->kind == LONEJSON_FIELD_KIND_F64_ARRAY);
  }
  return 0;
}

static LONEJSON__INLINE lonejson_status
lonejson__deliver_token(lonejson_parser *parser, lonejson_lex_mode mode) {
  lonejson_frame *frame = (parser->frame_count != 0u)
                              ? &parser->frames[parser->frame_count - 1u]
                              : NULL;
  const char *token_text;

  token_text = lonejson__scratch_cstr(&parser->token);

  if (parser->lex_is_key) {
    if (frame == NULL || frame->kind != LONEJSON_CONTAINER_OBJECT ||
        frame->state != LONEJSON_FRAME_OBJECT_KEY_OR_END) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "object key outside object context");
    }
    if (lonejson__json_value_parse_visitor_active(parser)) {
      frame->pending_field = NULL;
      frame->state = LONEJSON_FRAME_OBJECT_COLON;
      return LONEJSON_STATUS_OK;
    }
    frame->pending_field =
        lonejson__find_field(frame->map, frame, token_text, parser->token.len);
    if (parser->json_stream_active) {
      lonejson_status status = lonejson__json_value_emit_string(
          parser, token_text, parser->token.len);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      status = lonejson__json_value_emit(parser, ":", 1u);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
    frame->state = LONEJSON_FRAME_OBJECT_COLON;
    return LONEJSON_STATUS_OK;
  }

  if (frame == NULL) {
    if (!parser->root_started && parser->validate_only) {
      parser->root_started = 1;
      parser->root_finished = 1;
      return LONEJSON_STATUS_OK;
    }
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "unexpected scalar at document root");
  }

  if (mode == LONEJSON_LEX_NUMBER &&
      (parser->validate_only || !lonejson__mapped_numeric_target(frame)) &&
      !lonejson__is_valid_json_number(token_text, parser->token.len)) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column, "invalid JSON number");
  }

  if (frame->kind == LONEJSON_CONTAINER_OBJECT) {
    lonejson_status status;

    if (lonejson__json_value_parse_visitor_active(parser) &&
        frame->pending_field == NULL) {
      if (mode == LONEJSON_LEX_NUMBER) {
        status =
            lonejson__json_value_number(parser, token_text, parser->token.len);
      } else if (mode == LONEJSON_LEX_TRUE) {
        status = lonejson__json_value_visit_bool(parser, 1);
      } else if (mode == LONEJSON_LEX_FALSE) {
        status = lonejson__json_value_visit_bool(parser, 0);
      } else if (mode == LONEJSON_LEX_NULL) {
        status = lonejson__json_value_visit_event(
            parser, parser->json_stream_value->parse_visitor->null_value);
      } else {
        status = LONEJSON_STATUS_OK;
      }
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      return lonejson__complete_parent_after_value(parser);
    }

    if (parser->json_stream_active && frame->pending_field == NULL) {
      if (mode == LONEJSON_LEX_STRING) {
        status = lonejson__json_value_emit_string(parser, token_text,
                                                  parser->token.len);
      } else {
        static const char true_text[] = "true";
        static const char false_text[] = "false";
        static const char null_text[] = "null";
        const char *text = token_text;
        size_t len = parser->token.len;
        if (mode == LONEJSON_LEX_TRUE) {
          text = true_text;
          len = 4u;
        } else if (mode == LONEJSON_LEX_FALSE) {
          text = false_text;
          len = 5u;
        } else if (mode == LONEJSON_LEX_NULL) {
          text = null_text;
          len = 4u;
        }
        status = lonejson__json_value_emit(parser, text, len);
      }
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      return lonejson__complete_parent_after_value(parser);
    }
    status =
        lonejson__handle_scalar_for_field(parser, frame, frame->pending_field,
                                          token_text, parser->token.len, mode);
    if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
      return lonejson__complete_parent_after_value(parser);
    }
    return status;
  }

  if (frame->kind == LONEJSON_CONTAINER_ARRAY) {
    lonejson_status status;
    if (frame->field == NULL) {
      if (lonejson__json_value_parse_visitor_active(parser)) {
        if (mode == LONEJSON_LEX_NUMBER) {
          status = lonejson__json_value_number(parser, token_text,
                                               parser->token.len);
        } else if (mode == LONEJSON_LEX_TRUE) {
          status = lonejson__json_value_visit_bool(parser, 1);
        } else if (mode == LONEJSON_LEX_FALSE) {
          status = lonejson__json_value_visit_bool(parser, 0);
        } else if (mode == LONEJSON_LEX_NULL) {
          status = lonejson__json_value_visit_event(
              parser, parser->json_stream_value->parse_visitor->null_value);
        } else {
          status = LONEJSON_STATUS_OK;
        }
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
      } else if (parser->json_stream_active) {
        if (mode == LONEJSON_LEX_STRING) {
          status = lonejson__json_value_emit_string(parser, token_text,
                                                    parser->token.len);
        } else {
          static const char true_text[] = "true";
          static const char false_text[] = "false";
          static const char null_text[] = "null";
          const char *text = token_text;
          size_t len = parser->token.len;
          if (mode == LONEJSON_LEX_TRUE) {
            text = true_text;
            len = 4u;
          } else if (mode == LONEJSON_LEX_FALSE) {
            text = false_text;
            len = 5u;
          } else if (mode == LONEJSON_LEX_NULL) {
            text = null_text;
            len = 4u;
          }
          status = lonejson__json_value_emit(parser, text, len);
        }
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
      }
      frame->after_comma = 0;
      frame->state = LONEJSON_FRAME_ARRAY_COMMA_OR_END;
      return LONEJSON_STATUS_OK;
    }
    status = lonejson__handle_array_scalar(parser, frame, token_text,
                                           parser->token.len, mode);
    if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
      frame->state = LONEJSON_FRAME_ARRAY_COMMA_OR_END;
    }
    return status;
  }

  return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                             parser->error.offset, parser->error.line,
                             parser->error.column, "invalid parser state");
}

static int lonejson__hex_value(int ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if (ch >= 'a' && ch <= 'f') {
    return 10 + (ch - 'a');
  }
  if (ch >= 'A' && ch <= 'F') {
    return 10 + (ch - 'A');
  }
  return -1;
}

static LONEJSON__INLINE int
lonejson__decode_unicode_quad(const unsigned char *src, lonejson_uint32 *out) {
  lonejson_uint32 cp;
  int hv;

  cp = 0u;
  hv = lonejson__hex_value(src[0]);
  if (hv < 0) {
    return 0;
  }
  cp = (cp << 4u) | (lonejson_uint32)hv;
  hv = lonejson__hex_value(src[1]);
  if (hv < 0) {
    return 0;
  }
  cp = (cp << 4u) | (lonejson_uint32)hv;
  hv = lonejson__hex_value(src[2]);
  if (hv < 0) {
    return 0;
  }
  cp = (cp << 4u) | (lonejson_uint32)hv;
  hv = lonejson__hex_value(src[3]);
  if (hv < 0) {
    return 0;
  }
  *out = (cp << 4u) | (lonejson_uint32)hv;
  return 1;
}

static LONEJSON__INLINE lonejson_status lonejson__append_unicode_codepoint(
    lonejson_parser *parser, lonejson_uint32 cp) {
  unsigned char utf8[4];
  size_t utf8_len;

  if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_TOKEN) {
    if (!lonejson__utf8_append(&parser->token, cp)) {
      return lonejson__set_error(
          &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
          parser->error.offset, parser->error.line, parser->error.column,
          "failed to append unicode escape");
    }
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
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_INVALID_JSON, parser->error.offset,
        parser->error.line, parser->error.column, "invalid unicode codepoint");
  }
  if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_DIRECT) {
    return lonejson__direct_string_append_bytes(parser, utf8, utf8_len);
  }
  if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_JSON_VISITOR) {
    return lonejson__json_value_string_chunk(parser, (const char *)utf8,
                                             utf8_len);
  }
  if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_STREAM) {
    lonejson_status status;
    status = lonejson__stream_value_append(parser, (const unsigned char *)utf8,
                                           utf8_len);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    return LONEJSON_STATUS_OK;
  }
  return lonejson__set_error(
      &parser->error, LONEJSON_STATUS_INTERNAL_ERROR, parser->error.offset,
      parser->error.line, parser->error.column, "invalid unicode append mode");
}

static LONEJSON__INLINE lonejson_status
lonejson__append_string_byte(lonejson_parser *parser, unsigned char ch) {
  if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_TOKEN) {
    return lonejson__scratch_append(&parser->token, (char)ch)
               ? LONEJSON_STATUS_OK
               : lonejson__set_error(
                     &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                     parser->error.offset, parser->error.line,
                     parser->error.column, "failed to append string");
  }
  if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_DIRECT) {
    return lonejson__direct_string_append_bytes(parser, &ch, 1u);
  }
  if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_JSON_VISITOR) {
    return lonejson__json_value_string_chunk(parser, (const char *)&ch, 1u);
  }
  if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_STREAM) {
    return lonejson__stream_value_append(parser, &ch, 1u);
  }
  return lonejson__set_error(
      &parser->error, LONEJSON_STATUS_INTERNAL_ERROR, parser->error.offset,
      parser->error.line, parser->error.column, "invalid string append mode");
}

static LONEJSON__INLINE lonejson_status lonejson__consume_simple_escape_fast(
    lonejson_parser *parser, const unsigned char *bytes, size_t avail,
    size_t *used) {
  unsigned char ch;

  if (used != NULL) {
    *used = 0u;
  }
  if (parser == NULL || bytes == NULL || avail < 2u || bytes[0] != '\\' ||
      parser->unicode_pending_high != 0u) {
    return LONEJSON_STATUS_OK;
  }
  switch (bytes[1]) {
  case '"':
    ch = '"';
    break;
  case '\\':
    ch = '\\';
    break;
  case '/':
    ch = '/';
    break;
  case 'b':
    ch = '\b';
    break;
  case 'f':
    ch = '\f';
    break;
  case 'n':
    ch = '\n';
    break;
  case 'r':
    ch = '\r';
    break;
  case 't':
    ch = '\t';
    break;
  default:
    return LONEJSON_STATUS_OK;
  }
  if (used != NULL) {
    *used = 2u;
  }
  parser->error.offset += 2u;
  parser->error.column += 2u;
  lonejson__parser_note_workspace_usage(parser);
  return lonejson__append_string_byte(parser, ch);
}

static LONEJSON__INLINE lonejson_status lonejson__consume_unicode_escape_fast(
    lonejson_parser *parser, const unsigned char *bytes, size_t avail,
    size_t *used) {
  lonejson_uint32 cp;
  size_t consume;

  if (used != NULL) {
    *used = 0u;
  }
  if (parser == NULL || bytes == NULL || avail < 6u || bytes[0] != '\\' ||
      bytes[1] != 'u') {
    return LONEJSON_STATUS_OK;
  }
  if (!lonejson__decode_unicode_quad(bytes + 2u, &cp)) {
    return LONEJSON_STATUS_OK;
  }
  consume = 6u;
  if (parser->unicode_pending_high != 0u) {
    if (cp < 0xDC00u || cp > 0xDFFFu) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "invalid unicode surrogate pair");
    }
    cp = 0x10000u +
         (((parser->unicode_pending_high - 0xD800u) << 10u) | (cp - 0xDC00u));
    parser->unicode_pending_high = 0u;
  } else if (cp >= 0xD800u && cp <= 0xDBFFu) {
    lonejson_uint32 low;

    if (avail < 12u || bytes[6] != '\\' || bytes[7] != 'u' ||
        !lonejson__decode_unicode_quad(bytes + 8u, &low)) {
      return LONEJSON_STATUS_OK;
    }
    if (low < 0xDC00u || low > 0xDFFFu) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "invalid unicode surrogate pair");
    }
    cp = 0x10000u + (((cp - 0xD800u) << 10u) | (low - 0xDC00u));
    consume = 12u;
  } else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_INVALID_JSON, parser->error.offset,
        parser->error.line, parser->error.column, "unexpected low surrogate");
  }
  if (used != NULL) {
    *used = consume;
  }
  parser->error.offset += consume;
  parser->error.column += consume;
  lonejson__parser_note_workspace_usage(parser);
  return lonejson__append_unicode_codepoint(parser, cp);
}

static LONEJSON__INLINE LONEJSON__HOT lonejson_status
lonejson__consume_string_fast(lonejson_parser *parser,
                              const unsigned char *bytes, size_t avail,
                              size_t *used) {
  size_t pos;

  if (used != NULL) {
    *used = 0u;
  }
  if (parser == NULL || bytes == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  pos = 0u;
  while (pos < avail) {
    size_t start;

    start = pos;
    while (pos < avail && bytes[pos] != '"' && bytes[pos] != '\\' &&
           bytes[pos] >= 0x20u) {
      pos++;
    }
    if (pos != start) {
      if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_TOKEN) {
        if (!lonejson__scratch_append_bytes(&parser->token, bytes + start,
                                            pos - start)) {
          return lonejson__set_error(
              &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
              parser->error.offset, parser->error.line, parser->error.column,
              "failed to append string");
        }
      } else if (parser->string_capture_mode ==
                 LONEJSON_STRING_CAPTURE_DIRECT) {
        lonejson_status status = lonejson__direct_string_append_bytes(
            parser, bytes + start, pos - start);
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
      } else if (parser->string_capture_mode ==
                 LONEJSON_STRING_CAPTURE_JSON_VISITOR) {
        lonejson_status status = lonejson__json_value_string_chunk(
            parser, (const char *)(bytes + start), pos - start);
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
      } else if (parser->string_capture_mode ==
                 LONEJSON_STRING_CAPTURE_STREAM) {
        lonejson_status status =
            lonejson__stream_value_append(parser, bytes + start, pos - start);
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
      } else {
        return lonejson__set_error(
            &parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
            parser->error.offset, parser->error.line, parser->error.column,
            "invalid string capture mode");
      }
      parser->error.offset += pos - start;
      parser->error.column += pos - start;
      lonejson__parser_note_workspace_usage(parser);
      if (pos == avail) {
        break;
      }
    }

    if (bytes[pos] == '"') {
      lonejson_status status;

      if (parser->unicode_pending_high != 0u) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "unterminated unicode surrogate pair");
      }
      parser->error.offset++;
      parser->error.column++;
      parser->lex_mode = LONEJSON_LEX_NONE;
      if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_DIRECT) {
        status = lonejson__direct_string_finish(parser);
      } else if (parser->string_capture_mode ==
                 LONEJSON_STRING_CAPTURE_JSON_VISITOR) {
        status = lonejson__json_value_string_end(parser);
        if (status == LONEJSON_STATUS_OK) {
          status = lonejson__deliver_token(parser, LONEJSON_LEX_STRING);
        }
      } else if (parser->string_capture_mode ==
                 LONEJSON_STRING_CAPTURE_STREAM) {
        status = lonejson__stream_value_finish(parser);
        if (status == LONEJSON_STATUS_OK) {
          status = lonejson__deliver_token(parser, LONEJSON_LEX_STRING);
        }
      } else {
        status = lonejson__deliver_token(parser, LONEJSON_LEX_STRING);
      }
      if (used != NULL) {
        *used = pos + 1u;
      }
      return status;
    }

    if (bytes[pos] == '\\') {
      size_t consumed;
      lonejson_status status;

      status = lonejson__consume_simple_escape_fast(parser, bytes + pos,
                                                    avail - pos, &consumed);
      if (status != LONEJSON_STATUS_OK) {
        if (used != NULL) {
          *used = pos + consumed;
        }
        return status;
      }
      if (consumed != 0u) {
        pos += consumed;
        continue;
      }
      status = lonejson__consume_unicode_escape_fast(parser, bytes + pos,
                                                     avail - pos, &consumed);
      if (status != LONEJSON_STATUS_OK) {
        if (used != NULL) {
          *used = pos + consumed;
        }
        return status;
      }
      if (consumed != 0u) {
        pos += consumed;
        continue;
      }
      break;
    }

    if (bytes[pos] < 0x20u) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "control character in string literal");
    }
  }
  if (used != NULL) {
    *used = pos;
  }
  return LONEJSON_STATUS_OK;
}

static int lonejson__is_valid_json_number(const char *value, size_t len) {
  size_t i = 0u;

  if (len == 0u) {
    return 0;
  }
  if (value[i] == '-') {
    i++;
    if (i == len) {
      return 0;
    }
  }
  if (value[i] == '0') {
    i++;
    if (i < len && lonejson__is_digit((unsigned char)value[i])) {
      return 0;
    }
  } else {
    if (!lonejson__is_digit((unsigned char)value[i])) {
      return 0;
    }
    while (i < len && lonejson__is_digit((unsigned char)value[i])) {
      i++;
    }
  }
  if (i < len && value[i] == '.') {
    i++;
    if (i == len || !lonejson__is_digit((unsigned char)value[i])) {
      return 0;
    }
    while (i < len && lonejson__is_digit((unsigned char)value[i])) {
      i++;
    }
  }
  if (i < len && (value[i] == 'e' || value[i] == 'E')) {
    i++;
    if (i < len && (value[i] == '+' || value[i] == '-')) {
      i++;
    }
    if (i == len || !lonejson__is_digit((unsigned char)value[i])) {
      return 0;
    }
    while (i < len && lonejson__is_digit((unsigned char)value[i])) {
      i++;
    }
  }
  return i == len;
}

static lonejson_status lonejson__parser_consume_char(lonejson_parser *parser,
                                                     unsigned char ch) {
  lonejson_frame *frame = (parser->frame_count != 0u)
                              ? &parser->frames[parser->frame_count - 1u]
                              : NULL;

  parser->error.offset++;
  if (ch == '\n') {
    parser->error.line++;
    parser->error.column = 0u;
  } else {
    parser->error.column++;
  }

  if (parser->lex_mode == LONEJSON_LEX_STRING) {
    if (parser->unicode_digits_needed != 0) {
      int hv = lonejson__hex_value(ch);
      if (hv < 0) {
        return lonejson__set_error(
            &parser->error, LONEJSON_STATUS_INVALID_JSON, parser->error.offset,
            parser->error.line, parser->error.column, "invalid unicode escape");
      }
      parser->unicode_accum =
          (parser->unicode_accum << 4u) | (lonejson_uint32)hv;
      parser->unicode_digits_needed--;
      if (parser->unicode_digits_needed == 0) {
        lonejson_uint32 cp = parser->unicode_accum;
        if (parser->unicode_pending_high != 0u) {
          if (cp < 0xDC00u || cp > 0xDFFFu) {
            return lonejson__set_error(
                &parser->error, LONEJSON_STATUS_INVALID_JSON,
                parser->error.offset, parser->error.line, parser->error.column,
                "invalid unicode surrogate pair");
          }
          cp = 0x10000u + (((parser->unicode_pending_high - 0xD800u) << 10u) |
                           (cp - 0xDC00u));
          parser->unicode_pending_high = 0u;
          {
            lonejson_status status =
                lonejson__append_unicode_codepoint(parser, cp);
            if (status != LONEJSON_STATUS_OK) {
              return status;
            }
          }
        } else if (cp >= 0xD800u && cp <= 0xDBFFu) {
          parser->unicode_pending_high = cp;
        } else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
          return lonejson__set_error(
              &parser->error, LONEJSON_STATUS_INVALID_JSON,
              parser->error.offset, parser->error.line, parser->error.column,
              "unexpected low surrogate");
        } else {
          lonejson_status status =
              lonejson__append_unicode_codepoint(parser, cp);
          if (status != LONEJSON_STATUS_OK) {
            return status;
          }
        }
      }
      return LONEJSON_STATUS_OK;
    }
    if (parser->lex_escape) {
      parser->lex_escape = 0;
      switch (ch) {
      case '"':
        return lonejson__append_string_byte(parser, '"');
      case '\\':
        return lonejson__append_string_byte(parser, '\\');
      case '/':
        return lonejson__append_string_byte(parser, '/');
      case 'b':
        return lonejson__append_string_byte(parser, '\b');
      case 'f':
        return lonejson__append_string_byte(parser, '\f');
      case 'n':
        return lonejson__append_string_byte(parser, '\n');
      case 'r':
        return lonejson__append_string_byte(parser, '\r');
      case 't':
        return lonejson__append_string_byte(parser, '\t');
      case 'u':
        parser->unicode_digits_needed = 4;
        parser->unicode_accum = 0u;
        return LONEJSON_STATUS_OK;
      default:
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "invalid escape sequence");
      }
    }
    if (ch == '\\') {
      parser->lex_escape = 1;
      return LONEJSON_STATUS_OK;
    }
    if (ch == '"') {
      parser->lex_mode = LONEJSON_LEX_NONE;
      if (parser->unicode_pending_high != 0u) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "unterminated unicode surrogate pair");
      }
      if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_TOKEN) {
        return lonejson__deliver_token(parser, LONEJSON_LEX_STRING);
      }
      if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_DIRECT) {
        return lonejson__direct_string_finish(parser);
      }
      if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_JSON_VISITOR) {
        lonejson_status status = lonejson__json_value_string_end(parser);
        return status == LONEJSON_STATUS_OK
                   ? lonejson__deliver_token(parser, LONEJSON_LEX_STRING)
                   : status;
      }
      if (parser->string_capture_mode == LONEJSON_STRING_CAPTURE_STREAM) {
        return lonejson__stream_value_finish(parser) == LONEJSON_STATUS_OK
                   ? lonejson__deliver_token(parser, LONEJSON_LEX_STRING)
                   : parser->error.code;
      }
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "invalid string completion mode");
    }
    if (ch < 0x20u) {
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "control character in string");
    }
    if (parser->stream_value_active) {
      return lonejson__stream_value_append(parser, &ch, 1u);
    }
    return lonejson__scratch_append(&parser->token, (char)ch)
               ? LONEJSON_STATUS_OK
               : lonejson__set_error(
                     &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                     parser->error.offset, parser->error.line,
                     parser->error.column, "failed to append string");
  }

  if (parser->lex_mode == LONEJSON_LEX_NUMBER) {
    if (lonejson__is_digit(ch) || ch == '+' || ch == '-' || ch == '.' ||
        ch == 'e' || ch == 'E') {
      return lonejson__scratch_append(&parser->token, (char)ch)
                 ? LONEJSON_STATUS_OK
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "failed to append number");
    }
    parser->lex_mode = LONEJSON_LEX_NONE;
    if (lonejson__deliver_token(parser, LONEJSON_LEX_NUMBER) !=
        LONEJSON_STATUS_OK) {
      return parser->error.code;
    }
    return lonejson__parser_consume_char(parser, ch);
  }

  if (parser->lex_mode == LONEJSON_LEX_TRUE) {
    const char *literal = "true";
    size_t pos = parser->token.len;
    if (pos < 4u && ch == (unsigned char)literal[pos]) {
      return lonejson__scratch_append(&parser->token, (char)ch)
                 ? ((parser->token.len == 4u)
                        ? (parser->lex_mode = LONEJSON_LEX_NONE,
                           lonejson__deliver_token(parser, LONEJSON_LEX_TRUE))
                        : LONEJSON_STATUS_OK)
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "failed to append literal");
    }
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column, "invalid literal");
  }

  if (parser->lex_mode == LONEJSON_LEX_FALSE) {
    const char *literal = "false";
    size_t pos = parser->token.len;
    if (pos < 5u && ch == (unsigned char)literal[pos]) {
      return lonejson__scratch_append(&parser->token, (char)ch)
                 ? ((parser->token.len == 5u)
                        ? (parser->lex_mode = LONEJSON_LEX_NONE,
                           lonejson__deliver_token(parser, LONEJSON_LEX_FALSE))
                        : LONEJSON_STATUS_OK)
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "failed to append literal");
    }
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column, "invalid literal");
  }

  if (parser->lex_mode == LONEJSON_LEX_NULL) {
    const char *literal = "null";
    size_t pos = parser->token.len;
    if (pos < 4u && ch == (unsigned char)literal[pos]) {
      return lonejson__scratch_append(&parser->token, (char)ch)
                 ? ((parser->token.len == 4u)
                        ? (parser->lex_mode = LONEJSON_LEX_NONE,
                           lonejson__deliver_token(parser, LONEJSON_LEX_NULL))
                        : LONEJSON_STATUS_OK)
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "failed to append literal");
    }
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column, "invalid literal");
  }

  if (lonejson__is_json_space(ch)) {
    return LONEJSON_STATUS_OK;
  }

  if (!parser->root_started) {
    if (ch == '{') {
      return lonejson__begin_object_value(parser);
    }
    if (ch == '[') {
      return lonejson__begin_array_value(parser);
    }
    if (ch == '"') {
      parser->lex_mode = LONEJSON_LEX_STRING;
      parser->lex_is_key = 0;
      parser->lex_escape = 0;
      parser->unicode_pending_high = 0u;
      parser->unicode_digits_needed = 0;
      if (!lonejson__prepare_token(parser)) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "parser workspace exhausted by token");
      }
      return parser->validate_only
                 ? LONEJSON_STATUS_OK
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "root value must be an object");
    }
    if (ch == '-' || lonejson__is_digit(ch)) {
      parser->lex_mode = LONEJSON_LEX_NUMBER;
      parser->lex_is_key = 0;
      if (!lonejson__prepare_token(parser)) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "parser workspace exhausted by token");
      }
      if (!lonejson__scratch_append(&parser->token, (char)ch)) {
        return lonejson__set_error(
            &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
            parser->error.offset, parser->error.line, parser->error.column,
            "failed to append number");
      }
      return parser->validate_only
                 ? LONEJSON_STATUS_OK
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "root value must be an object");
    }
    if (ch == 't' || ch == 'f' || ch == 'n') {
      parser->lex_mode = (ch == 't')   ? LONEJSON_LEX_TRUE
                         : (ch == 'f') ? LONEJSON_LEX_FALSE
                                       : LONEJSON_LEX_NULL;
      parser->lex_is_key = 0;
      if (!lonejson__prepare_token(parser)) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "parser workspace exhausted by token");
      }
      if (!lonejson__scratch_append(&parser->token, (char)ch)) {
        return lonejson__set_error(
            &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
            parser->error.offset, parser->error.line, parser->error.column,
            "failed to append literal");
      }
      return parser->validate_only
                 ? LONEJSON_STATUS_OK
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_TYPE_MISMATCH,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "root value must be an object");
    }
    return lonejson__set_error(
        &parser->error,
        parser->validate_only ? LONEJSON_STATUS_INVALID_JSON
                              : LONEJSON_STATUS_TYPE_MISMATCH,
        parser->error.offset, parser->error.line, parser->error.column,
        parser->validate_only ? "expected JSON value"
                              : "root value must be an object");
  }

  if (frame == NULL) {
    return lonejson__set_error(
        &parser->error, LONEJSON_STATUS_INVALID_JSON, parser->error.offset,
        parser->error.line, parser->error.column, "unexpected trailing data");
  }

  if (frame->kind == LONEJSON_CONTAINER_OBJECT) {
    switch (frame->state) {
    case LONEJSON_FRAME_OBJECT_KEY_OR_END:
      if (ch == '}') {
        if (frame->after_comma) {
          return lonejson__set_error(
              &parser->error, LONEJSON_STATUS_INVALID_JSON,
              parser->error.offset, parser->error.line, parser->error.column,
              "trailing comma in object");
        }
        return lonejson__finalize_object(parser);
      }
      if (ch != '"') {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column, "expected object key");
      }
      frame->after_comma = 0;
      parser->lex_mode = LONEJSON_LEX_STRING;
      parser->lex_is_key = 1;
      parser->lex_escape = 0;
      parser->unicode_pending_high = 0u;
      parser->unicode_digits_needed = 0;
      if (!lonejson__prepare_token(parser)) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "parser workspace exhausted by token");
      }
      return LONEJSON_STATUS_OK;
    case LONEJSON_FRAME_OBJECT_COLON:
      if (ch != ':') {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "expected ':' after object key");
      }
      frame->state = LONEJSON_FRAME_OBJECT_VALUE;
      return LONEJSON_STATUS_OK;
    case LONEJSON_FRAME_OBJECT_VALUE:
      if (ch == '"') {
        lonejson_status status;

        parser->lex_mode = LONEJSON_LEX_STRING;
        parser->lex_is_key = 0;
        parser->lex_escape = 0;
        parser->unicode_pending_high = 0u;
        parser->unicode_digits_needed = 0;
        if (lonejson__field_is_streamed(frame->pending_field)) {
          void *ptr =
              lonejson__field_ptr(frame->object_ptr, frame->pending_field);
          status =
              lonejson__stream_value_begin(parser, frame->pending_field, ptr);
          if (status != LONEJSON_STATUS_OK) {
            return status;
          }
        } else if (!lonejson__prepare_token(parser)) {
          return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                     parser->error.offset, parser->error.line,
                                     parser->error.column,
                                     "parser workspace exhausted by token");
        }
        return LONEJSON_STATUS_OK;
      }
      if (ch == '{') {
        return lonejson__begin_object_value(parser);
      }
      if (ch == '[') {
        return lonejson__begin_array_value(parser);
      }
      if (ch == '-' || lonejson__is_digit(ch)) {
        parser->lex_mode = LONEJSON_LEX_NUMBER;
        parser->lex_is_key = 0;
        if (!lonejson__prepare_token(parser)) {
          return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                     parser->error.offset, parser->error.line,
                                     parser->error.column,
                                     "parser workspace exhausted by token");
        }
        return lonejson__scratch_append(&parser->token, (char)ch)
                   ? LONEJSON_STATUS_OK
                   : lonejson__set_error(
                         &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                         parser->error.offset, parser->error.line,
                         parser->error.column, "failed to append number");
      }
      if (ch == 't') {
        parser->lex_mode = LONEJSON_LEX_TRUE;
        parser->lex_is_key = 0;
        if (!lonejson__prepare_token(parser)) {
          return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                     parser->error.offset, parser->error.line,
                                     parser->error.column,
                                     "parser workspace exhausted by token");
        }
        return lonejson__scratch_append(&parser->token, 't')
                   ? ((parser->token.len == 4u)
                          ? (parser->lex_mode = LONEJSON_LEX_NONE,
                             lonejson__deliver_token(parser, LONEJSON_LEX_TRUE))
                          : LONEJSON_STATUS_OK)
                   : LONEJSON_STATUS_ALLOCATION_FAILED;
      }
      if (ch == 'f') {
        parser->lex_mode = LONEJSON_LEX_FALSE;
        parser->lex_is_key = 0;
        if (!lonejson__prepare_token(parser)) {
          return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                     parser->error.offset, parser->error.line,
                                     parser->error.column,
                                     "parser workspace exhausted by token");
        }
        return lonejson__scratch_append(&parser->token, 'f')
                   ? LONEJSON_STATUS_OK
                   : lonejson__set_error(
                         &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                         parser->error.offset, parser->error.line,
                         parser->error.column, "failed to append literal");
      }
      if (ch == 'n') {
        parser->lex_mode = LONEJSON_LEX_NULL;
        parser->lex_is_key = 0;
        if (!lonejson__prepare_token(parser)) {
          return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                     parser->error.offset, parser->error.line,
                                     parser->error.column,
                                     "parser workspace exhausted by token");
        }
        return lonejson__scratch_append(&parser->token, 'n')
                   ? LONEJSON_STATUS_OK
                   : lonejson__set_error(
                         &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                         parser->error.offset, parser->error.line,
                         parser->error.column, "failed to append literal");
      }
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column, "expected JSON value");
    case LONEJSON_FRAME_OBJECT_COMMA_OR_END:
      if (ch == ',') {
        if (parser->json_stream_active && !parser->json_stream_visit_active) {
          lonejson_status status = lonejson__json_value_emit(parser, ",", 1u);
          if (status != LONEJSON_STATUS_OK) {
            return status;
          }
        }
        frame->state = LONEJSON_FRAME_OBJECT_KEY_OR_END;
        frame->after_comma = 1;
        return LONEJSON_STATUS_OK;
      }
      if (ch == '}') {
        return lonejson__finalize_object(parser);
      }
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column,
                                 "expected ',' or '}' in object");
    default:
      return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                                 parser->error.offset, parser->error.line,
                                 parser->error.column, "invalid object state");
    }
  }

  switch (frame->state) {
  case LONEJSON_FRAME_ARRAY_VALUE_OR_END:
    if (ch == ']') {
      if (frame->after_comma) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "trailing comma in array");
      }
      return lonejson__finalize_array(parser);
    }
    frame->after_comma = 0;
    if (ch == '"') {
      return lonejson__begin_string_value_lex(
          parser, frame->field,
          frame->field != NULL
              ? lonejson__field_ptr(frame->object_ptr, frame->field)
              : NULL);
    }
    if (ch == '{') {
      return lonejson__begin_object_value(parser);
    }
    if (ch == '[') {
      return lonejson__begin_array_value(parser);
    }
    if (ch == '-' || lonejson__is_digit(ch)) {
      parser->lex_mode = LONEJSON_LEX_NUMBER;
      parser->lex_is_key = 0;
      if (!lonejson__prepare_token(parser)) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "parser workspace exhausted by token");
      }
      return lonejson__scratch_append(&parser->token, (char)ch)
                 ? LONEJSON_STATUS_OK
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "failed to append number");
    }
    if (ch == 't') {
      parser->lex_mode = LONEJSON_LEX_TRUE;
      parser->lex_is_key = 0;
      if (!lonejson__prepare_token(parser)) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "parser workspace exhausted by token");
      }
      return lonejson__scratch_append(&parser->token, 't')
                 ? LONEJSON_STATUS_OK
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "failed to append literal");
    }
    if (ch == 'f') {
      parser->lex_mode = LONEJSON_LEX_FALSE;
      parser->lex_is_key = 0;
      if (!lonejson__prepare_token(parser)) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "parser workspace exhausted by token");
      }
      return lonejson__scratch_append(&parser->token, 'f')
                 ? LONEJSON_STATUS_OK
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "failed to append literal");
    }
    if (ch == 'n') {
      parser->lex_mode = LONEJSON_LEX_NULL;
      parser->lex_is_key = 0;
      if (!lonejson__prepare_token(parser)) {
        return lonejson__set_error(&parser->error, LONEJSON_STATUS_OVERFLOW,
                                   parser->error.offset, parser->error.line,
                                   parser->error.column,
                                   "parser workspace exhausted by token");
      }
      return lonejson__scratch_append(&parser->token, 'n')
                 ? LONEJSON_STATUS_OK
                 : lonejson__set_error(
                       &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
                       parser->error.offset, parser->error.line,
                       parser->error.column, "failed to append literal");
    }
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column, "expected array value");
  case LONEJSON_FRAME_ARRAY_COMMA_OR_END:
    if (ch == ',') {
      if (parser->json_stream_active && !parser->json_stream_visit_active) {
        lonejson_status status = lonejson__json_value_emit(parser, ",", 1u);
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
      }
      frame->state = LONEJSON_FRAME_ARRAY_VALUE_OR_END;
      frame->after_comma = 1;
      return LONEJSON_STATUS_OK;
    }
    if (ch == ']') {
      return lonejson__finalize_array(parser);
    }
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INVALID_JSON,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "expected ',' or ']' in array");
  default:
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               parser->error.offset, parser->error.line,
                               parser->error.column, "invalid array state");
  }
}

static int lonejson__parser_root_complete(const lonejson_parser *parser) {
  return parser->root_finished && parser->frame_count == 0u &&
         parser->lex_mode == LONEJSON_LEX_NONE;
}

static LONEJSON__HOT lonejson_status
lonejson__parser_feed_bytes(lonejson_parser *parser, const unsigned char *bytes,
                            size_t len, size_t *consumed, int stop_at_root) {
  size_t i;

  if (parser == NULL || (bytes == NULL && len != 0u)) {
    return lonejson__set_error(parser ? &parser->error : NULL,
                               LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
                               "invalid parser feed arguments");
  }
  if (parser->failed) {
    return parser->error.code;
  }

  i = 0u;
  while (i < len) {
    lonejson_status status;

    if (stop_at_root && lonejson__parser_root_complete(parser)) {
      break;
    }

    if (parser->lex_mode == LONEJSON_LEX_STRING &&
        parser->unicode_digits_needed == 0 && !parser->lex_escape) {
      size_t used = 0u;

      status = lonejson__consume_string_fast(parser, bytes + i, len - i, &used);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        parser->failed = 1;
        if (consumed != NULL) {
          *consumed = i + used;
        }
        return status;
      }
      i += used;
      if (parser->lex_mode == LONEJSON_LEX_NONE) {
        lonejson__parser_note_workspace_usage(parser);
        continue;
      }
      if (used != 0u) {
        continue;
      }
    } else if (parser->lex_mode == LONEJSON_LEX_NUMBER) {
      size_t start;

      start = i;
      while (i < len && (lonejson__is_digit(bytes[i]) || bytes[i] == '+' ||
                         bytes[i] == '-' || bytes[i] == '.' ||
                         bytes[i] == 'e' || bytes[i] == 'E')) {
        i++;
      }
      if (i != start) {
        if (!lonejson__scratch_append_bytes(&parser->token, bytes + start,
                                            i - start)) {
          parser->failed = 1;
          status = lonejson__set_error(
              &parser->error, LONEJSON_STATUS_ALLOCATION_FAILED,
              parser->error.offset, parser->error.line, parser->error.column,
              "failed to append number");
          if (consumed != NULL) {
            *consumed = start;
          }
          return status;
        }
        parser->error.offset += i - start;
        parser->error.column += i - start;
        lonejson__parser_note_workspace_usage(parser);
        if (i == len) {
          break;
        }
        parser->lex_mode = LONEJSON_LEX_NONE;
        status = lonejson__deliver_token(parser, LONEJSON_LEX_NUMBER);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          parser->failed = 1;
          if (consumed != NULL) {
            *consumed = i;
          }
          return status;
        }
        lonejson__parser_note_workspace_usage(parser);
        continue;
      }
    }

    if (parser->lex_mode == LONEJSON_LEX_NONE && parser->frame_count != 0u) {
      lonejson_frame *active = &parser->frames[parser->frame_count - 1u];

      parser->error.offset++;
      parser->error.column++;
      if (active->kind == LONEJSON_CONTAINER_OBJECT) {
        if (active->state == LONEJSON_FRAME_OBJECT_KEY_OR_END &&
            bytes[i] == '"') {
          status = lonejson__begin_string_lex(parser, 1);
          if (status != LONEJSON_STATUS_OK &&
              status != LONEJSON_STATUS_TRUNCATED) {
            parser->failed = 1;
            if (consumed != NULL) {
              *consumed = i + 1u;
            }
            return status;
          }
          i++;
          continue;
        }
        if (active->state == LONEJSON_FRAME_OBJECT_COLON && bytes[i] == ':') {
          active->state = LONEJSON_FRAME_OBJECT_VALUE;
          i++;
          continue;
        }
        if (active->state == LONEJSON_FRAME_OBJECT_VALUE) {
          if (bytes[i] == '"') {
            status = lonejson__begin_string_value_lex(
                parser, active->pending_field,
                active->pending_field != NULL
                    ? lonejson__field_ptr(active->object_ptr,
                                          active->pending_field)
                    : NULL);
            if (status != LONEJSON_STATUS_OK &&
                status != LONEJSON_STATUS_TRUNCATED) {
              parser->failed = 1;
              if (consumed != NULL) {
                *consumed = i + 1u;
              }
              return status;
            }
            i++;
            continue;
          }
          if (bytes[i] == '-' || lonejson__is_digit(bytes[i])) {
            status = lonejson__begin_number_lex(parser, bytes[i]);
            if (status != LONEJSON_STATUS_OK &&
                status != LONEJSON_STATUS_TRUNCATED) {
              parser->failed = 1;
              if (consumed != NULL) {
                *consumed = i + 1u;
              }
              return status;
            }
            i++;
            continue;
          }
          if (bytes[i] == 't' || bytes[i] == 'f' || bytes[i] == 'n') {
            status = lonejson__begin_literal_lex(
                parser,
                (bytes[i] == 't')   ? LONEJSON_LEX_TRUE
                : (bytes[i] == 'f') ? LONEJSON_LEX_FALSE
                                    : LONEJSON_LEX_NULL,
                bytes[i]);
            if (status != LONEJSON_STATUS_OK &&
                status != LONEJSON_STATUS_TRUNCATED) {
              parser->failed = 1;
              if (consumed != NULL) {
                *consumed = i + 1u;
              }
              return status;
            }
            i++;
            continue;
          }
        }
        if (active->state == LONEJSON_FRAME_OBJECT_COMMA_OR_END) {
          if (bytes[i] == ',') {
            if (parser->json_stream_active &&
                !parser->json_stream_visit_active) {
              status = lonejson__json_value_emit(parser, ",", 1u);
              if (status != LONEJSON_STATUS_OK) {
                parser->failed = 1;
                if (consumed != NULL) {
                  *consumed = i + 1u;
                }
                return status;
              }
            }
            active->state = LONEJSON_FRAME_OBJECT_KEY_OR_END;
            active->after_comma = 1;
            i++;
            continue;
          }
          if (bytes[i] == '}') {
            status = lonejson__finalize_object(parser);
            if (status != LONEJSON_STATUS_OK &&
                status != LONEJSON_STATUS_TRUNCATED) {
              parser->failed = 1;
              if (consumed != NULL) {
                *consumed = i + 1u;
              }
              return status;
            }
            lonejson__parser_note_workspace_usage(parser);
            i++;
            continue;
          }
        }
      } else if (active->kind == LONEJSON_CONTAINER_ARRAY &&
                 active->state == LONEJSON_FRAME_ARRAY_VALUE_OR_END) {
        if (bytes[i] == '"') {
          status = lonejson__begin_string_value_lex(
              parser, active->field,
              active->field != NULL
                  ? lonejson__field_ptr(active->object_ptr, active->field)
                  : NULL);
          if (status != LONEJSON_STATUS_OK &&
              status != LONEJSON_STATUS_TRUNCATED) {
            parser->failed = 1;
            if (consumed != NULL) {
              *consumed = i + 1u;
            }
            return status;
          }
          i++;
          continue;
        }
        if (bytes[i] == '-' || lonejson__is_digit(bytes[i])) {
          status = lonejson__begin_number_lex(parser, bytes[i]);
          if (status != LONEJSON_STATUS_OK &&
              status != LONEJSON_STATUS_TRUNCATED) {
            parser->failed = 1;
            if (consumed != NULL) {
              *consumed = i + 1u;
            }
            return status;
          }
          i++;
          continue;
        }
        if (bytes[i] == 't' || bytes[i] == 'f' || bytes[i] == 'n') {
          status = lonejson__begin_literal_lex(
              parser,
              (bytes[i] == 't')   ? LONEJSON_LEX_TRUE
              : (bytes[i] == 'f') ? LONEJSON_LEX_FALSE
                                  : LONEJSON_LEX_NULL,
              bytes[i]);
          if (status != LONEJSON_STATUS_OK &&
              status != LONEJSON_STATUS_TRUNCATED) {
            parser->failed = 1;
            if (consumed != NULL) {
              *consumed = i + 1u;
            }
            return status;
          }
          i++;
          continue;
        }
      } else if (active->kind == LONEJSON_CONTAINER_ARRAY &&
                 active->state == LONEJSON_FRAME_ARRAY_COMMA_OR_END) {
        if (bytes[i] == ',') {
          if (parser->json_stream_active && !parser->json_stream_visit_active) {
            status = lonejson__json_value_emit(parser, ",", 1u);
            if (status != LONEJSON_STATUS_OK) {
              parser->failed = 1;
              if (consumed != NULL) {
                *consumed = i + 1u;
              }
              return status;
            }
          }
          active->state = LONEJSON_FRAME_ARRAY_VALUE_OR_END;
          active->after_comma = 1;
          i++;
          continue;
        }
        if (bytes[i] == ']') {
          status = lonejson__finalize_array(parser);
          if (status != LONEJSON_STATUS_OK &&
              status != LONEJSON_STATUS_TRUNCATED) {
            parser->failed = 1;
            if (consumed != NULL) {
              *consumed = i + 1u;
            }
            return status;
          }
          lonejson__parser_note_workspace_usage(parser);
          i++;
          continue;
        }
      }
      parser->error.offset--;
      parser->error.column--;
    }

    status = lonejson__parser_consume_char(parser, bytes[i]);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      parser->failed = 1;
      if (consumed != NULL) {
        *consumed = i + 1u;
      }
      return status;
    }
    lonejson__parser_note_workspace_usage(parser);
    i++;
  }

  if (consumed != NULL) {
    *consumed = i;
  }
  return parser->error.truncated ? LONEJSON_STATUS_TRUNCATED
                                 : LONEJSON_STATUS_OK;
}

static LONEJSON__INLINE lonejson_status lonejson__sink_buffer(
    void *user, const void *data, size_t len, lonejson_error *error) {
  lonejson_buffer_sink *sink = (lonejson_buffer_sink *)user;
  size_t available =
      (sink->capacity > sink->length) ? (sink->capacity - sink->length) : 0u;
  size_t writable = (available > 0u) ? (available - 1u) : 0u;

  sink->needed += len;
  if (available > 0u) {
    size_t copy_len = (len < writable) ? len : writable;
    if (copy_len > 0u) {
      memcpy(sink->buffer + sink->length, data, copy_len);
      sink->length += copy_len;
    }
  }
  if (len > writable) {
    sink->truncated = 1;
    if (sink->policy == LONEJSON_OVERFLOW_FAIL) {
      return lonejson__set_error(
          error, LONEJSON_STATUS_OVERFLOW, error ? error->offset : 0u,
          error ? error->line : 0u, error ? error->column : 0u,
          "output buffer too small");
    }
    return (sink->policy == LONEJSON_OVERFLOW_TRUNCATE)
               ? LONEJSON_STATUS_TRUNCATED
               : LONEJSON_STATUS_OK;
  }
  return LONEJSON_STATUS_OK;
}

static LONEJSON__INLINE lonejson_status lonejson__sink_buffer_exact(
    void *user, const void *data, size_t len, lonejson_error *error) {
  lonejson_buffer_sink *sink = (lonejson_buffer_sink *)user;
  size_t writable = (sink->capacity > sink->length + 1u)
                        ? (sink->capacity - sink->length - 1u)
                        : 0u;

  sink->needed += len;
  if (len > writable) {
    sink->truncated = 1;
    return lonejson__set_error(
        error, LONEJSON_STATUS_OVERFLOW, error ? error->offset : 0u,
        error ? error->line : 0u, error ? error->column : 0u,
        "output buffer too small");
  }
  if (len != 0u) {
    memcpy(sink->buffer + sink->length, data, len);
    sink->length += len;
  }
  return LONEJSON_STATUS_OK;
}

static LONEJSON__INLINE lonejson_status lonejson__sink_grow(
    void *user, const void *data, size_t len, lonejson_error *error) {
  lonejson_buffer_sink *sink = (lonejson_buffer_sink *)user;
  char *next;
  size_t next_cap;
  size_t required = sink->length + len + 1u;

  (void)error;
  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  if (required > sink->capacity) {
    next_cap = (sink->capacity == 0u) ? 2048u : sink->capacity;
    while (next_cap < required) {
      next_cap *= 2u;
    }
    next = (char *)lonejson__buffer_realloc(sink->allocator, sink->buffer,
                                            sink->alloc_size, next_cap);
    if (next == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u, "failed to grow output buffer");
    }
    sink->buffer = next;
    sink->capacity = next_cap;
    sink->alloc_size = next_cap;
  }
  memcpy(sink->buffer + sink->length, data, len);
  sink->length += len;
  sink->needed = sink->length;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__sink_file(void *user, const void *data,
                                           size_t len, lonejson_error *error) {
  FILE *fp = (FILE *)user;
  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  if (fwrite(data, 1u, len, fp) != len) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    return lonejson__set_error(
        error, LONEJSON_STATUS_IO_ERROR, error ? error->offset : 0u,
        error ? error->line : 0u, error ? error->column : 0u,
        "failed to write JSON output");
  }
  return LONEJSON_STATUS_OK;
}

static LONEJSON__INLINE lonejson_status lonejson__emit(lonejson_sink_fn sink,
                                                       void *user,
                                                       lonejson_error *error,
                                                       const char *data,
                                                       size_t len) {
  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  return sink(user, data, len, error);
}

static LONEJSON__INLINE lonejson_status
lonejson__emit_cstr(lonejson_sink_fn sink, void *user, lonejson_error *error,
                    const char *text) {
  return lonejson__emit(sink, user, error, text, LONEJSON__TEXT_LEN(text));
}

static lonejson_status
lonejson__emit_escaped_fragment(lonejson_sink_fn sink, void *user,
                                lonejson_error *error,
                                const unsigned char *text, size_t len) {
  static const char hex[] = "0123456789abcdef";
  size_t i;
  size_t start;
  lonejson_status status;

  start = 0u;
  for (i = 0u; i < len; ++i) {
    char escape_buf[7];
    if (text[i] >= 0x20u && text[i] != '"' && text[i] != '\\') {
      continue;
    }
    if (start != i) {
      status = lonejson__emit(sink, user, error, (const char *)(text + start),
                              i - start);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    switch (text[i]) {
    case '"':
      status = lonejson__emit_cstr(sink, user, error, "\\\"");
      break;
    case '\\':
      status = lonejson__emit_cstr(sink, user, error, "\\\\");
      break;
    case '\b':
      status = lonejson__emit_cstr(sink, user, error, "\\b");
      break;
    case '\f':
      status = lonejson__emit_cstr(sink, user, error, "\\f");
      break;
    case '\n':
      status = lonejson__emit_cstr(sink, user, error, "\\n");
      break;
    case '\r':
      status = lonejson__emit_cstr(sink, user, error, "\\r");
      break;
    case '\t':
      status = lonejson__emit_cstr(sink, user, error, "\\t");
      break;
    default:
      if (text[i] < 0x20u) {
        escape_buf[0] = '\\';
        escape_buf[1] = 'u';
        escape_buf[2] = '0';
        escape_buf[3] = '0';
        escape_buf[4] = hex[(text[i] >> 4u) & 0x0Fu];
        escape_buf[5] = hex[text[i] & 0x0Fu];
        escape_buf[6] = '\0';
        status = lonejson__emit(sink, user, error, escape_buf, 6u);
      } else {
        status = LONEJSON_STATUS_OK;
      }
      break;
    }
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    start = i + 1u;
  }
  if (start != len) {
    status = lonejson__emit(sink, user, error, (const char *)(text + start),
                            len - start);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__emit_escaped_bytes(lonejson_sink_fn sink,
                                                    void *user,
                                                    lonejson_error *error,
                                                    const unsigned char *text,
                                                    size_t len) {
  lonejson_status status;

  status = lonejson__emit_cstr(sink, user, error, "\"");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  status = lonejson__emit_escaped_fragment(sink, user, error, text, len);
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  return lonejson__emit_cstr(sink, user, error, "\"");
}

static lonejson_status lonejson__emit_escaped_string(lonejson_sink_fn sink,
                                                     void *user,
                                                     lonejson_error *error,
                                                     const char *text) {
  return lonejson__emit_escaped_bytes(
      sink, user, error, (const unsigned char *)text, LONEJSON__TEXT_LEN(text));
}

static LONEJSON__INLINE lonejson_status lonejson__emit_escaped_len(
    lonejson_sink_fn sink, void *user, lonejson_error *error, const char *text,
    size_t len) {
  return lonejson__emit_escaped_bytes(sink, user, error,
                                      (const unsigned char *)text, len);
}

typedef struct lonejson__write_state {
  lonejson_sink_fn sink;
  void *user;
  lonejson_error *error;
  int pretty;
  size_t depth;
} lonejson__write_state;

static LONEJSON__INLINE lonejson_status
lonejson__emit_pretty_indent(const lonejson__write_state *state, size_t depth) {
  size_t i;
  lonejson_status status;

  if (!state->pretty) {
    return LONEJSON_STATUS_OK;
  }
  status = lonejson__emit_cstr(state->sink, state->user, state->error, "\n");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  for (i = 0; i < depth; ++i) {
    status = lonejson__emit_cstr(state->sink, state->user, state->error, "  ");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__serialize_value_pretty(const lonejson_field *field, const void *ptr,
                                 lonejson__write_state *state);

static lonejson_status
lonejson__serialize_value_compact(const lonejson_field *field, const void *ptr,
                                  lonejson_sink_fn sink, void *user,
                                  lonejson_error *error);

static lonejson_status lonejson__serialize_map_compact(const lonejson_map *map,
                                                       const void *src,
                                                       lonejson_sink_fn sink,
                                                       void *user,
                                                       lonejson_error *error);

static lonejson_status
lonejson__serialize_map_pretty(const lonejson_map *map, const void *src,
                               lonejson__write_state *state) {
  size_t i;
  lonejson_status status;

  status = lonejson__emit_cstr(state->sink, state->user, state->error, "{");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  if (map->field_count == 0u) {
    return lonejson__emit_cstr(state->sink, state->user, state->error, "}");
  }
  for (i = 0; i < map->field_count; ++i) {
    if (i != 0u) {
      status = lonejson__emit_cstr(state->sink, state->user, state->error, ",");
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    if (state->pretty) {
      status = lonejson__emit_pretty_indent(state, state->depth + 1u);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    status = lonejson__emit_escaped_len(state->sink, state->user, state->error,
                                        map->fields[i].json_key,
                                        map->fields[i].json_key_len);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    status = lonejson__emit_cstr(state->sink, state->user, state->error,
                                 state->pretty ? ": " : ":");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    ++state->depth;
    status = lonejson__serialize_value_pretty(
        &map->fields[i], lonejson__field_cptr(src, &map->fields[i]), state);
    --state->depth;
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  if (state->pretty) {
    status = lonejson__emit_pretty_indent(state, state->depth);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  return lonejson__emit_cstr(state->sink, state->user, state->error, "}");
}

static lonejson_status
lonejson__serialize_jsonl_records(const lonejson_map *map, const void *items,
                                  size_t count, size_t stride,
                                  lonejson__write_state *state) {
  const unsigned char *base = (const unsigned char *)items;
  size_t i;
  lonejson_status status;

  for (i = 0; i < count; ++i) {
    const void *record = (const void *)(base + (i * stride));
    status = lonejson__serialize_map_pretty(map, record, state);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    status = state->sink(state->user, "\n", 1u, state->error);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__serialize_jsonl_records_compact(
    const lonejson_map *map, const void *items, size_t count, size_t stride,
    lonejson_sink_fn sink, void *user, lonejson_error *error) {
  const unsigned char *base = (const unsigned char *)items;
  size_t i;
  lonejson_status status;

  for (i = 0; i < count; ++i) {
    const void *record = (const void *)(base + (i * stride));
    status = lonejson__serialize_map_compact(map, record, sink, user, error);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    status = sink(user, "\n", 1u, error);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__emit_number_text(lonejson_sink_fn sink,
                                                  void *user,
                                                  lonejson_error *error,
                                                  const char *fmt, ...) {
  char buf[64];
  va_list ap;
  int written;

  va_start(ap, fmt);
  written = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (written < 0) {
    return lonejson__set_error(
        error, LONEJSON_STATUS_INTERNAL_ERROR, error ? error->offset : 0u,
        error ? error->line : 0u, error ? error->column : 0u,
        "failed to format number");
  }
  return lonejson__emit(sink, user, error, buf, (size_t)written);
}

static LONEJSON__INLINE lonejson_status
lonejson__emit_u64_value(lonejson_sink_fn sink, void *user,
                         lonejson_error *error, lonejson_uint64 value) {
  char buf[32];
  size_t idx;

  idx = sizeof(buf);
  do {
    lonejson_uint64 digit;
    digit = value % 10u;
    value /= 10u;
    --idx;
    buf[idx] = (char)('0' + (int)digit);
  } while (value != 0u);
  return lonejson__emit(sink, user, error, buf + idx, sizeof(buf) - idx);
}

static LONEJSON__INLINE lonejson_status
lonejson__emit_i64_value(lonejson_sink_fn sink, void *user,
                         lonejson_error *error, lonejson_int64 value) {
  lonejson_status status;
  lonejson_uint64 magnitude;

  if (value < 0) {
    status = lonejson__emit_cstr(sink, user, error, "-");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    magnitude = (lonejson_uint64)(-(value + 1)) + 1u;
  } else {
    magnitude = (lonejson_uint64)value;
  }
  return lonejson__emit_u64_value(sink, user, error, magnitude);
}

static lonejson_status lonejson__emit_base64_bytes(lonejson_sink_fn sink,
                                                   void *user,
                                                   lonejson_error *error,
                                                   const unsigned char *data,
                                                   size_t len) {
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
  return lonejson__emit(sink, user, error, out, sizeof(out));
}

static lonejson_status
lonejson__serialize_spooled_text(const lonejson_spooled *value,
                                 lonejson_sink_fn sink, void *user,
                                 lonejson_error *error) {
  unsigned char buffer[4096];
  lonejson_spooled cursor;
  lonejson_status status;

  status = lonejson__emit_cstr(sink, user, error, "\"");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  cursor = *value;
  status = lonejson_spooled_rewind(&cursor, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  for (;;) {
    lonejson_read_result chunk =
        lonejson_spooled_read(&cursor, buffer, sizeof(buffer));
    if (chunk.error_code != 0) {
      if (error != NULL) {
        error->system_errno = chunk.error_code;
      }
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to read spooled text");
    }
    if (chunk.bytes_read != 0u) {
      status = lonejson__emit_escaped_fragment(sink, user, error, buffer,
                                               chunk.bytes_read);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    if (chunk.eof) {
      break;
    }
  }
  return lonejson__emit_cstr(sink, user, error, "\"");
}

static lonejson_status
lonejson__serialize_spooled_base64(const lonejson_spooled *value,
                                   lonejson_sink_fn sink, void *user,
                                   lonejson_error *error) {
  unsigned char buffer[4096];
  unsigned char carry[3];
  size_t carry_len;
  lonejson_spooled cursor;
  lonejson_status status;

  carry_len = 0u;
  status = lonejson__emit_cstr(sink, user, error, "\"");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  cursor = *value;
  status = lonejson_spooled_rewind(&cursor, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  for (;;) {
    lonejson_read_result chunk =
        lonejson_spooled_read(&cursor, buffer, sizeof(buffer));
    size_t offset;

    if (chunk.error_code != 0) {
      if (error != NULL) {
        error->system_errno = chunk.error_code;
      }
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to read spooled bytes");
    }
    offset = 0u;
    if (carry_len != 0u) {
      while (carry_len < 3u && offset < chunk.bytes_read) {
        carry[carry_len++] = buffer[offset++];
      }
      if (carry_len == 3u) {
        status = lonejson__emit_base64_bytes(sink, user, error, carry, 3u);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
        carry_len = 0u;
      }
    }
    while (offset + 3u <= chunk.bytes_read) {
      status =
          lonejson__emit_base64_bytes(sink, user, error, buffer + offset, 3u);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
      offset += 3u;
    }
    while (offset < chunk.bytes_read) {
      carry[carry_len++] = buffer[offset++];
    }
    if (chunk.eof) {
      break;
    }
  }
  if (carry_len != 0u) {
    status = lonejson__emit_base64_bytes(sink, user, error, carry, carry_len);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  return lonejson__emit_cstr(sink, user, error, "\"");
}

static lonejson_status
lonejson__serialize_source_text(const lonejson_source *value,
                                lonejson_sink_fn sink, void *user,
                                lonejson_error *error) {
  unsigned char buffer[4096];
  lonejson__source_cursor cursor;
  lonejson_read_result chunk;
  lonejson_status status;

  if (value->kind == LONEJSON_SOURCE_NONE) {
    return lonejson__emit_cstr(sink, user, error, "null");
  }
  status = lonejson__source_cursor_open(value, &cursor, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__emit_cstr(sink, user, error, "\"");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    lonejson__source_cursor_close(&cursor);
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
                                 "failed to read source text");
    }
    if (chunk.bytes_read != 0u) {
      status = lonejson__emit_escaped_fragment(sink, user, error, buffer,
                                               chunk.bytes_read);
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
  return lonejson__emit_cstr(sink, user, error, "\"");
}

static lonejson_status
lonejson__serialize_source_base64(const lonejson_source *value,
                                  lonejson_sink_fn sink, void *user,
                                  lonejson_error *error) {
  unsigned char buffer[4096];
  unsigned char carry[3];
  size_t carry_len;
  lonejson__source_cursor cursor;
  lonejson_read_result chunk;
  lonejson_status status;

  if (value->kind == LONEJSON_SOURCE_NONE) {
    return lonejson__emit_cstr(sink, user, error, "null");
  }
  carry_len = 0u;
  status = lonejson__source_cursor_open(value, &cursor, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__emit_cstr(sink, user, error, "\"");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    lonejson__source_cursor_close(&cursor);
    return status;
  }
  for (;;) {
    size_t offset;

    chunk = lonejson__source_cursor_read(&cursor, buffer, sizeof(buffer));
    if (chunk.error_code != 0) {
      if (error != NULL) {
        error->system_errno = chunk.error_code;
      }
      lonejson__source_cursor_close(&cursor);
      return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to read source bytes");
    }
    offset = 0u;
    if (carry_len != 0u) {
      while (carry_len < 3u && offset < chunk.bytes_read) {
        carry[carry_len++] = buffer[offset++];
      }
      if (carry_len == 3u) {
        status = lonejson__emit_base64_bytes(sink, user, error, carry, 3u);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          lonejson__source_cursor_close(&cursor);
          return status;
        }
        carry_len = 0u;
      }
    }
    while (offset + 3u <= chunk.bytes_read) {
      status =
          lonejson__emit_base64_bytes(sink, user, error, buffer + offset, 3u);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        lonejson__source_cursor_close(&cursor);
        return status;
      }
      offset += 3u;
    }
    while (offset < chunk.bytes_read) {
      carry[carry_len++] = buffer[offset++];
    }
    if (chunk.eof) {
      break;
    }
  }
  lonejson__source_cursor_close(&cursor);
  if (carry_len != 0u) {
    status = lonejson__emit_base64_bytes(sink, user, error, carry, carry_len);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  return lonejson__emit_cstr(sink, user, error, "\"");
}

static lonejson_status
lonejson__serialize_value_pretty(const lonejson_field *field, const void *ptr,
                                 lonejson__write_state *state) {
  size_t i;
  lonejson_status status;

  switch (field->kind) {
  case LONEJSON_FIELD_KIND_STRING:
    if (field->storage == LONEJSON_STORAGE_DYNAMIC) {
      const char *text = *(const char *const *)ptr;
      if (text == NULL) {
        return lonejson__emit_cstr(state->sink, state->user, state->error,
                                   "null");
      }
      return lonejson__emit_escaped_string(state->sink, state->user,
                                           state->error, text);
    }
    return lonejson__emit_escaped_string(state->sink, state->user, state->error,
                                         (const char *)ptr);
  case LONEJSON_FIELD_KIND_STRING_STREAM:
    return lonejson__serialize_spooled_text(
        (const lonejson_spooled *)ptr, state->sink, state->user, state->error);
  case LONEJSON_FIELD_KIND_BASE64_STREAM:
    return lonejson__serialize_spooled_base64(
        (const lonejson_spooled *)ptr, state->sink, state->user, state->error);
  case LONEJSON_FIELD_KIND_STRING_SOURCE:
    return lonejson__serialize_source_text(
        (const lonejson_source *)ptr, state->sink, state->user, state->error);
  case LONEJSON_FIELD_KIND_BASE64_SOURCE:
    return lonejson__serialize_source_base64(
        (const lonejson_source *)ptr, state->sink, state->user, state->error);
  case LONEJSON_FIELD_KIND_JSON_VALUE:
    return lonejson__json_transcode((const lonejson_json_value *)ptr,
                                    state->sink, state->user, state->error, 1,
                                    state->depth + 1u);
  case LONEJSON_FIELD_KIND_I64:
    return lonejson__emit_i64_value(state->sink, state->user, state->error,
                                    *(const lonejson_int64 *)ptr);
  case LONEJSON_FIELD_KIND_U64:
    return lonejson__emit_u64_value(state->sink, state->user, state->error,
                                    *(const lonejson_uint64 *)ptr);
  case LONEJSON_FIELD_KIND_F64:
    if (!lonejson__is_finite_f64(*(const double *)ptr)) {
      return lonejson__set_error(state->error, LONEJSON_STATUS_TYPE_MISMATCH,
                                 state->error ? state->error->offset : 0u,
                                 state->error ? state->error->line : 0u,
                                 state->error ? state->error->column : 0u,
                                 "non-finite double cannot be serialized");
    }
    return lonejson__emit_number_text(state->sink, state->user, state->error,
                                      "%.17g", *(const double *)ptr);
  case LONEJSON_FIELD_KIND_BOOL:
    return lonejson__emit_cstr(state->sink, state->user, state->error,
                               *(const bool *)ptr ? "true" : "false");
  case LONEJSON_FIELD_KIND_OBJECT:
    return lonejson__serialize_map_pretty(field->submap, ptr, state);
  case LONEJSON_FIELD_KIND_STRING_ARRAY: {
    const lonejson_string_array *arr = (const lonejson_string_array *)ptr;
    status = lonejson__emit_cstr(state->sink, state->user, state->error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (arr->count == 0u) {
      return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status =
            lonejson__emit_cstr(state->sink, state->user, state->error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      if (state->pretty) {
        status = lonejson__emit_pretty_indent(state, state->depth + 1u);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      status =
          lonejson__emit_escaped_string(state->sink, state->user, state->error,
                                        arr->items[i] ? arr->items[i] : "");
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    if (state->pretty) {
      status = lonejson__emit_pretty_indent(state, state->depth);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
  }
  case LONEJSON_FIELD_KIND_I64_ARRAY: {
    const lonejson_i64_array *arr = (const lonejson_i64_array *)ptr;
    status = lonejson__emit_cstr(state->sink, state->user, state->error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (arr->count == 0u) {
      return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status =
            lonejson__emit_cstr(state->sink, state->user, state->error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      if (state->pretty) {
        status = lonejson__emit_pretty_indent(state, state->depth + 1u);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      status = lonejson__emit_i64_value(state->sink, state->user, state->error,
                                        arr->items[i]);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    if (state->pretty) {
      status = lonejson__emit_pretty_indent(state, state->depth);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
  }
  case LONEJSON_FIELD_KIND_U64_ARRAY: {
    const lonejson_u64_array *arr = (const lonejson_u64_array *)ptr;
    status = lonejson__emit_cstr(state->sink, state->user, state->error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (arr->count == 0u) {
      return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status =
            lonejson__emit_cstr(state->sink, state->user, state->error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      if (state->pretty) {
        status = lonejson__emit_pretty_indent(state, state->depth + 1u);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      status = lonejson__emit_u64_value(state->sink, state->user, state->error,
                                        arr->items[i]);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    if (state->pretty) {
      status = lonejson__emit_pretty_indent(state, state->depth);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
  }
  case LONEJSON_FIELD_KIND_F64_ARRAY: {
    const lonejson_f64_array *arr = (const lonejson_f64_array *)ptr;
    status = lonejson__emit_cstr(state->sink, state->user, state->error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (arr->count == 0u) {
      return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status =
            lonejson__emit_cstr(state->sink, state->user, state->error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      if (state->pretty) {
        status = lonejson__emit_pretty_indent(state, state->depth + 1u);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      if (!lonejson__is_finite_f64(arr->items[i])) {
        return lonejson__set_error(
            state->error, LONEJSON_STATUS_TYPE_MISMATCH,
            state->error ? state->error->offset : 0u,
            state->error ? state->error->line : 0u,
            state->error ? state->error->column : 0u,
            "non-finite double array element cannot be serialized");
      }
      status = lonejson__emit_number_text(state->sink, state->user,
                                          state->error, "%.17g", arr->items[i]);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    if (state->pretty) {
      status = lonejson__emit_pretty_indent(state, state->depth);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
  }
  case LONEJSON_FIELD_KIND_BOOL_ARRAY: {
    const lonejson_bool_array *arr = (const lonejson_bool_array *)ptr;
    status = lonejson__emit_cstr(state->sink, state->user, state->error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (arr->count == 0u) {
      return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status =
            lonejson__emit_cstr(state->sink, state->user, state->error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      if (state->pretty) {
        status = lonejson__emit_pretty_indent(state, state->depth + 1u);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      status = lonejson__emit_cstr(state->sink, state->user, state->error,
                                   arr->items[i] ? "true" : "false");
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    if (state->pretty) {
      status = lonejson__emit_pretty_indent(state, state->depth);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
  }
  case LONEJSON_FIELD_KIND_OBJECT_ARRAY: {
    const lonejson_object_array *arr = (const lonejson_object_array *)ptr;
    status = lonejson__emit_cstr(state->sink, state->user, state->error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (arr->count == 0u) {
      return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
    }
    for (i = 0; i < arr->count; ++i) {
      const void *elem =
          (const unsigned char *)arr->items + (i * field->elem_size);
      if (i != 0u) {
        status =
            lonejson__emit_cstr(state->sink, state->user, state->error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      if (state->pretty) {
        status = lonejson__emit_pretty_indent(state, state->depth + 1u);
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      ++state->depth;
      status = lonejson__serialize_map_pretty(field->submap, elem, state);
      --state->depth;
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    if (state->pretty) {
      status = lonejson__emit_pretty_indent(state, state->depth);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(state->sink, state->user, state->error, "]");
  }
  default:
    return lonejson__set_error(state->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               state->error ? state->error->offset : 0u,
                               state->error ? state->error->line : 0u,
                               state->error ? state->error->column : 0u,
                               "unsupported field kind");
  }
}

static lonejson_status lonejson__serialize_map_compact(const lonejson_map *map,
                                                       const void *src,
                                                       lonejson_sink_fn sink,
                                                       void *user,
                                                       lonejson_error *error) {
  size_t i;
  lonejson_status status;

  status = lonejson__emit_cstr(sink, user, error, "{");
  if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
    return status;
  }
  for (i = 0; i < map->field_count; ++i) {
    if (i != 0u) {
      status = lonejson__emit_cstr(sink, user, error, ",");
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    status =
        lonejson__emit_escaped_len(sink, user, error, map->fields[i].json_key,
                                   map->fields[i].json_key_len);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    status = lonejson__emit_cstr(sink, user, error, ":");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    status = lonejson__serialize_value_compact(
        &map->fields[i], lonejson__field_cptr(src, &map->fields[i]), sink, user,
        error);
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
  }
  return lonejson__emit_cstr(sink, user, error, "}");
}

static lonejson_status
lonejson__serialize_value_compact(const lonejson_field *field, const void *ptr,
                                  lonejson_sink_fn sink, void *user,
                                  lonejson_error *error) {
  size_t i;
  lonejson_status status;

  switch (field->kind) {
  case LONEJSON_FIELD_KIND_STRING:
    if (field->storage == LONEJSON_STORAGE_DYNAMIC) {
      const char *text = *(const char *const *)ptr;
      if (text == NULL) {
        return lonejson__emit_cstr(sink, user, error, "null");
      }
      return lonejson__emit_escaped_string(sink, user, error, text);
    }
    return lonejson__emit_escaped_string(sink, user, error, (const char *)ptr);
  case LONEJSON_FIELD_KIND_STRING_STREAM:
    return lonejson__serialize_spooled_text((const lonejson_spooled *)ptr, sink,
                                            user, error);
  case LONEJSON_FIELD_KIND_BASE64_STREAM:
    return lonejson__serialize_spooled_base64((const lonejson_spooled *)ptr,
                                              sink, user, error);
  case LONEJSON_FIELD_KIND_STRING_SOURCE:
    return lonejson__serialize_source_text((const lonejson_source *)ptr, sink,
                                           user, error);
  case LONEJSON_FIELD_KIND_BASE64_SOURCE:
    return lonejson__serialize_source_base64((const lonejson_source *)ptr, sink,
                                             user, error);
  case LONEJSON_FIELD_KIND_JSON_VALUE:
    return lonejson__json_transcode((const lonejson_json_value *)ptr, sink,
                                    user, error, 0, 0u);
  case LONEJSON_FIELD_KIND_I64:
    return lonejson__emit_i64_value(sink, user, error,
                                    *(const lonejson_int64 *)ptr);
  case LONEJSON_FIELD_KIND_U64:
    return lonejson__emit_u64_value(sink, user, error,
                                    *(const lonejson_uint64 *)ptr);
  case LONEJSON_FIELD_KIND_F64:
    if (!lonejson__is_finite_f64(*(const double *)ptr)) {
      return lonejson__set_error(
          error, LONEJSON_STATUS_TYPE_MISMATCH, error ? error->offset : 0u,
          error ? error->line : 0u, error ? error->column : 0u,
          "non-finite double cannot be serialized");
    }
    return lonejson__emit_number_text(sink, user, error, "%.17g",
                                      *(const double *)ptr);
  case LONEJSON_FIELD_KIND_BOOL:
    return lonejson__emit_cstr(sink, user, error,
                               *(const bool *)ptr ? "true" : "false");
  case LONEJSON_FIELD_KIND_OBJECT:
    return lonejson__serialize_map_compact(field->submap, ptr, sink, user,
                                           error);
  case LONEJSON_FIELD_KIND_STRING_ARRAY: {
    const lonejson_string_array *arr = (const lonejson_string_array *)ptr;
    status = lonejson__emit_cstr(sink, user, error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status = lonejson__emit_cstr(sink, user, error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      status = lonejson__emit_escaped_string(
          sink, user, error, arr->items[i] ? arr->items[i] : "");
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(sink, user, error, "]");
  }
  case LONEJSON_FIELD_KIND_I64_ARRAY: {
    const lonejson_i64_array *arr = (const lonejson_i64_array *)ptr;
    status = lonejson__emit_cstr(sink, user, error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status = lonejson__emit_cstr(sink, user, error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      status = lonejson__emit_i64_value(sink, user, error, arr->items[i]);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(sink, user, error, "]");
  }
  case LONEJSON_FIELD_KIND_U64_ARRAY: {
    const lonejson_u64_array *arr = (const lonejson_u64_array *)ptr;
    status = lonejson__emit_cstr(sink, user, error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status = lonejson__emit_cstr(sink, user, error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      status = lonejson__emit_u64_value(sink, user, error, arr->items[i]);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(sink, user, error, "]");
  }
  case LONEJSON_FIELD_KIND_F64_ARRAY: {
    const lonejson_f64_array *arr = (const lonejson_f64_array *)ptr;
    status = lonejson__emit_cstr(sink, user, error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status = lonejson__emit_cstr(sink, user, error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      if (!lonejson__is_finite_f64(arr->items[i])) {
        return lonejson__set_error(
            error, LONEJSON_STATUS_TYPE_MISMATCH, error ? error->offset : 0u,
            error ? error->line : 0u, error ? error->column : 0u,
            "non-finite double array element cannot be serialized");
      }
      status =
          lonejson__emit_number_text(sink, user, error, "%.17g", arr->items[i]);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(sink, user, error, "]");
  }
  case LONEJSON_FIELD_KIND_BOOL_ARRAY: {
    const lonejson_bool_array *arr = (const lonejson_bool_array *)ptr;
    status = lonejson__emit_cstr(sink, user, error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      if (i != 0u) {
        status = lonejson__emit_cstr(sink, user, error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      status = lonejson__emit_cstr(sink, user, error,
                                   arr->items[i] ? "true" : "false");
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(sink, user, error, "]");
  }
  case LONEJSON_FIELD_KIND_OBJECT_ARRAY: {
    const lonejson_object_array *arr = (const lonejson_object_array *)ptr;
    status = lonejson__emit_cstr(sink, user, error, "[");
    if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    for (i = 0; i < arr->count; ++i) {
      const void *elem =
          (const unsigned char *)arr->items + (i * field->elem_size);
      if (i != 0u) {
        status = lonejson__emit_cstr(sink, user, error, ",");
        if (status != LONEJSON_STATUS_OK &&
            status != LONEJSON_STATUS_TRUNCATED) {
          return status;
        }
      }
      status = lonejson__serialize_map_compact(field->submap, elem, sink, user,
                                               error);
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_TRUNCATED) {
        return status;
      }
    }
    return lonejson__emit_cstr(sink, user, error, "]");
  }
  default:
    return lonejson__set_error(
        error, LONEJSON_STATUS_INTERNAL_ERROR, error ? error->offset : 0u,
        error ? error->line : 0u, error ? error->column : 0u,
        "unsupported field kind");
  }
}
