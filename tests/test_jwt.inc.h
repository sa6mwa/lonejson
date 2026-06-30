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
  EXPECT(lonejson_base64url_decode("AA", 2u, out, sizeof(out), NULL, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
}

static void test_jwt_compact_parse_segments(void) {
  static const char token[] = "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIn0."
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
  EXPECT(lj_jwt_parse_compact("AA.AA.", 6u, &jwt, &error) == LJ_STATUS_OK);
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
  EXPECT(lonejson_jwk_parse_json(test_default_runtime(), rsa, strlen(rsa), &jwk,
                                 &error) == LONEJSON_STATUS_OK);
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

  EXPECT(lonejson_jwk_parse_json(test_default_runtime(), oct, strlen(oct), &jwk,
                                 &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(jwk.kty, "oct") == 0);
  EXPECT(strcmp(jwk.k, "c2VjcmV0") == 0);
  lonejson_jwk_cleanup(&jwk);
}

static void test_jwks_parse_and_select(void) {
  static const char jwks_json[] =
      "{\"keys\":["
      "{\"kty\":\"RSA\",\"kid\":\"first\",\"use\":\"sig\",\"alg\":\"RS256\","
      "\"n\":\"AQIDBA\",\"e\":\"AQAB\"},"
      "{\"kty\":\"RSA\",\"kid\":\"second\",\"use\":\"enc\",\"alg\":\"RSA-"
      "OAEP\","
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
                                   &jwk, &error) ==
           LONEJSON_STATUS_MISSING_REQUIRED_FIELD);
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
    const char *json =
        "{\"keys\":[{\"kty\":\"EC\",\"x\":\"AA\",\"y\":\"AA\"}]}";
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

static lonejson_jwt_claim_policy test_jwt_policy(void) {
  static const char *const algs[] = {"RS256"};
  static const char *const issuers[] = {"issuer"};
  static const char *const audiences[] = {"api"};
  static const char *const required[] = {"iss", "aud", "exp"};
  lonejson_jwt_claim_policy policy;

  memset(&policy, 0, sizeof(policy));
  policy.accepted_algs = algs;
  policy.accepted_alg_count = sizeof(algs) / sizeof(algs[0]);
  policy.accepted_issuers = issuers;
  policy.accepted_issuer_count = sizeof(issuers) / sizeof(issuers[0]);
  policy.accepted_audiences = audiences;
  policy.accepted_audience_count = sizeof(audiences) / sizeof(audiences[0]);
  policy.required_claims = required;
  policy.required_claim_count = sizeof(required) / sizeof(required[0]);
  policy.now = 1000;
  policy.allowed_clock_skew = 30;
  return policy;
}

static const char test_jwt_rs256_token[] =
    "eyJhbGciOiJSUzI1NiIsImtpZCI6InJzYS10ZXN0IiwidHlwIjoiSldUIn0."
    "eyJpc3MiOiJpc3N1ZXIiLCJzdWIiOiJzIiwiYXVkIjoiYXBpIiwiZXhwIjoyMDAwLCJuYm"
    "YiOjkwMCwiaWF0IjoxMDAwfQ."
    "PoouvDAoloqvsfTQxadOTpQGXKyeHq0lx6WQEPv0qvg59KMuy8lD-XBTBPCF_MpQGoe3DS"
    "84CSg27iktG7z12Qv6TX1gqbJUO2wkwhuW4dIWFPY9tDhI3e05W5yz8D70wARx7CL9tHKW"
    "sOpLwHRWf5ugfrq1PuofcC9atB7D-QUfrmmJ01NXbQl4aq6DJ02M7azHTLq-15X3TuE2CH"
    "N5P_zo_zkaJT8V0QhoQ3MUhUE_pBxMtAByRIUOEW32RbWjYgkwZ_zxaVkbhXv1CYznQCzi"
    "kX2wXn9OQ_1z0TCH7bT5Ao3EXEQeiK7Fhuq8lyPFbkhDc_yCjxFRjm7ufSFZbg";

static const char test_jwt_rs256_jwk_json[] =
    "{\"kty\":\"RSA\",\"kid\":\"rsa-test\",\"use\":\"sig\",\"alg\":\"RS256\","
    "\"n\":\"nGFfcf9mkkjv4XoIzgmENq-A3pTE4uT7gzmYMDB4_xwXvHaTogDTrduaIKcd-"
    "oziNa6mM1HXGk-4q8084Wvvz44ZTyRlaVKm2eRHPqjJ1hmxB80nG7iWEkORAKazobRfB8"
    "g7fGXZWhL0JsWqd51igefciKMefuvjs-2_JvusIF6uXu3jSCVRsqXkoZGnYsauGUq4Gcsp"
    "GtCHe5M4oie5kJrfbwcZgajJp4HS-ZUd4m1q12BPSuUSqi5Vb3wS6fLdVsQjxZXVqyk1O"
    "gnI3Ar5by-bbCTML4NZB8icr9uti6nO1TabV4M-skfnGyUFgbOWLxznKmHKphgpiMtHjWy"
    "YoQ\","
    "\"e\":\"AQAB\"}";

static const char test_jwt_rs256_jwks_json[] =
    "{\"keys\":[{\"kty\":\"RSA\",\"kid\":\"rsa-test\",\"use\":\"sig\","
    "\"alg\":\"RS256\","
    "\"n\":\"nGFfcf9mkkjv4XoIzgmENq-A3pTE4uT7gzmYMDB4_xwXvHaTogDTrduaIKcd-"
    "oziNa6mM1HXGk-4q8084Wvvz44ZTyRlaVKm2eRHPqjJ1hmxB80nG7iWEkORAKazobRfB8"
    "g7fGXZWhL0JsWqd51igefciKMefuvjs-2_JvusIF6uXu3jSCVRsqXkoZGnYsauGUq4Gcsp"
    "GtCHe5M4oie5kJrfbwcZgajJp4HS-ZUd4m1q12BPSuUSqi5Vb3wS6fLdVsQjxZXVqyk1O"
    "gnI3Ar5by-bbCTML4NZB8icr9uti6nO1TabV4M-skfnGyUFgbOWLxznKmHKphgpiMtHjWy"
    "YoQ\","
    "\"e\":\"AQAB\"}]}";

static const char test_jwt_rs256_other_kid_jwks_json[] =
    "{\"keys\":[{\"kty\":\"RSA\",\"kid\":\"other-key\",\"use\":\"sig\","
    "\"alg\":\"RS256\","
    "\"n\":\"nGFfcf9mkkjv4XoIzgmENq-A3pTE4uT7gzmYMDB4_xwXvHaTogDTrduaIKcd-"
    "oziNa6mM1HXGk-4q8084Wvvz44ZTyRlaVKm2eRHPqjJ1hmxB80nG7iWEkORAKazobRfB8"
    "g7fGXZWhL0JsWqd51igefciKMefuvjs-2_JvusIF6uXu3jSCVRsqXkoZGnYsauGUq4Gcsp"
    "GtCHe5M4oie5kJrfbwcZgajJp4HS-ZUd4m1q12BPSuUSqi5Vb3wS6fLdVsQjxZXVqyk1O"
    "gnI3Ar5by-bbCTML4NZB8icr9uti6nO1TabV4M-skfnGyUFgbOWLxznKmHKphgpiMtHjWy"
    "YoQ\","
    "\"e\":\"AQAB\"}]}";

static const char test_jwt_rs256_minimal_jwks_json[] =
    "{\"keys\":[{\"kty\":\"RSA\",\"kid\":\"rsa-test\","
    "\"n\":\"nGFfcf9mkkjv4XoIzgmENq-A3pTE4uT7gzmYMDB4_xwXvHaTogDTrduaIKcd-"
    "oziNa6mM1HXGk-4q8084Wvvz44ZTyRlaVKm2eRHPqjJ1hmxB80nG7iWEkORAKazobRfB8"
    "g7fGXZWhL0JsWqd51igefciKMefuvjs-2_JvusIF6uXu3jSCVRsqXkoZGnYsauGUq4Gcsp"
    "GtCHe5M4oie5kJrfbwcZgajJp4HS-ZUd4m1q12BPSuUSqi5Vb3wS6fLdVsQjxZXVqyk1O"
    "gnI3Ar5by-bbCTML4NZB8icr9uti6nO1TabV4M-skfnGyUFgbOWLxznKmHKphgpiMtHjWy"
    "YoQ\","
    "\"e\":\"AQAB\"}]}";

static const char test_jwt_ps256_token[] =
    "eyJhbGciOiJQUzI1NiIsImtpZCI6InBzLXRlc3QiLCJ0eXAiOiJKV1QifQ."
    "eyJpc3MiOiJpc3N1ZXIiLCJzdWIiOiJzdWJqZWN0IiwiYXVkIjoiYXBpIiwiZXhwIjoyM"
    "DAwLCJuYmYiOjkwMCwiaWF0IjoxMDAwfQ."
    "OdIVXXtzJsvBRJzTctmftYO4sDiVYLMy9r2O7yzmwwUPN0e-q4JWRiUc5aAITUDfWLbcn"
    "gBHcVrSCYL2I2o4AASQi2P1A44nKHXsV3Zrn442tSxmyShDiyrpr-mDTJHLI1G949SIA"
    "bF8Kt9zhQ3zcq9BC2fi-nl87Y7dngDWpFVqjF30QQMBV-l0MMlhEQrQT5u9LzlFj9xGE"
    "2etdjs6KpHrI15gOnlGoC7H7hBX4QP3PK0y7B6RMh9HqSkOoINNnAA2srmhMhaoC3ZAh"
    "awXx4fYS0hweeldTE39ZZiNmPk6wlHWLX8anu4AjFe22ND4mlbrBQX_Yt1uekCUUrb0I"
    "A";

static const char test_jwt_ps256_jwk_json[] =
    "{\"kty\":\"RSA\",\"kid\":\"ps-test\",\"use\":\"sig\",\"alg\":\"PS256\","
    "\"n\":\"rLE6MybeCWHjxJ-DG8PFFPGYeVwop5iEbtM75iTuQGglZIDmPRGbV62w6wnp"
    "ILXeaCIBG2Z_JgPh6Zr-2Ic81g8wjAK33Ihd-afm2Ey1qzZltClHyF9pj1TnWn47KnTop"
    "hziLpHuA3AtAyOxv9uyffHLsK6Y5fFtqLWgAynP1q4yDfjevY77TquLw05fmOnjyTbf9"
    "T8cdYAH_LejYltyVS0D6oPdxGFJ8XMNyUwkNHE_w-d4piFzMG_rediEb_l6YT8fn8qyr"
    "LwuxxgrDW88Rhr10W1w52jlHbwljJiN8dgNMR7e-mNe5-iWxAQQQCW3Pf8FQyiC5J3k"
    "o5kK5OjSaQ\","
    "\"e\":\"AQAB\"}";

static const char test_jwt_es256_token[] =
    "eyJhbGciOiJFUzI1NiIsImtpZCI6ImVjLXRlc3QiLCJ0eXAiOiJKV1QifQ."
    "eyJpc3MiOiJpc3N1ZXIiLCJzdWIiOiJzdWJqZWN0IiwiYXVkIjoiYXBpIiwiZXhwIjoyM"
    "DAwLCJuYmYiOjkwMCwiaWF0IjoxMDAwfQ."
    "95YnhtNbEKRS6fmhs543TmLdnljwB3r-wID-L4mGWcOYY4mEmIhmRlbTX1HM_GDKeLGK"
    "yAEw2vY78a4c51eCWQ";

static const char test_jwt_es256_jwk_json[] =
    "{\"kty\":\"EC\",\"kid\":\"ec-test\",\"use\":\"sig\",\"alg\":\"ES256\","
    "\"crv\":\"P-256\",\"x\":\"sKrqPeMnxHsFzQE1uhjgF1PK0x6oM9YBnJKeunXnVx8\","
    "\"y\":\"KTGlbL_TW5NzmtqJRMuTWywE45o4xtm80MeDA84pv4M\"}";

static const char test_jwt_eddsa_token[] =
    "eyJhbGciOiJFZERTQSIsImtpZCI6ImVkLXRlc3QiLCJ0eXAiOiJKV1QifQ."
    "eyJpc3MiOiJpc3N1ZXIiLCJzdWIiOiJzdWJqZWN0IiwiYXVkIjoiYXBpIiwiZXhwIjoyM"
    "DAwLCJuYmYiOjkwMCwiaWF0IjoxMDAwfQ."
    "vZ8EHlu8rMpJbieq2KShKybLutH-oljg9sqjFy8tzk5a4lGoj_9eIKoeckMS5RwMTGwp"
    "GeBgM5gQMl-yFVceCg";

static const char test_jwt_eddsa_jwk_json[] =
    "{\"kty\":\"OKP\",\"kid\":\"ed-test\",\"use\":\"sig\",\"alg\":\"EdDSA\","
    "\"crv\":\"Ed25519\",\"x\":\"clqC7T1N8EI4ekdG3YiNhqGO6Ef30cKhOB4IOKDXjCM\"}";

static void test_jwt_decode_and_validate_claims(void) {
  static const char token[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJzdWIiOiJzIiwiYXVkIjoiYXBpIiwiZXhwIjoyMDAwLCJuYmYi"
      "OjkwMCwiaWF0IjoxMDAwfQ."
      "c2ln";
  static const char aud_array_token[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOlsiYXBpIiwib3RoZXIiXSwiZXhwIjoyMDAwfQ."
      "c2ln";
  static const char nonce_token[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJzdWIiOiJzIiwiYXVkIjoiYXBpIiwiZXhwIjoyMDAwLCJu"
      "b25jZSI6Im5vbmNlLTQ1NiJ9."
      "c2ln";
  lonejson_jwt_header header;
  lonejson_jwt_claims claims;
  lonejson_jwt_claim_policy policy;
  lonejson_error error;

  lonejson_error_init(&error);
  lonejson_jwt_header_init(&header);
  lonejson_jwt_claims_init(&claims);
  policy = test_jwt_policy();

  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), token,
                                     strlen(token), &policy, &header, &claims,
                                     &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(header.alg, "RS256") == 0);
  EXPECT(strcmp(header.kid, "k1") == 0);
  EXPECT(strcmp(header.typ, "JWT") == 0);
  EXPECT(strcmp(claims.iss, "issuer") == 0);
  EXPECT(strcmp(claims.sub, "s") == 0);
  EXPECT(strcmp(claims.aud, "api") == 0);
  EXPECT(claims.has_exp != 0 && claims.exp == 2000);
  EXPECT(claims.has_nbf != 0 && claims.nbf == 900);
  EXPECT(claims.has_iat != 0 && claims.iat == 1000);
  EXPECT(lonejson_jwt_validate_claims(&header, &claims, &policy, &error) ==
         LONEJSON_STATUS_OK);
  lonejson_jwt_header_cleanup(&header);
  lonejson_jwt_claims_cleanup(&claims);

  EXPECT(lj_jwt_decode_compact(test_default_runtime(), aud_array_token,
                               strlen(aud_array_token), &policy, &header,
                               &claims, &error) == LJ_STATUS_OK);
  EXPECT(claims.aud == NULL);
  EXPECT(claims.aud_array.count == 2u);
  EXPECT(strcmp(claims.aud_array.items[0], "api") == 0);
  EXPECT(strcmp(claims.aud_array.items[1], "other") == 0);
  EXPECT(lj_jwt_validate_claims(&header, &claims, &policy, &error) ==
         LJ_STATUS_OK);
  lj_jwt_header_cleanup(&header);
  lj_jwt_claims_cleanup(&claims);

  policy = test_jwt_policy();
  policy.expected_nonce = "nonce-456";
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), nonce_token,
                                     strlen(nonce_token), &policy, &header,
                                     &claims,
                                     &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(claims.nonce, "nonce-456") == 0);
  EXPECT(lonejson_jwt_validate_claims(&header, &claims, &policy, &error) ==
         LONEJSON_STATUS_OK);
  policy.expected_nonce = "other";
  EXPECT(lonejson_jwt_validate_claims(&header, &claims, &policy, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  lonejson_jwt_header_cleanup(&header);
  lonejson_jwt_claims_cleanup(&claims);
}

static void test_jwt_validate_rs256_signature(void) {
  lonejson_jwt_compact compact;
  lonejson_jwt_header header;
  lonejson_jwt_claims claims;
  lonejson_jwk jwk;
  lonejson_error error;

  lonejson_error_init(&error);
  lonejson_jwt_header_init(&header);
  lonejson_jwt_claims_init(&claims);
  lonejson_jwk_init(&jwk);

  EXPECT(lonejson_jwt_parse_compact(test_jwt_rs256_token,
                                    strlen(test_jwt_rs256_token), &compact,
                                    &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(),
                                     test_jwt_rs256_token,
                                     strlen(test_jwt_rs256_token), NULL,
                                     &header, &claims,
                                     &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_jwk_parse_json(test_default_runtime(), test_jwt_rs256_jwk_json,
                                 strlen(test_jwt_rs256_jwk_json), &jwk,
                                 &error) == LONEJSON_STATUS_OK);
#ifdef LONEJSON_WITH_OPENSSL
  EXPECT(lonejson_jwt_validate_signature(&compact, &header, &jwk, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lj_jwt_validate_signature(&compact, &header, &jwk, &error) ==
         LJ_STATUS_OK);
  EXPECT(lonejson_jwt_validate_signature_with_runtime(
             test_default_runtime(), &compact, &header, &jwk, &error) ==
         LONEJSON_STATUS_OK);
#else
  EXPECT(lonejson_jwt_validate_signature(&compact, &header, &jwk, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(lj_jwt_validate_signature(&compact, &header, &jwk, &error) ==
         LJ_STATUS_TYPE_MISMATCH);
  EXPECT(lonejson_jwt_validate_signature_with_runtime(
             test_default_runtime(), &compact, &header, &jwk, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
#endif

  lonejson_jwk_cleanup(&jwk);
  lonejson_jwt_header_cleanup(&header);
  lonejson_jwt_claims_cleanup(&claims);
}

static void test_jwt_validate_recommended_signatures(void) {
#ifdef LONEJSON_WITH_OPENSSL
  static const struct {
    const char *token;
    const char *jwk_json;
  } cases[] = {{test_jwt_ps256_token, test_jwt_ps256_jwk_json},
               {test_jwt_es256_token, test_jwt_es256_jwk_json},
               {test_jwt_eddsa_token, test_jwt_eddsa_jwk_json}};
  size_t i;
  lonejson_error error;

  lonejson_error_init(&error);
  for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    lonejson_jwt_compact compact;
    lonejson_jwt_header header;
    lonejson_jwt_claims claims;
    lonejson_jwk jwk;

    lonejson_jwt_header_init(&header);
    lonejson_jwt_claims_init(&claims);
    lonejson_jwk_init(&jwk);
    EXPECT(lonejson_jwt_parse_compact(cases[i].token, strlen(cases[i].token),
                                      &compact, &error) == LONEJSON_STATUS_OK);
    EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), cases[i].token,
                                       strlen(cases[i].token), NULL, &header,
                                       &claims, &error) ==
           LONEJSON_STATUS_OK);
    EXPECT(lonejson_jwk_parse_json(test_default_runtime(), cases[i].jwk_json,
                                   strlen(cases[i].jwk_json), &jwk,
                                   &error) == LONEJSON_STATUS_OK);
    EXPECT(lonejson_jwt_validate_signature(&compact, &header, &jwk, &error) ==
           LONEJSON_STATUS_OK);
    EXPECT(lonejson_jwt_validate_signature_with_runtime(
               test_default_runtime(), &compact, &header, &jwk, &error) ==
           LONEJSON_STATUS_OK);
    lonejson_jwk_cleanup(&jwk);
    lonejson_jwt_header_cleanup(&header);
    lonejson_jwt_claims_cleanup(&claims);
  }
#endif
}

static void test_jwt_auth_provider_runtime_boundary(void) {
  lonejson_config config;
  lonejson *runtime;
  lonejson_jwt_compact compact;
  lonejson_jwt_header header;
  lonejson_jwt_claims claims;
  lonejson_jwk jwk;
  lonejson_error error;
#ifdef LONEJSON_WITH_OPENSSL
  lonejson_auth_provider provider;
#endif

  lonejson_error_init(&error);
  lonejson_jwt_header_init(&header);
  lonejson_jwt_claims_init(&claims);
  lonejson_jwk_init(&jwk);
  config = lonejson_default_config();
  runtime = lonejson_new(&config, &error);
  EXPECT(runtime != NULL);

  EXPECT(lonejson_jwt_parse_compact(test_jwt_rs256_token,
                                    strlen(test_jwt_rs256_token), &compact,
                                    &error) == LONEJSON_STATUS_OK);
  EXPECT(runtime->jwt_decode_compact != NULL);
  EXPECT(runtime->jwk_parse_json != NULL);
  EXPECT(runtime->jwt_validate_signature_with_runtime != NULL);
  EXPECT(runtime->jwt_decode_compact(runtime, test_jwt_rs256_token,
                                     strlen(test_jwt_rs256_token), NULL,
                                     &header, &claims, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(runtime->jwk_parse_json(runtime, test_jwt_rs256_jwk_json,
                                 strlen(test_jwt_rs256_jwk_json), &jwk,
                                 &error) == LONEJSON_STATUS_OK);
  EXPECT(runtime->jwt_validate_signature_with_runtime(runtime, &compact,
                                                      &header, &jwk, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(lonejson_set_auth_provider(runtime, NULL, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(runtime->set_auth_provider(runtime, NULL, &error) ==
         LONEJSON_STATUS_OK);
#ifdef LONEJSON_WITH_OPENSSL
  EXPECT(lonejson_auth_provider_init_openssl(&provider, NULL, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lj_set_auth_provider(runtime, &provider, &error) == LJ_STATUS_OK);
  EXPECT(runtime->jwt_validate_signature_with_runtime(runtime, &compact,
                                                      &header, &jwk, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lj_jwt_validate_signature_with_runtime(runtime, &compact, &header,
                                                &jwk, &error) == LJ_STATUS_OK);
#endif

  lonejson_jwk_cleanup(&jwk);
  lonejson_jwt_header_cleanup(&header);
  lonejson_jwt_claims_cleanup(&claims);
  lonejson_free(runtime);
}

static void test_jwt_validate_signature_failures(void) {
  lonejson_jwt_compact compact;
  lonejson_jwt_header header;
  lonejson_jwt_claims claims;
  lonejson_jwk jwk;
  lonejson_error error;
  char tampered[sizeof(test_jwt_rs256_token)];
  char oversized_n[11001];
  char *header_alg;
  char *jwk_alg;
  char *jwk_use;
  char *jwk_kty;
  char *jwk_kid;
  char *jwk_n;

  lonejson_error_init(&error);
  lonejson_jwt_header_init(&header);
  lonejson_jwt_claims_init(&claims);
  lonejson_jwk_init(&jwk);

  EXPECT(lonejson_jwt_parse_compact(test_jwt_rs256_token,
                                    strlen(test_jwt_rs256_token), &compact,
                                    &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(),
                                     test_jwt_rs256_token,
                                     strlen(test_jwt_rs256_token), NULL,
                                     &header, &claims,
                                     &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_jwk_parse_json(test_default_runtime(), test_jwt_rs256_jwk_json,
                                 strlen(test_jwt_rs256_jwk_json), &jwk,
                                 &error) == LONEJSON_STATUS_OK);
  header_alg = header.alg;
  jwk_alg = jwk.alg;
  jwk_use = jwk.use;
  jwk_kty = jwk.kty;
  jwk_kid = jwk.kid;
  jwk_n = jwk.n;

  memcpy(tampered, test_jwt_rs256_token, sizeof(tampered));
  tampered[sizeof(tampered) - 2u] =
      tampered[sizeof(tampered) - 2u] == 'A' ? 'B' : 'A';
  EXPECT(lonejson_jwt_parse_compact(tampered, strlen(tampered), &compact,
                                    &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_jwt_validate_signature(&compact, &header, &jwk, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);

  EXPECT(lonejson_jwt_parse_compact(test_jwt_rs256_token,
                                    strlen(test_jwt_rs256_token), &compact,
                                    &error) == LONEJSON_STATUS_OK);
  header.alg = (char *)"ES256";
  EXPECT(lonejson_jwt_validate_signature(&compact, &header, &jwk, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  header.alg = (char *)"none";
  EXPECT(lonejson_jwt_validate_signature(&compact, &header, &jwk, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  header.alg = (char *)"RS256";
  jwk.kid = (char *)"other-key";
  EXPECT(lonejson_jwt_validate_signature(&compact, &header, &jwk, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  jwk.kid = (char *)"rsa-test";
  jwk.alg = (char *)"ES256";
  EXPECT(lonejson_jwt_validate_signature(&compact, &header, &jwk, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  jwk.alg = (char *)"RS256";
  jwk.use = (char *)"enc";
  EXPECT(lonejson_jwt_validate_signature(&compact, &header, &jwk, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  jwk.use = (char *)"sig";
  jwk.kty = (char *)"EC";
  EXPECT(lonejson_jwt_validate_signature(&compact, &header, &jwk, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  jwk.kty = (char *)"RSA";
  memset(oversized_n, 'A', sizeof(oversized_n) - 1u);
  oversized_n[sizeof(oversized_n) - 1u] = '\0';
  jwk.n = oversized_n;
#ifdef LONEJSON_WITH_OPENSSL
  EXPECT(lonejson_jwt_validate_signature(&compact, &header, &jwk, &error) ==
         LONEJSON_STATUS_OVERFLOW);
#else
  EXPECT(lonejson_jwt_validate_signature(&compact, &header, &jwk, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
#endif

  EXPECT(lonejson_jwt_validate_signature(NULL, &header, &jwk, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_jwt_validate_signature(&compact, NULL, &jwk, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_jwt_validate_signature(&compact, &header, NULL, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);

  header.alg = header_alg;
  jwk.alg = jwk_alg;
  jwk.use = jwk_use;
  jwk.kty = jwk_kty;
  jwk.kid = jwk_kid;
  jwk.n = jwk_n;
  lonejson_jwk_cleanup(&jwk);
  lonejson_jwt_header_cleanup(&header);
  lonejson_jwt_claims_cleanup(&claims);
}

static void test_jwt_claim_validation_failures(void) {
  static const char valid_token[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJzdWIiOiJzIiwiYXVkIjoiYXBpIiwiZXhwIjoyMDAwLCJuYmYi"
      "OjkwMCwiaWF0IjoxMDAwfQ."
      "c2ln";
  static const char none_token[] =
      "eyJhbGciOiJub25lIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJleHAiOjIwMDB9.";
  static const char expired_token[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJleHAiOjkwMH0."
      "c2ln";
  lonejson_jwt_header header;
  lonejson_jwt_claims claims;
  lonejson_jwt_claim_policy policy;
  lonejson_error error;
  const char *bad_issuer = "other";
  const char *bad_audience = "other";
  const char *none_alg = "none";

  lonejson_error_init(&error);
  lonejson_jwt_header_init(&header);
  lonejson_jwt_claims_init(&claims);
  policy = test_jwt_policy();

  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), valid_token,
                                     strlen(valid_token), &policy, &header,
                                     &claims, &error) == LONEJSON_STATUS_OK);
  policy.accepted_issuers = &bad_issuer;
  EXPECT(lonejson_jwt_validate_claims(&header, &claims, &policy, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  policy = test_jwt_policy();
  policy.accepted_audiences = &bad_audience;
  EXPECT(lonejson_jwt_validate_claims(&header, &claims, &policy, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  policy = test_jwt_policy();
  policy.accepted_algs = &none_alg;
  EXPECT(lonejson_jwt_validate_claims(&header, &claims, &policy, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  policy = test_jwt_policy();
  policy.allowed_clock_skew = -1;
  EXPECT(lonejson_jwt_validate_claims(&header, &claims, &policy, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  lonejson_jwt_header_cleanup(&header);
  lonejson_jwt_claims_cleanup(&claims);

  policy = test_jwt_policy();
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), none_token,
                                     strlen(none_token), &policy, &header,
                                     &claims, &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_jwt_validate_claims(&header, &claims, &policy, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  lonejson_jwt_header_cleanup(&header);
  lonejson_jwt_claims_cleanup(&claims);

  policy = test_jwt_policy();
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), expired_token,
                                     strlen(expired_token), &policy, &header,
                                     &claims, &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_jwt_validate_claims(&header, &claims, &policy, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  lonejson_jwt_header_cleanup(&header);
  lonejson_jwt_claims_cleanup(&claims);
}

static void test_jwt_decode_claim_failures(void) {
  static const char duplicate_header[] =
      "eyJhbGciOiJSUzI1NiIsImFsZyI6IkVTMjU2In0."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJleHAiOjIwMDB9."
      "c2ln";
  static const char bad_aud_item[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOlsxXX0."
      "c2ln";
  static const char fractional_exp[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJleHAiOjEuNX0."
      "c2ln";
  static const char negative_exp[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJleHAiOi0xfQ."
      "c2ln";
  static const char nonce_number[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6ImsxIiwidHlwIjoiSldUIn0."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJleHAiOjIwMDAsIm5vbmNlIjoxMjN9."
      "c2ln";
  static const char root_array[] =
      "W10."
      "eyJpc3MiOiJpc3N1ZXIiLCJhdWQiOiJhcGkiLCJleHAiOjIwMDB9."
      "c2ln";
  lonejson_jwt_header header;
  lonejson_jwt_claims claims;
  lonejson_jwt_claim_policy policy;
  lonejson_error error;

  lonejson_error_init(&error);
  lonejson_jwt_header_init(&header);
  lonejson_jwt_claims_init(&claims);
  policy = test_jwt_policy();
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), duplicate_header,
                                     strlen(duplicate_header), &policy, &header,
                                     &claims, &error) ==
         LONEJSON_STATUS_DUPLICATE_FIELD);
  EXPECT(header.alg == NULL);
  EXPECT(claims.iss == NULL);

  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), bad_aud_item,
                                     strlen(bad_aud_item), &policy, &header,
                                     &claims,
                                     &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), fractional_exp,
                                     strlen(fractional_exp), &policy, &header,
                                     &claims,
                                     &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), negative_exp,
                                     strlen(negative_exp), &policy, &header,
                                     &claims,
                                     &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(lonejson_jwt_decode_compact(test_default_runtime(), nonce_number,
                                     strlen(nonce_number), &policy, &header,
                                     &claims,
                                     &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(lonejson_jwt_decode_compact(
             test_default_runtime(), root_array, strlen(root_array), &policy,
             &header, &claims, &error) == LONEJSON_STATUS_TYPE_MISMATCH);

  policy.max_token_bytes = 8u;
  EXPECT(lonejson_jwt_decode_compact(
             test_default_runtime(), root_array, strlen(root_array), &policy,
             &header, &claims, &error) == LONEJSON_STATUS_OVERFLOW);
  policy = test_jwt_policy();
  policy.max_decoded_claims_bytes = 4u;
  EXPECT(lonejson_jwt_decode_compact(
             test_default_runtime(), root_array, strlen(root_array), &policy,
             &header, &claims, &error) == LONEJSON_STATUS_OVERFLOW);
  lonejson_jwt_header_cleanup(&header);
  lonejson_jwt_claims_cleanup(&claims);
}

#ifdef LONEJSON_WITH_OIDC
static void test_oidc_discovery_url(void) {
  lonejson_owned_buffer out;
  lonejson_error error;

  lonejson_error_init(&error);
  lonejson_owned_buffer_init(&out);
  EXPECT(lonejson_oidc_discovery_url("https://id.example", &out, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(strcmp(out.data,
                "https://id.example/.well-known/openid-configuration") == 0);
  lonejson_owned_buffer_free(&out);

  lonejson_owned_buffer_init(&out);
  EXPECT(lj_oidc_discovery_url("https://id.example/tenant/", &out, &error) ==
         LJ_STATUS_OK);
  EXPECT(strcmp(out.data,
                "https://id.example/.well-known/openid-configuration/tenant") ==
         0);
  lonejson_owned_buffer_free(&out);

  lonejson_owned_buffer_init(&out);
  EXPECT(lonejson_oidc_discovery_url("http://id.example", &out, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_oidc_discovery_url("https://", &out, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_oidc_discovery_url("https://id.example?x=1", &out, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_oidc_discovery_url("https://id.example#frag", &out, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_oidc_discovery_url(NULL, &out, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_oidc_discovery_url("https://id.example", NULL, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  lonejson_owned_buffer_free(&out);
}

static void test_oidc_discovery_parse_and_validate(void) {
  static const char json[] =
      "{\"issuer\":\"https://id.example/tenant\","
      "\"authorization_endpoint\":\"https://id.example/auth\","
      "\"token_endpoint\":\"https://id.example/token\","
      "\"jwks_uri\":\"https://id.example/jwks\"}";
  lonejson_oidc_discovery discovery;
  lonejson_error error;

  lonejson_error_init(&error);
  lonejson_oidc_discovery_init(&discovery);
  EXPECT(lonejson_oidc_discovery_parse_json(test_default_runtime(), json,
                                            strlen(json), &discovery,
                                            &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(discovery.issuer, "https://id.example/tenant") == 0);
  EXPECT(strcmp(discovery.authorization_endpoint, "https://id.example/auth") ==
         0);
  EXPECT(strcmp(discovery.token_endpoint, "https://id.example/token") == 0);
  EXPECT(strcmp(discovery.jwks_uri, "https://id.example/jwks") == 0);
  EXPECT(lonejson_oidc_discovery_validate_issuer(&discovery,
                                                 "https://id.example/tenant",
                                                 &error) == LONEJSON_STATUS_OK);
  EXPECT(lj_oidc_discovery_validate_issuer(&discovery, "https://id.example",
                                           &error) == LJ_STATUS_TYPE_MISMATCH);
  lj_oidc_discovery_cleanup(&discovery);
  EXPECT(discovery.issuer == NULL);
}

static void test_oidc_discovery_failures(void) {
  lonejson_oidc_discovery discovery;
  lonejson_error error;

  lonejson_error_init(&error);
  lonejson_oidc_discovery_init(&discovery);
  {
    const char *json =
        "{\"issuer\":\"https://id.example\",\"jwks_uri\":\"https://id.example/"
        "jwks\"}";
    EXPECT(lonejson_oidc_discovery_parse_json(
               test_default_runtime(), json, strlen(json), &discovery,
               &error) == LONEJSON_STATUS_MISSING_REQUIRED_FIELD);
  }
  EXPECT(discovery.issuer == NULL);

  {
    const char *json = "{\"issuer\":\"http://id.example\","
                       "\"token_endpoint\":\"https://id.example/token\","
                       "\"jwks_uri\":\"https://id.example/jwks\"}";
    EXPECT(lonejson_oidc_discovery_parse_json(
               test_default_runtime(), json, strlen(json), &discovery,
               &error) == LONEJSON_STATUS_INVALID_JSON);
  }
  EXPECT(discovery.issuer == NULL);

  {
    const char *json = "{\"issuer\":\"https://id.example\","
                       "\"token_endpoint\":\"https://id.example/token\","
                       "\"jwks_uri\":\"/jwks\"}";
    EXPECT(lonejson_oidc_discovery_parse_json(
               test_default_runtime(), json, strlen(json), &discovery,
               &error) == LONEJSON_STATUS_INVALID_JSON);
  }
  EXPECT(discovery.issuer == NULL);

  EXPECT(lonejson_oidc_discovery_parse_json(test_default_runtime(), "{}", 2u,
                                            NULL, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_oidc_discovery_parse_json(test_default_runtime(), NULL, 1u,
                                            &discovery, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_oidc_discovery_validate_issuer(NULL, "https://id.example",
                                                 &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_oidc_discovery_validate_issuer(&discovery,
                                                 "http://id.example", &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  lonejson_oidc_discovery_cleanup(&discovery);
}

static lonejson_oidc_jwks_cache_policy test_oidc_jwks_cache_policy(void) {
  lonejson_oidc_jwks_cache_policy policy;

  memset(&policy, 0, sizeof(policy));
  policy.issuer = "https://id.example";
  policy.jwks_uri = "https://id.example/jwks";
  policy.max_jwks_bytes = 4096u;
  policy.now = 1000;
  policy.ttl_seconds = 60;
  return policy;
}

static void test_oidc_jwks_cache_update_and_select(void) {
  static const char jwks_json[] =
      "{\"keys\":["
      "{\"kty\":\"RSA\",\"kid\":\"rsa-test\",\"use\":\"sig\",\"alg\":\"RS256\","
      "\"n\":\"AQIDBA\",\"e\":\"AQAB\"},"
      "{\"kty\":\"EC\",\"kid\":\"ec\",\"use\":\"sig\",\"alg\":\"ES256\","
      "\"crv\":\"P-256\",\"x\":\"AAEC\",\"y\":\"AwQF\"}"
      "]}";
  lonejson_oidc_jwks_cache cache;
  lonejson_oidc_jwks_cache_policy policy;
  lonejson_jwk_select_options options;
  const lonejson_jwk *selected;
  lonejson_error error;

  lonejson_error_init(&error);
  lonejson_oidc_jwks_cache_init(&cache);
  policy = test_oidc_jwks_cache_policy();
  EXPECT(lonejson_oidc_jwks_cache_update_json(
             test_default_runtime(), &cache, &policy, jwks_json,
             strlen(jwks_json), &error) == LONEJSON_STATUS_OK);
  EXPECT(cache.has_jwks);
  EXPECT(cache.fetched_at == 1000);
  EXPECT(cache.expires_at == 1060);
  EXPECT(strcmp(cache.issuer, "https://id.example") == 0);
  EXPECT(strcmp(cache.jwks_uri, "https://id.example/jwks") == 0);
  EXPECT(lj_oidc_jwks_cache_is_fresh(&cache, &policy));
  {
    lonejson_oidc_jwks_cache_policy invalid_policy;
    memset(&invalid_policy, 0, sizeof(invalid_policy));
    EXPECT(!lonejson_oidc_jwks_cache_is_fresh(&cache, &invalid_policy));
    invalid_policy = test_oidc_jwks_cache_policy();
    invalid_policy.issuer = NULL;
    EXPECT(!lonejson_oidc_jwks_cache_is_fresh(&cache, &invalid_policy));
    invalid_policy = test_oidc_jwks_cache_policy();
    invalid_policy.jwks_uri = NULL;
    EXPECT(!lonejson_oidc_jwks_cache_is_fresh(&cache, &invalid_policy));
  }

  memset(&options, 0, sizeof(options));
  options.kid = "rsa-test";
  options.kty = "RSA";
  options.alg = "RS256";
  options.use = "sig";
  EXPECT(lonejson_oidc_jwks_cache_select(&cache, &policy, &options, &selected,
                                         &error) == LONEJSON_STATUS_OK);
  EXPECT(selected != NULL);
  EXPECT(strcmp(selected->kid, "rsa-test") == 0);

  policy.now = 1060;
  EXPECT(!lonejson_oidc_jwks_cache_is_fresh(&cache, &policy));
  EXPECT(lonejson_oidc_jwks_cache_select(&cache, &policy, &options, &selected,
                                         &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  policy = test_oidc_jwks_cache_policy();
  policy.issuer = "https://other.example";
  EXPECT(lonejson_oidc_jwks_cache_select(&cache, &policy, &options, &selected,
                                         &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);

  lonejson_oidc_jwks_cache_cleanup(&cache);
  EXPECT(cache.jwks.keys.items == NULL);
}

static void test_oidc_jwks_cache_failure_modes(void) {
  static const char good_jwks[] =
      "{\"keys\":[{\"kty\":\"RSA\",\"kid\":\"rsa-test\",\"use\":\"sig\","
      "\"alg\":\"RS256\",\"n\":\"AQIDBA\",\"e\":\"AQAB\"}]}";
  static const char bad_jwks[] = "{\"keys\":[]}";
  lonejson_oidc_jwks_cache cache;
  lonejson_oidc_jwks_cache_policy policy;
  lonejson_jwk_select_options options;
  const lonejson_jwk *selected;
  lonejson_error error;
  const char *old_issuer;

  lonejson_error_init(&error);
  lonejson_oidc_jwks_cache_init(&cache);
  policy = test_oidc_jwks_cache_policy();
  EXPECT(lonejson_oidc_jwks_cache_update_json(
             test_default_runtime(), &cache, &policy, good_jwks,
             strlen(good_jwks), &error) == LONEJSON_STATUS_OK);
  old_issuer = cache.issuer;

  policy.max_jwks_bytes = 4u;
  EXPECT(lonejson_oidc_jwks_cache_update_json(
             test_default_runtime(), &cache, &policy, good_jwks,
             strlen(good_jwks), &error) == LONEJSON_STATUS_OVERFLOW);
  EXPECT(cache.issuer == old_issuer);

  policy = test_oidc_jwks_cache_policy();
  EXPECT(lonejson_oidc_jwks_cache_update_json(
             test_default_runtime(), &cache, &policy, bad_jwks,
             strlen(bad_jwks), &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(cache.issuer == old_issuer);

  policy.ttl_seconds = 0;
  EXPECT(lonejson_oidc_jwks_cache_update_json(
             test_default_runtime(), &cache, &policy, good_jwks,
             strlen(good_jwks), &error) == LONEJSON_STATUS_INVALID_ARGUMENT);
  policy = test_oidc_jwks_cache_policy();
  policy.issuer = "http://id.example";
  EXPECT(lonejson_oidc_jwks_cache_update_json(
             test_default_runtime(), &cache, &policy, good_jwks,
             strlen(good_jwks), &error) == LONEJSON_STATUS_INVALID_JSON);
  policy = test_oidc_jwks_cache_policy();
  policy.now = (lonejson_int64)(LONEJSON_UINT64_MAX >> 1) - 1;
  policy.ttl_seconds = 2;
  EXPECT(lonejson_oidc_jwks_cache_update_json(
             test_default_runtime(), &cache, &policy, good_jwks,
             strlen(good_jwks), &error) == LONEJSON_STATUS_OVERFLOW);

  memset(&options, 0, sizeof(options));
  EXPECT(lonejson_oidc_jwks_cache_select(&cache, NULL, &options, &selected,
                                         &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_oidc_jwks_cache_select(&cache, &policy, &options, NULL,
                                         &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  lonejson_oidc_jwks_cache_cleanup(&cache);
}

static void test_oidc_jwks_cache_curl_adapter(void) {
#ifdef LONEJSON_WITH_CURL
  static const char chunk0[] =
      "{\"keys\":[{\"kty\":\"RSA\",\"kid\":\"rsa-test\",";
  static const char chunk1[] =
      "\"use\":\"sig\",\"alg\":\"RS256\",\"n\":\"AQIDBA\",\"e\":\"AQAB\"}]}";
  lonejson_oidc_jwks_cache cache;
  lonejson_oidc_jwks_cache_parse parse;
  lonejson_oidc_jwks_cache_policy policy;
  lonejson_jwk_select_options options;
  const lonejson_jwk *selected;
  size_t wrote;

  lonejson_oidc_jwks_cache_init(&cache);
  memset(&parse, 0, sizeof(parse));
  policy = test_oidc_jwks_cache_policy();
  EXPECT(lonejson_oidc_jwks_cache_parse_init(&parse, test_default_runtime(),
                                             &cache, &policy) ==
         LONEJSON_STATUS_OK);
  wrote = lonejson_oidc_jwks_cache_write_callback(
      (char *)chunk0, 1u, strlen(chunk0), &parse);
  EXPECT(wrote == strlen(chunk0));
  wrote = parse.write_callback(&parse, (char *)chunk1, 1u, strlen(chunk1));
  EXPECT(wrote == strlen(chunk1));
  EXPECT(parse.finish(&parse) == LONEJSON_STATUS_OK);

  memset(&options, 0, sizeof(options));
  options.kid = "rsa-test";
  EXPECT(lonejson_oidc_jwks_cache_select(&cache, &policy, &options, &selected,
                                         &parse.error) == LONEJSON_STATUS_OK);
  EXPECT(selected != NULL);
  parse.cleanup(&parse);

  policy.max_jwks_bytes = 8u;
  EXPECT(lj_oidc_jwks_cache_parse_init(&parse, test_default_runtime(), &cache,
                                       &policy) == LJ_STATUS_OK);
  wrote = lj_oidc_jwks_cache_write_callback((char *)chunk0, 1u,
                                            strlen(chunk0), &parse);
  EXPECT(wrote == 0u);
  EXPECT(lonejson_oidc_jwks_cache_parse_finish(&parse) ==
         LONEJSON_STATUS_OVERFLOW);
  lonejson_oidc_jwks_cache_parse_cleanup(&parse);
  lonejson_oidc_jwks_cache_cleanup(&cache);
#endif
}

typedef struct test_oidc_http_provider_state {
  int fail_callback;
  int requests;
} test_oidc_http_provider_state;

static lonejson_status test_oidc_http_provider_respond(
    lonejson_http_response *response, const char *body, size_t max_bytes,
    lonejson_error *error) {
  size_t len = strlen(body);
  lonejson_http_response_cleanup(response);
  if (max_bytes != 0u && len > max_bytes) {
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "test response exceeds configured limit");
  }
  response->status_code = 200;
  return lonejson_owned_buffer_sink(&response->body, body, len, error);
}

static lonejson_status test_oidc_http_provider_request(
    void *user_data, const lonejson_http_request *request,
    lonejson_http_response *response, lonejson_error *error) {
  static const char discovery_json[] =
      "{\"issuer\":\"https://issuer.example\","
      "\"authorization_endpoint\":\"https://issuer.example/auth\","
      "\"token_endpoint\":\"https://issuer.example/token\","
      "\"jwks_uri\":\"https://issuer.example/jwks\"}";
  static const char token_json[] =
      "{\"access_token\":\"abc.def.sig\",\"token_type\":\"Bearer\","
      "\"expires_in\":60}";
  test_oidc_http_provider_state *state =
      (test_oidc_http_provider_state *)user_data;

  EXPECT(state != NULL);
  EXPECT(request != NULL);
  EXPECT(response != NULL);
  ++state->requests;
  if (state->fail_callback) {
    return lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u,
                               0u, "test HTTP provider failed");
  }
  if (strcmp(request->url,
             "https://issuer.example/.well-known/openid-configuration") == 0) {
    EXPECT(strcmp(request->method, "GET") == 0);
    EXPECT(request->user_agent != NULL);
    EXPECT(strcmp(request->user_agent, "lonejson-test/1") == 0);
    EXPECT(request->body == NULL);
    EXPECT(request->body_len == 0u);
    return test_oidc_http_provider_respond(
        response, discovery_json, request->max_response_bytes, error);
  }
  if (strcmp(request->url, "https://issuer.example/jwks") == 0) {
    EXPECT(strcmp(request->method, "GET") == 0);
    EXPECT(request->user_agent != NULL);
    EXPECT(strcmp(request->user_agent, "lonejson-test/1") == 0);
    EXPECT(request->body == NULL);
    EXPECT(request->body_len == 0u);
    return test_oidc_http_provider_respond(
        response, test_jwt_rs256_jwks_json, request->max_response_bytes, error);
  }
  if (strcmp(request->url, "https://issuer.example/token") == 0) {
    EXPECT(strcmp(request->method, "POST") == 0);
    EXPECT(request->user_agent != NULL);
    EXPECT(strcmp(request->user_agent, "lonejson-test/1") == 0);
    EXPECT(request->content_type != NULL);
    EXPECT(strcmp(request->content_type,
                  "application/x-www-form-urlencoded") == 0);
    EXPECT(request->body != NULL);
    EXPECT(request->body_len != 0u);
    EXPECT(strstr((const char *)request->body, "grant_type=") != NULL);
    return test_oidc_http_provider_respond(
        response, token_json, request->max_response_bytes, error);
  }
  lonejson_http_response_cleanup(response);
  response->status_code = 404;
  return lonejson_owned_buffer_sink(&response->body, "not found", 9u, error);
}

static void test_oidc_http_provider_helpers(void) {
  lonejson *runtime;
  lonejson_error error;
  lonejson_http_provider provider;
  lonejson_http_provider_config provider_config;
  test_oidc_http_provider_state state;
  lonejson_oidc_discovery discovery;
  lonejson_oidc_jwks_cache cache;
  lonejson_oidc_jwks_cache_policy policy;
  lonejson_oauth2_client_credentials token_request;
  lonejson_oauth2_refresh_token refresh_request;
  lonejson_oidc_authorization_code_token code_request;
  lonejson_oauth2_token_response token_response;
  static const char discovery_json[] =
      "{\"issuer\":\"https://issuer.example\","
      "\"authorization_endpoint\":\"https://issuer.example/auth\","
      "\"token_endpoint\":\"https://issuer.example/token\","
      "\"jwks_uri\":\"https://issuer.example/jwks\"}";

  lonejson_error_init(&error);
  memset(&provider, 0, sizeof(provider));
  memset(&provider_config, 0, sizeof(provider_config));
  memset(&state, 0, sizeof(state));
  runtime = lonejson_new(NULL, &error);
  EXPECT(runtime != NULL);

  EXPECT(lonejson_set_http_provider(runtime, &provider, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_oidc_fetch_discovery(runtime, "https://issuer.example",
                                       4096u, &discovery, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);

  provider_config.user_data = &state;
  provider_config.user_agent = "lonejson-test/1";
  provider_config.request = test_oidc_http_provider_request;
  EXPECT(lonejson_http_provider_init(&provider, &provider_config, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(provider.user_data == &state);
  EXPECT(provider.user_agent == provider_config.user_agent);
  EXPECT(provider.request == test_oidc_http_provider_request);
  EXPECT(lonejson_http_provider_init_simple(
             &provider, &state, "lonejson-test/1",
             test_oidc_http_provider_request, &error) == LONEJSON_STATUS_OK);
  EXPECT(provider.user_data == &state);
  EXPECT(provider.user_agent != NULL);
  EXPECT(strcmp(provider.user_agent, "lonejson-test/1") == 0);
  EXPECT(provider.request == test_oidc_http_provider_request);
  EXPECT(runtime->set_http_provider(runtime, &provider, &error) ==
         LONEJSON_STATUS_OK);

  lonejson_oidc_discovery_init(&discovery);
  EXPECT(runtime->oidc_discovery_parse_json != NULL);
  EXPECT(runtime->oidc_fetch_discovery != NULL);
  EXPECT(runtime->oidc_jwks_cache_update_json != NULL);
  EXPECT(runtime->oidc_jwks_cache_refresh != NULL);
  EXPECT(runtime->oauth2_token_response_parse_json != NULL);
  EXPECT(runtime->oauth2_client_credentials_request != NULL);
  EXPECT(runtime->oauth2_refresh_token_request != NULL);
  EXPECT(runtime->oidc_authorization_code_token_request != NULL);
  EXPECT(runtime->oidc_validate_bearer_token != NULL);
  EXPECT(runtime->oidc_discovery_parse_json(
             runtime, discovery_json, strlen(discovery_json), &discovery,
             &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(discovery.issuer, "https://issuer.example") == 0);
  lonejson_oidc_discovery_cleanup(&discovery);

  lonejson_oidc_discovery_init(&discovery);
  EXPECT(runtime->oidc_fetch_discovery(runtime, "https://issuer.example",
                                       4096u, &discovery, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(strcmp(discovery.issuer, "https://issuer.example") == 0);
  EXPECT(strcmp(discovery.jwks_uri, "https://issuer.example/jwks") == 0);
  lonejson_oidc_discovery_cleanup(&discovery);

  lonejson_oidc_jwks_cache_init(&cache);
  policy = test_oidc_jwks_cache_policy();
  policy.issuer = "https://issuer.example";
  policy.jwks_uri = "https://issuer.example/jwks";
  EXPECT(runtime->oidc_jwks_cache_update_json(
             runtime, &cache, &policy, test_jwt_rs256_jwks_json,
             strlen(test_jwt_rs256_jwks_json), &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_oidc_jwks_cache_is_fresh(&cache, &policy));
  lonejson_oidc_jwks_cache_cleanup(&cache);

  lonejson_oidc_jwks_cache_init(&cache);
  EXPECT(runtime->oidc_jwks_cache_refresh(runtime, &cache, &policy, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_oidc_jwks_cache_is_fresh(&cache, &policy));
  lonejson_oidc_jwks_cache_cleanup(&cache);

  memset(&token_request, 0, sizeof(token_request));
  token_request.client_id = "client";
  token_request.client_secret = "secret";
  lonejson_oauth2_token_response_init(&token_response);
  EXPECT(runtime->oauth2_token_response_parse_json(
             runtime,
             "{\"access_token\":\"parsed\",\"token_type\":\"Bearer\","
             "\"expires_in\":1}",
             strlen("{\"access_token\":\"parsed\",\"token_type\":\"Bearer\","
                    "\"expires_in\":1}"),
             4096u, &token_response, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(token_response.access_token, "parsed") == 0);
  lonejson_oauth2_token_response_cleanup(&token_response);

  EXPECT(runtime->oauth2_client_credentials_request(
             runtime, "https://issuer.example/token", &token_request, 4096u,
             &token_response, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(token_response.access_token, "abc.def.sig") == 0);
  EXPECT(token_response.has_expires_in);
  EXPECT(token_response.expires_in == 60);
  lonejson_oauth2_token_response_cleanup(&token_response);

  memset(&refresh_request, 0, sizeof(refresh_request));
  refresh_request.refresh_token = "refresh token";
  refresh_request.client_id = "client";
  refresh_request.client_secret = "secret";
  EXPECT(runtime->oauth2_refresh_token_request(
             runtime, "https://issuer.example/token", &refresh_request, 4096u,
             &token_response, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(token_response.access_token, "abc.def.sig") == 0);
  lonejson_oauth2_token_response_cleanup(&token_response);

  memset(&code_request, 0, sizeof(code_request));
  code_request.client_id = "client";
  code_request.code = "code+123";
  code_request.redirect_uri = "http://127.0.0.1/cb";
  code_request.code_verifier =
      "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
  EXPECT(runtime->oidc_authorization_code_token_request(
             runtime, "https://issuer.example/token", &code_request, 4096u,
             &token_response, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(token_response.token_type, "Bearer") == 0);
  lonejson_oauth2_token_response_cleanup(&token_response);

  EXPECT(runtime->oauth2_client_credentials_request(
             runtime, "https://issuer.example/token", &token_request, 8u,
             &token_response, &error) == LONEJSON_STATUS_OVERFLOW);
  EXPECT(state.requests >= 6);

  state.fail_callback = 1;
  EXPECT(runtime->oidc_fetch_discovery(runtime, "https://issuer.example",
                                       4096u, &discovery, &error) ==
         LONEJSON_STATUS_CALLBACK_FAILED);
  state.fail_callback = 0;

  EXPECT(lonejson_set_http_provider(runtime, NULL, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_oidc_fetch_discovery(runtime, "https://issuer.example",
                                       4096u, &discovery, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);

  EXPECT(lonejson_http_provider_init(&provider, NULL, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  provider_config.request = NULL;
  EXPECT(lonejson_http_provider_init(&provider, &provider_config, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lj_http_provider_init_simple(&provider, &state, "lonejson-test/1",
                                      NULL, &error) ==
         LJ_STATUS_INVALID_ARGUMENT);

  lonejson_free(runtime);
}

static void test_oauth2_client_credentials_body(void) {
  lonejson_oauth2_client_credentials request;
  lonejson_oauth2_refresh_token refresh_request;
  lonejson_oidc_authorization_code_token code_request;
  lonejson_owned_buffer out;
  lonejson_error error;

  memset(&request, 0, sizeof(request));
  lonejson_owned_buffer_init(&out);
  lonejson_error_init(&error);
  request.client_id = "client id";
  request.client_secret = "s+e&c=r%t";
  request.scope = "read write";
  request.audience = "https://api.example/a?b=c";
  request.resource = "urn:example:resource";
  EXPECT(lonejson_oauth2_client_credentials_body(&request, &out, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(strcmp(out.data,
                "grant_type=client_credentials&client_id=client+id&"
                "client_secret=s%2Be%26c%3Dr%25t&scope=read+write&"
                "audience=https%3A%2F%2Fapi.example%2Fa%3Fb%3Dc&"
                "resource=urn%3Aexample%3Aresource") == 0);
  lonejson_owned_buffer_free(&out);

  request.max_body_bytes = 8u;
  EXPECT(lj_oauth2_client_credentials_body(&request, &out, &error) ==
         LJ_STATUS_OVERFLOW);
  EXPECT(out.data == NULL);

  request.max_body_bytes = 0u;
  request.client_secret = "";
  EXPECT(lonejson_oauth2_client_credentials_body(&request, &out, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  request.client_secret = "secret";
  request.client_id = NULL;
  EXPECT(lonejson_oauth2_client_credentials_body(&request, &out, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);

  memset(&refresh_request, 0, sizeof(refresh_request));
  refresh_request.refresh_token = "r+e&f";
  refresh_request.client_id = "client id";
  refresh_request.client_secret = "secret";
  refresh_request.scope = "read write";
  EXPECT(lonejson_oauth2_refresh_token_body(&refresh_request, &out, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(strcmp(out.data,
                "grant_type=refresh_token&refresh_token=r%2Be%26f&"
                "client_id=client+id&client_secret=secret&scope=read+write") ==
         0);
  lonejson_owned_buffer_free(&out);

  refresh_request.max_body_bytes = 8u;
  EXPECT(lonejson_oauth2_refresh_token_body(&refresh_request, &out, &error) ==
         LONEJSON_STATUS_OVERFLOW);
  refresh_request.max_body_bytes = 0u;
  refresh_request.refresh_token = NULL;
  EXPECT(lonejson_oauth2_refresh_token_body(&refresh_request, &out, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  refresh_request.refresh_token = "refresh";
  refresh_request.client_id = NULL;
  EXPECT(lonejson_oauth2_refresh_token_body(&refresh_request, &out, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);

  memset(&code_request, 0, sizeof(code_request));
  code_request.client_id = "client id";
  code_request.code = "code+123";
  code_request.redirect_uri = "http://127.0.0.1:1234/cb";
  code_request.code_verifier = "verifier value";
  code_request.client_secret = "secret";
  EXPECT(lonejson_oidc_authorization_code_token_body(&code_request, &out,
                                                     &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(strcmp(out.data,
                "grant_type=authorization_code&client_id=client+id&"
                "code=code%2B123&"
                "redirect_uri=http%3A%2F%2F127.0.0.1%3A1234%2Fcb&"
                "code_verifier=verifier+value&client_secret=secret") == 0);
  lonejson_owned_buffer_free(&out);

  code_request.max_body_bytes = 8u;
  EXPECT(lj_oidc_authorization_code_token_body(&code_request, &out, &error) ==
         LJ_STATUS_OVERFLOW);
  code_request.max_body_bytes = 0u;
  code_request.code_verifier = NULL;
  EXPECT(lonejson_oidc_authorization_code_token_body(&code_request, &out,
                                                     &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  lonejson_owned_buffer_free(&out);
}

static void test_oauth2_token_response_parse(void) {
  static const char response_json[] =
      "{\"access_token\":\"token\",\"token_type\":\"bearer\","
      "\"expires_in\":3600,\"scope\":\"read write\","
      "\"refresh_token\":\"refresh\",\"id_token\":\"id.jwt\"}";
  lonejson_oauth2_token_response response;
  lonejson_error error;

  lonejson_error_init(&error);
  lonejson_oauth2_token_response_init(&response);
  EXPECT(lonejson_oauth2_token_response_parse_json(
             test_default_runtime(), response_json, strlen(response_json), 0u,
             &response, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(response.access_token, "token") == 0);
  EXPECT(strcmp(response.token_type, "bearer") == 0);
  EXPECT(response.has_expires_in);
  EXPECT(response.expires_in == 3600);
  EXPECT(strcmp(response.scope, "read write") == 0);
  EXPECT(strcmp(response.refresh_token, "refresh") == 0);
  EXPECT(strcmp(response.id_token, "id.jwt") == 0);
  lonejson_oauth2_token_response_cleanup(&response);
  EXPECT(response.access_token == NULL);
}

static void test_oauth2_token_response_failures(void) {
  lonejson_oauth2_token_response response;
  lonejson_error error;

  lonejson_error_init(&error);
  lonejson_oauth2_token_response_init(&response);
  EXPECT(lonejson_oauth2_token_response_parse_json(
             test_default_runtime(), "{\"error\":\"invalid_client\"}",
             strlen("{\"error\":\"invalid_client\"}"), 0u, &response,
             &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(response.error == NULL);

  EXPECT(lonejson_oauth2_token_response_parse_json(
             test_default_runtime(), "{\"access_token\":\"token\"}",
             strlen("{\"access_token\":\"token\"}"), 0u, &response, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(response.access_token == NULL);

  EXPECT(lonejson_oauth2_token_response_parse_json(
             test_default_runtime(),
             "{\"access_token\":\"token\",\"token_type\":\"mac\"}",
             strlen("{\"access_token\":\"token\",\"token_type\":\"mac\"}"), 0u,
             &response, &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(response.access_token == NULL);

  EXPECT(lonejson_oauth2_token_response_parse_json(
             test_default_runtime(),
             "{\"access_token\":\"token\",\"token_type\":\"Bearer\","
             "\"expires_in\":-1}",
             strlen("{\"access_token\":\"token\",\"token_type\":\"Bearer\","
                    "\"expires_in\":-1}"),
             0u, &response, &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(response.access_token == NULL);

  EXPECT(lonejson_oauth2_token_response_parse_json(
             test_default_runtime(),
             "{\"access_token\":\"token\",\"token_type\":\"Bearer\"}",
             strlen("{\"access_token\":\"token\",\"token_type\":\"Bearer\"}"),
             8u, &response, &error) == LONEJSON_STATUS_OVERFLOW);
  lonejson_oauth2_token_response_cleanup(&response);
}

static void test_oidc_pkce_challenge_and_generate(void) {
  static const char verifier[] =
      "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
  lonejson_owned_buffer challenge;
  lonejson_oidc_pkce pkce;
  lonejson_error error;
#ifdef LONEJSON_WITH_OPENSSL
  size_t i;
#endif

  lonejson_error_init(&error);
  lonejson_owned_buffer_init(&challenge);
#ifdef LONEJSON_WITH_OPENSSL
  EXPECT(lonejson_oidc_pkce_challenge(verifier, &challenge, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(strcmp(challenge.data,
                "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM") == 0);
  lonejson_owned_buffer_free(&challenge);

  lonejson_oidc_pkce_init(&pkce);
  EXPECT(lj_oidc_pkce_generate(0u, &pkce, &error) == LJ_STATUS_OK);
  EXPECT(strlen(pkce.code_verifier) == 43u);
  EXPECT(strlen(pkce.code_challenge) == 43u);
  for (i = 0u; pkce.code_verifier[i] != '\0'; ++i) {
    EXPECT(pkce.code_verifier[i] != '+');
    EXPECT(pkce.code_verifier[i] != '/');
    EXPECT(pkce.code_verifier[i] != '=');
  }
  lonejson_oidc_pkce_cleanup(&pkce);
#else
  EXPECT(lonejson_oidc_pkce_challenge(verifier, &challenge, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  lonejson_oidc_pkce_init(&pkce);
  EXPECT(lj_oidc_pkce_generate(0u, &pkce, &error) ==
         LJ_STATUS_TYPE_MISMATCH);
#endif

  EXPECT(lonejson_oidc_pkce_challenge("too-short", &challenge, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_oidc_pkce_generate(31u, &pkce, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
}

static void test_oidc_authorization_url(void) {
  lonejson_oidc_authorization_request request;
  lonejson_owned_buffer url;
  lonejson_error error;

  memset(&request, 0, sizeof(request));
  lonejson_owned_buffer_init(&url);
  lonejson_error_init(&error);
  request.authorization_endpoint = "https://id.example/auth";
  request.client_id = "client id";
  request.redirect_uri = "http://127.0.0.1:1234/cb";
  request.scope = "openid profile";
  request.state = "state-123";
  request.nonce = "nonce-456";
  request.code_challenge = "challenge";
  request.audience = "https://api.example";
  EXPECT(lonejson_oidc_authorization_url(&request, &url, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(strcmp(url.data,
                "https://id.example/auth?response_type=code&"
                "client_id=client+id&"
                "redirect_uri=http%3A%2F%2F127.0.0.1%3A1234%2Fcb&"
                "scope=openid+profile&state=state-123&nonce=nonce-456&"
                "code_challenge=challenge&code_challenge_method=S256&"
                "audience=https%3A%2F%2Fapi.example") == 0);
  lonejson_owned_buffer_free(&url);

  request.max_url_bytes = 16u;
  EXPECT(lonejson_oidc_authorization_url(&request, &url, &error) ==
         LONEJSON_STATUS_OVERFLOW);
  EXPECT(url.data == NULL);
  request.max_url_bytes = 0u;
  request.authorization_endpoint = "http://id.example/auth";
  EXPECT(lonejson_oidc_authorization_url(&request, &url, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
}

static void test_oidc_authorization_callback_parse(void) {
  lonejson_oidc_authorization_callback callback;
  lonejson_error error;

  lonejson_error_init(&error);
  lonejson_oidc_authorization_callback_init(&callback);
  EXPECT(lonejson_oidc_authorization_callback_parse_query(
             "?code=abc%2B123&state=state+123", strlen("?code=abc%2B123&state=state+123"),
             "state 123", 0u, &callback, &error) == LONEJSON_STATUS_OK);
  EXPECT(strcmp(callback.code, "abc+123") == 0);
  EXPECT(strcmp(callback.state, "state 123") == 0);
  lonejson_oidc_authorization_callback_cleanup(&callback);

  EXPECT(lonejson_oidc_authorization_callback_parse_query(
             "code=abc&state=wrong", strlen("code=abc&state=wrong"),
             "state", 0u, &callback, &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(callback.code == NULL);
  EXPECT(lonejson_oidc_authorization_callback_parse_query(
             "error=access_denied&state=state",
             strlen("error=access_denied&state=state"), "state", 0u,
             &callback, &error) == LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(lonejson_oidc_authorization_callback_parse_query(
             "code=abc&code=def&state=state",
             strlen("code=abc&code=def&state=state"), "state", 0u,
             &callback, &error) == LONEJSON_STATUS_DUPLICATE_FIELD);
  EXPECT(lonejson_oidc_authorization_callback_parse_query(
             "code=abc&state=%zz", strlen("code=abc&state=%zz"), "state", 0u,
             &callback, &error) == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_oidc_authorization_callback_parse_query(
             "code=abc&state=state", strlen("code=abc&state=state"), "state",
             8u, &callback, &error) == LONEJSON_STATUS_OVERFLOW);
  lonejson_oidc_authorization_callback_cleanup(&callback);
}

static void test_oidc_authorization_bearer_token(void) {
  lonejson_jwt_segment token;
  lonejson_error error;

  lonejson_error_init(&error);
  EXPECT(lonejson_oidc_authorization_bearer_token("  bEaReR \t abc.def.sig  ",
                                                  &token, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(token.len == strlen("abc.def.sig"));
  EXPECT(memcmp(token.data, "abc.def.sig", token.len) == 0);
  EXPECT(lj_oidc_authorization_bearer_token("Bearer abc", &token, &error) ==
         LJ_STATUS_OK);
  EXPECT(token.len == 3u);
  EXPECT(lonejson_oidc_authorization_bearer_token(NULL, &token, &error) ==
         LONEJSON_STATUS_MISSING_REQUIRED_FIELD);
  EXPECT(lonejson_oidc_authorization_bearer_token("Basic abc", &token,
                                                  &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_oidc_authorization_bearer_token("Bearer ", &token, &error) ==
         LONEJSON_STATUS_MISSING_REQUIRED_FIELD);
  EXPECT(lonejson_oidc_authorization_bearer_token("Bearer abc def", &token,
                                                  &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(lonejson_oidc_authorization_bearer_token("Bearer abc", NULL,
                                                  &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
}

static void test_oidc_validate_bearer_token(void) {
  char header[1024];
  lonejson_oidc_jwks_cache cache;
  lonejson_oidc_jwks_cache_policy cache_policy;
  lonejson_jwt_claim_policy claim_policy;
  lonejson_oidc_bearer_validation_request request;
  lonejson_oidc_bearer_validation validation;
  lonejson_error error;

  lonejson_error_init(&error);
  lonejson_oidc_jwks_cache_init(&cache);
  lonejson_oidc_bearer_validation_init(&validation);
  cache_policy = test_oidc_jwks_cache_policy();
  claim_policy = test_jwt_policy();
  EXPECT(lonejson_oidc_jwks_cache_update_json(
             test_default_runtime(), &cache, &cache_policy,
             test_jwt_rs256_jwks_json, strlen(test_jwt_rs256_jwks_json),
             &error) == LONEJSON_STATUS_OK);
  snprintf(header, sizeof(header), "Bearer %s", test_jwt_rs256_token);
  memset(&request, 0, sizeof(request));
  request.authorization_header = header;
  request.jwks_cache = &cache;
  request.jwks_policy = &cache_policy;
  request.claim_policy = &claim_policy;
#ifdef LONEJSON_WITH_OPENSSL
  EXPECT(lonejson_oidc_validate_bearer_token(test_default_runtime(), &request,
                                             &validation, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_NONE);
  EXPECT(strcmp(lonejson_auth_failure_string(validation.failure), "none") ==
         0);
  EXPECT(validation.jwk != NULL);
  EXPECT(strcmp(validation.header.kid, "rsa-test") == 0);
  EXPECT(strcmp(validation.claims.iss, "issuer") == 0);
  lonejson_oidc_bearer_validation_cleanup(&validation);
  lonejson_oidc_bearer_validation_init(&validation);
  EXPECT(test_default_runtime()->oidc_validate_bearer_token != NULL);
  EXPECT(test_default_runtime()->oidc_validate_bearer_token(
             test_default_runtime(), &request, &validation, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_NONE);
  EXPECT(lj_oidc_validate_bearer_token(test_default_runtime(), &request,
                                       &validation, &error) == LJ_STATUS_OK);
#else
  EXPECT(lonejson_oidc_validate_bearer_token(test_default_runtime(), &request,
                                             &validation, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_INVALID_SIGNATURE);
#endif
  lonejson_oidc_bearer_validation_cleanup(&validation);
  lonejson_oidc_jwks_cache_cleanup(&cache);

  lonejson_oidc_jwks_cache_init(&cache);
  lonejson_oidc_bearer_validation_init(&validation);
  EXPECT(lonejson_oidc_jwks_cache_update_json(
             test_default_runtime(), &cache, &cache_policy,
             test_jwt_rs256_minimal_jwks_json,
             strlen(test_jwt_rs256_minimal_jwks_json), &error) ==
         LONEJSON_STATUS_OK);
  request.jwks_cache = &cache;
#ifdef LONEJSON_WITH_OPENSSL
  EXPECT(lonejson_oidc_validate_bearer_token(test_default_runtime(), &request,
                                             &validation, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_NONE);
  EXPECT(validation.jwk != NULL);
  EXPECT(validation.jwk->alg == NULL);
  EXPECT(validation.jwk->use == NULL);
#else
  EXPECT(lonejson_oidc_validate_bearer_token(test_default_runtime(), &request,
                                             &validation, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_INVALID_SIGNATURE);
#endif
  lj_oidc_bearer_validation_cleanup(&validation);
  lonejson_oidc_jwks_cache_cleanup(&cache);
}

static void test_oidc_validate_bearer_token_failures(void) {
  char header[1024];
  char tampered[sizeof(test_jwt_rs256_token)];
  lonejson_oidc_jwks_cache cache;
  lonejson_oidc_jwks_cache_policy cache_policy;
  lonejson_jwt_claim_policy claim_policy;
  lonejson_oidc_bearer_validation_request request;
  lonejson_oidc_bearer_validation validation;
  lonejson_error error;
  const char *bad_issuer = "other";
  const char *bad_audience = "other";

  lonejson_error_init(&error);
  lonejson_oidc_jwks_cache_init(&cache);
  lonejson_oidc_bearer_validation_init(&validation);
  cache_policy = test_oidc_jwks_cache_policy();
  claim_policy = test_jwt_policy();
  memset(&request, 0, sizeof(request));
  request.jwks_cache = &cache;
  request.jwks_policy = &cache_policy;
  request.claim_policy = &claim_policy;

  EXPECT(lonejson_oidc_jwks_cache_update_json(
             test_default_runtime(), &cache, &cache_policy,
             test_jwt_rs256_jwks_json, strlen(test_jwt_rs256_jwks_json),
             &error) == LONEJSON_STATUS_OK);

  request.authorization_header = NULL;
  EXPECT(lonejson_oidc_validate_bearer_token(test_default_runtime(), &request,
                                             &validation, &error) ==
         LONEJSON_STATUS_MISSING_REQUIRED_FIELD);
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_MISSING_CREDENTIALS);

  request.authorization_header = "Bearer not-a-jwt";
  EXPECT(lonejson_oidc_validate_bearer_token(test_default_runtime(), &request,
                                             &validation, &error) ==
         LONEJSON_STATUS_INVALID_JSON);
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_MALFORMED_TOKEN);

  snprintf(header, sizeof(header), "Bearer %s", test_jwt_rs256_token);
  request.authorization_header = header;
  cache_policy.now = 2000;
  EXPECT(lonejson_oidc_validate_bearer_token(test_default_runtime(), &request,
                                             &validation, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_CACHE_UNAVAILABLE);
  cache_policy = test_oidc_jwks_cache_policy();
  request.jwks_policy = &cache_policy;

  lonejson_oidc_jwks_cache_cleanup(&cache);
  lonejson_oidc_jwks_cache_init(&cache);
  EXPECT(lonejson_oidc_jwks_cache_update_json(
             test_default_runtime(), &cache, &cache_policy,
             test_jwt_rs256_other_kid_jwks_json,
             strlen(test_jwt_rs256_other_kid_jwks_json), &error) ==
         LONEJSON_STATUS_OK);
  request.jwks_cache = &cache;
  EXPECT(lonejson_oidc_validate_bearer_token(test_default_runtime(), &request,
                                             &validation, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_KEY_NOT_FOUND);

  lonejson_oidc_jwks_cache_cleanup(&cache);
  lonejson_oidc_jwks_cache_init(&cache);
  EXPECT(lonejson_oidc_jwks_cache_update_json(
             test_default_runtime(), &cache, &cache_policy,
             test_jwt_rs256_jwks_json, strlen(test_jwt_rs256_jwks_json),
             &error) == LONEJSON_STATUS_OK);
  request.jwks_cache = &cache;
  memcpy(tampered, test_jwt_rs256_token, sizeof(tampered));
  tampered[sizeof(tampered) - 2u] =
      tampered[sizeof(tampered) - 2u] == 'A' ? 'B' : 'A';
  snprintf(header, sizeof(header), "Bearer %s", tampered);
  EXPECT(lonejson_oidc_validate_bearer_token(test_default_runtime(), &request,
                                             &validation, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_INVALID_SIGNATURE);

  snprintf(header, sizeof(header), "Bearer %s", test_jwt_rs256_token);
  claim_policy = test_jwt_policy();
  claim_policy.now = 3000;
  request.claim_policy = &claim_policy;
  EXPECT(lonejson_oidc_validate_bearer_token(test_default_runtime(), &request,
                                             &validation, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
#ifdef LONEJSON_WITH_OPENSSL
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_EXPIRED_TOKEN);
#else
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_INVALID_SIGNATURE);
#endif

  claim_policy = test_jwt_policy();
  claim_policy.accepted_issuers = &bad_issuer;
  request.claim_policy = &claim_policy;
  EXPECT(lonejson_oidc_validate_bearer_token(test_default_runtime(), &request,
                                             &validation, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
#ifdef LONEJSON_WITH_OPENSSL
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_ISSUER_MISMATCH);
#else
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_INVALID_SIGNATURE);
#endif

  claim_policy = test_jwt_policy();
  claim_policy.accepted_audiences = &bad_audience;
  request.claim_policy = &claim_policy;
  EXPECT(lonejson_oidc_validate_bearer_token(test_default_runtime(), &request,
                                             &validation, &error) ==
         LONEJSON_STATUS_TYPE_MISMATCH);
#ifdef LONEJSON_WITH_OPENSSL
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_AUDIENCE_MISMATCH);
#else
  EXPECT(validation.failure == LONEJSON_AUTH_FAILURE_INVALID_SIGNATURE);
#endif

  lonejson_oidc_bearer_validation_cleanup(&validation);
  lonejson_oidc_jwks_cache_cleanup(&cache);
}
#else
static void test_oidc_discovery_url(void) {}
static void test_oidc_discovery_parse_and_validate(void) {}
static void test_oidc_discovery_failures(void) {}
static void test_oidc_jwks_cache_update_and_select(void) {}
static void test_oidc_jwks_cache_failure_modes(void) {}
static void test_oidc_jwks_cache_curl_adapter(void) {}
static void test_oidc_http_provider_helpers(void) {}
static void test_oauth2_client_credentials_body(void) {}
static void test_oauth2_token_response_parse(void) {}
static void test_oauth2_token_response_failures(void) {}
static void test_oidc_pkce_challenge_and_generate(void) {}
static void test_oidc_authorization_url(void) {}
static void test_oidc_authorization_callback_parse(void) {}
static void test_oidc_authorization_bearer_token(void) {}
static void test_oidc_validate_bearer_token(void) {}
static void test_oidc_validate_bearer_token_failures(void) {}
#endif
#else
static void test_jwt_base64url_decode_vectors(void) {}
static void test_jwt_compact_parse_segments(void) {}
static void test_jwt_compact_parse_failures(void) {}
static void test_jwk_parse_json_shapes(void) {}
static void test_jwks_parse_and_select(void) {}
static void test_jwk_parse_failures(void) {}
static void test_jwt_decode_and_validate_claims(void) {}
static void test_jwt_validate_rs256_signature(void) {}
static void test_jwt_validate_recommended_signatures(void) {}
static void test_jwt_auth_provider_runtime_boundary(void) {}
static void test_jwt_validate_signature_failures(void) {}
static void test_jwt_claim_validation_failures(void) {}
static void test_jwt_decode_claim_failures(void) {}
static void test_oidc_discovery_url(void) {}
static void test_oidc_discovery_parse_and_validate(void) {}
static void test_oidc_discovery_failures(void) {}
static void test_oidc_jwks_cache_update_and_select(void) {}
static void test_oidc_jwks_cache_failure_modes(void) {}
static void test_oidc_jwks_cache_curl_adapter(void) {}
static void test_oidc_http_provider_helpers(void) {}
static void test_oauth2_client_credentials_body(void) {}
static void test_oauth2_token_response_parse(void) {}
static void test_oauth2_token_response_failures(void) {}
static void test_oidc_pkce_challenge_and_generate(void) {}
static void test_oidc_authorization_url(void) {}
static void test_oidc_authorization_callback_parse(void) {}
static void test_oidc_authorization_bearer_token(void) {}
static void test_oidc_validate_bearer_token(void) {}
static void test_oidc_validate_bearer_token_failures(void) {}
#endif
