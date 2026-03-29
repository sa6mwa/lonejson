#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LJ_IMPLEMENTATION
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
  export_event events[2];
  lj_error error;
  char *jsonl;

  memset(events, 0, sizeof(events));
  strcpy(events[0].id, "evt-1");
  events[0].ok = true;
  strcpy(events[1].id, "evt-2");
  events[1].ok = false;

  jsonl = lj_serialize_jsonl_alloc(&export_event_map, events, 2u, 0u, NULL,
                                   NULL, &error);
  if (jsonl == NULL) {
    fprintf(stderr, "serialize failed: %s\n", error.message);
    return 1;
  }

  fputs(jsonl, stdout);
  free(jsonl);
  return 0;
}
