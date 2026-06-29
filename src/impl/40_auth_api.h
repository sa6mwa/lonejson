#ifdef LONEJSON_WITH_JWT
static int lonejson__base64url_value(unsigned char ch) {
  if (ch >= (unsigned char)'A' && ch <= (unsigned char)'Z') {
    return (int)(ch - (unsigned char)'A');
  }
  if (ch >= (unsigned char)'a' && ch <= (unsigned char)'z') {
    return (int)(ch - (unsigned char)'a') + 26;
  }
  if (ch >= (unsigned char)'0' && ch <= (unsigned char)'9') {
    return (int)(ch - (unsigned char)'0') + 52;
  }
  if (ch == (unsigned char)'-') {
    return 62;
  }
  if (ch == (unsigned char)'_') {
    return 63;
  }
  return -1;
}

static lonejson_status
lonejson__base64url_check(const char *data, size_t len, size_t *out_len,
                          lonejson_error *error) {
  size_t i;
  size_t rem;
  size_t decoded_len;

  if (data == NULL && len != 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "base64url input is required");
  }
  if (out_len == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "base64url decoded length output is required");
  }
  for (i = 0u; i < len; ++i) {
    if (lonejson__base64url_value((unsigned char)data[i]) < 0) {
      return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, i, 0u,
                                 0u, "invalid base64url character");
    }
  }
  rem = len & 3u;
  if (rem == 1u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, len, 0u, 0u,
                               "invalid base64url segment length");
  }
  decoded_len = (len / 4u) * 3u;
  if (rem == 2u) {
    decoded_len += 1u;
  } else if (rem == 3u) {
    decoded_len += 2u;
  }
  *out_len = decoded_len;
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_base64url_decoded_len(const char *data, size_t len,
                                               size_t *out_len,
                                               lonejson_error *error) {
  lonejson__clear_error(error);
  return lonejson__base64url_check(data, len, out_len, error);
}

lonejson_status lonejson_base64url_decode(const char *data, size_t len,
                                          unsigned char *out, size_t capacity,
                                          size_t *needed,
                                          lonejson_error *error) {
  size_t decoded_len;
  size_t in_pos;
  size_t out_pos;
  lonejson_status status;

  lonejson__clear_error(error);
  if (needed == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "base64url needed output is required");
  }
  status = lonejson__base64url_check(data, len, &decoded_len, error);
  if (status != LONEJSON_STATUS_OK) {
    *needed = 0u;
    return status;
  }
  *needed = decoded_len;
  if (decoded_len > capacity) {
    return lonejson__set_error(error, LONEJSON_STATUS_TRUNCATED, 0u, 0u, 0u,
                               "base64url output buffer is too small");
  }
  if (decoded_len != 0u && out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "base64url output buffer is required");
  }

  in_pos = 0u;
  out_pos = 0u;
  while (in_pos < len) {
    int values[4];
    size_t group_len;
    unsigned int bits;

    group_len = len - in_pos;
    if (group_len > 4u) {
      group_len = 4u;
    }
    values[0] = lonejson__base64url_value((unsigned char)data[in_pos]);
    values[1] =
        (group_len > 1u)
            ? lonejson__base64url_value((unsigned char)data[in_pos + 1u])
            : 0;
    values[2] =
        (group_len > 2u)
            ? lonejson__base64url_value((unsigned char)data[in_pos + 2u])
            : 0;
    values[3] =
        (group_len > 3u)
            ? lonejson__base64url_value((unsigned char)data[in_pos + 3u])
            : 0;
    bits = ((unsigned int)values[0] << 18) |
           ((unsigned int)values[1] << 12) |
           ((unsigned int)values[2] << 6) | (unsigned int)values[3];
    if (group_len >= 2u) {
      out[out_pos++] = (unsigned char)((bits >> 16) & 0xffu);
    }
    if (group_len >= 3u) {
      out[out_pos++] = (unsigned char)((bits >> 8) & 0xffu);
    }
    if (group_len == 4u) {
      out[out_pos++] = (unsigned char)(bits & 0xffu);
    }
    in_pos += group_len;
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__jwt_check_segment(const char *token,
                                                   size_t begin, size_t end,
                                                   int allow_empty,
                                                   lonejson_error *error) {
  size_t decoded_len;
  lonejson_status status;

  if (end < begin) {
    return lonejson__set_error(error, LONEJSON_STATUS_INTERNAL_ERROR, begin, 0u,
                               0u, "invalid JWT segment bounds");
  }
  if (!allow_empty && end == begin) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, begin, 0u,
                               0u, "JWT compact segment must not be empty");
  }
  status =
      lonejson__base64url_check(token + begin, end - begin, &decoded_len, error);
  if (status != LONEJSON_STATUS_OK) {
    if (error != NULL) {
      error->offset += begin;
    }
    return status;
  }
  (void)decoded_len;
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_jwt_parse_compact(const char *token, size_t len,
                                           lonejson_jwt_compact *out,
                                           lonejson_error *error) {
  size_t i;
  size_t first_dot;
  size_t second_dot;
  int seen_first;
  int seen_second;
  lonejson_status status;

  lonejson__clear_error(error);
  if (token == NULL || out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "JWT compact token and output are required");
  }
  memset(out, 0, sizeof(*out));
  first_dot = 0u;
  second_dot = 0u;
  seen_first = 0;
  seen_second = 0;
  for (i = 0u; i < len; ++i) {
    if (token[i] == '.') {
      if (!seen_first) {
        first_dot = i;
        seen_first = 1;
      } else if (!seen_second) {
        second_dot = i;
        seen_second = 1;
      } else {
        return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, i, 0u,
                                   0u, "JWT compact token has too many segments");
      }
    }
  }
  if (!seen_first || !seen_second) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, len, 0u, 0u,
                               "JWT compact token must contain three segments");
  }
  status = lonejson__jwt_check_segment(token, 0u, first_dot, 0, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status =
      lonejson__jwt_check_segment(token, first_dot + 1u, second_dot, 0, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  status = lonejson__jwt_check_segment(token, second_dot + 1u, len, 1, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  out->header.data = token;
  out->header.len = first_dot;
  out->payload.data = token + first_dot + 1u;
  out->payload.len = second_dot - first_dot - 1u;
  out->signature.data = token + second_dot + 1u;
  out->signature.len = len - second_dot - 1u;
  out->signing_input.data = token;
  out->signing_input.len = second_dot;
  return LONEJSON_STATUS_OK;
}
#endif
