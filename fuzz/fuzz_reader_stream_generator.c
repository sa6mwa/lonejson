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

typedef struct fuzz_reader_state {
  const uint8_t *data;
  size_t len;
  size_t offset;
  size_t chunk_size;
} fuzz_reader_state;

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

static void fuzz_parse_reader_path(const uint8_t *data, size_t size,
                                   size_t chunk_size) {
  fuzz_reader_state reader;
  fuzz_person person;
  lonejson_error error;

  memset(&person, 0, sizeof(person));
  reader.data = data;
  reader.len = size;
  reader.offset = 0u;
  reader.chunk_size = chunk_size;
  (void)lonejson_parse_reader(&fuzz_person_map, &person, fuzz_reader, &reader,
                              NULL, &error);
  lonejson_cleanup(&fuzz_person_map, &person);
}

static void fuzz_stream_path(const uint8_t *data, size_t size,
                             size_t chunk_size) {
  fuzz_reader_state reader;
  lonejson_stream *stream;
  lonejson_error error;
  size_t i;

  reader.data = data;
  reader.len = size;
  reader.offset = 0u;
  reader.chunk_size = chunk_size;
  stream = lonejson_stream_open_reader(&fuzz_person_map, fuzz_reader, &reader,
                                       NULL, &error);
  if (stream == NULL) {
    return;
  }
  for (i = 0u; i < 8u; ++i) {
    fuzz_person person;
    lonejson_stream_result result;

    memset(&person, 0, sizeof(person));
    result = lonejson_stream_next(stream, &person, &error);
    if (result == LONEJSON_STREAM_OBJECT) {
      lonejson_cleanup(&fuzz_person_map, &person);
      continue;
    }
    lonejson_cleanup(&fuzz_person_map, &person);
    break;
  }
  lonejson_stream_close(stream);
}

static void fuzz_generator_path(const fuzz_person *person, size_t chunk_size) {
  lonejson_generator generator;
  unsigned char buffer[32];
  size_t out_len;
  int eof;
  size_t i;

  memset(&generator, 0, sizeof(generator));
  if (lonejson_generator_init(&generator, &fuzz_person_map, person, NULL) !=
      LONEJSON_STATUS_OK) {
    return;
  }
  (void)lonejson_generator_measure(&fuzz_person_map, person, &out_len, NULL,
                                   NULL);
  eof = 0;
  for (i = 0u; i < 4096u && !eof; ++i) {
    size_t capacity = chunk_size;
    if (capacity > sizeof(buffer)) {
      capacity = sizeof(buffer);
    }
    if (lonejson_generator_read(&generator, buffer, capacity, &out_len, &eof) !=
        LONEJSON_STATUS_OK) {
      break;
    }
  }
  lonejson_generator_cleanup(&generator);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  fuzz_person person;
  lonejson_error error;
  lonejson_status status;
  size_t chunk_size;

  if (size == 0u) {
    return 0;
  }

  chunk_size = 1u + (size_t)(data[0] % 31u);
  fuzz_parse_reader_path(data, size, chunk_size);
  fuzz_stream_path(data, size, chunk_size);

  memset(&person, 0, sizeof(person));
  status = lonejson_parse_buffer(&fuzz_person_map, &person, data, size, NULL,
                                 &error);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    fuzz_generator_path(&person, chunk_size);
  }
  lonejson_cleanup(&fuzz_person_map, &person);
  return 0;
}
