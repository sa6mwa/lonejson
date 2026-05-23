#include <stdio.h>
#include <string.h>

#include "lonejson.h"

typedef struct visit_log {
  char text[2048];
  size_t len;
} visit_log;

static lj_status visit_append(visit_log *log, const char *text) {
  size_t len = strlen(text);

  if (log->len + len + 1u >= sizeof(log->text)) {
    return LJ_STATUS_OVERFLOW;
  }
  memcpy(log->text + log->len, text, len);
  log->len += len;
  log->text[log->len] = '\0';
  return LJ_STATUS_OK;
}

static lj_status on_object_begin(void *user, lj_error *error) {
  (void)error;
  return visit_append((visit_log *)user, "{");
}

static lj_status on_object_end(void *user, lj_error *error) {
  (void)error;
  return visit_append((visit_log *)user, "}");
}

static lj_status on_array_begin(void *user, lj_error *error) {
  (void)error;
  return visit_append((visit_log *)user, "[");
}

static lj_status on_array_end(void *user, lj_error *error) {
  (void)error;
  return visit_append((visit_log *)user, "]");
}

static lj_status on_key_begin(void *user, lj_error *error) {
  (void)error;
  return visit_append((visit_log *)user, "K(");
}

static lj_status on_string_begin(void *user, lj_error *error) {
  (void)error;
  return visit_append((visit_log *)user, "S(");
}

static lj_status on_chunk(void *user, const char *data, size_t len,
                          lj_error *error) {
  visit_log *log = (visit_log *)user;
  (void)error;

  if (log->len + len + 1u >= sizeof(log->text)) {
    return LJ_STATUS_OVERFLOW;
  }
  memcpy(log->text + log->len, data, len);
  log->len += len;
  log->text[log->len] = '\0';
  return LJ_STATUS_OK;
}

static lj_status on_key_end(void *user, lj_error *error) {
  (void)error;
  return visit_append((visit_log *)user, ")");
}

static lj_status on_string_end(void *user, lj_error *error) {
  (void)error;
  return visit_append((visit_log *)user, ")");
}

static lj_status on_number_begin(void *user, lj_error *error) {
  (void)error;
  return visit_append((visit_log *)user, "N(");
}

static lj_status on_number_end(void *user, lj_error *error) {
  (void)error;
  return visit_append((visit_log *)user, ")");
}

static lj_status on_boolean(void *user, int value, lj_error *error) {
  (void)error;
  return visit_append((visit_log *)user, value ? "T" : "F");
}

static lj_status on_null(void *user, lj_error *error) {
  (void)error;
  return visit_append((visit_log *)user, "Z");
}

static lj_status on_sse_json_value(void *user, const lj_sse_event *event,
                                   lj_json_value *value, lj_error *error) {
  visit_log *log = (visit_log *)user;
  (void)value;
  (void)error;

  printf("event=%s data_len=%lu events=%s\n", event->event,
         (unsigned long)event->data_len, log->text);
  log->len = 0u;
  log->text[0] = '\0';
  return LJ_STATUS_OK;
}

int main(void) {
  static const char *selected[] = {"keep"};
  static const char sse_bytes[] =
      "event: skip\n"
      "data: {\"ignored\":true}\n"
      "\n"
      "event: keep\n"
      "id: evt-1\n"
      "data: {\"kind\":\"delta\",\"parts\":[1,true,null,\"x\"]}\n"
      "\n"
      "event: keep\n"
      "id: evt-2\n"
      "data: \"done\"\n"
      "\n";
  lj_value_visitor visitor;
  lj_sse_json_options options;
  lj_json_value value;
  lj_sse *sse;
  visit_log log;
  lj_error error;
  lj_status status;

  memset(&log, 0, sizeof(log));
  visitor = lj_default_value_visitor();
  visitor.object_begin = on_object_begin;
  visitor.object_end = on_object_end;
  visitor.object_key_begin = on_key_begin;
  visitor.object_key_chunk = on_chunk;
  visitor.object_key_end = on_key_end;
  visitor.array_begin = on_array_begin;
  visitor.array_end = on_array_end;
  visitor.string_begin = on_string_begin;
  visitor.string_chunk = on_chunk;
  visitor.string_end = on_string_end;
  visitor.number_begin = on_number_begin;
  visitor.number_chunk = on_chunk;
  visitor.number_end = on_number_end;
  visitor.boolean_value = on_boolean;
  visitor.null_value = on_null;

  lj_json_value_init(&value);
  status = lj_json_value_set_parse_visitor(&value, &visitor, &log, NULL, &error);
  if (status != LJ_STATUS_OK) {
    fprintf(stderr, "visitor setup failed: %s\n", error.message);
    return 1;
  }

  options.event_names = selected;
  options.event_name_count = 1u;
  options.parse_options = NULL;

  sse = lj_sse_open(NULL, &error);
  if (sse == NULL) {
    fprintf(stderr, "SSE open failed: %s\n", error.message);
    lj_json_value_cleanup(&value);
    return 1;
  }

  status = lj_sse_push_json_value(sse, &value, sse_bytes, strlen(sse_bytes),
                                  &options, on_sse_json_value, &log, &error);
  if (status == LJ_STATUS_OK) {
    status = lj_sse_finish_json_value(sse, &value, &options, on_sse_json_value,
                                      &log, &error);
  }
  if (status != LJ_STATUS_OK) {
    fprintf(stderr, "SSE JSON value parse failed: %s\n", error.message);
    lj_sse_close(sse);
    lj_json_value_cleanup(&value);
    return 1;
  }

  lj_sse_close(sse);
  lj_json_value_cleanup(&value);
  return 0;
}
