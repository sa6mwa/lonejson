#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lonejson.h"

#define FUZZ_MAX_DEPTH 128u
#define FUZZ_CONTAINER_OBJECT 1
#define FUZZ_CONTAINER_ARRAY 2

typedef struct fuzz_path_segment {
  size_t len;
  uint32_t hash;
} fuzz_path_segment;

typedef struct fuzz_path_frame {
  int type;
  size_t next_index;
  size_t pending_key_len;
  uint32_t pending_key_hash;
} fuzz_path_frame;

typedef struct fuzz_path_state {
  fuzz_path_segment path[FUZZ_MAX_DEPTH];
  fuzz_path_frame containers[FUZZ_MAX_DEPTH];
  size_t path_depth;
  size_t container_depth;
  size_t callback_count;
  size_t fail_after;
  size_t key_bytes;
  size_t string_bytes;
  size_t number_bytes;
} fuzz_path_state;

typedef struct fuzz_reader_state {
  const uint8_t *data;
  size_t len;
  size_t offset;
  size_t chunk_size;
} fuzz_reader_state;

static uint32_t fuzz_hash_bytes(uint32_t hash, const char *data, size_t len) {
  size_t i;

  for (i = 0u; i < len; ++i) {
    hash ^= (uint32_t)(unsigned char)data[i];
    hash *= 16777619u;
  }
  return hash;
}

static void fuzz_abort_if(int condition) {
  if (condition) {
    abort();
  }
}

static void fuzz_check_path(fuzz_path_state *state,
                            const lonejson_value_path *path) {
  size_t i;
  uint32_t hash;

  fuzz_abort_if(path == NULL);
  fuzz_abort_if(path->segment_count != state->path_depth);
  if (path->segment_count != 0u) {
    fuzz_abort_if(path->segments == NULL);
  }
  for (i = 0u; i < path->segment_count; ++i) {
    fuzz_abort_if(path->segments[i].len != state->path[i].len);
    if (path->segments[i].len != 0u) {
      fuzz_abort_if(path->segments[i].data == NULL);
    }
    hash = fuzz_hash_bytes(2166136261u, path->segments[i].data,
                           path->segments[i].len);
    fuzz_abort_if(hash != state->path[i].hash);
  }
}

static int fuzz_current_container_is_array(fuzz_path_state *state) {
  if (state->container_depth == 0u) {
    return 0;
  }
  return state->containers[state->container_depth - 1u].type ==
         FUZZ_CONTAINER_ARRAY;
}

static void fuzz_push_segment(fuzz_path_state *state, size_t len,
                              uint32_t hash) {
  fuzz_abort_if(state->path_depth >= FUZZ_MAX_DEPTH);
  state->path[state->path_depth].len = len;
  state->path[state->path_depth].hash = hash;
  ++state->path_depth;
}

static void fuzz_before_value(fuzz_path_state *state) {
  fuzz_path_frame *frame;
  char index_text[32];
  int written;

  if (!fuzz_current_container_is_array(state)) {
    return;
  }
  frame = &state->containers[state->container_depth - 1u];
  written = snprintf(index_text, sizeof(index_text), "%lu",
                     (unsigned long)frame->next_index);
  fuzz_abort_if(written < 0 || (size_t)written >= sizeof(index_text));
  fuzz_push_segment(state, (size_t)written,
                    fuzz_hash_bytes(2166136261u, index_text,
                                    (size_t)written));
}

static void fuzz_complete_value(fuzz_path_state *state) {
  if (state->path_depth != 0u) {
    --state->path_depth;
  }
  if (fuzz_current_container_is_array(state)) {
    ++state->containers[state->container_depth - 1u].next_index;
  }
}

