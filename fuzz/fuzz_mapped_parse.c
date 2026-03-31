#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lonejson.h"

typedef struct fuzz_address {
  char city[16];
  lonejson_int64 zip;
} fuzz_address;

typedef struct fuzz_person {
  char *name;
  char nickname[8];
  lonejson_int64 age;
  bool active;
  fuzz_address address;
  lonejson_i64_array lucky_numbers;
  lonejson_string_array tags;
} fuzz_person;

static const lonejson_field fuzz_address_fields[] = {
    LONEJSON_FIELD_STRING_FIXED(fuzz_address, city, "city",
                                LONEJSON_OVERFLOW_TRUNCATE),
    LONEJSON_FIELD_I64(fuzz_address, zip, "zip")};
LONEJSON_MAP_DEFINE(fuzz_address_map, fuzz_address, fuzz_address_fields);

static const lonejson_field fuzz_person_fields[] = {
    LONEJSON_FIELD_STRING_ALLOC(fuzz_person, name, "name"),
    LONEJSON_FIELD_STRING_FIXED(fuzz_person, nickname, "nickname",
                                LONEJSON_OVERFLOW_TRUNCATE),
    LONEJSON_FIELD_I64(fuzz_person, age, "age"),
    LONEJSON_FIELD_BOOL(fuzz_person, active, "active"),
    LONEJSON_FIELD_OBJECT(fuzz_person, address, "address", &fuzz_address_map),
    LONEJSON_FIELD_I64_ARRAY(fuzz_person, lucky_numbers, "lucky_numbers",
                             LONEJSON_OVERFLOW_FAIL),
    LONEJSON_FIELD_STRING_ARRAY(fuzz_person, tags, "tags",
                                LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(fuzz_person_map, fuzz_person, fuzz_person_fields);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  fuzz_person person;
  lonejson_error error;
  lonejson_status status;

  memset(&person, 0, sizeof(person));
  status = lonejson_parse_buffer(&fuzz_person_map, &person, data, size, NULL,
                                 &error);
  if (status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED) {
    char buffer[1024];
    size_t needed;
    (void)lonejson_serialize_buffer(&fuzz_person_map, &person, buffer,
                                    sizeof(buffer), &needed, NULL, &error);
  }
  lonejson_cleanup(&fuzz_person_map, &person);
  return 0;
}
