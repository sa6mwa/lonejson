#define _POSIX_C_SOURCE 200809L
#include "lonejson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define CHECK(expr)                                                            \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "line %d: %s\n", __LINE__, #expr);                       \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

typedef struct payload {
  char *text;
} payload;
static const lonejson_field fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(payload, text, "payload")};
LONEJSON_MAP_DEFINE(payload_map, payload, fields);
typedef struct source_payload {
  lonejson_source text;
} source_payload;
static const lonejson_field source_fields[] = {
    LONEJSON_FIELD_STRING_SOURCE(source_payload, text, "payload")};
LONEJSON_MAP_DEFINE(source_map, source_payload, source_fields);
typedef struct reader_payload {
  lonejson_json_value text;
} reader_payload;
static const lonejson_field reader_fields[] = {
    LONEJSON_FIELD_JSON_VALUE_REQ(reader_payload, text, "payload")};
LONEJSON_MAP_DEFINE(reader_map, reader_payload, reader_fields);

typedef struct receipt {
  unsigned verified;
  char counts[32];
  size_t length;
} receipt;

static size_t check_header(char *data, size_t size, size_t count, void *user) {
  receipt *result = (receipt *)user;
  const char prefix[] = "X-Lonejson-Rewind-Verified: ";
  size_t length = size * count;
  if (length >= sizeof(prefix) - 1u &&
      memcmp(data, prefix, sizeof(prefix) - 1u) == 0) {
    CHECK(length == sizeof(prefix) - 1u + 3u);
    CHECK(data[sizeof(prefix) - 1u] == (char)('0' + result->verified));
    ++result->verified;
  }
  return length;
}

static size_t read_counts(char *data, size_t size, size_t count, void *user) {
  receipt *result = (receipt *)user;
  size_t length = size * count;
  CHECK(length < sizeof(result->counts) - result->length);
  memcpy(result->counts + result->length, data, length);
  result->length += length;
  result->counts[result->length] = '\0';
  return length;
}

