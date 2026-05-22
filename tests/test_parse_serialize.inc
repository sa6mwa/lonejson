static void test_parse_implicit_destination_reset(void) {
  const char *json =
      "{\"name\":\"Reset\",\"address\":{\"city\":\"Oslo\",\"zip\":42},"
      "\"active\":false,\"lucky_numbers\":[],\"tags\":[],\"items\":[]}";
  test_person person;
  lonejson_error error;
  lonejson_status status;

  poison_bytes(&person, sizeof(person), 0xA5u);
  status = lonejson_parse_cstr(&test_person_map, &person, json, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(person.name != NULL && strcmp(person.name, "Reset") == 0);
  EXPECT(person.nickname[0] == '\0');
  EXPECT(person.age == 0);
  EXPECT(person.score == 0.0);
  EXPECT(person.active == false);
  EXPECT(strcmp(person.address.city, "Oslo") == 0);
  EXPECT(person.address.zip == 42);
  EXPECT(person.lucky_numbers.count == 0);
  EXPECT(person.tags.count == 0);
  EXPECT(person.items.count == 0);
  lonejson_cleanup(&test_person_map, &person);
}

static void test_dynamic_allocation_cleanup_balance(void) {
  const char *json =
      "{\"name\":\"Balance\",\"nickname\":\"bal\",\"age\":9,\"score\":9.5,"
      "\"active\":true,\"address\":{\"city\":\"Rome\",\"zip\":900},"
      "\"lucky_numbers\":[1,2,3,4,5],\"tags\":[\"a\",\"b\",\"c\"],"
      "\"items\":[{\"id\":1,\"label\":\"first\"},{\"id\":2,\"label\":"
      "\"second\"}]}";
  test_person person;
  lonejson_error error;
  lonejson_status status;

  reset_lonejson_alloc_stats();
  poison_bytes(&person, sizeof(person), 0x11u);
  status = lonejson_parse_cstr(&test_person_map, &person, json, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  EXPECT(g_lonejson_alloc_calls > 0u);
  EXPECT(g_alloc_record_count > 0u);
  lonejson_cleanup(&test_person_map, &person);
  EXPECT(g_alloc_record_count == 0u);
  EXPECT(g_lonejson_free_calls > 0u);
}

static void test_zero_alloc_validate_path(void) {
  lonejson_error error;
  lonejson_status status;

  reset_lonejson_alloc_stats();
  status = lonejson_validate_cstr("{\"ok\":true,\"items\":[1,2,3]}", &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);
}

static void test_f64_boundary_exponents_stay_on_fast_path(void) {
  double parsed;

  parsed = 0.0;
  EXPECT(lonejson__parse_f64_fast("0.1e309", strlen("0.1e309"), &parsed));
  EXPECT(parsed > 9.0e307);
  EXPECT(parsed < DBL_MAX);

  parsed = 0.0;
  EXPECT(lonejson__parse_f64_fast("1.5e-309", strlen("1.5e-309"), &parsed));
  EXPECT(parsed > 0.0);
  EXPECT(parsed < 1.0e-308);
}

static void test_f64_boundary_exponents_are_locale_independent(void) {
  static const char *const locales[] = {"sv_SE.UTF-8", "sv_SE.utf8",
                                        "de_DE.UTF-8", "de_DE.utf8",
                                        "fr_FR.UTF-8", "fr_FR.utf8"};
  char saved_locale[128];
  const char *current_locale;
  const char *selected_locale = NULL;
  size_t i;
  test_person person;
  lonejson_error error;
  lonejson_status status;
  const char *json =
      "{\"name\":\"Locale\",\"score\":0.1e309,"
      "\"address\":{\"city\":\"Malmo\",\"zip\":211},"
      "\"active\":true,\"lucky_numbers\":[],\"tags\":[],\"items\":[]}";

  current_locale = setlocale(LC_NUMERIC, NULL);
  snprintf(saved_locale, sizeof(saved_locale), "%s",
           current_locale != NULL ? current_locale : "C");
  for (i = 0u; i < sizeof(locales) / sizeof(locales[0]); ++i) {
    if (setlocale(LC_NUMERIC, locales[i]) != NULL) {
      selected_locale = locales[i];
      break;
    }
  }
  if (selected_locale == NULL) {
    (void)setlocale(LC_NUMERIC, saved_locale);
    return;
  }

  poison_bytes(&person, sizeof(person), 0x3Cu);
  status = lonejson_parse_cstr(&test_person_map, &person, json, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  if (status == LONEJSON_STATUS_OK) {
    EXPECT(person.score > 9.0e307);
    EXPECT(person.score < DBL_MAX);
    lonejson_cleanup(&test_person_map, &person);
  }
  (void)setlocale(LC_NUMERIC, saved_locale);
}

static void test_parser_workspace_accounting(void) {
  const char *json = "{\"name\":\"workspace-check\",\"codes\":[5,8,13,21]}";
  test_fixed_log log;
  lonejson_int64 codes_storage[4];
  lonejson_parser parser_storage;
  unsigned char parser_workspace[LONEJSON_PARSER_BUFFER_SIZE];
  lonejson_error error;
  lonejson_status status;

  memset(&log, 0, sizeof(log));
  lonejson_init(&test_fixed_log_map, &log);
  log.codes.items = codes_storage;
  log.codes.capacity = 4u;
  log.codes.flags = LONEJSON_ARRAY_FIXED_CAPACITY;

  reset_lonejson_alloc_stats();
  lonejson__parser_init_state(&parser_storage, &test_fixed_log_map, &log, NULL,
                              0, parser_workspace, sizeof(parser_workspace));
  status = lonejson__parser_feed_bytes(
      &parser_storage, (const unsigned char *)json, strlen(json), NULL, 0);
  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  status = lonejson_parser_finish(&parser_storage);
  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  error = parser_storage.error;
  EXPECT(error.code == LONEJSON_STATUS_OK || error.code == status);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);
  EXPECT(lonejson__parser_peak_workspace_used(&parser_storage) > 0u);
  EXPECT(lonejson__parser_peak_workspace_used(&parser_storage) <=
         (size_t)LONEJSON_PARSER_BUFFER_SIZE);
  lonejson_cleanup(&test_fixed_log_map, &log);
}

static void test_parser_workspace_alignment_regression(void) {
  const char *json = "{\"name\":\"aligned\",\"codes\":[1,2]}";
  lonejson_parser parser_storage;
  unsigned char raw_workspace[LONEJSON_PARSER_BUFFER_SIZE +
                              LONEJSON__PARSER_WORKSPACE_ALIGNMENT];
  unsigned char *misaligned_workspace = raw_workspace + 1u;
  test_fixed_log log;
  lonejson_int64 codes_storage[4];
  lonejson_status status;
  size_t alignment_offset;

  memset(&log, 0, sizeof(log));
  lonejson_init(&test_fixed_log_map, &log);
  log.codes.items = codes_storage;
  log.codes.capacity = 4u;
  log.codes.flags = LONEJSON_ARRAY_FIXED_CAPACITY;

  lonejson__parser_init_state(&parser_storage, &test_fixed_log_map, &log, NULL,
                              0, misaligned_workspace,
                              LONEJSON_PARSER_BUFFER_SIZE);
  EXPECT(parser_storage.workspace != misaligned_workspace);
  EXPECT(parser_storage.workspace_size <= LONEJSON_PARSER_BUFFER_SIZE);
  alignment_offset = (size_t)(parser_storage.workspace - misaligned_workspace);
  EXPECT(alignment_offset < LONEJSON__PARSER_WORKSPACE_ALIGNMENT);
  EXPECT(parser_storage.frames == (lonejson_frame *)parser_storage.workspace);
  EXPECT(parser_storage.workspace ==
         lonejson__align_pointer(misaligned_workspace,
                                 LONEJSON__PARSER_WORKSPACE_ALIGNMENT));

  status = lonejson__parser_feed_bytes(
      &parser_storage, (const unsigned char *)json, strlen(json), NULL, 0);
  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  status = lonejson_parser_finish(&parser_storage);
  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  EXPECT(strcmp(log.name, "aligned") == 0);
  EXPECT(log.codes.count == 2u);
  EXPECT(log.codes.items[1] == 2);

  lonejson_cleanup(&test_fixed_log_map, &log);
}

static void test_init_does_not_preserve_garbage_array_state(void) {
  test_person person;

  poison_bytes(&person, sizeof(person), 0x7bu);
  person.items.items = (test_item *)0x10f;
  person.items.count = 99u;
  person.items.capacity = 99u;
  person.items.elem_size = sizeof(test_item);
  person.items.flags = LONEJSON_ARRAY_FIXED_CAPACITY;

  lonejson_init(&test_person_map, &person);

  EXPECT(person.name == NULL);
  EXPECT(person.nickname[0] == '\0');
  EXPECT(person.items.items == NULL);
  EXPECT(person.items.count == 0u);
  EXPECT(person.items.capacity == 0u);
  EXPECT(person.items.flags == 0u);
  EXPECT(person.items.elem_size == sizeof(test_item));

  lonejson_cleanup(&test_person_map, &person);
}

static void test_zero_alloc_fixed_storage_parse(void) {
  const char *json = "{\"name\":\"long-event-name\",\"codes\":[5,8,13]}";
  test_fixed_log log;
  lonejson_int64 codes_storage[4];
  lonejson_error error;
  lonejson_status status;
  lonejson_parse_options options = lonejson_default_parse_options();

  memset(&log, 0, sizeof(log));
  lonejson_init(&test_fixed_log_map, &log);
  log.codes.items = codes_storage;
  log.codes.capacity = 4u;
  log.codes.flags = LONEJSON_ARRAY_FIXED_CAPACITY;
  options.clear_destination = 0;

  reset_lonejson_alloc_stats();
  status =
      lonejson_parse_cstr(&test_fixed_log_map, &log, json, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  EXPECT(strcmp(log.name, "long-ev") == 0);
  EXPECT(log.codes.count == 3u);
  EXPECT(log.codes.items[2] == 13);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);
  lonejson_cleanup(&test_fixed_log_map, &log);
}

static void test_zero_alloc_fixed_storage_serialize(void) {
  test_fixed_log log;
  lonejson_int64 codes_storage[4];
  test_counting_sink sink;
  char buffer[128];
  char path[] = "/tmp/lonejson-serialize-XXXXXX";
  lonejson_error error;
  lonejson_status status;
  int fd;
  FILE *fp;

  memset(&log, 0, sizeof(log));
  memset(codes_storage, 0, sizeof(codes_storage));
  memset(&sink, 0, sizeof(sink));
  strcpy(log.name, "evt");
  codes_storage[0] = 5;
  codes_storage[1] = 8;
  codes_storage[2] = 13;
  log.codes.items = codes_storage;
  log.codes.count = 3u;
  log.codes.capacity = 4u;
  log.codes.flags = LONEJSON_ARRAY_FIXED_CAPACITY;

  reset_lonejson_alloc_stats();
  status = lonejson_serialize_sink(
      &test_fixed_log_map, &log, test_counting_sink_write, &sink, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(sink.total != 0u);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);

  reset_lonejson_alloc_stats();
  status = lonejson_serialize_buffer(&test_fixed_log_map, &log, buffer,
                                     sizeof(buffer), NULL, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, "{\"name\":\"evt\",\"codes\":[5,8,13]}") == 0);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);

  fd = mkstemp(path);
  EXPECT(fd >= 0);
  if (fd < 0) {
    return;
  }
  fp = fdopen(fd, "wb");
  EXPECT(fp != NULL);
  if (fp == NULL) {
    close(fd);
    unlink(path);
    return;
  }
  reset_lonejson_alloc_stats();
  status =
      lonejson_serialize_filep(&test_fixed_log_map, &log, fp, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);
  fclose(fp);

  reset_lonejson_alloc_stats();
  status =
      lonejson_serialize_path(&test_fixed_log_map, &log, path, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);
  unlink(path);
}

