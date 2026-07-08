typedef enum lonejson__candidate_transform_frame_kind {
  LONEJSON__CANDIDATE_TRANSFORM_OBJECT = 1,
  LONEJSON__CANDIDATE_TRANSFORM_ARRAY = 2
} lonejson__candidate_transform_frame_kind;

typedef struct lonejson__candidate_transform_frame {
  lonejson__candidate_transform_frame_kind kind;
  lonejson__byte_buffer key;
  lonejson__byte_buffer seen_keys;
  lonejson_uint64 output_index;
  int projection_passthrough;
} lonejson__candidate_transform_frame;

typedef struct lonejson__candidate_transform_state {
  const lonejson_candidate_transform_options *options;
  const lonejson_runtime *runtime;
  const lonejson_allocator *allocator;
  lonejson_error *error;
  const lonejson_candidate_info *candidate_override;
  const lonejson_candidate_transform_candidate_info *transform_override;
  lonejson_candidate_info candidate;
  lonejson_candidate_transform_candidate_info transform_candidate;
  lonejson_writer writer;
  lonejson_path_value_visitor visitor;
  lonejson_candidate_stream_options candidate_options;
  lonejson__candidate_transform_frame *frames;
  size_t frame_count;
  size_t frame_cap;
  int writer_open;
  int candidate_output_started;
  int emitted_any_candidate;
  int stopped;
  lonejson_status deferred_status;
  int skipping;
  size_t skip_depth;
  int skip_after_member;
  int current_emit;
  int current_scalar_replace;
  int current_scalar_materialized;
  int current_projection_descendant;
  int projection_enabled;
  lonejson__candidate_transform_frame_kind projection_root_kind;
  lonejson__byte_buffer scalar;
  lonejson_candidate_transform_old_value old_value;
  int suppress_result;
} lonejson__candidate_transform_state;

typedef struct lonejson__candidate_transform_gated_state {
  const lonejson_candidate_transform_options *options;
  const lonejson_runtime *runtime;
  lonejson_error *error;
  lonejson_candidate_info candidate;
  lonejson_candidate_transform_candidate_info transform_candidate;
  lonejson_status deferred_status;
} lonejson__candidate_transform_gated_state;

static lonejson_status lonejson__transform_candidates_reader_core(
    const lonejson_runtime *runtime_state, lonejson_reader_fn reader,
    void *reader_user, const lonejson_candidate_transform_options *options,
    lonejson_error *error, const lonejson_candidate_info *candidate_override,
    const lonejson_candidate_transform_candidate_info *transform_override,
    int suppress_result, int *stopped_out);

static lonejson_status lonejson__candidate_transform_validate_projection(
    const lonejson_candidate_transform_options *options,
    lonejson__candidate_transform_frame_kind *root_kind,
    lonejson_error *error);

static int lonejson__candidate_transform_parse_uint64(const char *data,
                                                       size_t len,
                                                       lonejson_uint64 *out);

static lonejson__candidate_transform_frame *
lonejson__candidate_transform_top(lonejson__candidate_transform_state *state) {
  return state->frame_count == 0u ? NULL
                                  : &state->frames[state->frame_count - 1u];
}

static int lonejson__candidate_transform_projection_has(
    const lonejson_candidate_transform_options *options) {
  return options != NULL && options->projection_paths != NULL &&
         options->projection_path_count != 0u;
}

static int lonejson__candidate_transform_projection_passthrough_active(
    const lonejson__candidate_transform_state *state) {
  size_t i;

  if (state == NULL) {
    return 0;
  }
  for (i = 0u; i < state->frame_count; ++i) {
    if (state->frames[i].projection_passthrough) {
      return 1;
    }
  }
  return 0;
}

static const char *lonejson__candidate_transform_projection_key(
    const lonejson_candidate_transform_projection_segment *segment) {
  return segment != NULL && segment->key != NULL ? segment->key : "";
}

static int lonejson__candidate_transform_segment_matches_path(
    lonejson__candidate_transform_state *state, const lonejson_value_path *path,
    size_t index,
    const lonejson_candidate_transform_projection_segment *segment) {
  lonejson__candidate_transform_frame *parent;
  lonejson_uint64 parsed_index;

  if (path == NULL || index >= path->segment_count || segment == NULL ||
      index >= state->frame_count) {
    return 0;
  }
  parent = &state->frames[index];
  if (segment->kind == LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER) {
    return parent->kind == LONEJSON__CANDIDATE_TRANSFORM_OBJECT &&
           segment->key_len == path->segments[index].len &&
           (segment->key_len == 0u ||
            memcmp(lonejson__candidate_transform_projection_key(segment),
                   path->segments[index].data, segment->key_len) == 0);
  }
  if (segment->kind == LONEJSON_CANDIDATE_TRANSFORM_PROJECT_ARRAY_INDEX) {
    if (parent->kind != LONEJSON__CANDIDATE_TRANSFORM_ARRAY) {
      return 0;
    }
    return lonejson__candidate_transform_parse_uint64(
               path->segments[index].data, path->segments[index].len,
               &parsed_index) &&
           parsed_index == segment->index;
  }
  return 0;
}

static int lonejson__candidate_transform_projection_path_matches(
    lonejson__candidate_transform_state *state,
    const lonejson_candidate_transform_projection_path *rule,
    const lonejson_value_path *path) {
  size_t i;

  if (rule == NULL || path == NULL || rule->segment_count != path->segment_count) {
    return 0;
  }
  if (rule->segment_count == 0u) {
    return path->segment_count == 0u;
  }
  for (i = 0u; i < rule->segment_count; ++i) {
    if (!lonejson__candidate_transform_segment_matches_path(
            state, path, i, &rule->segments[i])) {
      return 0;
    }
  }
  return 1;
}

static int lonejson__candidate_transform_projection_path_has_prefix(
    lonejson__candidate_transform_state *state,
    const lonejson_candidate_transform_projection_path *rule,
    const lonejson_value_path *path) {
  size_t i;

  if (rule == NULL || path == NULL || path->segment_count > rule->segment_count) {
    return 0;
  }
  for (i = 0u; i < path->segment_count; ++i) {
    if (!lonejson__candidate_transform_segment_matches_path(
            state, path, i, &rule->segments[i])) {
      return 0;
    }
  }
  return 1;
}

static int lonejson__candidate_transform_projection_matches_exact(
    lonejson__candidate_transform_state *state, const lonejson_value_path *path) {
  size_t i;

  if (!state->projection_enabled) {
    return 1;
  }
  for (i = 0u; i < state->options->projection_path_count; ++i) {
    if (lonejson__candidate_transform_projection_path_matches(
            state, &state->options->projection_paths[i], path)) {
      return 1;
    }
  }
  return 0;
}

static int lonejson__candidate_transform_projection_has_descendant(
    lonejson__candidate_transform_state *state, const lonejson_value_path *path) {
  size_t i;

  if (!state->projection_enabled) {
    return 1;
  }
  for (i = 0u; i < state->options->projection_path_count; ++i) {
    const lonejson_candidate_transform_projection_path *rule =
        &state->options->projection_paths[i];
    if (rule->segment_count > (path != NULL ? path->segment_count : 0u) &&
        lonejson__candidate_transform_projection_path_has_prefix(state, rule,
                                                                 path)) {
      return 1;
    }
  }
  return 0;
}

