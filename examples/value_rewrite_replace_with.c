#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lonejson.h"

typedef struct increment_context {
  lj_int64 delta;
} increment_context;

static lj_status increment_selected_value(lj_writer *writer,
                                          const lj_value_rewrite_old_value *old,
                                          void *user, lj_error *error) {
  increment_context *ctx = (increment_context *)user;
  char number[64];
  char *end = NULL;
  long value;

  if (!old->present) {
    return lj_writer_i64(writer, ctx->delta, error);
  }
  if (old->type != LJ_VALUE_NUMBER) {
    if (error != NULL) {
      lj_error_init(error);
      error->code = LJ_STATUS_TYPE_MISMATCH;
      snprintf(error->message, sizeof(error->message),
               "increment target is not a JSON number");
    }
    return LJ_STATUS_TYPE_MISMATCH;
  }
  if (old->number_len >= sizeof(number)) {
    if (error != NULL) {
      lj_error_init(error);
      error->code = LJ_STATUS_OVERFLOW;
      snprintf(error->message, sizeof(error->message),
               "number token is too large for this example");
    }
    return LJ_STATUS_OVERFLOW;
  }

  memcpy(number, old->number, old->number_len);
  number[old->number_len] = '\0';
  value = strtol(number, &end, 10);
  if (end == number || *end != '\0') {
    if (error != NULL) {
      lj_error_init(error);
      error->code = LJ_STATUS_TYPE_MISMATCH;
      snprintf(error->message, sizeof(error->message),
               "increment target is not an integer number");
    }
    return LJ_STATUS_TYPE_MISMATCH;
  }

  return lj_writer_i64(writer, (lj_int64)value + ctx->delta, error);
}

int main(void) {
  static const char input[] =
      "{\"meta\":{\"version\":7},\"payload\":{\"ok\":true}}";
  lj *runtime;
  lj_value_rewrite_selector_options options = {0};
  lj_owned_buffer output;
  lj_error error;
  lj_status status;
  increment_context ctx;

  ctx.delta = 5;
  lj_owned_buffer_init(&output);
  runtime = lj_new(NULL, &error);
  if (runtime == NULL) {
    fprintf(stderr, "runtime init failed: %s\n", error.message);
    return 1;
  }

  options.selector = "meta.version";
  options.action = LJ_VALUE_REWRITE_REPLACE_WITH;
  options.replace = increment_selected_value;
  options.replace_user = &ctx;

  status = lj_value_rewrite_selector_buffer(
      runtime, input, strlen(input), lj_owned_buffer_sink, &output, &options,
      &error);
  if (status != LJ_STATUS_OK) {
    fprintf(stderr, "rewrite failed: %s\n", error.message);
    lj_owned_buffer_free(&output);
    lj_free(runtime);
    return 1;
  }

  printf("%s\n", output.data);
  lj_owned_buffer_free(&output);
  lj_free(runtime);
  return 0;
}