static void test_zero_alloc_jsonl_serialize(void) {
  test_event events[2];
  test_counting_sink sink;
  char buffer[128];
  char path[] = "/tmp/lonejson-jsonl-serialize-XXXXXX";
  lonejson_error error;
  lonejson_status status;
  int fd;
  FILE *fp;

  memset(events, 0, sizeof(events));
  memset(&sink, 0, sizeof(sink));
  strcpy(events[0].id, "evt-1");
  events[0].ok = true;
  strcpy(events[1].id, "evt-2");
  events[1].ok = false;

  reset_lonejson_alloc_stats();
  status = lonejson_serialize_jsonl_sink(&test_event_map, events, 2u, 0u,
                                         test_counting_sink_write, &sink, NULL,
                                         &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(sink.total != 0u);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);

  reset_lonejson_alloc_stats();
  status =
      lonejson_serialize_jsonl_buffer(&test_event_map, events, 2u, 0u, buffer,
                                      sizeof(buffer), NULL, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, "{\"id\":\"evt-1\",\"ok\":true}\n"
                        "{\"id\":\"evt-2\",\"ok\":false}\n") == 0);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);

  fd = mkstemp(path);
  EXPECT(fd >= 0);
  if (fd < 0) {
    return;
  }
  fp = fdopen(fd, "wb");
  EXPECT(fp != NULL);
  if (fp == NULL) {
    close(fd);
    unlink(path);
    return;
  }
  reset_lonejson_alloc_stats();
  status = lonejson_serialize_jsonl_filep(&test_event_map, events, 2u, 0u, fp,
                                          NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);
  fclose(fp);

  reset_lonejson_alloc_stats();
  status = lonejson_serialize_jsonl_path(&test_event_map, events, 2u, 0u, path,
                                         NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(g_lonejson_alloc_calls == 0u);
  EXPECT(g_lonejson_free_calls == 0u);
  EXPECT(g_alloc_record_count == 0u);
  unlink(path);
}

