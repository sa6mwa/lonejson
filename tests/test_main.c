#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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
#define LONEJSON_IMPLEMENTATION
#include "lonejson.h"

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

  poison_bytes(&log, sizeof(log), 0x44u);
  memset(codes_storage, 0, sizeof(codes_storage));
  log.codes.items = codes_storage;
  log.codes.capacity = 4u;
  log.codes.flags = LONEJSON_ARRAY_FIXED_CAPACITY;

  reset_lonejson_alloc_stats();
  lonejson__parser_init_state(&parser_storage, &test_fixed_log_map, &log, NULL,
                              0, parser_workspace, sizeof(parser_workspace));
  lonejson__init_map(&test_fixed_log_map, &log);
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

static void test_zero_alloc_fixed_storage_parse(void) {
  const char *json = "{\"name\":\"long-event-name\",\"codes\":[5,8,13]}";
  test_fixed_log log;
  lonejson_int64 codes_storage[4];
  lonejson_error error;
  lonejson_status status;

  poison_bytes(&log, sizeof(log), 0x33u);
  memset(codes_storage, 0, sizeof(codes_storage));
  log.codes.items = codes_storage;
  log.codes.capacity = 4u;
  log.codes.flags = LONEJSON_ARRAY_FIXED_CAPACITY;

  reset_lonejson_alloc_stats();
  status = lonejson_parse_cstr(&test_fixed_log_map, &log, json, NULL, &error);
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
    free(jsonl);
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

  memset(items_buf, 0, sizeof(items_buf));
  person.items.items = items_buf;
  person.items.capacity = 2;
  person.items.elem_size = sizeof(items_buf[0]);
  person.items.flags = LONEJSON_ARRAY_FIXED_CAPACITY;

  status = lonejson_parse_cstr(&test_person_map, &person, json, NULL, &error);
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
    free(serialized);
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

  memset(&person, 0, sizeof(person));
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
    free(serialized);
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

  memset(&doc, 0, sizeof(doc));
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

  memset(&doc, 0, sizeof(doc));
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

int main(void) {
  test_parse_implicit_destination_reset();
  test_dynamic_allocation_cleanup_balance();
  test_dynamic_allocation_reset_reparse_balance();
  test_zero_alloc_validate_path();
  test_parser_workspace_accounting();
  test_zero_alloc_fixed_storage_parse();
  test_zero_alloc_fixed_storage_serialize();
  test_zero_alloc_jsonl_serialize();
  test_serialize_alloc_balance();
  test_serialize_jsonl_alloc_balance();
  test_parse_buffer_basic();
  test_chunked_parser_and_missing_required();
  test_object_framed_stream_jsonl();
  test_stream_reuses_and_changes_destination();
  test_file_and_buffer_helpers();
  test_jsonl_helpers();
  test_fixed_capacity_object_array();
  test_formatting_variants_and_roundtrip();
  test_duplicate_key_policy();
  test_public_api_argument_and_serialization_guards();
  test_small_valid_spec_fixtures();
  test_invalid_json_vectors();
  test_fixture_documents();
  test_large_generated_fixture_sizes();
  test_large_generated_fixture_parsing();
  test_json_test_suite_parsing();
  test_spooled_fields_roundtrip();
  test_spooled_fields_small_and_null();
  test_spooled_field_failures();
  test_source_fields_path_and_raw_sink();
  test_source_fields_file_and_fd();
  test_source_field_parse_and_seek_failures();
  test_source_fields_do_not_mutate_sink_on_open_failure();

  if (g_failures != 0) {
    fprintf(stderr, "test failures: %d\n", g_failures);
    return 1;
  }
  return 0;
}
