typedef enum lonejson__candidate_transform_frame_kind {
  LONEJSON__CANDIDATE_TRANSFORM_OBJECT = 1,
  LONEJSON__CANDIDATE_TRANSFORM_ARRAY = 2
} lonejson__candidate_transform_frame_kind;

typedef struct lonejson__candidate_transform_frame {
  lonejson__candidate_transform_frame_kind kind;
  lonejson__byte_buffer key;
} lonejson__candidate_transform_frame;

typedef struct lonejson__candidate_transform_state {
  const lonejson_candidate_transform_options *options;
  const lonejson_runtime *runtime;
  const lonejson_allocator *allocator;
  lonejson_error *error;
  lonejson_candidate_info candidate;
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
  int current_emit;
  int current_scalar_replace;
} lonejson__candidate_transform_state;

static lonejson__candidate_transform_frame *
lonejson__candidate_transform_top(lonejson__candidate_transform_state *state) {
  return state->frame_count == 0u ? NULL
                                  : &state->frames[state->frame_count - 1u];
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
  }
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
  lonejson_status status;

  if (state->candidate_output_started) {
    return LONEJSON_STATUS_OK;
  }
  if (state->emitted_any_candidate) {
    status =
        state->options->sink(state->options->sink_user, "\n", 1u, state->error);
    lonejson__candidate_transform_record_status(state, status);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
  }
  state->candidate_output_started = 1;
  state->emitted_any_candidate = 1;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__candidate_transform_emit_key(
    lonejson__candidate_transform_state *state) {
  lonejson__candidate_transform_frame *frame;

  frame = lonejson__candidate_transform_top(state);
  if (frame == NULL || frame->kind != LONEJSON__CANDIDATE_TRANSFORM_OBJECT) {
    return LONEJSON_STATUS_OK;
  }
  return lonejson__candidate_transform_record_status(
      state, lonejson_writer_key(&state->writer,
                                 frame->key.data != NULL ? frame->key.data : "",
                                 frame->key.len, state->error));
}

static lonejson_status lonejson__candidate_transform_prepare_emit(
    lonejson__candidate_transform_state *state) {
  lonejson_status status;

  status = lonejson__candidate_transform_prefix(state);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__candidate_transform_emit_key(state);
}

static lonejson_status lonejson__candidate_transform_replace(
    lonejson__candidate_transform_state *state, const lonejson_value_path *path,
    lonejson_value_type type) {
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
  event.path = path;
  event.value_type = type;
  status = lonejson__candidate_transform_prepare_emit(state);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  return lonejson__candidate_transform_record_status(
      state, state->options->replace(state->options->transform_user, &event,
                                     &state->writer, state->error));
}