static void run_case(const char *base, const char *ca, const char *method,
                     int redirect, int kind, size_t length, int known,
                     unsigned serial) {
  char url[1024];
  char case_id[96];
  CURL *curl;
  CURLcode result;
  long response_code = 0;
  struct curl_slist *headers;
  lonejson *runtime;
  lonejson_curl_upload upload = {0};
  payload fixed;
  source_payload source;
  reader_payload reader;
  lonejson_buffer_reader input;
  const lonejson_map *map = &payload_map;
  const void *value = &fixed;
  FILE *file = NULL;
  char *text;
  char *json;
  receipt received;
  curl_off_t body_size;
  int n;

  memset(&received, 0, sizeof(received));
  n = snprintf(case_id, sizeof(case_id), "%lu-%lu-%u", (unsigned long)getpid(),
               (unsigned long)time(NULL), serial);
  CHECK(n > 0 && (size_t)n < sizeof(case_id));
  n = snprintf(url, sizeof(url), "%s/rewind/%s/%s/%d/%lu/%s/0", base, case_id,
               method, redirect, (unsigned long)length,
               known ? "known" : "chunked");
  CHECK(n > 0 && (size_t)n < sizeof(url));
  fprintf(stderr, "curl rewind e2e: %s %d source=%d bytes=%lu %s\n", method,
          redirect, kind, (unsigned long)length, known ? "known" : "chunked");
  text = (char *)malloc(length + 1u);
  json = (char *)malloc(length + 3u);
  CHECK(text != NULL && json != NULL);
  memset(text, 'a', length);
  text[length] = '\0';
  json[0] = '"';
  memcpy(json + 1u, text, length);
  json[length + 1u] = '"';
  json[length + 2u] = '\0';
  fixed.text = text;
  runtime = lonejson_new(NULL, NULL);
  CHECK(runtime != NULL);
  lonejson_source_init(&source.text);
  lonejson_json_value_init(NULL, &reader.text);
  if (kind == 1) {
    file = tmpfile();
    CHECK(file != NULL);
    CHECK(fwrite(text, 1u, length, file) == length);
    CHECK(fflush(file) == 0);
    CHECK(lonejson_source_set_file(&source.text, file, NULL) ==
          LONEJSON_STATUS_OK);
    map = &source_map;
    value = &source;
  } else if (kind == 2) {
    lonejson_buffer_reader_init(&input, json, length + 2u);
    CHECK(lonejson_json_value_set_reader(&reader.text,
                                         lonejson_buffer_reader_read, &input,
                                         NULL) == LONEJSON_STATUS_OK);
    map = &reader_map;
    value = &reader;
  }
  CHECK(lonejson_curl_upload_init(&upload, runtime, map, value) ==
        LONEJSON_STATUS_OK);
  CHECK(upload.is_rewindable(&upload) == (kind != 2));
  CHECK(upload.size_fn(&upload) == (curl_off_t)-1);
  /* Test-owned deterministic length; capability never depends on measuring. */
  body_size = known ? (curl_off_t)(length + strlen("{\"payload\":\"\"}"))
                    : (curl_off_t)-1;
  curl = curl_easy_init();
  CHECK(curl != NULL);
  headers = curl_slist_append(NULL, "Expect:");
  CHECK(headers != NULL);
#define SET(option, val) CHECK(curl_easy_setopt(curl, option, val) == CURLE_OK)
  SET(CURLOPT_URL, url);
  SET(CURLOPT_PROXY, "");
  SET(CURLOPT_CAINFO, ca);
  SET(CURLOPT_SSL_VERIFYPEER, 1L);
  SET(CURLOPT_SSL_VERIFYHOST, 2L);
  SET(CURLOPT_FOLLOWLOCATION, 1L);
  SET(CURLOPT_MAXREDIRS, 3L);
  SET(CURLOPT_TIMEOUT, 20L);
  SET(CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
  SET(CURLOPT_HTTPHEADER, headers);
  SET(CURLOPT_HEADERFUNCTION, check_header);
  SET(CURLOPT_HEADERDATA, &received);
  if (strcmp(method, "POST") == 0) {
    SET(CURLOPT_POST, 1L);
    SET(CURLOPT_POSTFIELDSIZE_LARGE, body_size);
  } else {
    SET(CURLOPT_UPLOAD, 1L);
    SET(CURLOPT_CUSTOMREQUEST, method);
    SET(CURLOPT_INFILESIZE_LARGE, body_size);
  }
  SET(CURLOPT_READFUNCTION, lonejson_curl_read_callback);
  SET(CURLOPT_READDATA, &upload);
  SET(CURLOPT_SEEKFUNCTION, lonejson_curl_seek_callback);
  SET(CURLOPT_SEEKDATA, &upload);
  result = curl_easy_perform(curl);
  if (result != (kind == 2 ? CURLE_SEND_FAIL_REWIND : CURLE_OK))
    fprintf(stderr, "curl error: %s; lonejson error: %s\n",
            curl_easy_strerror(result), upload.generator.error.message);
  CHECK(result == (kind == 2 ? CURLE_SEND_FAIL_REWIND : CURLE_OK));
  CHECK(received.verified == (kind == 2 ? 1u : 3u));
  CHECK(curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code) ==
        CURLE_OK);
  CHECK(response_code == (kind == 2 ? redirect : 200L));
  curl_easy_cleanup(curl);
  curl_slist_free_all(headers);
  upload.cleanup(&upload);
  lonejson_source_cleanup(&source.text);
  lonejson_json_value_cleanup(&reader.text);
  if (file != NULL)
    CHECK(fclose(file) == 0);
  lonejson_free(runtime);
  free(text);
  free(json);

  /* Independently verify the fixture saw no extra requests or corrupt bodies.
   */
  n = snprintf(url, sizeof(url), "%s/rewind-result/%s", base, case_id);
  CHECK(n > 0 && (size_t)n < sizeof(url));
  curl = curl_easy_init();
  CHECK(curl != NULL);
  SET(CURLOPT_URL, url);
  SET(CURLOPT_PROXY, "");
  SET(CURLOPT_CAINFO, ca);
  SET(CURLOPT_TIMEOUT, 20L);
  SET(CURLOPT_WRITEFUNCTION, read_counts);
  SET(CURLOPT_WRITEDATA, &received);
  CHECK(curl_easy_perform(curl) == CURLE_OK);
  CHECK(curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code) ==
        CURLE_OK);
  CHECK(response_code == 200L);
  CHECK(strcmp(received.counts, kind == 2 ? "1 1" : "3 3") == 0);
  curl_easy_cleanup(curl);
#undef SET
}

int main(int argc, char **argv) {
  const char *methods[] = {"POST", "PUT", "PATCH"};
  size_t sizes[] = {3u, 262144u};
  size_t method;
  size_t size;
  int redirect;
  int kind;
  int known;
  unsigned serial = 0u;
  if (argc != 3 || strncmp(argv[1], "https://", 8u) != 0) {
    fprintf(stderr, "usage: curl_rewind_e2e HTTPS_BASE_URL CA_FILE\n");
    return 2;
  }
  CHECK(curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK);
  for (method = 0u; method < 3u; ++method)
    for (redirect = 307; redirect <= 308; ++redirect)
      for (kind = 0; kind < 3; ++kind)
        for (size = 0u; size < 2u; ++size)
          for (known = 0; known < 2; ++known)
            run_case(argv[1], argv[2], methods[method], redirect, kind,
                     sizes[size], known, ++serial);
  curl_global_cleanup();
  printf("curl HTTPS rewind e2e: %u cases passed\n", serial);
  return 0;
}