static lonejson_status lonejson__candidate_transform_seen_key_add(
    lonejson__candidate_transform_state *state,
    lonejson__candidate_transform_frame *frame, const char *key, size_t len) {
  lonejson_status status;

  if (frame == NULL || key == NULL) {
    return LONEJSON_STATUS_OK;
  }
  if (len > (SIZE_MAX - 1u) - sizeof(len)) {
    return lonejson__set_error(state->error, LONEJSON_STATUS_OVERFLOW, 0u, 0u,
                               0u,
                               "candidate projection object key is too large");
  }
  status = lonejson__byte_append(&frame->seen_keys, &len, sizeof(len),
                                 (SIZE_MAX - 1u) - len, state->allocator,
                                 state->error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__byte_append(&frame->seen_keys, key, len, SIZE_MAX - 1u,
                               state->allocator, state->error);
}

static int lonejson__candidate_transform_seen_key_has(
    const lonejson__candidate_transform_frame *frame, const char *key,
    size_t len) {
  size_t pos;
  size_t item_len;
  const char *item;

  if (frame == NULL || key == NULL) {
    return 0;
  }
  pos = 0u;
  while (pos < frame->seen_keys.len) {
    if (frame->seen_keys.len - pos < sizeof(item_len)) {
      return 0;
    }
    memcpy(&item_len, frame->seen_keys.data + pos, sizeof(item_len));
    pos += sizeof(item_len);
    if (frame->seen_keys.len - pos < item_len) {
      return 0;
    }
    item = frame->seen_keys.data + pos;
    if (item_len == len && memcmp(item, key, len) == 0) {
      return 1;
    }
    pos += item_len;
  }
  return 0;
}

static lonejson_status lonejson__candidate_transform_push(
    lonejson__candidate_transform_state *state,
    lonejson__candidate_transform_frame_kind kind) {
  lonejson__candidate_transform_frame *next;
  size_t next_cap;

  if (state->frame_count == state->frame_cap) {
    next_cap = state->frame_cap == 0u ? 8u : state->frame_cap * 2u;
    next = (lonejson__candidate_transform_frame *)lonejson__buffer_realloc(
        state->allocator, state->frames,
        state->frame_cap * sizeof(*state->frames),
        next_cap * sizeof(*state->frames));
    if (next == NULL) {
      return lonejson__set_error(state->error,
                                 LONEJSON_STATUS_ALLOCATION_FAILED, 0u, 0u, 0u,
                                 "failed to allocate candidate transform "
                                 "frame stack");
    }
    state->frames = next;
    state->frame_cap = next_cap;
  }
  memset(&state->frames[state->frame_count], 0, sizeof(state->frames[0]));
  state->frames[state->frame_count].kind = kind;
  state->frame_count++;
  return LONEJSON_STATUS_OK;
}

static void
lonejson__candidate_transform_pop(lonejson__candidate_transform_state *state) {
  lonejson__candidate_transform_frame *frame;

  if (state->frame_count == 0u) {
    return;
  }
  frame = &state->frames[state->frame_count - 1u];
  lonejson__byte_free(&frame->key, state->allocator);
  lonejson__byte_free(&frame->seen_keys, state->allocator);
  memset(frame, 0, sizeof(*frame));
  state->frame_count--;
}

static void lonejson__candidate_transform_cleanup(
    lonejson__candidate_transform_state *state) {
  size_t i;

  if (state == NULL) {
    return;
  }
  if (state->writer_open) {
    lonejson_writer_cleanup(&state->writer);
  }
  for (i = 0u; i < state->frame_count; ++i) {
    lonejson__byte_free(&state->frames[i].key, state->allocator);
    lonejson__byte_free(&state->frames[i].seen_keys, state->allocator);
  }
  lonejson__byte_free(&state->scalar, state->allocator);
  lonejson__buffer_free(state->allocator, state->frames,
                        state->frame_cap * sizeof(*state->frames));
  memset(state, 0, sizeof(*state));
}

static lonejson_status lonejson__candidate_transform_record_status(
    lonejson__candidate_transform_state *state, lonejson_status status) {
  if (status == LONEJSON_STATUS_TRUNCATED &&
      state->deferred_status == LONEJSON_STATUS_OK) {
    state->deferred_status = status;
    state->stopped = 1;
    state->current_emit = 0;
  }
  return status;
}

static lonejson_read_result
lonejson__candidate_transform_spooled_reader(void *user, unsigned char *buffer,
                                             size_t capacity) {
  return lonejson_spooled_read((lonejson_spooled *)user, buffer, capacity);
}

static lonejson_status lonejson__candidate_transform_forward_event(
    lonejson__candidate_transform_state *state, lonejson_path_value_event_fn fn,
    const lonejson_value_path *path) {
  lonejson_status status;

  if (fn == NULL) {
    return LONEJSON_STATUS_OK;
  }
  status = fn(state->options->observer_user, path, state->error);
  return lonejson__candidate_transform_record_status(state, status);
}

static lonejson_status lonejson__candidate_transform_forward_chunk(
    lonejson__candidate_transform_state *state, lonejson_path_value_chunk_fn fn,
    const lonejson_value_path *path, const char *data, size_t len) {
  lonejson_status status;

  if (fn == NULL) {
    return LONEJSON_STATUS_OK;
  }
  status = fn(state->options->observer_user, path, data, len, state->error);
  return lonejson__candidate_transform_record_status(state, status);
}

static lonejson_status lonejson__candidate_transform_forward_bool(
    lonejson__candidate_transform_state *state, const lonejson_value_path *path,
    int value) {
  lonejson_status status;

  if (state->options->observer == NULL ||
      state->options->observer->boolean_value == NULL) {
    return LONEJSON_STATUS_OK;
  }
  status = state->options->observer->boolean_value(
      state->options->observer_user, path, value, state->error);
  return lonejson__candidate_transform_record_status(state, status);
}

static lonejson_status lonejson__candidate_transform_prefix(
    lonejson__candidate_transform_state *state) {
  if (state->candidate_output_started) {
    return LONEJSON_STATUS_OK;
  }
  state->candidate_output_started = 1;
  state->emitted_any_candidate = 1;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__candidate_transform_emit_key(
    lonejson__candidate_transform_state *state) {
  lonejson__candidate_transform_frame *frame;
  lonejson_status status;

  frame = lonejson__candidate_transform_top(state);
  if (frame == NULL || frame->kind != LONEJSON__CANDIDATE_TRANSFORM_OBJECT) {
    return LONEJSON_STATUS_OK;
  }
  status = lonejson__candidate_transform_record_status(
      state, lonejson_writer_key(&state->writer,
                                 frame->key.data != NULL ? frame->key.data : "",
                                 frame->key.len, state->error));
  if (status != LONEJSON_STATUS_OK || !state->projection_enabled) {
    return status;
  }
  return lonejson__candidate_transform_seen_key_add(
      state, frame, frame->key.data != NULL ? frame->key.data : "",
      frame->key.len);
}

static lonejson_status lonejson__candidate_transform_insert_for_top(
    lonejson__candidate_transform_state *state, const lonejson_value_path *path,
    lonejson_candidate_transform_insert_phase phase);

static int lonejson__candidate_transform_parse_uint64(const char *data,
                                                       size_t len,
                                                       lonejson_uint64 *out) {
  lonejson_uint64 value;
  size_t i;

  if (data == NULL || len == 0u || out == NULL) {
    return 0;
  }
  value = 0u;
  for (i = 0u; i < len; ++i) {
    unsigned char ch = (unsigned char)data[i];
    if (ch < (unsigned char)'0' || ch > (unsigned char)'9') {
      return 0;
    }
    if (value > (LONEJSON_UINT64_MAX / 10u)) {
      return 0;
    }
    value = (value * 10u) + (lonejson_uint64)(ch - (unsigned char)'0');
  }
  *out = value;
  return 1;
}

static lonejson_status lonejson__candidate_transform_emit_array_prefix(
    lonejson__candidate_transform_state *state, const lonejson_value_path *path) {
  lonejson__candidate_transform_frame *frame;
  lonejson_uint64 index;
  lonejson_status status;

  if (!state->projection_enabled ||
      lonejson__candidate_transform_projection_passthrough_active(state)) {
    return LONEJSON_STATUS_OK;
  }
  frame = lonejson__candidate_transform_top(state);
  if (frame == NULL || frame->kind != LONEJSON__CANDIDATE_TRANSFORM_ARRAY ||
      path == NULL || path->segment_count == 0u) {
    return LONEJSON_STATUS_OK;
  }
  if (!lonejson__candidate_transform_parse_uint64(
          path->segments[path->segment_count - 1u].data,
          path->segments[path->segment_count - 1u].len, &index)) {
    return lonejson__set_error(state->error, LONEJSON_STATUS_UNSUPPORTED, 0u,
                               0u, 0u,
                               "candidate projection array index is invalid");
  }
  while (frame->output_index < index) {
    status = lonejson__candidate_transform_record_status(
        state, lonejson_writer_null(&state->writer, state->error));
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    frame->output_index++;
  }
  if (frame->output_index == index) {
    frame->output_index++;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__candidate_transform_mark_projection_handled(
    lonejson__candidate_transform_state *state, const lonejson_value_path *path) {
  lonejson__candidate_transform_frame *frame;
  lonejson_uint64 index;
  lonejson_status status;

  if (!state->projection_enabled ||
      lonejson__candidate_transform_projection_passthrough_active(state) ||
      path == NULL || path->segment_count == 0u) {
    return LONEJSON_STATUS_OK;
  }
  frame = lonejson__candidate_transform_top(state);
  if (frame == NULL) {
    return LONEJSON_STATUS_OK;
  }
  if (frame->kind == LONEJSON__CANDIDATE_TRANSFORM_OBJECT) {
    return lonejson__candidate_transform_seen_key_add(
        state, frame, frame->key.data != NULL ? frame->key.data : "",
        frame->key.len);
  }
  if (frame->kind != LONEJSON__CANDIDATE_TRANSFORM_ARRAY) {
    return LONEJSON_STATUS_OK;
  }
  if (!lonejson__candidate_transform_parse_uint64(
          path->segments[path->segment_count - 1u].data,
          path->segments[path->segment_count - 1u].len, &index)) {
    return lonejson__set_error(state->error, LONEJSON_STATUS_UNSUPPORTED, 0u,
                               0u, 0u,
                               "candidate projection array index is invalid");
  }
  while (frame->output_index < index) {
    status = lonejson__candidate_transform_record_status(
        state, lonejson_writer_null(&state->writer, state->error));
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    frame->output_index++;
  }
  if (frame->output_index == index) {
    frame->output_index++;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__candidate_transform_prepare_emit(
    lonejson__candidate_transform_state *state,
    const lonejson_value_path *path) {
  lonejson_status status;

  status = lonejson__candidate_transform_prefix(state);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (state->projection_enabled &&
      !lonejson__candidate_transform_projection_passthrough_active(state)) {
    status = lonejson__candidate_transform_insert_for_top(
        state, path, LONEJSON_CANDIDATE_TRANSFORM_INSERT_BEFORE_MEMBER);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  status = lonejson__candidate_transform_emit_key(state);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__candidate_transform_emit_array_prefix(state, path);
}

static int lonejson__candidate_transform_projection_segments_equal(
    const lonejson_candidate_transform_projection_segment *a,
    const lonejson_candidate_transform_projection_segment *b) {
  if (a == NULL || b == NULL || a->kind != b->kind) {
    return 0;
  }
  if (a->kind == LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER) {
    return a->key_len == b->key_len &&
           (a->key_len == 0u ||
            memcmp(lonejson__candidate_transform_projection_key(a),
                   lonejson__candidate_transform_projection_key(b),
                   a->key_len) == 0);
  }
  if (a->kind == LONEJSON_CANDIDATE_TRANSFORM_PROJECT_ARRAY_INDEX) {
    return a->index == b->index;
  }
  return 0;
}

static int lonejson__candidate_transform_projection_rule_prefix_equal(
    const lonejson_candidate_transform_projection_path *rule,
    const lonejson_candidate_transform_projection_path *seed, size_t depth) {
  size_t i;

  if (rule == NULL || seed == NULL || rule->segment_count < depth ||
      seed->segment_count < depth) {
    return 0;
  }
  for (i = 0u; i < depth; ++i) {
    if (!lonejson__candidate_transform_projection_segments_equal(
            &rule->segments[i], &seed->segments[i])) {
      return 0;
    }
  }
  return 1;
}

static int lonejson__candidate_transform_projection_segment_seen_in_group(
    lonejson__candidate_transform_state *state,
    const lonejson_candidate_transform_projection_path *seed, size_t depth,
    size_t before,
    const lonejson_candidate_transform_projection_segment *segment) {
  const lonejson_candidate_transform_projection_path *rule;
  size_t i;

  for (i = 0u; i < before; ++i) {
    rule = &state->options->projection_paths[i];
    if (rule->segment_count > depth &&
        lonejson__candidate_transform_projection_rule_prefix_equal(rule, seed,
                                                                   depth) &&
        lonejson__candidate_transform_projection_segments_equal(
            &rule->segments[depth], segment)) {
      return 1;
    }
  }
  return 0;
}

static lonejson_status lonejson__candidate_transform_synthesize_group(
    lonejson__candidate_transform_state *state,
    const lonejson_candidate_transform_projection_path *seed, size_t depth);

static lonejson_status lonejson__candidate_transform_synthesize_object_group(
    lonejson__candidate_transform_state *state,
    const lonejson_candidate_transform_projection_path *seed, size_t depth) {
  const lonejson_candidate_transform_projection_path *rule;
  const lonejson_candidate_transform_projection_segment *segment;
  lonejson_status status;
  size_t i;

  status = lonejson__candidate_transform_record_status(
      state, lonejson_writer_begin_object(&state->writer, state->error));
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  for (i = 0u; i < state->options->projection_path_count; ++i) {
    rule = &state->options->projection_paths[i];
    if (rule->segment_count <= depth ||
        !lonejson__candidate_transform_projection_rule_prefix_equal(rule, seed,
                                                                    depth)) {
      continue;
    }
    segment = &rule->segments[depth];
    if (segment->kind != LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER ||
        lonejson__candidate_transform_projection_segment_seen_in_group(
            state, seed, depth, i, segment)) {
      continue;
    }
    status = lonejson__candidate_transform_record_status(
        state, lonejson_writer_key(
                   &state->writer,
                   lonejson__candidate_transform_projection_key(segment),
                   segment->key_len, state->error));
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    status =
        lonejson__candidate_transform_synthesize_group(state, rule, depth + 1u);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  return lonejson__candidate_transform_record_status(
      state, lonejson_writer_end_object(&state->writer, state->error));
}

static lonejson_status lonejson__candidate_transform_synthesize_array_group(
    lonejson__candidate_transform_state *state,
    const lonejson_candidate_transform_projection_path *seed, size_t depth) {
  const lonejson_candidate_transform_projection_path *rule;
  const lonejson_candidate_transform_projection_path *next_rule;
  lonejson_uint64 output_index;
  lonejson_uint64 next_index;
  lonejson_status status;
  size_t i;
  int found;

  status = lonejson__candidate_transform_record_status(
      state, lonejson_writer_begin_array(&state->writer, state->error));
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  output_index = 0u;
  for (;;) {
    found = 0;
    next_rule = NULL;
    next_index = LONEJSON_UINT64_MAX;
    for (i = 0u; i < state->options->projection_path_count; ++i) {
      rule = &state->options->projection_paths[i];
      if (rule->segment_count <= depth ||
          !lonejson__candidate_transform_projection_rule_prefix_equal(
              rule, seed, depth) ||
          rule->segments[depth].kind !=
              LONEJSON_CANDIDATE_TRANSFORM_PROJECT_ARRAY_INDEX ||
          rule->segments[depth].index < output_index) {
        continue;
      }
      if (!found || rule->segments[depth].index < next_index) {
        found = 1;
        next_rule = rule;
        next_index = rule->segments[depth].index;
      }
    }
    if (!found) {
      break;
    }
    while (output_index < next_index) {
      status = lonejson__candidate_transform_record_status(
          state, lonejson_writer_null(&state->writer, state->error));
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      output_index++;
    }
    status = lonejson__candidate_transform_synthesize_group(
        state, next_rule, depth + 1u);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    output_index++;
  }
  return lonejson__candidate_transform_record_status(
      state, lonejson_writer_end_array(&state->writer, state->error));
}

static lonejson_status lonejson__candidate_transform_synthesize_group(
    lonejson__candidate_transform_state *state,
    const lonejson_candidate_transform_projection_path *seed, size_t depth) {
  size_t i;
  int has_exact;
  int has_object;
  int has_array;

  has_exact = 0;
  has_object = 0;
  has_array = 0;
  for (i = 0u; i < state->options->projection_path_count; ++i) {
    const lonejson_candidate_transform_projection_path *rule =
        &state->options->projection_paths[i];
    if (!lonejson__candidate_transform_projection_rule_prefix_equal(rule, seed,
                                                                    depth)) {
      continue;
    }
    if (rule->segment_count == depth) {
      has_exact = 1;
    } else if (rule->segments[depth].kind ==
               LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER) {
      has_object = 1;
    } else if (rule->segments[depth].kind ==
               LONEJSON_CANDIDATE_TRANSFORM_PROJECT_ARRAY_INDEX) {
      has_array = 1;
    }
  }
  if (has_exact || (!has_object && !has_array)) {
    return lonejson__candidate_transform_record_status(
        state, lonejson_writer_null(&state->writer, state->error));
  }
  if (has_object && has_array) {
    return lonejson__set_error(state->error, LONEJSON_STATUS_UNSUPPORTED, 0u,
                               0u, 0u,
                               "candidate projection mixes object and array "
                               "children in a missing subtree");
  }
  if (has_object) {
    return lonejson__candidate_transform_synthesize_object_group(state, seed,
                                                                 depth);
  }
  return lonejson__candidate_transform_synthesize_array_group(state, seed,
                                                              depth);
}

static lonejson_status lonejson__candidate_transform_synthesize_member_tail(
    lonejson__candidate_transform_state *state,
    const lonejson_candidate_transform_projection_path *rule, size_t index) {
  const lonejson_candidate_transform_projection_segment *segment;
  lonejson_status status;

  if (rule == NULL || index >= rule->segment_count) {
    return LONEJSON_STATUS_OK;
  }
  segment = &rule->segments[index];
  if (segment->kind != LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER) {
    return lonejson__set_error(state->error, LONEJSON_STATUS_UNSUPPORTED, 0u,
                               0u, 0u,
                               "candidate projection missing object member "
                               "requires an object-member segment");
  }
  status = lonejson__candidate_transform_record_status(
      state, lonejson_writer_key(
                 &state->writer,
                 lonejson__candidate_transform_projection_key(segment),
                 segment->key_len, state->error));
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__candidate_transform_synthesize_group(state, rule,
                                                        index + 1u);
}

static lonejson_status lonejson__candidate_transform_synthesize_array_tail(
    lonejson__candidate_transform_state *state,
    lonejson__candidate_transform_frame *frame,
    const lonejson_candidate_transform_projection_path *rule, size_t index) {
  const lonejson_candidate_transform_projection_segment *segment;
  lonejson_status status;

  if (rule == NULL || frame == NULL || index >= rule->segment_count) {
    return LONEJSON_STATUS_OK;
  }
  segment = &rule->segments[index];
  if (segment->kind != LONEJSON_CANDIDATE_TRANSFORM_PROJECT_ARRAY_INDEX) {
    return lonejson__set_error(state->error, LONEJSON_STATUS_UNSUPPORTED, 0u,
                               0u, 0u,
                               "candidate projection missing array element "
                               "requires an array-index segment");
  }
  while (frame->output_index < segment->index) {
    status = lonejson__candidate_transform_record_status(
        state, lonejson_writer_null(&state->writer, state->error));
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    frame->output_index++;
  }
  if (frame->output_index == segment->index) {
    frame->output_index++;
    return lonejson__candidate_transform_synthesize_group(state, rule,
                                                          index + 1u);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__candidate_transform_emit_missing_projection(
    lonejson__candidate_transform_state *state, const lonejson_value_path *path,
    lonejson_value_type type) {
  lonejson__candidate_transform_frame *frame;
  const lonejson_candidate_transform_projection_path *rule;
  const lonejson_candidate_transform_projection_segment *segment;
  size_t i;
  size_t depth;
  lonejson_status status;

  if (!state->projection_enabled || path == NULL) {
    return LONEJSON_STATUS_OK;
  }
  frame = lonejson__candidate_transform_top(state);
  if (frame == NULL) {
    return LONEJSON_STATUS_OK;
  }
  depth = path->segment_count;
  for (i = 0u; i < state->options->projection_path_count; ++i) {
    rule = &state->options->projection_paths[i];
    if (rule->segment_count <= depth ||
        !lonejson__candidate_transform_projection_path_has_prefix(state, rule,
                                                                  path)) {
      continue;
    }
    segment = &rule->segments[depth];
    if (type == LONEJSON_VALUE_OBJECT) {
      if (segment->kind != LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER ||
          lonejson__candidate_transform_seen_key_has(
              frame, lonejson__candidate_transform_projection_key(segment),
              segment->key_len)) {
        continue;
      }
      status = lonejson__candidate_transform_synthesize_member_tail(state, rule,
                                                                    depth);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      status = lonejson__candidate_transform_seen_key_add(
          state, frame, lonejson__candidate_transform_projection_key(segment),
          segment->key_len);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    } else if (type == LONEJSON_VALUE_ARRAY) {
      if (segment->kind != LONEJSON_CANDIDATE_TRANSFORM_PROJECT_ARRAY_INDEX) {
        continue;
      }
      status =
          lonejson__candidate_transform_synthesize_array_tail(state, frame, rule,
                                                              depth);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status
lonejson__candidate_transform_synthesize_wrong_shape_scalar(
    lonejson__candidate_transform_state *state, const lonejson_value_path *path) {
  const lonejson_candidate_transform_projection_path *rule;
  lonejson_status status;
  size_t i;
  size_t depth;

  if (!state->projection_enabled || path == NULL ||
      !lonejson__candidate_transform_projection_has_descendant(state, path)) {
    return LONEJSON_STATUS_OK;
  }
  status = lonejson__candidate_transform_prepare_emit(state, path);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  depth = path->segment_count;
  for (i = 0u; i < state->options->projection_path_count; ++i) {
    rule = &state->options->projection_paths[i];
    if (rule->segment_count > depth &&
        lonejson__candidate_transform_projection_path_has_prefix(state, rule,
                                                                 path)) {
      return lonejson__candidate_transform_synthesize_group(state, rule, depth);
    }
  }
  return LONEJSON_STATUS_OK;
}

static int lonejson__candidate_transform_projection_container_mismatches(
    lonejson__candidate_transform_state *state, const lonejson_value_path *path,
    lonejson__candidate_transform_frame_kind frame_kind) {
  const lonejson_candidate_transform_projection_path *rule;
  const lonejson_candidate_transform_projection_segment *segment;
  size_t i;
  size_t depth;

  if (!state->projection_enabled || path == NULL ||
      !lonejson__candidate_transform_projection_has_descendant(state, path)) {
    return 0;
  }
  depth = path->segment_count;
  for (i = 0u; i < state->options->projection_path_count; ++i) {
    rule = &state->options->projection_paths[i];
    if (rule->segment_count <= depth ||
        !lonejson__candidate_transform_projection_path_has_prefix(state, rule,
                                                                  path)) {
      continue;
    }
    segment = &rule->segments[depth];
    if ((segment->kind == LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER &&
         frame_kind != LONEJSON__CANDIDATE_TRANSFORM_OBJECT) ||
        (segment->kind == LONEJSON_CANDIDATE_TRANSFORM_PROJECT_ARRAY_INDEX &&
         frame_kind != LONEJSON__CANDIDATE_TRANSFORM_ARRAY)) {
      return 1;
    }
  }
  return 0;
}

static lonejson_status
lonejson__candidate_transform_synthesize_wrong_shape_container(
    lonejson__candidate_transform_state *state, const lonejson_value_path *path) {
  const lonejson_candidate_transform_projection_path *rule;
  lonejson_status status;
  size_t i;
  size_t depth;

  status = lonejson__candidate_transform_prepare_emit(state, path);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  depth = path != NULL ? path->segment_count : 0u;
  for (i = 0u; i < state->options->projection_path_count; ++i) {
    rule = &state->options->projection_paths[i];
    if (rule->segment_count > depth &&
        lonejson__candidate_transform_projection_path_has_prefix(state, rule,
                                                                 path)) {
      return lonejson__candidate_transform_synthesize_group(state, rule, depth);
    }
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__candidate_transform_replace(
    lonejson__candidate_transform_state *state, const lonejson_value_path *path,
    lonejson_value_type type,
    const lonejson_candidate_transform_old_value *old_value) {
  lonejson_candidate_transform_event event;
  lonejson_status status;

  if (state->options->replace == NULL) {
    return lonejson__set_error(state->error, LONEJSON_STATUS_INVALID_ARGUMENT,
                               0u, 0u, 0u,
                               "candidate transform replacement callback is "
                               "required");
  }
  memset(&event, 0, sizeof(event));
  event.candidate = &state->candidate;
  event.transform_candidate = &state->transform_candidate;
  event.path = path;
  event.value_type = type;
  event.old_value = old_value;
  status = lonejson__candidate_transform_prepare_emit(state, path);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__candidate_transform_record_status(
      state, state->options->replace(state->options->transform_user, &event,
                                     &state->writer, state->error));
}

static lonejson_status lonejson__candidate_transform_insert(
    lonejson__candidate_transform_state *state, const lonejson_value_path *path,
    lonejson_candidate_transform_insert_phase phase,
    const char *object_key, size_t object_key_len) {
  lonejson_candidate_transform_event event;
  lonejson_status status;

  if (state->options->insert == NULL || state->skipping || state->stopped) {
    return LONEJSON_STATUS_OK;
  }
  memset(&event, 0, sizeof(event));
  event.candidate = &state->candidate;
  event.transform_candidate = &state->transform_candidate;
  event.path = path;
  event.value_type = LONEJSON_VALUE_OBJECT;
  event.insert_phase = phase;
  event.object_key = object_key;
  event.object_key_len = object_key_len;
  status = state->options->insert(state->options->transform_user, &event,
                                  &state->writer, state->error);
  return lonejson__candidate_transform_record_status(state, status);
}

static lonejson_status lonejson__candidate_transform_insert_for_top(
    lonejson__candidate_transform_state *state, const lonejson_value_path *path,
    lonejson_candidate_transform_insert_phase phase) {
  lonejson__candidate_transform_frame *frame;

  frame = lonejson__candidate_transform_top(state);
  if (frame == NULL || frame->kind != LONEJSON__CANDIDATE_TRANSFORM_OBJECT) {
    return LONEJSON_STATUS_OK;
  }
  return lonejson__candidate_transform_insert(
      state, path, phase, frame->key.data != NULL ? frame->key.data : "",
      frame->key.len);
}

static lonejson_candidate_transform_action
lonejson__candidate_transform_decide(lonejson__candidate_transform_state *state,
                                     const lonejson_value_path *path,
                                     lonejson_value_type type,
                                     const lonejson_candidate_transform_old_value
                                         *old_value) {
  lonejson_candidate_transform_event event;
  lonejson_candidate_transform_action action;

  if (state->options->transform == NULL) {
    return LONEJSON_CANDIDATE_TRANSFORM_KEEP;
  }
  memset(&event, 0, sizeof(event));
  event.candidate = &state->candidate;
  event.transform_candidate = &state->transform_candidate;
  event.path = path;
  event.value_type = type;
  event.old_value = old_value;
  lonejson__clear_error(state->error);
  action = state->options->transform(state->options->transform_user, &event,
                                     state->error);
  if (action == LONEJSON_CANDIDATE_TRANSFORM_STOP) {
    state->stopped = 1;
  }
  return action;
}

static lonejson_status lonejson__candidate_transform_action_status(
    lonejson__candidate_transform_state *state,
    lonejson_candidate_transform_action action, const lonejson_value_path *path,
    lonejson_value_type type, int container_value,
    const lonejson_candidate_transform_old_value *old_value) {
  lonejson_status status;

  if (action == LONEJSON_CANDIDATE_TRANSFORM_KEEP) {
    status = lonejson__candidate_transform_prepare_emit(state, path);
    if (status == LONEJSON_STATUS_OK) {
      state->current_emit = container_value ? 0 : 1;
    }
    return status;
  }
  if (action == LONEJSON_CANDIDATE_TRANSFORM_DROP) {
    state->current_emit = 0;
    return lonejson__candidate_transform_mark_projection_handled(state, path);
  }
  if (action == LONEJSON_CANDIDATE_TRANSFORM_REPLACE) {
    status =
        lonejson__candidate_transform_replace(state, path, type, old_value);
    if (status == LONEJSON_STATUS_OK) {
      state->current_emit = 0;
      state->current_scalar_replace = container_value ? 0 : 1;
    }
    return status;
  }
  if (action == LONEJSON_CANDIDATE_TRANSFORM_STOP) {
    state->current_emit = 0;
    return LONEJSON_STATUS_OK;
  }
  if (state->error != NULL && (state->error->code == LONEJSON_STATUS_OK ||
                               state->error->code == (lonejson_status)0)) {
    return lonejson__set_error(state->error, LONEJSON_STATUS_CALLBACK_FAILED,
                               0u, 0u, 0u,
                               "candidate transform callback failed");
  }
  return LONEJSON_STATUS_CALLBACK_FAILED;
}

static lonejson_status lonejson__candidate_transform_begin_container(
    lonejson__candidate_transform_state *state, const lonejson_value_path *path,
    lonejson_value_type type,
    lonejson__candidate_transform_frame_kind frame_kind) {
  lonejson_candidate_transform_action action;
  lonejson_status status;
  int projection_exact;
  int projection_descendant;
  int projection_passthrough;

  if (state->skipping) {
    state->skip_depth++;
    return LONEJSON_STATUS_OK;
  }
  if (state->stopped) {
    state->skipping = 1;
    state->skip_depth = 1u;
    return LONEJSON_STATUS_OK;
  }
  projection_passthrough =
      lonejson__candidate_transform_projection_passthrough_active(state);
  projection_exact = projection_passthrough
                         ? 1
                         : lonejson__candidate_transform_projection_matches_exact(
                               state, path);
  projection_descendant =
      projection_passthrough
          ? 1
          : lonejson__candidate_transform_projection_has_descendant(state, path);
  if (state->projection_enabled && !projection_passthrough &&
      !projection_exact &&
      !projection_descendant) {
    state->skipping = 1;
    state->skip_depth = 1u;
    return LONEJSON_STATUS_OK;
  }
  if (state->projection_enabled && !projection_passthrough &&
      !projection_exact && projection_descendant &&
      lonejson__candidate_transform_projection_container_mismatches(
          state, path, frame_kind)) {
    status = lonejson__candidate_transform_synthesize_wrong_shape_container(
        state, path);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    {
      lonejson__candidate_transform_frame *frame =
          lonejson__candidate_transform_top(state);
      state->skip_after_member =
          frame != NULL && frame->kind == LONEJSON__CANDIDATE_TRANSFORM_OBJECT;
    }
    state->skipping = 1;
    state->skip_depth = 1u;
    return LONEJSON_STATUS_OK;
  }
  if (state->projection_enabled && !projection_passthrough && path != NULL &&
      path->segment_count == 0u &&
      !projection_exact) {
    if ((frame_kind == LONEJSON__CANDIDATE_TRANSFORM_OBJECT &&
         state->projection_root_kind != LONEJSON__CANDIDATE_TRANSFORM_OBJECT) ||
        (frame_kind == LONEJSON__CANDIDATE_TRANSFORM_ARRAY &&
         state->projection_root_kind != LONEJSON__CANDIDATE_TRANSFORM_ARRAY)) {
      return lonejson__set_error(
          state->error, LONEJSON_STATUS_UNSUPPORTED, 0u, 0u, 0u,
          "candidate projection root kind does not match source value");
    }
  }
  action = (!state->projection_enabled || projection_passthrough ||
            projection_exact || projection_descendant)
               ? lonejson__candidate_transform_decide(state, path, type, NULL)
               : LONEJSON_CANDIDATE_TRANSFORM_KEEP;
  status = lonejson__candidate_transform_action_status(
      state, action, path, type, 1, NULL);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (action == LONEJSON_CANDIDATE_TRANSFORM_KEEP) {
    if (type == LONEJSON_VALUE_OBJECT) {
      status = lonejson_writer_begin_object(&state->writer, state->error);
    } else {
      status = lonejson_writer_begin_array(&state->writer, state->error);
    }
    lonejson__candidate_transform_record_status(state, status);
    if (status != LONEJSON_STATUS_OK) {
      state->skipping = 1;
      state->skip_depth = 1u;
      return status;
    }
    status = lonejson__candidate_transform_push(state, frame_kind);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    if (state->projection_enabled && !projection_passthrough &&
        projection_exact) {
      lonejson__candidate_transform_top(state)->projection_passthrough = 1;
    }
    if (type == LONEJSON_VALUE_OBJECT) {
      return lonejson__candidate_transform_insert(
          state, path, LONEJSON_CANDIDATE_TRANSFORM_INSERT_OBJECT_BEGIN, NULL,
          0u);
    }
    return LONEJSON_STATUS_OK;
  }
  if (action == LONEJSON_CANDIDATE_TRANSFORM_STOP) {
    state->skipping = 1;
    state->skip_depth = 1u;
    return LONEJSON_STATUS_OK;
  }
  {
    lonejson__candidate_transform_frame *frame =
        lonejson__candidate_transform_top(state);
    state->skip_after_member =
        frame != NULL && frame->kind == LONEJSON__CANDIDATE_TRANSFORM_OBJECT;
  }
  state->skipping = 1;
  state->skip_depth = 1u;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__candidate_transform_object_begin(
    void *user, const lonejson_value_path *path, lonejson_error *error) {
  lonejson__candidate_transform_state *state =
      (lonejson__candidate_transform_state *)user;
  lonejson_status status;
  (void)error;
  status = lonejson__candidate_transform_forward_event(
      state,
      state->options->observer != NULL ? state->options->observer->object_begin
                                       : NULL,
      path);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__candidate_transform_begin_container(
      state, path, LONEJSON_VALUE_OBJECT, LONEJSON__CANDIDATE_TRANSFORM_OBJECT);
}

static lonejson_status lonejson__candidate_transform_array_begin(
    void *user, const lonejson_value_path *path, lonejson_error *error) {
  lonejson__candidate_transform_state *state =
      (lonejson__candidate_transform_state *)user;
  lonejson_status status;
  (void)error;
  status = lonejson__candidate_transform_forward_event(
      state,
      state->options->observer != NULL ? state->options->observer->array_begin
                                       : NULL,
      path);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__candidate_transform_begin_container(
      state, path, LONEJSON_VALUE_ARRAY, LONEJSON__CANDIDATE_TRANSFORM_ARRAY);
}

static lonejson_status lonejson__candidate_transform_end_container(
    lonejson__candidate_transform_state *state, const lonejson_value_path *path,
    lonejson_value_type type) {
  lonejson_status status;

  if (state->skipping) {
    if (state->skip_depth != 0u) {
      state->skip_depth--;
    }
    if (state->skip_depth == 0u) {
      state->skipping = 0;
      if (state->skip_after_member) {
        state->skip_after_member = 0;
        return lonejson__candidate_transform_insert_for_top(
            state, path, LONEJSON_CANDIDATE_TRANSFORM_INSERT_AFTER_MEMBER);
      }
    }
    return LONEJSON_STATUS_OK;
  }
  status = lonejson__candidate_transform_emit_missing_projection(state, path,
                                                                 type);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (type == LONEJSON_VALUE_OBJECT) {
    status = lonejson__candidate_transform_insert(
        state, path, LONEJSON_CANDIDATE_TRANSFORM_INSERT_OBJECT_END, NULL, 0u);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    lonejson__candidate_transform_pop(state);
    status = lonejson__candidate_transform_record_status(
        state, lonejson_writer_end_object(&state->writer, state->error));
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    return lonejson__candidate_transform_insert_for_top(
        state, path, LONEJSON_CANDIDATE_TRANSFORM_INSERT_AFTER_MEMBER);
  }
  lonejson__candidate_transform_pop(state);
  status = lonejson__candidate_transform_record_status(
      state, lonejson_writer_end_array(&state->writer, state->error));
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__candidate_transform_insert_for_top(
      state, path, LONEJSON_CANDIDATE_TRANSFORM_INSERT_AFTER_MEMBER);
}

static lonejson_status lonejson__candidate_transform_object_end(
    void *user, const lonejson_value_path *path, lonejson_error *error) {
  lonejson__candidate_transform_state *state =
      (lonejson__candidate_transform_state *)user;
  lonejson_status status;
  (void)error;
  status = lonejson__candidate_transform_forward_event(
      state,
      state->options->observer != NULL ? state->options->observer->object_end
                                       : NULL,
      path);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__candidate_transform_end_container(state, path,
                                                     LONEJSON_VALUE_OBJECT);
}

static lonejson_status lonejson__candidate_transform_array_end(
    void *user, const lonejson_value_path *path, lonejson_error *error) {
  lonejson__candidate_transform_state *state =
      (lonejson__candidate_transform_state *)user;
  lonejson_status status;
  (void)error;
  status = lonejson__candidate_transform_forward_event(
      state,
      state->options->observer != NULL ? state->options->observer->array_end
                                       : NULL,
      path);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__candidate_transform_end_container(state, path,
                                                     LONEJSON_VALUE_ARRAY);
}

static lonejson_status lonejson__candidate_transform_key_begin(
    void *user, const lonejson_value_path *path, lonejson_error *error) {
  lonejson__candidate_transform_state *state =
      (lonejson__candidate_transform_state *)user;
  lonejson__candidate_transform_frame *frame;
  (void)error;
  if (!state->skipping) {
    frame = lonejson__candidate_transform_top(state);
    if (frame != NULL) {
      lonejson__byte_reset(&frame->key);
    }
  }
  return lonejson__candidate_transform_forward_event(
      state,
      state->options->observer != NULL
          ? state->options->observer->object_key_begin
          : NULL,
      path);
}

static lonejson_status lonejson__candidate_transform_key_chunk(
    void *user, const lonejson_value_path *path, const char *data, size_t len,
    lonejson_error *error) {
  lonejson__candidate_transform_state *state =
      (lonejson__candidate_transform_state *)user;
  lonejson__candidate_transform_frame *frame;
  lonejson_status status;
  (void)error;
  if (!state->skipping) {
    frame = lonejson__candidate_transform_top(state);
    if (frame != NULL) {
      status = lonejson__byte_append(&frame->key, data, len, SIZE_MAX - 1u,
                                     state->allocator, state->error);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
    }
  }
  return lonejson__candidate_transform_forward_chunk(
      state,
      state->options->observer != NULL
          ? state->options->observer->object_key_chunk
          : NULL,
      path, data, len);
}

static lonejson_status lonejson__candidate_transform_key_end(
    void *user, const lonejson_value_path *path, lonejson_error *error) {
  lonejson__candidate_transform_state *state =
      (lonejson__candidate_transform_state *)user;
  lonejson_status status;
  (void)error;
  status = lonejson__candidate_transform_forward_event(
      state,
      state->options->observer != NULL
          ? state->options->observer->object_key_end
          : NULL,
      path);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (!state->projection_enabled ||
      lonejson__candidate_transform_projection_passthrough_active(state)) {
    return lonejson__candidate_transform_insert_for_top(
        state, path, LONEJSON_CANDIDATE_TRANSFORM_INSERT_BEFORE_MEMBER);
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__candidate_transform_scalar_begin(
    lonejson__candidate_transform_state *state, const lonejson_value_path *path,
    lonejson_value_type type) {
  lonejson_candidate_transform_action action;
  lonejson_status status;

  state->current_emit = 0;
  state->current_scalar_replace = 0;
  state->current_scalar_materialized = 0;
  state->current_projection_descendant = 0;
  lonejson__byte_reset(&state->scalar);
  memset(&state->old_value, 0, sizeof(state->old_value));
  state->old_value.value_type = type;
  if (state->skipping || state->stopped) {
    return LONEJSON_STATUS_OK;
  }
  if (state->projection_enabled && path != NULL && path->segment_count == 0u &&
      !lonejson__candidate_transform_projection_matches_exact(state, path) &&
      lonejson__candidate_transform_projection_has_descendant(state, path)) {
    return lonejson__set_error(
        state->error, LONEJSON_STATUS_UNSUPPORTED, 0u, 0u, 0u,
        "candidate projection root kind does not match source value");
  }
  if (state->projection_enabled &&
      !lonejson__candidate_transform_projection_passthrough_active(state) &&
      !lonejson__candidate_transform_projection_matches_exact(state, path)) {
    if (lonejson__candidate_transform_projection_has_descendant(state, path)) {
      state->current_projection_descendant = 1;
      if ((type == LONEJSON_VALUE_STRING || type == LONEJSON_VALUE_NUMBER) &&
          state->options->old_scalar_mode !=
              LONEJSON_CANDIDATE_TRANSFORM_OLD_SCALAR_COMPLETE) {
        return LONEJSON_STATUS_OK;
      }
      state->current_scalar_materialized = 1;
      return LONEJSON_STATUS_OK;
    }
    return LONEJSON_STATUS_OK;
  }
  if ((type == LONEJSON_VALUE_STRING || type == LONEJSON_VALUE_NUMBER) &&
      state->options->old_scalar_mode !=
          LONEJSON_CANDIDATE_TRANSFORM_OLD_SCALAR_COMPLETE) {
    action = lonejson__candidate_transform_decide(state, path, type, NULL);
    status = lonejson__candidate_transform_action_status(
        state, action, path, type, 0, NULL);
    if (status != LONEJSON_STATUS_OK || !state->current_emit) {
      return status;
    }
    if (type == LONEJSON_VALUE_STRING) {
      return lonejson__candidate_transform_record_status(
          state, lonejson_writer_string_begin(&state->writer, state->error));
    }
    return lonejson__candidate_transform_record_status(
        state, lonejson_writer_number_begin(&state->writer, state->error));
  }
  state->current_scalar_materialized = 1;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__candidate_transform_emit_old_scalar(
    lonejson__candidate_transform_state *state, lonejson_value_type type) {
  if (type == LONEJSON_VALUE_STRING) {
    return lonejson__candidate_transform_record_status(
        state, lonejson_writer_string(&state->writer, state->scalar.data,
                                      state->scalar.len, state->error));
  }
  if (type == LONEJSON_VALUE_NUMBER) {
    return lonejson__candidate_transform_record_status(
        state, lonejson_writer_number_text(&state->writer, state->scalar.data,
                                           state->scalar.len, state->error));
  }
  if (type == LONEJSON_VALUE_BOOL) {
    return lonejson__candidate_transform_record_status(
        state, lonejson_writer_bool(&state->writer,
                                    state->old_value.boolean_value,
                                    state->error));
  }
  return lonejson__candidate_transform_record_status(
      state, lonejson_writer_null(&state->writer, state->error));
}

static lonejson_status lonejson__candidate_transform_scalar_end(
    lonejson__candidate_transform_state *state, const lonejson_value_path *path,
    lonejson_value_type type) {
  lonejson_candidate_transform_action action;
  lonejson_status status;

  if (state->skipping || state->stopped) {
    lonejson__byte_reset(&state->scalar);
    return LONEJSON_STATUS_OK;
  }
  if (state->projection_enabled &&
      !lonejson__candidate_transform_projection_passthrough_active(state) &&
      !lonejson__candidate_transform_projection_matches_exact(state, path)) {
    if (state->current_projection_descendant) {
      if (type == LONEJSON_VALUE_STRING || type == LONEJSON_VALUE_NUMBER) {
        state->old_value.data =
            state->scalar.data != NULL ? state->scalar.data : "";
        state->old_value.len = state->scalar.len;
      }
      action = lonejson__candidate_transform_decide(
          state, path, type,
          state->current_scalar_materialized ? &state->old_value : NULL);
      if (action == LONEJSON_CANDIDATE_TRANSFORM_KEEP) {
        status =
            lonejson__candidate_transform_synthesize_wrong_shape_scalar(state,
                                                                        path);
      } else {
        status = lonejson__candidate_transform_action_status(
            state, action, path, type, 0,
            state->current_scalar_materialized ? &state->old_value : NULL);
      }
      if (status == LONEJSON_STATUS_OK && !state->stopped) {
        status = lonejson__candidate_transform_insert_for_top(
            state, path, LONEJSON_CANDIDATE_TRANSFORM_INSERT_AFTER_MEMBER);
      }
      state->current_emit = 0;
      state->current_scalar_replace = 0;
      state->current_scalar_materialized = 0;
      state->current_projection_descendant = 0;
      lonejson__byte_reset(&state->scalar);
      return status;
    }
    state->current_emit = 0;
    state->current_scalar_replace = 0;
    state->current_scalar_materialized = 0;
    state->current_projection_descendant = 0;
    lonejson__byte_reset(&state->scalar);
    return LONEJSON_STATUS_OK;
  }
  if (!state->current_scalar_materialized &&
      (type == LONEJSON_VALUE_STRING || type == LONEJSON_VALUE_NUMBER)) {
    if (state->current_emit) {
      status = type == LONEJSON_VALUE_STRING
                   ? lonejson_writer_string_end(&state->writer, state->error)
                   : lonejson_writer_number_end(&state->writer, state->error);
      status = lonejson__candidate_transform_record_status(state, status);
    } else {
      status = LONEJSON_STATUS_OK;
    }
    if (status == LONEJSON_STATUS_OK && !state->stopped) {
      status = lonejson__candidate_transform_insert_for_top(
          state, path, LONEJSON_CANDIDATE_TRANSFORM_INSERT_AFTER_MEMBER);
    }
    state->current_emit = 0;
    state->current_scalar_replace = 0;
    lonejson__byte_reset(&state->scalar);
    return status;
  }
  if (type == LONEJSON_VALUE_STRING || type == LONEJSON_VALUE_NUMBER) {
    state->old_value.data =
        state->scalar.data != NULL ? state->scalar.data : "";
    state->old_value.len = state->scalar.len;
  }
  action =
      lonejson__candidate_transform_decide(state, path, type, &state->old_value);
  status = lonejson__candidate_transform_action_status(
      state, action, path, type, 0, &state->old_value);
  if (status == LONEJSON_STATUS_OK && state->current_emit) {
    status = lonejson__candidate_transform_emit_old_scalar(state, type);
  }
  if (status == LONEJSON_STATUS_OK && !state->stopped) {
    status = lonejson__candidate_transform_insert_for_top(
        state, path, LONEJSON_CANDIDATE_TRANSFORM_INSERT_AFTER_MEMBER);
  }
  state->current_emit = 0;
  state->current_scalar_replace = 0;
  state->current_scalar_materialized = 0;
  state->current_projection_descendant = 0;
  lonejson__byte_reset(&state->scalar);
  return status;
}

static lonejson_status lonejson__candidate_transform_string_begin(
    void *user, const lonejson_value_path *path, lonejson_error *error) {
  lonejson__candidate_transform_state *state =
      (lonejson__candidate_transform_state *)user;
  lonejson_status status;
  (void)error;
  status = lonejson__candidate_transform_forward_event(
      state,
      state->options->observer != NULL ? state->options->observer->string_begin
                                       : NULL,
      path);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__candidate_transform_scalar_begin(state, path,
                                                      LONEJSON_VALUE_STRING);
  return status;
}

static lonejson_status lonejson__candidate_transform_string_chunk(
    void *user, const lonejson_value_path *path, const char *data, size_t len,
    lonejson_error *error) {
  lonejson__candidate_transform_state *state =
      (lonejson__candidate_transform_state *)user;
  lonejson_status status;
  (void)error;
  status = lonejson__candidate_transform_forward_chunk(
      state,
      state->options->observer != NULL ? state->options->observer->string_chunk
                                       : NULL,
      path, data, len);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (!state->current_scalar_materialized) {
    if (state->current_emit) {
      return lonejson__candidate_transform_record_status(
          state,
          lonejson_writer_string_chunk(&state->writer, data, len,
                                       state->error));
    }
    return LONEJSON_STATUS_OK;
  }
  if (!state->current_emit) {
    if (status != LONEJSON_STATUS_OK || state->skipping || state->stopped) {
      return status;
    }
  }
  return lonejson__byte_append(&state->scalar, data, len, SIZE_MAX - 1u,
                               state->allocator, state->error);
}

static lonejson_status lonejson__candidate_transform_string_end(
    void *user, const lonejson_value_path *path, lonejson_error *error) {
  lonejson__candidate_transform_state *state =
      (lonejson__candidate_transform_state *)user;
  lonejson_status status;
  (void)error;
  status = lonejson__candidate_transform_forward_event(
      state,
      state->options->observer != NULL ? state->options->observer->string_end
                                       : NULL,
      path);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__candidate_transform_scalar_end(state, path,
                                                  LONEJSON_VALUE_STRING);
}

static lonejson_status lonejson__candidate_transform_number_begin(
    void *user, const lonejson_value_path *path, lonejson_error *error) {
  lonejson__candidate_transform_state *state =
      (lonejson__candidate_transform_state *)user;
  lonejson_status status;
  (void)error;
  status = lonejson__candidate_transform_forward_event(
      state,
      state->options->observer != NULL ? state->options->observer->number_begin
                                       : NULL,
      path);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__candidate_transform_scalar_begin(state, path,
                                                      LONEJSON_VALUE_NUMBER);
  return status;
}

static lonejson_status lonejson__candidate_transform_number_chunk(
    void *user, const lonejson_value_path *path, const char *data, size_t len,
    lonejson_error *error) {
  lonejson__candidate_transform_state *state =
      (lonejson__candidate_transform_state *)user;
  lonejson_status status;
  (void)error;
  status = lonejson__candidate_transform_forward_chunk(
      state,
      state->options->observer != NULL ? state->options->observer->number_chunk
                                       : NULL,
      path, data, len);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (!state->current_scalar_materialized) {
    if (state->current_emit) {
      return lonejson__candidate_transform_record_status(
          state,
          lonejson_writer_number_chunk(&state->writer, data, len,
                                       state->error));
    }
    return LONEJSON_STATUS_OK;
  }
  if (!state->current_emit) {
    if (status != LONEJSON_STATUS_OK || state->skipping || state->stopped) {
      return status;
    }
  }
  return lonejson__byte_append(&state->scalar, data, len, SIZE_MAX - 1u,
                               state->allocator, state->error);
}

static lonejson_status lonejson__candidate_transform_number_end(
    void *user, const lonejson_value_path *path, lonejson_error *error) {
  lonejson__candidate_transform_state *state =
      (lonejson__candidate_transform_state *)user;
  lonejson_status status;
  (void)error;
  status = lonejson__candidate_transform_forward_event(
      state,
      state->options->observer != NULL ? state->options->observer->number_end
                                       : NULL,
      path);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__candidate_transform_scalar_end(state, path,
                                                  LONEJSON_VALUE_NUMBER);
}

static lonejson_status
lonejson__candidate_transform_bool(void *user, const lonejson_value_path *path,
                                   int value, lonejson_error *error) {
  lonejson__candidate_transform_state *state =
      (lonejson__candidate_transform_state *)user;
  lonejson_status status;
  (void)error;
  status = lonejson__candidate_transform_forward_bool(state, path, value);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__candidate_transform_scalar_begin(state, path,
                                                      LONEJSON_VALUE_BOOL);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  state->old_value.boolean_value = value != 0;
  return lonejson__candidate_transform_scalar_end(state, path,
                                                  LONEJSON_VALUE_BOOL);
}

static lonejson_status
lonejson__candidate_transform_null(void *user, const lonejson_value_path *path,
                                   lonejson_error *error) {
  lonejson__candidate_transform_state *state =
      (lonejson__candidate_transform_state *)user;
  lonejson_status status;
  (void)error;
  status = lonejson__candidate_transform_forward_event(
      state,
      state->options->observer != NULL ? state->options->observer->null_value
                                       : NULL,
      path);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__candidate_transform_scalar_begin(state, path,
                                                      LONEJSON_VALUE_NULL);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__candidate_transform_scalar_end(state, path,
                                                  LONEJSON_VALUE_NULL);
}

static lonejson_candidate_callback_result
lonejson__candidate_transform_begin(void *user,
                                    const lonejson_candidate_info *candidate,
                                    lonejson_error *error) {
  lonejson__candidate_transform_state *state =
      (lonejson__candidate_transform_state *)user;
  lonejson_status status;

  state->candidate =
      state->candidate_override != NULL ? *state->candidate_override
                                        : *candidate;
  if (state->transform_override != NULL) {
    state->transform_candidate = *state->transform_override;
  } else {
    memset(&state->transform_candidate, 0, sizeof(state->transform_candidate));
    state->transform_candidate.mode = state->options->mode;
    state->transform_candidate.physical_index = candidate->index;
    state->transform_candidate.logical_index = candidate->index;
    state->transform_candidate.stream_offset = candidate->stream_offset;
    state->transform_candidate.byte_size = LONEJSON_CANDIDATE_BYTE_SIZE_UNKNOWN;
    state->transform_candidate.gated_spooled =
        state->options->mode ==
        LONEJSON_CANDIDATE_TRANSFORM_MODE_GATED_SPOOLED;
  }
  state->candidate_output_started = 0;
  state->skipping = 0;
  state->skip_depth = 0u;
  state->skip_after_member = 0;
  state->current_emit = 0;
  state->current_scalar_replace = 0;
  status = lonejson__writer_init_sink_with_options(
      &state->writer, state->options->sink, state->options->sink_user,
      &state->runtime->write_options, state->runtime, error);
  if (status != LONEJSON_STATUS_OK) {
    return LONEJSON_CANDIDATE_ERROR;
  }
  state->writer_open = 1;
  if (state->options->candidate_begin != NULL) {
    return state->options->candidate_begin(state->options->candidate_user,
                                           candidate, error);
  }
  return LONEJSON_CANDIDATE_CONTINUE;
}

static lonejson_candidate_callback_result
lonejson__candidate_transform_end(void *user,
                                  const lonejson_candidate_info *candidate,
                                  lonejson_error *error) {
  lonejson__candidate_transform_state *state =
      (lonejson__candidate_transform_state *)user;
  lonejson_candidate_callback_result result;
  lonejson_status status;

  state->candidate =
      state->candidate_override != NULL ? *state->candidate_override
                                        : *candidate;
  if (state->transform_override != NULL) {
    state->transform_candidate = *state->transform_override;
  } else {
    state->transform_candidate.byte_size = candidate->byte_size;
  }
  if (state->candidate_output_started) {
    status = lonejson_writer_finish(&state->writer, error);
    lonejson__candidate_transform_record_status(state, status);
    if (status != LONEJSON_STATUS_OK) {
      return LONEJSON_CANDIDATE_ERROR;
    }
    status = state->options->sink(state->options->sink_user, "\n", 1u, error);
    lonejson__candidate_transform_record_status(state, status);
    if (status != LONEJSON_STATUS_OK) {
      return LONEJSON_CANDIDATE_ERROR;
    }
  }
  lonejson_writer_cleanup(&state->writer);
  state->writer_open = 0;
  while (state->frame_count != 0u) {
    lonejson__candidate_transform_pop(state);
  }
  if (state->options->result != NULL && !state->suppress_result) {
    state->options->result->last_candidate = state->transform_candidate;
    if (state->transform_candidate.mode ==
        LONEJSON_CANDIDATE_TRANSFORM_MODE_STREAMING) {
      state->options->result->candidates_streamed++;
    } else if (state->transform_candidate.mode ==
               LONEJSON_CANDIDATE_TRANSFORM_MODE_GATED_SPOOLED) {
      state->options->result->candidates_spooled++;
    }
  }
  if (state->stopped) {
    return LONEJSON_CANDIDATE_STOP;
  }
  if (state->options->candidate_end != NULL) {
    result = state->options->candidate_end(state->options->candidate_user,
                                           candidate, error);
    return result;
  }
  return LONEJSON_CANDIDATE_CONTINUE;
}

static lonejson_candidate_callback_result
lonejson__candidate_transform_gated_begin(void *user,
                                          const lonejson_candidate_info *candidate,
                                          lonejson_error *error) {
  lonejson__candidate_transform_gated_state *state =
      (lonejson__candidate_transform_gated_state *)user;

  state->candidate = *candidate;
  memset(&state->transform_candidate, 0, sizeof(state->transform_candidate));
  state->transform_candidate.mode =
      LONEJSON_CANDIDATE_TRANSFORM_MODE_GATED_SPOOLED;
  state->transform_candidate.physical_index = candidate->index;
  state->transform_candidate.logical_index = candidate->index;
  state->transform_candidate.stream_offset = candidate->stream_offset;
  state->transform_candidate.byte_size = LONEJSON_CANDIDATE_BYTE_SIZE_UNKNOWN;
  state->transform_candidate.gated_spooled = 1;
  if (state->options->candidate_begin != NULL) {
    return state->options->candidate_begin(state->options->candidate_user,
                                           candidate, error);
  }
  return LONEJSON_CANDIDATE_CONTINUE;
}

static lonejson_candidate_callback_result
lonejson__candidate_transform_gated_end(void *user,
                                        const lonejson_candidate_info *candidate,
                                        lonejson_error *error) {
  lonejson__candidate_transform_gated_state *state =
      (lonejson__candidate_transform_gated_state *)user;
  lonejson_candidate_transform_options replay_options;
  lonejson_candidate_callback_result result;
  lonejson_spooled spool_cursor;
  const lonejson_spooled *spool;
  lonejson_status status;
  size_t spool_size;
  size_t memory_bytes;
  int replay_stopped;

  replay_stopped = 0;
  state->candidate = *candidate;
  state->transform_candidate.byte_size = candidate->byte_size;
  state->transform_candidate.replay_count = 1u;
  spool = candidate->payload_spool;
  if (spool == NULL) {
    state->deferred_status = lonejson__set_error(
        error, LONEJSON_STATUS_INTERNAL_ERROR, 0u, 0u, 0u,
        "candidate transform gated spool is missing");
    return LONEJSON_CANDIDATE_ERROR;
  }
  spool_size = lonejson_spooled_size(spool);
  state->transform_candidate.bytes_spooled = (lonejson_uint64)spool_size;
  state->transform_candidate.spilled = lonejson_spooled_spilled(spool);
  memory_bytes = spool->memory_len;
  state->transform_candidate.memory_bytes = (lonejson_uint64)memory_bytes;
  state->transform_candidate.spill_bytes =
      spool_size > memory_bytes ? (lonejson_uint64)(spool_size - memory_bytes)
                                : 0u;
  if (state->options->result != NULL) {
    state->options->result->candidates_spooled++;
    state->options->result->total_bytes_spooled +=
        state->transform_candidate.bytes_spooled;
    if (state->transform_candidate.spilled) {
      state->options->result->candidates_spilled++;
    }
    state->options->result->total_spill_bytes +=
        state->transform_candidate.spill_bytes;
    state->options->result->candidates_replayed++;
    state->options->result->last_candidate = state->transform_candidate;
  }
  spool_cursor = *spool;
  spool_cursor.read_offset = 0u;
  replay_options = *state->options;
  replay_options.framing = LONEJSON_CANDIDATE_FRAMING_SINGLE_VALUE;
  replay_options.mode = LONEJSON_CANDIDATE_TRANSFORM_MODE_STREAMING;
  replay_options.observer = NULL;
  replay_options.observer_user = NULL;
  replay_options.candidate_begin = NULL;
  replay_options.candidate_end = NULL;
  replay_options.candidate_user = NULL;
  replay_options.result = NULL;
  status = lonejson__transform_candidates_reader_core(
      state->runtime, lonejson__candidate_transform_spooled_reader,
      &spool_cursor,
      &replay_options, error, &state->candidate, &state->transform_candidate,
      1, &replay_stopped);
  if (status != LONEJSON_STATUS_OK) {
    state->deferred_status = status;
    return LONEJSON_CANDIDATE_ERROR;
  }
  if (replay_stopped) {
    return LONEJSON_CANDIDATE_STOP;
  }
  if (state->options->candidate_end != NULL) {
    result = state->options->candidate_end(state->options->candidate_user,
                                           candidate, error);
    return result;
  }
  return LONEJSON_CANDIDATE_CONTINUE;
}

static lonejson_status lonejson__transform_candidates_reader_gated(
    const lonejson_runtime *runtime_state, lonejson_reader_fn reader,
    void *reader_user, const lonejson_candidate_transform_options *options,
    lonejson_error *error) {
  lonejson__candidate_transform_gated_state state;
  lonejson_candidate_stream_options candidate_options;
  lonejson_status status;

  memset(&state, 0, sizeof(state));
  state.options = options;
  state.runtime = runtime_state;
  state.error = error;
  state.deferred_status = LONEJSON_STATUS_OK;
  candidate_options = lonejson_default_candidate_stream_options();
  candidate_options.framing = options->framing;
  candidate_options.capture_mode = LONEJSON_CANDIDATE_CAPTURE_SPOOLED;
  candidate_options.spool_class = options->spool_class;
  candidate_options.max_spooled_payload_bytes =
      options->max_spooled_candidate_bytes;
  candidate_options.path_visitor = options->observer;
  candidate_options.visitor_user = options->observer_user;
  candidate_options.candidate_begin = lonejson__candidate_transform_gated_begin;
  candidate_options.candidate_end = lonejson__candidate_transform_gated_end;
  candidate_options.candidate_user = &state;
  status = lonejson__visit_candidates_reader_with_limits(
      reader, reader_user, &candidate_options, runtime_state,
      &runtime_state->value_limits, runtime_state->config.allocator, error);
  if (status == LONEJSON_STATUS_CALLBACK_FAILED &&
      state.deferred_status != LONEJSON_STATUS_OK) {
    status = state.deferred_status;
  }
  return status;
}

static lonejson_status lonejson__transform_candidates_reader_core(
    const lonejson_runtime *runtime_state, lonejson_reader_fn reader,
    void *reader_user, const lonejson_candidate_transform_options *options,
    lonejson_error *error, const lonejson_candidate_info *candidate_override,
    const lonejson_candidate_transform_candidate_info *transform_override,
    int suppress_result, int *stopped_out) {
  lonejson__candidate_transform_state state;
  lonejson_status status;

  memset(&state, 0, sizeof(state));
  state.options = options;
  state.runtime = runtime_state;
  state.allocator = runtime_state->config.allocator;
  state.error = error;
  state.candidate_override = candidate_override;
  state.transform_override = transform_override;
  state.deferred_status = LONEJSON_STATUS_OK;
  state.suppress_result = suppress_result;
  state.projection_enabled =
      lonejson__candidate_transform_projection_has(options);
  if (state.projection_enabled) {
    status = lonejson__candidate_transform_validate_projection(
        options, &state.projection_root_kind, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  state.visitor = lonejson_default_path_value_visitor();
  state.visitor.object_begin = lonejson__candidate_transform_object_begin;
  state.visitor.object_end = lonejson__candidate_transform_object_end;
  state.visitor.object_key_begin = lonejson__candidate_transform_key_begin;
  state.visitor.object_key_chunk = lonejson__candidate_transform_key_chunk;
  state.visitor.object_key_end = lonejson__candidate_transform_key_end;
  state.visitor.array_begin = lonejson__candidate_transform_array_begin;
  state.visitor.array_end = lonejson__candidate_transform_array_end;
  state.visitor.string_begin = lonejson__candidate_transform_string_begin;
  state.visitor.string_chunk = lonejson__candidate_transform_string_chunk;
  state.visitor.string_end = lonejson__candidate_transform_string_end;
  state.visitor.number_begin = lonejson__candidate_transform_number_begin;
  state.visitor.number_chunk = lonejson__candidate_transform_number_chunk;
  state.visitor.number_end = lonejson__candidate_transform_number_end;
  state.visitor.boolean_value = lonejson__candidate_transform_bool;
  state.visitor.null_value = lonejson__candidate_transform_null;
  state.candidate_options = lonejson_default_candidate_stream_options();
  state.candidate_options.framing = options->framing;
  state.candidate_options.path_visitor = &state.visitor;
  state.candidate_options.visitor_user = &state;
  state.candidate_options.candidate_begin = lonejson__candidate_transform_begin;
  state.candidate_options.candidate_end = lonejson__candidate_transform_end;
  state.candidate_options.candidate_user = &state;
  status = lonejson__visit_candidates_reader_with_limits(
      reader, reader_user, &state.candidate_options, runtime_state,
      &runtime_state->value_limits, runtime_state->config.allocator, error);
  if ((status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED ||
       status == LONEJSON_STATUS_CALLBACK_FAILED) &&
      state.deferred_status != LONEJSON_STATUS_OK) {
    status = state.deferred_status;
    if (status == LONEJSON_STATUS_TRUNCATED && error != NULL &&
        error->code != LONEJSON_STATUS_TRUNCATED) {
      lonejson__set_error(error, LONEJSON_STATUS_TRUNCATED, 0u, 0u, 0u,
                          "candidate transform output truncated");
    }
  }
  if (stopped_out != NULL) {
    *stopped_out = state.stopped;
  }
  lonejson__candidate_transform_cleanup(&state);
  return status;
}

static lonejson_status lonejson__candidate_transform_validate_projection(
    const lonejson_candidate_transform_options *options,
    lonejson__candidate_transform_frame_kind *root_kind,
    lonejson_error *error) {
  const lonejson_candidate_transform_projection_path *rule;
  const lonejson_candidate_transform_projection_segment *segment;
  lonejson__candidate_transform_frame_kind local_root;
  size_t i;
  size_t j;

  if (!lonejson__candidate_transform_projection_has(options)) {
    if (root_kind != NULL) {
      *root_kind = (lonejson__candidate_transform_frame_kind)0;
    }
    return LONEJSON_STATUS_OK;
  }
  local_root = (lonejson__candidate_transform_frame_kind)0;
  for (i = 0u; i < options->projection_path_count; ++i) {
    rule = &options->projection_paths[i];
    if (rule->segment_count == 0u) {
      return lonejson__set_error(error, LONEJSON_STATUS_UNSUPPORTED, 0u, 0u,
                                 0u,
                                 "candidate projection root paths are not "
                                 "supported");
    }
    if (rule->segments == NULL) {
      return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u,
                                 0u, 0u,
                                 "candidate projection path segments are "
                                 "required");
    }
    for (j = 0u; j < rule->segment_count; ++j) {
      segment = &rule->segments[j];
      if (segment->kind ==
          LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER) {
        if (segment->key == NULL && segment->key_len != 0u) {
          return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT,
                                     0u, 0u, 0u,
                                     "candidate projection object key is "
                                     "required");
        }
      } else if (segment->kind !=
                 LONEJSON_CANDIDATE_TRANSFORM_PROJECT_ARRAY_INDEX) {
        return lonejson__set_error(error, LONEJSON_STATUS_UNSUPPORTED, 0u, 0u,
                                   0u,
                                   "candidate projection segment kind is "
                                   "unsupported");
      }
    }
    if (rule->segments[0].kind ==
        LONEJSON_CANDIDATE_TRANSFORM_PROJECT_OBJECT_MEMBER) {
      if (local_root == 0) {
        local_root = LONEJSON__CANDIDATE_TRANSFORM_OBJECT;
      } else if (local_root != LONEJSON__CANDIDATE_TRANSFORM_OBJECT) {
        return lonejson__set_error(error, LONEJSON_STATUS_UNSUPPORTED, 0u, 0u,
                                   0u,
                                   "candidate projection mixes root object and "
                                   "array paths");
      }
    } else {
      if (local_root == 0) {
        local_root = LONEJSON__CANDIDATE_TRANSFORM_ARRAY;
      } else if (local_root != LONEJSON__CANDIDATE_TRANSFORM_ARRAY) {
        return lonejson__set_error(error, LONEJSON_STATUS_UNSUPPORTED, 0u, 0u,
                                   0u,
                                   "candidate projection mixes root object and "
                                   "array paths");
      }
    }
  }
  if (root_kind != NULL) {
    *root_kind = local_root;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__transform_candidates_reader_common(
    lonejson *runtime, lonejson_reader_fn reader, void *reader_user,
    const lonejson_candidate_transform_options *options,
    lonejson_error *error) {
  lonejson_candidate_transform_options local;
  lonejson__runtime_borrow borrow;
  const lonejson_runtime *runtime_state;
  lonejson__candidate_transform_frame_kind projection_root_kind;
  lonejson_status status;

  runtime_state = lonejson__require_runtime_borrow(runtime, &borrow, error);
  if (runtime_state == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  memset(&local, 0, sizeof(local));
  if (options != NULL) {
    local = *options;
  }
  if (local.result != NULL) {
    memset(local.result, 0, sizeof(*local.result));
  }
  if (reader == NULL || local.sink == NULL) {
    lonejson__runtime_borrow_release(&borrow);
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "candidate transform reader and sink are "
                               "required");
  }
  if (local.output_framing == 0) {
    local.output_framing = LONEJSON_CANDIDATE_TRANSFORM_OUTPUT_NDJSON;
  }
  if (local.output_framing != LONEJSON_CANDIDATE_TRANSFORM_OUTPUT_NDJSON) {
    lonejson__runtime_borrow_release(&borrow);
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "invalid candidate transform output framing");
  }
  if (local.mode != LONEJSON_CANDIDATE_TRANSFORM_MODE_STREAMING &&
      local.mode != LONEJSON_CANDIDATE_TRANSFORM_MODE_GATED_SPOOLED &&
      local.mode != LONEJSON_CANDIDATE_TRANSFORM_MODE_UNSUPPORTED) {
    lonejson__runtime_borrow_release(&borrow);
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "invalid candidate transform mode");
  }
  if (local.spool_class != LONEJSON_SPOOL_CLASS_DEFAULT &&
      local.spool_class != LONEJSON_SPOOL_CLASS_BLOB &&
      local.spool_class != LONEJSON_SPOOL_CLASS_LARGE_TEXT) {
    lonejson__runtime_borrow_release(&borrow);
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u,
                               "invalid candidate transform spool class");
  }
  status = lonejson__candidate_transform_validate_projection(
      &local, &projection_root_kind, error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson__runtime_borrow_release(&borrow);
    return status;
  }
  if (local.mode == LONEJSON_CANDIDATE_TRANSFORM_MODE_UNSUPPORTED) {
    lonejson__runtime_borrow_release(&borrow);
    return lonejson__set_error(error, LONEJSON_STATUS_UNSUPPORTED, 0u, 0u, 0u,
                               "candidate transform mode is unsupported");
  }
  if (local.transform == NULL) {
    lonejson__runtime_borrow_release(&borrow);
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "candidate transform callback is required");
  }
  if (local.mode == LONEJSON_CANDIDATE_TRANSFORM_MODE_GATED_SPOOLED) {
    status = lonejson__transform_candidates_reader_gated(
        runtime_state, reader, reader_user, &local, error);
  } else {
    status = lonejson__transform_candidates_reader_core(
        runtime_state, reader, reader_user, &local, error, NULL, NULL, 0, NULL);
  }
  lonejson__runtime_borrow_release(&borrow);
  return status;
}

lonejson_status lonejson_transform_candidates_reader(
    lonejson *runtime, lonejson_reader_fn reader, void *reader_user,
    const lonejson_candidate_transform_options *options,
    lonejson_error *error) {
  return lonejson__transform_candidates_reader_common(
      runtime, reader, reader_user, options, error);
}

lonejson_status lonejson_transform_candidates_buffer(
    lonejson *runtime, const void *data, size_t len,
    const lonejson_candidate_transform_options *options,
    lonejson_error *error) {
  lonejson_buffer_reader reader;

  if (data == NULL && len != 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "candidate transform buffer is required");
  }
  lonejson_buffer_reader_init(&reader, data, len);
  return lonejson_transform_candidates_reader(
      runtime, lonejson_buffer_reader_read, &reader, options, error);
}

lonejson_status lonejson_transform_candidates_filep(
    lonejson *runtime, FILE *fp,
    const lonejson_candidate_transform_options *options,
    lonejson_error *error) {
  if (fp == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "candidate transform file is required");
  }
  return lonejson_transform_candidates_reader(runtime, lonejson__file_reader,
                                              fp, options, error);
}

lonejson_status lonejson_transform_candidates_path(
    lonejson *runtime, const char *path,
    const lonejson_candidate_transform_options *options,
    lonejson_error *error) {
  FILE *fp;
  lonejson_status status;

  if (path == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "candidate transform path is required");
  }
  fp = fopen(path, "rb");
  if (fp == NULL) {
    if (error != NULL) {
      error->system_errno = errno;
    }
    return lonejson__set_error(error, LONEJSON_STATUS_IO_ERROR, 0u, 1u, 0u,
                               "failed to open '%s'", path);
  }
  status = lonejson_transform_candidates_filep(runtime, fp, options, error);
  fclose(fp);
  return status;
}

lonejson_status lonejson_transform_candidates_fd(
    lonejson *runtime, int fd,
    const lonejson_candidate_transform_options *options,
    lonejson_error *error) {
  if (fd < 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "candidate transform fd is required");
  }
  return lonejson_transform_candidates_reader(runtime, lonejson__fd_reader, &fd,
                                              options, error);
}
