#include <stddef.h>
#include <stdint.h>

#include "lonejson.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  lonejson *runtime;
  lonejson_error error;

  runtime = lonejson_new(NULL, &error);
  if (runtime == NULL) {
    return 0;
  }
  (void)lonejson_validate_buffer(runtime, data, size, &error);
  lonejson_free(runtime);
  return 0;
}
