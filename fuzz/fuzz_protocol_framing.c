#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lonejson.h"

typedef struct fuzz_framing_state {
  size_t events;
  size_t parts;
  size_t bytes;
} fuzz_framing_state;

static lonejson_status fuzz_sse_begin(void *user,
                                      const lonejson_sse_event *event,
                                      lonejson_error *error) {
  fuzz_framing_state *state = (fuzz_framing_state *)user;
  (void)event;
  (void)error;
  state->events++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_sse_data(void *user, const void *bytes, size_t len,
                                     lonejson_error *error) {
  fuzz_framing_state *state = (fuzz_framing_state *)user;
  (void)bytes;
  (void)error;
  state->bytes += len;
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_sse_end(void *user, const lonejson_sse_event *event,
                                    lonejson_error *error) {
  (void)user;
  (void)event;
  (void)error;
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_multipart_begin(void *user,
                                            const lonejson_multipart_part *part,
                                            lonejson_error *error) {
  fuzz_framing_state *state = (fuzz_framing_state *)user;
  (void)part;
  (void)error;
  state->parts++;
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_multipart_data(void *user, const void *bytes,
                                           size_t len, lonejson_error *error) {
  fuzz_framing_state *state = (fuzz_framing_state *)user;
  (void)bytes;
  (void)error;
  state->bytes += len;
  return LONEJSON_STATUS_OK;
}

static lonejson_status fuzz_multipart_end(void *user,
                                          const lonejson_multipart_part *part,
                                          lonejson_error *error) {
  (void)user;
  (void)part;
  (void)error;
  return LONEJSON_STATUS_OK;
}

static void fuzz_sse(const uint8_t *data, size_t size) {
  lonejson_sse_options options = lonejson_default_sse_options();
  lonejson_sse_handler handler;
  fuzz_framing_state state;
  lonejson_sse *sse;
  lonejson_error error;

  options.max_line_bytes = 4096u;
  options.max_event_data_bytes = 65536u;
  options.max_buffered_bytes = 65536u;
  memset(&handler, 0, sizeof(handler));
  handler.begin_event = fuzz_sse_begin;
  handler.data_chunk = fuzz_sse_data;
  handler.end_event = fuzz_sse_end;
  memset(&state, 0, sizeof(state));
  sse = lonejson_sse_open(&options, &error);
  if (sse == NULL) {
    return;
  }
  (void)lonejson_sse_push(sse, data, size, &handler, &state, &error);
  (void)lonejson_sse_finish(sse, &handler, &state, &error);
  lonejson_sse_close(sse);
}

static void fuzz_multipart(const uint8_t *data, size_t size) {
  lonejson_multipart_options options = lonejson_default_multipart_options();
  lonejson_multipart_handler handler;
  fuzz_framing_state state;
  lonejson_multipart *mp;
  lonejson_error error;

  options.max_boundary_bytes = 64u;
  options.max_header_line_bytes = 4096u;
  options.max_header_count = 32u;
  options.max_part_buffered_bytes = 65536u;
  memset(&handler, 0, sizeof(handler));
  handler.begin_part = fuzz_multipart_begin;
  handler.part_data = fuzz_multipart_data;
  handler.end_part = fuzz_multipart_end;
  memset(&state, 0, sizeof(state));
  mp = lonejson_multipart_open("multipart/mixed; boundary=ljboundary", &options,
                               &error);
  if (mp == NULL) {
    return;
  }
  (void)lonejson_multipart_push(mp, data, size, &handler, &state, &error);
  (void)lonejson_multipart_finish(mp, &handler, &state, &error);
  lonejson_multipart_close(mp);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size > 0u && data[0] == (uint8_t)'M') {
    fuzz_multipart(data + 1u, size - 1u);
  } else if (size > 0u && data[0] == (uint8_t)'S') {
    fuzz_sse(data + 1u, size - 1u);
  } else {
    fuzz_sse(data, size);
  }
  return 0;
}
