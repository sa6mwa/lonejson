#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct alloc_record {
  void *ptr;
} alloc_record;

static alloc_record g_alloc_records[8192];
static size_t g_alloc_record_count = 0u;
static size_t g_lonejson_alloc_calls = 0u;
static size_t g_lonejson_free_calls = 0u;

static void track_alloc_ptr(void *ptr) {
  if (ptr == NULL) {
    return;
  }
  if (g_alloc_record_count <
      sizeof(g_alloc_records) / sizeof(g_alloc_records[0])) {
    g_alloc_records[g_alloc_record_count].ptr = ptr;
    ++g_alloc_record_count;
  }
}

static void track_free_ptr(void *ptr) {
  size_t i;

  if (ptr == NULL) {
    return;
  }
  for (i = 0; i < g_alloc_record_count; ++i) {
    if (g_alloc_records[i].ptr == ptr) {
      g_alloc_records[i] = g_alloc_records[g_alloc_record_count - 1u];
      --g_alloc_record_count;
      return;
    }
  }
}

static void *test_lonejson_malloc(size_t size) {
  void *ptr = malloc(size);
  if (ptr != NULL) {
    ++g_lonejson_alloc_calls;
    track_alloc_ptr(ptr);
  }
  return ptr;
}

static void *test_lonejson_realloc(void *ptr, size_t size) {
  void *next;

  if (ptr == NULL) {
    next = realloc(NULL, size);
    if (next != NULL) {
      ++g_lonejson_alloc_calls;
      track_alloc_ptr(next);
    }
    return next;
  }

  track_free_ptr(ptr);
  next = realloc(ptr, size);
  if (next != NULL) {
    track_alloc_ptr(next);
  } else {
    track_alloc_ptr(ptr);
  }
  return next;
}

static void test_lonejson_free(void *ptr) {
  if (ptr != NULL) {
    ++g_lonejson_free_calls;
    track_free_ptr(ptr);
  }
  free(ptr);
}

static void reset_lonejson_alloc_stats(void) {
  g_alloc_record_count = 0u;
  g_lonejson_alloc_calls = 0u;
  g_lonejson_free_calls = 0u;
}

#define LONEJSON_MALLOC test_lonejson_malloc
#define LONEJSON_REALLOC test_lonejson_realloc
#define LONEJSON_FREE test_lonejson_free
#define LONEJSON_TRACK_WORKSPACE_USAGE 1
#include "../src/lonejson_internal.h"

typedef struct test_allocator_state {
  lonejson_allocator allocator;
  lonejson_allocator_stats stats;
} test_allocator_state;

static void *test_allocator_malloc(void *ctx, size_t size) {
  (void)ctx;
  return malloc(size);
}

static void *test_allocator_realloc(void *ctx, void *ptr, size_t size) {
  (void)ctx;
  return realloc(ptr, size);
}

static void test_allocator_free(void *ctx, void *ptr) {
  (void)ctx;
  free(ptr);
}

static void test_allocator_init(test_allocator_state *state) {
  memset(state, 0, sizeof(*state));
  state->allocator = lonejson_default_allocator();
  state->allocator.malloc_fn = test_allocator_malloc;
  state->allocator.realloc_fn = test_allocator_realloc;
  state->allocator.free_fn = test_allocator_free;
  state->allocator.stats = &state->stats;
}

typedef struct test_address {
  char city[16];
  lonejson_int64 zip;
} test_address;

typedef struct test_item {
  lonejson_int64 id;
  char label[12];
} test_item;

typedef struct test_fixed_log {
  char name[8];
  lonejson_i64_array codes;
} test_fixed_log;

typedef struct test_event {
  char id[8];
  bool ok;
} test_event;

typedef struct test_large_meta {
  lonejson_int64 shard;
  bool ok;
} test_large_meta;

typedef struct test_large_stream_record {
  lonejson_int64 id;
  char name[16];
  char kind[8];
  char payload[32];
  char base64[16];
  test_large_meta meta;
} test_large_stream_record;

typedef struct test_spool_doc {
  char id[16];
  lonejson_spooled text;
  lonejson_spooled bytes;
} test_spool_doc;

typedef struct test_spool_limits_doc {
  lonejson_spooled text;
} test_spool_limits_doc;

typedef struct test_source_doc {
  char id[16];
  lonejson_source text;
  lonejson_source bytes;
} test_source_doc;

typedef struct test_json_value_doc {
  char id[16];
  lonejson_json_value selector;
  lonejson_json_value fields;
  lonejson_json_value last_error;
} test_json_value_doc;

typedef struct test_alloc_parse_doc {
  char *name;
  lonejson_spooled body;
  lonejson_string_array tags;
} test_alloc_parse_doc;

typedef struct test_alloc_json_value_doc {
  char id[16];
  lonejson_json_value selector;
} test_alloc_json_value_doc;

typedef struct test_fixed_result_row {
  char name[32];
  char group[16];
} test_fixed_result_row;

typedef struct test_fixed_result_run {
  lonejson_object_array results;
  test_fixed_result_row result_storage[4];
} test_fixed_result_run;

typedef struct test_aligned_item {
  char name[8];
  long double weight;
} test_aligned_item;

typedef struct test_aligned_doc {
  lonejson_object_array items;
} test_aligned_doc;

typedef struct test_omit_reasoning {
  char *effort;
  char *summary;
} test_omit_reasoning;

typedef struct test_omit_deep_leaf {
  char *value;
} test_omit_deep_leaf;

typedef struct test_omit_deep_middle {
  test_omit_deep_leaf leaf;
} test_omit_deep_middle;

typedef struct test_omit_deep_doc {
  char *id;
  test_omit_deep_middle middle;
} test_omit_deep_doc;

typedef struct test_omit_doc {
  char *model;
  char *instructions;
  lonejson_int64 max_output_tokens;
  int has_max_output_tokens;
  lonejson_uint64 seed;
  int has_seed;
  double temperature;
  int has_temperature;
  bool stream;
  int has_stream;
  test_omit_reasoning reasoning;
  lonejson_string_array tags;
  lonejson_object_array tools;
  test_item tool_storage[1];
  lonejson_source payload;
  lonejson_json_value metadata;
} test_omit_doc;

typedef struct test_reader_state {
  const char *json;
  size_t offset;
  size_t chunk_size;
} test_reader_state;

typedef struct test_counting_sink {
  size_t total;
} test_counting_sink;

typedef struct test_buffer_sink {
  unsigned char *buffer;
  size_t capacity;
  size_t length;
} test_buffer_sink;

typedef struct test_failing_sink {
  size_t fail_after;
  size_t total;
} test_failing_sink;

typedef struct test_visit_state {
  char log[4096];
  size_t len;
} test_visit_state;

typedef struct test_visit_chunk_state {
  size_t object_count;
  size_t array_count;
  size_t key_begin_count;
  size_t key_chunk_count;
  size_t key_end_count;
  size_t string_begin_count;
  size_t string_chunk_count;
  size_t string_end_count;
  size_t number_begin_count;
  size_t number_chunk_count;
  size_t number_end_count;
  size_t bool_count;
  size_t null_count;
  size_t callback_count;
  size_t fail_after;
  char last_key[256];
  size_t last_key_len;
  char last_string[256];
  size_t last_string_len;
  char last_number[128];
  size_t last_number_len;
} test_visit_chunk_state;

typedef struct test_person {
  char *name;
  char nickname[8];
  lonejson_int64 age;
  double score;
  bool active;
  test_address address;
  lonejson_i64_array lucky_numbers;
  lonejson_string_array tags;
  lonejson_object_array items;
} test_person;

static const lonejson_field test_address_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(test_address, city, "city",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_I64_REQ(test_address, zip, "zip")};
LONEJSON_MAP_DEFINE(test_address_map, test_address, test_address_fields);

static const lonejson_field test_item_fields[] = {
    LONEJSON_FIELD_I64_REQ(test_item, id, "id"),
    LONEJSON_FIELD_STRING_FIXED(test_item, label, "label",
                                LONEJSON_OVERFLOW_TRUNCATE)};
LONEJSON_MAP_DEFINE(test_item_map, test_item, test_item_fields);