static lonejson_status fuzz_maybe_fail(fuzz_path_state *state,
                                       lonejson_error *error) {
  ++state->callback_count;
  if (state->fail_after != 0u && state->callback_count >= state->fail_after) {
    if (error != NULL) {
      lonejson_error_init(error);
      error->code = LONEJSON_STATUS_CALLBACK_FAILED;
      memcpy(error->message, "fuzz path visitor failure",
             sizeof("fuzz path visitor failure"));
    }
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_object_begin(void *user,
                                         const lonejson_value_path *path,
                                         lonejson_error *error) {
  fuzz_path_state *state = (fuzz_path_state *)user;
  fuzz_path_frame *frame;

  fuzz_before_value(state);
  fuzz_check_path(state, path);
  fuzz_abort_if(state->container_depth >= FUZZ_MAX_DEPTH);
  frame = &state->containers[state->container_depth++];
  memset(frame, 0, sizeof(*frame));
  frame->type = FUZZ_CONTAINER_OBJECT;
  return fuzz_maybe_fail(state, error);
}

static lonejson_status fuzz_object_end(void *user,
                                       const lonejson_value_path *path,
                                       lonejson_error *error) {
  fuzz_path_state *state = (fuzz_path_state *)user;

  fuzz_check_path(state, path);
  fuzz_abort_if(state->container_depth == 0u);
  fuzz_abort_if(state->containers[state->container_depth - 1u].type !=
                FUZZ_CONTAINER_OBJECT);
  --state->container_depth;
  fuzz_complete_value(state);
  return fuzz_maybe_fail(state, error);
}

static lonejson_status fuzz_array_begin(void *user,
                                        const lonejson_value_path *path,
                                        lonejson_error *error) {
  fuzz_path_state *state = (fuzz_path_state *)user;
  fuzz_path_frame *frame;

  fuzz_before_value(state);
  fuzz_check_path(state, path);
  fuzz_abort_if(state->container_depth >= FUZZ_MAX_DEPTH);
  frame = &state->containers[state->container_depth++];
  memset(frame, 0, sizeof(*frame));
  frame->type = FUZZ_CONTAINER_ARRAY;
  return fuzz_maybe_fail(state, error);
}

static lonejson_status fuzz_array_end(void *user,
                                      const lonejson_value_path *path,
                                      lonejson_error *error) {
  fuzz_path_state *state = (fuzz_path_state *)user;

  fuzz_check_path(state, path);
  fuzz_abort_if(state->container_depth == 0u);
  fuzz_abort_if(state->containers[state->container_depth - 1u].type !=
                FUZZ_CONTAINER_ARRAY);
  --state->container_depth;
  fuzz_complete_value(state);
  return fuzz_maybe_fail(state, error);
}

static lonejson_status fuzz_key_begin(void *user,
                                      const lonejson_value_path *path,
                                      lonejson_error *error) {
  fuzz_path_state *state = (fuzz_path_state *)user;
  fuzz_path_frame *frame;

  fuzz_check_path(state, path);
  fuzz_abort_if(state->container_depth == 0u);
  frame = &state->containers[state->container_depth - 1u];
  fuzz_abort_if(frame->type != FUZZ_CONTAINER_OBJECT);
  frame->pending_key_len = 0u;
  frame->pending_key_hash = 2166136261u;
  return fuzz_maybe_fail(state, error);
}

static lonejson_status fuzz_key_chunk(void *user,
                                      const lonejson_value_path *path,
                                      const char *data, size_t len,
                                      lonejson_error *error) {
  fuzz_path_state *state = (fuzz_path_state *)user;
  fuzz_path_frame *frame;

  fuzz_check_path(state, path);
  fuzz_abort_if(state->container_depth == 0u);
  frame = &state->containers[state->container_depth - 1u];
  fuzz_abort_if(frame->type != FUZZ_CONTAINER_OBJECT);
  frame->pending_key_len += len;
  frame->pending_key_hash =
      fuzz_hash_bytes(frame->pending_key_hash, data, len);
  state->key_bytes += len;
  return fuzz_maybe_fail(state, error);
}

static lonejson_status fuzz_key_end(void *user, const lonejson_value_path *path,
                                    lonejson_error *error) {
  fuzz_path_state *state = (fuzz_path_state *)user;
  fuzz_path_frame *frame;
  lonejson_status status;

  fuzz_check_path(state, path);
  fuzz_abort_if(state->container_depth == 0u);
  frame = &state->containers[state->container_depth - 1u];
  fuzz_abort_if(frame->type != FUZZ_CONTAINER_OBJECT);
  status = fuzz_maybe_fail(state, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  fuzz_push_segment(state, frame->pending_key_len, frame->pending_key_hash);
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_string_begin(void *user,
                                         const lonejson_value_path *path,
                                         lonejson_error *error) {
  fuzz_path_state *state = (fuzz_path_state *)user;

  fuzz_before_value(state);
  fuzz_check_path(state, path);
  return fuzz_maybe_fail(state, error);
}

static lonejson_status fuzz_string_chunk(void *user,
                                         const lonejson_value_path *path,
                                         const char *data, size_t len,
                                         lonejson_error *error) {
  fuzz_path_state *state = (fuzz_path_state *)user;

  fuzz_check_path(state, path);
  (void)data;
  state->string_bytes += len;
  return fuzz_maybe_fail(state, error);
}

static lonejson_status fuzz_string_end(void *user,
                                       const lonejson_value_path *path,
                                       lonejson_error *error) {
  fuzz_path_state *state = (fuzz_path_state *)user;

  fuzz_check_path(state, path);
  fuzz_complete_value(state);
  return fuzz_maybe_fail(state, error);
}

static lonejson_status fuzz_number_begin(void *user,
                                         const lonejson_value_path *path,
                                         lonejson_error *error) {
  fuzz_path_state *state = (fuzz_path_state *)user;

  fuzz_before_value(state);
  fuzz_check_path(state, path);
  return fuzz_maybe_fail(state, error);
}

static lonejson_status fuzz_number_chunk(void *user,
                                         const lonejson_value_path *path,
                                         const char *data, size_t len,
                                         lonejson_error *error) {
  fuzz_path_state *state = (fuzz_path_state *)user;

  fuzz_check_path(state, path);
  (void)data;
  state->number_bytes += len;
  return fuzz_maybe_fail(state, error);
}

static lonejson_status fuzz_number_end(void *user,
                                       const lonejson_value_path *path,
                                       lonejson_error *error) {
  fuzz_path_state *state = (fuzz_path_state *)user;

  fuzz_check_path(state, path);
  fuzz_complete_value(state);
  return fuzz_maybe_fail(state, error);
}

static lonejson_status fuzz_bool(void *user, const lonejson_value_path *path,
                                 int value, lonejson_error *error) {
  fuzz_path_state *state = (fuzz_path_state *)user;

  (void)value;
  fuzz_before_value(state);
  fuzz_check_path(state, path);
  fuzz_complete_value(state);
  return fuzz_maybe_fail(state, error);
}

static lonejson_status fuzz_null(void *user, const lonejson_value_path *path,
                                 lonejson_error *error) {
  fuzz_path_state *state = (fuzz_path_state *)user;

  fuzz_before_value(state);
  fuzz_check_path(state, path);
  fuzz_complete_value(state);
  return fuzz_maybe_fail(state, error);
}

static lonejson_path_value_visitor fuzz_visitor(void) {
  lonejson_path_value_visitor visitor;

  visitor = lonejson_default_path_value_visitor();
  visitor.object_begin = fuzz_object_begin;
  visitor.object_end = fuzz_object_end;
  visitor.object_key_begin = fuzz_key_begin;
  visitor.object_key_chunk = fuzz_key_chunk;
  visitor.object_key_end = fuzz_key_end;
  visitor.array_begin = fuzz_array_begin;
  visitor.array_end = fuzz_array_end;
  visitor.string_begin = fuzz_string_begin;
  visitor.string_chunk = fuzz_string_chunk;
  visitor.string_end = fuzz_string_end;
  visitor.number_begin = fuzz_number_begin;
  visitor.number_chunk = fuzz_number_chunk;
  visitor.number_end = fuzz_number_end;
  visitor.boolean_value = fuzz_bool;
  visitor.null_value = fuzz_null;
  return visitor;
}

static lonejson_read_result fuzz_reader(void *user, unsigned char *buffer,
                                        size_t capacity) {
  fuzz_reader_state *state = (fuzz_reader_state *)user;
  lonejson_read_result result;
  size_t remaining;
  size_t take;

  memset(&result, 0, sizeof(result));
  remaining = state->len - state->offset;
  if (remaining == 0u) {
    result.eof = 1;
    return result;
  }
  take = state->chunk_size;
  if (take > remaining) {
    take = remaining;
  }
  if (take > capacity) {
    take = capacity;
  }
  memcpy(buffer, state->data + state->offset, take);
  state->offset += take;
  result.bytes_read = take;
  result.eof = state->offset == state->len ? 1 : 0;
  return result;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  lonejson_config config;
  lonejson *runtime;
  lonejson_path_value_visitor visitor;
  lonejson_error error;
  fuzz_path_state state;
  fuzz_reader_state reader;
  char *cstr;

  if (size == 0u) {
    return 0;
  }

  config = lonejson_default_config();
  config.json_value_max_depth = 1u + (size_t)(data[0] & 0x3Fu);
  config.json_value_max_string_bytes = 1u + (size_t)data[0] * 64u;
  config.json_value_max_key_bytes = 1u + (size_t)data[0] * 16u;
  config.json_value_max_number_bytes = 1u + (size_t)(data[0] & 0x7Fu);
  config.json_value_max_total_bytes = (data[0] & 0x80u) ? size : 0u;
  runtime = lonejson_new(&config, &error);
  if (runtime == NULL) {
    return 0;
  }
  visitor = fuzz_visitor();

  memset(&state, 0, sizeof(state));
  state.fail_after = (data[0] & 0x10u) ? (size_t)(data[0] & 0x0Fu) : 0u;
  (void)lonejson_visit_path_value_buffer(runtime, data, size, &visitor, &state,
                                         &error);

  memset(&state, 0, sizeof(state));
  reader.data = data;
  reader.len = size;
  reader.offset = 0u;
  reader.chunk_size = 1u + (size_t)(data[0] % 31u);
  (void)lonejson_visit_path_value_reader(runtime, fuzz_reader, &reader,
                                         &visitor, &state, &error);

  cstr = (char *)malloc(size + 1u);
  if (cstr != NULL) {
    memcpy(cstr, data, size);
    cstr[size] = '\0';
    memset(&state, 0, sizeof(state));
    (void)lonejson_visit_path_value_cstr(runtime, cstr, &visitor, &state,
                                         &error);
    free(cstr);
  }

  lonejson_free(runtime);
  return 0;
}
