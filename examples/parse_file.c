#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LONEJSON_IMPLEMENTATION
#include "lonejson.h"

typedef struct inventory_item {
  char sku[16];
  lonejson_int64 quantity;
} inventory_item;

static const lonejson_field inventory_item_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(inventory_item, sku, "sku",
                                    LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_I64_REQ(inventory_item, quantity, "quantity")};
LONEJSON_MAP_DEFINE(inventory_item_map, inventory_item, inventory_item_fields);

int main(void) {
  char path[] = "/tmp/lonejson-example-file-XXXXXX";
  const char *json = "{\n  \"sku\": \"ABC-42\",\n  \"quantity\": 19\n}\n";
  inventory_item item;
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
  fputs(json, fp);
  fclose(fp);

  if (lonejson_parse_path(&inventory_item_map, &item, path, NULL, &error) !=
      LONEJSON_STATUS_OK) {
    fprintf(stderr, "parse failed: %s\n", error.message);
    unlink(path);
    return 1;
  }

  printf("sku=%s quantity=%ld\n", item.sku, (long)item.quantity);
  unlink(path);
  return 0;
}
