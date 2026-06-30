static const char lonejson__base64_std_alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char lonejson__base64_url_alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static LONEJSON__INLINE int
lonejson__base64_is_url(lonejson_base64_variant variant) {
  return variant == LONEJSON_BASE64_URL || variant == LONEJSON_BASE64_URL_RAW;
}

static LONEJSON__INLINE int
lonejson__base64_is_raw(lonejson_base64_variant variant) {
  return variant == LONEJSON_BASE64_STANDARD_RAW ||
         variant == LONEJSON_BASE64_URL_RAW;
}

static const char *lonejson__base64_alphabet(lonejson_base64_variant variant) {
  return lonejson__base64_is_url(variant) ? lonejson__base64_url_alphabet
                                          : lonejson__base64_std_alphabet;
}

static int lonejson__base64_variant_valid(lonejson_base64_variant variant) {
  return variant == LONEJSON_BASE64_STANDARD ||
         variant == LONEJSON_BASE64_STANDARD_RAW ||
         variant == LONEJSON_BASE64_URL || variant == LONEJSON_BASE64_URL_RAW;
}

static int lonejson__base64_value_for_variant(unsigned char ch,
                                              lonejson_base64_variant variant) {
  if (ch >= (unsigned char)'A' && ch <= (unsigned char)'Z') {
    return (int)(ch - (unsigned char)'A');
  }
  if (ch >= (unsigned char)'a' && ch <= (unsigned char)'z') {
    return (int)(ch - (unsigned char)'a') + 26;
  }
  if (ch >= (unsigned char)'0' && ch <= (unsigned char)'9') {
    return (int)(ch - (unsigned char)'0') + 52;
  }
  if (lonejson__base64_is_url(variant)) {
    if (ch == (unsigned char)'-') {
      return 62;
    }
    if (ch == (unsigned char)'_') {
      return 63;
    }
  } else {
    if (ch == (unsigned char)'+') {
      return 62;
    }
    if (ch == (unsigned char)'/') {
      return 63;
    }
  }
  return -1;
}

static lonejson_status
lonejson__base64_require_variant(lonejson_base64_variant variant,
                                 lonejson_error *error) {
  if (!lonejson__base64_variant_valid(variant)) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "base64 variant is invalid");
  }
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_base64_encoded_len(size_t len,
                                            lonejson_base64_variant variant,
                                            size_t *out_len,
                                            lonejson_error *error) {
  size_t groups;
  size_t rem;
  size_t encoded;

  lonejson__clear_error(error);
  if (out_len == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "base64 encoded length output is required");
  }
  if (lonejson__base64_require_variant(variant, error) != LONEJSON_STATUS_OK) {
    *out_len = 0u;
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  groups = len / 3u;
  rem = len % 3u;
  if (groups > (SIZE_MAX / 4u)) {
    *out_len = 0u;
    return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                               "base64 encoded length overflows");
  }
  encoded = groups * 4u;
  if (rem != 0u) {
    if (lonejson__base64_is_raw(variant)) {
      encoded += rem + 1u;
    } else {
      if (encoded > SIZE_MAX - 4u) {
        *out_len = 0u;
        return lonejson__set_error(error, LONEJSON_STATUS_OVERFLOW, 0u, 0u, 0u,
                                   "base64 encoded length overflows");
      }
      encoded += 4u;
    }
  }
  *out_len = encoded;
  return LONEJSON_STATUS_OK;
}

static lonejson_status lonejson__base64_emit_quad(
    const unsigned char *data, size_t len, lonejson_base64_variant variant,
    lonejson_sink_fn sink, void *user, lonejson_error *error) {
  const char *alphabet;
  char out[4];
  size_t out_len;

  alphabet = lonejson__base64_alphabet(variant);
  out[0] = alphabet[(data[0] >> 2u) & 0x3Fu];
  out[1] =
      alphabet[((data[0] & 0x03u) << 4u) | (((len > 1u) ? data[1] : 0u) >> 4u)];
  out_len = 2u;
  if (len > 1u) {
    out[2] = alphabet[((data[1] & 0x0Fu) << 2u) |
                      (((len > 2u) ? data[2] : 0u) >> 6u)];
    out_len = 3u;
  } else if (!lonejson__base64_is_raw(variant)) {
    out[2] = '=';
    out_len = 3u;
  }
  if (len > 2u) {
    out[3] = alphabet[data[2] & 0x3Fu];
    out_len = 4u;
  } else if (!lonejson__base64_is_raw(variant)) {
    out[3] = '=';
    out_len = 4u;
  }
  return sink(user, out, out_len, error);
}

