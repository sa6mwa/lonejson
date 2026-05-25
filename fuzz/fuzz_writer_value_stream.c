#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lonejson.h"

typedef struct fuzz_writer_value_sink {
  unsigned char data[4096];
  size_t len;
  size_t fail_after;
} fuzz_writer_value_sink;

typedef struct fuzz_writer_value_reader {
  const uint8_t *data;
  size_t len;
  size_t off;
  size_t chunk_size;
  int fail;
} fuzz_writer_value_reader;

typedef struct fuzz_spooled_state {
  lonejson_spooled value;
  size_t offset;
  int initialized;
} fuzz_spooled_state;

static lonejson_status fuzz_writer_value_sink_write(void *user,
                                                    const void *data,
                                                    size_t len,
                                                    lonejson_error *error) {
  fuzz_writer_value_sink *sink = (fuzz_writer_value_sink *)user;

  (void)error;
  if (sink->fail_after != 0u && sink->len + len >= sink->fail_after) {
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  if (sink->len + len >= sizeof(sink->data)) {
    return LONEJSON_STATUS_TRUNCATED;
  }
  memcpy(sink->data + sink->len, data, len);
  sink->len += len;
  sink->data[sink->len] = '\0';
  return LONEJSON_STATUS_OK;
}

static void fuzz_push_chunks(lonejson_writer_value_stream *stream,
                             const uint8_t *data, size_t size,
                             size_t chunk_size) {
  lonejson_error error;
  size_t off = 0u;

  while (off < size) {
    size_t take = size - off;
    lonejson_status status;

    if (take > chunk_size) {
      take = chunk_size;
    }
    status =
        lonejson_writer_value_stream_push(stream, data + off, take, &error);
    if (status != LONEJSON_STATUS_OK) {
      return;
    }
    off += take;
  }
  (void)lonejson_writer_value_stream_close(stream, &error);
}

static lonejson_read_result
fuzz_writer_value_read(void *user, unsigned char *buffer, size_t capacity) {
  fuzz_writer_value_reader *reader = (fuzz_writer_value_reader *)user;
  lonejson_read_result result;
  size_t take;

  memset(&result, 0, sizeof(result));
  if (reader->fail && reader->off >= reader->len / 2u) {
    result.error_code = EIO;
    return result;
  }
  if (reader->off >= reader->len) {
    result.eof = 1;
    return result;
  }
  take = reader->len - reader->off;
  if (take > reader->chunk_size) {
    take = reader->chunk_size;
  }
  if (take > capacity) {
    take = capacity;
  }
  memcpy(buffer, reader->data + reader->off, take);
  reader->off += take;
  result.bytes_read = take;
  result.eof = reader->off == reader->len;
  return result;
}

static int fuzz_spooled_prepare(lonejson *runtime, fuzz_spooled_state *state,
                                const uint8_t *data, size_t size,
                                uint8_t control) {
  lonejson_error error;
  unsigned char scratch[8];
  size_t pre;
  size_t consumed = 0u;

  memset(state, 0, sizeof(*state));
  lonejson_spooled_init(runtime, &state->value);
  state->initialized = 1;
  if (lonejson_spooled_append(&state->value, data, size, &error) !=
      LONEJSON_STATUS_OK) {
    return 0;
  }
  if (!lonejson_spooled_spilled(&state->value)) {
    return 1;
  }
  pre = size == 0u
            ? 0u
            : 1u + (size_t)(control %
                            (uint8_t)(size < 17u ? size : 17u));
  while (consumed < pre) {
    size_t want = pre - consumed;
    lonejson_read_result chunk;

    if (want > sizeof(scratch)) {
      want = sizeof(scratch);
    }
    chunk = lonejson_spooled_read(&state->value, scratch, want);
    if (chunk.error_code != 0) {
      abort();
    }
    if (chunk.bytes_read == 0u) {
      break;
    }
    if (memcmp(scratch, data + consumed, chunk.bytes_read) != 0) {
      abort();
    }
    consumed += chunk.bytes_read;
  }
  state->offset = consumed;
  return 1;
}

static void fuzz_spooled_check_and_cleanup(fuzz_spooled_state *state,
                                           const uint8_t *data, size_t size) {
  unsigned char scratch[8];
  size_t consumed = 0u;

  if (!state->initialized) {
    return;
  }
  while (state->offset + consumed < size && consumed < sizeof(scratch)) {
    lonejson_read_result chunk = lonejson_spooled_read(
        &state->value, scratch + consumed, sizeof(scratch) - consumed);
    if (chunk.error_code != 0) {
      abort();
    }
    if (chunk.bytes_read == 0u) {
      break;
    }
    consumed += chunk.bytes_read;
  }
  if (consumed != 0u &&
      memcmp(scratch, data + state->offset, consumed) != 0) {
    abort();
  }
  if (state->offset + consumed < size && consumed == 0u) {
    abort();
  }
  lonejson_spooled_cleanup(&state->value);
  state->initialized = 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  lonejson_config config;
  lonejson *runtime;
  lonejson_writer writer;
  lonejson_writer_value_stream stream;
  lonejson_error error;
  lonejson_status status;
  fuzz_writer_value_sink sink;
  fuzz_writer_value_reader reader;
  fuzz_spooled_state spool;
  size_t control;
  size_t chunk_size;
  int array_items_mode;

  if (size == 0u) {
    return 0;
  }
  control = data[0];
  data++;
  size--;

  memset(&sink, 0, sizeof(sink));
  memset(&spool, 0, sizeof(spool));
  if ((control & 0x40u) != 0u) {
    sink.fail_after = 1u + (size_t)(control & 0x1Fu);
  }
  config = lonejson_default_config();
  config.json_value_max_depth = 1u + (size_t)(control & 0x0Fu);
  config.json_value_max_string_bytes = 1u + (size_t)(control * 8u);
  config.json_value_max_key_bytes = 1u + (size_t)(control * 4u);
  config.json_value_max_number_bytes = 1u + (size_t)(control & 0x3Fu);
  config.json_value_max_total_bytes =
      (control & 0x80u) != 0u ? 1u + size / 2u : 0u;
  config.spool_default.memory_limit = (size_t)(control & 7u);
  runtime = lonejson_new(&config, &error);
  if (runtime == NULL) {
    return 0;
  }
  chunk_size = 1u + (size_t)(control % 17u);
  array_items_mode = (control & 0x30u) == 0x30u;

  status = lonejson_writer_init_sink(runtime, &writer,
                                     fuzz_writer_value_sink_write, &sink,
                                     &error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_free(runtime);
    return 0;
  }
  if (array_items_mode) {
    status = lonejson_writer_begin_array(&writer, &error);
    if (status == LONEJSON_STATUS_OK && (control & 1u) != 0u) {
      status = lonejson_writer_bool(&writer, 1, &error);
    }
  } else if ((control & 1u) != 0u) {
    status = lonejson_writer_begin_array(&writer, &error);
    if (status == LONEJSON_STATUS_OK) {
      status = lonejson_writer_bool(&writer, 1, &error);
    }
  }
  if (status == LONEJSON_STATUS_OK) {
    if (array_items_mode) {
      const char *selector = (control & 8u) != 0u ? "items" : NULL;
      if ((control & 0x80u) != 0u &&
          fuzz_spooled_prepare(runtime, &spool, data, size, control)) {
        status = lonejson_writer_array_items_spooled(
            &writer, selector, &spool.value, &error);
      } else if ((control & 4u) != 0u) {
        memset(&reader, 0, sizeof(reader));
        reader.data = data;
        reader.len = size;
        reader.chunk_size = chunk_size;
        reader.fail = (control & 2u) != 0u;
        status = lonejson_writer_array_items_reader(
            &writer, selector, fuzz_writer_value_read, &reader, &error);
      } else {
        status = lonejson_writer_array_items_buffer(&writer, selector, data,
                                                    size, &error);
      }
    } else if ((control & 0x30u) == 0x10u) {
      status = lonejson_writer_json_value_buffer(&writer, data, size, &error);
    } else if ((control & 0x30u) == 0x20u) {
      if ((control & 0x80u) != 0u &&
          fuzz_spooled_prepare(runtime, &spool, data, size, control)) {
        status = lonejson_writer_json_value_spooled(&writer, &spool.value,
                                                    &error);
      } else {
        memset(&reader, 0, sizeof(reader));
        reader.data = data;
        reader.len = size;
        reader.chunk_size = chunk_size;
        reader.fail = (control & 4u) != 0u;
        status = lonejson_writer_json_value_reader(
            &writer, fuzz_writer_value_read, &reader, &error);
      }
    } else {
      status = lonejson_writer_value_stream_open(&stream, &writer, &error);
      if (status == LONEJSON_STATUS_OK) {
        if ((control & 2u) != 0u) {
          (void)lonejson_writer_null(&writer, &error);
        }
        fuzz_push_chunks(&stream, data, size, chunk_size);
        lonejson_writer_value_stream_cleanup(&stream);
      }
    }
  }
  if (array_items_mode && status == LONEJSON_STATUS_OK) {
    (void)lonejson_writer_i64(&writer, -5, &error);
    (void)lonejson_writer_end_array(&writer, &error);
  } else if ((control & 1u) != 0u && status == LONEJSON_STATUS_OK) {
    (void)lonejson_writer_i64(&writer, -5, &error);
    (void)lonejson_writer_end_array(&writer, &error);
  }
  status = lonejson_writer_finish(&writer, &error);
  if (status == LONEJSON_STATUS_OK) {
    (void)lonejson_validate_buffer(runtime, sink.data, sink.len, &error);
  }
  fuzz_spooled_check_and_cleanup(&spool, data, size);
  lonejson_writer_cleanup(&writer);
  lonejson_free(runtime);
  return 0;
}
