#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lonejson.h"

typedef struct fuzz_candidate_state {
  size_t begin_count;
  size_t end_count;
  size_t event_count;
  size_t last_begin_offset;
  unsigned stop_after_first;
  unsigned fail_after_first;
} fuzz_candidate_state;

typedef struct fuzz_candidate_reader {
  const uint8_t *data;
  size_t len;
  size_t offset;
  size_t chunk_size;
} fuzz_candidate_reader;

static void fuzz_abort_if(int cond) {
  if (cond) {
    abort();
  }
}

static lonejson_candidate_callback_result
fuzz_candidate_begin(void *user, const lonejson_candidate_info *candidate,
                     lonejson_error *error) {
  fuzz_candidate_state *state = (fuzz_candidate_state *)user;
  (void)error;
  fuzz_abort_if(candidate == NULL);
  fuzz_abort_if(candidate->byte_size != (size_t)-1);
  fuzz_abort_if(state->begin_count != candidate->index);
  state->last_begin_offset = candidate->stream_offset;
  ++state->begin_count;
  if (state->fail_after_first && state->begin_count == 2u) {
    return LONEJSON_CANDIDATE_ERROR;
  }
  return LONEJSON_CANDIDATE_CONTINUE;
}

static lonejson_candidate_callback_result
fuzz_candidate_end(void *user, const lonejson_candidate_info *candidate,
                   lonejson_error *error) {
  fuzz_candidate_state *state = (fuzz_candidate_state *)user;
  (void)error;
  fuzz_abort_if(candidate == NULL);
  fuzz_abort_if(state->end_count != candidate->index);
  fuzz_abort_if(candidate->byte_size == (size_t)-1);
  fuzz_abort_if(candidate->stream_offset != state->last_begin_offset);
  ++state->end_count;
  if (state->stop_after_first && state->end_count == 1u) {
    return LONEJSON_CANDIDATE_STOP;
  }
  return LONEJSON_CANDIDATE_CONTINUE;
}

