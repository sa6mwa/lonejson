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
  lonejson *runtime;
  lonejson *runtime_pretty;
  lonejson *runtime_streamed;
  lonejson_config config;
  fuzz_json_value_doc doc;
  fuzz_json_value_doc streamed;
  lonejson_error error;
  lonejson_status status;
  fuzz_sink_state selector_sink;
  fuzz_sink_state fields_sink;
  fuzz_sink_state error_sink;

  memset(&doc, 0, sizeof(doc));
  runtime = lonejson_new(NULL, &error);
  if (runtime == NULL) {
    return 0;
  }
  status =
      lonejson_parse_buffer(runtime, &fuzz_json_value_doc_map, &doc, data, size,
                            &error);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    char compact[2048];
    char pretty[4096];
    size_t needed = 0u;
    config = lonejson_default_config();
    config.write_pretty = 1;
    runtime_pretty = lonejson_new(&config, &error);
    if (runtime_pretty == NULL) {
      runtime_pretty = NULL;
    }

    (void)lonejson_serialize_buffer(runtime, &fuzz_json_value_doc_map, &doc,
                                    compact, sizeof(compact), &needed, &error);
    if (runtime_pretty != NULL) {
      (void)lonejson_serialize_buffer(runtime_pretty, &fuzz_json_value_doc_map,
                                      &doc, pretty, sizeof(pretty), &needed,
                                      &error);
      lonejson_free(runtime_pretty);
    }
  }
  lonejson_cleanup(&fuzz_json_value_doc_map, &doc);
  lonejson_free(runtime);

  memset(&streamed, 0, sizeof(streamed));
  config = lonejson_default_config();
  config.clear_destination_by_default = 0;
  runtime_streamed = lonejson_new(&config, &error);
  if (runtime_streamed == NULL) {
    return 0;
  }
  lonejson_init(runtime_streamed, &fuzz_json_value_doc_map, &streamed);
  memset(&selector_sink, 0, sizeof(selector_sink));
  memset(&fields_sink, 0, sizeof(fields_sink));
  memset(&error_sink, 0, sizeof(error_sink));
  lonejson_json_value_set_parse_sink(&streamed.selector, fuzz_count_sink,
                                     &selector_sink, &error);
  lonejson_json_value_set_parse_sink(&streamed.fields, fuzz_count_sink,
                                     &fields_sink, &error);
  lonejson_json_value_set_parse_sink(&streamed.last_error, fuzz_count_sink,
                                     &error_sink, &error);
  (void)lonejson_parse_buffer(runtime_streamed, &fuzz_json_value_doc_map,
                              &streamed, data, size, &error);
  lonejson_cleanup(&fuzz_json_value_doc_map, &streamed);
  lonejson_free(runtime_streamed);
  return 0;
}
