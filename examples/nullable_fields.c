#include <stdio.h>
#include <stdlib.h>

#include "lonejson.h"

typedef struct job_status {
  char name[32];
  lj_uint64 attempts;
  int has_attempts;
  double score;
  int has_score;
  bool enabled;
  int has_enabled;
} job_status;

static const lj_field job_status_fields[] = {
    LJ_FIELD_STRING_FIXED_REQ(job_status, name, "name", LJ_OVERFLOW_FAIL),
    LJ_FIELD_U64_PRESENT_NULLABLE(job_status, attempts, has_attempts,
                                  "attempts"),
    LJ_FIELD_F64_PRESENT_NULLABLE(job_status, score, has_score, "score"),
    LJ_FIELD_BOOL_PRESENT_NULLABLE(job_status, enabled, has_enabled,
                                   "enabled")};
LJ_MAP_DEFINE(job_status_map, job_status, job_status_fields);

static int parse_and_print(const char *json) {
  job_status status;
  lj_error error;

  lj_init(&job_status_map, &status);
  if (lj_parse_cstr(&job_status_map, &status, json, NULL, &error) !=
      LJ_STATUS_OK) {
    fprintf(stderr, "parse failed: %s\n", error.message);
    return 1;
  }

  printf("name=%s attempts=%s", status.name,
         status.has_attempts ? "present" : "nil");
  if (status.has_attempts) {
    printf("(%lu)", (unsigned long)status.attempts);
  }
  printf(" score=%s", status.has_score ? "present" : "nil");
  if (status.has_score) {
    printf("(%.2f)", status.score);
  }
  printf(" enabled=%s", status.has_enabled ? "present" : "nil");
  if (status.has_enabled) {
    printf("(%s)", status.enabled ? "true" : "false");
  }
  putchar('\n');

  lj_cleanup(&job_status_map, &status);
  return 0;
}

int main(void) {
  job_status out;
  lj_error error;
  char *json;

  if (parse_and_print(
          "{\"name\":\"queued\",\"attempts\":null,\"enabled\":true}") != 0) {
    return 1;
  }
  if (parse_and_print("{\"name\":\"new\"}") != 0) {
    return 1;
  }

  lj_init(&job_status_map, &out);
  snprintf(out.name, sizeof(out.name), "%s", "retrying");
  out.attempts = 3u;
  out.has_attempts = 1;
  out.has_score = 0;
  out.enabled = false;
  out.has_enabled = 1;

  json = lj_serialize_alloc(&job_status_map, &out, NULL, NULL, &error);
  if (json == NULL) {
    fprintf(stderr, "serialize failed: %s\n", error.message);
    return 1;
  }

  puts(json);
  LJ_FREE(json);
  lj_cleanup(&job_status_map, &out);
  return 0;
}
