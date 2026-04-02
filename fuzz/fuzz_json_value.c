#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lonejson.h"

typedef struct fuzz_json_value_doc {
  char id[16];
  lonejson_json_value selector;
  lonejson_json_value fields;
  lonejson_json_value last_error;
} fuzz_json_value_doc;

typedef struct fuzz_sink_state {
  size_t total;
} fuzz_sink_state;

static lonejson_status fuzz_count_sink(void *user, const void *data, size_t len,
                                       lonejson_error *error) {
  fuzz_sink_state *state = (fuzz_sink_state *)user;
  (void)data;
  (void)error;
  state->total += len;
  return LONEJSON_STATUS_OK;
}

static const lonejson_field fuzz_json_value_doc_fields[] = {
    LONEJSON_FIELD_STRING_FIXED(fuzz_json_value_doc, id, "id",
                                LONEJSON_OVERFLOW_TRUNCATE),
    LONEJSON_FIELD_JSON_VALUE(fuzz_json_value_doc, selector, "selector"),
    LONEJSON_FIELD_JSON_VALUE(fuzz_json_value_doc, fields, "fields"),
    LONEJSON_FIELD_JSON_VALUE(fuzz_json_value_doc, last_error, "last_error")};
LONEJSON_MAP_DEFINE(fuzz_json_value_doc_map, fuzz_json_value_doc,
                    fuzz_json_value_doc_fields);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  fuzz_json_value_doc doc;
  fuzz_json_value_doc streamed;
  lonejson_error error;
  lonejson_status status;
  lonejson_parse_options options;
  fuzz_sink_state selector_sink;
  fuzz_sink_state fields_sink;
  fuzz_sink_state error_sink;

  memset(&doc, 0, sizeof(doc));
  status = lonejson_parse_buffer(&fuzz_json_value_doc_map, &doc, data, size,
                                 NULL, &error);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    char compact[2048];
    char pretty[4096];
    size_t needed = 0u;
    lonejson_write_options options = lonejson_default_write_options();

    (void)lonejson_serialize_buffer(&fuzz_json_value_doc_map, &doc, compact,
                                    sizeof(compact), &needed, NULL, &error);
    options.pretty = 1;
    (void)lonejson_serialize_buffer(&fuzz_json_value_doc_map, &doc, pretty,
                                    sizeof(pretty), &needed, &options, &error);
  }
  lonejson_cleanup(&fuzz_json_value_doc_map, &doc);

  memset(&streamed, 0, sizeof(streamed));
  lonejson_json_value_set_parse_sink(&streamed.selector, fuzz_count_sink,
                                     &selector_sink, &error);
  lonejson_json_value_set_parse_sink(&streamed.fields, fuzz_count_sink,
                                     &fields_sink, &error);
  lonejson_json_value_set_parse_sink(&streamed.last_error, fuzz_count_sink,
                                     &error_sink, &error);
  memset(&selector_sink, 0, sizeof(selector_sink));
  memset(&fields_sink, 0, sizeof(fields_sink));
  memset(&error_sink, 0, sizeof(error_sink));
  options = lonejson_default_parse_options();
  options.clear_destination = 0;
  (void)lonejson_parse_buffer(&fuzz_json_value_doc_map, &streamed, data, size,
                              &options, &error);
  lonejson_cleanup(&fuzz_json_value_doc_map, &streamed);
  return 0;
}