lonejson_status lonejson_base64_encode_sink(const void *data, size_t len,
                                            lonejson_base64_variant variant,
                                            lonejson_sink_fn sink, void *user,
                                            lonejson_error *error) {
  const unsigned char *bytes;
  size_t offset;
  lonejson_status status;

  lonejson__clear_error(error);
  status = lonejson__base64_require_variant(variant, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (sink == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "base64 sink is required");
  }
  if (data == NULL && len != 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "base64 input is required");
  }
  bytes = (const unsigned char *)data;
  offset = 0u;
  while (offset + 3u <= len) {
    status = lonejson__base64_emit_quad(bytes + offset, 3u, variant, sink, user,
                                        error);
    if (status != LONEJSON_STATUS_OK) {
      return status;
    }
    offset += 3u;
  }
  if (offset < len) {
    return lonejson__base64_emit_quad(bytes + offset, len - offset, variant,
                                      sink, user, error);
  }
  return LONEJSON_STATUS_OK;
}

typedef struct lonejson__base64_buffer_sink {
  char *out;
  size_t capacity;
  size_t len;
} lonejson__base64_buffer_sink;

static lonejson_status lonejson__base64_write_buffer(void *user,
                                                     const void *data,
                                                     size_t len,
                                                     lonejson_error *error) {
  lonejson__base64_buffer_sink *sink = (lonejson__base64_buffer_sink *)user;

  (void)error;
  memcpy(sink->out + sink->len, data, len);
  sink->len += len;
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_base64_encode(const void *data, size_t len,
                                       lonejson_base64_variant variant,
                                       char *out, size_t capacity,
                                       size_t *needed, lonejson_error *error) {
  lonejson__base64_buffer_sink sink;
  lonejson_status status;
  size_t encoded_len;

  lonejson__clear_error(error);
  if (needed == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "base64 needed output is required");
  }
  status = lonejson_base64_encoded_len(len, variant, &encoded_len, error);
  if (status != LONEJSON_STATUS_OK) {
    *needed = 0u;
    return status;
  }
  *needed = encoded_len;
  if (encoded_len > capacity) {
    return lonejson__set_error(error, LONEJSON_STATUS_TRUNCATED, 0u, 0u, 0u,
                               "base64 output buffer is too small");
  }
  if (encoded_len != 0u && out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "base64 output buffer is required");
  }
  sink.out = out;
  sink.capacity = capacity;
  sink.len = 0u;
  status = lonejson_base64_encode_sink(
      data, len, variant, lonejson__base64_write_buffer, &sink, error);
  (void)sink.capacity;
  return status;
}

static lonejson_status lonejson__base64_check(const char *data, size_t len,
                                              lonejson_base64_variant variant,
                                              size_t *out_len,
                                              lonejson_error *error) {
  size_t i;
  size_t decoded_len = 0u;
  size_t quartet_len = 0u;
  int padding = 0;
  int saw_padding = 0;
  int raw;

  if (out_len == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "base64 decoded length output is required");
  }
  if (data == NULL && len != 0u) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "base64 input is required");
  }
  if (lonejson__base64_require_variant(variant, error) != LONEJSON_STATUS_OK) {
    *out_len = 0u;
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  raw = lonejson__base64_is_raw(variant);
  for (i = 0u; i < len; ++i) {
    unsigned char ch = (unsigned char)data[i];
    if (ch == (unsigned char)'=') {
      if (raw) {
        return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, i, 0u,
                                   0u, "base64 raw variant rejects padding");
      }
      saw_padding = 1;
      ++padding;
      if (padding > 2 || quartet_len < 2u) {
        return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, i, 0u,
                                   0u, "invalid base64 padding");
      }
    } else {
      if (saw_padding) {
        return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, i, 0u,
                                   0u, "non-padding base64 data after padding");
      }
      if (lonejson__base64_value_for_variant(ch, variant) < 0) {
        return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, i, 0u,
                                   0u, "invalid base64 character");
      }
    }
    ++quartet_len;
    if (quartet_len == 4u) {
      decoded_len += 3u - (size_t)padding;
      quartet_len = 0u;
      padding = 0;
    } else if (saw_padding && quartet_len == 0u) {
      return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, i, 0u, 0u,
                                 "invalid base64 padding");
    }
  }
  if (quartet_len != 0u) {
    if (!raw) {
      return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, len, 0u,
                                 0u, "incomplete padded base64 quartet");
    }
    if (quartet_len == 1u) {
      return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, len, 0u,
                                 0u, "invalid raw base64 length");
    }
    decoded_len += quartet_len - 1u;
  } else if (saw_padding && padding != 0) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_JSON, len, 0u, 0u,
                               "invalid base64 padding");
  }
  *out_len = decoded_len;
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_base64_decoded_len(const char *data, size_t len,
                                            lonejson_base64_variant variant,
                                            size_t *out_len,
                                            lonejson_error *error) {
  lonejson__clear_error(error);
  return lonejson__base64_check(data, len, variant, out_len, error);
}

