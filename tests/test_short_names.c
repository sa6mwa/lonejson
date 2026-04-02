#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define LJ_IMPLEMENTATION
#include "lonejson.h"

typedef struct short_doc {
  char id[8];
  lj_source text;
} short_doc;

typedef struct short_parse_doc {
  char name[16];
  lj_int64 age;
  bool active;
} short_parse_doc;

static const lj_field short_doc_fields[] = {
    LJ_FIELD_STRING_FIXED_REQ(short_doc, id, "id", LJ_OVERFLOW_FAIL),
    LJ_FIELD_STRING_SOURCE(short_doc, text, "text")};
LJ_MAP_DEFINE(short_doc_map, short_doc, short_doc_fields);

static const lj_field short_parse_doc_fields[] = {
    LJ_FIELD_STRING_FIXED_REQ(short_parse_doc, name, "name", LJ_OVERFLOW_FAIL),
    LJ_FIELD_I64(short_parse_doc, age, "age"),
    LJ_FIELD_BOOL(short_parse_doc, active, "active")};
LJ_MAP_DEFINE(short_parse_doc_map, short_parse_doc, short_parse_doc_fields);

int main(void) {
  static const char payload[] = "short alias text";
  static const char parse_json[] =
      "{\"name\":\"Alias\",\"age\":42,\"active\":true}";
  short_doc doc;
  short_parse_doc short_record;
  short_parse_doc long_record;
  lj_error error;
  lj_status status;
  char path[] = "/tmp/lonejson-short-XXXXXX";
  int fd;
  char buffer[128];
  char short_json[128];
  char long_json[128];

  lj_init(&short_doc_map, &doc);
  lj_init(&short_parse_doc_map, &short_record);
  lonejson_init(&short_parse_doc_map, &long_record);
  strcpy(doc.id, "ok");

  fd = mkstemp(path);
  if (fd < 0) {
    return 1;
  }
  if (write(fd, payload, sizeof(payload) - 1u) !=
      (ssize_t)(sizeof(payload) - 1u)) {
    close(fd);
    unlink(path);
    return 1;
  }
  close(fd);

  status = lj_source_set_path(&doc.text, path, &error);
  if (status != LJ_STATUS_OK) {
    unlink(path);
    return 1;
  }
  status = lj_serialize_buffer(&short_doc_map, &doc, buffer, sizeof(buffer),
                               NULL, NULL, &error);
  lj_cleanup(&short_doc_map, &doc);
  unlink(path);
  if (status != LJ_STATUS_OK) {
    return 1;
  }
  if (strstr(buffer, "\"text\":\"short alias text\"") == NULL) {
    return 1;
  }

  status = lj_parse_cstr(&short_parse_doc_map, &short_record, parse_json, NULL,
                         &error);
  if (status != LJ_STATUS_OK) {
    return 1;
  }
  status = lonejson_parse_cstr(&short_parse_doc_map, &long_record, parse_json,
                               NULL, &error);
  if (status != LONEJSON_STATUS_OK) {
    lj_cleanup(&short_parse_doc_map, &short_record);
    lonejson_cleanup(&short_parse_doc_map, &long_record);
    return 1;
  }
  if (strcmp(short_record.name, long_record.name) != 0 ||
      short_record.age != long_record.age ||
      short_record.active != long_record.active) {
    lj_cleanup(&short_parse_doc_map, &short_record);
    lonejson_cleanup(&short_parse_doc_map, &long_record);
    return 1;
  }

  status = lj_serialize_buffer(&short_parse_doc_map, &short_record, short_json,
                               sizeof(short_json), NULL, NULL, &error);
  if (status != LJ_STATUS_OK) {
    lj_cleanup(&short_parse_doc_map, &short_record);
    lonejson_cleanup(&short_parse_doc_map, &long_record);
    return 1;
  }
  status =
      lonejson_serialize_buffer(&short_parse_doc_map, &long_record, long_json,
                                sizeof(long_json), NULL, NULL, &error);
  if (status != LONEJSON_STATUS_OK) {
    lj_cleanup(&short_parse_doc_map, &short_record);
    lonejson_cleanup(&short_parse_doc_map, &long_record);
    return 1;
  }
  lj_cleanup(&short_parse_doc_map, &short_record);
  lonejson_cleanup(&short_parse_doc_map, &long_record);
  return strcmp(short_json, long_json) == 0 ? 0 : 1;
}
