#include <stdio.h>
#include <string.h>

#include "lonejson.h"

typedef struct response_payload {
  char status[16];
  lj_json_value payload;
} response_payload;

typedef struct event_doc {
  char type[32];
  response_payload response;
} event_doc;

static const lj_field response_payload_fields[] = {
    LJ_FIELD_STRING_FIXED(response_payload, status, "status",
                          LJ_OVERFLOW_FAIL),
    LJ_FIELD_JSON_VALUE_REQ(response_payload, payload, "payload")};
LJ_MAP_DEFINE(response_payload_map, response_payload, response_payload_fields);

static const lj_field event_doc_fields[] = {
    LJ_FIELD_STRING_FIXED_REQ(event_doc, type, "type", LJ_OVERFLOW_FAIL),
    LJ_FIELD_OBJECT_REQ(event_doc, response, "response", &response_payload_map)};
LJ_MAP_DEFINE(event_doc_map, event_doc, event_doc_fields);

typedef struct buffer_sink {
  unsigned char *buffer;
  size_t capacity;
  size_t length;
} buffer_sink;

static lj_status buffer_sink_write(void *user, const void *data, size_t len,
                                   lj_error *error) {
  buffer_sink *sink = (buffer_sink *)user;
  const unsigned char *bytes = (const unsigned char *)data;

  (void)error;

  if (sink == NULL || sink->buffer == NULL || sink->capacity == 0u) {
    return LJ_STATUS_INVALID_ARGUMENT;
  }
  if (sink->length + len + 1u > sink->capacity) {
    return LJ_STATUS_OVERFLOW;
  }
  memcpy(sink->buffer + sink->length, bytes, len);
  sink->length += len;
  sink->buffer[sink->length] = '\0';
  return LJ_STATUS_OK;
}

int main(void) {
  static const char first_json[] =
      "{\"type\":\"response.completed\","
      "\"response\":{\"status\":\"ok\",\"payload\":{\"a\":[1,2,3],"
      "\"mode\":\"full\"}}}";
  static const char second_json[] =
      "{\"type\":\"response.delta\","
      "\"response\":{\"payload\":{\"delta\":[true,null,2]}}}";
  event_doc doc;
  lj_parse_options options;
  lj_error error;
  lj_status status;
  unsigned char payload_bytes[256];
  buffer_sink sink;

  memset(&sink, 0, sizeof(sink));
  sink.buffer = payload_bytes;
  sink.capacity = sizeof(payload_bytes);

  lj_init(&event_doc_map, &doc);
  status =
      lj_json_value_set_parse_sink(&doc.response.payload, buffer_sink_write,
                                   &sink, &error);
  if (status != LJ_STATUS_OK) {
    fprintf(stderr, "payload sink setup failed: %s\n", error.message);
    return 1;
  }

  options = lj_default_parse_options();
  options.clear_destination = 0;

  status = lj_parse_cstr(&event_doc_map, &doc, first_json, &options, &error);
  if (status != LJ_STATUS_OK) {
    fprintf(stderr, "first parse failed: %s\n", error.message);
    lj_cleanup(&event_doc_map, &doc);
    return 1;
  }

  printf("first type=%s status=%s payload=%s\n", doc.type, doc.response.status,
         payload_bytes);

  sink.length = 0u;
  payload_bytes[0] = '\0';

  status = lj_parse_cstr(&event_doc_map, &doc, second_json, &options, &error);
  if (status != LJ_STATUS_OK) {
    fprintf(stderr, "second parse failed: %s\n", error.message);
    lj_cleanup(&event_doc_map, &doc);
    return 1;
  }

  printf("second type=%s status=%s payload=%s\n", doc.type, doc.response.status,
         payload_bytes);

  lj_cleanup(&event_doc_map, &doc);
  return 0;
}
