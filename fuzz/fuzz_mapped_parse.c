#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lonejson.h"

typedef struct fuzz_address {
  char city[16];
  lonejson_int64 zip;
} fuzz_address;

typedef struct fuzz_person {
  char *name;
  char nickname[8];
  lonejson_int64 age;
  bool active;
  fuzz_address address;
  lonejson_i64_array lucky_numbers;
  lonejson_string_array tags;
} fuzz_person;

typedef struct fuzz_stream_envelope {
  lonejson_string_array_stream keys;
  char cursor[16];
  lonejson_uint64 index_seq;
  lonejson_json_value metadata;
} fuzz_stream_envelope;

typedef struct fuzz_stream_state {
  size_t begins;
  size_t chunks;
  size_t ends;
  size_t fail_after_chunks;
} fuzz_stream_state;

static const lonejson_field fuzz_address_fields[] = {
    LONEJSON_FIELD_STRING_FIXED(fuzz_address, city, "city",
                                LONEJSON_OVERFLOW_TRUNCATE),
    LONEJSON_FIELD_I64(fuzz_address, zip, "zip")};
LONEJSON_MAP_DEFINE(fuzz_address_map, fuzz_address, fuzz_address_fields);

static const lonejson_field fuzz_person_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC(fuzz_person, name, "name"),
    LONEJSON_FIELD_STRING_FIXED(fuzz_person, nickname, "nickname",
                                LONEJSON_OVERFLOW_TRUNCATE),
    LONEJSON_FIELD_I64(fuzz_person, age, "age"),
    LONEJSON_FIELD_BOOL(fuzz_person, active, "active"),
    LONEJSON_FIELD_OBJECT(fuzz_person, address, "address", &fuzz_address_map),
    LONEJSON_FIELD_I64_ARRAY(fuzz_person, lucky_numbers, "lucky_numbers",
                             LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_ARRAY(fuzz_person, tags, "tags",
                                LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(fuzz_person_map, fuzz_person, fuzz_person_fields);

static const lonejson_field fuzz_stream_envelope_fields[] = {
    LONEJSON_FIELD_STRING_ARRAY_STREAM_REQ(fuzz_stream_envelope, keys, "keys"),
    LONEJSON_FIELD_STRING_FIXED(fuzz_stream_envelope, cursor, "cursor",
                                LONEJSON_OVERFLOW_TRUNCATE),
    LONEJSON_FIELD_U64(fuzz_stream_envelope, index_seq, "index_seq"),
    LONEJSON_FIELD_JSON_VALUE(fuzz_stream_envelope, metadata, "metadata")};
LONEJSON_MAP_DEFINE(fuzz_stream_envelope_map, fuzz_stream_envelope,
                    fuzz_stream_envelope_fields);

static lonejson_status fuzz_stream_begin(void *user, lonejson_error *error) {
  fuzz_stream_state *state = (fuzz_stream_state *)user;
  (void)error;
  state->begins++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_stream_chunk(void *user, const char *data,
                                         size_t len, lonejson_error *error) {
  fuzz_stream_state *state = (fuzz_stream_state *)user;
  (void)data;
  (void)len;
  (void)error;
  state->chunks++;
  if (state->fail_after_chunks != 0u &&
      state->chunks >= state->fail_after_chunks) {
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_stream_end(void *user, lonejson_error *error) {
  fuzz_stream_state *state = (fuzz_stream_state *)user;
  (void)error;
  state->ends++;
  return LONEJSON_STATUS_OK;
}

static void fuzz_parse_stream_envelope(const uint8_t *data, size_t size) {
  fuzz_stream_envelope envelope;
  fuzz_stream_state state;
  lonejson_array_stream_string_handler handler;
  lonejson_parse_options options;
  lonejson_error error;

  memset(&envelope, 0, sizeof(envelope));
  memset(&state, 0, sizeof(state));
  memset(&handler, 0, sizeof(handler));
  options = lonejson_default_parse_options();
  options.clear_destination = 0;
  if (size > 0u) {
    options.reject_duplicate_keys = (data[0] & 1u) ? 1 : 0;
    state.fail_after_chunks = (data[0] & 8u) ? ((data[0] & 3u) + 1u) : 0u;
  }
  handler.begin = fuzz_stream_begin;
  handler.chunk = fuzz_stream_chunk;
  handler.end = fuzz_stream_end;

  lonejson_init(&fuzz_stream_envelope_map, &envelope);
  (void)lonejson_string_array_stream_set_handler(&envelope.keys, &handler,
                                                 &state, &error);
  (void)lonejson_json_value_enable_parse_capture(&envelope.metadata, &error);
  (void)lonejson_parse_buffer(&fuzz_stream_envelope_map, &envelope, data, size,
                              &options, &error);
  lonejson_cleanup(&fuzz_stream_envelope_map, &envelope);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  fuzz_person person;
  lonejson_error error;
  lonejson_status status;

  fuzz_parse_stream_envelope(data, size);

  memset(&person, 0, sizeof(person));
  status = lonejson_parse_buffer(&fuzz_person_map, &person, data, size, NULL,
                                 &error);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    char buffer[1024];
    size_t needed;
    (void)lonejson_serialize_buffer(&fuzz_person_map, &person, buffer,
                                    sizeof(buffer), &needed, NULL, &error);
  }
  lonejson_cleanup(&fuzz_person_map, &person);
  return 0;
}
