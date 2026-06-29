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

static void test_jwk_parse_json_shapes(void) {
  lonejson_jwk jwk;
  lonejson_error error;
  const char *rsa =
      "{\"kty\":\"RSA\",\"kid\":\"rsa1\",\"use\":\"sig\",\"alg\":\"RS256\","
      "\"n\":\"AQIDBA\",\"e\":\"AQAB\"}";
  const char *ec =
      "{\"kty\":\"EC\",\"kid\":\"ec1\",\"crv\":\"P-256\",\"x\":\"AAEC\","
      "\"y\":\"AwQF\"}";
  const char *oct = "{\"kty\":\"oct\",\"kid\":\"sym1\",\"k\":\"c2VjcmV0\"}";

  lonejson_error_init(&error);
  lonejson_jwk_init(&jwk);
  EXPECT(lonejson_jwk_parse_json(test_default_runtime(), rsa, strlen(rsa),
                                 &jwk, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(jwk.kty, "RSA") == 0);
  EXPECT(strcmp(jwk.kid, "rsa1") == 0);
  EXPECT(strcmp(jwk.alg, "RS256") == 0);
  EXPECT(strcmp(jwk.n, "AQIDBA") == 0);
  EXPECT(strcmp(jwk.e, "AQAB") == 0);
  lonejson_jwk_cleanup(&jwk);
  EXPECT(jwk.kty == NULL);

  EXPECT(lj_jwk_parse_json(test_default_runtime(), ec, strlen(ec), &jwk,
                           &error) == LJ_STATUS_OK);
  EXPECT(strcmp(jwk.kty, "EC") == 0);
  EXPECT(strcmp(jwk.crv, "P-256") == 0);
  EXPECT(strcmp(jwk.x, "AAEC") == 0);
  EXPECT(strcmp(jwk.y, "AwQF") == 0);
  lj_jwk_cleanup(&jwk);

  EXPECT(lonejson_jwk_parse_json(test_default_runtime(), oct, strlen(oct),
                                 &jwk, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(jwk.kty, "oct") == 0);
  EXPECT(strcmp(jwk.k, "c2VjcmV0") == 0);
  lonejson_jwk_cleanup(&jwk);
}

static void test_jwks_parse_and_select(void) {
  static const char jwks_json[] =
      "{\"keys\":["
      "{\"kty\":\"RSA\",\"kid\":\"first\",\"use\":\"sig\",\"alg\":\"RS256\","
      "\"n\":\"AQIDBA\",\"e\":\"AQAB\"},"
      "{\"kty\":\"RSA\",\"kid\":\"second\",\"use\":\"enc\",\"alg\":\"RSA-OAEP\","
      "\"n\":\"BQYH\",\"e\":\"AQAB\"},"
      "{\"kty\":\"EC\",\"kid\":\"ec\",\"use\":\"sig\",\"alg\":\"ES256\","
      "\"crv\":\"P-256\",\"x\":\"AAEC\",\"y\":\"AwQF\"}"
      "]}";
  lonejson_jwks jwks;
  lonejson_jwk_select_options options;
  const lonejson_jwk *selected;
  lonejson_error error;

  lonejson_error_init(&error);
  lonejson_jwks_init(&jwks);
  EXPECT(lonejson_jwks_parse_json(test_default_runtime(), jwks_json,
                                  strlen(jwks_json), &jwks,
                                  &error) == LONEJSON_STATUS_OK);
  EXPECT(jwks.keys.count == 3u);

  memset(&options, 0, sizeof(options));
  options.kid = "second";
  options.kty = "RSA";
  EXPECT(lonejson_jwks_select(&jwks, &options, &selected, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(selected != NULL);
  EXPECT(strcmp(selected->kid, "second") == 0);
  EXPECT(strcmp(selected->use, "enc") == 0);

  memset(&options, 0, sizeof(options));
  options.use = "sig";
  options.alg = "ES256";
  EXPECT(lj_jwks_select(&jwks, &options, &selected, &error) == LJ_STATUS_OK);
  EXPECT(selected != NULL);
  EXPECT(strcmp(selected->kid, "ec") == 0);

  memset(&options, 0, sizeof(options));
  options.kid = "missing";
  selected = (const lonejson_jwk *)1;
  EXPECT(lonejson_jwks_select(&jwks, &options, &selected, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(selected == NULL);

  lonejson_jwks_cleanup(&jwks);
  EXPECT(jwks.keys.items == NULL);
  EXPECT(jwks.keys.count == 0u);
}

static void test_jwk_parse_failures(void) {
  lonejson_jwk jwk;
  lonejson_jwks jwks;
  const lonejson_jwk *selected;
  lonejson_error error;

  lonejson_error_init(&error);
  lonejson_jwk_init(&jwk);
  {
    const char *json = "{\"kid\":\"x\"}";
    EXPECT(lonejson_jwk_parse_json(test_default_runtime(), json, strlen(json),
                                   &jwk,
                                 &error) == LONEJSON_STATUS_MISSING_REQUIRED_FIELD);
  }
  {
    const char *json = "{\"kty\":\"RSA\",\"n\":\"AQID\"}";
    EXPECT(lonejson_jwk_parse_json(test_default_runtime(), json, strlen(json),
                                   &jwk,
                                   &error) == LONEJSON_STATUS_INVALID_JSON);
  }
  {
    const char *json = "{\"kty\":\"RSA\",\"n\":\"AQ=\",\"e\":\"AQAB\"}";
    EXPECT(lonejson_jwk_parse_json(test_default_runtime(), json, strlen(json),
                                   &jwk,
                                   &error) == LONEJSON_STATUS_INVALID_JSON);
  }
  EXPECT(jwk.kty == NULL);

  lonejson_jwks_init(&jwks);
  {
    const char *json = "{\"keys\":[]}";
    EXPECT(lonejson_jwks_parse_json(test_default_runtime(), json, strlen(json),
                                    &jwks,
                                    &error) == LONEJSON_STATUS_INVALID_JSON);
  }
  {
    const char *json = "{\"keys\":[{\"kty\":\"EC\",\"x\":\"AA\",\"y\":\"AA\"}]}";
    EXPECT(lonejson_jwks_parse_json(test_default_runtime(), json, strlen(json),
                                    &jwks,
                                    &error) == LONEJSON_STATUS_INVALID_JSON);
  }
  EXPECT(lonejson_jwks_select(NULL, NULL, &selected, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_jwks_select(&jwks, NULL, NULL, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  lonejson_jwks_cleanup(&jwks);
}
#else
static void test_jwt_base64url_decode_vectors(void) {}
static void test_jwt_compact_parse_segments(void) {}
static void test_jwt_compact_parse_failures(void) {}
static void test_jwk_parse_json_shapes(void) {}
static void test_jwks_parse_and_select(void) {}
static void test_jwk_parse_failures(void) {}
#endif
