#include <string.h>

#define LONEJSON_DISABLE_SHORT_NAMES 1
#define LONEJSON_IMPLEMENTATION
#include "lonejson.h"

#if defined(lj_status)
#error short type aliases must be disabled
#endif

#if defined(LJ_STATUS_OK)
#error short constant aliases must be disabled
#endif

#if defined(lj_parse_cstr)
#error short function aliases must be disabled
#endif

#if defined(LJ_FIELD_I64)
#error short field macros must be disabled
#endif

int main(void) {
  lonejson_error error;

  memset(&error, 0, sizeof(error));
  return error.code == LONEJSON_STATUS_OK ? 0 : 1;
}