static void test_serialize_alloc_balance(void) {
  test_fixed_log log;
  lonejson_int64 codes_storage[2];
  lonejson_error error;
  char *json;

  memset(&log, 0, sizeof(log));
  strcpy(log.name, "evt");
  codes_storage[0] = 1;
  codes_storage[1] = 2;
  log.codes.items = codes_storage;
  log.codes.count = 2u;
  log.codes.capacity = 2u;
  log.codes.flags = LONEJSON_ARRAY_FIXED_CAPACITY;

  reset_lonejson_alloc_stats();
  json =
      lonejson_serialize_alloc(&test_fixed_log_map, &log, NULL, NULL, &error);
  EXPECT(json != NULL);
  EXPECT(g_lonejson_alloc_calls > 0u);
  EXPECT(g_alloc_record_count == 1u);
  if (json != NULL) {
    LONEJSON_FREE(json);
  }
  EXPECT(g_alloc_record_count == 0u);
  EXPECT(g_lonejson_free_calls > 0u);
}

static void test_serialize_owned_output_cap_is_enforced(void) {
  test_allocator_state alloc;
  lonejson_write_options options = lonejson_default_write_options();
  lonejson_error error;
  lonejson_status status;
  lonejson_owned_buffer owned;
  test_event event;
  char buffer[128];
  size_t needed = 0u;

  memset(&event, 0, sizeof(event));
  strcpy(event.id, "evt-cap");
  event.ok = true;

  options.max_output_bytes = 16u;
  status = lonejson_serialize_buffer(&test_event_map, &event, buffer,
                                     sizeof(buffer), &needed, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, "{\"id\":\"evt-cap\",\"ok\":true}") == 0);
  EXPECT(needed == strlen(buffer));

  test_allocator_init(&alloc);
  options.allocator = &alloc.allocator;
  lonejson_owned_buffer_init(&owned);
  status = lonejson_serialize_owned(&test_event_map, &event, &owned, &options,
                                    &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(error.code == LONEJSON_STATUS_OVERFLOW);
  EXPECT(strstr(error.message, "max_output_bytes") != NULL);
  EXPECT(owned.data == NULL);
  EXPECT(owned.len == 0u);
  EXPECT(alloc.stats.peak_bytes_live <= options.max_output_bytes + 1u);
  EXPECT(alloc.stats.bytes_live == 0u);
}