static lonejson_status
lonejson__base64_decode_emit(const int values[4], size_t group_len,
                             size_t out_len, lonejson_sink_fn sink, void *user,
                             lonejson_error *error) {
  unsigned int bits;
  unsigned char out[3];

  bits = ((unsigned int)values[0] << 18) | ((unsigned int)values[1] << 12) |
         ((unsigned int)values[2] << 6) | (unsigned int)values[3];
  if (group_len >= 2u && out_len >= 1u) {
    out[0] = (unsigned char)((bits >> 16) & 0xffu);
  }
  if (group_len >= 3u && out_len >= 2u) {
    out[1] = (unsigned char)((bits >> 8) & 0xffu);
  }
  if (group_len == 4u && out_len == 3u) {
    out[2] = (unsigned char)(bits & 0xffu);
  }
  return out_len == 0u ? LONEJSON_STATUS_OK : sink(user, out, out_len, error);
}

lonejson_status lonejson_base64_decode_sink(const char *data, size_t len,
                                            lonejson_base64_variant variant,
                                            lonejson_sink_fn sink, void *user,
                                            lonejson_error *error) {
  size_t decoded_len;
  size_t i;
  int values[4] = {0, 0, 0, 0};
  size_t group_len = 0u;
  int padding = 0;
  lonejson_status status;

  lonejson__clear_error(error);
  status = lonejson__base64_check(data, len, variant, &decoded_len, error);
  if (status != LONEJSON_STATUS_OK) {
    return status;
  }
  if (sink == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "base64 sink is required");
  }
  for (i = 0u; i < len; ++i) {
    unsigned char ch = (unsigned char)data[i];
    if (ch == (unsigned char)'=') {
      values[group_len++] = 0;
      ++padding;
    } else {
      values[group_len++] = lonejson__base64_value_for_variant(ch, variant);
    }
    if (group_len == 4u) {
      status = lonejson__base64_decode_emit(values, group_len, 3u - padding,
                                            sink, user, error);
      if (status != LONEJSON_STATUS_OK) {
        return status;
      }
      values[0] = values[1] = values[2] = values[3] = 0;
      group_len = 0u;
      padding = 0;
    }
  }
  if (group_len != 0u) {
    status = lonejson__base64_decode_emit(values, group_len, group_len - 1u,
                                          sink, user, error);
  }
  return status;
}

typedef struct lonejson__base64_decode_buffer_sink {
  unsigned char *out;
  size_t len;
} lonejson__base64_decode_buffer_sink;

static lonejson_status
lonejson__base64_write_decode_buffer(void *user, const void *data, size_t len,
                                     lonejson_error *error) {
  lonejson__base64_decode_buffer_sink *sink =
      (lonejson__base64_decode_buffer_sink *)user;

  (void)error;
  memcpy(sink->out + sink->len, data, len);
  sink->len += len;
  return LONEJSON_STATUS_OK;
}

lonejson_status lonejson_base64_decode(const char *data, size_t len,
                                       lonejson_base64_variant variant,
                                       unsigned char *out, size_t capacity,
                                       size_t *needed, lonejson_error *error) {
  lonejson__base64_decode_buffer_sink sink;
  lonejson_status status;
  size_t decoded_len;

  lonejson__clear_error(error);
  if (needed == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "base64 needed output is required");
  }
  status = lonejson_base64_decoded_len(data, len, variant, &decoded_len, error);
  if (status != LONEJSON_STATUS_OK) {
    *needed = 0u;
    return status;
  }
  *needed = decoded_len;
  if (decoded_len > capacity) {
    return lonejson__set_error(error, LONEJSON_STATUS_TRUNCATED, 0u, 0u, 0u,
                               "base64 output buffer is too small");
  }
  if (decoded_len != 0u && out == NULL) {
    return lonejson__set_error(error, LONEJSON_STATUS_INVALID_ARGUMENT, 0u, 0u,
                               0u, "base64 output buffer is required");
  }
  sink.out = out;
  sink.len = 0u;
  return lonejson_base64_decode_sink(
      data, len, variant, lonejson__base64_write_decode_buffer, &sink, error);
}
