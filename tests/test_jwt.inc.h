#ifdef LONEJSON_WITH_JWT
static void test_jwt_base64url_decode_vectors(void) {
  lonejson_error error;
  unsigned char out[8];
  size_t needed;
  size_t decoded_len;

  lonejson_error_init(&error);
  EXPECT(lonejson_base64url_decoded_len("", 0u, &decoded_len, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(decoded_len == 0u);

  memset(out, 0xaa, sizeof(out));
  EXPECT(lonejson_base64url_decode("SGVsbG8", 7u, out, sizeof(out), &needed,
                                   &error) == LONEJSON_STATUS_OK);
  EXPECT(needed == 5u);
  EXPECT(memcmp(out, "Hello", 5u) == 0);

  memset(out, 0xaa, sizeof(out));
  EXPECT(lonejson_base64url_decode("_w", 2u, out, sizeof(out), &needed,
                                   &error) == LONEJSON_STATUS_OK);
  EXPECT(needed == 1u);
  EXPECT(out[0] == 0xffu);

  memset(out, 0xaa, sizeof(out));
  EXPECT(lj_base64url_decode("SGVsbG8", 7u, out, 4u, &needed, &error) ==
         LJ_STATUS_TRUNCATED);
  EXPECT(needed == 5u);
  EXPECT(out[0] == 0xaau);

  EXPECT(lonejson_base64url_decode("A", 1u, out, sizeof(out), &needed,
                                   &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_base64url_decode("AA=", 3u, out, sizeof(out), &needed,
                                   &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_base64url_decode("AA+/", 4u, out, sizeof(out), &needed,
                                   &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_base64url_decode("AA", 2u, NULL, 1u, &needed, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_base64url_decode("AA", 2u, out, sizeof(out), NULL,
                                   &error) == LONEJSON_STATUS_INVALID_ARGUMENT);
}

static void test_jwt_compact_parse_segments(void) {
  static const char token[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJzdWIiOiJzIn0."
      "c2ln";
  lonejson_jwt_compact jwt;
  lonejson_error error;

  lonejson_error_init(&error);
  EXPECT(lonejson_jwt_parse_compact(token, strlen(token), &jwt, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(jwt.header.data == token);
  EXPECT(jwt.header.len == 35u);
  EXPECT(jwt.payload.data == token + 36u);
  EXPECT(jwt.payload.len == 35u);
  EXPECT(jwt.signature.data == token + 72u);
  EXPECT(jwt.signature.len == 4u);
  EXPECT(jwt.signing_input.data == token);
  EXPECT(jwt.signing_input.len == 71u);

  memset(&jwt, 0, sizeof(jwt));
  EXPECT(lj_jwt_parse_compact("AA.AA.", 6u, &jwt, &error) ==
         LJ_STATUS_OK);
  EXPECT(jwt.header.len == 2u);
  EXPECT(jwt.payload.len == 2u);
  EXPECT(jwt.signature.len == 0u);
  EXPECT(jwt.signing_input.len == 5u);
}

static void test_jwt_compact_parse_failures(void) {
  lonejson_jwt_compact jwt;
  lonejson_error error;

  lonejson_error_init(&error);
  EXPECT(lonejson_jwt_parse_compact(NULL, 0u, &jwt, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_jwt_parse_compact("AA.AA.AA", 8u, NULL, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_jwt_parse_compact("", 0u, &jwt, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_jwt_parse_compact("AA.AA", 5u, &jwt, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_jwt_parse_compact(".AA.AA", 6u, &jwt, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_jwt_parse_compact("AA..AA", 6u, &jwt, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_jwt_parse_compact("AA.AA.AA.AA", 11u, &jwt, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_jwt_parse_compact("A.AA.AA", 7u, &jwt, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(error.offset == 1u);
  EXPECT(lonejson_jwt_parse_compact("AA.A=.AA", 8u, &jwt, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(error.offset == 4u);
  EXPECT(lonejson_jwt_parse_compact("AA.AA.A+", 8u, &jwt, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(error.offset == 7u);
}
#else
static void test_jwt_base64url_decode_vectors(void) {}
static void test_jwt_compact_parse_segments(void) {}
static void test_jwt_compact_parse_failures(void) {}
#endif
