#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lonejson.h"

typedef struct fuzz_visit_state {
  size_t object_count;
  size_t array_count;
  size_t key_bytes;
  size_t string_bytes;
  size_t number_bytes;
  size_t bool_count;
  size_t null_count;
  size_t callback_count;
} fuzz_visit_state;

static lonejson_status fuzz_visit_object_begin(void *user,
                                               lonejson_error *error) {
  fuzz_visit_state *state = (fuzz_visit_state *)user;
  (void)error;
  ++state->object_count;
  ++state->callback_count;
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_visit_array_begin(void *user,
                                              lonejson_error *error) {
  fuzz_visit_state *state = (fuzz_visit_state *)user;
  (void)error;
  ++state->array_count;
  ++state->callback_count;
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_visit_key_chunk(void *user, const char *data,
                                            size_t len, lonejson_error *error) {
  fuzz_visit_state *state = (fuzz_visit_state *)user;
  (void)data;
  (void)error;
  state->key_bytes += len;
  ++state->callback_count;
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_visit_string_chunk(void *user, const char *data,
                                               size_t len,
                                               lonejson_error *error) {
  fuzz_visit_state *state = (fuzz_visit_state *)user;
  (void)data;
  (void)error;
  state->string_bytes += len;
  ++state->callback_count;
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_visit_number_chunk(void *user, const char *data,
                                               size_t len,
                                               lonejson_error *error) {
  fuzz_visit_state *state = (fuzz_visit_state *)user;
  (void)data;
  (void)error;
  state->number_bytes += len;
  ++state->callback_count;
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_visit_bool(void *user, int value,
                                       lonejson_error *error) {
  fuzz_visit_state *state = (fuzz_visit_state *)user;
  (void)value;
  (void)error;
  ++state->bool_count;
  ++state->callback_count;
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_visit_null(void *user, lonejson_error *error) {
  fuzz_visit_state *state = (fuzz_visit_state *)user;
  (void)error;
  ++state->null_count;
  ++state->callback_count;
  return LONEJSON_STATUS_OK;
}

typedef struct fuzz_reader_state {
  const uint8_t *data;
  size_t len;
  size_t offset;
  size_t chunk_size;
} fuzz_reader_state;

static lonejson_read_result fuzz_reader(void *user, unsigned char *buffer,
                                        size_t capacity) {
  fuzz_reader_state *state = (fuzz_reader_state *)user;
  lonejson_read_result rr;
  size_t remaining;
  size_t take;

  memset(&rr, 0, sizeof(rr));
  remaining = state->len - state->offset;
  if (remaining == 0u) {
    rr.eof = 1;
    return rr;
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
  rr.bytes_read = take;
  rr.eof = state->offset == state->len ? 1 : 0;
  return rr;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  lonejson_value_visitor visitor;
  lonejson_value_limits limits;
  lonejson_error error;
  lonejson_status status;
  fuzz_visit_state state;
  fuzz_reader_state reader;
  char *cstr;

  if (size == 0u) {
    return 0;
  }

  memset(&visitor, 0, sizeof(visitor));
  visitor.object_begin = fuzz_visit_object_begin;
  visitor.array_begin = fuzz_visit_array_begin;
  visitor.object_key_chunk = fuzz_visit_key_chunk;
  visitor.string_chunk = fuzz_visit_string_chunk;
  visitor.number_chunk = fuzz_visit_number_chunk;
  visitor.boolean_value = fuzz_visit_bool;
  visitor.null_value = fuzz_visit_null;

  memset(&state, 0, sizeof(state));
  limits = lonejson_default_value_limits();
  limits.max_depth = 1u + (size_t)(data[0] & 0x3Fu);
  limits.max_string_bytes = 1u + (size_t)data[0] * 64u;
  limits.max_key_bytes = 1u + (size_t)data[0] * 16u;
  limits.max_number_bytes = 1u + (size_t)(data[0] & 0x7Fu);
  limits.max_total_bytes = (data[0] & 0x80u) ? size : 0u;
  status = lonejson_visit_value_buffer(data, size, &visitor, &state, &limits,
                                       &error);
  if (status == LONEJSON_STATUS_OK) {
    (void)state.callback_count;
  }

  memset(&state, 0, sizeof(state));
  reader.data = data;
  reader.len = size;
  reader.offset = 0u;
  reader.chunk_size = 1u + (size_t)(data[0] % 31u);
  status = lonejson_visit_value_reader(fuzz_reader, &reader, &visitor, &state,
                                       NULL, &error);
  if (status == LONEJSON_STATUS_OK) {
    (void)state.string_bytes;
  }

  cstr = (char *)malloc(size + 1u);
  if (cstr != NULL) {
    memcpy(cstr, data, size);
    cstr[size] = '\0';
    memset(&state, 0, sizeof(state));
    (void)lonejson_visit_value_cstr(cstr, &visitor, &state, NULL, &error);
    free(cstr);
  }

  return 0;
}
