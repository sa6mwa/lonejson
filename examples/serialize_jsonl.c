#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LONEJSON_IMPLEMENTATION
#include "lonejson.h"

typedef struct export_event {
  char id[8];
  bool ok;
} export_event;

static const lj_field export_event_fields[] = {
    LJ_FIELD_STRING_FIXED_REQ(export_event, id, "id", LJ_OVERFLOW_FAIL),
    LJ_FIELD_BOOL(export_event, ok, "ok")};
LJ_MAP_DEFINE(export_event_map, export_event, export_event_fields);

int main(void) {
  lj *runtime;
  export_event events[2];
  lj_error error;
  char *jsonl;

  memset(events, 0, sizeof(events));
  strcpy(events[0].id, "evt-1");
  events[0].ok = true;
  strcpy(events[1].id, "evt-2");
  events[1].ok = false;

  runtime = lj_new(NULL, &error);
  if (runtime == NULL) {
    fprintf(stderr, "runtime init failed: %s\n", error.message);
    return 1;
  }

  jsonl = lj_serialize_jsonl_alloc(runtime, &export_event_map, events, 2u, 0u,
                                   NULL, &error);
  if (jsonl == NULL) {
    fprintf(stderr, "serialize failed: %s\n", error.message);
    lj_free(runtime);
    return 1;
  }

  fputs(jsonl, stdout);
  LONEJSON_FREE(jsonl);
  lj_free(runtime);
  return 0;
}
