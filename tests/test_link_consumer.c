#include <stdio.h>
#include <string.h>

#include "lonejson.h"

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

int main(void) {
  static const char json[] =
      "{\"person\":{\"name\":\"Alice\",\"age\":37,\"active\":true},"
      "\"tags\":[\"prod\",\"edge\"]}";
  consumer_log log_record;
  lonejson_string_array_stream string_stream = LONEJSON_STRING_ARRAY_STREAM_INIT;
  lonejson_mapped_array_stream mapped_stream = LONEJSON_MAPPED_ARRAY_STREAM_INIT;
  lonejson_json_value value;
  consumer_sink sink;
  lonejson_error error;
  lonejson_status status;
  char buffer[256];
  char value_buffer[16];

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
      consumer_runtime()->serialize_buffer == NULL) {
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
  return 0;
}
