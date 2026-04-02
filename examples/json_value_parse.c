#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lonejson.h"

typedef struct parsed_envelope {
  char id[16];
  lj_json_value selector;
  lj_json_value fields;
  lj_json_value last_error;
} parsed_envelope;

static const lj_field parsed_envelope_fields[] = {
    LJ_FIELD_STRING_FIXED_REQ(parsed_envelope, id, "id", LJ_OVERFLOW_FAIL),
    LJ_FIELD_JSON_VALUE(parsed_envelope, selector, "selector"),
    LJ_FIELD_JSON_VALUE(parsed_envelope, fields, "fields"),
    LJ_FIELD_JSON_VALUE(parsed_envelope, last_error, "last_error")};
LJ_MAP_DEFINE(parsed_envelope_map, parsed_envelope, parsed_envelope_fields);

int main(void) {
  static const char inbound_json[] =
      "{\"id\":\"evt-1\","
      "\"selector\":{\"and\":[{\"eq\":{\"field\":\"/status\",\"value\":\"open\"}},"
      "{\"exists\":{\"field\":\"/meta/etag\"}}]},"
      "\"fields\":{\"include\":[\"/id\",\"/meta/etag\"],\"strict\":true},"
      "\"last_error\":{\"code\":\"bad_selector\",\"detail\":{\"offset\":17}}}";
  parsed_envelope doc;
  lj_error error;
  lj_status status;
  lj_parse_options parse_options;
  lj_write_options options;
  char *pretty;

  lj_init(&parsed_envelope_map, &doc);
  status = lj_json_value_enable_parse_capture(&doc.selector, &error);
  if (status != LJ_STATUS_OK) {
    fprintf(stderr, "selector capture setup failed: %s\n", error.message);
    return 1;
  }
  status = lj_json_value_enable_parse_capture(&doc.fields, &error);
  if (status != LJ_STATUS_OK) {
    fprintf(stderr, "fields capture setup failed: %s\n", error.message);
    lj_cleanup(&parsed_envelope_map, &doc);
    return 1;
  }
  status = lj_json_value_enable_parse_capture(&doc.last_error, &error);
  if (status != LJ_STATUS_OK) {
    fprintf(stderr, "last_error capture setup failed: %s\n", error.message);
    lj_cleanup(&parsed_envelope_map, &doc);
    return 1;
  }

  parse_options = lj_default_parse_options();
  parse_options.clear_destination = 0;
  status = lj_parse_cstr(&parsed_envelope_map, &doc, inbound_json, &parse_options,
                         &error);
  if (status != LJ_STATUS_OK) {
    fprintf(stderr, "parse failed: %s\n", error.message);
    return 1;
  }

  printf("selector compact=%s\n", doc.selector.json ? doc.selector.json : "null");
  printf("fields compact=%s\n", doc.fields.json ? doc.fields.json : "null");
  printf("last_error compact=%s\n",
         doc.last_error.json ? doc.last_error.json : "null");

  options = lj_default_write_options();
  options.pretty = 1;
  pretty = lj_serialize_alloc(&parsed_envelope_map, &doc, NULL, &options, &error);
  if (pretty == NULL) {
    fprintf(stderr, "pretty serialize failed: %s\n", error.message);
    lj_cleanup(&parsed_envelope_map, &doc);
    return 1;
  }

  printf("pretty=%s\n", pretty);

  free(pretty);
  lj_cleanup(&parsed_envelope_map, &doc);
  return 0;
}
