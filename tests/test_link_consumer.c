#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lonejson.h"

#if defined(__has_feature)
#if __has_feature(memory_sanitizer)
#include <sanitizer/msan_interface.h>
#define CONSUMER_HAS_MSAN 1
#endif
#endif
#ifndef CONSUMER_HAS_MSAN
#define CONSUMER_HAS_MSAN 0
#endif

typedef struct consumer_person {
  char name[16];
  lonejson_int64 age;
  bool active;
} consumer_person;

typedef struct consumer_log {
  consumer_person person;
  lonejson_string_array tags;
} consumer_log;

typedef struct consumer_sink {
  char *buffer;
  size_t len;
  size_t capacity;
} consumer_sink;

typedef struct consumer_response_json {
  char *response_json;
} consumer_response_json;

typedef struct consumer_rewrite_item {
  char *name;
} consumer_rewrite_item;

typedef struct consumer_rewrite_counter {
  size_t count;
} consumer_rewrite_counter;

static const lonejson_field consumer_person_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(consumer_person, name, "name",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_I64_REQ(consumer_person, age, "age"),
    LONEJSON_FIELD_BOOL_REQ(consumer_person, active, "active")};
LONEJSON_MAP_DEFINE(consumer_person_map, consumer_person,
                    consumer_person_fields);

static const lonejson_field consumer_log_fields[] = {
    LONEJSON_FIELD_OBJECT_REQ(consumer_log, person, "person",
                              &consumer_person_map),
    LONEJSON_FIELD_STRING_ARRAY(consumer_log, tags, "tags",
                                LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(consumer_log_map, consumer_log, consumer_log_fields);

static const lonejson_field consumer_response_json_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(consumer_response_json, response_json,
                                    "response_json")};
LONEJSON_MAP_DEFINE(consumer_response_json_map, consumer_response_json,
                    consumer_response_json_fields);

static const lonejson_field consumer_rewrite_item_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(consumer_rewrite_item, name, "name")};
LONEJSON_MAP_DEFINE(consumer_rewrite_item_map, consumer_rewrite_item,
                    consumer_rewrite_item_fields);

static lonejson *consumer_runtime(void) {
  static lonejson *runtime = NULL;

  if (runtime == NULL) {
    lonejson_config config = lonejson_default_config();
    config.write_pretty = 1;
    runtime = lonejson_new(&config, NULL);
  }
  return runtime;
}

static lonejson_status consumer_sink_write(void *user, const void *data,
                                           size_t len, lonejson_error *error) {
  consumer_sink *sink;

  (void)error;
  sink = (consumer_sink *)user;
  if (sink->len + len + 1u > sink->capacity) {
    return LONEJSON_STATUS_OVERFLOW;
  }
  memcpy(sink->buffer + sink->len, data, len);
  sink->len += len;
  sink->buffer[sink->len] = '\0';
  return LONEJSON_STATUS_OK;
}

static int consumer_msan_bytes_initialized(const void *ptr, size_t len) {
#if CONSUMER_HAS_MSAN
  return ptr == NULL || __msan_test_shadow(ptr, len) < 0;
#else
  (void)ptr;
  (void)len;
  return 1;
#endif
}

static int consumer_msan_cstr_initialized(const char *text) {
  return text == NULL ||
         consumer_msan_bytes_initialized(text, strlen(text) + 1u);
}

static void consumer_msan_poison_bytes(void *ptr, size_t len) {
#if CONSUMER_HAS_MSAN
  __msan_poison(ptr, len);
#else
  (void)ptr;
  (void)len;
#endif
}

