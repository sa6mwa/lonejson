#include <stdio.h>
#include <stdlib.h>

#include "lonejson.h"

typedef struct upload_payload {
  char *message;
  lonejson_int64 id;
} upload_payload;

static const char *find_test_ca_path(void) {
  const char *configured_path = getenv("LONEJSON_CURL_E2E_CAINFO");
  static const char *const candidates[] = {
      "devenv/volumes/nginx/certs/server.crt",
      "../devenv/volumes/nginx/certs/server.crt",
      "../../devenv/volumes/nginx/certs/server.crt"};
  FILE *fp;
  size_t i;

  if (configured_path != NULL && configured_path[0] != '\0') {
    fp = fopen(configured_path, "rb");
    if (fp != NULL) {
      fclose(fp);
      return configured_path;
    }
  }
  for (i = 0u; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
    fp = fopen(candidates[i], "rb");
    if (fp != NULL) {
      fclose(fp);
      return candidates[i];
    }
  }
  return NULL;
}

static const lonejson_field upload_payload_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC_REQ(upload_payload, message, "message"),
    LONEJSON_FIELD_I64_REQ(upload_payload, id, "id")};
LONEJSON_MAP_DEFINE(upload_payload_map, upload_payload, upload_payload_fields);

int main(void) {
  CURL *curl;
  CURLcode rc;
  const char *ca_path;
  const char *url;
  upload_payload payload = {"from curl_put.c", 77};
  lonejson_curl_upload upload_ctx;
  lonejson *runtime;
  lonejson_error error;
  lonejson_status status;

  runtime = lonejson_new(NULL, &error);
  if (runtime == NULL) {
    fprintf(stderr, "runtime init failed: %s\n", error.message);
    return 1;
  }

  status = lonejson_curl_upload_init(&upload_ctx, runtime, &upload_payload_map,
                                     &payload);
  if (status != LONEJSON_STATUS_OK) {
    fprintf(stderr, "upload init failed: %s\n",
            upload_ctx.generator.error.message);
    lonejson_curl_upload_cleanup(&upload_ctx);
    lonejson_free(runtime);
    return 1;
  }

  curl = curl_easy_init();
  if (curl == NULL) {
    fprintf(stderr, "curl_easy_init failed\n");
    lonejson_curl_upload_cleanup(&upload_ctx);
    lonejson_free(runtime);
    return 1;
  }

  ca_path = find_test_ca_path();
  if (ca_path == NULL) {
    fprintf(stderr, "could not locate e2e TLS certificate\n");
    curl_easy_cleanup(curl);
    lonejson_curl_upload_cleanup(&upload_ctx);
    lonejson_free(runtime);
    return 1;
  }

  url = getenv("LONEJSON_CURL_E2E_PUT_URL");
  if (url == NULL || url[0] == '\0') {
    url = "https://localhost:8443/ingest";
  }
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_CAINFO, ca_path);
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, lonejson_curl_read_callback);
  curl_easy_setopt(curl, CURLOPT_READDATA, &upload_ctx);
  curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE,
                   lonejson_curl_upload_size(&upload_ctx));

  rc = curl_easy_perform(curl);
  if (rc != CURLE_OK) {
    fprintf(stderr, "upload failed: %s\n", curl_easy_strerror(rc));
    if (rc == CURLE_COULDNT_CONNECT) {
      fprintf(
          stderr,
          "hint: start the local compose environment with 'make compose-up'\n");
    }
    curl_easy_cleanup(curl);
    lonejson_curl_upload_cleanup(&upload_ctx);
    lonejson_free(runtime);
    return 1;
  }

  puts("");
  puts("upload completed");
  curl_easy_cleanup(curl);
  lonejson_curl_upload_cleanup(&upload_ctx);
  lonejson_free(runtime);
  return 0;
}
