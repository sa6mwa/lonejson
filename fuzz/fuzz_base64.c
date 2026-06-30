#include "lonejson.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct fuzz_sink {
  unsigned char *data;
  size_t len;
  size_t cap;
  int fail_after_first;
  int calls;
} fuzz_sink;

static lonejson_status fuzz_collect(void *user, const void *data, size_t len,
                                    lonejson_error *error) {
  fuzz_sink *sink = (fuzz_sink *)user;

  (void)error;
  ++sink->calls;
  if (sink->fail_after_first && sink->calls > 1) {
    return LONEJSON_STATUS_TRUNCATED;
  }
  if (len > sink->cap - sink->len) {
    return LONEJSON_STATUS_TRUNCATED;
  }
  memcpy(sink->data + sink->len, data, len);
  sink->len += len;
  return LONEJSON_STATUS_OK;
}

static void fuzz_variant(const uint8_t *data, size_t size,
                         lonejson_base64_variant variant) {
  lonejson_error error;
  char stack_encoded[256];
  unsigned char stack_decoded[256];
  char *encoded;
  unsigned char *decoded;
  size_t encoded_len = 0u;
  size_t decoded_len = 0u;
  size_t needed = 0u;
  fuzz_sink sink;

  lonejson_error_init(&error);
  (void)lonejson_base64_encoded_len(size, variant, &encoded_len, &error);
  (void)lonejson_base64_encode(data, size, variant, stack_encoded,
                               sizeof(stack_encoded), &needed, &error);
  if (encoded_len <= 65536u) {
    encoded = (char *)malloc(encoded_len == 0u ? 1u : encoded_len);
    decoded = (unsigned char *)malloc(size == 0u ? 1u : size);
    if (encoded != NULL && decoded != NULL &&
        lonejson_base64_encode(data, size, variant, encoded, encoded_len,
                               &needed, &error) == LONEJSON_STATUS_OK) {
      if (needed != encoded_len) {
        abort();
      }
      if (lonejson_base64_decoded_len(encoded, encoded_len, variant,
                                      &decoded_len,
                                      &error) != LONEJSON_STATUS_OK ||
          decoded_len != size) {
        abort();
      }
      if (lonejson_base64_decode(encoded, encoded_len, variant, decoded, size,
                                 &needed, &error) != LONEJSON_STATUS_OK ||
          needed != size || memcmp(decoded, data, size) != 0) {
        abort();
      }
      sink.data = decoded;
      sink.len = 0u;
      sink.cap = size;
      sink.fail_after_first = 0;
      sink.calls = 0;
      if (lonejson_base64_decode_sink(encoded, encoded_len, variant,
                                      fuzz_collect, &sink,
                                      &error) != LONEJSON_STATUS_OK ||
          sink.len != size || memcmp(sink.data, data, size) != 0) {
        abort();
      }
    }
    free(encoded);
    free(decoded);
  }

  (void)lonejson_base64_decoded_len((const char *)data, size, variant,
                                    &decoded_len, &error);
  (void)lonejson_base64_decode((const char *)data, size, variant, stack_decoded,
                               sizeof(stack_decoded), &needed, &error);
  sink.data = (unsigned char *)stack_encoded;
  sink.len = 0u;
  sink.cap = sizeof(stack_encoded);
  sink.fail_after_first = size > 8u;
  sink.calls = 0;
  (void)lonejson_base64_encode_sink(data, size, variant, fuzz_collect, &sink,
                                    &error);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size > 65536u) {
    return 0;
  }
  fuzz_variant(data, size, LONEJSON_BASE64_STANDARD);
  fuzz_variant(data, size, LONEJSON_BASE64_STANDARD_RAW);
  fuzz_variant(data, size, LONEJSON_BASE64_URL);
  fuzz_variant(data, size, LONEJSON_BASE64_URL_RAW);
  if (size <= 4096u) {
    size_t needed = 0u;
    unsigned char out[4096];
    lonejson_error error;

    lonejson_error_init(&error);
    (void)lonejson_base64url_decoded_len((const char *)data, size, &needed,
                                         &error);
    (void)lonejson_base64url_decode((const char *)data, size, out, sizeof(out),
                                    &needed, &error);
  }
  return 0;
}
