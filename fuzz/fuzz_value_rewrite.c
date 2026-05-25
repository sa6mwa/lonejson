#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lonejson.h"

typedef struct fuzz_visit_state {
  size_t events;
  size_t bytes;
  unsigned fail_mode;
} fuzz_visit_state;

typedef struct fuzz_replace_with_state {
  int mode;
  unsigned fail_mode;
} fuzz_replace_with_state;

typedef struct fuzz_sink_state {
  lonejson_owned_buffer out;
  size_t total;
  size_t fail_after;
} fuzz_sink_state;

static lonejson_status fuzz_fail(lonejson_error *error, const char *message) {
  if (error != NULL) {
    lonejson_error_init(error);
    error->code = LONEJSON_STATUS_CALLBACK_FAILED;
    memcpy(error->message, message, strlen(message) + 1u);
  }
  return LONEJSON_STATUS_CALLBACK_FAILED;
}

static lonejson_status fuzz_visit_event(void *user, lonejson_error *error) {
  fuzz_visit_state *state = (fuzz_visit_state *)user;
  state->events++;
  if (state->fail_mode == 1u) {
    return fuzz_fail(error, "fuzz visitor event failure");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_visit_chunk(void *user, const char *data,
                                        size_t len, lonejson_error *error) {
  fuzz_visit_state *state = (fuzz_visit_state *)user;
  (void)data;
  state->bytes += len;
  if (state->fail_mode == 2u) {
    return fuzz_fail(error, "fuzz visitor chunk failure");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_visit_bool(void *user, int value,
                                       lonejson_error *error) {
  fuzz_visit_state *state = (fuzz_visit_state *)user;
  (void)value;
  state->events++;
  if (state->fail_mode == 3u) {
    return fuzz_fail(error, "fuzz visitor bool failure");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_sink(void *user, const void *data, size_t len,
                                 lonejson_error *error) {
  fuzz_sink_state *state = (fuzz_sink_state *)user;

  state->total += len;
  if (state->fail_after != 0u && state->total >= state->fail_after) {
    return fuzz_fail(error, "fuzz sink failure");
  }
  return lonejson_owned_buffer_sink(&state->out, data, len, error);
}

static lonejson_status
fuzz_replace_with(lonejson_writer *writer,
                  const lonejson_value_rewrite_old_value *old_value, void *user,
                  lonejson_error *error) {
  fuzz_replace_with_state *state = (fuzz_replace_with_state *)user;

  if (state->fail_mode == 1u) {
    return fuzz_fail(error, "fuzz replacement failure");
  }
  if (state->fail_mode == 2u) {
    return LONEJSON_STATUS_OK;
  }
  if (state->fail_mode == 3u) {
    lonejson_status status = lonejson_writer_null(writer, error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    return lonejson_writer_null(writer, error);
  }
  if (!old_value->present) {
    return lonejson_writer_i64(writer, 1, error);
  }
  switch (old_value->type) {
  case LONEJSON_VALUE_NUMBER:
    if (old_value->number == NULL && old_value->number_len != 0u) {
      return LONEJSON_STATUS_INTERNAL_ERROR;
    }
    return lonejson_writer_i64(writer, (lonejson_int64)old_value->number_len,
                               error);
  case LONEJSON_VALUE_BOOL:
    return lonejson_writer_bool(writer,
                                state->mode ? !old_value->boolean
                                            : old_value->boolean,
                                error);
  case LONEJSON_VALUE_NULL:
    return lonejson_writer_null(writer, error);
  case LONEJSON_VALUE_STRING:
    return lonejson_writer_string(writer, "string", 6u, error);
  case LONEJSON_VALUE_OBJECT:
  case LONEJSON_VALUE_ARRAY:
  default:
    return lonejson_writer_null(writer, error);
  }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  lonejson *runtime;
  static const char *const path_count[] = {"count"};
  static const char *const path_meta_count[] = {"meta", "count"};
  static const char *const path_payload[] = {"payload"};
  static const char *const path_items_zero[] = {"items", "0"};
  const char *const *paths[5];
  size_t path_counts[5];
  lonejson_value_rewrite_options options;
  lonejson_value_rewrite_selector_options selector_options;
  lonejson_value_visitor visitor;
  lonejson_json_value replacement;
  fuzz_sink_state sink_state;
  lonejson_error error;
  lonejson_error validate_error;
  lonejson_status status;
  fuzz_visit_state visit_state;
  fuzz_replace_with_state replace_state;
  size_t selector;
  int use_selector;
  int action;

  if (size == 0u) {
    return 0;
  }

  paths[0] = NULL;
  path_counts[0] = 0u;
  paths[1] = path_count;
  path_counts[1] = 1u;
  paths[2] = path_meta_count;
  path_counts[2] = 2u;
  paths[3] = path_payload;
  path_counts[3] = 1u;
  paths[4] = path_items_zero;
  path_counts[4] = 2u;

  selector = (size_t)(data[0] % 5u);
  action = (int)((size > 1u ? data[1] : 0u) % 4u);
  use_selector = size > 2u && (data[2] & 1u);
  replace_state.mode = size > 3u && (data[3] & 1u);
  replace_state.fail_mode = size > 4u ? (unsigned)(data[4] % 5u) : 0u;
  memset(&visit_state, 0, sizeof(visit_state));
  visit_state.fail_mode = size > 5u ? (unsigned)(data[5] % 5u) : 0u;
  memset(&sink_state, 0, sizeof(sink_state));
  if (size > 6u && (data[6] & 1u)) {
    sink_state.fail_after = (size_t)((data[6] % 17u) + 1u);
  }
  visitor = lonejson_default_value_visitor();
  visitor.object_begin = fuzz_visit_event;
  visitor.object_end = fuzz_visit_event;
  visitor.object_key_chunk = fuzz_visit_chunk;
  visitor.array_begin = fuzz_visit_event;
  visitor.array_end = fuzz_visit_event;
  visitor.string_chunk = fuzz_visit_chunk;
  visitor.number_chunk = fuzz_visit_chunk;
  visitor.boolean_value = fuzz_visit_bool;
  visitor.null_value = fuzz_visit_event;

  runtime = lonejson_new(NULL, &error);
  if (runtime == NULL) {
    return 0;
  }
  lonejson_json_value_init(runtime, &replacement);
  (void)lonejson_json_value_set_buffer(&replacement, "0", 1u, &error);
  lonejson_owned_buffer_init(&sink_state.out);
  memset(&options, 0, sizeof(options));
  options.target_segments = paths[selector];
  options.target_segment_count = path_counts[selector];
  options.action = (lonejson_value_rewrite_action)action;
  if (options.action == LONEJSON_VALUE_REWRITE_DROP &&
      options.target_segment_count == 0u) {
    options.action = LONEJSON_VALUE_REWRITE_KEEP;
  }
  options.replacement.json = &replacement;
  options.old_value_visitor = &visitor;
  options.old_value_user = &visit_state;
  options.replace = fuzz_replace_with;
  options.replace_user = &replace_state;

  if (use_selector) {
    memset(&selector_options, 0, sizeof(selector_options));
    selector_options.selector = selector == 2u ? "meta.count" : "count";
    selector_options.action = options.action;
    selector_options.replacement = options.replacement;
    selector_options.old_value_visitor = options.old_value_visitor;
    selector_options.old_value_user = options.old_value_user;
    selector_options.replace = options.replace;
    selector_options.replace_user = options.replace_user;
    status = lonejson_value_rewrite_selector_buffer(runtime, data, size,
                                                    fuzz_sink, &sink_state,
                                                    &selector_options, &error);
  } else {
    status = lonejson_value_rewrite_buffer(runtime, data, size, fuzz_sink,
                                           &sink_state, &options, &error);
  }

  if (status == LONEJSON_STATUS_OK) {
    if (sink_state.out.data == NULL ||
        lonejson_validate_buffer(runtime, sink_state.out.data, sink_state.out.len,
                                 &validate_error) != LONEJSON_STATUS_OK) {
      abort();
    }
  }
  lonejson_owned_buffer_free(&sink_state.out);
  lonejson_json_value_cleanup(&replacement);
  lonejson_free(runtime);
  return 0;
}
