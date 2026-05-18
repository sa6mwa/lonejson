typedef enum lonejson__value_rewrite_frame_kind {
  LONEJSON__VALUE_REWRITE_FRAME_OBJECT = 1,
  LONEJSON__VALUE_REWRITE_FRAME_ARRAY = 2
} lonejson__value_rewrite_frame_kind;

typedef struct lonejson__value_rewrite_frame {
  lonejson__value_rewrite_frame_kind kind;
  size_t path_len;
  size_t count;
  int prefix_matches;
  int saw_target_child;
  char *key;
  size_t key_len;
  size_t key_cap;
} lonejson__value_rewrite_frame;

typedef struct lonejson__value_rewrite_state {
  lonejson_writer writer;
  lonejson_value_rewrite_options options;
  lonejson_parse_options parse_options;
  lonejson_allocator allocator;
  lonejson_error *error;
  lonejson__value_rewrite_frame *frames;
  size_t frame_count;
  size_t frame_cap;
  lonejson__byte_buffer number;
  size_t number_limit;
  int found;
  int current_emit;
  int skipping;
  size_t skip_depth;
} lonejson__value_rewrite_state;

typedef struct lonejson__value_rewrite_selector {
  lonejson_allocator allocator;
  char **segments;
  size_t segment_count;
  size_t segment_cap;
} lonejson__value_rewrite_selector;

static int lonejson__value_rewrite_segment_is_index(const char *segment) {
  const unsigned char *p;

  if (segment == NULL || segment[0] == '\0') {
    return 0;
  }
  p = (const unsigned char *)segment;
  while (*p != '\0') {
    if (!isdigit(*p)) {
      return 0;
    }
    p++;
  }
  return 1;
}

static void lonejson__value_rewrite_selector_cleanup(
    lonejson__value_rewrite_selector *selector) {
  size_t i;

  if (selector == NULL) {
    return;
  }
  for (i = 0u; i < selector->segment_count; ++i) {
    if (selector->segments[i] != NULL) {
      lonejson__buffer_free(&selector->allocator, selector->segments[i],
                            strlen(selector->segments[i]) + 1u);
    }
  }
  lonejson__buffer_free(&selector->allocator, selector->segments,
                        selector->segment_cap * sizeof(*selector->segments));
  memset(selector, 0, sizeof(*selector));
}

static lonejson_status lonejson__value_rewrite_selector_add(
    lonejson__value_rewrite_selector *selector, const char *data, size_t len,
    lonejson_error *error) {
  char **next_segments;
  char *copy;
  size_t next_cap;

  if (len == 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "value rewrite selector contains empty segment");
  }
  if (selector->segment_count == selector->segment_cap) {
    next_cap = selector->segment_cap == 0u ? 4u : selector->segment_cap * 2u;
    next_segments = (char **)lonejson__buffer_realloc(
        &selector->allocator, selector->segments,
        selector->segment_cap * sizeof(*selector->segments),
        next_cap * sizeof(*selector->segments));
    if (next_segments == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u,
                                 0u, 0u,
                                 "failed to allocate value rewrite selector");
    }
    selector->segments = next_segments;
    selector->segment_cap = next_cap;
  }
  copy = (char *)lonejson__buffer_alloc(&selector->allocator, len + 1u);
  if (copy == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u,
                               0u,
                               "failed to allocate value rewrite selector "
                               "segment");
  }
  memcpy(copy, data, len);
  copy[len] = '\0';
  selector->segments[selector->segment_count++] = copy;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__value_rewrite_parse_selector(
    lonejson__value_rewrite_selector *selector, const char *text,
    const lonejson_allocator *allocator, lonejson_error *error) {
  lonejson__byte_buffer segment;
  lonejson_status status;
  const char *p;

  memset(selector, 0, sizeof(*selector));
  selector->allocator = lonejson__allocator_resolve(allocator);
  memset(&segment, 0, sizeof(segment));
  if (text == NULL || text[0] == '\0') {
    return LONEJSON_STATUS_OK;
  }
  p = text;
  while (*p != '\0') {
    if (*p == '.') {
      status = lonejson__value_rewrite_selector_add(
          selector, segment.data, segment.len, error);
      if (status != LONEJSON_STATUS_OK) {
        lonejson__byte_free(&segment, &selector->allocator);
        return status;
      }
      lonejson__byte_reset(&segment);
      p++;
      continue;
    }
    if (*p == '\\') {
      p++;
      if (*p == '\0') {
        lonejson__byte_free(&segment, &selector->allocator);
        return lonejson__set_error(
            error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u, 0u,
            "value rewrite selector ends with an escape");
      }
    }
    status = lonejson__byte_append(&segment, p, 1u, SIZE_MAX - 1u,
                                   &selector->allocator, error);
    if (status != LONEJSON_STATUS_OK) {
      lonejson__byte_free(&segment, &selector->allocator);
      return status;
    }
    p++;
  }
  status = lonejson__value_rewrite_selector_add(selector, segment.data,
                                                segment.len, error);
  lonejson__byte_free(&segment, &selector->allocator);
  return status;
}