static const lonejson_field test_fixed_log_fields[] = {
    LONEJSON_FIELD_STRING_FIXED(test_fixed_log, name, "name",
                                LONEJSON_OVERFLOW_TRUNCATE),
    LONEJSON_FIELD_I64_ARRAY(test_fixed_log, codes, "codes",
                             LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(test_fixed_log_map, test_fixed_log, test_fixed_log_fields);

static const lonejson_field test_event_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(test_event, id, "id",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_BOOL(test_event, ok, "ok")};
LONEJSON_MAP_DEFINE(test_event_map, test_event, test_event_fields);

static const lonejson_field test_large_meta_fields[] = {
    LONEJSON_FIELD_I64_REQ(test_large_meta, shard, "shard"),
    LONEJSON_FIELD_BOOL_REQ(test_large_meta, ok, "ok")};
LONEJSON_MAP_DEFINE(test_large_meta_map, test_large_meta,
                    test_large_meta_fields);

static const lonejson_field test_large_stream_record_fields[] = {
    LONEJSON_FIELD_I64_REQ(test_large_stream_record, id, "id"),
    LONEJSON_FIELD_STRING_FIXED_REQ(test_large_stream_record, name, "name",
                                    LONEJSON_OVERFLOW_TRUNCATE),
    LONEJSON_FIELD_STRING_FIXED_REQ(test_large_stream_record, kind, "kind",
                                    LONEJSON_OVERFLOW_TRUNCATE),
    LONEJSON_FIELD_STRING_FIXED_REQ(test_large_stream_record, payload,
                                    "payload", LONEJSON_OVERFLOW_TRUNCATE),
    LONEJSON_FIELD_STRING_FIXED_REQ(test_large_stream_record, base64, "base64",
                                    LONEJSON_OVERFLOW_TRUNCATE),
    LONEJSON_FIELD_OBJECT_REQ(test_large_stream_record, meta, "meta",
                              &test_large_meta_map)};
LONEJSON_MAP_DEFINE(test_large_stream_record_map, test_large_stream_record,
                    test_large_stream_record_fields);

static const lonejson_spool_options test_spool_small_options = {64u, 0u, NULL};
static const lonejson_spool_options test_spool_limited_options = {32u, 96u,
                                                                  NULL};

static const lonejson_field test_spool_doc_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(test_spool_doc, id, "id",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_STREAM_OPTS(test_spool_doc, text, "text",
                                      &test_spool_small_options),
    LONEJSON_FIELD_BASE64_STREAM_OPTS(test_spool_doc, bytes, "bytes",
                                      &test_spool_small_options)};
LONEJSON_MAP_DEFINE(test_spool_doc_map, test_spool_doc, test_spool_doc_fields);

static const lonejson_field test_spool_limits_doc_fields[] = {
    LONEJSON_FIELD_STRING_STREAM_OPTS(test_spool_limits_doc, text, "text",
                                      &test_spool_limited_options)};
LONEJSON_MAP_DEFINE(test_spool_limits_doc_map, test_spool_limits_doc,
                    test_spool_limits_doc_fields);

static const lonejson_field test_source_doc_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(test_source_doc, id, "id",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_SOURCE(test_source_doc, text, "text"),
    LONEJSON_FIELD_BASE64_SOURCE(test_source_doc, bytes, "bytes")};
LONEJSON_MAP_DEFINE(test_source_doc_map, test_source_doc,
                    test_source_doc_fields);

static const lonejson_field test_json_value_doc_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(test_json_value_doc, id, "id",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_JSON_VALUE_REQ(test_json_value_doc, selector, "selector"),
    LONEJSON_FIELD_JSON_VALUE(test_json_value_doc, fields, "fields"),
    LONEJSON_FIELD_JSON_VALUE(test_json_value_doc, last_error, "last_error")};
LONEJSON_MAP_DEFINE(test_json_value_doc_map, test_json_value_doc,
                    test_json_value_doc_fields);

static const lonejson_field test_alloc_parse_doc_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(test_alloc_parse_doc, name, "name"),
    LONEJSON_FIELD_STRING_STREAM_OPTS(test_alloc_parse_doc, body, "body",
                                      &test_spool_small_options),
    LONEJSON_FIELD_STRING_ARRAY(test_alloc_parse_doc, tags, "tags",
                                LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(test_alloc_parse_doc_map, test_alloc_parse_doc,
                    test_alloc_parse_doc_fields);

static const lonejson_field test_alloc_json_value_doc_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(test_alloc_json_value_doc, id, "id",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_JSON_VALUE_REQ(test_alloc_json_value_doc, selector,
                                  "selector")};
LONEJSON_MAP_DEFINE(test_alloc_json_value_doc_map, test_alloc_json_value_doc,
                    test_alloc_json_value_doc_fields);

static const lonejson_field test_fixed_result_row_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(test_fixed_result_row, name, "name",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_FIXED_REQ(test_fixed_result_row, group, "group",
                                    LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(test_fixed_result_row_map, test_fixed_result_row,
                    test_fixed_result_row_fields);

static const lonejson_field test_fixed_result_run_fields[] = {
    LONEJSON_FIELD_OBJECT_ARRAY(
        test_fixed_result_run, results, "results", test_fixed_result_row,
        &test_fixed_result_row_map, LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(test_fixed_result_run_map, test_fixed_result_run,
                    test_fixed_result_run_fields);

static const lonejson_field test_aligned_item_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(test_aligned_item, name, "name",
                                    LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(test_aligned_item_map, test_aligned_item,
                    test_aligned_item_fields);

static const lonejson_field test_aligned_doc_fields[] = {
    LONEJSON_FIELD_OBJECT_ARRAY(test_aligned_doc, items, "items",
                                test_aligned_item, &test_aligned_item_map,
                                LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(test_aligned_doc_map, test_aligned_doc,
                    test_aligned_doc_fields);

static const lonejson_field test_omit_reasoning_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_OMIT_NULL(test_omit_reasoning, effort,
                                          "effort"),
    LONEJSON_FIELD_STRING_ALLOC_OMIT_NULL(test_omit_reasoning, summary,
                                          "summary")};
LONEJSON_MAP_DEFINE(test_omit_reasoning_map, test_omit_reasoning,
                    test_omit_reasoning_fields);

static const lonejson_field test_omit_deep_leaf_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_OMIT_NULL(test_omit_deep_leaf, value, "value")};
LONEJSON_MAP_DEFINE(test_omit_deep_leaf_map, test_omit_deep_leaf,
                    test_omit_deep_leaf_fields);

static const lonejson_field test_omit_deep_middle_fields[] = {
    LONEJSON_FIELD_OBJECT_OMIT_EMPTY(test_omit_deep_middle, leaf, "leaf",
                                     &test_omit_deep_leaf_map)};
LONEJSON_MAP_DEFINE(test_omit_deep_middle_map, test_omit_deep_middle,
                    test_omit_deep_middle_fields);

static const lonejson_field test_omit_deep_doc_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(test_omit_deep_doc, id, "id"),
    LONEJSON_FIELD_OBJECT_OMIT_EMPTY(test_omit_deep_doc, middle, "middle",
                                     &test_omit_deep_middle_map)};
LONEJSON_MAP_DEFINE(test_omit_deep_doc_map, test_omit_deep_doc,
                    test_omit_deep_doc_fields);

static const lonejson_field test_omit_doc_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(test_omit_doc, model, "model"),
    LONEJSON_FIELD_STRING_ALLOC_OMIT_NULL(test_omit_doc, instructions,
                                          "instructions"),
    LONEJSON_FIELD_I64_PRESENT(test_omit_doc, max_output_tokens,
                               has_max_output_tokens, "max_output_tokens"),
    LONEJSON_FIELD_U64_PRESENT(test_omit_doc, seed, has_seed, "seed"),
    LONEJSON_FIELD_F64_PRESENT(test_omit_doc, temperature, has_temperature,
                               "temperature"),
    LONEJSON_FIELD_BOOL_PRESENT(test_omit_doc, stream, has_stream, "stream"),
    LONEJSON_FIELD_OBJECT_OMIT_EMPTY(test_omit_doc, reasoning, "reasoning",
                                     &test_omit_reasoning_map),
    LONEJSON_FIELD_STRING_ARRAY_OMIT_EMPTY(test_omit_doc, tags, "tags",
                                           LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_OBJECT_ARRAY_OMIT_EMPTY(test_omit_doc, tools, "tools",
                                           test_item, &test_item_map,
                                           LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_SOURCE_OMIT_NULL(test_omit_doc, payload, "payload"),
    LONEJSON_FIELD_JSON_VALUE_OMIT_NULL(test_omit_doc, metadata, "metadata")};
LONEJSON_MAP_DEFINE(test_omit_doc_map, test_omit_doc, test_omit_doc_fields);

static const lonejson_field test_bad_required_omit_fields[] = {
    {"model", LONEJSON__KEY_LEN("model"), LONEJSON__KEY_FIRST("model"),
     LONEJSON__KEY_LAST("model"), offsetof(test_omit_doc, model),
     LONEJSON_FIELD_KIND_STRING, LONEJSON_STORAGE_DYNAMIC,
     LONEJSON_OVERFLOW_FAIL, LONEJSON_FIELD_REQUIRED | LONEJSON_FIELD_OMIT_NULL,
     0u, 0u, NULL, NULL, 0u}};
LONEJSON_MAP_DEFINE(test_bad_required_omit_map, test_omit_doc,
                    test_bad_required_omit_fields);

static const lonejson_field test_bad_required_presence_fields[] = {
    {"max_output_tokens", LONEJSON__KEY_LEN("max_output_tokens"),
     LONEJSON__KEY_FIRST("max_output_tokens"),
     LONEJSON__KEY_LAST("max_output_tokens"),
     offsetof(test_omit_doc, max_output_tokens), LONEJSON_FIELD_KIND_I64,
     LONEJSON_STORAGE_FIXED, LONEJSON_OVERFLOW_FAIL,
     LONEJSON_FIELD_REQUIRED | LONEJSON_FIELD_HAS_PRESENCE, 0u, 0u, NULL, NULL,
     offsetof(test_omit_doc, has_max_output_tokens)}};
LONEJSON_MAP_DEFINE(test_bad_required_presence_map, test_omit_doc,
                    test_bad_required_presence_fields);

static const lonejson_field test_bad_presence_kind_fields[] = {
    {"instructions", LONEJSON__KEY_LEN("instructions"),
     LONEJSON__KEY_FIRST("instructions"), LONEJSON__KEY_LAST("instructions"),
     offsetof(test_omit_doc, instructions), LONEJSON_FIELD_KIND_STRING,
     LONEJSON_STORAGE_DYNAMIC, LONEJSON_OVERFLOW_FAIL,
     LONEJSON_FIELD_HAS_PRESENCE, 0u, 0u, NULL, NULL,
     offsetof(test_omit_doc, has_stream)}};
LONEJSON_MAP_DEFINE(test_bad_presence_kind_map, test_omit_doc,
                    test_bad_presence_kind_fields);

static const lonejson_field test_bad_presence_offset_fields[] = {
    {"max_output_tokens", LONEJSON__KEY_LEN("max_output_tokens"),
     LONEJSON__KEY_FIRST("max_output_tokens"),
     LONEJSON__KEY_LAST("max_output_tokens"),
     offsetof(test_omit_doc, max_output_tokens), LONEJSON_FIELD_KIND_I64,
     LONEJSON_STORAGE_FIXED, LONEJSON_OVERFLOW_FAIL,
     LONEJSON_FIELD_HAS_PRESENCE, 0u, 0u, NULL, NULL, sizeof(test_omit_doc)}};
LONEJSON_MAP_DEFINE(test_bad_presence_offset_map, test_omit_doc,
                    test_bad_presence_offset_fields);

static const lonejson_field test_pretty_omit_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_OMIT_NULL(test_omit_doc, instructions,
                                          "instructions"),
    LONEJSON_FIELD_I64_PRESENT(test_omit_doc, max_output_tokens,
                               has_max_output_tokens, "max_output_tokens"),
    LONEJSON_FIELD_STRING_ALLOC_REQ(test_omit_doc, model, "model"),
    LONEJSON_FIELD_JSON_VALUE_OMIT_NULL(test_omit_doc, metadata, "metadata")};
LONEJSON_MAP_DEFINE(test_pretty_omit_map, test_omit_doc,
                    test_pretty_omit_fields);

static const lonejson_field test_omit_empty_null_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(test_omit_doc, model, "model"),
    {"instructions", LONEJSON__KEY_LEN("instructions"),
     LONEJSON__KEY_FIRST("instructions"), LONEJSON__KEY_LAST("instructions"),
     offsetof(test_omit_doc, instructions), LONEJSON_FIELD_KIND_STRING,
     LONEJSON_STORAGE_DYNAMIC, LONEJSON_OVERFLOW_FAIL,
     LONEJSON_FIELD_OMIT_EMPTY, 0u, 0u, NULL, NULL, 0u},
    {"payload", LONEJSON__KEY_LEN("payload"), LONEJSON__KEY_FIRST("payload"),
     LONEJSON__KEY_LAST("payload"), offsetof(test_omit_doc, payload),
     LONEJSON_FIELD_KIND_STRING_SOURCE, LONEJSON_STORAGE_DYNAMIC,
     LONEJSON_OVERFLOW_FAIL, LONEJSON_FIELD_OMIT_EMPTY, 0u, 0u, NULL, NULL, 0u},
    {"metadata", LONEJSON__KEY_LEN("metadata"), LONEJSON__KEY_FIRST("metadata"),
     LONEJSON__KEY_LAST("metadata"), offsetof(test_omit_doc, metadata),
     LONEJSON_FIELD_KIND_JSON_VALUE, LONEJSON_STORAGE_DYNAMIC,
     LONEJSON_OVERFLOW_FAIL, LONEJSON_FIELD_OMIT_EMPTY, 0u, 0u, NULL, NULL,
     0u}};
LONEJSON_MAP_DEFINE(test_omit_empty_null_map, test_omit_doc,
                    test_omit_empty_null_fields);

static const lonejson_field test_person_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(test_person, name, "name"),
    LONEJSON_FIELD_STRING_FIXED(test_person, nickname, "nickname",
                                LONEJSON_OVERFLOW_TRUNCATE),
    LONEJSON_FIELD_I64(test_person, age, "age"),
    LONEJSON_FIELD_F64(test_person, score, "score"),
    LONEJSON_FIELD_BOOL(test_person, active, "active"),
    LONEJSON_FIELD_OBJECT_REQ(test_person, address, "address",
                              &test_address_map),
    LONEJSON_FIELD_I64_ARRAY(test_person, lucky_numbers, "lucky_numbers",
                             LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_ARRAY(test_person, tags, "tags",
                                LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_OBJECT_ARRAY(test_person, items, "items", test_item,
                                &test_item_map, LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(test_person_map, test_person, test_person_fields);

static int g_failures = 0;

static lonejson_read_result test_state_reader(void *user, unsigned char *buffer,
                                              size_t capacity) {
  test_reader_state *st = (test_reader_state *)user;
  lonejson_read_result rr;
  size_t remaining = strlen(st->json) - st->offset;
  size_t chunk = st->chunk_size;

  memset(&rr, 0, sizeof(rr));
  if (remaining == 0u) {
    rr.eof = 1;
    return rr;
  }
  if (chunk > remaining) {
    chunk = remaining;
  }
  if (chunk > capacity) {
    chunk = capacity;
  }
  memcpy(buffer, st->json + st->offset, chunk);
  st->offset += chunk;
  rr.bytes_read = chunk;
  rr.eof = (st->json[st->offset] == '\0') ? 1 : 0;
  return rr;
}

static lonejson_read_result
test_framed_reader(void *user, unsigned char *buffer, size_t capacity) {
  test_reader_state *st = (test_reader_state *)user;
  lonejson_read_result rr;
  size_t remaining = strlen(st->json) - st->offset;
  size_t chunk = remaining < st->chunk_size ? remaining : st->chunk_size;

  memset(&rr, 0, sizeof(rr));
  if (chunk > capacity) {
    chunk = capacity;
  }
  if (chunk != 0u) {
    memcpy(buffer, st->json + st->offset, chunk);
    st->offset += chunk;
    rr.bytes_read = chunk;
  }
  rr.eof = (st->json[st->offset] == '\0') ? 1 : 0;
  return rr;
}

static char *make_repeating_text(size_t len) {
  static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789-_:";
  char *text;
  size_t i;

  text = (char *)malloc(len + 1u);
  if (text == NULL) {
    return NULL;
  }
  for (i = 0u; i < len; ++i) {
    text[i] = alphabet[i % (sizeof(alphabet) - 1u)];
  }
  text[len] = '\0';
  return text;
}

static char *base64_encode_bytes(const unsigned char *data, size_t len) {
  static const char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  char *encoded;
  size_t out_len;
  size_t in;
  size_t out;

  out_len = ((len + 2u) / 3u) * 4u;
  encoded = (char *)malloc(out_len + 1u);
  if (encoded == NULL) {
    return NULL;
  }
  in = 0u;
  out = 0u;
  while (in < len) {
    unsigned char a = data[in++];
    unsigned char b = 0u;
    unsigned char c = 0u;
    int have_b = 0;
    int have_c = 0;

    if (in < len) {
      b = data[in++];
      have_b = 1;
    }
    if (in < len) {
      c = data[in++];
      have_c = 1;
    }

    encoded[out++] = alphabet[(a >> 2u) & 0x3Fu];
    encoded[out++] = alphabet[((a & 0x03u) << 4u) | (b >> 4u)];
    encoded[out++] = have_b ? alphabet[((b & 0x0Fu) << 2u) | (c >> 6u)] : '=';
    encoded[out++] = have_c ? alphabet[c & 0x3Fu] : '=';
  }
  encoded[out] = '\0';
  return encoded;
}

static char *make_spool_json(const char *id, const char *text,
                             const char *base64_text) {
  const char *fmt = "{\"id\":\"%s\",\"text\":\"%s\",\"bytes\":\"%s\"}";
  size_t need = snprintf(NULL, 0, fmt, id, text, base64_text);
  char *json = (char *)malloc(need + 1u);

  if (json == NULL) {
    return NULL;
  }
  snprintf(json, need + 1u, fmt, id, text, base64_text);
  return json;
}

static size_t read_spooled_all(lonejson_spooled *value, unsigned char *buffer,
                               size_t capacity) {
  size_t total;
  lonejson_error error;

  total = 0u;
  if (lonejson_spooled_rewind(value, &error) != LONEJSON_STATUS_OK) {
    return 0u;
  }
  for (;;) {
    lonejson_read_result chunk =
        lonejson_spooled_read(value, buffer + total, capacity - total);
    if (chunk.error_code != 0) {
      return total;
    }
    total += chunk.bytes_read;
    if (chunk.eof) {
      break;
    }
    if (total >= capacity) {
      break;
    }
  }
  return total;
}

static lonejson_status test_counting_sink_write(void *user, const void *data,
                                                size_t len,
                                                lonejson_error *error) {
  test_counting_sink *sink = (test_counting_sink *)user;
  (void)data;
  (void)error;
  sink->total += len;
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_buffer_sink_write(void *user, const void *data,
                                              size_t len,
                                              lonejson_error *error) {
  test_buffer_sink *sink = (test_buffer_sink *)user;

  if (sink->length + len >= sink->capacity) {
    return lonejson__set_error(error, LONEJSON_STATUS_TRUNCATED, 0u, 0u, 0u,
                               "test sink buffer too small");
  }
  memcpy(sink->buffer + sink->length, data, len);
  sink->length += len;
  sink->buffer[sink->length] = '\0';
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_failing_sink_write(void *user, const void *data,
                                               size_t len,
                                               lonejson_error *error) {
  test_failing_sink *sink = (test_failing_sink *)user;
  (void)data;
  sink->total += len;
  if (sink->total >= sink->fail_after) {
    return lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u,
                               0u, "intentional sink failure");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_visit_append(test_visit_state *state,
                                         const char *text) {
  size_t len = strlen(text);
  if (state->len + len + 1u >= sizeof(state->log)) {
    return LONEJSON_STATUS_OVERFLOW;
  }
  memcpy(state->log + state->len, text, len);
  state->len += len;
  state->log[state->len] = '\0';
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_visit_object_begin(void *user,
                                               lonejson_error *error) {
  (void)error;
  return test_visit_append((test_visit_state *)user, "{");
}

static lonejson_status test_visit_object_end(void *user,
                                             lonejson_error *error) {
  (void)error;
  return test_visit_append((test_visit_state *)user, "}");
}

static lonejson_status test_visit_array_begin(void *user,
                                              lonejson_error *error) {
  (void)error;
  return test_visit_append((test_visit_state *)user, "[");
}

static lonejson_status test_visit_array_end(void *user, lonejson_error *error) {
  (void)error;
  return test_visit_append((test_visit_state *)user, "]");
}

static lonejson_status test_visit_key_begin(void *user, lonejson_error *error) {
  (void)error;
  return test_visit_append((test_visit_state *)user, "K(");
}

static lonejson_status test_visit_key_chunk(void *user, const char *data,
                                            size_t len, lonejson_error *error) {
  test_visit_state *state = (test_visit_state *)user;
  (void)error;
  if (state->len + len + 1u >= sizeof(state->log)) {
    return LONEJSON_STATUS_OVERFLOW;
  }
  memcpy(state->log + state->len, data, len);
  state->len += len;
  state->log[state->len] = '\0';
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_visit_key_end(void *user, lonejson_error *error) {
  (void)error;
  return test_visit_append((test_visit_state *)user, ")");
}

static lonejson_status test_visit_string_begin(void *user,
                                               lonejson_error *error) {
  (void)error;
  return test_visit_append((test_visit_state *)user, "S(");
}

static lonejson_status test_visit_string_end(void *user,
                                             lonejson_error *error) {
  (void)error;
  return test_visit_append((test_visit_state *)user, ")");
}

static lonejson_status test_visit_number_begin(void *user,
                                               lonejson_error *error) {
  (void)error;
  return test_visit_append((test_visit_state *)user, "N(");
}

static lonejson_status test_visit_number_end(void *user,
                                             lonejson_error *error) {
  (void)error;
  return test_visit_append((test_visit_state *)user, ")");
}

static lonejson_status test_visit_bool(void *user, int value,
                                       lonejson_error *error) {
  (void)error;
  return test_visit_append((test_visit_state *)user, value ? "T" : "F");
}

static lonejson_status test_visit_null(void *user, lonejson_error *error) {
  (void)error;
  return test_visit_append((test_visit_state *)user, "Z");
}

static lonejson_value_visitor test_value_visitor(void) {
  lonejson_value_visitor visitor = lonejson_default_value_visitor();
  visitor.object_begin = test_visit_object_begin;
  visitor.object_end = test_visit_object_end;
  visitor.object_key_begin = test_visit_key_begin;
  visitor.object_key_chunk = test_visit_key_chunk;
  visitor.object_key_end = test_visit_key_end;
  visitor.array_begin = test_visit_array_begin;
  visitor.array_end = test_visit_array_end;
  visitor.string_begin = test_visit_string_begin;
  visitor.string_chunk = test_visit_key_chunk;
  visitor.string_end = test_visit_string_end;
  visitor.number_begin = test_visit_number_begin;
  visitor.number_chunk = test_visit_key_chunk;
  visitor.number_end = test_visit_number_end;
  visitor.boolean_value = test_visit_bool;
  visitor.null_value = test_visit_null;
  return visitor;
}

static lonejson_status
test_visit_counter_maybe_fail(test_visit_chunk_state *state,
                              lonejson_error *error) {
  ++state->callback_count;
  if (state->fail_after != 0u && state->callback_count >= state->fail_after) {
    return lonejson__set_error(error, LONEJSON_STATUS_CALLBACK_FAILED, 0u, 0u,
                               0u, "intentional visitor failure");
  }
  return LONEJSON_STATUS_OK;
}

static lonejson_status test_visit_count_object_begin(void *user,
                                                     lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  ++state->object_count;
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_status test_visit_count_object_end(void *user,
                                                   lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_status test_visit_count_array_begin(void *user,
                                                    lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  ++state->array_count;
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_status test_visit_count_array_end(void *user,
                                                  lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_status test_visit_count_key_begin(void *user,
                                                  lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  ++state->key_begin_count;
  state->last_key_len = 0u;
  state->last_key[0] = '\0';
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_status test_visit_count_key_chunk(void *user, const char *data,
                                                  size_t len,
                                                  lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  size_t copy_len = len;

  ++state->key_chunk_count;
  if (state->last_key_len + copy_len >= sizeof(state->last_key)) {
    copy_len = sizeof(state->last_key) - state->last_key_len - 1u;
  }
  if (copy_len != 0u) {
    memcpy(state->last_key + state->last_key_len, data, copy_len);
    state->last_key_len += copy_len;
    state->last_key[state->last_key_len] = '\0';
  }
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_status test_visit_count_key_end(void *user,
                                                lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  ++state->key_end_count;
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_status test_visit_count_string_begin(void *user,
                                                     lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  ++state->string_begin_count;
  state->last_string_len = 0u;
  state->last_string[0] = '\0';
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_status test_visit_count_string_chunk(void *user,
                                                     const char *data,
                                                     size_t len,
                                                     lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  size_t copy_len = len;

  ++state->string_chunk_count;
  if (state->last_string_len + copy_len >= sizeof(state->last_string)) {
    copy_len = sizeof(state->last_string) - state->last_string_len - 1u;
  }
  if (copy_len != 0u) {
    memcpy(state->last_string + state->last_string_len, data, copy_len);
    state->last_string_len += copy_len;
    state->last_string[state->last_string_len] = '\0';
  }
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_status test_visit_count_string_end(void *user,
                                                   lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  ++state->string_end_count;
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_status test_visit_count_number_begin(void *user,
                                                     lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  ++state->number_begin_count;
  state->last_number_len = 0u;
  state->last_number[0] = '\0';
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_status test_visit_count_number_chunk(void *user,
                                                     const char *data,
                                                     size_t len,
                                                     lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  size_t copy_len = len;

  ++state->number_chunk_count;
  if (state->last_number_len + copy_len >= sizeof(state->last_number)) {
    copy_len = sizeof(state->last_number) - state->last_number_len - 1u;
  }
  if (copy_len != 0u) {
    memcpy(state->last_number + state->last_number_len, data, copy_len);
    state->last_number_len += copy_len;
    state->last_number[state->last_number_len] = '\0';
  }
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_status test_visit_count_number_end(void *user,
                                                   lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  ++state->number_end_count;
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_status test_visit_count_bool(void *user, int value,
                                             lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  (void)value;
  ++state->bool_count;
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_status test_visit_count_null(void *user,
                                             lonejson_error *error) {
  test_visit_chunk_state *state = (test_visit_chunk_state *)user;
  ++state->null_count;
  return test_visit_counter_maybe_fail(state, error);
}

static lonejson_value_visitor test_counting_value_visitor(void) {
  lonejson_value_visitor visitor = lonejson_default_value_visitor();
  visitor.object_begin = test_visit_count_object_begin;
  visitor.object_end = test_visit_count_object_end;
  visitor.object_key_begin = test_visit_count_key_begin;
  visitor.object_key_chunk = test_visit_count_key_chunk;
  visitor.object_key_end = test_visit_count_key_end;
  visitor.array_begin = test_visit_count_array_begin;
  visitor.array_end = test_visit_count_array_end;
  visitor.string_begin = test_visit_count_string_begin;
  visitor.string_chunk = test_visit_count_string_chunk;
  visitor.string_end = test_visit_count_string_end;
  visitor.number_begin = test_visit_count_number_begin;
  visitor.number_chunk = test_visit_count_number_chunk;
  visitor.number_end = test_visit_count_number_end;
  visitor.boolean_value = test_visit_count_bool;
  visitor.null_value = test_visit_count_null;
  return visitor;
}

static int write_temp_text_file(char *path_template, const char *text) {
  int fd;

  fd = mkstemp(path_template);
  if (fd < 0) {
    return -1;
  }
  if (write(fd, text, strlen(text)) != (ssize_t)strlen(text)) {
    close(fd);
    unlink(path_template);
    return -1;
  }
  return fd;
}

static char *fixture_path(const char *name) {
  const char *root = LONEJSON_SOURCE_DIR;
  const char *prefix = "/tests/fixtures/";
  size_t len = strlen(root) + strlen(prefix) + strlen(name) + 1u;
  char *path = (char *)malloc(len);

  if (path == NULL) {
    return NULL;
  }
  strcpy(path, root);
  strcat(path, prefix);
  strcat(path, name);
  return path;
}

static char *generated_fixture_path(const char *name) {
  const char *root = LONEJSON_GENERATED_FIXTURE_DIR;
  size_t len = strlen(root) + 1u + strlen(name) + 1u;
  char *path = (char *)malloc(len);

  if (path == NULL) {
    return NULL;
  }
  strcpy(path, root);
  strcat(path, "/");
  strcat(path, name);
  return path;
}

static char *vendor_fixture_path(const char *name) {
  const char *root = LONEJSON_SOURCE_DIR;
  const char *prefix = "/tests/fixtures/vendor/json_test_suite/test_parsing/";
  size_t len = strlen(root) + strlen(prefix) + strlen(name) + 1u;
  char *path = (char *)malloc(len);

  if (path == NULL) {
    return NULL;
  }
  strcpy(path, root);
  strcat(path, prefix);
  strcat(path, name);
  return path;
}

static void expect_true(int cond, const char *expr, const char *file,
                        int line) {
  if (!cond) {
    fprintf(stderr, "%s:%d: expectation failed: %s\n", file, line, expr);
    g_failures++;
  }
}

#define EXPECT(expr) expect_true((expr), #expr, __FILE__, __LINE__)

static void poison_bytes(void *ptr, size_t len, unsigned char value) {
  unsigned char *bytes = (unsigned char *)ptr;
  size_t i;

  for (i = 0; i < len; ++i) {
    bytes[i] = value;
  }
}

static void test_parse_implicit_destination_reset(void) {
  const char *json =
      "{\"name\":\"Reset\",\"address\":{\"city\":\"Oslo\",\"zip\":42},"
      "\"active\":false,\"lucky_numbers\":[],\"tags\":[],\"items\":[]}";
  test_person person;
  lonejson_error error;
  lonejson_status status;

  poison_bytes(&person, sizeof(person), 0xA5u);
  status = lonejson_parse_cstr(&test_person_map, &person, json, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(person.name != NULL && strcmp(person.name, "Reset") == 0);
  EXPECT(person.nickname[0] == '\0');
  EXPECT(person.age == 0);
  EXPECT(person.score == 0.0);
  EXPECT(person.active == false);
  EXPECT(strcmp(person.address.city, "Oslo") == 0);
  EXPECT(person.address.zip == 42);
  EXPECT(person.lucky_numbers.count == 0);
  EXPECT(person.tags.count == 0);
  EXPECT(person.items.count == 0);
  lonejson_cleanup(&test_person_map, &person);
}

static void test_dynamic_allocation_cleanup_balance(void) {
  const char *json =
      "{\"name\":\"Balance\",\"nickname\":\"bal\",\"age\":9,\"score\":9.5,"
      "\"active\":true,\"address\":{\"city\":\"Rome\",\"zip\":900},"
      "\"lucky_numbers\":[1,2,3,4,5],\"tags\":[\"a\",\"b\",\"c\"],"
      "\"items\":[{\"id\":1,\"label\":\"first\"},{\"id\":2,\"label\":"
      "\"second\"}]}";
  test_person person;
  lonejson_error error;
  lonejson_status status;

  reset_lonejson_alloc_stats();
  poison_bytes(&person, sizeof(person), 0x11u);
  status = lonejson_parse_cstr(&test_person_map, &person, json, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  EXPECT(g_lonejson_alloc_calls > 0u);
  EXPECT(g_alloc_record_count > 0u);
  lonejson_cleanup(&test_person_map, &person);
  EXPECT(g_alloc_record_count == 0u);
  EXPECT(g_lonejson_free_calls > 0u);
}

static void test_zero_alloc_validate_path(void) {
  lonejson_error error;
  lonejson_status status;

  reset_lonejson_alloc_stats();
  status = lonejson_validate_cstr("{\"ok\":true,\"items\":[1,2,3]}", &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);
}

static void test_parser_workspace_accounting(void) {
  const char *json = "{\"name\":\"workspace-check\",\"codes\":[5,8,13,21]}";
  test_fixed_log log;
  lonejson_int64 codes_storage[4];
  lonejson_parser parser_storage;
  unsigned char parser_workspace[LONEJSON_PARSER_BUFFER_SIZE];
  lonejson_error error;
  lonejson_status status;

  memset(&log, 0, sizeof(log));
  lonejson_init(&test_fixed_log_map, &log);
  log.codes.items = codes_storage;
  log.codes.capacity = 4u;
  log.codes.flags = LONEJSON_ARRAY_FIXED_CAPACITY;

  reset_lonejson_alloc_stats();
  lonejson__parser_init_state(&parser_storage, &test_fixed_log_map, &log, NULL,
                              0, parser_workspace, sizeof(parser_workspace));
  status = lonejson__parser_feed_bytes(
      &parser_storage, (const unsigned char *)json, strlen(json), NULL, 0);
  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  status = lonejson_parser_finish(&parser_storage);
  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  error = parser_storage.error;
  EXPECT(error.code == LONEJSON_STATUS_OK || error.code == status);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);
  EXPECT(lonejson__parser_peak_workspace_used(&parser_storage) > 0u);
  EXPECT(lonejson__parser_peak_workspace_used(&parser_storage) <=
         (size_t)LONEJSON_PARSER_BUFFER_SIZE);
  lonejson_cleanup(&test_fixed_log_map, &log);
}

static void test_parser_workspace_alignment_regression(void) {
  const char *json = "{\"name\":\"aligned\",\"codes\":[1,2]}";
  lonejson_parser parser_storage;
  unsigned char raw_workspace[LONEJSON_PARSER_BUFFER_SIZE +
                              LONEJSON__PARSER_WORKSPACE_ALIGNMENT];
  unsigned char *misaligned_workspace = raw_workspace + 1u;
  test_fixed_log log;
  lonejson_int64 codes_storage[4];
  lonejson_status status;
  size_t alignment_offset;

  memset(&log, 0, sizeof(log));
  lonejson_init(&test_fixed_log_map, &log);
  log.codes.items = codes_storage;
  log.codes.capacity = 4u;
  log.codes.flags = LONEJSON_ARRAY_FIXED_CAPACITY;

  lonejson__parser_init_state(&parser_storage, &test_fixed_log_map, &log, NULL,
                              0, misaligned_workspace,
                              LONEJSON_PARSER_BUFFER_SIZE);
  EXPECT(parser_storage.workspace != misaligned_workspace);
  EXPECT(parser_storage.workspace_size <= LONEJSON_PARSER_BUFFER_SIZE);
  alignment_offset = (size_t)(parser_storage.workspace - misaligned_workspace);
  EXPECT(alignment_offset < LONEJSON__PARSER_WORKSPACE_ALIGNMENT);
  EXPECT(parser_storage.frames == (lonejson_frame *)parser_storage.workspace);
  EXPECT(parser_storage.workspace ==
         lonejson__align_pointer(misaligned_workspace,
                                 LONEJSON__PARSER_WORKSPACE_ALIGNMENT));

  status = lonejson__parser_feed_bytes(
      &parser_storage, (const unsigned char *)json, strlen(json), NULL, 0);
  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  status = lonejson_parser_finish(&parser_storage);
  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  EXPECT(strcmp(log.name, "aligned") == 0);
  EXPECT(log.codes.count == 2u);
  EXPECT(log.codes.items[1] == 2);

  lonejson_cleanup(&test_fixed_log_map, &log);
}

static void test_init_does_not_preserve_garbage_array_state(void) {
  test_person person;

  poison_bytes(&person, sizeof(person), 0x7bu);
  person.items.items = (test_item *)0x10f;
  person.items.count = 99u;
  person.items.capacity = 99u;
  person.items.elem_size = sizeof(test_item);
  person.items.flags = LONEJSON_ARRAY_FIXED_CAPACITY;

  lonejson_init(&test_person_map, &person);

  EXPECT(person.name == NULL);
  EXPECT(person.nickname[0] == '\0');
  EXPECT(person.items.items == NULL);
  EXPECT(person.items.count == 0u);
  EXPECT(person.items.capacity == 0u);
  EXPECT(person.items.flags == 0u);
  EXPECT(person.items.elem_size == sizeof(test_item));

  lonejson_cleanup(&test_person_map, &person);
}

static void test_zero_alloc_fixed_storage_parse(void) {
  const char *json = "{\"name\":\"long-event-name\",\"codes\":[5,8,13]}";
  test_fixed_log log;
  lonejson_int64 codes_storage[4];
  lonejson_error error;
  lonejson_status status;
  lonejson_parse_options options = lonejson_default_parse_options();

  memset(&log, 0, sizeof(log));
  lonejson_init(&test_fixed_log_map, &log);
  log.codes.items = codes_storage;
  log.codes.capacity = 4u;
  log.codes.flags = LONEJSON_ARRAY_FIXED_CAPACITY;
  options.clear_destination = 0;

  reset_lonejson_alloc_stats();
  status =
      lonejson_parse_cstr(&test_fixed_log_map, &log, json, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  EXPECT(strcmp(log.name, "long-ev") == 0);
  EXPECT(log.codes.count == 3u);
  EXPECT(log.codes.items[2] == 13);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);
  lonejson_cleanup(&test_fixed_log_map, &log);
}

static void test_zero_alloc_fixed_storage_serialize(void) {
  test_fixed_log log;
  lonejson_int64 codes_storage[4];
  test_counting_sink sink;
  char buffer[128];
  char path[] = "/tmp/lonejson-serialize-XXXXXX";
  lonejson_error error;
  lonejson_status status;
  int fd;
  FILE *fp;

  memset(&log, 0, sizeof(log));
  memset(codes_storage, 0, sizeof(codes_storage));
  memset(&sink, 0, sizeof(sink));
  strcpy(log.name, "evt");
  codes_storage[0] = 5;
  codes_storage[1] = 8;
  codes_storage[2] = 13;
  log.codes.items = codes_storage;
  log.codes.count = 3u;
  log.codes.capacity = 4u;
  log.codes.flags = LONEJSON_ARRAY_FIXED_CAPACITY;

  reset_lonejson_alloc_stats();
  status = lonejson_serialize_sink(
      &test_fixed_log_map, &log, test_counting_sink_write, &sink, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(sink.total != 0u);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);

  reset_lonejson_alloc_stats();
  status = lonejson_serialize_buffer(&test_fixed_log_map, &log, buffer,
                                     sizeof(buffer), NULL, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, "{\"name\":\"evt\",\"codes\":[5,8,13]}") == 0);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);

  fd = mkstemp(path);
  EXPECT(fd >= 0);
  if (fd < 0) {
    return;
  }
  fp = fdopen(fd, "wb");
  EXPECT(fp != NULL);
  if (fp == NULL) {
    close(fd);
    unlink(path);
    return;
  }
  reset_lonejson_alloc_stats();
  status =
      lonejson_serialize_filep(&test_fixed_log_map, &log, fp, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);
  fclose(fp);

  reset_lonejson_alloc_stats();
  status =
      lonejson_serialize_path(&test_fixed_log_map, &log, path, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);
  unlink(path);
}

static void test_zero_alloc_jsonl_serialize(void) {
  test_event events[2];
  test_counting_sink sink;
  char buffer[128];
  char path[] = "/tmp/lonejson-jsonl-serialize-XXXXXX";
  lonejson_error error;
  lonejson_status status;
  int fd;
  FILE *fp;

  memset(events, 0, sizeof(events));
  memset(&sink, 0, sizeof(sink));
  strcpy(events[0].id, "evt-1");
  events[0].ok = true;
  strcpy(events[1].id, "evt-2");
  events[1].ok = false;

  reset_lonejson_alloc_stats();
  status = lonejson_serialize_jsonl_sink(&test_event_map, events, 2u, 0u,
                                         test_counting_sink_write, &sink, NULL,
                                         &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(sink.total != 0u);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);

  reset_lonejson_alloc_stats();
  status =
      lonejson_serialize_jsonl_buffer(&test_event_map, events, 2u, 0u, buffer,
                                      sizeof(buffer), NULL, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, "{\"id\":\"evt-1\",\"ok\":true}\n"
                        "{\"id\":\"evt-2\",\"ok\":false}\n") == 0);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);

  fd = mkstemp(path);
  EXPECT(fd >= 0);
  if (fd < 0) {
    return;
  }
  fp = fdopen(fd, "wb");
  EXPECT(fp != NULL);
  if (fp == NULL) {
    close(fd);
    unlink(path);
    return;
  }
  reset_lonejson_alloc_stats();
  status = lonejson_serialize_jsonl_filep(&test_event_map, events, 2u, 0u, fp,
                                          NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);
  fclose(fp);

  reset_lonejson_alloc_stats();
  status = lonejson_serialize_jsonl_path(&test_event_map, events, 2u, 0u, path,
                                         NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);
  unlink(path);
}

static void test_serialize_alloc_balance(void) {
  test_fixed_log log;
  lonejson_int64 codes_storage[2];
  lonejson_error error;
  char *json;

  memset(&log, 0, sizeof(log));
  strcpy(log.name, "evt");
  codes_storage[0] = 1;
  codes_storage[1] = 2;
  log.codes.items = codes_storage;
  log.codes.count = 2u;
  log.codes.capacity = 2u;
  log.codes.flags = LONEJSON_ARRAY_FIXED_CAPACITY;

  reset_lonejson_alloc_stats();
  json =
      lonejson_serialize_alloc(&test_fixed_log_map, &log, NULL, NULL, &error);
  EXPECT(json != NULL);
  EXPECT(g_lonejson_alloc_calls > 0u);
  EXPECT(g_alloc_record_count == 1u);
  if (json != NULL) {
    LONEJSON_FREE(json);
  }
  EXPECT(g_alloc_record_count == 0u);
  EXPECT(g_lonejson_free_calls > 0u);
}

static int test_child_serialize_alloc_free_compatibility(void) {
  test_fixed_log log;
  lonejson_int64 codes_storage[2];
  lonejson_error error;
  char *json;

  memset(&log, 0, sizeof(log));
  strcpy(log.name, "evt");
  codes_storage[0] = 1;
  codes_storage[1] = 2;
  log.codes.items = codes_storage;
  log.codes.count = 2u;
  log.codes.capacity = 2u;
  log.codes.flags = LONEJSON_ARRAY_FIXED_CAPACITY;

  reset_lonejson_alloc_stats();
  json =
      lonejson_serialize_alloc(&test_fixed_log_map, &log, NULL, NULL, &error);
  if (json == NULL) {
    return 2;
  }
  LONEJSON_FREE(json);
  return (g_alloc_record_count == 0u) ? 0 : 3;
}

static void test_serialize_alloc_default_free_compatibility(void) {
  pid_t pid;
  int status;

  pid = fork();
  EXPECT(pid >= 0);
  if (pid == 0) {
    _exit(test_child_serialize_alloc_free_compatibility());
  }
  if (pid < 0) {
    return;
  }
  EXPECT(waitpid(pid, &status, 0) == pid);
  EXPECT(WIFEXITED(status));
  if (WIFEXITED(status)) {
    EXPECT(WEXITSTATUS(status) == 0);
  }
}

static void test_serialize_jsonl_alloc_balance(void) {
  test_event events[2];
  lonejson_error error;
  char *jsonl;

  memset(events, 0, sizeof(events));
  strcpy(events[0].id, "evt-1");
  events[0].ok = true;
  strcpy(events[1].id, "evt-2");
  events[1].ok = false;

  reset_lonejson_alloc_stats();
  jsonl = lonejson_serialize_jsonl_alloc(&test_event_map, events, 2u, 0u, NULL,
                                         NULL, &error);
  EXPECT(jsonl != NULL);
  EXPECT(g_lonejson_alloc_calls > 0u);
  EXPECT(g_alloc_record_count == 1u);
  if (jsonl != NULL) {
    LONEJSON_FREE(jsonl);
  }
  EXPECT(g_alloc_record_count == 0u);
  EXPECT(g_lonejson_free_calls > 0u);
}

static int test_child_serialize_jsonl_alloc_free_compatibility(void) {
  test_event events[2];
  lonejson_error error;
  char *jsonl;

  memset(events, 0, sizeof(events));
  strcpy(events[0].id, "evt-1");
  events[0].ok = true;
  strcpy(events[1].id, "evt-2");
  events[1].ok = false;

  reset_lonejson_alloc_stats();
  jsonl = lonejson_serialize_jsonl_alloc(&test_event_map, events, 2u, 0u, NULL,
                                         NULL, &error);
  if (jsonl == NULL) {
    return 2;
  }
  LONEJSON_FREE(jsonl);
  return (g_alloc_record_count == 0u) ? 0 : 3;
}

static void test_serialize_jsonl_alloc_default_free_compatibility(void) {
  pid_t pid;
  int status;

  pid = fork();
  EXPECT(pid >= 0);
  if (pid == 0) {
    _exit(test_child_serialize_jsonl_alloc_free_compatibility());
  }
  if (pid < 0) {
    return;
  }
  EXPECT(waitpid(pid, &status, 0) == pid);
  EXPECT(WIFEXITED(status));
  if (WIFEXITED(status)) {
    EXPECT(WEXITSTATUS(status) == 0);
  }
}

static void test_dynamic_object_array_alignment(void) {
  static const char json[] =
      "{\"items\":[{\"name\":\"a\"},{\"name\":\"b\"},{\"name\":\"c\"}]}";
  test_aligned_doc doc;
  lonejson_error error;
  lonejson_status status;
  struct align_probe {
    char c;
    test_aligned_item value;
  };
  size_t alignment;
  uintptr_t bits;

  memset(&doc, 0, sizeof(doc));
  status = lonejson_parse_cstr(&test_aligned_doc_map, &doc, json, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(doc.items.count == 3u);
  EXPECT(doc.items.items != NULL);
  if (doc.items.items != NULL) {
    alignment = offsetof(struct align_probe, value);
    bits = (uintptr_t)doc.items.items;
    EXPECT((bits % alignment) == 0u);
    EXPECT(strcmp(((test_aligned_item *)doc.items.items)[0].name, "a") == 0);
    ((test_aligned_item *)doc.items.items)[2].weight = (long double)3.75;
    EXPECT(((test_aligned_item *)doc.items.items)[2].weight > 3.7);
  }
  lonejson_cleanup(&test_aligned_doc_map, &doc);
}

static void test_dynamic_allocation_reset_reparse_balance(void) {
  const char *json_a =
      "{\"name\":\"First\",\"nickname\":\"aa\",\"age\":1,\"score\":1.0,"
      "\"active\":false,\"address\":{\"city\":\"A\",\"zip\":1},"
      "\"lucky_numbers\":[1],\"tags\":[\"x\"],"
      "\"items\":[{\"id\":10,\"label\":\"one\"}]}";
  const char *json_b =
      "{\"name\":\"Second\",\"nickname\":\"bb\",\"age\":2,\"score\":2.0,"
      "\"active\":true,\"address\":{\"city\":\"B\",\"zip\":2},"
      "\"lucky_numbers\":[7,8],\"tags\":[\"y\",\"z\"],"
      "\"items\":[{\"id\":20,\"label\":\"two\"},{\"id\":30,\"label\":\"three\"}"
      "]}";
  test_person person;
  lonejson_error error;
  lonejson_status status;

  reset_lonejson_alloc_stats();
  poison_bytes(&person, sizeof(person), 0x22u);
  status = lonejson_parse_cstr(&test_person_map, &person, json_a, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  lonejson_reset(&test_person_map, &person);
  EXPECT(g_alloc_record_count == 0u);
  status = lonejson_parse_cstr(&test_person_map, &person, json_b, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  EXPECT(person.name != NULL && strcmp(person.name, "Second") == 0);
  EXPECT(person.tags.count == 2);
  lonejson_cleanup(&test_person_map, &person);
  EXPECT(g_alloc_record_count == 0u);
}

static void test_parse_buffer_basic(void) {
  const char *json = "{"
                     "\"name\":\"Alice\","
                     "\"nickname\":\"Wonderland\","
                     "\"age\":34,"
                     "\"score\":99.5,"
                     "\"active\":true,"
                     "\"address\":{\"city\":\"Stockholm\",\"zip\":12345},"
                     "\"lucky_numbers\":[7,11,42],"
                     "\"tags\":[\"admin\",\"ops\"],"
                     "\"items\":[{\"id\":1,\"label\":\"alpha\"},{\"id\":2,"
                     "\"label\":\"bravo-longer-than-fit\"}],"
                     "\"ignored\":{\"nested\":true}"
                     "}";
  test_person person;
  lonejson_error error;
  lonejson_status status;

  status = lonejson_parse_cstr(&test_person_map, &person, json, NULL, &error);

  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  EXPECT(person.name != NULL && strcmp(person.name, "Alice") == 0);
  EXPECT(strcmp(person.nickname, "Wonderl") == 0);
  EXPECT(person.age == 34);
  EXPECT(person.score > 99.4 && person.score < 99.6);
  EXPECT(person.active == true);
  EXPECT(strcmp(person.address.city, "Stockholm") == 0);
  EXPECT(person.address.zip == 12345);
  EXPECT(person.lucky_numbers.count == 3);
  EXPECT(person.lucky_numbers.items[0] == 7);
  EXPECT(person.tags.count == 2);
  EXPECT(strcmp(person.tags.items[1], "ops") == 0);
  EXPECT(person.items.count == 2);
  EXPECT(((test_item *)person.items.items)[0].id == 1);
  EXPECT(strcmp(((test_item *)person.items.items)[1].label, "bravo-longe") ==
         0);

  lonejson_cleanup(&test_person_map, &person);
}

static void test_parse_fixed_strings_with_escapes_and_truncation(void) {
  const char *json = "{"
                     "\"name\":\"Alice\","
                     "\"nickname\":\"AB\\nCDEFG\","
                     "\"age\":1,"
                     "\"score\":1.0,"
                     "\"active\":false,"
                     "\"address\":{\"city\":\"M\\u00f6lle\",\"zip\":7},"
                     "\"lucky_numbers\":[],"
                     "\"tags\":[],"
                     "\"items\":[]"
                     "}";
  test_person person;
  lonejson_error error;
  lonejson_status status;

  status = lonejson_parse_cstr(&test_person_map, &person, json, NULL, &error);

  EXPECT(status == LONEJSON_STATUS_TRUNCATED);
  EXPECT(person.name != NULL && strcmp(person.name, "Alice") == 0);
  EXPECT(strcmp(person.nickname, "AB\nCDEF") == 0);
  EXPECT(strcmp(person.address.city, "M\303\266lle") == 0);
  EXPECT(person.address.zip == 7);

  lonejson_cleanup(&test_person_map, &person);
}

static void test_chunked_parser_and_missing_required(void) {
  test_reader_state state = {
      "{\"name\":\"Bob\",\"address\":{\"city\":\"Uppsala\",\"zip\":77},"
      "\"active\":false,\"lucky_numbers\":[1,2],\"tags\":[],\"items\":[]}",
      0u, 9u};
  test_person person;
  lonejson_stream *stream;
  lonejson_error error;
  lonejson_status status;
  lonejson_stream_result result;

  poison_bytes(&person, sizeof(person), 0x5Au);
  stream = lonejson_stream_open_reader(&test_person_map, test_state_reader,
                                       &state, NULL, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    return;
  }

  result = lonejson_stream_next(stream, &person, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(person.name != NULL && strcmp(person.name, "Bob") == 0);
  EXPECT(strcmp(person.address.city, "Uppsala") == 0);
  lonejson_stream_close(stream);
  lonejson_cleanup(&test_person_map, &person);

  poison_bytes(&person, sizeof(person), 0x5Au);
  status = lonejson_parse_cstr(&test_person_map, &person, "{\"age\":1}", NULL,
                               &error);
  EXPECT(status == LONEJSON_STATUS_MISSING_REQUIRED_FIELD);
  lonejson_cleanup(&test_person_map, &person);
}

static void test_object_framed_stream_jsonl(void) {
  test_reader_state state = {
      " \n{\"name\":\"One\",\"address\":{\"city\":\"A\",\"zip\":1},"
      "\"active\":true,\"lucky_numbers\":[],\"tags\":[],\"items\":[]}"
      "{\"name\":\"Two\",\"address\":{\"city\":\"B\",\"zip\":2},"
      "\"active\":false,\"lucky_numbers\":[],\"tags\":[],\"items\":[]}\n"
      "{\n  \"name\":\"Three\",\n  \"address\":{\"city\":\"C\",\"zip\":3},\n"
      "  \"active\":true,\n  \"lucky_numbers\":[],\n  \"tags\":[],\n"
      "  \"items\":[]\n}\n",
      0u, 7u};
  test_person person;
  lonejson_stream *stream;
  lonejson_error error;
  lonejson_stream_result result;

  stream = lonejson_stream_open_reader(&test_person_map, test_framed_reader,
                                       &state, NULL, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    return;
  }

  result = lonejson_stream_next(stream, &person, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(strcmp(person.name, "One") == 0);
  lonejson_cleanup(&test_person_map, &person);

  result = lonejson_stream_next(stream, &person, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(strcmp(person.name, "Two") == 0);
  lonejson_cleanup(&test_person_map, &person);

  result = lonejson_stream_next(stream, &person, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(strcmp(person.name, "Three") == 0);
  lonejson_cleanup(&test_person_map, &person);

  result = lonejson_stream_next(stream, &person, &error);
  EXPECT(result == LONEJSON_STREAM_EOF);
  lonejson_stream_close(stream);
}

static void test_stream_reuses_and_changes_destination(void) {
  test_reader_state state = {
      "{\"name\":\"One\",\"address\":{\"city\":\"A\",\"zip\":1},"
      "\"active\":true,\"lucky_numbers\":[],\"tags\":[],\"items\":[]}"
      "{\"name\":\"Two\",\"address\":{\"city\":\"B\",\"zip\":2},"
      "\"active\":false,\"lucky_numbers\":[],\"tags\":[],\"items\":[]}"
      "{\"name\":\"Three\",\"address\":{\"city\":\"C\",\"zip\":3},"
      "\"active\":true,\"lucky_numbers\":[],\"tags\":[],\"items\":[]}",
      0u, 11u};
  test_person first;
  test_person second;
  lonejson_stream *stream;
  lonejson_error error;
  lonejson_stream_result result;

  poison_bytes(&first, sizeof(first), 0x5Au);
  poison_bytes(&second, sizeof(second), 0xA5u);
  stream = lonejson_stream_open_reader(&test_person_map, test_framed_reader,
                                       &state, NULL, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    return;
  }

  result = lonejson_stream_next(stream, &first, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(strcmp(first.name, "One") == 0);

  result = lonejson_stream_next(stream, &first, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(strcmp(first.name, "Two") == 0);

  result = lonejson_stream_next(stream, &second, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(strcmp(second.name, "Three") == 0);

  result = lonejson_stream_next(stream, &second, &error);
  EXPECT(result == LONEJSON_STREAM_EOF);

  lonejson_stream_close(stream);
  lonejson_cleanup(&test_person_map, &first);
  lonejson_cleanup(&test_person_map, &second);
}

static void test_file_and_buffer_helpers(void) {
  const char *json =
      "{\"name\":\"Cara\",\"nickname\":\"cj\",\"age\":1,\"score\":1.5,"
      "\"active\":true,\"address\":{\"city\":\"Lund\",\"zip\":88},\"lucky_"
      "numbers\":[],\"tags\":[],\"items\":[]}";
  static const char expected_pretty[] = "{\n"
                                        "  \"name\": \"Cara\",\n"
                                        "  \"nickname\": \"cj\",\n"
                                        "  \"age\": 1,\n"
                                        "  \"score\": 1.5,\n"
                                        "  \"active\": true,\n"
                                        "  \"address\": {\n"
                                        "    \"city\": \"Lund\",\n"
                                        "    \"zip\": 88\n"
                                        "  },\n"
                                        "  \"lucky_numbers\": [],\n"
                                        "  \"tags\": [],\n"
                                        "  \"items\": []\n"
                                        "}";
  test_person person;
  char path[] = "/tmp/lonejson-test-XXXXXX";
  int fd;
  FILE *fp;
  char out[64];
  char pretty[512];
  size_t needed = 0;
  lonejson_error error;
  lonejson_write_options write_options = lonejson_default_write_options();
  lonejson_status status;

  fd = mkstemp(path);
  EXPECT(fd >= 0);
  if (fd < 0) {
    return;
  }
  EXPECT(write(fd, json, strlen(json)) == (ssize_t)strlen(json));
  close(fd);

  status = lonejson_parse_path(&test_person_map, &person, path, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(person.address.city, "Lund") == 0);

  write_options.overflow_policy = LONEJSON_OVERFLOW_TRUNCATE;
  status =
      lonejson_serialize_buffer(&test_person_map, &person, out, sizeof(out),
                                &needed, &write_options, &error);
  EXPECT(status == LONEJSON_STATUS_TRUNCATED || status == LONEJSON_STATUS_OK);
  EXPECT(needed > strlen(out));

  write_options = lonejson_default_write_options();
  write_options.pretty = 1;
  status = lonejson_serialize_buffer(&test_person_map, &person, pretty,
                                     sizeof(pretty), &needed, &write_options,
                                     &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(pretty, expected_pretty) == 0);

  fp = fopen(path, "wb");
  EXPECT(fp != NULL);
  if (fp != NULL) {
    status =
        lonejson_serialize_filep(&test_person_map, &person, fp, NULL, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    fclose(fp);
  }

  unlink(path);
  lonejson_cleanup(&test_person_map, &person);
}

static void test_jsonl_helpers(void) {
  static const char expected_jsonl[] = "{\"id\":\"evt-1\",\"ok\":true}\n"
                                       "{\"id\":\"evt-2\",\"ok\":false}\n";
  static const char expected_pretty_jsonl[] = "{\n"
                                              "  \"id\": \"evt-1\",\n"
                                              "  \"ok\": true\n"
                                              "}\n"
                                              "{\n"
                                              "  \"id\": \"evt-2\",\n"
                                              "  \"ok\": false\n"
                                              "}\n";
  test_event events[2];
  lonejson_error error;
  size_t needed = 0u;
  char buffer[128];
  char pretty[256];
  char *jsonl;
  lonejson_status status;
  char path[] = "/tmp/lonejson-jsonl-XXXXXX";
  int fd;
  FILE *fp;
  char filebuf[128];
  size_t filelen;
  lonejson_write_options write_options = lonejson_default_write_options();

  memset(events, 0, sizeof(events));
  strcpy(events[0].id, "evt-1");
  events[0].ok = true;
  strcpy(events[1].id, "evt-2");
  events[1].ok = false;

  status =
      lonejson_serialize_jsonl_buffer(&test_event_map, events, 2u, 0u, buffer,
                                      sizeof(buffer), &needed, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, expected_jsonl) == 0);
  EXPECT(needed == strlen(buffer));

  write_options.pretty = 1;
  status = lonejson_serialize_jsonl_buffer(&test_event_map, events, 2u, 0u,
                                           pretty, sizeof(pretty), &needed,
                                           &write_options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(pretty, expected_pretty_jsonl) == 0);
  EXPECT(needed == strlen(pretty));

  jsonl = lonejson_serialize_jsonl_alloc(&test_event_map, events, 2u, 0u, NULL,
                                         NULL, &error);
  EXPECT(jsonl != NULL);
  if (jsonl != NULL) {
    EXPECT(strcmp(jsonl, buffer) == 0);
    LONEJSON_FREE(jsonl);
  }

  status =
      lonejson_serialize_jsonl_buffer(&test_event_map, NULL, 0u, 0u, buffer,
                                      sizeof(buffer), &needed, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, "") == 0);
  EXPECT(needed == 0u);

  fd = mkstemp(path);
  EXPECT(fd >= 0);
  if (fd < 0) {
    return;
  }
  fp = fdopen(fd, "wb");
  EXPECT(fp != NULL);
  if (fp == NULL) {
    close(fd);
    unlink(path);
    return;
  }
  status = lonejson_serialize_jsonl_filep(&test_event_map, events, 2u, 0u, fp,
                                          NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  fclose(fp);

  fp = fopen(path, "rb");
  EXPECT(fp != NULL);
  if (fp != NULL) {
    filelen = fread(filebuf, 1u, sizeof(filebuf) - 1u, fp);
    filebuf[filelen] = '\0';
    fclose(fp);
    EXPECT(strcmp(filebuf, expected_jsonl) == 0);
  }
  unlink(path);
}

static void test_fixed_capacity_object_array(void) {
  const char *json =
      "{\"name\":\"Dee\",\"nickname\":\"dee\",\"age\":1,\"score\":2.0,"
      "\"active\":false,\"address\":{\"city\":\"Malmo\",\"zip\":1},\"lucky_"
      "numbers\":[],\"tags\":[],\"items\":[{\"id\":1,\"label\":\"one\"},{"
      "\"id\":2,\"label\":\"two\"}]}";
  test_person person;
  test_item items_buf[2];
  lonejson_error error;
  lonejson_status status;
  lonejson_parse_options options = lonejson_default_parse_options();

  memset(&person, 0, sizeof(person));
  lonejson_init(&test_person_map, &person);
  memset(items_buf, 0, sizeof(items_buf));
  person.items.items = items_buf;
  person.items.capacity = 2;
  person.items.elem_size = sizeof(items_buf[0]);
  person.items.flags = LONEJSON_ARRAY_FIXED_CAPACITY;
  options.clear_destination = 0;

  status =
      lonejson_parse_cstr(&test_person_map, &person, json, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(person.items.count == 2);
  EXPECT(person.items.items == items_buf);
  EXPECT(items_buf[0].id == 1);
  EXPECT(items_buf[1].id == 2);

  lonejson_cleanup(&test_person_map, &person);
}

static void test_formatting_variants_and_roundtrip(void) {
  const char *json =
      "{\n"
      "\t\"name\" : \"E\\u006dma\",\n"
      "\t\"nickname\" : \"em\",\n"
      "\t\"age\" : -12,\n"
      "\t\"score\" : 1.25e2,\n"
      "\t\"active\" : false,\n"
      "\t\"address\" : {\n"
      "\t  \"city\" : \"G\\u00f6teborg\",\n"
      "\t  \"zip\" : 90210\n"
      "\t},\n"
      "\t\"lucky_numbers\" : [ 0, -1, 200 ],\n"
      "\t\"tags\" : [ \"line\\nfeed\", \"quote\\\"ok\" ],\n"
      "\t\"items\" : [ ],\n"
      "\t\"ignored\" : { \"deep\" : [1, {\"x\": [2,3,{\"y\":4}]}, 5] }\n"
      "}\n";
  test_person person;
  test_person roundtrip;
  lonejson_error error;
  lonejson_status status;
  char *serialized = NULL;

  status = lonejson_parse_cstr(&test_person_map, &person, json, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(person.name, "Emma") == 0);
  EXPECT(strcmp(person.address.city, "Göteborg") == 0);
  EXPECT(person.age == -12);
  EXPECT(person.score > 124.9 && person.score < 125.1);
  EXPECT(person.tags.count == 2);
  EXPECT(strcmp(person.tags.items[0], "line\nfeed") == 0);
  EXPECT(strcmp(person.tags.items[1], "quote\"ok") == 0);

  serialized =
      lonejson_serialize_alloc(&test_person_map, &person, NULL, NULL, &error);
  EXPECT(serialized != NULL);
  if (serialized != NULL) {
    status = lonejson_parse_cstr(&test_person_map, &roundtrip, serialized, NULL,
                                 &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(strcmp(roundtrip.name, "Emma") == 0);
    EXPECT(strcmp(roundtrip.address.city, "Göteborg") == 0);
    LONEJSON_FREE(serialized);
  }

  lonejson_cleanup(&test_person_map, &person);
  lonejson_cleanup(&test_person_map, &roundtrip);
}

static void test_duplicate_key_policy(void) {
  const char *json =
      "{\"name\":\"first\",\"name\":\"second\",\"address\":{\"city\":\"A\","
      "\"zip\":1},\"nickname\":\"n\",\"age\":1,\"score\":1,\"active\":true,"
      "\"lucky_numbers\":[],\"tags\":[],\"items\":[]}";
  test_person person;
  lonejson_parse_options options = lonejson_default_parse_options();
  lonejson_error error;
  lonejson_status status;

  status =
      lonejson_parse_cstr(&test_person_map, &person, json, &options, &error);
  EXPECT(status == LONEJSON_STATUS_DUPLICATE_FIELD);
  lonejson_cleanup(&test_person_map, &person);

  poison_bytes(&person, sizeof(person), 0xC3u);
  options.reject_duplicate_keys = 0;
  status =
      lonejson_parse_cstr(&test_person_map, &person, json, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(person.name, "second") == 0);
  lonejson_cleanup(&test_person_map, &person);
}

static void test_public_api_argument_and_serialization_guards(void) {
  static const char *json = "{\"name\":\"Alice\",\"age\":34}";
  static const char *json_with_nul =
      "{\"name\":\"a\\u0000b\",\"age\":34,\"active\":true,"
      "\"address\":{\"city\":\"X\",\"zip\":1},\"lucky_numbers\":[],"
      "\"tags\":[],\"items\":[]}";
  test_person person;
  lonejson_error error;
  lonejson_status status;
  char *serialized;

  status = lonejson_parse_cstr(&test_person_map, NULL, json, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);

  status = lonejson_parse_cstr(&test_person_map, &person, json_with_nul, NULL,
                               &error);
  EXPECT(status != LONEJSON_STATUS_OK);
  lonejson_cleanup(&test_person_map, &person);

  lonejson_init(&test_person_map, &person);
  person.name = "finite-check";
  person.score = strtod("nan", NULL);
  serialized =
      lonejson_serialize_alloc(&test_person_map, &person, NULL, NULL, &error);
  EXPECT(serialized == NULL);
  person.score = strtod("inf", NULL);
  serialized =
      lonejson_serialize_alloc(&test_person_map, &person, NULL, NULL, &error);
  EXPECT(serialized == NULL);
}

static void test_public_initializers_and_defaults(void) {
  lonejson_error error;
  lonejson_read_result read_result;
  lonejson_value_visitor visitor;
  lonejson_parse_options parse_options;
  lonejson_write_options write_options;
  lonejson_value_limits value_limits;
  test_json_value_doc doc;

  poison_bytes(&error, sizeof(error), 0xA5u);
  lonejson_error_init(&error);
  EXPECT(error.code == LONEJSON_STATUS_OK);
  EXPECT(error.line == 0u);
  EXPECT(error.column == 0u);
  EXPECT(error.offset == 0u);
  EXPECT(error.system_errno == 0);
  EXPECT(error.truncated == 0);
  EXPECT(error.message[0] == '\0');

  read_result = lonejson_default_read_result();
  EXPECT(read_result.bytes_read == 0u);
  EXPECT(read_result.eof == 0);
  EXPECT(read_result.would_block == 0);
  EXPECT(read_result.error_code == 0);

  visitor = lonejson_default_value_visitor();
  EXPECT(visitor.object_begin == NULL);
  EXPECT(visitor.object_end == NULL);
  EXPECT(visitor.object_key_begin == NULL);
  EXPECT(visitor.object_key_chunk == NULL);
  EXPECT(visitor.object_key_end == NULL);
  EXPECT(visitor.array_begin == NULL);
  EXPECT(visitor.array_end == NULL);
  EXPECT(visitor.string_begin == NULL);
  EXPECT(visitor.string_chunk == NULL);
  EXPECT(visitor.string_end == NULL);
  EXPECT(visitor.number_begin == NULL);
  EXPECT(visitor.number_chunk == NULL);
  EXPECT(visitor.number_end == NULL);
  EXPECT(visitor.boolean_value == NULL);
  EXPECT(visitor.null_value == NULL);

  parse_options = lonejson_default_parse_options();
  EXPECT(parse_options.clear_destination == 1);
  EXPECT(parse_options.reject_duplicate_keys == 1);
  EXPECT(parse_options.max_depth == 64u);

  write_options = lonejson_default_write_options();
  EXPECT(write_options.overflow_policy == LONEJSON_OVERFLOW_FAIL);
  EXPECT(write_options.pretty == 0);

  value_limits = lonejson_default_value_limits();

  poison_bytes(&doc, sizeof(doc), 0x5Au);
  lonejson_init(&test_json_value_doc_map, &doc);
  EXPECT(doc.id[0] == '\0');
  EXPECT(doc.selector.kind == LONEJSON_JSON_VALUE_NULL);
  EXPECT(doc.selector.json == NULL);
  EXPECT(doc.selector.len == 0u);
  EXPECT(doc.selector.reader == NULL);
  EXPECT(doc.selector.reader_user == NULL);
  EXPECT(doc.selector.fp == NULL);
  EXPECT(doc.selector.fd == -1);
  EXPECT(doc.selector.path == NULL);
  EXPECT(doc.selector.parse_mode == LONEJSON_JSON_VALUE_PARSE_NONE);
  EXPECT(doc.selector.parse_sink == NULL);
  EXPECT(doc.selector.parse_sink_user == NULL);
  EXPECT(doc.selector.parse_visitor == NULL);
  EXPECT(doc.selector.parse_visitor_user == NULL);
  EXPECT(doc.selector.parse_visitor_limits.max_depth == value_limits.max_depth);
  EXPECT(doc.selector.parse_visitor_limits.max_string_bytes ==
         value_limits.max_string_bytes);
  EXPECT(doc.selector.parse_visitor_limits.max_number_bytes ==
         value_limits.max_number_bytes);
  EXPECT(doc.selector.parse_visitor_limits.max_key_bytes ==
         value_limits.max_key_bytes);
  EXPECT(doc.selector.parse_visitor_limits.max_total_bytes ==
         value_limits.max_total_bytes);
}

static void test_invalid_json_vectors(void) {
  static const char *invalid_cases[] = {
      "spec/invalid_extra_trailing_garbage.json",
      "spec/invalid_leading_zero.json",
      "spec/invalid_fraction_without_digit.json",
      "spec/invalid_exponent_without_digit.json",
      "spec/invalid_plus_sign.json",
      "spec/invalid_double_comma.json",
      "spec/invalid_array_trailing_comma.json",
      "spec/invalid_object_trailing_comma.json",
      "spec/invalid_missing_colon.json",
      "spec/invalid_missing_value.json",
      "spec/invalid_bad_literal.json",
      "spec/invalid_raw_control_char_in_string.json",
      "spec/invalid_bad_escape.json",
      "spec/invalid_bad_unicode.json",
      "spec/invalid_unterminated_array.json",
      "spec/invalid_unterminated_object.json",
      "spec/invalid_unterminated_string.json"};
  size_t i;

  for (i = 0; i < sizeof(invalid_cases) / sizeof(invalid_cases[0]); ++i) {
    char *path = fixture_path(invalid_cases[i]);
    test_person person;
    lonejson_error error;
    lonejson_status status;

    EXPECT(path != NULL);
    if (path == NULL) {
      return;
    }
    lonejson_init(&test_person_map, &person);
    status = lonejson_parse_path(&test_person_map, &person, path, NULL, &error);
    EXPECT(status != LONEJSON_STATUS_OK);
    free(path);
    lonejson_cleanup(&test_person_map, &person);
  }
}

static void assert_fixture_person(const test_person *person) {
  EXPECT(person->name != NULL);
  if (person->name == NULL) {
    return;
  }
  EXPECT(strncmp(person->name, "Fixture Person Example", 22) == 0);
  EXPECT(strcmp(person->nickname, "fixture") == 0);
  EXPECT(person->age == 57);
  EXPECT(person->score > 4321.0 && person->score < 4321.2);
  EXPECT(person->active == true);
  EXPECT(strcmp(person->address.city, "Stockholm") == 0);
  EXPECT(person->address.zip == 11223344);
  EXPECT(person->lucky_numbers.count == 96);
  if (person->lucky_numbers.count >= 96 &&
      person->lucky_numbers.items != NULL) {
    EXPECT(person->lucky_numbers.items[0] == 1);
    EXPECT(person->lucky_numbers.items[11] == 233);
    EXPECT(person->lucky_numbers.items[95] == 233);
  }
  EXPECT(person->tags.count == 10);
  if (person->tags.count >= 10 && person->tags.items != NULL) {
    EXPECT(strcmp(person->tags.items[0], "alpha") == 0);
    EXPECT(strcmp(person->tags.items[9], "kappa") == 0);
  }
  EXPECT(person->items.count == 5);
  if (person->items.count >= 5 && person->items.items != NULL) {
    EXPECT(((test_item *)person->items.items)[0].id == 101);
    EXPECT(((test_item *)person->items.items)[4].id == 505);
    EXPECT(strcmp(((test_item *)person->items.items)[4].label, "fifth-entry") ==
           0);
  }
}

static void test_fixture_documents(void) {
  static const char *fixture_names[] = {
      "profile_compact.json", "profile_pretty.json", "profile_custom.json"};
  size_t i;

  for (i = 0; i < sizeof(fixture_names) / sizeof(fixture_names[0]); ++i) {
    char *path = generated_fixture_path(fixture_names[i]);
    test_person person;
    lonejson_error error;
    lonejson_status status;

    EXPECT(path != NULL);
    if (path == NULL) {
      return;
    }
    status = lonejson_parse_path(&test_person_map, &person, path, NULL, &error);
    EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
    assert_fixture_person(&person);
    free(path);
    lonejson_cleanup(&test_person_map, &person);
  }
}

static void test_large_generated_fixture_sizes(void) {
  static const struct {
    const char *name;
    long min_bytes;
  } cases[] = {
      {"large_object_compact.json", 22L * 1024L * 1024L},
      {"large_object_pretty.json", 16L * 1024L * 1024L},
      {"large_array_objects.json", 18L * 1024L * 1024L},
      {"large_base64_payloads.json", 16L * 1024L * 1024L},
      {"large_stream.jsonl", 18L * 1024L * 1024L},
  };
  size_t i;
  long total_bytes;

  total_bytes = 0L;
  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    char *path = generated_fixture_path(cases[i].name);
    struct stat st;

    EXPECT(path != NULL);
    if (path == NULL) {
      return;
    }
    EXPECT(stat(path, &st) == 0);
    if (stat(path, &st) == 0) {
      EXPECT((long)st.st_size >= cases[i].min_bytes);
      total_bytes += (long)st.st_size;
    }
    free(path);
  }
  EXPECT(total_bytes >= 95L * 1024L * 1024L);
  EXPECT(total_bytes <= 110L * 1024L * 1024L);
}

static void test_large_generated_fixture_parsing(void) {
  static const char *object_fixture_names[] = {
      "large_object_compact.json", "large_object_pretty.json",
      "large_array_objects.json", "large_base64_payloads.json"};
  size_t i;

  for (i = 0;
       i < sizeof(object_fixture_names) / sizeof(object_fixture_names[0]);
       ++i) {
    char *path = generated_fixture_path(object_fixture_names[i]);
    lonejson_error error;
    lonejson_status status;

    EXPECT(path != NULL);
    if (path == NULL) {
      return;
    }
    status = lonejson_validate_path(path, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    free(path);
  }

  {
    char *path = generated_fixture_path("large_stream.jsonl");
    lonejson_stream *stream;
    lonejson_stream_result result;
    lonejson_error error;
    size_t seen;
    test_large_stream_record record;

    EXPECT(path != NULL);
    if (path == NULL) {
      return;
    }
    stream = lonejson_stream_open_path(&test_large_stream_record_map, path,
                                       NULL, &error);
    EXPECT(stream != NULL);
    if (stream == NULL) {
      free(path);
      return;
    }
    seen = 0u;
    for (;;) {
      result = lonejson_stream_next(stream, &record, &error);
      if (result == LONEJSON_STREAM_EOF) {
        break;
      }
      EXPECT(result == LONEJSON_STREAM_OBJECT);
      if (result != LONEJSON_STREAM_OBJECT) {
        break;
      }
      EXPECT(record.id > 0);
      EXPECT(strncmp(record.name, "stream-", 7) == 0 ||
             strcmp(record.name, "tail") == 0);
      EXPECT(strncmp(record.kind, "jsonl", 5) == 0 || record.id == 999999999);
      EXPECT(record.meta.shard >= 0 || record.id == 999999999);
      ++seen;
    }
    lonejson_stream_close(stream);
    free(path);
    EXPECT(seen > 1000u);
  }
}

static void test_small_valid_spec_fixtures(void) {
  static const char *fixture_names[] = {
      "spec/valid_empty_arrays.json",   "spec/valid_escaped_strings.json",
      "spec/valid_nested_ignored.json", "spec/valid_object_basic.json",
      "spec/valid_object_pretty.json",  "spec/valid_unicode_escapes.json"};
  size_t i;

  for (i = 0; i < sizeof(fixture_names) / sizeof(fixture_names[0]); ++i) {
    char *path = fixture_path(fixture_names[i]);
    test_person person;
    lonejson_error error;
    lonejson_status status;

    EXPECT(path != NULL);
    if (path == NULL) {
      return;
    }
    status = lonejson_parse_path(&test_person_map, &person, path, NULL, &error);
    EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
    free(path);
    lonejson_cleanup(&test_person_map, &person);
  }
}

static void test_json_test_suite_parsing(void) {
  const char *dir_path =
      LONEJSON_SOURCE_DIR "/tests/fixtures/vendor/json_test_suite/test_parsing";
  DIR *dir;
  struct dirent *entry;
  size_t y_count = 0u;
  size_t n_count = 0u;
  size_t i_count = 0u;
  size_t i_accept = 0u;
  size_t i_reject = 0u;

  dir = opendir(dir_path);
  EXPECT(dir != NULL);
  if (dir == NULL) {
    return;
  }

  while ((entry = readdir(dir)) != NULL) {
    char *path;
    lonejson_error error;
    lonejson_status status;

    if (entry->d_name[0] == '.') {
      continue;
    }
    path = vendor_fixture_path(entry->d_name);
    EXPECT(path != NULL);
    if (path == NULL) {
      closedir(dir);
      return;
    }

    status = lonejson_validate_path(path, &error);
    if (strncmp(entry->d_name, "y_", 2u) == 0) {
      ++y_count;
      EXPECT(status == LONEJSON_STATUS_OK);
    } else if (strncmp(entry->d_name, "n_", 2u) == 0) {
      ++n_count;
      EXPECT(status != LONEJSON_STATUS_OK);
    } else if (strncmp(entry->d_name, "i_", 2u) == 0) {
      ++i_count;
      if (status == LONEJSON_STATUS_OK) {
        ++i_accept;
      } else {
        ++i_reject;
      }
    } else {
      EXPECT(0);
    }
    free(path);
  }

  closedir(dir);
  EXPECT(y_count == 95u);
  EXPECT(n_count == 188u);
  EXPECT(i_count == 35u);
  EXPECT(i_accept + i_reject == i_count);
}

static void test_spooled_fields_roundtrip(void) {
  unsigned char raw_bytes[180];
  unsigned char read_back[256];
  test_spool_doc doc;
  test_spool_doc roundtrip;
  lonejson_error error;
  lonejson_status status;
  char *text;
  char *base64_text;
  char *json;
  char *serialized;
  size_t serialized_len;
  size_t read_len;
  size_t i;

  for (i = 0u; i < sizeof(raw_bytes); ++i) {
    raw_bytes[i] = (unsigned char)((i * 13u) & 0xFFu);
  }
  text = make_repeating_text(220u);
  base64_text = base64_encode_bytes(raw_bytes, sizeof(raw_bytes));
  json = make_spool_json("spool-1", text, base64_text);
  EXPECT(text != NULL);
  EXPECT(base64_text != NULL);
  EXPECT(json != NULL);
  if (text == NULL || base64_text == NULL || json == NULL) {
    free(text);
    free(base64_text);
    free(json);
    return;
  }

  status = lonejson_parse_cstr(&test_spool_doc_map, &doc, json, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(doc.id, "spool-1") == 0);
  EXPECT(lonejson_spooled_size(&doc.text) == strlen(text));
  EXPECT(lonejson_spooled_size(&doc.bytes) == sizeof(raw_bytes));
  EXPECT(lonejson_spooled_spilled(&doc.text) != 0);
  EXPECT(lonejson_spooled_spilled(&doc.bytes) != 0);

  read_len = read_spooled_all(&doc.text, read_back, sizeof(read_back));
  EXPECT(read_len == strlen(text));
  EXPECT(memcmp(read_back, text, read_len) == 0);

  memset(read_back, 0, sizeof(read_back));
  read_len = read_spooled_all(&doc.bytes, read_back, sizeof(read_back));
  EXPECT(read_len == sizeof(raw_bytes));
  EXPECT(memcmp(read_back, raw_bytes, sizeof(raw_bytes)) == 0);

  serialized = lonejson_serialize_alloc(&test_spool_doc_map, &doc,
                                        &serialized_len, NULL, &error);
  EXPECT(serialized != NULL);
  if (serialized != NULL) {
    EXPECT(serialized_len > strlen(text));
    EXPECT(lonejson_validate_cstr(serialized, &error) == LONEJSON_STATUS_OK);
    status = lonejson_parse_cstr(&test_spool_doc_map, &roundtrip, serialized,
                                 NULL, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(lonejson_spooled_size(&roundtrip.text) == strlen(text));
    EXPECT(lonejson_spooled_size(&roundtrip.bytes) == sizeof(raw_bytes));
    lonejson_cleanup(&test_spool_doc_map, &roundtrip);
    LONEJSON_FREE(serialized);
  }

  lonejson_cleanup(&test_spool_doc_map, &doc);
  free(text);
  free(base64_text);
  free(json);
}

static void test_spooled_fields_small_and_null(void) {
  static const char *json =
      "{\"id\":\"tiny\",\"text\":\"hello\",\"bytes\":\"AQID\"}";
  static const char *null_json =
      "{\"id\":\"tiny\",\"text\":null,\"bytes\":null}";
  test_spool_doc doc;
  lonejson_error error;
  lonejson_status status;
  unsigned char read_back[16];
  size_t read_len;

  reset_lonejson_alloc_stats();
  status = lonejson_parse_cstr(&test_spool_doc_map, &doc, json, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(lonejson_spooled_spilled(&doc.text) == 0);
  EXPECT(lonejson_spooled_spilled(&doc.bytes) == 0);
  EXPECT(lonejson_spooled_size(&doc.text) == 5u);
  EXPECT(lonejson_spooled_size(&doc.bytes) == 3u);
  read_len = read_spooled_all(&doc.bytes, read_back, sizeof(read_back));
  EXPECT(read_len == 3u);
  EXPECT(read_back[0] == 1u && read_back[1] == 2u && read_back[2] == 3u);

  status =
      lonejson_parse_cstr(&test_spool_doc_map, &doc, null_json, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(lonejson_spooled_size(&doc.text) == 0u);
  EXPECT(lonejson_spooled_size(&doc.bytes) == 0u);
  lonejson_cleanup(&test_spool_doc_map, &doc);
  EXPECT(g_alloc_record_count == 0u);
}

static void test_spooled_append_api(void) {
  lonejson_spool_options options;
  lonejson_spooled value;
  lonejson_error error;
  unsigned char read_back[32];
  lonejson_read_result chunk;
  size_t read_len;
  size_t total;

  options = lonejson_default_spool_options();
  options.memory_limit = 4u;
  options.max_bytes = 0u;
  options.temp_dir = NULL;
  lonejson_error_init(&error);
  lonejson_spooled_init(&value, &options);

  EXPECT(lonejson_spooled_append(&value, "ab", 2u, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_spooled_append(&value, "cdef", 4u, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_spooled_size(&value) == 6u);
  EXPECT(lonejson_spooled_spilled(&value) != 0);

  read_len = read_spooled_all(&value, read_back, sizeof(read_back));
  EXPECT(read_len == 6u);
  EXPECT(memcmp(read_back, "abcdef", 6u) == 0);

  EXPECT(lonejson_spooled_rewind(&value, &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_spooled_append(&value, "gh", 2u, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_spooled_size(&value) == 8u);

  read_len = read_spooled_all(&value, read_back, sizeof(read_back));
  EXPECT(read_len == 8u);
  EXPECT(memcmp(read_back, "abcdefgh", 8u) == 0);

  EXPECT(lonejson_spooled_rewind(&value, &error) == LONEJSON_STATUS_OK);
  chunk = lonejson_spooled_read(&value, read_back, 4u);
  EXPECT(chunk.error_code == 0);
  EXPECT(chunk.bytes_read == 4u);
  EXPECT(chunk.eof == 0);
  chunk = lonejson_spooled_read(&value, read_back + 4u, 1u);
  EXPECT(chunk.error_code == 0);
  EXPECT(chunk.bytes_read == 1u);
  EXPECT(chunk.eof == 0);
  EXPECT(memcmp(read_back, "abcde", 5u) == 0);
  EXPECT(lonejson_spooled_append(&value, "ij", 2u, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_spooled_size(&value) == 10u);

  total = 5u;
  while (total < sizeof(read_back)) {
    chunk = lonejson_spooled_read(&value, read_back + total,
                                  sizeof(read_back) - total);
    EXPECT(chunk.error_code == 0);
    total += chunk.bytes_read;
    if (chunk.eof) {
      break;
    }
  }
  EXPECT(total == 10u);
  EXPECT(memcmp(read_back, "abcdefghij", 10u) == 0);

  lonejson_spooled_cleanup(&value);

  options.memory_limit = 2u;
  options.max_bytes = 4u;
  lonejson_spooled_init(&value, &options);
  EXPECT(lonejson_spooled_append(NULL, "x", 1u, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_spooled_append(&value, NULL, 1u, &error) ==
         LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(lonejson_spooled_append(&value, NULL, 0u, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_spooled_append(&value, "abcd", 4u, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_spooled_size(&value) == 4u);
  EXPECT(lonejson_spooled_append(&value, "e", 1u, &error) ==
         LONEJSON_STATUS_OVERFLOW);
  EXPECT(lonejson_spooled_size(&value) == 4u);
  lonejson_spooled_cleanup(&value);
}

static void test_spooled_field_failures(void) {
  static const char *bad_base64 =
      "{\"id\":\"bad\",\"text\":\"ok\",\"bytes\":\"###\"}";
  char *too_large_text;
  char *too_large_json;
  test_spool_doc doc;
  test_spool_limits_doc limited;
  lonejson_error error;
  lonejson_status status;

  status =
      lonejson_parse_cstr(&test_spool_doc_map, &doc, bad_base64, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
  lonejson_cleanup(&test_spool_doc_map, &doc);

  too_large_text = make_repeating_text(140u);
  EXPECT(too_large_text != NULL);
  if (too_large_text == NULL) {
    return;
  }
  {
    const char *fmt = "{\"text\":\"%s\"}";
    size_t need = snprintf(NULL, 0, fmt, too_large_text);
    too_large_json = (char *)malloc(need + 1u);
    EXPECT(too_large_json != NULL);
    if (too_large_json == NULL) {
      free(too_large_text);
      return;
    }
    snprintf(too_large_json, need + 1u, fmt, too_large_text);
  }
  status = lonejson_parse_cstr(&test_spool_limits_doc_map, &limited,
                               too_large_json, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  lonejson_cleanup(&test_spool_limits_doc_map, &limited);
  free(too_large_text);
  free(too_large_json);
}

static void test_source_fields_path_and_raw_sink(void) {
  static const char text_payload[] = "alpha \"quoted\"\nline\\tab\tend";
  static const unsigned char bytes_payload[] = {0x00u, 0x01u, 0x7fu,
                                                0x80u, 0xffu, 0x42u};
  test_source_doc doc;
  lonejson_error error;
  lonejson_status status;
  char text_path[] = "/tmp/lonejson-source-text-XXXXXX";
  char bytes_path[] = "/tmp/lonejson-source-bytes-XXXXXX";
  int text_fd;
  int bytes_fd;
  char *json;
  char *encoded;
  size_t expected_len;
  char serialized[256];
  unsigned char raw_buffer[128];
  test_buffer_sink sink;

  lonejson_init(&test_source_doc_map, &doc);
  strcpy(doc.id, "src-1");
  text_fd = mkstemp(text_path);
  EXPECT(text_fd >= 0);
  if (text_fd < 0) {
    return;
  }
  bytes_fd = mkstemp(bytes_path);
  EXPECT(bytes_fd >= 0);
  if (bytes_fd < 0) {
    close(text_fd);
    unlink(text_path);
    return;
  }
  EXPECT(write(text_fd, text_payload, sizeof(text_payload) - 1u) ==
         (ssize_t)(sizeof(text_payload) - 1u));
  EXPECT(write(bytes_fd, bytes_payload, sizeof(bytes_payload)) ==
         (ssize_t)sizeof(bytes_payload));
  close(text_fd);
  close(bytes_fd);

  status = lonejson_source_set_path(&doc.text, text_path, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_source_set_path(&doc.bytes, bytes_path, &error);
  EXPECT(status == LONEJSON_STATUS_OK);

  encoded = base64_encode_bytes(bytes_payload, sizeof(bytes_payload));
  EXPECT(encoded != NULL);
  if (encoded == NULL) {
    lonejson_cleanup(&test_source_doc_map, &doc);
    unlink(text_path);
    unlink(bytes_path);
    return;
  }
  expected_len = snprintf(NULL, 0,
                          "{\"id\":\"src-1\",\"text\":\"alpha "
                          "\\\"quoted\\\"\\nline\\\\tab\\tend\","
                          "\"bytes\":\"%s\"}",
                          encoded);
  json = (char *)malloc(expected_len + 1u);
  EXPECT(json != NULL);
  if (json != NULL) {
    snprintf(json, expected_len + 1u,
             "{\"id\":\"src-1\",\"text\":\"alpha "
             "\\\"quoted\\\"\\nline\\\\tab\\tend\","
             "\"bytes\":\"%s\"}",
             encoded);
    EXPECT(lonejson_serialize_buffer(&test_source_doc_map, &doc, serialized,
                                     sizeof(raw_buffer), NULL, NULL,
                                     &error) == LONEJSON_STATUS_OK);
    EXPECT(strcmp(serialized, json) == 0);
    EXPECT(lonejson_serialize_buffer(&test_source_doc_map, &doc, serialized,
                                     sizeof(raw_buffer), NULL, NULL,
                                     &error) == LONEJSON_STATUS_OK);
    EXPECT(strcmp(serialized, json) == 0);
  }

  memset(&sink, 0, sizeof(sink));
  sink.buffer = raw_buffer;
  sink.capacity = sizeof(raw_buffer);
  status = lonejson_source_write_to_sink(&doc.text, test_buffer_sink_write,
                                         &sink, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)raw_buffer, text_payload) == 0);

  lonejson_cleanup(&test_source_doc_map, &doc);
  unlink(text_path);
  unlink(bytes_path);
  free(encoded);
  free(json);
}

static void test_source_fields_file_and_fd(void) {
  static const char text_payload[] = "streamed from FILE*";
  static const unsigned char bytes_payload[] = {0xdeu, 0xadu, 0xbeu, 0xefu};
  test_source_doc doc;
  lonejson_error error;
  lonejson_status status;
  char text_path[] = "/tmp/lonejson-source-file-XXXXXX";
  char bytes_path[] = "/tmp/lonejson-source-fd-XXXXXX";
  int text_fd;
  int bytes_fd;
  FILE *fp;
  unsigned char check[32];
  char *json;
  char *encoded;
  size_t expected_len;
  char serialized[256];

  lonejson_init(&test_source_doc_map, &doc);
  strcpy(doc.id, "src-2");
  text_fd = mkstemp(text_path);
  EXPECT(text_fd >= 0);
  if (text_fd < 0) {
    return;
  }
  bytes_fd = mkstemp(bytes_path);
  EXPECT(bytes_fd >= 0);
  if (bytes_fd < 0) {
    close(text_fd);
    unlink(text_path);
    return;
  }
  EXPECT(write(text_fd, text_payload, sizeof(text_payload) - 1u) ==
         (ssize_t)(sizeof(text_payload) - 1u));
  EXPECT(write(bytes_fd, bytes_payload, sizeof(bytes_payload)) ==
         (ssize_t)sizeof(bytes_payload));
  close(text_fd);
  close(bytes_fd);

  fp = fopen(text_path, "rb");
  EXPECT(fp != NULL);
  if (fp == NULL) {
    unlink(text_path);
    unlink(bytes_path);
    return;
  }
  bytes_fd = open(bytes_path, O_RDONLY);
  EXPECT(bytes_fd >= 0);
  if (bytes_fd < 0) {
    fclose(fp);
    unlink(text_path);
    unlink(bytes_path);
    return;
  }

  status = lonejson_source_set_file(&doc.text, fp, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_source_set_fd(&doc.bytes, bytes_fd, &error);
  EXPECT(status == LONEJSON_STATUS_OK);

  encoded = base64_encode_bytes(bytes_payload, sizeof(bytes_payload));
  EXPECT(encoded != NULL);
  if (encoded != NULL) {
    expected_len =
        snprintf(NULL, 0, "{\"id\":\"src-2\",\"text\":\"%s\",\"bytes\":\"%s\"}",
                 text_payload, encoded);
    json = (char *)malloc(expected_len + 1u);
    EXPECT(json != NULL);
    if (json != NULL) {
      snprintf(json, expected_len + 1u,
               "{\"id\":\"src-2\",\"text\":\"%s\",\"bytes\":\"%s\"}",
               text_payload, encoded);
      status =
          lonejson_serialize_buffer(&test_source_doc_map, &doc, serialized,
                                    sizeof(serialized), NULL, NULL, &error);
      EXPECT(status == LONEJSON_STATUS_OK);
      EXPECT(strcmp(serialized, json) == 0);
      free(json);
    }
    free(encoded);
  }

  EXPECT(fseek(fp, 0L, SEEK_SET) == 0);
  EXPECT(fread(check, 1u, sizeof(text_payload) - 1u, fp) ==
         sizeof(text_payload) - 1u);
  EXPECT(memcmp(check, text_payload, sizeof(text_payload) - 1u) == 0);

  EXPECT(lseek(bytes_fd, 0, SEEK_SET) == 0);
  EXPECT(read(bytes_fd, check, sizeof(bytes_payload)) ==
         (ssize_t)sizeof(bytes_payload));
  EXPECT(memcmp(check, bytes_payload, sizeof(bytes_payload)) == 0);

  lonejson_cleanup(&test_source_doc_map, &doc);
  fclose(fp);
  close(bytes_fd);
  unlink(text_path);
  unlink(bytes_path);
}

static void test_source_field_parse_and_seek_failures(void) {
  static const char *null_json =
      "{\"id\":\"src-3\",\"text\":null,\"bytes\":null}";
  static const char *bad_json = "{\"id\":\"src-3\",\"text\":\"inline\"}";
  test_source_doc doc;
  test_counting_sink sink;
  lonejson_error error;
  lonejson_status status;
  int pipe_fds[2];

  status =
      lonejson_parse_cstr(&test_source_doc_map, &doc, null_json, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(doc.text.kind == LONEJSON_SOURCE_NONE);
  EXPECT(doc.bytes.kind == LONEJSON_SOURCE_NONE);
  lonejson_cleanup(&test_source_doc_map, &doc);

  status =
      lonejson_parse_cstr(&test_source_doc_map, &doc, bad_json, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_TYPE_MISMATCH);
  lonejson_cleanup(&test_source_doc_map, &doc);

  if (pipe(pipe_fds) != 0) {
    EXPECT(0);
    return;
  }
  EXPECT(write(pipe_fds[1], "pipe-bytes", 10u) == 10);
  close(pipe_fds[1]);
  memset(&sink, 0, sizeof(sink));
  lonejson_source_init(&doc.text);
  status = lonejson_source_set_fd(&doc.text, pipe_fds[0], &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_source_write_to_sink(&doc.text, test_counting_sink_write,
                                         &sink, &error);
  EXPECT(status == LONEJSON_STATUS_IO_ERROR);
  lonejson_source_cleanup(&doc.text);
  close(pipe_fds[0]);
}

static void test_source_fields_do_not_mutate_sink_on_open_failure(void) {
  lonejson_source source;
  lonejson_error error;
  lonejson_status status;
  test_buffer_sink sink;
  char out[64];

  lonejson_source_init(&source);
  status =
      lonejson_source_set_path(&source, "/tmp/does-not-exist-lonejson", &error);
  EXPECT(status == LONEJSON_STATUS_OK);

  memset(&sink, 0, sizeof(sink));
  memset(out, 0, sizeof(out));
  sink.buffer = (unsigned char *)out;
  sink.capacity = sizeof(out);
  status = lonejson__serialize_source_text(&source, test_buffer_sink_write,
                                           &sink, &error);
  EXPECT(status == LONEJSON_STATUS_IO_ERROR);
  EXPECT(sink.length == 0u);
  EXPECT(out[0] == '\0');

  status = lonejson__serialize_source_base64(&source, test_buffer_sink_write,
                                             &sink, &error);
  EXPECT(status == LONEJSON_STATUS_IO_ERROR);
  EXPECT(sink.length == 0u);
  EXPECT(out[0] == '\0');

  lonejson_source_cleanup(&source);
}

static void test_json_value_parse_and_roundtrip(void) {
  const char *json =
      "{"
      "\"id\":\"lql-1\","
      "\"selector\":{\"kind\":\"term\",\"field\":\"name\",\"value\":\"alice\"},"
      "\"fields\":[\"id\",\"name\",\"created_at\"],"
      "\"last_error\":{\"code\":\"bad_selector\",\"detail\":{\"path\":\"/"
      "name\"}}"
      "}";
  const char *expected_compact =
      "{\"id\":\"lql-1\",\"selector\":{\"kind\":\"term\",\"field\":\"name\","
      "\"value\":\"alice\"},\"fields\":[\"id\",\"name\",\"created_at\"],"
      "\"last_error\":{\"code\":\"bad_selector\",\"detail\":{\"path\":\"/"
      "name\"}}}";
  test_json_value_doc doc;
  test_json_value_doc streamed;
  test_reader_state stream_state;
  lonejson_parse_options parse_options = lonejson_default_parse_options();
  lonejson_write_options options = lonejson_default_write_options();
  lonejson_error error;
  lonejson_status status;
  lonejson_stream *stream;
  lonejson_stream_result result;
  char *serialized;

  poison_bytes(&doc, sizeof(doc), 0xA5u);
  lonejson__init_map(&test_json_value_doc_map, &doc);
  status = lonejson_json_value_enable_parse_capture(&doc.selector, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_json_value_enable_parse_capture(&doc.fields, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_json_value_enable_parse_capture(&doc.last_error, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  parse_options.clear_destination = 0;
  status = lonejson_parse_cstr(&test_json_value_doc_map, &doc, json,
                               &parse_options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(doc.selector.kind == LONEJSON_JSON_VALUE_BUFFER);
  EXPECT(strcmp(doc.selector.json,
                "{\"kind\":\"term\",\"field\":\"name\",\"value\":\"alice\"}") ==
         0);
  EXPECT(strcmp(doc.fields.json, "[\"id\",\"name\",\"created_at\"]") == 0);
  EXPECT(
      strcmp(doc.last_error.json,
             "{\"code\":\"bad_selector\",\"detail\":{\"path\":\"/name\"}}") ==
      0);

  serialized = lonejson_serialize_alloc(&test_json_value_doc_map, &doc, NULL,
                                        NULL, &error);
  EXPECT(serialized != NULL);
  if (serialized != NULL) {
    EXPECT(strcmp(serialized, expected_compact) == 0);
    LONEJSON_FREE(serialized);
  }

  options.pretty = 1;
  serialized = lonejson_serialize_alloc(&test_json_value_doc_map, &doc, NULL,
                                        &options, &error);
  EXPECT(serialized != NULL);
  if (serialized != NULL) {
    EXPECT(lonejson_validate_cstr(serialized, &error) == LONEJSON_STATUS_OK);
    EXPECT(strstr(serialized, "\n  \"selector\": {\n") != NULL);
    EXPECT(strstr(serialized, "\"kind\": \"term\"") != NULL);
    EXPECT(strstr(serialized, "\n  \"fields\": [\n") != NULL);
    EXPECT(strstr(serialized, "\"created_at\"") != NULL);
    EXPECT(strstr(serialized, "\n  \"last_error\": {\n") != NULL);
    LONEJSON_FREE(serialized);
  }

  stream_state.json = json;
  stream_state.offset = 0u;
  stream_state.chunk_size = 5u;
  stream =
      lonejson_stream_open_reader(&test_json_value_doc_map, test_state_reader,
                                  &stream_state, &parse_options, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    poison_bytes(&streamed, sizeof(streamed), 0x5Au);
    lonejson__init_map(&test_json_value_doc_map, &streamed);
    status =
        lonejson_json_value_enable_parse_capture(&streamed.selector, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    status = lonejson_json_value_enable_parse_capture(&streamed.fields, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    status =
        lonejson_json_value_enable_parse_capture(&streamed.last_error, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    result = lonejson_stream_next(stream, &streamed, &error);
    EXPECT(result == LONEJSON_STREAM_OBJECT);
    if (result == LONEJSON_STREAM_OBJECT) {
      EXPECT(strcmp(streamed.selector.json, doc.selector.json) == 0);
      lonejson_cleanup(&test_json_value_doc_map, &streamed);
    }
    lonejson_stream_close(stream);
  }

  lonejson_cleanup(&test_json_value_doc_map, &doc);
}

static void test_json_value_parse_stream_sink(void) {
  const char *json =
      "{"
      "\"id\":\"sink-1\","
      "\"selector\":{\"kind\":\"term\",\"field\":\"name\",\"value\":\"alice\"},"
      "\"fields\":[\"id\",\"name\",\"created_at\"],"
      "\"last_error\":{\"code\":\"bad_selector\",\"detail\":{\"path\":\"/"
      "name\"}}"
      "}";
  test_json_value_doc doc;
  lonejson_parse_options options = lonejson_default_parse_options();
  lonejson_error error;
  lonejson_status status;
  unsigned char selector_buf[128];
  unsigned char fields_buf[128];
  unsigned char error_buf[128];
  test_buffer_sink selector_sink;
  test_buffer_sink fields_sink;
  test_buffer_sink error_sink;

  memset(&selector_sink, 0, sizeof(selector_sink));
  memset(&fields_sink, 0, sizeof(fields_sink));
  memset(&error_sink, 0, sizeof(error_sink));
  selector_sink.buffer = selector_buf;
  selector_sink.capacity = sizeof(selector_buf);
  fields_sink.buffer = fields_buf;
  fields_sink.capacity = sizeof(fields_buf);
  error_sink.buffer = error_buf;
  error_sink.capacity = sizeof(error_buf);

  poison_bytes(&doc, sizeof(doc), 0xCCu);
  lonejson__init_map(&test_json_value_doc_map, &doc);
  status = lonejson_json_value_set_parse_sink(
      &doc.selector, test_buffer_sink_write, &selector_sink, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_json_value_set_parse_sink(
      &doc.fields, test_buffer_sink_write, &fields_sink, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_json_value_set_parse_sink(
      &doc.last_error, test_buffer_sink_write, &error_sink, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  options.clear_destination = 0;
  status = lonejson_parse_cstr(&test_json_value_doc_map, &doc, json, &options,
                               &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp((const char *)selector_buf,
                "{\"kind\":\"term\",\"field\":\"name\",\"value\":\"alice\"}") ==
         0);
  EXPECT(strcmp((const char *)fields_buf, "[\"id\",\"name\",\"created_at\"]") ==
         0);
  EXPECT(
      strcmp((const char *)error_buf,
             "{\"code\":\"bad_selector\",\"detail\":{\"path\":\"/name\"}}") ==
      0);
  EXPECT(doc.selector.kind == LONEJSON_JSON_VALUE_NULL);
  EXPECT(doc.fields.kind == LONEJSON_JSON_VALUE_NULL);
  EXPECT(doc.last_error.kind == LONEJSON_JSON_VALUE_NULL);

  lonejson_cleanup(&test_json_value_doc_map, &doc);
}

static void test_json_value_parse_visitor(void) {
  const char *json =
      "{"
      "\"id\":\"visit-1\","
      "\"selector\":{\"kind\":\"term\",\"field\":\"name\",\"value\":\"alice\"},"
      "\"fields\":[\"id\",\"name\",{\"nested\":[true,null,3.5]}],"
      "\"last_error\":null"
      "}";
  test_json_value_doc doc;
  lonejson_parse_options options = lonejson_default_parse_options();
  lonejson_error error;
  lonejson_status status;
  test_visit_state selector_state;
  test_visit_state fields_state;
  test_visit_state error_state;
  lonejson_value_visitor visitor = test_value_visitor();
  lonejson_value_limits limits = lonejson_default_value_limits();

  memset(&selector_state, 0, sizeof(selector_state));
  memset(&fields_state, 0, sizeof(fields_state));
  memset(&error_state, 0, sizeof(error_state));
  poison_bytes(&doc, sizeof(doc), 0xCCu);
  lonejson__init_map(&test_json_value_doc_map, &doc);
  status = lonejson_json_value_set_parse_visitor(
      &doc.selector, &visitor, &selector_state, &limits, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_json_value_set_parse_visitor(
      &doc.fields, &visitor, &fields_state, &limits, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_json_value_set_parse_visitor(&doc.last_error, &visitor,
                                                 &error_state, &limits, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  options.clear_destination = 0;
  status = lonejson_parse_cstr(&test_json_value_doc_map, &doc, json, &options,
                               &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(selector_state.log,
                "{K(kind)S(term)K(field)S(name)K(value)S(alice)}") == 0);
  EXPECT(strcmp(fields_state.log, "[S(id)S(name){K(nested)[TZN(3.5)]}]") == 0);
  EXPECT(strcmp(error_state.log, "Z") == 0);
  EXPECT(doc.selector.kind == LONEJSON_JSON_VALUE_NULL);
  EXPECT(doc.fields.kind == LONEJSON_JSON_VALUE_NULL);
  EXPECT(doc.last_error.kind == LONEJSON_JSON_VALUE_NULL);

  limits.max_string_bytes = 3u;
  lonejson_cleanup(&test_json_value_doc_map, &doc);
  poison_bytes(&doc, sizeof(doc), 0xCCu);
  lonejson__init_map(&test_json_value_doc_map, &doc);
  memset(&selector_state, 0, sizeof(selector_state));
  status = lonejson_json_value_set_parse_visitor(
      &doc.selector, &visitor, &selector_state, &limits, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  options.clear_destination = 0;
  status = lonejson_parse_cstr(
      &test_json_value_doc_map, &doc,
      "{\"id\":\"visit-2\",\"selector\":{\"value\":\"alice\"}}", &options,
      &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(strstr(error.message,
                "JSON string exceeds maximum decoded byte limit") != NULL);

  lonejson_cleanup(&test_json_value_doc_map, &doc);
}

static void test_json_value_parse_visitor_fast_path_regressions(void) {
  const char *json =
      "{"
      "\"id\":\"visit-fast\","
      "\"selector\":{\"outer\":\"abcdefghijklmnopqrstuvwxyz0123456789\","
      "\"nested\":{\"inner\":\"plain-fast-path-string\",\"arr\":[\"alpha\","
      "{\"deep\":\"omega\"}]},\"tail\":\"done\"}"
      "}";
  test_json_value_doc doc;
  lonejson_parse_options options = lonejson_default_parse_options();
  lonejson_error error;
  lonejson_status status;
  test_visit_chunk_state state;
  lonejson_value_visitor visitor = test_counting_value_visitor();

  memset(&state, 0, sizeof(state));
  poison_bytes(&doc, sizeof(doc), 0xCCu);
  lonejson__init_map(&test_json_value_doc_map, &doc);
  status = lonejson_json_value_set_parse_visitor(&doc.selector, &visitor,
                                                 &state, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  options.clear_destination = 0;
  status = lonejson_parse_cstr(&test_json_value_doc_map, &doc, json, &options,
                               &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.object_count == 3u);
  EXPECT(state.array_count == 1u);
  EXPECT(state.key_begin_count == 6u);
  EXPECT(state.key_end_count == 6u);
  EXPECT(state.string_begin_count == 5u);
  EXPECT(state.string_end_count == 5u);
  EXPECT(state.string_chunk_count >= 5u);
  EXPECT(state.number_begin_count == 0u);
  EXPECT(strcmp(state.last_string, "done") == 0);
  lonejson_cleanup(&test_json_value_doc_map, &doc);
}

static void test_json_value_setters_and_failures(void) {
  const char *selector_pretty = "{\n"
                                "  \"kind\": \"term\",\n"
                                "  \"field\": \"name\",\n"
                                "  \"value\": \"alice\"\n"
                                "}";
  const char *fields_pretty = "[\"id\", \"name\", \"created_at\"]";
  const char *error_pretty =
      "{ \"code\": \"bad_selector\", \"detail\": { \"path\": \"/name\" } }";
  const char *expected_compact =
      "{\"id\":\"emit-1\",\"selector\":{\"kind\":\"term\",\"field\":\"name\","
      "\"value\":\"alice\"},\"fields\":[\"id\",\"name\",\"created_at\"],"
      "\"last_error\":{\"code\":\"bad_selector\",\"detail\":{\"path\":\"/"
      "name\"}}}";
  char selector_path[] = "/tmp/lonejson-json-value-selector-XXXXXX";
  char error_path[] = "/tmp/lonejson-json-value-error-XXXXXX";
  test_json_value_doc doc;
  test_reader_state fields_reader;
  lonejson_error error;
  lonejson_status status;
  int selector_fd = -1;
  int error_fd = -1;
  FILE *error_fp = NULL;
  char *serialized;

  selector_fd = mkstemp(selector_path);
  EXPECT(selector_fd >= 0);
  error_fd = mkstemp(error_path);
  EXPECT(error_fd >= 0);
  if (selector_fd < 0 || error_fd < 0) {
    if (selector_fd >= 0) {
      close(selector_fd);
      unlink(selector_path);
    }
    if (error_fd >= 0) {
      close(error_fd);
      unlink(error_path);
    }
    return;
  }
  EXPECT(write(selector_fd, selector_pretty, strlen(selector_pretty)) ==
         (ssize_t)strlen(selector_pretty));
  EXPECT(write(error_fd, error_pretty, strlen(error_pretty)) ==
         (ssize_t)strlen(error_pretty));
  error_fp = fdopen(error_fd, "rb");
  EXPECT(error_fp != NULL);
  if (error_fp == NULL) {
    close(selector_fd);
    close(error_fd);
    unlink(selector_path);
    unlink(error_path);
    return;
  }

  poison_bytes(&doc, sizeof(doc), 0xABu);
  lonejson__init_map(&test_json_value_doc_map, &doc);
  strcpy(doc.id, "emit-1");
  fields_reader.json = fields_pretty;
  fields_reader.offset = 0u;
  fields_reader.chunk_size = 4u;
  status = lonejson_json_value_set_path(&doc.selector, selector_path, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_json_value_set_reader(&doc.fields, test_state_reader,
                                          &fields_reader, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_json_value_set_file(&doc.last_error, error_fp, &error);
  EXPECT(status == LONEJSON_STATUS_OK);

  serialized = lonejson_serialize_alloc(&test_json_value_doc_map, &doc, NULL,
                                        NULL, &error);
  EXPECT(serialized != NULL);
  if (serialized != NULL) {
    EXPECT(strcmp(serialized, expected_compact) == 0);
    LONEJSON_FREE(serialized);
  }

  status = lonejson_json_value_set_buffer(&doc.selector, "{bad", 4u, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);

  status = lonejson_json_value_set_buffer(&doc.selector, selector_pretty,
                                          strlen(selector_pretty), &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(doc.selector.json,
                "{\"kind\":\"term\",\"field\":\"name\",\"value\":\"alice\"}") ==
         0);

  lonejson_cleanup(&test_json_value_doc_map, &doc);
  fclose(error_fp);
  close(selector_fd);
  unlink(selector_path);
  unlink(error_path);
}

static void test_json_value_scalars_null_and_reset(void) {
  const char *json =
      "{\"id\":\"scalar-1\",\"selector\":null,"
      "\"fields\":\"line\\nquoted\\\"value\",\"last_error\":false}";
  test_json_value_doc doc;
  lonejson_error error;
  lonejson_status status;
  char *serialized;

  lonejson_parse_options options = lonejson_default_parse_options();

  poison_bytes(&doc, sizeof(doc), 0xC1u);
  lonejson__init_map(&test_json_value_doc_map, &doc);
  status = lonejson_json_value_enable_parse_capture(&doc.selector, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_json_value_enable_parse_capture(&doc.fields, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_json_value_enable_parse_capture(&doc.last_error, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  options.clear_destination = 0;
  status = lonejson_parse_cstr(&test_json_value_doc_map, &doc, json, &options,
                               &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(doc.selector.kind == LONEJSON_JSON_VALUE_BUFFER);
  EXPECT(strcmp(doc.selector.json, "null") == 0);
  EXPECT(strcmp(doc.fields.json, "\"line\\nquoted\\\"value\"") == 0);
  EXPECT(strcmp(doc.last_error.json, "false") == 0);

  serialized = lonejson_serialize_alloc(&test_json_value_doc_map, &doc, NULL,
                                        NULL, &error);
  EXPECT(serialized != NULL);
  if (serialized != NULL) {
    EXPECT(strcmp(serialized, json) == 0);
    LONEJSON_FREE(serialized);
  }

  reset_lonejson_alloc_stats();
  status =
      lonejson_json_value_set_buffer(&doc.selector, "{\"a\":1}", 7u, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(g_alloc_record_count == 1u);
  lonejson_json_value_reset(&doc.selector);
  EXPECT(doc.selector.kind == LONEJSON_JSON_VALUE_NULL);
  EXPECT(g_alloc_record_count == 0u);

  serialized = lonejson_serialize_alloc(&test_json_value_doc_map, &doc, NULL,
                                        NULL, &error);
  EXPECT(serialized != NULL);
  if (serialized != NULL) {
    EXPECT(strstr(serialized, "\"selector\":null") != NULL);
    LONEJSON_FREE(serialized);
  }

  lonejson_cleanup(&test_json_value_doc_map, &doc);
  EXPECT(g_alloc_record_count == 0u);
}

static void test_json_value_reuse_and_cleanup_ownership(void) {
  char selector_path[] = "/tmp/lonejson-json-value-reuse-XXXXXX";
  test_json_value_doc doc;
  lonejson_json_value value;
  lonejson_error error;
  lonejson_status status;
  char *serialized;
  int fd;

  reset_lonejson_alloc_stats();
  lonejson_json_value_init(&value);
  fd = write_temp_text_file(selector_path, "{\"from\":\"path\"}");
  EXPECT(fd >= 0);
  if (fd < 0) {
    return;
  }
  close(fd);

  status = lonejson_json_value_set_path(&value, selector_path, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(g_alloc_record_count == 1u);

  status = lonejson_json_value_set_buffer(
      &value, "{\"from\":\"buffer\"}", strlen("{\"from\":\"buffer\"}"), &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(value.kind == LONEJSON_JSON_VALUE_BUFFER);
  EXPECT(g_alloc_record_count == 1u);
  unlink(selector_path);

  lonejson_init(&test_json_value_doc_map, &doc);
  strcpy(doc.id, "reuse");
  doc.selector = value;
  lonejson_json_value_init(&value);
  serialized = lonejson_serialize_alloc(&test_json_value_doc_map, &doc, NULL,
                                        NULL, &error);
  EXPECT(serialized != NULL);
  if (serialized != NULL) {
    EXPECT(strstr(serialized, "\"selector\":{\"from\":\"buffer\"}") != NULL);
    LONEJSON_FREE(serialized);
  }

  lonejson_cleanup(&test_json_value_doc_map, &doc);
  lonejson_json_value_cleanup(&value);
  EXPECT(g_alloc_record_count == 0u);
}

static void test_json_value_source_validation_failures(void) {
  lonejson_json_value value;
  lonejson_error error;
  lonejson_status status;
  char trailing_path[] = "/tmp/lonejson-json-value-trailing-XXXXXX";
  char invalid_path[] = "/tmp/lonejson-json-value-invalid-XXXXXX";
  char trailing_path2[] = "/tmp/lonejson-json-value-trailing-XXXXXX";
  char invalid_path2[] = "/tmp/lonejson-json-value-invalid-XXXXXX";
  int trailing_fd;
  int invalid_fd;
  FILE *fp;
  test_reader_state trailing_reader;
  test_reader_state invalid_reader;
  test_counting_sink sink;

  lonejson_json_value_init(&value);

  status = lonejson_json_value_set_buffer(&value, "{} trailing",
                                          strlen("{} trailing"), &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
  status = lonejson_json_value_set_buffer(&value, "{bad", 4u, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);

  trailing_reader.json = "{} trailing";
  trailing_reader.offset = 0u;
  trailing_reader.chunk_size = 3u;
  status = lonejson_json_value_set_reader(&value, test_state_reader,
                                          &trailing_reader, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  memset(&sink, 0, sizeof(sink));
  status = lonejson_json_value_write_to_sink(&value, test_counting_sink_write,
                                             &sink, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);

  invalid_reader.json = "{bad";
  invalid_reader.offset = 0u;
  invalid_reader.chunk_size = 2u;
  status = lonejson_json_value_set_reader(&value, test_state_reader,
                                          &invalid_reader, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  memset(&sink, 0, sizeof(sink));
  status = lonejson_json_value_write_to_sink(&value, test_counting_sink_write,
                                             &sink, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);

  trailing_fd = write_temp_text_file(trailing_path, "{} trailing");
  EXPECT(trailing_fd >= 0);
  invalid_fd = write_temp_text_file(invalid_path, "{bad");
  EXPECT(invalid_fd >= 0);
  if (trailing_fd >= 0) {
    close(trailing_fd);
    status = lonejson_json_value_set_path(&value, trailing_path, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    memset(&sink, 0, sizeof(sink));
    status = lonejson_json_value_write_to_sink(&value, test_counting_sink_write,
                                               &sink, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
    unlink(trailing_path);
  }
  if (invalid_fd >= 0) {
    close(invalid_fd);
    status = lonejson_json_value_set_path(&value, invalid_path, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    memset(&sink, 0, sizeof(sink));
    status = lonejson_json_value_write_to_sink(&value, test_counting_sink_write,
                                               &sink, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
    unlink(invalid_path);
  }

  trailing_fd = write_temp_text_file(trailing_path2, "{} trailing");
  EXPECT(trailing_fd >= 0);
  if (trailing_fd >= 0) {
    fp = fdopen(trailing_fd, "rb");
    EXPECT(fp != NULL);
    if (fp != NULL) {
      status = lonejson_json_value_set_file(&value, fp, &error);
      EXPECT(status == LONEJSON_STATUS_OK);
      memset(&sink, 0, sizeof(sink));
      status = lonejson_json_value_write_to_sink(
          &value, test_counting_sink_write, &sink, &error);
      EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
      fclose(fp);
    } else {
      close(trailing_fd);
    }
    unlink(trailing_path2);
  }

  invalid_fd = write_temp_text_file(invalid_path2, "{bad");
  EXPECT(invalid_fd >= 0);
  if (invalid_fd >= 0) {
    status = lonejson_json_value_set_fd(&value, invalid_fd, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    memset(&sink, 0, sizeof(sink));
    status = lonejson_json_value_write_to_sink(&value, test_counting_sink_write,
                                               &sink, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
    close(invalid_fd);
    unlink(invalid_path2);
  }

  lonejson_json_value_cleanup(&value);
}

static void
test_json_value_parse_requires_destination_and_partial_failure(void) {
  const char *missing_config_json =
      "{\"id\":\"missing\",\"selector\":{\"a\":1}}";
  const char *broken_json =
      "{\"id\":\"broken\",\"selector\":{\"a\":[1,2,},\"fields\":null}";
  test_json_value_doc doc;
  lonejson_parse_options options = lonejson_default_parse_options();
  lonejson_error error;
  lonejson_status status;
  unsigned char selector_buf[128];
  test_buffer_sink selector_sink;

  poison_bytes(&doc, sizeof(doc), 0xD3u);
  lonejson__init_map(&test_json_value_doc_map, &doc);
  status = lonejson_parse_cstr(&test_json_value_doc_map, &doc,
                               missing_config_json, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  lonejson_cleanup(&test_json_value_doc_map, &doc);

  memset(&selector_sink, 0, sizeof(selector_sink));
  selector_sink.buffer = selector_buf;
  selector_sink.capacity = sizeof(selector_buf);
  poison_bytes(&doc, sizeof(doc), 0xD4u);
  lonejson__init_map(&test_json_value_doc_map, &doc);
  status = lonejson_json_value_set_parse_sink(
      &doc.selector, test_buffer_sink_write, &selector_sink, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_json_value_enable_parse_capture(&doc.fields, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  options.clear_destination = 0;
  status = lonejson_parse_cstr(&test_json_value_doc_map, &doc, broken_json,
                               &options, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
  EXPECT(strstr((const char *)selector_buf, "{\"a\":[1,2") != NULL);
  lonejson_cleanup(&test_json_value_doc_map, &doc);
}

static void test_json_value_nonseekable_and_sink_failures(void) {
  lonejson_json_value value;
  lonejson_error error;
  lonejson_status status;
  int pipe_fds[2];
  FILE *fp;
  test_failing_sink failing_sink;
  test_counting_sink sink;

  lonejson_json_value_init(&value);

  if (pipe(pipe_fds) == 0) {
    EXPECT(write(pipe_fds[1], "{\"x\":1}", 7u) == 7);
    close(pipe_fds[1]);
    status = lonejson_json_value_set_fd(&value, pipe_fds[0], &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    memset(&sink, 0, sizeof(sink));
    status = lonejson_json_value_write_to_sink(&value, test_counting_sink_write,
                                               &sink, &error);
    EXPECT(status == LONEJSON_STATUS_IO_ERROR);
    close(pipe_fds[0]);
  } else {
    EXPECT(0);
  }

  if (pipe(pipe_fds) == 0) {
    EXPECT(write(pipe_fds[1], "{\"x\":1}", 7u) == 7);
    close(pipe_fds[1]);
    fp = fdopen(pipe_fds[0], "rb");
    EXPECT(fp != NULL);
    if (fp != NULL) {
      status = lonejson_json_value_set_file(&value, fp, &error);
      EXPECT(status == LONEJSON_STATUS_OK);
      memset(&sink, 0, sizeof(sink));
      status = lonejson_json_value_write_to_sink(
          &value, test_counting_sink_write, &sink, &error);
      EXPECT(status == LONEJSON_STATUS_IO_ERROR);
      fclose(fp);
    } else {
      close(pipe_fds[0]);
    }
  } else {
    EXPECT(0);
  }

  status = lonejson_json_value_set_buffer(
      &value, "{\"kind\":\"term\",\"field\":\"name\"}",
      strlen("{\"kind\":\"term\",\"field\":\"name\"}"), &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  failing_sink.fail_after = 8u;
  failing_sink.total = 0u;
  status = lonejson_json_value_write_to_sink(&value, test_failing_sink_write,
                                             &failing_sink, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);

  lonejson_json_value_cleanup(&value);
}

static void test_json_value_large_source_backed_serialization(void) {
  test_json_value_doc doc;
  lonejson_error error;
  lonejson_status status;
  test_counting_sink sink;
  char large_path[] = "/tmp/lonejson-json-value-large-XXXXXX";
  char *large_array;
  size_t payload_len;
  int fd;
  size_t i;

  large_array = (char *)malloc(300000u);
  EXPECT(large_array != NULL);
  if (large_array == NULL) {
    return;
  }
  payload_len = 0u;
  large_array[payload_len++] = '[';
  for (i = 0u; i < 40000u; ++i) {
    int wrote;
    wrote = snprintf(large_array + payload_len, 300000u - payload_len, "%s%lu",
                     (i == 0u) ? "" : ",", (unsigned long)i);
    EXPECT(wrote > 0);
    if (wrote <= 0) {
      free(large_array);
      return;
    }
    payload_len += (size_t)wrote;
  }
  large_array[payload_len++] = ']';
  large_array[payload_len] = '\0';

  fd = write_temp_text_file(large_path, large_array);
  EXPECT(fd >= 0);
  if (fd < 0) {
    free(large_array);
    return;
  }
  close(fd);

  poison_bytes(&doc, sizeof(doc), 0xCDu);
  lonejson__init_map(&test_json_value_doc_map, &doc);
  strcpy(doc.id, "large-1");
  status = lonejson_json_value_set_path(&doc.fields, large_path, &error);
  EXPECT(status == LONEJSON_STATUS_OK);

  reset_lonejson_alloc_stats();
  memset(&sink, 0, sizeof(sink));
  status =
      lonejson_serialize_sink(&test_json_value_doc_map, &doc,
                              test_counting_sink_write, &sink, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(sink.total > payload_len);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);

  lonejson_cleanup(&test_json_value_doc_map, &doc);
  unlink(large_path);
  free(large_array);
}

static void test_json_value_nested_failure_matrix(void) {
  static const char *invalid_cases[] = {
      "{\"a\":[1,2,}",
      "{\"a\":{\"b\":}}",
      "{\"a\":\"unterminated}",
      "{\"a\":tru}",
      "{\"a\":01}",
      "[{\"a\":1},]",
      "{\"a\":{\"b\":[null,false,true,{\"c\":}]}}",
      "{\"a\":[{\"b\":\"x\"}]} trailing"};
  static const char *invalid_parse_docs[] = {
      "{\"id\":\"bad-1\",\"selector\":{\"a\":[1,2,},\"fields\":null}",
      "{\"id\":\"bad-2\",\"selector\":[{\"a\":1},],\"fields\":null}",
      "{\"id\":\"bad-3\",\"selector\":{\"a\":{\"b\":}},\"fields\":null}"};
  size_t i;
  lonejson_error error;
  lonejson_status status;
  lonejson_json_value value;
  test_counting_sink sink;
  char path_template[] = "/tmp/lonejson-json-value-matrix-XXXXXX";
  int fd;
  FILE *fp;
  test_reader_state reader_state;
  test_json_value_doc doc;

  for (i = 0u; i < sizeof(invalid_parse_docs) / sizeof(invalid_parse_docs[0]);
       ++i) {
    lonejson_parse_options options = lonejson_default_parse_options();
    poison_bytes(&doc, sizeof(doc), 0xE7u);
    lonejson__init_map(&test_json_value_doc_map, &doc);
    status = lonejson_json_value_enable_parse_capture(&doc.selector, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    status = lonejson_json_value_enable_parse_capture(&doc.fields, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    status = lonejson_json_value_enable_parse_capture(&doc.last_error, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    options.clear_destination = 0;
    status = lonejson_parse_cstr(&test_json_value_doc_map, &doc,
                                 invalid_parse_docs[i], &options, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
    lonejson_cleanup(&test_json_value_doc_map, &doc);
  }

  lonejson_json_value_init(&value);
  for (i = 0u; i < sizeof(invalid_cases) / sizeof(invalid_cases[0]); ++i) {
    status = lonejson_json_value_set_buffer(&value, invalid_cases[i],
                                            strlen(invalid_cases[i]), &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);

    reader_state.json = invalid_cases[i];
    reader_state.offset = 0u;
    reader_state.chunk_size = 3u;
    status = lonejson_json_value_set_reader(&value, test_state_reader,
                                            &reader_state, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    memset(&sink, 0, sizeof(sink));
    status = lonejson_json_value_write_to_sink(&value, test_counting_sink_write,
                                               &sink, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);

    strcpy(path_template, "/tmp/lonejson-json-value-matrix-XXXXXX");
    fd = write_temp_text_file(path_template, invalid_cases[i]);
    EXPECT(fd >= 0);
    if (fd >= 0) {
      close(fd);
      status = lonejson_json_value_set_path(&value, path_template, &error);
      EXPECT(status == LONEJSON_STATUS_OK);
      memset(&sink, 0, sizeof(sink));
      status = lonejson_json_value_write_to_sink(
          &value, test_counting_sink_write, &sink, &error);
      EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
      unlink(path_template);
    }

    strcpy(path_template, "/tmp/lonejson-json-value-matrix-XXXXXX");
    fd = write_temp_text_file(path_template, invalid_cases[i]);
    EXPECT(fd >= 0);
    if (fd >= 0) {
      fp = fdopen(fd, "rb");
      EXPECT(fp != NULL);
      if (fp != NULL) {
        status = lonejson_json_value_set_file(&value, fp, &error);
        EXPECT(status == LONEJSON_STATUS_OK);
        memset(&sink, 0, sizeof(sink));
        status = lonejson_json_value_write_to_sink(
            &value, test_counting_sink_write, &sink, &error);
        EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
        fclose(fp);
      } else {
        close(fd);
      }
      unlink(path_template);
    }

    strcpy(path_template, "/tmp/lonejson-json-value-matrix-XXXXXX");
    fd = write_temp_text_file(path_template, invalid_cases[i]);
    EXPECT(fd >= 0);
    if (fd >= 0) {
      status = lonejson_json_value_set_fd(&value, fd, &error);
      EXPECT(status == LONEJSON_STATUS_OK);
      memset(&sink, 0, sizeof(sink));
      status = lonejson_json_value_write_to_sink(
          &value, test_counting_sink_write, &sink, &error);
      EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
      close(fd);
      unlink(path_template);
    }
  }
  lonejson_json_value_cleanup(&value);
}

static void test_value_visitor_success_and_limits(void) {
  const char *json = "{\"name\":\"a\\n\\u20ac\",\"items\":[1,-2.5e3,true,false,"
                     "null,{\"x\":[]}]}";
  lonejson_value_visitor visitor = test_value_visitor();
  lonejson_value_limits limits = lonejson_default_value_limits();
  lonejson_error error;
  lonejson_status status;
  test_visit_state state;
  test_reader_state reader;
  char path[] = "/tmp/lonejson-value-visitor-XXXXXX";
  int fd;
  FILE *fp;

  memset(&state, 0, sizeof(state));
  status = lonejson_visit_value_cstr(json, &visitor, &state, &limits, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strstr(state.log, "{K(name)S(a\n") != NULL);
  EXPECT(strstr(state.log, "K(items)[N(1)N(-2.5e3)TFZ{K(x)[]}]") != NULL);

  memset(&state, 0, sizeof(state));
  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 2u;
  status = lonejson_visit_value_reader(test_state_reader, &reader, &visitor,
                                       &state, &limits, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(
      strcmp(state.log,
             "{K(name)S(a\n\xE2\x82\xAC)K(items)[N(1)N(-2.5e3)TFZ{K(x)[]}]}") ==
      0);

  strcpy(path, "/tmp/lonejson-value-visitor-XXXXXX");
  fd = write_temp_text_file(path, json);
  EXPECT(fd >= 0);
  if (fd >= 0) {
    close(fd);
    memset(&state, 0, sizeof(state));
    status = lonejson_visit_value_path(path, &visitor, &state, &limits, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    unlink(path);
  }

  limits.max_depth = 2u;
  status = lonejson_visit_value_cstr(json, &visitor, &state, &limits, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);

  limits = lonejson_default_value_limits();
  limits.max_key_bytes = 3u;
  status = lonejson_visit_value_cstr(json, &visitor, &state, &limits, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);

  limits = lonejson_default_value_limits();
  limits.max_string_bytes = 2u;
  status = lonejson_visit_value_cstr("{\"k\":\"abcd\"}", &visitor, &state,
                                     &limits, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);

  limits = lonejson_default_value_limits();
  limits.max_number_bytes = 3u;
  status =
      lonejson_visit_value_cstr("12345", &visitor, &state, &limits, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);

  limits = lonejson_default_value_limits();
  limits.max_total_bytes = 5u;
  status =
      lonejson_visit_value_cstr("{\"k\":1}", &visitor, &state, &limits, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);

  status = lonejson_visit_value_cstr("{\"k\":1} trailing", &visitor, &state,
                                     NULL, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);

  status =
      lonejson_visit_value_cstr("{\"k\":tru}", &visitor, &state, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_JSON);

  strcpy(path, "/tmp/lonejson-value-visitor-XXXXXX");
  fd = write_temp_text_file(path, json);
  EXPECT(fd >= 0);
  if (fd >= 0) {
    fp = fdopen(fd, "rb");
    EXPECT(fp != NULL);
    if (fp != NULL) {
      EXPECT(fseek(fp, 0L, SEEK_SET) == 0);
      memset(&state, 0, sizeof(state));
      status = lonejson_visit_value_filep(fp, &visitor, &state, NULL, &error);
      EXPECT(status == LONEJSON_STATUS_OK);
      fclose(fp);
    } else {
      close(fd);
    }
    unlink(path);
  }

  strcpy(path, "/tmp/lonejson-value-visitor-XXXXXX");
  fd = write_temp_text_file(path, json);
  EXPECT(fd >= 0);
  if (fd >= 0) {
    EXPECT(lseek(fd, 0, SEEK_SET) == 0);
    memset(&state, 0, sizeof(state));
    status = lonejson_visit_value_fd(fd, &visitor, &state, NULL, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    close(fd);
    unlink(path);
  }
}

static void test_value_visitor_chunking_unicode_and_failures(void) {
  const char *json =
      "{\"\\u20acuro_key\":\"line\\n\\uD834\\uDD1E\","
      "\"items\":[\"alpha\",\"beta\",-12345.678e-9,true,false,null]}";
  static const char *invalid_cases[] = {
      "{\"bad\":\"\\uD834x\"}", "{\"bad\":\"\\uD834\\u0000\"}",
      "{\"bad\":\"\\uDD1E\"}",  "{\"bad\":\"\\u12\"}",
      "{\"bad\":\"\\x41\"}",    "{\"bad\":[1,2,}",
      "{\"bad\":1} trailing"};
  lonejson_value_visitor visitor = test_counting_value_visitor();
  lonejson_error error;
  lonejson_status status;
  test_visit_chunk_state state;
  test_reader_state reader;
  lonejson_value_limits limits;
  size_t i;

  memset(&state, 0, sizeof(state));
  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 1u;
  status = lonejson_visit_value_reader(test_state_reader, &reader, &visitor,
                                       &state, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(state.object_count == 1u);
  EXPECT(state.array_count == 1u);
  EXPECT(state.key_begin_count == 2u);
  EXPECT(state.key_chunk_count > state.key_begin_count);
  EXPECT(state.string_begin_count == 3u);
  EXPECT(state.string_chunk_count > state.string_begin_count);
  EXPECT(state.number_begin_count == 1u);
  EXPECT(state.number_chunk_count >= 1u);
  EXPECT(state.bool_count == 2u);
  EXPECT(state.null_count == 1u);
  EXPECT(strcmp(state.last_number, "-12345.678e-9") == 0);

  memset(&state, 0, sizeof(state));
  limits = lonejson_default_value_limits();
  limits.max_total_bytes = strlen(json);
  status = lonejson_visit_value_cstr(json, &visitor, &state, &limits, &error);
  EXPECT(status == LONEJSON_STATUS_OK);

  memset(&state, 0, sizeof(state));
  state.fail_after = 6u;
  status = lonejson_visit_value_cstr(json, &visitor, &state, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_CALLBACK_FAILED);
  EXPECT(state.callback_count == 6u);
  EXPECT(state.key_begin_count > 0u);

  status = lonejson_visit_value_cstr(json, NULL, &state, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);

  memset(&state, 0, sizeof(state));
  status = lonejson_visit_value_cstr(json, &visitor, &state, NULL, NULL);
  EXPECT(status == LONEJSON_STATUS_OK);

  for (i = 0u; i < sizeof(invalid_cases) / sizeof(invalid_cases[0]); ++i) {
    memset(&state, 0, sizeof(state));
    status = lonejson_visit_value_cstr(invalid_cases[i], &visitor, &state, NULL,
                                       &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_JSON);
  }
}

static void test_custom_allocator_parse_cleanup_and_stream(void) {
  static const char json[] =
      "{\"name\":\"alpha\",\"body\":\"hello world\",\"tags\":[\"x\",\"y\"]}";
  test_allocator_state alloc;
  lonejson_parse_options options = lonejson_default_parse_options();
  lonejson_error error;
  lonejson_status status;
  test_alloc_parse_doc doc;
  lonejson_stream *stream;
  test_reader_state reader;

  test_allocator_init(&alloc);
  memset(&doc, 0, sizeof(doc));
  options.allocator = &alloc.allocator;
  status = lonejson_parse_cstr(&test_alloc_parse_doc_map, &doc, json, &options,
                               &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(alloc.stats.alloc_calls > 0u);
  EXPECT(alloc.stats.bytes_live > 0u);
  lonejson_cleanup(&test_alloc_parse_doc_map, &doc);
  EXPECT(alloc.stats.bytes_live == 0u);
  EXPECT(alloc.stats.free_calls > 0u);

  test_allocator_init(&alloc);
  options = lonejson_default_parse_options();
  options.allocator = &alloc.allocator;
  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 7u;
  stream = lonejson_stream_open_reader(
      &test_alloc_parse_doc_map, test_state_reader, &reader, &options, &error);
  EXPECT(stream != NULL);
  EXPECT(alloc.stats.alloc_calls > 0u);
  if (stream != NULL) {
    memset(&doc, 0, sizeof(doc));
    EXPECT(lonejson_stream_next(stream, &doc, &error) ==
           LONEJSON_STREAM_OBJECT);
    lonejson_cleanup(&test_alloc_parse_doc_map, &doc);
    lonejson_stream_close(stream);
  }
  EXPECT(alloc.stats.bytes_live == 0u);
}

static void test_custom_allocator_json_value_capture_and_serialize_alloc(void) {
  static const char json[] =
      "{\"id\":\"q1\",\"selector\":{\"a\":[1,true,null]}}";
  test_allocator_state parse_alloc;
  test_allocator_state write_alloc;
  lonejson_parse_options parse_options = lonejson_default_parse_options();
  lonejson_write_options write_options = lonejson_default_write_options();
  lonejson_error error;
  lonejson_status status;
  test_alloc_json_value_doc doc;
  test_event event;
  lonejson_owned_buffer serialized;

  test_allocator_init(&parse_alloc);
  test_allocator_init(&write_alloc);
  lonejson_json_value_init_with_allocator(&doc.selector,
                                          &parse_alloc.allocator);
  status = lonejson_json_value_enable_parse_capture(&doc.selector, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  memset(doc.id, 0, sizeof(doc.id));
  parse_options.clear_destination = 0;
  parse_options.allocator = &parse_alloc.allocator;
  status = lonejson_parse_cstr(&test_alloc_json_value_doc_map, &doc, json,
                               &parse_options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(parse_alloc.stats.alloc_calls > 0u);
  EXPECT(parse_alloc.stats.bytes_live > 0u);
  EXPECT(strcmp(doc.selector.json, "{\"a\":[1,true,null]}") == 0);
  lonejson_cleanup(&test_alloc_json_value_doc_map, &doc);
  EXPECT(parse_alloc.stats.bytes_live == 0u);

  write_options.allocator = &write_alloc.allocator;
  memset(&event, 0, sizeof(event));
  memcpy(event.id, "evt-1", 6u);
  event.ok = true;
  lonejson_owned_buffer_init(&serialized);
  status = lonejson_serialize_owned(&test_event_map, &event, &serialized,
                                    &write_options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(serialized.data != NULL);
  EXPECT(write_alloc.stats.alloc_calls > 0u);
  EXPECT(write_alloc.stats.bytes_live > 0u);
  if (serialized.data != NULL) {
    EXPECT(strcmp(serialized.data, "{\"id\":\"evt-1\",\"ok\":true}") == 0);
    lonejson_owned_buffer_free(&serialized);
  }
  EXPECT(write_alloc.stats.bytes_live == 0u);
}

static void test_custom_allocator_raw_serialize_alloc_is_rejected(void) {
  test_allocator_state write_alloc;
  lonejson_write_options write_options = lonejson_default_write_options();
  lonejson_error error;
  test_event event;
  char *serialized;

  test_allocator_init(&write_alloc);
  write_options.allocator = &write_alloc.allocator;
  memset(&event, 0, sizeof(event));
  memcpy(event.id, "evt-raw", 8u);
  event.ok = true;
  serialized = lonejson_serialize_alloc(&test_event_map, &event, NULL,
                                        &write_options, &error);
  EXPECT(serialized == NULL);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(strstr(error.message,
                "serialize_alloc uses the default allocator only") != NULL);
  EXPECT(write_alloc.stats.alloc_calls == 0u);
  EXPECT(write_alloc.stats.realloc_calls == 0u);
  EXPECT(write_alloc.stats.free_calls == 0u);
  EXPECT(write_alloc.stats.bytes_live == 0u);
}

static void
test_explicit_default_allocator_raw_serialize_alloc_is_allowed(void) {
  lonejson_write_options options = lonejson_default_write_options();
  lonejson_allocator allocator = lonejson_default_allocator();
  lonejson_error error;
  test_event event;
  char *serialized;

  options.allocator = &allocator;
  memset(&event, 0, sizeof(event));
  memcpy(event.id, "evt-def", 8u);
  event.ok = true;
  serialized =
      lonejson_serialize_alloc(&test_event_map, &event, NULL, &options, &error);
  EXPECT(serialized != NULL);
  if (serialized != NULL) {
    EXPECT(strcmp(serialized, "{\"id\":\"evt-def\",\"ok\":true}") == 0);
    LONEJSON_FREE(serialized);
  }
}

static void
test_explicit_default_allocator_raw_serialize_jsonl_alloc_is_allowed(void) {
  lonejson_write_options options = lonejson_default_write_options();
  lonejson_allocator allocator = lonejson_default_allocator();
  lonejson_error error;
  test_event events[2];
  char *serialized;

  options.allocator = &allocator;
  memset(events, 0, sizeof(events));
  memcpy(events[0].id, "evt-1", 6u);
  events[0].ok = true;
  memcpy(events[1].id, "evt-2", 6u);
  events[1].ok = false;
  serialized = lonejson_serialize_jsonl_alloc(&test_event_map, events, 2u, 0u,
                                              NULL, &options, &error);
  EXPECT(serialized != NULL);
  if (serialized != NULL) {
    EXPECT(strcmp(serialized, "{\"id\":\"evt-1\",\"ok\":true}\n"
                              "{\"id\":\"evt-2\",\"ok\":false}\n") == 0);
    LONEJSON_FREE(serialized);
  }
}

static void
test_custom_allocator_json_value_capture_preserves_handle_allocator(void) {
  static const char json[] = "{\"id\":\"q2\",\"selector\":{\"k\":[1,2,3]}}";
  test_allocator_state parse_alloc;
  lonejson_parse_options parse_options = lonejson_default_parse_options();
  lonejson_error error;
  lonejson_status status;
  test_alloc_json_value_doc doc;

  memset(&doc, 0, sizeof(doc));
  test_allocator_init(&parse_alloc);
  lonejson_json_value_init_with_allocator(&doc.selector,
                                          &parse_alloc.allocator);
  status = lonejson_json_value_enable_parse_capture(&doc.selector, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  parse_options.clear_destination = 0;
  parse_options.allocator = NULL;
  status = lonejson_parse_cstr(&test_alloc_json_value_doc_map, &doc, json,
                               &parse_options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(parse_alloc.stats.alloc_calls > 0u);
  EXPECT(parse_alloc.stats.bytes_live > 0u);
  EXPECT(strcmp(doc.selector.json, "{\"k\":[1,2,3]}") == 0);
  lonejson_cleanup(&test_alloc_json_value_doc_map, &doc);
  EXPECT(parse_alloc.stats.bytes_live == 0u);
}

static void test_partial_allocator_parse_is_rejected(void) {
  static const char json[] = "{\"name\":\"alpha\",\"body\":\"hello\"}";
  test_allocator_state alloc;
  lonejson_parse_options options = lonejson_default_parse_options();
  lonejson_error error;
  lonejson_status status;
  test_alloc_parse_doc doc;

  memset(&doc, 0, sizeof(doc));
  test_allocator_init(&alloc);
  alloc.allocator.realloc_fn = NULL;
  alloc.allocator.free_fn = NULL;
  options.allocator = &alloc.allocator;
  status = lonejson_parse_cstr(&test_alloc_parse_doc_map, &doc, json, &options,
                               &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(strstr(error.message,
                "allocator must provide either all callbacks or none") != NULL);
  EXPECT(alloc.stats.alloc_calls == 0u);
  EXPECT(alloc.stats.realloc_calls == 0u);
  EXPECT(alloc.stats.free_calls == 0u);
  EXPECT(alloc.stats.bytes_live == 0u);
}

static void test_partial_allocator_parse_reader_is_rejected(void) {
  static const char json[] = "{\"name\":\"alpha\",\"body\":\"hello\"}";
  test_allocator_state alloc;
  lonejson_parse_options options = lonejson_default_parse_options();
  lonejson_error error;
  lonejson_status status;
  test_alloc_parse_doc doc;
  test_reader_state reader;

  memset(&doc, 0, sizeof(doc));
  test_allocator_init(&alloc);
  alloc.allocator.realloc_fn = NULL;
  alloc.allocator.free_fn = NULL;
  options.allocator = &alloc.allocator;
  reader.json = json;
  reader.offset = 0u;
  reader.chunk_size = 5u;
  status = lonejson_parse_reader(&test_alloc_parse_doc_map, &doc,
                                 test_state_reader, &reader, &options, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(strstr(error.message,
                "allocator must provide either all callbacks or none") != NULL);
  EXPECT(alloc.stats.alloc_calls == 0u);
  EXPECT(alloc.stats.realloc_calls == 0u);
  EXPECT(alloc.stats.free_calls == 0u);
  EXPECT(alloc.stats.bytes_live == 0u);
}

static void test_partial_allocator_serialize_alloc_is_rejected(void) {
  test_allocator_state alloc;
  lonejson_write_options options = lonejson_default_write_options();
  lonejson_error error;
  test_event event;
  char *serialized;

  memset(&event, 0, sizeof(event));
  memcpy(event.id, "evt-2", 6u);
  event.ok = true;
  test_allocator_init(&alloc);
  alloc.allocator.realloc_fn = NULL;
  alloc.allocator.free_fn = NULL;
  options.allocator = &alloc.allocator;
  serialized =
      lonejson_serialize_alloc(&test_event_map, &event, NULL, &options, &error);
  EXPECT(serialized == NULL);
  EXPECT(error.code == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(strstr(error.message,
                "allocator must provide either all callbacks or none") != NULL);
  EXPECT(alloc.stats.alloc_calls == 0u);
  EXPECT(alloc.stats.realloc_calls == 0u);
  EXPECT(alloc.stats.free_calls == 0u);
  EXPECT(alloc.stats.bytes_live == 0u);
}

static void test_partial_allocator_generator_init_is_rejected(void) {
  test_allocator_state alloc;
  lonejson_write_options options = lonejson_default_write_options();
  lonejson_generator generator;
  test_event event;
  lonejson_status status;

  memset(&event, 0, sizeof(event));
  memcpy(event.id, "evt-3", 6u);
  event.ok = true;
  test_allocator_init(&alloc);
  alloc.allocator.realloc_fn = NULL;
  alloc.allocator.free_fn = NULL;
  options.allocator = &alloc.allocator;
  status =
      lonejson_generator_init(&generator, &test_event_map, &event, &options);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(generator.state == NULL);
  EXPECT(generator.error.code == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(strstr(generator.error.message,
                "allocator must provide either all callbacks or none") != NULL);
  EXPECT(alloc.stats.alloc_calls == 0u);
  EXPECT(alloc.stats.realloc_calls == 0u);
  EXPECT(alloc.stats.free_calls == 0u);
  EXPECT(alloc.stats.bytes_live == 0u);
}

static void test_generator_compact_streams_event_map(void) {
  test_event event;
  lonejson_generator generator;
  unsigned char chunk[7];
  char output[128];
  size_t out_len;
  size_t total;
  int eof;

  memset(&event, 0, sizeof(event));
  memset(&generator, 0, sizeof(generator));
  memset(output, 0, sizeof(output));
  strcpy(event.id, "evt-1");
  event.ok = true;

  EXPECT(lonejson_generator_init(&generator, &test_event_map, &event, NULL) ==
         LONEJSON_STATUS_OK);
  total = 0u;
  eof = 0;
  while (!eof) {
    EXPECT(lonejson_generator_read(&generator, chunk, sizeof(chunk), &out_len,
                                   &eof) == LONEJSON_STATUS_OK);
    EXPECT(total + out_len < sizeof(output));
    memcpy(output + total, chunk, out_len);
    total += out_len;
  }
  output[total] = '\0';
  EXPECT(strcmp(output, "{\"id\":\"evt-1\",\"ok\":true}") == 0);
  lonejson_generator_cleanup(&generator);
}

static void test_generator_pretty_streams_source_field(void) {
  test_source_doc doc;
  lonejson_generator generator;
  lonejson_write_options options = lonejson_default_write_options();
  char path[] = "/tmp/lonejson-generator-XXXXXX";
  unsigned char chunk[64];
  char output[512];
  size_t out_len;
  size_t total;
  int eof;
  int fd;
  FILE *fp;

  memset(&doc, 0, sizeof(doc));
  memset(&generator, 0, sizeof(generator));
  memset(output, 0, sizeof(output));
  strcpy(doc.id, "evt-1");
  lonejson_source_init(&doc.text);
  lonejson_source_init(&doc.bytes);
  options.pretty = 1;

  fd = mkstemp(path);
  EXPECT(fd >= 0);
  if (fd < 0) {
    return;
  }
  fp = fdopen(fd, "wb");
  EXPECT(fp != NULL);
  if (fp == NULL) {
    close(fd);
    unlink(path);
    return;
  }
  fputs("hello", fp);
  fclose(fp);

  EXPECT(lonejson_source_set_path(&doc.text, path, NULL) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_generator_init(&generator, &test_source_doc_map, &doc,
                                 &options) == LONEJSON_STATUS_OK);
  total = 0u;
  eof = 0;
  while (!eof) {
    EXPECT(lonejson_generator_read(&generator, chunk, sizeof(chunk), &out_len,
                                   &eof) == LONEJSON_STATUS_OK);
    EXPECT(total + out_len < sizeof(output));
    memcpy(output + total, chunk, out_len);
    total += out_len;
  }
  output[total] = '\0';
  EXPECT(strstr(output, "\n") != NULL);
  EXPECT(strstr(output, "\"text\": \"hello\"") != NULL);
  lonejson_generator_cleanup(&generator);
  lonejson_cleanup(&test_source_doc_map, &doc);
  unlink(path);
}

static void test_generator_streams_json_value_fields(void) {
  test_json_value_doc doc;
  lonejson_generator generator;
  unsigned char chunk[11];
  char output[512];
  size_t out_len;
  size_t total;
  int eof;

  memset(&doc, 0, sizeof(doc));
  memset(&generator, 0, sizeof(generator));
  memset(output, 0, sizeof(output));
  strcpy(doc.id, "evt-1");
  lonejson_json_value_init(&doc.selector);
  lonejson_json_value_init(&doc.fields);
  lonejson_json_value_init(&doc.last_error);
  EXPECT(lonejson_json_value_set_buffer(
             &doc.selector, "{\"op\":\"and\",\"clauses\":[1,true]}",
             strlen("{\"op\":\"and\",\"clauses\":[1,true]}"),
             NULL) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_json_value_set_buffer(
             &doc.fields, "[\"id\",{\"metrics\":[null,3.14]}]",
             strlen("[\"id\",{\"metrics\":[null,3.14]}]"),
             NULL) == LONEJSON_STATUS_OK);

  EXPECT(lonejson_generator_init(&generator, &test_json_value_doc_map, &doc,
                                 NULL) == LONEJSON_STATUS_OK);
  total = 0u;
  eof = 0;
  while (!eof) {
    EXPECT(lonejson_generator_read(&generator, chunk, sizeof(chunk), &out_len,
                                   &eof) == LONEJSON_STATUS_OK);
    EXPECT(total + out_len < sizeof(output));
    memcpy(output + total, chunk, out_len);
    total += out_len;
  }
  output[total] = '\0';
  EXPECT(strcmp(output,
                "{\"id\":\"evt-1\",\"selector\":{\"op\":\"and\",\"clauses\":[1,"
                "true]},\"fields\":[\"id\",{\"metrics\":[null,3.14]}],"
                "\"last_error\":null}") == 0);
  lonejson_generator_cleanup(&generator);
  lonejson_cleanup(&test_json_value_doc_map, &doc);
}

static void test_generator_pretty_streams_json_value_reader(void) {
  test_json_value_doc doc;
  lonejson_generator generator;
  lonejson_write_options options = lonejson_default_write_options();
  test_reader_state selector_reader = {
      "{\"filters\":[{\"op\":\"eq\",\"field\":\"kind\",\"value\":\"event\"},"
      "{\"op\":\"gt\",\"field\":\"score\",\"value\":3.5}],\"ok\":true}",
      0u, 5u};
  unsigned char chunk[17];
  char output[1024];
  size_t out_len;
  size_t total;
  int eof;

  memset(&doc, 0, sizeof(doc));
  memset(&generator, 0, sizeof(generator));
  memset(output, 0, sizeof(output));
  strcpy(doc.id, "evt-1");
  lonejson_json_value_init(&doc.selector);
  lonejson_json_value_init(&doc.fields);
  lonejson_json_value_init(&doc.last_error);
  options.pretty = 1;

  EXPECT(lonejson_json_value_set_reader(&doc.selector, test_state_reader,
                                        &selector_reader,
                                        NULL) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_json_value_set_buffer(
             &doc.fields, "[\"id\",\"kind\",{\"nested\":[1,null,false]}]",
             strlen("[\"id\",\"kind\",{\"nested\":[1,null,false]}]"),
             NULL) == LONEJSON_STATUS_OK);

  EXPECT(lonejson_generator_init(&generator, &test_json_value_doc_map, &doc,
                                 &options) == LONEJSON_STATUS_OK);
  total = 0u;
  eof = 0;
  while (!eof) {
    EXPECT(lonejson_generator_read(&generator, chunk, sizeof(chunk), &out_len,
                                   &eof) == LONEJSON_STATUS_OK);
    EXPECT(total + out_len < sizeof(output));
    memcpy(output + total, chunk, out_len);
    total += out_len;
  }
  output[total] = '\0';
  EXPECT(strstr(output, "\n") != NULL);
  EXPECT(strstr(output, "\"selector\": {\n") != NULL);
  EXPECT(strstr(output, "\"filters\": [\n") != NULL);
  EXPECT(strstr(output, "\"fields\": [\n") != NULL);
  EXPECT(strstr(output, "\"nested\": [\n") != NULL);
  lonejson_generator_cleanup(&generator);
  lonejson_cleanup(&test_json_value_doc_map, &doc);
}

static void test_serialize_omits_empty_optional_fields(void) {
  static char *tags[] = {"request"};
  test_omit_doc doc;
  test_omit_doc parsed;
  lonejson_error error;
  lonejson_status status;
  test_buffer_sink sink;
  lonejson_generator generator;
  char buffer[1024];
  char sink_output[1024];
  char generated[1024];
  char *allocated;
  unsigned char chunk[3];
  size_t needed;
  size_t measured;
  size_t out_len;
  size_t total;
  int eof;
  test_reader_state metadata_reader = {"{\"one_shot\":true}", 0u, 4u};

  memset(&doc, 0, sizeof(doc));
  memset(&parsed, 0, sizeof(parsed));
  memset(&generator, 0, sizeof(generator));
  lonejson_error_init(&error);
  lonejson_source_init(&doc.payload);
  lonejson_json_value_init(&doc.metadata);
  doc.model = (char *)malloc(strlen("gpt-5-nano") + 1u);
  EXPECT(doc.model != NULL);
  if (doc.model == NULL) {
    return;
  }
  strcpy(doc.model, "gpt-5-nano");
  doc.max_output_tokens = 128;
  doc.seed = 42u;
  doc.temperature = 0.25;
  doc.stream = true;

  allocated =
      lonejson_serialize_alloc(&test_omit_doc_map, &doc, NULL, NULL, &error);
  EXPECT(allocated != NULL);
  if (allocated != NULL) {
    EXPECT(strcmp(allocated, "{\"model\":\"gpt-5-nano\"}") == 0);
    free(allocated);
  }

  status = lonejson_parse_cstr(
      &test_omit_doc_map, &parsed,
      "{\"model\":\"parsed\",\"max_output_tokens\":5,\"seed\":6,"
      "\"temperature\":1.5,\"stream\":false}",
      NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(parsed.max_output_tokens == 5);
  EXPECT(parsed.has_max_output_tokens != 0);
  EXPECT(parsed.seed == 6u);
  EXPECT(parsed.has_seed != 0);
  EXPECT(parsed.temperature == 1.5);
  EXPECT(parsed.has_temperature != 0);
  EXPECT(parsed.stream == false);
  EXPECT(parsed.has_stream != 0);
  allocated =
      lonejson_serialize_alloc(&test_omit_doc_map, &parsed, NULL, NULL, &error);
  EXPECT(allocated != NULL);
  if (allocated != NULL) {
    EXPECT(strcmp(allocated,
                  "{\"model\":\"parsed\",\"max_output_tokens\":5,\"seed\":6,"
                  "\"temperature\":1.5,\"stream\":false}") == 0);
    free(allocated);
  }
  lonejson_cleanup(&test_omit_doc_map, &parsed);

  doc.instructions = (char *)malloc(strlen("be terse") + 1u);
  doc.reasoning.effort = (char *)malloc(strlen("medium") + 1u);
  EXPECT(doc.instructions != NULL);
  EXPECT(doc.reasoning.effort != NULL);
  if (doc.instructions == NULL || doc.reasoning.effort == NULL) {
    free(doc.model);
    free(doc.instructions);
    free(doc.reasoning.effort);
    lonejson_source_cleanup(&doc.payload);
    lonejson_json_value_cleanup(&doc.metadata);
    return;
  }
  strcpy(doc.instructions, "be terse");
  strcpy(doc.reasoning.effort, "medium");
  doc.max_output_tokens = 128;
  doc.has_max_output_tokens = 1;
  doc.seed = 42u;
  doc.has_seed = 1;
  doc.temperature = 0.25;
  doc.has_temperature = 1;
  doc.stream = true;
  doc.has_stream = 1;
  doc.tags.items = tags;
  doc.tags.count = 1u;
  doc.tags.capacity = 1u;
  doc.tags.flags = LONEJSON_ARRAY_FIXED_CAPACITY;
  doc.tools.items = doc.tool_storage;
  doc.tools.count = 1u;
  doc.tools.capacity = 1u;
  doc.tools.elem_size = sizeof(doc.tool_storage[0]);
  doc.tools.flags = LONEJSON_ARRAY_FIXED_CAPACITY;
  doc.tool_storage[0].id = 7;
  strcpy(doc.tool_storage[0].label, "tool");
  EXPECT(lonejson_json_value_set_buffer(&doc.metadata, "{\"ok\":true}",
                                        strlen("{\"ok\":true}"),
                                        &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_source_is_rewindable(&doc.payload) != 0);
  EXPECT(lonejson_json_value_is_rewindable(&doc.metadata) != 0);

  status = lonejson_serialize_buffer(&test_omit_doc_map, &doc, buffer,
                                     sizeof(buffer), &needed, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer,
                "{\"model\":\"gpt-5-nano\",\"instructions\":\"be terse\","
                "\"max_output_tokens\":128,\"seed\":42,\"temperature\":0.25,"
                "\"stream\":true,\"reasoning\":{\"effort\":\"medium\"},"
                "\"tags\":[\"request\"],\"tools\":[{\"id\":7,"
                "\"label\":\"tool\"}],\"metadata\":{\"ok\":true}}") == 0);
  measured = 0u;
  status = lonejson_generator_measure(&test_omit_doc_map, &doc, &measured, NULL,
                                      &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(measured == strlen(buffer));

  memset(sink_output, 0, sizeof(sink_output));
  sink.buffer = (unsigned char *)sink_output;
  sink.capacity = sizeof(sink_output);
  sink.length = 0u;
  status = lonejson_serialize_sink(&test_omit_doc_map, &doc,
                                   test_buffer_sink_write, &sink, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(sink_output, buffer) == 0);

  EXPECT(lonejson_generator_init(&generator, &test_omit_doc_map, &doc, NULL) ==
         LONEJSON_STATUS_OK);
  memset(generated, 0, sizeof(generated));
  total = 0u;
  eof = 0;
  while (!eof) {
    EXPECT(lonejson_generator_read(&generator, chunk, sizeof(chunk), &out_len,
                                   &eof) == LONEJSON_STATUS_OK);
    EXPECT(total + out_len < sizeof(generated));
    memcpy(generated + total, chunk, out_len);
    total += out_len;
  }
  generated[total] = '\0';
  EXPECT(strcmp(generated, buffer) == 0);
  lonejson_generator_cleanup(&generator);

  lonejson_json_value_cleanup(&doc.metadata);
  lonejson_json_value_init(&doc.metadata);
  EXPECT(lonejson_json_value_set_reader(&doc.metadata, test_state_reader,
                                        &metadata_reader,
                                        &error) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_json_value_is_rewindable(&doc.metadata) == 0);
  status = lonejson_generator_measure(&test_omit_doc_map, &doc, &measured, NULL,
                                      &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
  EXPECT(strstr(error.message, "non-rewindable") != NULL);

  lonejson_json_value_cleanup(&doc.metadata);
  lonejson_source_cleanup(&doc.payload);
  free(doc.model);
  free(doc.instructions);
  free(doc.reasoning.effort);
}

static void test_serialize_omit_layout_failures(void) {
  test_omit_doc doc;
  lonejson_error error;
  const lonejson_map *maps[4];
  char buffer[128];
  lonejson_status status;
  size_t i;

  memset(&doc, 0, sizeof(doc));
  lonejson_source_init(&doc.payload);
  lonejson_json_value_init(&doc.metadata);
  doc.model = (char *)malloc(strlen("gpt-5-nano") + 1u);
  EXPECT(doc.model != NULL);
  if (doc.model == NULL) {
    return;
  }
  strcpy(doc.model, "gpt-5-nano");

  maps[0] = &test_bad_required_omit_map;
  maps[1] = &test_bad_required_presence_map;
  maps[2] = &test_bad_presence_kind_map;
  maps[3] = &test_bad_presence_offset_map;
  for (i = 0u; i < sizeof(maps) / sizeof(maps[0]); ++i) {
    status = lonejson_serialize_buffer(maps[i], &doc, buffer, sizeof(buffer),
                                       NULL, NULL, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
    EXPECT(strstr(error.message, "invalid map layout") != NULL);
  }

  lonejson_source_cleanup(&doc.payload);
  lonejson_json_value_cleanup(&doc.metadata);
  free(doc.model);
}

static void test_serialize_omit_pretty_and_recursive_invariants(void) {
  test_omit_doc doc;
  test_omit_deep_doc deep;
  lonejson_write_options options = lonejson_default_write_options();
  lonejson_error error;
  char buffer[1024];
  lonejson_status status;

  memset(&doc, 0, sizeof(doc));
  memset(&deep, 0, sizeof(deep));
  lonejson_source_init(&doc.payload);
  lonejson_json_value_init(&doc.metadata);
  options.pretty = 1;

  doc.model = (char *)malloc(strlen("gpt-5-nano") + 1u);
  deep.id = (char *)malloc(strlen("deep-1") + 1u);
  EXPECT(doc.model != NULL);
  EXPECT(deep.id != NULL);
  if (doc.model == NULL || deep.id == NULL) {
    free(doc.model);
    free(deep.id);
    lonejson_source_cleanup(&doc.payload);
    lonejson_json_value_cleanup(&doc.metadata);
    return;
  }
  strcpy(doc.model, "gpt-5-nano");
  strcpy(deep.id, "deep-1");

  status = lonejson_serialize_buffer(&test_pretty_omit_map, &doc, buffer,
                                     sizeof(buffer), NULL, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, "{\n  \"model\": \"gpt-5-nano\"\n}") == 0);

  doc.instructions = (char *)malloc(strlen("be terse") + 1u);
  EXPECT(doc.instructions != NULL);
  if (doc.instructions == NULL) {
    free(doc.model);
    free(deep.id);
    lonejson_source_cleanup(&doc.payload);
    lonejson_json_value_cleanup(&doc.metadata);
    return;
  }
  strcpy(doc.instructions, "be terse");
  doc.max_output_tokens = 4096;
  doc.has_max_output_tokens = 0;
  EXPECT(lonejson_json_value_set_buffer(&doc.metadata, "{\"ok\":true}",
                                        strlen("{\"ok\":true}"),
                                        &error) == LONEJSON_STATUS_OK);
  status = lonejson_serialize_buffer(&test_pretty_omit_map, &doc, buffer,
                                     sizeof(buffer), NULL, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, "{\n"
                        "  \"instructions\": \"be terse\",\n"
                        "  \"model\": \"gpt-5-nano\",\n"
                        "  \"metadata\": {\n"
                        "      \"ok\": true\n"
                        "    }\n"
                        "}") == 0);

  status = lonejson_serialize_buffer(&test_omit_deep_doc_map, &deep, buffer,
                                     sizeof(buffer), NULL, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, "{\"id\":\"deep-1\"}") == 0);

  deep.middle.leaf.value = (char *)malloc(strlen("set") + 1u);
  EXPECT(deep.middle.leaf.value != NULL);
  if (deep.middle.leaf.value != NULL) {
    strcpy(deep.middle.leaf.value, "set");
    status = lonejson_serialize_buffer(&test_omit_deep_doc_map, &deep, buffer,
                                       sizeof(buffer), NULL, NULL, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(strcmp(buffer, "{\"id\":\"deep-1\",\"middle\":{\"leaf\":{\"value\":"
                          "\"set\"}}}") == 0);
  }

  lonejson_source_cleanup(&doc.payload);
  lonejson_json_value_cleanup(&doc.metadata);
  free(doc.model);
  free(doc.instructions);
  free(deep.id);
  free(deep.middle.leaf.value);
}

static void test_serialize_omit_empty_implies_null(void) {
  test_omit_doc doc;
  lonejson_error error;
  char buffer[256];
  lonejson_status status;

  memset(&doc, 0, sizeof(doc));
  lonejson_source_init(&doc.payload);
  lonejson_json_value_init(&doc.metadata);
  doc.model = (char *)malloc(strlen("gpt-5-nano") + 1u);
  EXPECT(doc.model != NULL);
  if (doc.model == NULL) {
    lonejson_source_cleanup(&doc.payload);
    lonejson_json_value_cleanup(&doc.metadata);
    return;
  }
  strcpy(doc.model, "gpt-5-nano");

  status = lonejson_serialize_buffer(&test_omit_empty_null_map, &doc, buffer,
                                     sizeof(buffer), NULL, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, "{\"model\":\"gpt-5-nano\"}") == 0);

  lonejson_source_cleanup(&doc.payload);
  lonejson_json_value_cleanup(&doc.metadata);
  free(doc.model);
}

static void test_rewindability_edges_and_measure_preserves_sources(void) {
  test_source_doc source_doc;
  test_json_value_doc json_doc;
  lonejson_error error;
  lonejson_status status;
  char text_path[] = "/tmp/lonejson-rewind-source-text-XXXXXX";
  char bytes_path[] = "/tmp/lonejson-rewind-source-bytes-XXXXXX";
  char json_path[] = "/tmp/lonejson-rewind-json-XXXXXX";
  int text_fd = -1;
  int bytes_fd = -1;
  int json_fd = -1;
  int pipe_fds[2];
  FILE *text_fp = NULL;
  FILE *json_fp = NULL;
  size_t measured;
  char buffer[512];

  text_fd = write_temp_text_file(text_path, "seekable text");
  bytes_fd = write_temp_text_file(bytes_path, "abc");
  json_fd = write_temp_text_file(json_path, "{\"path\":true}");
  EXPECT(text_fd >= 0);
  EXPECT(bytes_fd >= 0);
  EXPECT(json_fd >= 0);
  if (text_fd < 0 || bytes_fd < 0 || json_fd < 0) {
    if (text_fd >= 0) {
      close(text_fd);
      unlink(text_path);
    }
    if (bytes_fd >= 0) {
      close(bytes_fd);
      unlink(bytes_path);
    }
    if (json_fd >= 0) {
      close(json_fd);
      unlink(json_path);
    }
    return;
  }
  close(text_fd);
  close(bytes_fd);
  close(json_fd);

  lonejson_init(&test_source_doc_map, &source_doc);
  strcpy(source_doc.id, "rewind-1");
  text_fp = fopen(text_path, "rb");
  bytes_fd = open(bytes_path, O_RDONLY);
  EXPECT(text_fp != NULL);
  EXPECT(bytes_fd >= 0);
  if (text_fp == NULL || bytes_fd < 0) {
    if (text_fp != NULL) {
      fclose(text_fp);
    }
    if (bytes_fd >= 0) {
      close(bytes_fd);
    }
    lonejson_cleanup(&test_source_doc_map, &source_doc);
    unlink(text_path);
    unlink(bytes_path);
    unlink(json_path);
    return;
  }
  EXPECT(lonejson_source_set_file(&source_doc.text, text_fp, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_source_set_fd(&source_doc.bytes, bytes_fd, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_source_is_rewindable(&source_doc.text) != 0);
  EXPECT(lonejson_source_is_rewindable(&source_doc.bytes) != 0);
  status = lonejson_generator_measure(&test_source_doc_map, &source_doc,
                                      &measured, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  status = lonejson_serialize_buffer(&test_source_doc_map, &source_doc, buffer,
                                     sizeof(buffer), NULL, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, "{\"id\":\"rewind-1\",\"text\":\"seekable text\","
                        "\"bytes\":\"YWJj\"}") == 0);
  EXPECT(measured == strlen(buffer));
  EXPECT(fseek(text_fp, 0L, SEEK_SET) == 0);
  EXPECT(lseek(bytes_fd, 0, SEEK_SET) == 0);
  lonejson_cleanup(&test_source_doc_map, &source_doc);
  fclose(text_fp);
  close(bytes_fd);

  lonejson_source_init(&source_doc.text);
  EXPECT(lonejson_source_set_path(&source_doc.text, text_path, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_source_is_rewindable(&source_doc.text) != 0);
  lonejson_source_cleanup(&source_doc.text);

  if (pipe(pipe_fds) == 0) {
    EXPECT(write(pipe_fds[1], "pipe", 4u) == 4);
    close(pipe_fds[1]);
    lonejson_init(&test_source_doc_map, &source_doc);
    strcpy(source_doc.id, "pipe-1");
    EXPECT(lonejson_source_set_fd(&source_doc.text, pipe_fds[0], &error) ==
           LONEJSON_STATUS_OK);
    EXPECT(lonejson_source_is_rewindable(&source_doc.text) == 0);
    status = lonejson_generator_measure(&test_source_doc_map, &source_doc,
                                        &measured, NULL, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
    lonejson_cleanup(&test_source_doc_map, &source_doc);
    close(pipe_fds[0]);
  } else {
    EXPECT(0);
  }

  poison_bytes(&json_doc, sizeof(json_doc), 0xC3u);
  lonejson__init_map(&test_json_value_doc_map, &json_doc);
  strcpy(json_doc.id, "json-1");
  EXPECT(lonejson_json_value_set_path(&json_doc.selector, json_path, &error) ==
         LONEJSON_STATUS_OK);
  EXPECT(lonejson_json_value_is_rewindable(&json_doc.selector) != 0);
  lonejson_cleanup(&test_json_value_doc_map, &json_doc);

  poison_bytes(&json_doc, sizeof(json_doc), 0xC4u);
  lonejson__init_map(&test_json_value_doc_map, &json_doc);
  strcpy(json_doc.id, "json-2");
  json_fp = fopen(json_path, "rb");
  json_fd = open(json_path, O_RDONLY);
  EXPECT(json_fp != NULL);
  EXPECT(json_fd >= 0);
  if (json_fp != NULL && json_fd >= 0) {
    EXPECT(lonejson_json_value_set_file(&json_doc.selector, json_fp, &error) ==
           LONEJSON_STATUS_OK);
    EXPECT(lonejson_json_value_set_fd(&json_doc.fields, json_fd, &error) ==
           LONEJSON_STATUS_OK);
    EXPECT(lonejson_json_value_is_rewindable(&json_doc.selector) != 0);
    EXPECT(lonejson_json_value_is_rewindable(&json_doc.fields) != 0);
    status = lonejson_generator_measure(&test_json_value_doc_map, &json_doc,
                                        &measured, NULL, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
  }
  lonejson_cleanup(&test_json_value_doc_map, &json_doc);
  if (json_fp != NULL) {
    fclose(json_fp);
  }
  if (json_fd >= 0) {
    close(json_fd);
  }

  if (pipe(pipe_fds) == 0) {
    EXPECT(write(pipe_fds[1], "{\"pipe\":true}", strlen("{\"pipe\":true}")) ==
           (ssize_t)strlen("{\"pipe\":true}"));
    close(pipe_fds[1]);
    poison_bytes(&json_doc, sizeof(json_doc), 0xC5u);
    lonejson__init_map(&test_json_value_doc_map, &json_doc);
    strcpy(json_doc.id, "json-3");
    EXPECT(lonejson_json_value_set_fd(&json_doc.selector, pipe_fds[0],
                                      &error) == LONEJSON_STATUS_OK);
    EXPECT(lonejson_json_value_is_rewindable(&json_doc.selector) == 0);
    status = lonejson_generator_measure(&test_json_value_doc_map, &json_doc,
                                        &measured, NULL, &error);
    EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);
    lonejson_cleanup(&test_json_value_doc_map, &json_doc);
    close(pipe_fds[0]);
  } else {
    EXPECT(0);
  }

  unlink(text_path);
  unlink(bytes_path);
  unlink(json_path);
}

#ifdef LONEJSON_WITH_CURL
static int test_child_curl_upload_cleanup_default_allocator(void) {
  test_event event;
  lonejson_curl_upload upload;

  memset(&event, 0, sizeof(event));
  strcpy(event.id, "evt-1");
  event.ok = true;

  if (lonejson_curl_upload_init(&upload, &test_event_map, &event, NULL) !=
      LONEJSON_STATUS_OK) {
    return 2;
  }
  lonejson_curl_upload_cleanup(&upload);
  return 0;
}

static void test_curl_upload_cleanup_default_allocator(void) {
  pid_t pid;
  int status;

  pid = fork();
  EXPECT(pid >= 0);
  if (pid == 0) {
    _exit(test_child_curl_upload_cleanup_default_allocator());
  }
  if (pid < 0) {
    return;
  }
  EXPECT(waitpid(pid, &status, 0) == pid);
  EXPECT(WIFEXITED(status));
  if (WIFEXITED(status)) {
    EXPECT(WEXITSTATUS(status) == 0);
  }
}

static void test_curl_upload_custom_allocator_balance(void) {
  test_event event;
  test_allocator_state write_alloc;
  lonejson_write_options options = lonejson_default_write_options();
  lonejson_curl_upload upload;
  char buffer[128];
  size_t total;

  memset(&event, 0, sizeof(event));
  strcpy(event.id, "evt-1");
  event.ok = true;
  test_allocator_init(&write_alloc);
  options.allocator = &write_alloc.allocator;

  EXPECT(lonejson_curl_upload_init(&upload, &test_event_map, &event,
                                   &options) == LONEJSON_STATUS_OK);
  EXPECT(write_alloc.stats.alloc_calls > 0u);
  EXPECT(write_alloc.stats.bytes_live > 0u);
  total = lonejson_curl_read_callback(buffer, 1u, sizeof(buffer) - 1u, &upload);
  EXPECT(total != 0u);
  EXPECT(total != CURL_READFUNC_ABORT);
  buffer[total] = '\0';
  EXPECT(strstr(buffer, "\"id\":\"evt-1\"") != NULL);
  lonejson_curl_upload_cleanup(&upload);
  EXPECT(write_alloc.stats.bytes_live == 0u);
  EXPECT(write_alloc.stats.free_calls > 0u);
}

static void test_curl_upload_streaming_does_not_buffer_payload(void) {
  test_source_doc doc;
  test_allocator_state write_alloc;
  lonejson_write_options options = lonejson_default_write_options();
  lonejson_curl_upload upload;
  char path[] = "/tmp/lonejson-curl-upload-XXXXXX";
  char chunk[4096];
  int fd;
  FILE *fp;
  size_t total;
  size_t i;

  memset(&doc, 0, sizeof(doc));
  strcpy(doc.id, "evt-1");
  lonejson_source_init(&doc.text);
  lonejson_source_init(&doc.bytes);
  test_allocator_init(&write_alloc);
  options.allocator = &write_alloc.allocator;

  fd = mkstemp(path);
  EXPECT(fd >= 0);
  if (fd < 0) {
    return;
  }
  fp = fdopen(fd, "wb");
  EXPECT(fp != NULL);
  if (fp == NULL) {
    close(fd);
    unlink(path);
    return;
  }
  for (i = 0u; i < 262144u; ++i) {
    fputc('a', fp);
  }
  fclose(fp);

  EXPECT(lonejson_source_set_path(&doc.text, path, NULL) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_curl_upload_init(&upload, &test_source_doc_map, &doc,
                                   &options) == LONEJSON_STATUS_OK);
  EXPECT(lonejson_curl_upload_size(&upload) == (curl_off_t)-1);
  EXPECT(write_alloc.stats.peak_bytes_live < 65536u);

  total = 0u;
  for (;;) {
    size_t got = lonejson_curl_read_callback(chunk, 1u, sizeof(chunk), &upload);
    if (got == 0u) {
      break;
    }
    EXPECT(got != CURL_READFUNC_ABORT);
    total += got;
  }
  EXPECT(upload.generator.error.code == LONEJSON_STATUS_OK ||
         upload.generator.error.code == LONEJSON_STATUS_TRUNCATED);
  EXPECT(total > 262144u);
  EXPECT(write_alloc.stats.peak_bytes_live < 65536u);
  lonejson_curl_upload_cleanup(&upload);
  EXPECT(write_alloc.stats.bytes_live == 0u);
  lonejson_cleanup(&test_source_doc_map, &doc);
  unlink(path);
}
#endif

static int test_child_parse_poisoned_stream_and_json_value_fields(void) {
  test_alloc_parse_doc stream_doc;
  test_alloc_json_value_doc json_doc;

  memset(&stream_doc, 0xA5, sizeof(stream_doc));
  stream_doc.body._lonejson_magic = LONEJSON__SPOOLED_MAGIC;
  stream_doc.body.memory = (unsigned char *)1;
  stream_doc.body.spill_fp = (FILE *)1;
  lonejson__init_map(&test_alloc_parse_doc_map, &stream_doc);
  lonejson_cleanup(&test_alloc_parse_doc_map, &stream_doc);

  memset(&json_doc, 0xA5, sizeof(json_doc));
  json_doc.selector._lonejson_magic = LONEJSON__JSON_VALUE_MAGIC;
  json_doc.selector.json = (char *)1;
  json_doc.selector.path = (char *)1;
  lonejson__init_map(&test_alloc_json_value_doc_map, &json_doc);
  lonejson_cleanup(&test_alloc_json_value_doc_map, &json_doc);
  return 0;
}

static void
test_clear_destination_ignores_poisoned_stream_and_json_value_state(void) {
  pid_t pid;
  int status;

  pid = fork();
  EXPECT(pid >= 0);
  if (pid == 0) {
    _exit(test_child_parse_poisoned_stream_and_json_value_fields());
  }
  if (pid < 0) {
    return;
  }
  EXPECT(waitpid(pid, &status, 0) == pid);
  EXPECT(WIFEXITED(status));
  EXPECT(WEXITSTATUS(status) == 0);
}

static void test_prepared_fixed_object_array_parse_preserves_storage(void) {
  static const char json[] =
      "{\"results\":["
      "{\"name\":\"parse/buffer_fixed/lonejson\",\"group\":\"parse\"},"
      "{\"name\":\"serialize/sink/lonejson\",\"group\":\"serialize\"}]}";
  test_fixed_result_run run;
  lonejson_parse_options options;
  lonejson_error error;
  lonejson_status status;

  memset(&run, 0, sizeof(run));
  run.results.items = run.result_storage;
  run.results.capacity =
      sizeof(run.result_storage) / sizeof(run.result_storage[0]);
  run.results.elem_size = sizeof(run.result_storage[0]);
  run.results.flags = LONEJSON_ARRAY_FIXED_CAPACITY;

  options = lonejson_default_parse_options();
  options.clear_destination = 0;
  status = lonejson_parse_cstr(&test_fixed_result_run_map, &run, json, &options,
                               &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(run.results.items == run.result_storage);
  EXPECT(run.results.count == 2u);
  EXPECT(strcmp(run.result_storage[0].name, "parse/buffer_fixed/lonejson") ==
         0);
  EXPECT(strcmp(run.result_storage[0].group, "parse") == 0);
  EXPECT(strcmp(run.result_storage[1].name, "serialize/sink/lonejson") == 0);
  EXPECT(strcmp(run.result_storage[1].group, "serialize") == 0);
  lonejson_cleanup(&test_fixed_result_run_map, &run);
}

int main(void) {
  test_parse_implicit_destination_reset();
  test_dynamic_allocation_cleanup_balance();
  test_dynamic_allocation_reset_reparse_balance();
  test_zero_alloc_validate_path();
  test_parser_workspace_accounting();
  test_parser_workspace_alignment_regression();
  test_init_does_not_preserve_garbage_array_state();
  test_zero_alloc_fixed_storage_parse();
  test_zero_alloc_fixed_storage_serialize();
  test_zero_alloc_jsonl_serialize();
  test_serialize_alloc_balance();
  test_serialize_alloc_default_free_compatibility();
  test_serialize_jsonl_alloc_balance();
  test_serialize_jsonl_alloc_default_free_compatibility();
  test_parse_buffer_basic();
  test_parse_fixed_strings_with_escapes_and_truncation();
  test_chunked_parser_and_missing_required();
  test_object_framed_stream_jsonl();
  test_stream_reuses_and_changes_destination();
  test_file_and_buffer_helpers();
  test_jsonl_helpers();
  test_fixed_capacity_object_array();
  test_dynamic_object_array_alignment();
  test_formatting_variants_and_roundtrip();
  test_duplicate_key_policy();
  test_public_api_argument_and_serialization_guards();
  test_public_initializers_and_defaults();
  test_small_valid_spec_fixtures();
  test_invalid_json_vectors();
  test_fixture_documents();
  test_large_generated_fixture_sizes();
  test_large_generated_fixture_parsing();
  test_json_test_suite_parsing();
  test_spooled_fields_roundtrip();
  test_spooled_fields_small_and_null();
  test_spooled_append_api();
  test_spooled_field_failures();
  test_source_fields_path_and_raw_sink();
  test_source_fields_file_and_fd();
  test_source_field_parse_and_seek_failures();
  test_source_fields_do_not_mutate_sink_on_open_failure();
  test_json_value_parse_and_roundtrip();
  test_json_value_parse_stream_sink();
  test_json_value_parse_visitor();
  test_json_value_parse_visitor_fast_path_regressions();
  test_json_value_setters_and_failures();
  test_json_value_scalars_null_and_reset();
  test_json_value_reuse_and_cleanup_ownership();
  test_json_value_source_validation_failures();
  test_json_value_nonseekable_and_sink_failures();
  test_json_value_large_source_backed_serialization();
  test_json_value_parse_requires_destination_and_partial_failure();
  test_json_value_nested_failure_matrix();
  test_value_visitor_success_and_limits();
  test_value_visitor_chunking_unicode_and_failures();
  test_custom_allocator_parse_cleanup_and_stream();
  test_custom_allocator_json_value_capture_and_serialize_alloc();
  test_custom_allocator_raw_serialize_alloc_is_rejected();
  test_explicit_default_allocator_raw_serialize_alloc_is_allowed();
  test_explicit_default_allocator_raw_serialize_jsonl_alloc_is_allowed();
  test_custom_allocator_json_value_capture_preserves_handle_allocator();
  test_partial_allocator_parse_is_rejected();
  test_partial_allocator_parse_reader_is_rejected();
  test_partial_allocator_serialize_alloc_is_rejected();
  test_partial_allocator_generator_init_is_rejected();
  test_generator_compact_streams_event_map();
  test_generator_pretty_streams_source_field();
  test_generator_streams_json_value_fields();
  test_generator_pretty_streams_json_value_reader();
  test_serialize_omits_empty_optional_fields();
  test_serialize_omit_layout_failures();
  test_serialize_omit_pretty_and_recursive_invariants();
  test_serialize_omit_empty_implies_null();
  test_rewindability_edges_and_measure_preserves_sources();
#ifdef LONEJSON_WITH_CURL
  test_curl_upload_cleanup_default_allocator();
  test_curl_upload_custom_allocator_balance();
  test_curl_upload_streaming_does_not_buffer_payload();
#endif
  test_clear_destination_ignores_poisoned_stream_and_json_value_state();
  test_prepared_fixed_object_array_parse_preserves_storage();

  if (g_failures != 0) {
    fprintf(stderr, "test failures: %d\n", g_failures);
    return 1;
  }
  return 0;
}