static lonejson_status fuzz_event(void *user, lonejson_error *error) {
  fuzz_candidate_state *state = (fuzz_candidate_state *)user;
  (void)error;
  ++state->event_count;
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_chunk(void *user, const char *data, size_t len,
                                  lonejson_error *error) {
  fuzz_candidate_state *state = (fuzz_candidate_state *)user;
  (void)data;
  (void)len;
  (void)error;
  ++state->event_count;
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_bool(void *user, int value, lonejson_error *error) {
  fuzz_candidate_state *state = (fuzz_candidate_state *)user;
  (void)value;
  (void)error;
  ++state->event_count;
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_path_event(void *user,
                                       const lonejson_value_path *path,
                                       lonejson_error *error) {
  fuzz_candidate_state *state = (fuzz_candidate_state *)user;
  (void)path;
  (void)error;
  ++state->event_count;
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_path_chunk(void *user,
                                       const lonejson_value_path *path,
                                       const char *data, size_t len,
                                       lonejson_error *error) {
  fuzz_candidate_state *state = (fuzz_candidate_state *)user;
  (void)path;
  (void)data;
  (void)len;
  (void)error;
  ++state->event_count;
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_path_bool(void *user,
                                      const lonejson_value_path *path,
                                      int value, lonejson_error *error) {
  fuzz_candidate_state *state = (fuzz_candidate_state *)user;
  (void)path;
  (void)value;
  (void)error;
  ++state->event_count;
  return LONEJSON_STATUS_OK;
}

static lonejson_read_result fuzz_read(void *user, unsigned char *buffer,
                                      size_t capacity) {
  fuzz_candidate_reader *reader = (fuzz_candidate_reader *)user;
  lonejson_read_result result;
  size_t remaining;
  size_t take;

  memset(&result, 0, sizeof(result));
  remaining = reader->len - reader->offset;
  if (remaining == 0u) {
    result.eof = 1;
    return result;
  }
  take = reader->chunk_size;
  if (take > remaining) {
    take = remaining;
  }
  if (take > capacity) {
    take = capacity;
  }
  memcpy(buffer, reader->data + reader->offset, take);
  reader->offset += take;
  result.bytes_read = take;
  result.eof = reader->offset == reader->len ? 1 : 0;
  return result;
}

static void fuzz_configure_value_visitor(lonejson_value_visitor *visitor) {
  memset(visitor, 0, sizeof(*visitor));
  visitor->object_begin = fuzz_event;
  visitor->object_end = fuzz_event;
  visitor->object_key_begin = fuzz_event;
  visitor->object_key_chunk = fuzz_chunk;
  visitor->object_key_end = fuzz_event;
  visitor->array_begin = fuzz_event;
  visitor->array_end = fuzz_event;
  visitor->string_begin = fuzz_event;
  visitor->string_chunk = fuzz_chunk;
  visitor->string_end = fuzz_event;
  visitor->number_begin = fuzz_event;
  visitor->number_chunk = fuzz_chunk;
  visitor->number_end = fuzz_event;
  visitor->boolean_value = fuzz_bool;
  visitor->null_value = fuzz_event;
}

static void
fuzz_configure_path_visitor(lonejson_path_value_visitor *visitor) {
  memset(visitor, 0, sizeof(*visitor));
  visitor->object_begin = fuzz_path_event;
  visitor->object_end = fuzz_path_event;
  visitor->object_key_begin = fuzz_path_event;
  visitor->object_key_chunk = fuzz_path_chunk;
  visitor->object_key_end = fuzz_path_event;
  visitor->array_begin = fuzz_path_event;
  visitor->array_end = fuzz_path_event;
  visitor->string_begin = fuzz_path_event;
  visitor->string_chunk = fuzz_path_chunk;
  visitor->string_end = fuzz_path_event;
  visitor->number_begin = fuzz_path_event;
  visitor->number_chunk = fuzz_path_chunk;
  visitor->number_end = fuzz_path_event;
  visitor->boolean_value = fuzz_path_bool;
  visitor->null_value = fuzz_path_event;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  lonejson_config config;
  lonejson *runtime;
  lonejson_candidate_stream_options options;
  lonejson_value_visitor value_visitor;
  lonejson_path_value_visitor path_visitor;
  lonejson_error error;
  fuzz_candidate_state state;
  fuzz_candidate_reader reader;
  const uint8_t *json;
  size_t json_len;

  if (size < 3u) {
    return 0;
  }
  json = data + 2u;
  json_len = size - 2u;

  config = lonejson_default_config();
  config.json_value_max_depth = 1u + (size_t)(data[0] & 0x3fu);
  config.json_value_max_string_bytes = 1u + (size_t)data[0] * 64u;
  config.json_value_max_key_bytes = 1u + (size_t)data[0] * 16u;
  config.json_value_max_number_bytes = 1u + (size_t)(data[0] & 0x7fu);
  config.json_value_max_total_bytes = (data[0] & 0x80u) ? json_len : 0u;
  runtime = lonejson_new(&config, &error);
  if (runtime == NULL) {
    return 0;
  }

  memset(&state, 0, sizeof(state));
  state.stop_after_first = (unsigned)((data[1] & 0x40u) != 0u);
  state.fail_after_first = (unsigned)((data[1] & 0x80u) != 0u);
  options = lonejson_default_candidate_stream_options();
  options.framing = (lonejson_candidate_framing)(data[1] & 0x03u);
  options.candidate_begin = fuzz_candidate_begin;
  options.candidate_end = fuzz_candidate_end;
  options.candidate_user = &state;
  if ((data[1] & 0x10u) != 0u) {
    fuzz_configure_path_visitor(&path_visitor);
    options.path_visitor = &path_visitor;
  } else if ((data[1] & 0x20u) != 0u) {
    fuzz_configure_value_visitor(&value_visitor);
    options.visitor = &value_visitor;
  }
  options.visitor_user = &state;

  (void)lonejson_visit_candidates_buffer(runtime, json, json_len, &options,
                                         &error);

  memset(&state, 0, sizeof(state));
  state.stop_after_first = (unsigned)((data[1] & 0x40u) != 0u);
  state.fail_after_first = (unsigned)((data[1] & 0x80u) != 0u);
  reader.data = json;
  reader.len = json_len;
  reader.offset = 0u;
  reader.chunk_size = 1u + (size_t)(data[0] % 17u);
  options.candidate_user = &state;
  options.visitor_user = &state;
  (void)lonejson_visit_candidates_reader(runtime, fuzz_read, &reader, &options,
                                         &error);

  lonejson_free(runtime);
  return 0;
}
