#define _POSIX_C_SOURCE 200809L
#include "lonejson.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(expr)                                                            \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "line %d: %s\n", __LINE__, #expr);                       \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

typedef struct payload {
  char text[16];
} payload;
static const lonejson_field fields[] = {LONEJSON_FIELD_STRING_FIXED_REQ(
    payload, text, "payload", LONEJSON_OVERFLOW_FAIL)};
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

/* The fixture consumes the complete body before redirecting, forcing replay. */
static void serve(int listener, const char *method, int redirect,
                  int one_shot) {
  int pass;
  for (pass = 0; pass < (one_shot ? 1 : 2); ++pass) {
    int fd = accept(listener, NULL, NULL);
    FILE *input;
    char line[512];
    char expected_line[128];
    char body[128];
    char response[512];
    size_t length = 0u;
    size_t content_length = 0u;
    int chunked = 0;
    int n;
    CHECK(fd >= 0);
    input = fdopen(fd, "r");
    CHECK(input != NULL);
    CHECK(fgets(line, sizeof(line), input) != NULL);
    snprintf(expected_line, sizeof(expected_line), "%s /%s HTTP/1.1\r\n",
             method, pass == 0 ? "start" : "received");
    CHECK(strcmp(line, expected_line) == 0);
    for (;;) {
      CHECK(fgets(line, sizeof(line), input) != NULL);
      if (strcmp(line, "\r\n") == 0)
        break;
      if (strncasecmp(line, "Transfer-Encoding: chunked", 26u) == 0)
        chunked = 1;
      if (strncasecmp(line, "Content-Length:", 15u) == 0)
        content_length = (size_t)strtoul(line + 15, NULL, 10);
    }
    if (chunked) {
      for (;;) {
        size_t chunk;
        CHECK(fgets(line, sizeof(line), input) != NULL);
        chunk = (size_t)strtoul(line, NULL, 16);
        if (chunk == 0u) {
          CHECK(fgets(line, sizeof(line), input) != NULL);
          CHECK(strcmp(line, "\r\n") == 0);
          break;
        }
        CHECK(chunk < sizeof(body) - length);
        CHECK(fread(body + length, 1u, chunk, input) == chunk);
        length += chunk;
        CHECK(fgets(line, sizeof(line), input) != NULL);
        CHECK(strcmp(line, "\r\n") == 0);
      }
    } else {
      CHECK(content_length < sizeof(body));
      CHECK(fread(body, 1u, content_length, input) == content_length);
      length = content_length;
    }
    body[length] = '\0';
    CHECK(strcmp(body, "{\"payload\":\"abc\"}") == 0);
    if (pass == 0)
      n = snprintf(
          response, sizeof(response),
          "HTTP/1.1 %d Redirect\r\nLocation: /received\r\nContent-Length: "
          "0\r\nConnection: close\r\n\r\n",
          redirect);
    else
      n = snprintf(
          response, sizeof(response),
          "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    CHECK(n > 0 && (size_t)n < sizeof(response));
    CHECK(write(fd, response, (size_t)n) == n);
    CHECK(fclose(input) == 0);
  }
  close(listener);
  _exit(0);
}

static void run_case(const char *method, int redirect, int kind) {
  struct sockaddr_in address;
  socklen_t address_len = sizeof(address);
  int listener;
  pid_t child;
  int status;
  char url[128];
  CURL *curl;
  CURLcode result;
  long response_code = 0;
  struct curl_slist *headers = NULL;
  lonejson *runtime;
  lonejson_curl_upload upload = {0};
  payload fixed = {"abc"};
  source_payload source;
  reader_payload reader;
  lonejson_buffer_reader input;
  const lonejson_map *map = &payload_map;
  const void *value = &fixed;
  FILE *file = NULL;

  listener = socket(AF_INET, SOCK_STREAM, 0);
  CHECK(listener >= 0);
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  CHECK(bind(listener, (struct sockaddr *)&address, sizeof(address)) == 0);
  CHECK(getsockname(listener, (struct sockaddr *)&address, &address_len) == 0);
  CHECK(listen(listener, 2) == 0);
  child = fork();
  CHECK(child >= 0);
  if (child == 0)
    serve(listener, method, redirect, kind == 2);
  close(listener);
  snprintf(url, sizeof(url), "http://127.0.0.1:%u/start",
           (unsigned)ntohs(address.sin_port));
  runtime = lonejson_new(NULL, NULL);
  CHECK(runtime != NULL);
  lonejson_source_init(&source.text);
  lonejson_json_value_init(NULL, &reader.text);
  if (kind == 1) {
    file = tmpfile();
    CHECK(file != NULL);
    CHECK(fwrite("abc", 1u, 3u, file) == 3u);
    CHECK(fflush(file) == 0);
    CHECK(lonejson_source_set_file(&source.text, file, NULL) ==
          LONEJSON_STATUS_OK);
    map = &source_map;
    value = &source;
  } else if (kind == 2) {
    lonejson_buffer_reader_init(&input, "\"abc\"", 5u);
    CHECK(lonejson_json_value_set_reader(&reader.text,
                                         lonejson_buffer_reader_read, &input,
                                         NULL) == LONEJSON_STATUS_OK);
    map = &reader_map;
    value = &reader;
  }
  CHECK(lonejson_curl_upload_init(&upload, runtime, map, value) ==
        LONEJSON_STATUS_OK);
  curl = curl_easy_init();
  CHECK(curl != NULL);
  headers = curl_slist_append(headers, "Expect:");
  CHECK(headers != NULL);
#define SET(option, val) CHECK(curl_easy_setopt(curl, option, val) == CURLE_OK)
  SET(CURLOPT_URL, url);
  SET(CURLOPT_PROXY, "");
  SET(CURLOPT_FOLLOWLOCATION, 1L);
  SET(CURLOPT_MAXREDIRS, 2L);
  SET(CURLOPT_TIMEOUT, 10L);
  SET(CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
  SET(CURLOPT_HTTPHEADER, headers);
  if (strcmp(method, "POST") == 0) {
    SET(CURLOPT_POST, 1L);
    SET(CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)-1);
  } else {
    SET(CURLOPT_UPLOAD, 1L);
    SET(CURLOPT_CUSTOMREQUEST, method);
    SET(CURLOPT_INFILESIZE_LARGE, (curl_off_t)-1);
  }
  SET(CURLOPT_READFUNCTION, lonejson_curl_read_callback);
  SET(CURLOPT_READDATA, &upload);
  SET(CURLOPT_SEEKFUNCTION, lonejson_curl_seek_callback);
  SET(CURLOPT_SEEKDATA, &upload);
#undef SET
  result = curl_easy_perform(curl);
  if (kind == 2)
    CHECK(result == CURLE_SEND_FAIL_REWIND);
  else {
    CHECK(result == CURLE_OK);
    CHECK(curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code) ==
          CURLE_OK);
    CHECK(response_code == 200L);
  }
  curl_easy_cleanup(curl);
  curl_slist_free_all(headers);
  upload.cleanup(&upload);
  lonejson_source_cleanup(&source.text);
  lonejson_json_value_cleanup(&reader.text);
  if (file != NULL)
    fclose(file);
  lonejson_free(runtime);
  CHECK(waitpid(child, &status, 0) == child);
  CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

int main(void) {
  const char *methods[] = {"POST", "PUT", "PATCH"};
  size_t method;
  int redirect;
  int kind;
  CHECK(curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK);
  for (method = 0u; method < 3u; ++method)
    for (redirect = 307; redirect <= 308; ++redirect)
      for (kind = 0; kind < 3; ++kind)
        run_case(methods[method], redirect, kind);
  curl_global_cleanup();
  puts("curl upload replay: 18 cases passed");
  return 0;
}
