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
  lonejson_jwk jwk;
  lonejson_jwks jwks;
  lonejson_error error;
  lonejson *runtime;
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
  runtime = lonejson_new(NULL, &error);
  if (runtime != NULL) {
    lonejson_jwk_init(&jwk);
    if (lonejson_jwk_parse_json(runtime, text, size, &jwk, &error) ==
        LONEJSON_STATUS_OK) {
      (void)jwk.kty;
    }
    lonejson_jwk_cleanup(&jwk);
    lonejson_jwks_init(&jwks);
    if (lonejson_jwks_parse_json(runtime, text, size, &jwks, &error) ==
        LONEJSON_STATUS_OK) {
      lonejson_jwk_select_options options;
      const lonejson_jwk *selected;
      memset(&options, 0, sizeof(options));
      (void)lonejson_jwks_select(&jwks, &options, &selected, &error);
      if (jwks.keys.count != 0u) {
        const lonejson_jwk *first = (const lonejson_jwk *)jwks.keys.items;
        options.kid = first->kid;
        options.kty = first->kty;
        options.alg = first->alg;
        options.use = first->use;
        (void)lonejson_jwks_select(&jwks, &options, &selected, &error);
      }
    }
    lonejson_jwks_cleanup(&jwks);
    lonejson_free(runtime);
  }
  free(text);
  return 0;
}
