#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lonejson.h"

typedef struct outbound_bytes_doc {
  char id[16];
  lonejson_source bytes;
} outbound_bytes_doc;

static const lonejson_field outbound_bytes_doc_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(outbound_bytes_doc, id, "id",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_BASE64_SOURCE(outbound_bytes_doc, bytes, "bytes")};
LONEJSON_MAP_DEFINE(outbound_bytes_doc_map, outbound_bytes_doc,
                    outbound_bytes_doc_fields);

int main(void) {
  static const unsigned char payload[] = {0x00u, 0x01u, 0x02u,
                                          0x7fu, 0x80u, 0xffu};
  outbound_bytes_doc doc;
  lonejson_error error;
  lonejson_status status;
  char path[] = "/tmp/lonejson-source-bytes-XXXXXX";
  int fd;
  char *json;

  lonejson_init(&outbound_bytes_doc_map, &doc);
  strcpy(doc.id, "bin-1");

  fd = mkstemp(path);
  if (fd < 0) {
    perror("mkstemp");
    return 1;
  }
  if (write(fd, payload, sizeof(payload)) != (ssize_t)sizeof(payload)) {
    perror("write");
    close(fd);
    unlink(path);
    return 1;
  }
  if (lseek(fd, 0, SEEK_SET) < 0) {
    perror("lseek");
    close(fd);
    unlink(path);
    return 1;
  }

  status = lonejson_source_set_fd(&doc.bytes, fd, &error);
  if (status != LONEJSON_STATUS_OK) {
    fprintf(stderr, "set_fd failed: %s\n", error.message);
    close(fd);
    unlink(path);
    return 1;
  }

  json = lonejson_serialize_alloc(&outbound_bytes_doc_map, &doc, NULL, NULL,
                                  &error);
  if (json == NULL) {
    fprintf(stderr, "serialize failed: %s\n", error.message);
    lonejson_cleanup(&outbound_bytes_doc_map, &doc);
    close(fd);
    unlink(path);
    return 1;
  }

  printf("fd-backed bytes source serialized as base64 JSON\n");
  printf("json=%s\n", json);

  free(json);
  lonejson_cleanup(&outbound_bytes_doc_map, &doc);
  close(fd);
  unlink(path);
  return 0;
}
