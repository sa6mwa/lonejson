#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "lonejson.h"

typedef struct export_record {
  char id[12];
  double total;
} export_record;

static const lonejson_field export_record_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(export_record, id, "id",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_F64(export_record, total, "total")};
LONEJSON_MAP_DEFINE(export_record_map, export_record, export_record_fields);

int main(void) {
  char path[] = "/tmp/lonejson-example-write-XXXXXX";
  lonejson *lj;
  export_record record = {"INV-1001", 42.5};
  lonejson_error error;
  int fd = mkstemp(path);
  FILE *fp;

  if (fd < 0) {
    perror("mkstemp");
    return 1;
  }
  fp = fdopen(fd, "wb");
  if (fp == NULL) {
    perror("fdopen");
    close(fd);
    unlink(path);
    return 1;
  }

  lj = lonejson_new(NULL, &error);
  if (lj == NULL) {
    fprintf(stderr, "runtime init failed: %s\n", error.message);
    fclose(fp);
    unlink(path);
    return 1;
  }

  if (lonejson_serialize_filep(lj, &export_record_map, &record, fp, &error) !=
      LONEJSON_STATUS_OK) {
    fprintf(stderr, "serialize failed: %s\n", error.message);
    fclose(fp);
    unlink(path);
    lonejson_free(lj);
    return 1;
  }
  fclose(fp);

  printf("wrote %s\n", path);
  unlink(path);
  lonejson_free(lj);
  return 0;
}