static lonejson_status consumer_rewrite_item_cb(
    void *user, const lonejson_array_rewrite_context *context, void *item,
    lonejson_array_rewrite_result *result, lonejson_error *error) {
  consumer_rewrite_counter *counter = (consumer_rewrite_counter *)user;
  consumer_rewrite_item *rewrite_item = (consumer_rewrite_item *)item;

  (void)context;
  (void)result;
  (void)error;
  if (rewrite_item == NULL || rewrite_item->name == NULL ||
      !consumer_msan_cstr_initialized(rewrite_item->name)) {
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  if (counter->count == 0u && strcmp(rewrite_item->name, "ok") != 0) {
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  if (counter->count == 1u && strcmp(rewrite_item->name, "go") != 0) {
    return LONEJSON_STATUS_CALLBACK_FAILED;
  }
  counter->count++;
  return LONEJSON_STATUS_OK;
}

int main(void) {
  static const char json[] =
      "{\"person\":{\"name\":\"Alice\",\"age\":37,\"active\":true},"
      "\"tags\":[\"prod\",\"edge\"]}";
  static const char response_json[] = "{\"response_json\":\"created\"}";
  static const char rewrite_json[] =
      "{\"items\":[{\"name\":\"ok\"},{\"name\":\"go\"}]}";
  consumer_log log_record;
  consumer_response_json response;
  consumer_rewrite_item rewrite_item;
  consumer_rewrite_counter rewrite_counter;
  lonejson_string_array_stream string_stream = LONEJSON_STRING_ARRAY_STREAM_INIT;
  lonejson_mapped_array_stream mapped_stream = LONEJSON_MAPPED_ARRAY_STREAM_INIT;
  lonejson_array_rewrite_options rewrite_options;
  lonejson_json_value value;
  lonejson_spooled spool;
  lonejson_buffer_reader reader;
  lonejson_read_result rr;
  lonejson_read_result normalized;
  consumer_sink sink;
  lonejson_error error;
  lonejson_status status;
  char buffer[256];
  char value_buffer[16];
  unsigned char read_buffer[16];

  lonejson_init(consumer_runtime(), &consumer_log_map, &log_record);
  lonejson_error_init(&error);

  status =
      lonejson_parse_cstr(consumer_runtime(), &consumer_log_map, &log_record,
                          json, &error);
  if (status != LONEJSON_STATUS_OK) {
    return 1;
  }
  if (strcmp(log_record.person.name, "Alice") != 0 ||
      log_record.person.age != 37 || !log_record.person.active ||
      log_record.tags.count != 2u ||
      strcmp(log_record.tags.items[0], "prod") != 0 ||
      strcmp(log_record.tags.items[1], "edge") != 0) {
    consumer_runtime()->cleanup(consumer_runtime(), &consumer_log_map,
                                &log_record);
    return 1;
  }
  if (string_stream.set_handler == NULL || mapped_stream.set_handler == NULL ||
      consumer_runtime()->cleanup == NULL ||
      consumer_runtime()->serialize_sink == NULL ||
      consumer_runtime()->serialize_buffer == NULL) {
    consumer_runtime()->cleanup(consumer_runtime(), &consumer_log_map,
                                &log_record);
    return 1;
  }

  memset(&sink, 0, sizeof(sink));
  memset(buffer, 0, sizeof(buffer));
  sink.buffer = buffer;
  sink.capacity = sizeof(buffer);
  status = consumer_runtime()->serialize_sink(
      consumer_runtime(), &consumer_log_map, &log_record, consumer_sink_write,
      &sink, &error);
  if (status != LONEJSON_STATUS_OK) {
    consumer_runtime()->cleanup(consumer_runtime(), &consumer_log_map,
                                &log_record);
    return 1;
  }
  if (strstr(buffer, "\"name\": \"Alice\"") == NULL ||
      strstr(buffer, "\"tags\": [") == NULL) {
    consumer_runtime()->cleanup(consumer_runtime(), &consumer_log_map,
                                &log_record);
    return 1;
  }

  status = consumer_runtime()->serialize_buffer(
      consumer_runtime(), &consumer_log_map, &log_record, buffer,
      sizeof(buffer), NULL, &error);
  consumer_runtime()->cleanup(consumer_runtime(), &consumer_log_map,
                              &log_record);
  if (status != LONEJSON_STATUS_OK) {
    return 1;
  }
  if (strstr(buffer, "\"name\": \"Alice\"") == NULL ||
      strstr(buffer, "\"tags\": [") == NULL) {
    return 1;
  }

  consumer_runtime()->json_value_init(consumer_runtime(), &value);
  if (value.methods == NULL || value.methods->set_buffer == NULL ||
      value.methods->write_to_sink == NULL || value.methods->cleanup == NULL) {
    return 1;
  }
  status = value.methods->set_buffer(&value, "true", 4u, &error);
  if (status != LONEJSON_STATUS_OK) {
    value.methods->cleanup(&value);
    return 1;
  }
  memset(value_buffer, 0, sizeof(value_buffer));
  sink.buffer = value_buffer;
  sink.len = 0u;
  sink.capacity = sizeof(value_buffer);
  status =
      value.methods->write_to_sink(&value, consumer_sink_write, &sink, &error);
  value.methods->cleanup(&value);
  if (status != LONEJSON_STATUS_OK || strcmp(value_buffer, "true") != 0) {
    return 1;
  }

  memset(&response, 0, sizeof(response));
  status = lonejson_parse_cstr(consumer_runtime(), &consumer_response_json_map,
                               &response, response_json, &error);
  if (status != LONEJSON_STATUS_OK || response.response_json == NULL ||
      !consumer_msan_cstr_initialized(response.response_json) ||
      strcmp(response.response_json, "created") != 0) {
    lonejson_cleanup(&consumer_response_json_map, &response);
    return 1;
  }
  lonejson_cleanup(&consumer_response_json_map, &response);

  response.response_json = value_buffer;
  value_buffer[0] = '\0';
  consumer_msan_poison_bytes(value_buffer + 1u, sizeof(value_buffer) - 1u);
  {
    char *allocated = lonejson_serialize_alloc(
        consumer_runtime(), &consumer_response_json_map, &response, NULL,
        &error);
    if (allocated == NULL || !consumer_msan_cstr_initialized(allocated) ||
        strstr(allocated, "\"response_json\"") == NULL ||
        strstr(allocated, "\"\"") == NULL) {
      free(allocated);
      return 1;
    }
    free(allocated);
  }

  memset(&rewrite_options, 0, sizeof(rewrite_options));
  rewrite_counter.count = 0u;
  rewrite_item.name = NULL;
  consumer_msan_poison_bytes(&rewrite_item.name, sizeof(rewrite_item.name));
  rewrite_options.item_map = &consumer_rewrite_item_map;
  rewrite_options.item_dst = &rewrite_item;
  rewrite_options.item = consumer_rewrite_item_cb;
  rewrite_options.user = &rewrite_counter;
  memset(&sink, 0, sizeof(sink));
  memset(buffer, 0, sizeof(buffer));
  sink.buffer = buffer;
  sink.capacity = sizeof(buffer);
  lonejson_buffer_reader_init(&reader, rewrite_json, strlen(rewrite_json));
  status = lonejson_array_rewrite_reader(
      consumer_runtime(), "items", lonejson_buffer_reader_read, &reader,
      consumer_sink_write, &sink, &rewrite_options, &error);
  if (status != LONEJSON_STATUS_OK || rewrite_counter.count != 2u ||
      strstr(buffer, "\"items\":[{\"name\":\"ok\"},{\"name\":\"go\"}]") ==
          NULL) {
    return 1;
  }

  lonejson_spooled_init(consumer_runtime(), &spool);
  status = lonejson_spooled_append(&spool, "{}", 2u, &error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_spooled_cleanup(&spool);
    return 1;
  }
  status = lonejson_spooled_rewind(&spool, &error);
  if (status != LONEJSON_STATUS_OK) {
    lonejson_spooled_cleanup(&spool);
    return 1;
  }
  rr = lonejson_spooled_read(&spool, read_buffer, sizeof(read_buffer));
  if (!consumer_msan_bytes_initialized(&rr, sizeof(rr)) ||
      !consumer_msan_bytes_initialized(read_buffer, rr.bytes_read)) {
    lonejson_spooled_cleanup(&spool);
    return 1;
  }
  normalized = lonejson_default_read_result();
  normalized.bytes_read = rr.bytes_read;
  normalized.eof = rr.eof;
  normalized.would_block = rr.would_block;
  normalized.error_code = rr.error_code;
  normalized.reserved = rr.reserved;
  if (!consumer_msan_bytes_initialized(&normalized, sizeof(normalized)) ||
      normalized.bytes_read != 2u) {
    lonejson_spooled_cleanup(&spool);
    return 1;
  }
  lonejson_spooled_cleanup(&spool);

  return 0;
}