static lonejson__value_rewrite_frame *
lonejson__value_rewrite_top(lonejson__value_rewrite_state *state) {
  return state->frame_count == 0u ? NULL
                                  : &state->frames[state->frame_count - 1u];
}

static size_t lonejson__value_rewrite_format_size(char *buffer,
                                                  size_t capacity,
                                                  size_t value) {
  char tmp[32];
  size_t len;

  len = 0u;
  do {
    tmp[len++] = (char)('0' + (value % 10u));
    value /= 10u;
  } while (value != 0u && len < sizeof(tmp));
  if (len + 1u > capacity) {
    return 0u;
  }
  {
    size_t i;
    for (i = 0u; i < len; ++i) {
      buffer[i] = tmp[len - i - 1u];
    }
  }
  buffer[len] = '\0';
  return len;
}

static int lonejson__value_rewrite_source_is_valid(
    const lonejson_value_rewrite_replacement *source) {
  int forms;

  if (source == NULL) {
    return 0;
  }
  forms = 0;
  if (source->emit != NULL) {
    forms++;
  }
  if (source->json != NULL) {
    forms++;
  }
  if (source->map != NULL || source->src != NULL) {
    forms++;
  }
  if (forms != 1) {
    return 0;
  }
  if (source->emit != NULL) {
    return source->map == NULL && source->src == NULL && source->json == NULL;
  }
  if (source->json != NULL) {
    return source->map == NULL && source->src == NULL &&
           lonejson_json_value_is_rewindable(source->json);
  }
  return source->map != NULL && source->src != NULL;
}

static lonejson_status lonejson__value_rewrite_emit_source(
    lonejson__value_rewrite_state *state,
    const lonejson_value_rewrite_replacement *source) {
  if (!lonejson__value_rewrite_source_is_valid(source)) {
    return lonejson__set_error(state->error, LONEJSON_STATUS_INVALID_ARGUMENT,
                               0u, 0u, 0u,
                               "value rewrite replacement source is invalid");
  }
  if (source->emit != NULL) {
    return source->emit(&state->writer, source->emit_user, state->error);
  }
  if (source->json != NULL) {
    return lonejson_writer_json_value(&state->writer, source->json,
                                      state->error);
  }
  return lonejson_writer_mapped(&state->writer, source->map, source->src,
                                state->error);
}

static int lonejson__value_rewrite_parent_child_matches(
    lonejson__value_rewrite_state *state,
    const lonejson__value_rewrite_frame *parent) {
  char index_text[32];
  size_t len;

  if (parent == NULL || !parent->prefix_matches ||
      parent->path_len >= state->options.target_segment_count) {
    return 0;
  }
  if (parent->kind == LONEJSON__VALUE_REWRITE_FRAME_OBJECT) {
    return parent->key_len ==
               strlen(state->options.target_segments[parent->path_len]) &&
           memcmp(parent->key, state->options.target_segments[parent->path_len],
                  parent->key_len) == 0;
  }
  len = lonejson__value_rewrite_format_size(index_text, sizeof(index_text),
                                            parent->count);
  if (len == 0u) {
    return 0;
  }
  return strcmp(index_text, state->options.target_segments[parent->path_len]) ==
         0;
}