static lonejson_candidate_transform_action
lonejson__candidate_transform_decide(lonejson__candidate_transform_state *state,
                                     const lonejson_value_path *path,
                                     lonejson_value_type type) {
  lonejson_candidate_transform_event event;
  lonejson_candidate_transform_action action;

  if (state->options->transform == NULL) {
    return LONEJSON_CANDIDATE_TRANSFORM_KEEP;
  }
  memset(&event, 0, sizeof(event));
  event.candidate = &state->candidate;
  event.path = path;
  event.value_type = type;
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
    lonejson_value_type type, int container_value) {
  lonejson_status status;

  if (action == LONEJSON_CANDIDATE_TRANSFORM_KEEP) {
    status = lonejson__candidate_transform_prepare_emit(state);
    if (status == LONEJSON_STATUS_OK) {
      state->current_emit = container_value ? 0 : 1;
    }
    return status;
  }
  if (action == LONEJSON_CANDIDATE_TRANSFORM_DROP) {
    state->current_emit = 0;
    return LONEJSON_STATUS_OK;
  }
  if (action == LONEJSON_CANDIDATE_TRANSFORM_REPLACE) {
    status = lonejson__candidate_transform_replace(state, path, type);
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

  if (state->skipping) {
    state->skip_depth++;
    return LONEJSON_STATUS_OK;
  }
  if (state->stopped) {
    state->skipping = 1;
    state->skip_depth = 1u;
    return LONEJSON_STATUS_OK;
  }
  action = lonejson__candidate_transform_decide(state, path, type);
  status =
      lonejson__candidate_transform_action_status(state, action, path, type, 1);
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
    return lonejson__candidate_transform_push(state, frame_kind);
  }
  if (action == LONEJSON_CANDIDATE_TRANSFORM_STOP) {
    state->skipping = 1;
    state->skip_depth = 1u;
    return LONEJSON_STATUS_OK;
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
    lonejson__candidate_transform_state *state, lonejson_value_type type) {
  if (state->skipping) {
    if (state->skip_depth != 0u) {
      state->skip_depth--;
    }
    if (state->skip_depth == 0u) {
      state->skipping = 0;
    }
    return LONEJSON_STATUS_OK;
  }
  if (type == LONEJSON_VALUE_OBJECT) {
    lonejson__candidate_transform_pop(state);
    return lonejson__candidate_transform_record_status(
        state, lonejson_writer_end_object(&state->writer, state->error));
  }
  lonejson__candidate_transform_pop(state);
  return lonejson__candidate_transform_record_status(
      state, lonejson_writer_end_array(&state->writer, state->error));
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
  return lonejson__candidate_transform_end_container(state,
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
  return lonejson__candidate_transform_end_container(state,
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
  (void)error;
  return lonejson__candidate_transform_forward_event(
      state,
      state->options->observer != NULL
          ? state->options->observer->object_key_end
          : NULL,
      path);
}

static lonejson_status lonejson__candidate_transform_scalar_begin(
    lonejson__candidate_transform_state *state, const lonejson_value_path *path,
    lonejson_value_type type) {
  lonejson_candidate_transform_action action;

  state->current_emit = 0;
  state->current_scalar_replace = 0;
  if (state->skipping || state->stopped) {
    return LONEJSON_STATUS_OK;
  }
  action = lonejson__candidate_transform_decide(state, path, type);
  return lonejson__candidate_transform_action_status(state, action, path, type,
                                                     0);
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
  if (status == LONEJSON_STATUS_OK && state->current_emit) {
    status = lonejson_writer_string_begin(&state->writer, state->error);
    lonejson__candidate_transform_record_status(state, status);
  }
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
  if (status != LONEJSON_STATUS_OK || !state->current_emit) {
    return status;
  }
  return lonejson__candidate_transform_record_status(
      state,
      lonejson_writer_string_chunk(&state->writer, data, len, state->error));
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
  if (status != LONEJSON_STATUS_OK || !state->current_emit) {
    state->current_emit = 0;
    return status;
  }
  state->current_emit = 0;
  return lonejson__candidate_transform_record_status(
      state, lonejson_writer_string_end(&state->writer, state->error));
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
  if (status == LONEJSON_STATUS_OK && state->current_emit) {
    status = lonejson_writer_number_begin(&state->writer, state->error);
    lonejson__candidate_transform_record_status(state, status);
  }
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
  if (status != LONEJSON_STATUS_OK || !state->current_emit) {
    return status;
  }
  return lonejson__candidate_transform_record_status(
      state,
      lonejson_writer_number_chunk(&state->writer, data, len, state->error));
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
  if (status != LONEJSON_STATUS_OK || !state->current_emit) {
    state->current_emit = 0;
    return status;
  }
  state->current_emit = 0;
  return lonejson__candidate_transform_record_status(
      state, lonejson_writer_number_end(&state->writer, state->error));
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
  if (status == LONEJSON_STATUS_OK && state->current_emit) {
    status = lonejson_writer_bool(&state->writer, value, state->error);
    lonejson__candidate_transform_record_status(state, status);
  }
  state->current_emit = 0;
  return status;
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
  if (status == LONEJSON_STATUS_OK && state->current_emit) {
    status = lonejson_writer_null(&state->writer, state->error);
    lonejson__candidate_transform_record_status(state, status);
  }
  state->current_emit = 0;
  return status;
}

static lonejson_candidate_callback_result
lonejson__candidate_transform_begin(void *user,
                                    const lonejson_candidate_info *candidate,
                                    lonejson_error *error) {
  lonejson__candidate_transform_state *state =
      (lonejson__candidate_transform_state *)user;
  lonejson_status status;

  state->candidate = *candidate;
  state->candidate_output_started = 0;
  state->skipping = 0;
  state->skip_depth = 0u;
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

  state->candidate = *candidate;
  if (state->candidate_output_started) {
    status = lonejson_writer_finish(&state->writer, error);
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

static lonejson_status lonejson__transform_candidates_reader_common(
    lonejson *runtime, lonejson_reader_fn reader, void *reader_user,
    const lonejson_candidate_transform_options *options,
    lonejson_error *error) {
  lonejson__candidate_transform_state state;
  lonejson_candidate_transform_options local;
  lonejson__runtime_borrow borrow;
  const lonejson_runtime *runtime_state;
  lonejson_status status;

  runtime_state = lonejson__require_runtime_borrow(runtime, &borrow, error);
  if (runtime_state == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  memset(&local, 0, sizeof(local));
  if (options != NULL) {
    local = *options;
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
  if (local.transform == NULL) {
    lonejson__runtime_borrow_release(&borrow);
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "candidate transform callback is required");
  }
  memset(&state, 0, sizeof(state));
  state.options = &local;
  state.runtime = runtime_state;
  state.allocator = runtime_state->config.allocator;
  state.error = error;
  state.deferred_status = LONEJSON_STATUS_OK;
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
  state.candidate_options.framing = local.framing;
  state.candidate_options.path_visitor = &state.visitor;
  state.candidate_options.visitor_user = &state;
  state.candidate_options.candidate_begin = lonejson__candidate_transform_begin;
  state.candidate_options.candidate_end = lonejson__candidate_transform_end;
  state.candidate_options.candidate_user = &state;
  status = lonejson_visit_candidates_reader(runtime, reader, reader_user,
                                            &state.candidate_options, error);
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
  lonejson__candidate_transform_cleanup(&state);
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
