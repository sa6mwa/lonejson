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

int main(void) {
  static const char json[] =
      "{\"person\":{\"name\":\"Alice\",\"age\":37,\"active\":true},"
      "\"tags\":[\"prod\",\"edge\"]}";
  consumer_log log_record;
  lonejson_error error;
  lonejson_status status;
  lonejson_write_options options;
  char buffer[256];

  memset(&log_record, 0, sizeof(log_record));
  memset(&error, 0, sizeof(error));

  status =
      lonejson_parse_cstr(&consumer_log_map, &log_record, json, NULL, &error);
  if (status != LONEJSON_STATUS_OK) {
    return 1;
  }
  if (strcmp(log_record.person.name, "Alice") != 0 ||
      log_record.person.age != 37 || !log_record.person.active ||
      log_record.tags.count != 2u ||
      strcmp(log_record.tags.items[0], "prod") != 0 ||
      strcmp(log_record.tags.items[1], "edge") != 0) {
    lonejson_cleanup(&consumer_log_map, &log_record);
    return 1;
  }

  options = lonejson_default_write_options();
  options.pretty = 1;
  status = lonejson_serialize_buffer(&consumer_log_map, &log_record, buffer,
                                     sizeof(buffer), NULL, &options, &error);
  lonejson_cleanup(&consumer_log_map, &log_record);
  if (status != LONEJSON_STATUS_OK) {
    return 1;
  }
  if (strstr(buffer, "\"name\": \"Alice\"") == NULL ||
      strstr(buffer, "\"tags\": [") == NULL) {
    return 1;
  }
  return 0;
}
