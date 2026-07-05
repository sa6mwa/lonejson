typedef struct test_base64_sink {
  unsigned char bytes[128];
  size_t len;
} test_base64_sink;

static lonejson_status test_base64_collect(void *user, const void *data,
                                           size_t len, lonejson_error *error) {
  test_base64_sink *sink = (test_base64_sink *)user;

  (void)error;
  EXPECT(sink->len + len <= sizeof(sink->bytes));
  memcpy(sink->bytes + sink->len, data, len);
  sink->len += len;
  return LONEJSON_STATUS_OK;
}

static void test_base64_encode_decode_variants(void) {
  static const unsigned char hello[] = "Hello?";
  struct vector {
    lonejson_base64_variant variant;
    const char *encoded;
  } vectors[] = {{LONEJSON_BASE64_STANDARD, "SGVsbG8/"},
                 {LONEJSON_BASE64_STANDARD_RAW, "SGVsbG8/"},
                 {LONEJSON_BASE64_URL, "SGVsbG8_"},
                 {LONEJSON_BASE64_URL_RAW, "SGVsbG8_"}};
  char out[32];
  unsigned char decoded[32];
  size_t needed;
  size_t out_len;
  size_t i;
  lonejson_error error;

  lonejson_error_init(&error);
  EXPECT(lonejson_base64_encoded_len(5u, LONEJSON_BASE64_STANDARD, &out_len,
                                     &error) == LONEJSON_STATUS_OK);
  EXPECT(out_len == 8u);
  EXPECT(lonejson_base64_encoded_len(5u, LONEJSON_BASE64_STANDARD_RAW, &out_len,
                                     &error) == LONEJSON_STATUS_OK);
  EXPECT(out_len == 7u);

  for (i = 0u; i < sizeof(vectors) / sizeof(vectors[0]); ++i) {
    memset(out, 0, sizeof(out));
    EXPECT(lonejson_base64_encode(hello, sizeof(hello) - 1u, vectors[i].variant,
                                  out, sizeof(out), &needed,
                                  &error) == LONEJSON_STATUS_OK);
    EXPECT(needed == strlen(vectors[i].encoded));
    EXPECT(memcmp(out, vectors[i].encoded, needed) == 0);
    memset(decoded, 0, sizeof(decoded));
    EXPECT(lonejson_base64_decode(vectors[i].encoded,
                                  strlen(vectors[i].encoded),
                                  vectors[i].variant, decoded, sizeof(decoded),
                                  &needed, &error) == LONEJSON_STATUS_OK);
    EXPECT(needed == sizeof(hello) - 1u);
    EXPECT(memcmp(decoded, hello, needed) == 0);
  }
}

static void test_base64_padding_and_failure_modes(void) {
  unsigned char decoded[8];
  char encoded[8];
  size_t needed = 777u;
  size_t out_len = 777u;
  lonejson_error error;

  lonejson_error_init(&error);
  EXPECT(lonejson_base64_encode("hi", 2u, LONEJSON_BASE64_STANDARD, encoded, 3u,
                                &needed, &error) == LONEJSON_STATUS_TRUNCATED);
  EXPECT(needed == 4u);
  EXPECT(lonejson_base64_decode("aGk=", 4u, LONEJSON_BASE64_STANDARD, decoded,
                                1u, &needed,
                                &error) == LONEJSON_STATUS_TRUNCATED);
  EXPECT(needed == 2u);
  EXPECT(lonejson_base64_decode("aGk=", 4u, LONEJSON_BASE64_STANDARD_RAW,
                                decoded, sizeof(decoded), &needed,
                                &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(needed == 0u);
  EXPECT(lonejson_base64_decode("aGk", 3u, LONEJSON_BASE64_STANDARD, decoded,
                                sizeof(decoded), &needed,
                                &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_base64_decode("aG=k", 4u, LONEJSON_BASE64_STANDARD, decoded,
                                sizeof(decoded), &needed,
                                &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_base64_decode("aGk+", 4u, LONEJSON_BASE64_URL, decoded,
                                sizeof(decoded), &needed,
                                &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_base64_decoded_len("A", 1u, LONEJSON_BASE64_URL_RAW, &out_len,
                                     &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_base64_encoded_len(1u, (lonejson_base64_variant)99, &out_len,
                                     &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_base64_encode("x", 1u, LONEJSON_BASE64_STANDARD, encoded,
                                sizeof(encoded), NULL,
                                &error) == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_base64_decode("AA==", 4u, LONEJSON_BASE64_STANDARD, NULL, 1u,
                                &needed,
                                &error) == LONEJSON_STATUS_INVALID_ARGUMENT);
}

static void test_base64_sink_equivalence_and_aliases(void) {
  static const unsigned char bytes[] = {0x00u, 0xffu, 0x10u, 0x20u, 0x30u};
  test_base64_sink encoded_sink;
  test_base64_sink decoded_sink;
  char encoded[32];
  unsigned char decoded[32];
  size_t needed;
  lonejson_error error;

  lonejson_error_init(&error);
  memset(&encoded_sink, 0, sizeof(encoded_sink));
  memset(&decoded_sink, 0, sizeof(decoded_sink));
  EXPECT(lj_base64_encode(bytes, sizeof(bytes), LJ_BASE64_URL_RAW, encoded,
                          sizeof(encoded), &needed,
                          &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_base64_encode_sink(
             bytes, sizeof(bytes), LONEJSON_BASE64_URL_RAW, test_base64_collect,
             &encoded_sink, &error) == LONEJSON_STATUS_OK);
  EXPECT(encoded_sink.len == needed);
  EXPECT(memcmp(encoded_sink.bytes, encoded, needed) == 0);

  EXPECT(lonejson_base64_decode_sink(encoded, needed, LONEJSON_BASE64_URL_RAW,
                                     test_base64_collect, &decoded_sink,
                                     &error) == LONEJSON_STATUS_OK);
  EXPECT(decoded_sink.len == sizeof(bytes));
  EXPECT(memcmp(decoded_sink.bytes, bytes, sizeof(bytes)) == 0);

  memset(decoded, 0, sizeof(decoded));
  EXPECT(lonejson_base64_decode(encoded, needed, LONEJSON_BASE64_URL_RAW,
                                decoded, sizeof(decoded), &needed,
                                &error) == LONEJSON_STATUS_OK);
  EXPECT(needed == sizeof(bytes));
  EXPECT(memcmp(decoded, bytes, sizeof(bytes)) == 0);
}
