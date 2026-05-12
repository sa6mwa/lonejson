#ifdef LONEJSON_WITH_CURL
lonejson_status
lonejson_curl_parse_init(lonejson_curl_parse *ctx, const lonejson_map *map,
                         void *dst, const lonejson_parse_options *options) {
  if (ctx == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  memset(ctx, 0, sizeof(*ctx));
  ctx->parser = lonejson__parser_create_ex(map, dst, options, &ctx->error, 0);
  return ctx->parser ? LONEJSON_STATUS_OK : ctx->error.code;
}

size_t lonejson_curl_write_callback(char *ptr, size_t size, size_t nmemb,
                                    void *userdata) {
  lonejson_curl_parse *ctx = (lonejson_curl_parse *)userdata;
  size_t bytes = size * nmemb;
  lonejson_status status;
  if (ctx == NULL || ctx->parser == NULL) {
    return 0u;
  }
  status = lonejson_parser_feed(ctx->parser, ptr, bytes);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    return bytes;
  }
  ctx->error = ((lonejson_parser *)ctx->parser)->error;
  return 0u;
}

lonejson_status lonejson_curl_parse_finish(lonejson_curl_parse *ctx) {
  if (ctx == NULL || ctx->parser == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  ctx->error.code = lonejson_parser_finish(ctx->parser);
  ctx->error = ((lonejson_parser *)ctx->parser)->error;
  return ctx->error.code;
}

void lonejson_curl_parse_cleanup(lonejson_curl_parse *ctx) {
  if (ctx == NULL) {
    return;
  }
  lonejson_parser_destroy(ctx->parser);
  ctx->parser = NULL;
}

lonejson_status lonejson_curl_array_parse_init(
    lonejson_curl_array_parse *ctx, const char *path, const lonejson_map *map,
    void *dst, const lonejson_parse_options *options,
    lonejson_array_stream_item_fn callback, void *user) {
  if (ctx == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  memset(ctx, 0, sizeof(*ctx));
  if (map == NULL || dst == NULL || callback == NULL) {
    return lonejson__set_error(&ctx->error, LONEJSON_STATUS_INVALID_ARGUMENT,
                               0u, 0u, 0u,
                               "map, destination, and callback are required");
  }
  ctx->map = map;
  ctx->dst = dst;
  ctx->callback = callback;
  ctx->user = user;
  ctx->stream = lonejson_array_stream_open_push(path, options, &ctx->error);
  return ctx->stream ? LONEJSON_STATUS_OK : ctx->error.code;
}

size_t lonejson_curl_array_write_callback(char *ptr, size_t size,
                                          size_t nmemb, void *userdata) {
  lonejson_curl_array_parse *ctx = (lonejson_curl_array_parse *)userdata;
  size_t bytes = size * nmemb;
  lonejson_status status;

  if (ctx == NULL || ctx->stream == NULL) {
    return 0u;
  }
  if (bytes != 0u && ptr == NULL) {
    return 0u;
  }
  status = lonejson_array_stream_push(ctx->stream, ctx->map, ctx->dst, ptr,
                                      bytes, ctx->callback, ctx->user,
                                      &ctx->error);
  return (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED)
             ? bytes
             : 0u;
}

lonejson_status
lonejson_curl_array_parse_finish(lonejson_curl_array_parse *ctx) {
  if (ctx == NULL || ctx->stream == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  return lonejson_array_stream_finish(ctx->stream, ctx->map, ctx->dst,
                                      ctx->callback, ctx->user, &ctx->error);
}

void lonejson_curl_array_parse_cleanup(lonejson_curl_array_parse *ctx) {
  if (ctx == NULL) {
    return;
  }
  lonejson_array_stream_close(ctx->stream);
  ctx->stream = NULL;
}

lonejson_status
lonejson_curl_upload_init(lonejson_curl_upload *ctx, const lonejson_map *map,
                          const void *src,
                          const lonejson_write_options *options) {
  if (ctx == NULL) {
    return LONEJSON_STATUS_INVALID_ARGUMENT;
  }
  memset(ctx, 0, sizeof(*ctx));
  if (lonejson_generator_init(&ctx->generator, map, src, options) !=
      LONEJSON_STATUS_OK) {
    return ctx->generator.error.code;
  }
  return LONEJSON_STATUS_OK;
}

size_t lonejson_curl_read_callback(char *ptr, size_t size, size_t nmemb,
                                   void *userdata) {
  lonejson_curl_upload *ctx = (lonejson_curl_upload *)userdata;
  lonejson_status status;
  size_t out_len;
  int eof;
  size_t capacity = size * nmemb;

  if (ctx == NULL || ptr == NULL) {
    return CURL_READFUNC_ABORT;
  }
  if (ctx->generator.state == NULL) {
    return CURL_READFUNC_ABORT;
  }
  if (capacity == 0u) {
    return 0u;
  }
  status = lonejson_generator_read(&ctx->generator, (unsigned char *)ptr,
                                   capacity, &out_len, &eof);
  if (status != LONEJSON_STATUS_OK) {
    return CURL_READFUNC_ABORT;
  }
  return out_len;
}

curl_off_t lonejson_curl_upload_size(const lonejson_curl_upload *ctx) {
  (void)ctx;
  return (curl_off_t)-1;
}

void lonejson_curl_upload_cleanup(lonejson_curl_upload *ctx) {
  if (ctx == NULL) {
    return;
  }
  lonejson_generator_cleanup(&ctx->generator);
}
#endif