static void test_serialize_owned_output_cap_allows_exact_payload(void) {
  lonejson_write_options options = lonejson_default_write_options();
  lonejson_error error;
  lonejson_status status;
  lonejson_owned_buffer owned;
  test_event event;
  const char *expected;

  memset(&event, 0, sizeof(event));
  strcpy(event.id, "exact");
  event.ok = true;
  expected = "{\"id\":\"exact\",\"ok\":true}";
  options.max_output_bytes = strlen(expected);
  lonejson_owned_buffer_init(&owned);

  status = lonejson_serialize_owned(&test_event_map, &event, &owned, &options,
                                    &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(owned.data != NULL);
  EXPECT(owned.len == strlen(expected));
  if (owned.data != NULL) {
    EXPECT(strcmp(owned.data, expected) == 0);
    EXPECT(owned.data[owned.len] == '\0');
  }
  lonejson_owned_buffer_free(&owned);

  options.max_output_bytes = strlen(expected) - 1u;
  status = lonejson_serialize_owned(&test_event_map, &event, &owned, &options,
                                    &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(error.code == LONEJSON_STATUS_OVERFLOW);
  EXPECT(owned.data == NULL);
}

static void test_serialize_alloc_zero_cap_uses_safe_default(void) {
  lonejson_write_options options = lonejson_default_write_options();
  lonejson_error error;
  test_event event;
  char *serialized;

  memset(&event, 0, sizeof(event));
  strcpy(event.id, "zcap");
  event.ok = true;
  options.max_output_bytes = 0u;
  serialized =
      lonejson_serialize_alloc(&test_event_map, &event, NULL, &options, &error);
  EXPECT(serialized != NULL);
  if (serialized != NULL) {
    EXPECT(strcmp(serialized, "{\"id\":\"zcap\",\"ok\":true}") == 0);
    LONEJSON_FREE(serialized);
  }
}

static int test_child_serialize_alloc_free_compatibility(void) {
  test_fixed_log log;
  lonejson_int64 codes_storage[2];
  lonejson_error error;
  char *json;

  memset(&log, 0, sizeof(log));
  strcpy(log.name, "evt");
  codes_storage[0] = 1;
  codes_storage[1] = 2;
  log.codes.items = codes_storage;
  log.codes.count = 2u;
  log.codes.capacity = 2u;
  log.codes.flags = LONEJSON_ARRAY_FIXED_CAPACITY;

  reset_lonejson_alloc_stats();
  json =
      lonejson_serialize_alloc(&test_fixed_log_map, &log, NULL, NULL, &error);
  if (json == NULL) {
    return 2;
  }
  LONEJSON_FREE(json);
  return (g_alloc_record_count == 0u) ? 0 : 3;
}

static void test_serialize_alloc_default_free_compatibility(void) {
  pid_t pid;
  int status;

  pid = fork();
  EXPECT(pid >= 0);
  if (pid == 0) {
    _exit(test_child_serialize_alloc_free_compatibility());
  }
  if (pid < 0) {
    return;
  }
  EXPECT(waitpid(pid, &status, 0) == pid);
  EXPECT(WIFEXITED(status));
  if (WIFEXITED(status)) {
    EXPECT(WEXITSTATUS(status) == 0);
  }
}

static void test_serialize_jsonl_alloc_balance(void) {
  test_event events[2];
  lonejson_error error;
  char *jsonl;

  memset(events, 0, sizeof(events));
  strcpy(events[0].id, "evt-1");
  events[0].ok = true;
  strcpy(events[1].id, "evt-2");
  events[1].ok = false;

  reset_lonejson_alloc_stats();
  jsonl = lonejson_serialize_jsonl_alloc(&test_event_map, events, 2u, 0u, NULL,
                                         NULL, &error);
  EXPECT(jsonl != NULL);
  EXPECT(g_lonejson_alloc_calls > 0u);
  EXPECT(g_alloc_record_count == 1u);
  if (jsonl != NULL) {
    LONEJSON_FREE(jsonl);
  }
  EXPECT(g_alloc_record_count == 0u);
  EXPECT(g_lonejson_free_calls > 0u);
}

static void test_serialize_jsonl_alloc_output_cap_is_enforced(void) {
  lonejson_write_options options = lonejson_default_write_options();
  lonejson_error error;
  test_event events[2];
  char *jsonl;

  memset(events, 0, sizeof(events));
  strcpy(events[0].id, "cap-1");
  events[0].ok = true;
  strcpy(events[1].id, "cap-2");
  events[1].ok = false;

  options.max_output_bytes = 32u;
  jsonl = lonejson_serialize_jsonl_alloc(&test_event_map, events, 2u, 0u, NULL,
                                         &options, &error);
  EXPECT(jsonl == NULL);
  EXPECT(error.code == LONEJSON_STATUS_OVERFLOW);
  EXPECT(strstr(error.message, "max_output_bytes") != NULL);
}

static void test_serialize_alloc_pretty_output_cap_is_enforced(void) {
  lonejson_write_options options = lonejson_default_write_options();
  lonejson_error error;
  test_event event;
  char *serialized;

  memset(&event, 0, sizeof(event));
  strcpy(event.id, "pcap");
  event.ok = true;

  options.pretty = 1;
  options.max_output_bytes = 24u;
  serialized =
      lonejson_serialize_alloc(&test_event_map, &event, NULL, &options, &error);
  EXPECT(serialized == NULL);
  EXPECT(error.code == LONEJSON_STATUS_OVERFLOW);
  EXPECT(strstr(error.message, "max_output_bytes") != NULL);
}

static int test_child_serialize_jsonl_alloc_free_compatibility(void) {
  test_event events[2];
  lonejson_error error;
  char *jsonl;

  memset(events, 0, sizeof(events));
  strcpy(events[0].id, "evt-1");
  events[0].ok = true;
  strcpy(events[1].id, "evt-2");
  events[1].ok = false;

  reset_lonejson_alloc_stats();
  jsonl = lonejson_serialize_jsonl_alloc(&test_event_map, events, 2u, 0u, NULL,
                                         NULL, &error);
  if (jsonl == NULL) {
    return 2;
  }
  LONEJSON_FREE(jsonl);
  return (g_alloc_record_count == 0u) ? 0 : 3;
}

static void test_serialize_jsonl_alloc_default_free_compatibility(void) {
  pid_t pid;
  int status;

  pid = fork();
  EXPECT(pid >= 0);
  if (pid == 0) {
    _exit(test_child_serialize_jsonl_alloc_free_compatibility());
  }
  if (pid < 0) {
    return;
  }
  EXPECT(waitpid(pid, &status, 0) == pid);
  EXPECT(WIFEXITED(status));
  if (WIFEXITED(status)) {
    EXPECT(WEXITSTATUS(status) == 0);
  }
}

static void test_dynamic_object_array_alignment(void) {
  static const char json[] =
      "{\"items\":[{\"name\":\"a\"},{\"name\":\"b\"},{\"name\":\"c\"}]}";
  test_aligned_doc doc;
  lonejson_error error;
  lonejson_status status;
  struct align_probe {
    char c;
    test_aligned_item value;
  };
  size_t alignment;
  uintptr_t bits;

  memset(&doc, 0, sizeof(doc));
  status = lonejson_parse_cstr(&test_aligned_doc_map, &doc, json, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(doc.items.count == 3u);
  EXPECT(doc.items.items != NULL);
  if (doc.items.items != NULL) {
    alignment = offsetof(struct align_probe, value);
    bits = (uintptr_t)doc.items.items;
    EXPECT((bits % alignment) == 0u);
    EXPECT(strcmp(((test_aligned_item *)doc.items.items)[0].name, "a") == 0);
    ((test_aligned_item *)doc.items.items)[2].weight = (long double)3.75;
    EXPECT(((test_aligned_item *)doc.items.items)[2].weight > 3.7);
  }
  lonejson_cleanup(&test_aligned_doc_map, &doc);
}

static void test_dynamic_allocation_reset_reparse_balance(void) {
  const char *json_a =
      "{\"name\":\"First\",\"nickname\":\"aa\",\"age\":1,\"score\":1.0,"
      "\"active\":false,\"address\":{\"city\":\"A\",\"zip\":1},"
      "\"lucky_numbers\":[1],\"tags\":[\"x\"],"
      "\"items\":[{\"id\":10,\"label\":\"one\"}]}";
  const char *json_b =
      "{\"name\":\"Second\",\"nickname\":\"bb\",\"age\":2,\"score\":2.0,"
      "\"active\":true,\"address\":{\"city\":\"B\",\"zip\":2},"
      "\"lucky_numbers\":[7,8],\"tags\":[\"y\",\"z\"],"
      "\"items\":[{\"id\":20,\"label\":\"two\"},{\"id\":30,\"label\":\"three\"}"
      "]}";
  test_person person;
  lonejson_error error;
  lonejson_status status;

  reset_lonejson_alloc_stats();
  poison_bytes(&person, sizeof(person), 0x22u);
  status = lonejson_parse_cstr(&test_person_map, &person, json_a, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  lonejson_reset(&test_person_map, &person);
  EXPECT(g_alloc_record_count == 0u);
  status = lonejson_parse_cstr(&test_person_map, &person, json_b, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  EXPECT(person.name != NULL && strcmp(person.name, "Second") == 0);
  EXPECT(person.tags.count == 2);
  lonejson_cleanup(&test_person_map, &person);
  EXPECT(g_alloc_record_count == 0u);
}

static void test_clear_destination_false_present_arrays_replace(void) {
  const char *json_a =
      "{\"name\":\"First\",\"nickname\":\"aa\",\"age\":1,\"score\":1.0,"
      "\"active\":false,\"address\":{\"city\":\"A\",\"zip\":1},"
      "\"lucky_numbers\":[1,2,3],\"tags\":[\"x\",\"y\"],"
      "\"items\":[{\"id\":10,\"label\":\"one\"},{\"id\":11,\"label\":\"two\"}]}";
  const char *json_b =
      "{\"name\":\"First\",\"address\":{\"city\":\"A\",\"zip\":1},"
      "\"lucky_numbers\":[7,8],\"tags\":[\"z\"],"
      "\"items\":[{\"id\":20,\"label\":\"three\"}]}";
  test_person person;
  lonejson_parse_options options = lonejson_default_parse_options();
  lonejson_error error;
  lonejson_status status;

  poison_bytes(&person, sizeof(person), 0x23u);
  status = lonejson_parse_cstr(&test_person_map, &person, json_a, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);

  options.clear_destination = 0;
  status =
      lonejson_parse_cstr(&test_person_map, &person, json_b, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  EXPECT(person.name != NULL && strcmp(person.name, "First") == 0);
  EXPECT(person.lucky_numbers.count == 2u);
  EXPECT(person.lucky_numbers.items[0] == 7);
  EXPECT(person.lucky_numbers.items[1] == 8);
  EXPECT(person.tags.count == 1u);
  EXPECT(strcmp(person.tags.items[0], "z") == 0);
  EXPECT(person.items.count == 1u);
  EXPECT(((test_item *)person.items.items)[0].id == 20);

  lonejson_cleanup(&test_person_map, &person);
}

static void
test_clear_destination_false_fixed_capacity_string_array_reuse_frees_replaced_entries(
    void) {
  const char *json_a =
      "{\"name\":\"First\",\"address\":{\"city\":\"A\",\"zip\":1},"
      "\"tags\":[\"old-a\",\"old-b\",\"old-c\"]}";
  const char *json_b =
      "{\"name\":\"Second\",\"address\":{\"city\":\"B\",\"zip\":2},"
      "\"tags\":[\"fresh\"]}";
  test_person person;
  char *tag_storage[4];
  test_allocator_state alloc;
  lonejson_parse_options options = lonejson_default_parse_options();
  lonejson_error error;
  lonejson_status status;

  memset(&person, 0, sizeof(person));
  memset(tag_storage, 0, sizeof(tag_storage));
  test_allocator_init(&alloc);
  lonejson_init(&test_person_map, &person);
  person.tags.items = tag_storage;
  person.tags.capacity = 4u;
  person.tags.flags = LONEJSON_ARRAY_FIXED_CAPACITY;
  options.clear_destination = 0;
  options.allocator = &alloc.allocator;

  status = lonejson_parse_cstr(&test_person_map, &person, json_a, &options,
                               &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(person.tags.count == 3u);
  EXPECT(alloc.stats.alloc_calls > 0u);
  EXPECT(alloc.stats.bytes_live > 0u);

  status = lonejson_parse_cstr(&test_person_map, &person, json_b, &options,
                               &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(person.tags.count == 1u);
  EXPECT(strcmp(person.tags.items[0], "fresh") == 0);

  lonejson_cleanup(&test_person_map, &person);
  EXPECT(alloc.stats.bytes_live == 0u);
}

static void test_parse_buffer_basic(void) {
  const char *json = "{"
                     "\"name\":\"Alice\","
                     "\"nickname\":\"Wonderland\","
                     "\"age\":34,"
                     "\"score\":99.5,"
                     "\"active\":true,"
                     "\"address\":{\"city\":\"Stockholm\",\"zip\":12345},"
                     "\"lucky_numbers\":[7,11,42],"
                     "\"tags\":[\"admin\",\"ops\"],"
                     "\"items\":[{\"id\":1,\"label\":\"alpha\"},{\"id\":2,"
                     "\"label\":\"bravo-longer-than-fit\"}],"
                     "\"ignored\":{\"nested\":true}"
                     "}";
  test_person person;
  lonejson_error error;
  lonejson_status status;

  status = lonejson_parse_cstr(&test_person_map, &person, json, NULL, &error);

  EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
  EXPECT(person.name != NULL && strcmp(person.name, "Alice") == 0);
  EXPECT(strcmp(person.nickname, "Wonderl") == 0);
  EXPECT(person.age == 34);
  EXPECT(person.score > 99.4 && person.score < 99.6);
  EXPECT(person.active == true);
  EXPECT(strcmp(person.address.city, "Stockholm") == 0);
  EXPECT(person.address.zip == 12345);
  EXPECT(person.lucky_numbers.count == 3);
  EXPECT(person.lucky_numbers.items[0] == 7);
  EXPECT(person.tags.count == 2);
  EXPECT(strcmp(person.tags.items[1], "ops") == 0);
  EXPECT(person.items.count == 2);
  EXPECT(((test_item *)person.items.items)[0].id == 1);
  EXPECT(strcmp(((test_item *)person.items.items)[1].label, "bravo-longe") ==
         0);

  lonejson_cleanup(&test_person_map, &person);
}

static void test_parse_fixed_strings_with_escapes_and_truncation(void) {
  const char *json = "{"
                     "\"name\":\"Alice\","
                     "\"nickname\":\"AB\\nCDEFG\","
                     "\"age\":1,"
                     "\"score\":1.0,"
                     "\"active\":false,"
                     "\"address\":{\"city\":\"M\\u00f6lle\",\"zip\":7},"
                     "\"lucky_numbers\":[],"
                     "\"tags\":[],"
                     "\"items\":[]"
                     "}";
  test_person person;
  lonejson_error error;
  lonejson_status status;

  status = lonejson_parse_cstr(&test_person_map, &person, json, NULL, &error);

  EXPECT(status == LONEJSON_STATUS_TRUNCATED);
  EXPECT(person.name != NULL && strcmp(person.name, "Alice") == 0);
  EXPECT(strcmp(person.nickname, "AB\nCDEF") == 0);
  EXPECT(strcmp(person.address.city, "M\303\266lle") == 0);
  EXPECT(person.address.zip == 7);

  lonejson_cleanup(&test_person_map, &person);
}

static void test_chunked_parser_and_missing_required(void) {
  test_reader_state state = {
      "{\"name\":\"Bob\",\"address\":{\"city\":\"Uppsala\",\"zip\":77},"
      "\"active\":false,\"lucky_numbers\":[1,2],\"tags\":[],\"items\":[]}",
      0u, 9u};
  test_person person;
  lonejson_stream *stream;
  lonejson_error error;
  lonejson_status status;
  lonejson_stream_result result;

  poison_bytes(&person, sizeof(person), 0x5Au);
  stream = lonejson_stream_open_reader(&test_person_map, test_state_reader,
                                       &state, NULL, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    return;
  }

  result = lonejson_stream_next(stream, &person, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(person.name != NULL && strcmp(person.name, "Bob") == 0);
  EXPECT(strcmp(person.address.city, "Uppsala") == 0);
  lonejson_stream_close(stream);
  lonejson_cleanup(&test_person_map, &person);

  poison_bytes(&person, sizeof(person), 0x5Au);
  status = lonejson_parse_cstr(&test_person_map, &person, "{\"age\":1}", NULL,
                               &error);
  EXPECT(status == LONEJSON_STATUS_MISSING_REQUIRED_FIELD);
  lonejson_cleanup(&test_person_map, &person);
}

static void test_object_framed_stream_jsonl(void) {
  test_reader_state state = {
      " \n{\"name\":\"One\",\"address\":{\"city\":\"A\",\"zip\":1},"
      "\"active\":true,\"lucky_numbers\":[],\"tags\":[],\"items\":[]}"
      "{\"name\":\"Two\",\"address\":{\"city\":\"B\",\"zip\":2},"
      "\"active\":false,\"lucky_numbers\":[],\"tags\":[],\"items\":[]}\n"
      "{\n  \"name\":\"Three\",\n  \"address\":{\"city\":\"C\",\"zip\":3},\n"
      "  \"active\":true,\n  \"lucky_numbers\":[],\n  \"tags\":[],\n"
      "  \"items\":[]\n}\n",
      0u, 7u};
  test_person person;
  lonejson_stream *stream;
  lonejson_error error;
  lonejson_stream_result result;

  stream = lonejson_stream_open_reader(&test_person_map, test_framed_reader,
                                       &state, NULL, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    return;
  }

  result = lonejson_stream_next(stream, &person, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(strcmp(person.name, "One") == 0);
  lonejson_cleanup(&test_person_map, &person);

  result = lonejson_stream_next(stream, &person, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(strcmp(person.name, "Two") == 0);
  lonejson_cleanup(&test_person_map, &person);

  result = lonejson_stream_next(stream, &person, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(strcmp(person.name, "Three") == 0);
  lonejson_cleanup(&test_person_map, &person);

  result = lonejson_stream_next(stream, &person, &error);
  EXPECT(result == LONEJSON_STREAM_EOF);
  lonejson_stream_close(stream);
}

static void test_stream_reuses_and_changes_destination(void) {
  test_reader_state state = {
      "{\"name\":\"One\",\"address\":{\"city\":\"A\",\"zip\":1},"
      "\"active\":true,\"lucky_numbers\":[],\"tags\":[],\"items\":[]}"
      "{\"name\":\"Two\",\"address\":{\"city\":\"B\",\"zip\":2},"
      "\"active\":false,\"lucky_numbers\":[],\"tags\":[],\"items\":[]}"
      "{\"name\":\"Three\",\"address\":{\"city\":\"C\",\"zip\":3},"
      "\"active\":true,\"lucky_numbers\":[],\"tags\":[],\"items\":[]}",
      0u, 11u};
  test_person first;
  test_person second;
  lonejson_stream *stream;
  lonejson_error error;
  lonejson_stream_result result;

  poison_bytes(&first, sizeof(first), 0x5Au);
  poison_bytes(&second, sizeof(second), 0xA5u);
  stream = lonejson_stream_open_reader(&test_person_map, test_framed_reader,
                                       &state, NULL, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    return;
  }

  result = lonejson_stream_next(stream, &first, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(strcmp(first.name, "One") == 0);

  result = lonejson_stream_next(stream, &first, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(strcmp(first.name, "Two") == 0);

  result = lonejson_stream_next(stream, &second, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(strcmp(second.name, "Three") == 0);

  result = lonejson_stream_next(stream, &second, &error);
  EXPECT(result == LONEJSON_STREAM_EOF);

  lonejson_stream_close(stream);
  lonejson_cleanup(&test_person_map, &first);
  lonejson_cleanup(&test_person_map, &second);
}

static void test_object_stream_nonblocking_fd_would_block(void) {
  static const char json[] =
      "{\"name\":\"Fd\",\"address\":{\"city\":\"Pipe\",\"zip\":9},"
      "\"active\":true,\"lucky_numbers\":[],\"tags\":[],\"items\":[]}";
  test_person person;
  lonejson_stream *stream;
  lonejson_error error;
  lonejson_stream_result result;
  int pipe_fds[2];
  int flags;

  if (pipe(pipe_fds) != 0) {
    EXPECT(0);
    return;
  }
  flags = fcntl(pipe_fds[0], F_GETFL, 0);
  EXPECT(flags >= 0);
  if (flags >= 0) {
    EXPECT(fcntl(pipe_fds[0], F_SETFL, flags | O_NONBLOCK) == 0);
  }

  stream = lonejson_stream_open_fd(&test_person_map, pipe_fds[0], NULL, &error);
  EXPECT(stream != NULL);
  if (stream != NULL) {
    poison_bytes(&person, sizeof(person), 0x5Au);
    result = lonejson_stream_next(stream, &person, &error);
    EXPECT(result == LONEJSON_STREAM_WOULD_BLOCK);
    EXPECT(error.code == LONEJSON_STATUS_OK);

    EXPECT(write(pipe_fds[1], json, strlen(json)) == (ssize_t)strlen(json));
    close(pipe_fds[1]);
    pipe_fds[1] = -1;

    result = lonejson_stream_next(stream, &person, &error);
    EXPECT(result == LONEJSON_STREAM_OBJECT);
    if (result == LONEJSON_STREAM_OBJECT) {
      EXPECT(strcmp(person.name, "Fd") == 0);
      EXPECT(strcmp(person.address.city, "Pipe") == 0);
      lonejson_cleanup(&test_person_map, &person);
    }
    result = lonejson_stream_next(stream, &person, &error);
    EXPECT(result == LONEJSON_STREAM_EOF);
    lonejson_stream_close(stream);
  }
  if (pipe_fds[1] >= 0) {
    close(pipe_fds[1]);
  }
  close(pipe_fds[0]);
}

static void
test_stream_reuse_fixed_capacity_string_array_frees_replaced_entries(void) {
  test_reader_state state = {
      "{\"name\":\"One\",\"address\":{\"city\":\"A\",\"zip\":1},"
      "\"tags\":[\"old-a\",\"old-b\"]}"
      "{\"name\":\"Two\",\"address\":{\"city\":\"B\",\"zip\":2},"
      "\"tags\":[\"fresh\"]}",
      0u,
      9u};
  test_person person;
  char *tag_storage[4];
  test_allocator_state alloc;
  lonejson_parse_options options = lonejson_default_parse_options();
  lonejson_stream *stream;
  lonejson_error error;
  lonejson_stream_result result;

  memset(&person, 0, sizeof(person));
  memset(tag_storage, 0, sizeof(tag_storage));
  test_allocator_init(&alloc);
  lonejson_init(&test_person_map, &person);
  person.tags.items = tag_storage;
  person.tags.capacity = 4u;
  person.tags.flags = LONEJSON_ARRAY_FIXED_CAPACITY;
  options.clear_destination = 0;
  options.allocator = &alloc.allocator;

  stream = lonejson_stream_open_reader(&test_person_map, test_framed_reader,
                                       &state, &options, &error);
  EXPECT(stream != NULL);
  if (stream == NULL) {
    lonejson_cleanup(&test_person_map, &person);
    return;
  }

  result = lonejson_stream_next(stream, &person, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(person.tags.count == 2u);
  EXPECT(alloc.stats.alloc_calls > 0u);
  EXPECT(alloc.stats.bytes_live > 0u);

  result = lonejson_stream_next(stream, &person, &error);
  EXPECT(result == LONEJSON_STREAM_OBJECT);
  EXPECT(person.tags.count == 1u);
  EXPECT(strcmp(person.tags.items[0], "fresh") == 0);

  result = lonejson_stream_next(stream, &person, &error);
  EXPECT(result == LONEJSON_STREAM_EOF);

  lonejson_stream_close(stream);
  lonejson_cleanup(&test_person_map, &person);
  EXPECT(alloc.stats.bytes_live == 0u);
}
