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
    static const double powers10[] = {1e1,  1e2,  1e4,   1e8,  1e16,
                                      1e32, 1e64, 1e128, 1e256};
    static const double neg_powers10[] = {
        1e-1, 1e-2, 1e-4, 1e-8, 1e-16, 1e-32, 1e-64, 1e-128, 1e-256};
    const double *powers = exponent_negative ? neg_powers10 : powers10;
    size_t power_index = 0u;

    while (exponent != 0u &&
           power_index < sizeof(powers10) / sizeof(powers10[0])) {
      if ((exponent & 1u) != 0u) {
        scale *= powers[power_index];
      }
      exponent >>= 1u;
      power_index++;
    }
    if (exponent != 0u || !lonejson__is_finite_f64(scale)) {
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
  if ((field->flags & LONEJSON_FIELD_ACCEPT_NULL) != 0u &&
      lonejson__field_has_presence(field)) {
    switch (field->kind) {
    case LONEJSON_FIELD_KIND_I64:
      *(lonejson_int64 *)ptr = 0;
      return LONEJSON_STATUS_OK;
    case LONEJSON_FIELD_KIND_U64:
      *(lonejson_uint64 *)ptr = 0u;
      return LONEJSON_STATUS_OK;
    case LONEJSON_FIELD_KIND_F64:
      *(double *)ptr = 0.0;
      return LONEJSON_STATUS_OK;
    case LONEJSON_FIELD_KIND_BOOL:
      *(bool *)ptr = false;
      return LONEJSON_STATUS_OK;
    default:
      break;
    }
  }
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

  if (value == NULL) {
    return lonejson__set_error(&parser->error, LONEJSON_STATUS_INTERNAL_ERROR,
                               parser->error.offset, parser->error.line,
                               parser->error.column,
                               "JSON value stream has no destination");
  }
  parser->json_stream_text_bytes = 0u;
  parser->json_stream_text_is_key = is_key;
  if (!lonejson__json_value_parse_visitor_active(parser)) {
    return lonejson__json_value_emit(parser, "\"", 1u);
  }
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
    return lonejson__emit_escaped_fragment(
        parser->json_stream_sink_active ? value->parse_sink
                                        : lonejson__json_buffer_sink,
        parser->json_stream_sink_active ? value->parse_sink_user : value,
        &parser->error, (const unsigned char *)data, len);
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

  parser->json_stream_text_bytes = 0u;
  parser->json_stream_text_is_key = 0;
  if (!lonejson__json_value_parse_visitor_active(parser)) {
    return lonejson__json_value_emit(parser, "\"", 1u);
  }
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
  memset(slot, 0, arr->elem_size);
  if (field->submap != NULL) {
    lonejson__init_map_with_allocator(field->submap, slot, &parser->allocator);
  }
  arr->count++;
  return slot;
}
