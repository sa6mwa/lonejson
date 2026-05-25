#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lonejson.h"

typedef struct fuzz_writer_doc {
  char payload[256];
} fuzz_writer_doc;

typedef struct fuzz_writer_ctx {
  int step;
  const char *key;
  size_t key_len;
  const char *string_value;
  size_t string_len;
  const char *number_value;
  size_t number_len;
  lonejson_int64 i64_value;
  lonejson_uint64 u64_value;
  double f64_value;
  lonejson_json_value json_value;
  fuzz_writer_doc doc;
} fuzz_writer_ctx;

typedef struct fuzz_writer_buf {
  unsigned char *data;
  size_t len;
  size_t cap;
} fuzz_writer_buf;

typedef struct fuzz_writer_allocator {
  size_t max_size;
  size_t observed_max_size;
} fuzz_writer_allocator;

static const lonejson_field fuzz_writer_doc_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(fuzz_writer_doc, payload, "payload",
                                    LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(fuzz_writer_doc_map, fuzz_writer_doc,
                    fuzz_writer_doc_fields);

static lonejson_status fuzz_writer_sink(void *user, const void *data,
                                        size_t len, lonejson_error *error) {
  fuzz_writer_buf *buf = (fuzz_writer_buf *)user;
  unsigned char *next;

  (void)error;
  if (len == 0u) {
    return LONEJSON_STATUS_OK;
  }
  if (buf->len + len + 1u < buf->len) {
    return LONEJSON_STATUS_OVERFLOW;
  }
  if (buf->len + len + 1u > buf->cap) {
    size_t next_cap = buf->cap == 0u ? 256u : buf->cap * 2u;
    while (next_cap < buf->len + len + 1u) {
      if (next_cap > ((size_t)-1) / 2u) {
        return LONEJSON_STATUS_OVERFLOW;
      }
      next_cap *= 2u;
    }
    next = (unsigned char *)realloc(buf->data, next_cap);
    if (next == NULL) {
      return LONEJSON_STATUS_ALLOCATION_FAILED;
    }
    buf->data = next;
    buf->cap = next_cap;
  }
  memcpy(buf->data + buf->len, data, len);
  buf->len += len;
  buf->data[buf->len] = '\0';
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_writer_producer(lonejson_writer *writer, void *user,
                                            lonejson_error *error) {
  fuzz_writer_ctx *ctx = (fuzz_writer_ctx *)user;
  lonejson_status status;

  for (;;) {
    switch (ctx->step) {
    case 0:
      status = lonejson_writer_begin_object(writer, error);
      break;
    case 1:
      status = lonejson_writer_key(writer, ctx->key, ctx->key_len, error);
      break;
    case 2:
      status = lonejson_writer_string(writer, ctx->string_value,
                                      ctx->string_len, error);
      break;
    case 3:
      status = lonejson_writer_key(writer, "n", 1u, error);
      break;
    case 4:
      status = lonejson_writer_number_text(writer, ctx->number_value,
                                           ctx->number_len, error);
      break;
    case 5:
      status = lonejson_writer_key(writer, "i", 1u, error);
      break;
    case 6:
      status = lonejson_writer_i64(writer, ctx->i64_value, error);
      break;
    case 7:
      status = lonejson_writer_key(writer, "u", 1u, error);
      break;
    case 8:
      status = lonejson_writer_u64(writer, ctx->u64_value, error);
      break;
    case 9:
      status = lonejson_writer_key(writer, "f", 1u, error);
      break;
    case 10:
      status = lonejson_writer_f64(writer, ctx->f64_value, error);
      break;
    case 11:
      status = lonejson_writer_key(writer, "raw", 3u, error);
      break;
    case 12:
      status = lonejson_writer_json_value(writer, &ctx->json_value, error);
      break;
    case 13:
      status = lonejson_writer_key(writer, "mapped", 6u, error);
      break;
    case 14:
      status = lonejson_writer_mapped(writer, &fuzz_writer_doc_map, &ctx->doc,
                                      error);
      break;
    case 15:
      status = lonejson_writer_key(writer, "arr", 3u, error);
      break;
    case 16:
      status = lonejson_writer_begin_array(writer, error);
      break;
    case 17:
      status = lonejson_writer_bool(writer, 1, error);
      break;
    case 18:
      status = lonejson_writer_null(writer, error);
      break;
    case 19:
      status = lonejson_writer_i64(writer, ctx->i64_value, error);
      break;
    case 20:
      status = lonejson_writer_u64(writer, ctx->u64_value, error);
      break;
    case 21:
      status = lonejson_writer_f64(writer, ctx->f64_value, error);
      break;
    case 22:
      status = lonejson_writer_end_array(writer, error);
      break;
    case 23:
      status = lonejson_writer_end_object(writer, error);
      break;
    default:
      return LONEJSON_STATUS_OK;
    }
    if (status == LONEJSON_STATUS_TRUNCATED) {
      return status;
    }
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    ctx->step++;
  }
}

static void fuzz_fill_ascii(char *dst, size_t dst_cap, const uint8_t *data,
                            size_t size, size_t offset, size_t *out_len) {
  size_t i;
  size_t len;

  if (dst_cap == 0u) {
    *out_len = 0u;
    return;
  }
  len = size > offset ? (size_t)(data[offset] % (dst_cap - 1u)) : 0u;
  if (len > size - offset) {
    len = size - offset;
  }
  for (i = 0u; i < len; i++) {
    dst[i] = (char)('a' + (data[offset + i] % 26u));
  }
  dst[len] = '\0';
  *out_len = len;
}

static int fuzz_escape_json_string(char *dst, size_t dst_cap, const char *src,
                                   size_t src_len, size_t *out_len) {
  size_t i;
  size_t len = 0u;

  if (dst_cap < 3u) {
    return 0;
  }
  dst[len++] = '"';
  for (i = 0u; i < src_len; i++) {
    if (len + 3u > dst_cap) {
      return 0;
    }
    if (src[i] == '"' || src[i] == '\\') {
      dst[len++] = '\\';
    }
    dst[len++] = src[i];
  }
  dst[len++] = '"';
  dst[len] = '\0';
  *out_len = len;
  return 1;
}

static lonejson_uint64 fuzz_read_u64(const uint8_t *data, size_t size,
                                     size_t offset) {
  lonejson_uint64 value = 0u;
  size_t i;

  for (i = 0u; i < 8u; i++) {
    value <<= 8u;
    if (offset + i < size) {
      value |= (lonejson_uint64)data[offset + i];
    }
  }
  return value;
}

static void *fuzz_writer_malloc(void *user, size_t size) {
  fuzz_writer_allocator *state = (fuzz_writer_allocator *)user;

  if (state != NULL && size > state->observed_max_size) {
    state->observed_max_size = size;
  }
  if (state != NULL && size > state->max_size) {
    return NULL;
  }
  return malloc(size);
}

static void *fuzz_writer_realloc(void *user, void *ptr, size_t size) {
  fuzz_writer_allocator *state = (fuzz_writer_allocator *)user;

  if (state != NULL && size > state->observed_max_size) {
    state->observed_max_size = size;
  }
  if (state != NULL && size > state->max_size) {
    return NULL;
  }
  return realloc(ptr, size);
}

static void fuzz_writer_free(void *user, void *ptr) {
  (void)user;
  free(ptr);
}

static int fuzz_render_with_sink(fuzz_writer_ctx *ctx, fuzz_writer_buf *out) {
  lonejson *runtime;
  lonejson_writer writer;
  lonejson_error error;
  lonejson_status status;

  ctx->step = 0;
  memset(out, 0, sizeof(*out));
  runtime = lonejson_new(NULL, &error);
  if (runtime == NULL) {
    return 0;
  }
  status = lonejson_writer_init_sink(runtime, &writer, fuzz_writer_sink, out,
                                     &error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_free(runtime);
    return 0;
  }
  status = fuzz_writer_producer(&writer, ctx, &error);
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_writer_finish(&writer, &error);
  }
  lonejson_writer_cleanup(&writer);
  lonejson_free(runtime);
  return status == LONEJSON_STATUS_OK;
}

static int fuzz_render_with_generator(fuzz_writer_ctx *ctx, size_t first_cap,
                                      fuzz_writer_buf *out) {
  lonejson_config config;
  lonejson *runtime;
  lonejson_generator generator;
  lonejson_error error;
  fuzz_writer_allocator allocator_state;
  lonejson_allocator allocator;
  unsigned char chunk[32];
  size_t out_len;
  size_t reads;
  int eof;
  lonejson_status status;

  ctx->step = 0;
  memset(out, 0, sizeof(*out));
  memset(&allocator_state, 0, sizeof(allocator_state));
  allocator_state.max_size = 65536u;
  allocator = lonejson_default_allocator();
  allocator.malloc_fn = fuzz_writer_malloc;
  allocator.realloc_fn = fuzz_writer_realloc;
  allocator.free_fn = fuzz_writer_free;
  allocator.ctx = &allocator_state;
  config = lonejson_default_config();
  config.allocator = &allocator;
  runtime = lonejson_new(&config, &error);
  if (runtime == NULL) {
    return 0;
  }
  status = lonejson_writer_generator_init(runtime, &generator,
                                          fuzz_writer_producer, ctx);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_free(runtime);
    return 0;
  }
  eof = 0;
  for (reads = 0u; reads < 65536u && !eof; reads++) {
    size_t cap = (reads == 0u) ? 0u : (1u + ((first_cap + reads * 7u) % 31u));
    status = lonejson_generator_read(&generator, chunk, cap, &out_len, &eof);
    if (status != LONEJSON_STATUS_OK) {
      lonejson_generator_cleanup(&generator);
      lonejson_free(runtime);
      return 0;
    }
    if (out_len != 0u &&
        fuzz_writer_sink(out, chunk, out_len, &error) != LONEJSON_STATUS_OK) {
      lonejson_generator_cleanup(&generator);
      lonejson_free(runtime);
      return 0;
    }
  }
  lonejson_generator_cleanup(&generator);
  lonejson_free(runtime);
  return eof != 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  lonejson *runtime;
  lonejson_error error;
  fuzz_writer_ctx sink_ctx;
  fuzz_writer_ctx generator_ctx;
  fuzz_writer_buf sink_out;
  fuzz_writer_buf generator_out;
  char key[32];
  char str[128];
  char raw_json[260];
  size_t key_len;
  size_t str_len;
  size_t raw_len;
  int ok1;
  int ok2;

  if (size < 4u) {
    return 0;
  }
  memset(&sink_ctx, 0, sizeof(sink_ctx));
  memset(&generator_ctx, 0, sizeof(generator_ctx));
  runtime = lonejson_new(NULL, &error);
  if (runtime == NULL) {
    return 0;
  }
  lonejson_json_value_init(runtime, &sink_ctx.json_value);
  lonejson_json_value_init(runtime, &generator_ctx.json_value);
  fuzz_fill_ascii(key, sizeof(key), data, size, 1u, &key_len);
  fuzz_fill_ascii(str, sizeof(str), data, size, 2u, &str_len);
  fuzz_fill_ascii(sink_ctx.doc.payload, sizeof(sink_ctx.doc.payload), data,
                  size, 3u, &raw_len);
  memcpy(generator_ctx.doc.payload, sink_ctx.doc.payload,
         sizeof(generator_ctx.doc.payload));
  if (!fuzz_escape_json_string(raw_json, sizeof(raw_json), str, str_len,
                               &raw_len)) {
    lonejson_free(runtime);
    return 0;
  }
  sink_ctx.key = key;
  sink_ctx.key_len = key_len;
  sink_ctx.string_value = str;
  sink_ctx.string_len = str_len;
  sink_ctx.number_value = "-12.5e+2";
  sink_ctx.number_len = 8u;
  if ((data[0] & 3u) == 0u) {
    sink_ctx.i64_value = -12;
  } else if ((data[0] & 3u) == 1u) {
    sink_ctx.i64_value = -1234567890;
  } else if ((data[0] & 3u) == 2u) {
    sink_ctx.i64_value = (lonejson_int64)(fuzz_read_u64(data, size, 4u) &
                                          (((lonejson_uint64)1u << 62u) - 1u));
  } else {
    sink_ctx.i64_value = -(lonejson_int64)(fuzz_read_u64(data, size, 4u) &
                                           (((lonejson_uint64)1u << 62u) - 1u));
  }
  sink_ctx.u64_value = fuzz_read_u64(data, size, 12u);
  if ((data[0] & 12u) == 0u) {
    sink_ctx.f64_value = -12.5;
  } else if ((data[0] & 12u) == 4u) {
    sink_ctx.f64_value = 0.125;
  } else if ((data[0] & 12u) == 8u) {
    sink_ctx.f64_value = 1.0e30;
  } else {
    sink_ctx.f64_value = -1.0e-30;
  }
  generator_ctx = sink_ctx;
  lonejson_json_value_init(runtime, &sink_ctx.json_value);
  lonejson_json_value_init(runtime, &generator_ctx.json_value);
  if (lonejson_json_value_set_buffer(&sink_ctx.json_value, raw_json, raw_len,
                                     NULL) != LONEJSON_STATUS_OK ||
      lonejson_json_value_set_buffer(&generator_ctx.json_value, raw_json,
                                     raw_len, NULL) != LONEJSON_STATUS_OK) {
    lonejson_json_value_cleanup(&sink_ctx.json_value);
    lonejson_json_value_cleanup(&generator_ctx.json_value);
    lonejson_free(runtime);
    return 0;
  }

  ok1 = fuzz_render_with_sink(&sink_ctx, &sink_out);
  ok2 = fuzz_render_with_generator(&generator_ctx, data[0], &generator_out);
  if (ok1 != ok2 ||
      (ok1 && (sink_out.len != generator_out.len ||
               memcmp(sink_out.data, generator_out.data, sink_out.len) != 0))) {
    abort();
  }
  if (ok1 && (lonejson_validate_buffer(runtime, sink_out.data, sink_out.len, NULL) !=
                  LONEJSON_STATUS_OK ||
              lonejson_validate_buffer(runtime, generator_out.data, generator_out.len,
                                       NULL) != LONEJSON_STATUS_OK)) {
    abort();
  }
  free(sink_out.data);
  free(generator_out.data);
  lonejson_json_value_cleanup(&sink_ctx.json_value);
  lonejson_json_value_cleanup(&generator_ctx.json_value);
  lonejson_free(runtime);
  return 0;
}
