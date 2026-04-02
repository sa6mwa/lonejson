#include <stdio.h>
#include <stdlib.h>

#include "lonejson.h"

typedef struct upload_payload {
  char *message;
  lonejson_int64 id;
} upload_payload;

static const char *find_test_ca_path(void) {
  static const char *const candidates[] = {
      "docker/nginx/certs/server.crt", "../docker/nginx/certs/server.crt",
      "../../docker/nginx/certs/server.crt"};
  FILE *fp;
  size_t i;

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
  upload_payload payload = {"from curl_put.c", 77};
  lonejson_curl_upload upload_ctx;

  if (lonejson_curl_upload_init(&upload_ctx, &upload_payload_map, &payload,
                                NULL) != LONEJSON_STATUS_OK) {
    fprintf(stderr, "upload init failed: %s\n", upload_ctx.error.message);
    return 1;
  }

  curl = curl_easy_init();
  if (curl == NULL) {
    fprintf(stderr, "curl_easy_init failed\n");
    lonejson_curl_upload_cleanup(&upload_ctx);
    return 1;
  }

  ca_path = find_test_ca_path();
  if (ca_path == NULL) {
    fprintf(stderr, "could not locate docker/nginx/certs/server.crt\n");
    curl_easy_cleanup(curl);
    lonejson_curl_upload_cleanup(&upload_ctx);
    return 1;
  }

  curl_easy_setopt(curl, CURLOPT_URL, "https://localhost:8443/ingest");
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
    return 1;
  }

  puts("");
  puts("upload completed");
  curl_easy_cleanup(curl);
  lonejson_curl_upload_cleanup(&upload_ctx);
  return 0;
}
