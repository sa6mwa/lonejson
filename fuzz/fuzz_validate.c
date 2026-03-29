#include <stddef.h>
#include <stdint.h>

#define LONEJSON_IMPLEMENTATION
#include "lonejson.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  lonejson_error error;

  (void)lonejson_validate_buffer(data, size, &error);
  return 0;
}
