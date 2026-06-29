#define LONEJSON_WITH_OPENSSL 1
#define LONEJSON_WITH_JWT 1
#include "lonejson.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void fuzz_decode_segment(const char *data, size_t len) {
  lonejson_error error;
  unsigned char stack_buf[128];
  unsigned char *heap_buf;
  size_t needed;
  size_t decoded_len;

  lonejson_error_init(&error);
  decoded_len = 0u;
  if (lonejson_base64url_decoded_len(data, len, &decoded_len, &error) !=
      LONEJSON_STATUS_OK) {
    decoded_len = 0u;
  }
  (void)lonejson_base64url_decode(data, len, stack_buf, sizeof(stack_buf),
                                  &needed, &error);
  if (decoded_len <= 4096u) {
    heap_buf = (unsigned char *)malloc(decoded_len == 0u ? 1u : decoded_len);
    if (heap_buf != NULL) {
      (void)lonejson_base64url_decode(data, len, heap_buf, decoded_len, &needed,
                                      &error);
      free(heap_buf);
    }
  }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  lonejson_jwt_compact jwt;
  lonejson_error error;
  char *text;

  if (size > 65536u) {
    return 0;
  }
  text = (char *)malloc(size == 0u ? 1u : size);
  if (text == NULL) {
    return 0;
  }
  if (size != 0u) {
    memcpy(text, data, size);
  }

  fuzz_decode_segment(text, size);
  lonejson_error_init(&error);
  if (lonejson_jwt_parse_compact(text, size, &jwt, &error) ==
      LONEJSON_STATUS_OK) {
    fuzz_decode_segment(jwt.header.data, jwt.header.len);
    fuzz_decode_segment(jwt.payload.data, jwt.payload.len);
    fuzz_decode_segment(jwt.signature.data, jwt.signature.len);
    fuzz_decode_segment(jwt.signing_input.data, jwt.signing_input.len);
  }
  free(text);
  return 0;
}
