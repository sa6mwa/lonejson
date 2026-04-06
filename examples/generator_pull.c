#include <stdio.h>
#include <string.h>

#include "lonejson.h"

typedef struct example_record {
  char id[32];
  bool ok;
} example_record;

static const lonejson_field example_record_fields[] = {
    LONEJSON_FIELD_STRING_FIXED_REQ(example_record, id, "id",
                                    LONEJSON_OVERFLOW_TRUNCATE),
    LONEJSON_FIELD_BOOL_REQ(example_record, ok, "ok")};

LONEJSON_MAP_DEFINE(example_record_map, example_record, example_record_fields);

int main(void) {
  example_record record;
  lonejson_generator generator;
  unsigned char buffer[16];
  size_t out_len;
  int eof;

  memset(&record, 0, sizeof(record));
  memcpy(record.id, "evt-1", 6u);
  record.ok = true;

  if (lonejson_generator_init(&generator, &example_record_map, &record, NULL) !=
      LONEJSON_STATUS_OK) {
    fprintf(stderr, "generator init failed: %s\n", generator.error.message);
    return 1;
  }

  eof = 0;
  while (!eof) {
    if (lonejson_generator_read(&generator, buffer, sizeof(buffer), &out_len,
                                &eof) != LONEJSON_STATUS_OK) {
      fprintf(stderr, "generator read failed: %s\n", generator.error.message);
      lonejson_generator_cleanup(&generator);
      return 1;
    }
    if (out_len != 0u) {
      fwrite(buffer, 1u, out_len, stdout);
    }
  }
  fputc('\n', stdout);
  lonejson_generator_cleanup(&generator);
  return 0;
}