static lonejson_status lonejson__value_rewrite_push_frame(
    lonejson__value_rewrite_state *state, lonejson__value_rewrite_frame_kind kind,
    size_t path_len, int prefix_matches) {
  lonejson__value_rewrite_frame *next;
  size_t next_cap;

  if (state->frame_count == state->frame_cap) {
    next_cap = state->frame_cap == 0u ? 8u : state->frame_cap * 2u;
    next = (lonejson__value_rewrite_frame *)lonejson__buffer_realloc(
        &state->allocator, state->frames,
        state->frame_cap * sizeof(*state->frames),
        next_cap * sizeof(*state->frames));
    if (next == NULL) {
      return lonejson__set_error(state->error,
                                 LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
                                 "failed to grow value rewrite stack");
    }
    state->frames = next;
    state->frame_cap = next_cap;
  }
  memset(&state->frames[state->frame_count], 0,
         sizeof(state->frames[state->frame_count]));
  state->frames[state->frame_count].kind = kind;
  state->frames[state->frame_count].path_len = path_len;
  state->frames[state->frame_count].prefix_matches = prefix_matches;
  state->frame_count++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__value_rewrite_pop_frame(lonejson__value_rewrite_state *state) {
  lonejson__value_rewrite_frame *frame;

  if (state->frame_count == 0u) {
    return lonejson__set_error(state->error, LONEJSON_STATUS_INTERNAL_ERROR, 0u,
                               0u, 0u, "value rewrite frame underflow");
  }
  frame = &state->frames[state->frame_count - 1u];
  lonejson__buffer_free(&state->allocator, frame->key, frame->key_cap);
  memset(frame, 0, sizeof(*frame));
  state->frame_count--;
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__value_rewrite_emit_missing_chain(
    lonejson__value_rewrite_state *state,
    lonejson__value_rewrite_frame *frame) {
  size_t i;
  size_t open_objects;
  lonejson_status status;

  if (state->found || state->options.action != LONEJSON_VALUE_REWRITE_REPLACE ||
      !frame->prefix_matches ||
      frame->path_len >= state->options.target_segment_count ||
      frame->saw_target_child ||
      lonejson__value_rewrite_segment_is_index(
          state->options.target_segments[frame->path_len])) {
    return LONEJSON_STATUS_OK;
  }
  status = lonejson_writer_key(&state->writer,
                               state->options.target_segments[frame->path_len],
                               strlen(state->options
                                          .target_segments[frame->path_len]),
                               state->error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (frame->path_len + 1u == state->options.target_segment_count) {
    status =
        lonejson__value_rewrite_emit_source(state, &state->options.replacement);
    state->found = status == LONEJSON_STATUS_OK;
    return status;
  }
  open_objects = 0u;
  status = lonejson_writer_begin_object(&state->writer, state->error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  open_objects++;
  for (i = frame->path_len + 1u; i < state->options.target_segment_count; ++i) {
    if (lonejson__value_rewrite_segment_is_index(
            state->options.target_segments[i])) {
      return lonejson__set_error(
          state->error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u, 0u,
          "value rewrite cannot synthesize a missing array index");
    }
    if (i + 1u == state->options.target_segment_count) {
      status = lonejson_writer_key(&state->writer,
                                   state->options.target_segments[i],
                                   strlen(state->options.target_segments[i]),
                                   state->error);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      status = lonejson__value_rewrite_emit_source(
          state, &state->options.replacement);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      break;
    }
    status = lonejson_writer_key(&state->writer,
                                 state->options.target_segments[i],
                                 strlen(state->options.target_segments[i]),
                                 state->error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    status = lonejson_writer_begin_object(&state->writer, state->error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    open_objects++;
  }
  while (open_objects != 0u) {
    status = lonejson_writer_end_object(&state->writer, state->error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    open_objects--;
  }
  state->found = 1;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__value_rewrite_before_value(
    lonejson__value_rewrite_state *state, int *emit_original,
    int *push_prefix_matches, size_t *value_path_len) {
  lonejson__value_rewrite_frame *parent;
  int child_matches;
  lonejson_status status;

  *emit_original = 0;
  *push_prefix_matches = 0;
  *value_path_len = 0u;
  if (state->skipping) {
    return LONEJSON_STATUS_OK;
  }
  parent = lonejson__value_rewrite_top(state);
  child_matches = lonejson__value_rewrite_parent_child_matches(state, parent);
  if (parent != NULL) {
    *value_path_len = parent->path_len + 1u;
    if (child_matches) {
      parent->saw_target_child = 1;
    }
    *push_prefix_matches = child_matches;
    if (parent->kind == LONEJSON__VALUE_REWRITE_FRAME_ARRAY) {
      parent->count++;
    }
  } else {
    *push_prefix_matches = 1;
  }
  if (*value_path_len == state->options.target_segment_count &&
      (parent == NULL || child_matches)) {
    state->found = 1;
    if (state->options.action == LONEJSON_VALUE_REWRITE_KEEP) {
      *emit_original = 1;
      if (parent != NULL &&
          parent->kind == LONEJSON__VALUE_REWRITE_FRAME_OBJECT) {
        status = lonejson_writer_key(&state->writer, parent->key,
                                     parent->key_len, state->error);
        if (status != LONEJSON_STATUS_OK) {
          return status;
        }
      }
      return LONEJSON_STATUS_OK;
    }
    if (state->options.action == LONEJSON_VALUE_REWRITE_DROP) {
      return LONEJSON_STATUS_OK;
    }
    if (parent != NULL &&
        parent->kind == LONEJSON__VALUE_REWRITE_FRAME_OBJECT) {
      status = lonejson_writer_key(&state->writer, parent->key,
                                   parent->key_len, state->error);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
    if (state->options.action == LONEJSON_VALUE_REWRITE_REPLACE) {
      return lonejson__value_rewrite_emit_source(state,
                                                 &state->options.replacement);
    }
  } else {
    *emit_original = 1;
    if (parent != NULL &&
        parent->kind == LONEJSON__VALUE_REWRITE_FRAME_OBJECT) {
      status = lonejson_writer_key(&state->writer, parent->key,
                                   parent->key_len, state->error);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__value_rewrite_object_begin(void *user,
                                                            lonejson_error *e) {
  lonejson__value_rewrite_state *state = (lonejson__value_rewrite_state *)user;
  int emit_original;
  int prefix_matches;
  size_t path_len;
  lonejson_status status;

  (void)e;
  if (state->skipping) {
    state->skip_depth++;
    return LONEJSON_STATUS_OK;
  }
  status = lonejson__value_rewrite_before_value(
      state, &emit_original, &prefix_matches, &path_len);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (!emit_original) {
    state->skipping = 1;
    state->skip_depth = 1u;
    return LONEJSON_STATUS_OK;
  }
  status = lonejson_writer_begin_object(&state->writer, state->error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__value_rewrite_push_frame(
      state, LONEJSON__VALUE_REWRITE_FRAME_OBJECT, path_len, prefix_matches);
}

static lonejson_status lonejson__value_rewrite_object_end(void *user,
                                                          lonejson_error *e) {
  lonejson__value_rewrite_state *state = (lonejson__value_rewrite_state *)user;
  lonejson__value_rewrite_frame *frame;
  lonejson_status status;

  (void)e;
  if (state->skipping) {
    state->skip_depth--;
    if (state->skip_depth == 0u) {
      state->skipping = 0;
    }
    return LONEJSON_STATUS_OK;
  }
  frame = lonejson__value_rewrite_top(state);
  if (frame == NULL || frame->kind != LONEJSON__VALUE_REWRITE_FRAME_OBJECT) {
    return lonejson__set_error(state->error, LONEJSON_STATUS_INTERNAL_ERROR, 0u,
                               0u, 0u,
                               "value rewrite object frame mismatch");
  }
  status = lonejson__value_rewrite_emit_missing_chain(state, frame);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson_writer_end_object(&state->writer, state->error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__value_rewrite_pop_frame(state);
}

static lonejson_status lonejson__value_rewrite_array_begin(void *user,
                                                           lonejson_error *e) {
  lonejson__value_rewrite_state *state = (lonejson__value_rewrite_state *)user;
  int emit_original;
  int prefix_matches;
  size_t path_len;
  lonejson_status status;

  (void)e;
  if (state->skipping) {
    state->skip_depth++;
    return LONEJSON_STATUS_OK;
  }
  status = lonejson__value_rewrite_before_value(
      state, &emit_original, &prefix_matches, &path_len);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (!emit_original) {
    state->skipping = 1;
    state->skip_depth = 1u;
    return LONEJSON_STATUS_OK;
  }
  status = lonejson_writer_begin_array(&state->writer, state->error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__value_rewrite_push_frame(
      state, LONEJSON__VALUE_REWRITE_FRAME_ARRAY, path_len, prefix_matches);
}

static lonejson_status lonejson__value_rewrite_array_end(void *user,
                                                         lonejson_error *e) {
  lonejson__value_rewrite_state *state = (lonejson__value_rewrite_state *)user;
  lonejson_status status;
  lonejson__value_rewrite_frame *frame;

  (void)e;
  if (state->skipping) {
    state->skip_depth--;
    if (state->skip_depth == 0u) {
      state->skipping = 0;
    }
    return LONEJSON_STATUS_OK;
  }
  frame = lonejson__value_rewrite_top(state);
  if (frame == NULL || frame->kind != LONEJSON__VALUE_REWRITE_FRAME_ARRAY) {
    return lonejson__set_error(state->error, LONEJSON_STATUS_INTERNAL_ERROR, 0u,
                               0u, 0u, "value rewrite array frame mismatch");
  }
  status = lonejson_writer_end_array(&state->writer, state->error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__value_rewrite_pop_frame(state);
}

static lonejson_status lonejson__value_rewrite_key_begin(void *user,
                                                         lonejson_error *e) {
  lonejson__value_rewrite_state *state = (lonejson__value_rewrite_state *)user;
  lonejson__value_rewrite_frame *frame = lonejson__value_rewrite_top(state);

  (void)e;
  if (!state->skipping && frame != NULL) {
    frame->key_len = 0u;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__value_rewrite_key_chunk(void *user,
                                                         const char *data,
                                                         size_t len,
                                                         lonejson_error *e) {
  lonejson__value_rewrite_state *state = (lonejson__value_rewrite_state *)user;
  lonejson__value_rewrite_frame *frame = lonejson__value_rewrite_top(state);
  char *next;
  size_t next_cap;

  (void)e;
  if (state->skipping || frame == NULL || len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  if (frame->key_len + len + 1u > frame->key_cap) {
    next_cap = frame->key_cap == 0u ? 32u : frame->key_cap;
    while (next_cap < frame->key_len + len + 1u) {
      next_cap *= 2u;
    }
    next = (char *)lonejson__buffer_realloc(&state->allocator, frame->key,
                                            frame->key_cap, next_cap);
    if (next == NULL) {
      return lonejson__set_error(state->error,
                                 LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
                                 "failed to allocate value rewrite key");
    }
    frame->key = next;
    frame->key_cap = next_cap;
  }
  memcpy(frame->key + frame->key_len, data, len);
  frame->key_len += len;
  frame->key[frame->key_len] = '\0';
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__value_rewrite_key_end(void *user,
                                                       lonejson_error *e) {
  (void)user;
  (void)e;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__value_rewrite_string_begin(void *user,
                                                            lonejson_error *e) {
  lonejson__value_rewrite_state *state = (lonejson__value_rewrite_state *)user;
  int prefix_matches;
  size_t path_len;
  lonejson_status status;

  (void)e;
  status = lonejson__value_rewrite_before_value(
      state, &state->current_emit, &prefix_matches, &path_len);
  (void)prefix_matches;
  (void)path_len;
  if (status != LONEJSON_STATUS_OK || !state->current_emit) {
    return status;
  }
  return lonejson_writer_string_begin(&state->writer, state->error);
}

static lonejson_status lonejson__value_rewrite_string_chunk(void *user,
                                                            const char *data,
                                                            size_t len,
                                                            lonejson_error *e) {
  lonejson__value_rewrite_state *state = (lonejson__value_rewrite_state *)user;
  (void)e;
  if (!state->current_emit) {
    return LONEJSON_STATUS_OK;
  }
  return lonejson_writer_string_chunk(&state->writer, data, len, state->error);
}

static lonejson_status lonejson__value_rewrite_string_end(void *user,
                                                          lonejson_error *e) {
  lonejson__value_rewrite_state *state = (lonejson__value_rewrite_state *)user;
  lonejson_status status;
  (void)e;
  if (!state->current_emit) {
    state->current_emit = 0;
    return LONEJSON_STATUS_OK;
  }
  status = lonejson_writer_string_end(&state->writer, state->error);
  state->current_emit = 0;
  return status;
}

static lonejson_status lonejson__value_rewrite_number_begin(void *user,
                                                            lonejson_error *e) {
  lonejson__value_rewrite_state *state = (lonejson__value_rewrite_state *)user;
  int prefix_matches;
  size_t path_len;
  lonejson_status status;

  (void)e;
  lonejson__byte_reset(&state->number);
  status = lonejson__value_rewrite_before_value(
      state, &state->current_emit, &prefix_matches, &path_len);
  (void)prefix_matches;
  (void)path_len;
  return status;
}

static lonejson_status lonejson__value_rewrite_number_chunk(void *user,
                                                            const char *data,
                                                            size_t len,
                                                            lonejson_error *e) {
  lonejson__value_rewrite_state *state = (lonejson__value_rewrite_state *)user;
  (void)e;
  if (!state->current_emit) {
    return LONEJSON_STATUS_OK;
  }
  return lonejson__byte_append(&state->number, data, len,
                               state->number_limit, &state->allocator,
                               state->error);
}

static lonejson_status lonejson__value_rewrite_number_end(void *user,
                                                          lonejson_error *e) {
  lonejson__value_rewrite_state *state = (lonejson__value_rewrite_state *)user;
  lonejson_status status;
  (void)e;
  if (!state->current_emit) {
    state->current_emit = 0;
    return LONEJSON_STATUS_OK;
  }
  status = lonejson_writer_number_text(&state->writer, state->number.data,
                                       state->number.len, state->error);
  state->current_emit = 0;
  lonejson__byte_reset(&state->number);
  return status;
}

static lonejson_status lonejson__value_rewrite_boolean(void *user, int value,
                                                       lonejson_error *e) {
  lonejson__value_rewrite_state *state = (lonejson__value_rewrite_state *)user;
  int emit_original;
  int prefix_matches;
  size_t path_len;
  lonejson_status status;

  (void)e;
  status = lonejson__value_rewrite_before_value(
      state, &emit_original, &prefix_matches, &path_len);
  (void)prefix_matches;
  (void)path_len;
  if (status != LONEJSON_STATUS_OK || !emit_original) {
    return status;
  }
  return lonejson_writer_bool(&state->writer, value, state->error);
}

static lonejson_status lonejson__value_rewrite_null(void *user,
                                                    lonejson_error *e) {
  lonejson__value_rewrite_state *state = (lonejson__value_rewrite_state *)user;
  int emit_original;
  int prefix_matches;
  size_t path_len;
  lonejson_status status;

  (void)e;
  status = lonejson__value_rewrite_before_value(
      state, &emit_original, &prefix_matches, &path_len);
  (void)prefix_matches;
  (void)path_len;
  if (status != LONEJSON_STATUS_OK || !emit_original) {
    return status;
  }
  return lonejson_writer_null(&state->writer, state->error);
}

static void
lonejson__value_rewrite_cleanup(lonejson__value_rewrite_state *state) {
  size_t i;

  lonejson_writer_cleanup(&state->writer);
  for (i = 0u; i < state->frame_count; ++i) {
    lonejson__buffer_free(&state->allocator, state->frames[i].key,
                          state->frames[i].key_cap);
  }
  lonejson__buffer_free(&state->allocator, state->frames,
                        state->frame_cap * sizeof(*state->frames));
  lonejson__byte_free(&state->number, &state->allocator);
}

static void lonejson__value_rewrite_resolve_limits(
    const lonejson_parse_options *options, lonejson_value_limits *limits) {
  *limits = lonejson_default_value_limits();
  if (options != NULL && options->max_depth != 0u) {
    limits->max_depth = options->max_depth;
  }
  (void)options;
}

static lonejson_status lonejson__value_rewrite_validate_options(
    const lonejson_value_rewrite_options *options, lonejson_error *error) {
  size_t i;

  if (options == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "value rewrite options are required");
  }
  if (options->target_segment_count != 0u && options->target_segments == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "value rewrite target segments are required");
  }
  for (i = 0u; i < options->target_segment_count; ++i) {
    if (options->target_segments[i] == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u,
                                 0u, 0u,
                                 "value rewrite target segment is NULL");
    }
  }
  if (options->action == LONEJSON_VALUE_REWRITE_REPLACE &&
      !lonejson__value_rewrite_source_is_valid(&options->replacement)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "value rewrite replacement source is invalid");
  }
  if (options->action == LONEJSON_VALUE_REWRITE_DROP &&
      options->target_segment_count == 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "value rewrite cannot drop the root value");
  }
  if (options->action != LONEJSON_VALUE_REWRITE_KEEP &&
      options->action != LONEJSON_VALUE_REWRITE_DROP &&
      options->action != LONEJSON_VALUE_REWRITE_REPLACE) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "invalid value rewrite action");
  }
  if (options->parse_options != NULL &&
      !LONEJSON__ALLOCATOR_IS_VALID_CONFIG(options->parse_options->allocator)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "allocator must provide either all callbacks "
                               "or none");
  }
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_value_rewrite_reader(
    lonejson_reader_fn reader, void *reader_user, lonejson_sink_fn sink,
    void *sink_user, const lonejson_value_rewrite_options *options,
    lonejson_error *error) {
  lonejson__value_rewrite_state state;
  lonejson_value_visitor visitor;
  lonejson_value_limits limits;
  lonejson_status status;

  if (reader == NULL || sink == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "reader and sink are required");
  }
  status = lonejson__value_rewrite_validate_options(options, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  memset(&state, 0, sizeof(state));
  state.options = *options;
  state.parse_options = options->parse_options != NULL
                            ? *options->parse_options
                            : lonejson_default_parse_options();
  state.allocator = lonejson__allocator_resolve(state.parse_options.allocator);
  state.error = error;
  lonejson__clear_error(error);
  status = lonejson_writer_init_sink(&state.writer, sink, sink_user, NULL,
                                     error);
  if (status == LONEJSON_STATUS_OK) {
    visitor = lonejson_default_value_visitor();
    visitor.object_begin = lonejson__value_rewrite_object_begin;
    visitor.object_end = lonejson__value_rewrite_object_end;
    visitor.object_key_begin = lonejson__value_rewrite_key_begin;
    visitor.object_key_chunk = lonejson__value_rewrite_key_chunk;
    visitor.object_key_end = lonejson__value_rewrite_key_end;
    visitor.array_begin = lonejson__value_rewrite_array_begin;
    visitor.array_end = lonejson__value_rewrite_array_end;
    visitor.string_begin = lonejson__value_rewrite_string_begin;
    visitor.string_chunk = lonejson__value_rewrite_string_chunk;
    visitor.string_end = lonejson__value_rewrite_string_end;
    visitor.number_begin = lonejson__value_rewrite_number_begin;
    visitor.number_chunk = lonejson__value_rewrite_number_chunk;
    visitor.number_end = lonejson__value_rewrite_number_end;
    visitor.boolean_value = lonejson__value_rewrite_boolean;
    visitor.null_value = lonejson__value_rewrite_null;
    lonejson__value_rewrite_resolve_limits(&state.parse_options, &limits);
    state.number_limit = limits.max_number_bytes;
    status = lonejson_visit_value_reader(reader, reader_user, &visitor, &state,
                                         &limits, error);
  }
  if (status == LONEJSON_STATUS_OK && !state.found &&
      state.options.action != LONEJSON_VALUE_REWRITE_KEEP) {
    status = lonejson__set_error(error, LONEJSON_STATUS_TYPE_MISMATCH, 0u, 0u,
                                 0u, "value rewrite target was not found");
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_writer_finish(&state.writer, error);
  }
  lonejson__value_rewrite_cleanup(&state);
  return status;
}

lonejson_status lonejson_value_rewrite_filep(
    FILE *fp, lonejson_sink_fn sink, void *sink_user,
    const lonejson_value_rewrite_options *options, lonejson_error *error) {
  if (fp == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "input file is required");
  }
  return lonejson_value_rewrite_reader(lonejson__file_reader, fp, sink,
                                       sink_user, options, error);
}

lonejson_status lonejson_value_rewrite_fd(
    int fd, lonejson_sink_fn sink, void *sink_user,
    const lonejson_value_rewrite_options *options, lonejson_error *error) {
  if (fd < 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "input fd must be non-negative");
  }
  return lonejson_value_rewrite_reader(lonejson__fd_reader, &fd, sink,
                                       sink_user, options, error);
}

lonejson_status lonejson_value_rewrite_path(
    const char *input_path, const char *output_path,
    const lonejson_value_rewrite_options *options, lonejson_error *error) {
  struct stat input_stat;
  struct stat output_stat;
  FILE *in;
  FILE *out;
  lonejson_status status;

  if (input_path == NULL || output_path == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "input and output paths are required");
  }
  if (stat(input_path, &input_stat) == 0 &&
      stat(output_path, &output_stat) == 0 &&
      input_stat.st_dev == output_stat.st_dev &&
      input_stat.st_ino == output_stat.st_ino) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "input and output paths must refer to distinct "
                               "files");
  }
  in = fopen(input_path, "rb");
  if (in == NULL) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                               "failed to open value rewrite input path");
  }
  out = fopen(output_path, "wb");
  if (out == NULL) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    fclose(in);
    return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                               "failed to open value rewrite output path");
  }
  status = lonejson_value_rewrite_filep(in, lonejson__sink_file, out, options,
                                        error);
  if (fclose(out) != 0 && status == LONEJSON_STATUS_OK) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    status = lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 0u, 0u,
                                 "failed to close value rewrite output path");
  }
  fclose(in);
  return status;
}

static lonejson_status lonejson__value_rewrite_selector_to_options(
    const lonejson_value_rewrite_selector_options *selector_options,
    lonejson_value_rewrite_options *options,
    lonejson__value_rewrite_selector *selector, lonejson_error *error) {
  lonejson_parse_options parse_options;
  lonejson_status status;

  if (selector_options == NULL || options == NULL || selector == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "value rewrite selector options are required");
  }
  parse_options = selector_options->parse_options != NULL
                      ? *selector_options->parse_options
                      : lonejson_default_parse_options();
  status = lonejson__value_rewrite_parse_selector(
      selector, selector_options->selector, parse_options.allocator, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  memset(options, 0, sizeof(*options));
  options->parse_options = selector_options->parse_options;
  options->target_segments = (const char *const *)selector->segments;
  options->target_segment_count = selector->segment_count;
  options->action = selector_options->action;
  options->replacement = selector_options->replacement;
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_value_rewrite_selector_reader(
    lonejson_reader_fn reader, void *reader_user, lonejson_sink_fn sink,
    void *sink_user, const lonejson_value_rewrite_selector_options *options,
    lonejson_error *error) {
  lonejson__value_rewrite_selector selector;
  lonejson_value_rewrite_options normalized;
  lonejson_status status;

  memset(&selector, 0, sizeof(selector));
  status = lonejson__value_rewrite_selector_to_options(options, &normalized,
                                                       &selector, error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_value_rewrite_reader(reader, reader_user, sink, sink_user,
                                           &normalized, error);
  }
  lonejson__value_rewrite_selector_cleanup(&selector);
  return status;
}

lonejson_status lonejson_value_rewrite_selector_filep(
    FILE *fp, lonejson_sink_fn sink, void *sink_user,
    const lonejson_value_rewrite_selector_options *options,
    lonejson_error *error) {
  if (fp == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "input file is required");
  }
  return lonejson_value_rewrite_selector_reader(lonejson__file_reader, fp, sink,
                                                sink_user, options, error);
}

lonejson_status lonejson_value_rewrite_selector_fd(
    int fd, lonejson_sink_fn sink, void *sink_user,
    const lonejson_value_rewrite_selector_options *options,
    lonejson_error *error) {
  if (fd < 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "input fd must be non-negative");
  }
  return lonejson_value_rewrite_selector_reader(lonejson__fd_reader, &fd, sink,
                                                sink_user, options, error);
}

lonejson_status lonejson_value_rewrite_selector_path(
    const char *input_path, const char *output_path,
    const lonejson_value_rewrite_selector_options *options,
    lonejson_error *error) {
  lonejson__value_rewrite_selector selector;
  lonejson_value_rewrite_options normalized;
  lonejson_status status;

  memset(&selector, 0, sizeof(selector));
  status = lonejson__value_rewrite_selector_to_options(options, &normalized,
                                                       &selector, error);
  if (status == LONEJSON_STATUS_OK) {
    status =
        lonejson_value_rewrite_path(input_path, output_path, &normalized, error);
  }
  lonejson__value_rewrite_selector_cleanup(&selector);
  return status;
}
