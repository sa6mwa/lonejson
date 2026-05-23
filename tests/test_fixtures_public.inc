static void test_file_and_buffer_helpers(void) {
  const char *json =
      "{\"name\":\"Cara\",\"nickname\":\"cj\",\"age\":1,\"score\":1.5,"
      "\"active\":true,\"address\":{\"city\":\"Lund\",\"zip\":88},\"lucky_"
      "numbers\":[],\"tags\":[],\"items\":[]}";
  static const char expected_pretty[] = "{\n"
                                        "  \"name\": \"Cara\",\n"
                                        "  \"nickname\": \"cj\",\n"
                                        "  \"age\": 1,\n"
                                        "  \"score\": 1.5,\n"
                                        "  \"active\": true,\n"
                                        "  \"address\": {\n"
                                        "    \"city\": \"Lund\",\n"
                                        "    \"zip\": 88\n"
                                        "  },\n"
                                        "  \"lucky_numbers\": [],\n"
                                        "  \"tags\": [],\n"
                                        "  \"items\": []\n"
                                        "}";
  test_person person;
  char path[] = "/tmp/lonejson-test-XXXXXX";
  int fd;
  FILE *fp;
  char out[64];
  char compact[512];
  char pretty[512];
  char tiny[8];
  size_t needed = 0;
  lonejson_error error;
  lonejson_write_options write_options = lonejson_default_write_options();
  lonejson_status status;

  fd = mkstemp(path);
  EXPECT(fd >= 0);
  if (fd < 0) {
    return;
  }
  EXPECT(write(fd, json, strlen(json)) == (ssize_t)strlen(json));
  close(fd);

  status = lonejson_parse_path(&test_person_map, &person, path, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(person.address.city, "Lund") == 0);

  status = lonejson_serialize_buffer(&test_person_map, &person, compact,
                                     sizeof(compact), &needed, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(compact, json) == 0);
  EXPECT(needed == strlen(json));

  status = lonejson_serialize_buffer(&test_person_map, &person, tiny,
                                     sizeof(tiny), &needed, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OVERFLOW);
  EXPECT(error.code == LONEJSON_STATUS_OVERFLOW);
  EXPECT(tiny[sizeof(tiny) - 1u] == '\0');

  write_options.overflow_policy = LONEJSON_OVERFLOW_TRUNCATE;
  status =
      lonejson_serialize_buffer(&test_person_map, &person, out, sizeof(out),
                                &needed, &write_options, &error);
  EXPECT(status == LONEJSON_STATUS_TRUNCATED || status == LONEJSON_STATUS_OK);
  EXPECT(needed > strlen(out));

  write_options = lonejson_default_write_options();
  write_options.pretty = 1;
  status = lonejson_serialize_buffer(&test_person_map, &person, pretty,
                                     sizeof(pretty), &needed, &write_options,
                                     &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(pretty, expected_pretty) == 0);

  fp = fopen(path, "wb");
  EXPECT(fp != NULL);
  if (fp != NULL) {
    status =
        lonejson_serialize_filep(&test_person_map, &person, fp, NULL, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    fclose(fp);
  }

  unlink(path);
  lonejson_cleanup(&test_person_map, &person);
}

static void test_jsonl_helpers(void) {
  static const char expected_jsonl[] = "{\"id\":\"evt-1\",\"ok\":true}\n"
                                       "{\"id\":\"evt-2\",\"ok\":false}\n";
  static const char expected_pretty_jsonl[] = "{\n"
                                              "  \"id\": \"evt-1\",\n"
                                              "  \"ok\": true\n"
                                              "}\n"
                                              "{\n"
                                              "  \"id\": \"evt-2\",\n"
                                              "  \"ok\": false\n"
                                              "}\n";
  test_event events[2];
  lonejson_error error;
  size_t needed = 0u;
  char buffer[128];
  char pretty[256];
  char *jsonl;
  lonejson_status status;
  char path[] = "/tmp/lonejson-jsonl-XXXXXX";
  int fd;
  FILE *fp;
  char filebuf[128];
  size_t filelen;
  lonejson_write_options write_options = lonejson_default_write_options();

  memset(events, 0, sizeof(events));
  strcpy(events[0].id, "evt-1");
  events[0].ok = true;
  strcpy(events[1].id, "evt-2");
  events[1].ok = false;

  status =
      lonejson_serialize_jsonl_buffer(&test_event_map, events, 2u, 0u, buffer,
                                      sizeof(buffer), &needed, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, expected_jsonl) == 0);
  EXPECT(needed == strlen(buffer));

  write_options.pretty = 1;
  status = lonejson_serialize_jsonl_buffer(&test_event_map, events, 2u, 0u,
                                           pretty, sizeof(pretty), &needed,
                                           &write_options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(pretty, expected_pretty_jsonl) == 0);
  EXPECT(needed == strlen(pretty));

  jsonl = lonejson_serialize_jsonl_alloc(&test_event_map, events, 2u, 0u, NULL,
                                         NULL, &error);
  EXPECT(jsonl != NULL);
  if (jsonl != NULL) {
    EXPECT(strcmp(jsonl, buffer) == 0);
    LONEJSON_FREE(jsonl);
  }

  status =
      lonejson_serialize_jsonl_buffer(&test_event_map, NULL, 0u, 0u, buffer,
                                      sizeof(buffer), &needed, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(buffer, "") == 0);
  EXPECT(needed == 0u);

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
  status = lonejson_serialize_jsonl_filep(&test_event_map, events, 2u, 0u, fp,
                                          NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  fclose(fp);

  fp = fopen(path, "rb");
  EXPECT(fp != NULL);
  if (fp != NULL) {
    filelen = fread(filebuf, 1u, sizeof(filebuf) - 1u, fp);
    filebuf[filelen] = '\0';
    fclose(fp);
    EXPECT(strcmp(filebuf, expected_jsonl) == 0);
  }
  unlink(path);
}

static void test_fixed_capacity_object_array(void) {
  const char *json =
      "{\"name\":\"Dee\",\"nickname\":\"dee\",\"age\":1,\"score\":2.0,"
      "\"active\":false,\"address\":{\"city\":\"Malmo\",\"zip\":1},\"lucky_"
      "numbers\":[],\"tags\":[],\"items\":[{\"id\":1,\"label\":\"one\"},{"
      "\"id\":2,\"label\":\"two\"}]}";
  test_person person;
  test_item items_buf[2];
  lonejson_error error;
  lonejson_status status;
  lonejson_parse_options options = lonejson_default_parse_options();

  memset(&person, 0, sizeof(person));
  lonejson_init(&test_person_map, &person);
  memset(items_buf, 0, sizeof(items_buf));
  person.items.items = items_buf;
  person.items.capacity = 2;
  person.items.elem_size = sizeof(items_buf[0]);
  person.items.flags = LONEJSON_ARRAY_FIXED_CAPACITY;
  options.clear_destination = 0;

  status =
      lonejson_parse_cstr(&test_person_map, &person, json, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(person.items.count == 2);
  EXPECT(person.items.items == items_buf);
  EXPECT(items_buf[0].id == 1);
  EXPECT(items_buf[1].id == 2);

  lonejson_cleanup(&test_person_map, &person);
}

static void test_formatting_variants_and_roundtrip(void) {
  const char *json =
      "{\n"
      "\t\"name\" : \"E\\u006dma\",\n"
      "\t\"nickname\" : \"em\",\n"
      "\t\"age\" : -12,\n"
      "\t\"score\" : 1.25e2,\n"
      "\t\"active\" : false,\n"
      "\t\"address\" : {\n"
      "\t  \"city\" : \"G\\u00f6teborg\",\n"
      "\t  \"zip\" : 90210\n"
      "\t},\n"
      "\t\"lucky_numbers\" : [ 0, -1, 200 ],\n"
      "\t\"tags\" : [ \"line\\nfeed\", \"quote\\\"ok\" ],\n"
      "\t\"items\" : [ ],\n"
      "\t\"ignored\" : { \"deep\" : [1, {\"x\": [2,3,{\"y\":4}]}, 5] }\n"
      "}\n";
  test_person person;
  test_person roundtrip;
  lonejson_error error;
  lonejson_status status;
  char *serialized = NULL;

  status = lonejson_parse_cstr(&test_person_map, &person, json, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(person.name, "Emma") == 0);
  EXPECT(strcmp(person.address.city, "Göteborg") == 0);
  EXPECT(person.age == -12);
  EXPECT(person.score > 124.9 && person.score < 125.1);
  EXPECT(person.tags.count == 2);
  EXPECT(strcmp(person.tags.items[0], "line\nfeed") == 0);
  EXPECT(strcmp(person.tags.items[1], "quote\"ok") == 0);

  serialized =
      lonejson_serialize_alloc(&test_person_map, &person, NULL, NULL, &error);
  EXPECT(serialized != NULL);
  if (serialized != NULL) {
    status = lonejson_parse_cstr(&test_person_map, &roundtrip, serialized, NULL,
                                 &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    EXPECT(strcmp(roundtrip.name, "Emma") == 0);
    EXPECT(strcmp(roundtrip.address.city, "Göteborg") == 0);
    LONEJSON_FREE(serialized);
  }

  lonejson_cleanup(&test_person_map, &person);
  lonejson_cleanup(&test_person_map, &roundtrip);
}

static void test_duplicate_key_policy(void) {
  const char *json =
      "{\"name\":\"first\",\"name\":\"second\",\"address\":{\"city\":\"A\","
      "\"zip\":1},\"nickname\":\"n\",\"age\":1,\"score\":1,\"active\":true,"
      "\"lucky_numbers\":[],\"tags\":[],\"items\":[]}";
  test_person person;
  lonejson_parse_options options = lonejson_default_parse_options();
  lonejson_error error;
  lonejson_status status;

  status =
      lonejson_parse_cstr(&test_person_map, &person, json, &options, &error);
  EXPECT(status == LONEJSON_STATUS_DUPLICATE_FIELD);
  lonejson_cleanup(&test_person_map, &person);

  poison_bytes(&person, sizeof(person), 0xC3u);
  options.reject_duplicate_keys = 0;
  status =
      lonejson_parse_cstr(&test_person_map, &person, json, &options, &error);
  EXPECT(status == LONEJSON_STATUS_OK);
  EXPECT(strcmp(person.name, "second") == 0);
  lonejson_cleanup(&test_person_map, &person);
}

static void test_public_api_argument_and_serialization_guards(void) {
  static const char *json = "{\"name\":\"Alice\",\"age\":34}";
  static const char *json_with_nul =
      "{\"name\":\"a\\u0000b\",\"age\":34,\"active\":true,"
      "\"address\":{\"city\":\"X\",\"zip\":1},\"lucky_numbers\":[],"
      "\"tags\":[],\"items\":[]}";
  test_person person;
  lonejson_error error;
  lonejson_status status;
  char *serialized;

  status = lonejson_parse_cstr(&test_person_map, NULL, json, NULL, &error);
  EXPECT(status == LONEJSON_STATUS_INVALID_ARGUMENT);

  status = lonejson_parse_cstr(&test_person_map, &person, json_with_nul, NULL,
                               &error);
  EXPECT(status != LONEJSON_STATUS_OK);
  lonejson_cleanup(&test_person_map, &person);

  lonejson_init(&test_person_map, &person);
  person.name = "finite-check";
  person.score = strtod("nan", NULL);
  serialized =
      lonejson_serialize_alloc(&test_person_map, &person, NULL, NULL, &error);
  EXPECT(serialized == NULL);
  person.score = strtod("inf", NULL);
  serialized =
      lonejson_serialize_alloc(&test_person_map, &person, NULL, NULL, &error);
  EXPECT(serialized == NULL);
}

static void test_public_initializers_and_defaults(void) {
  lonejson_error error;
  lonejson_read_result read_result;
  lonejson_value_visitor visitor;
  lonejson_parse_options parse_options;
  lonejson_write_options write_options;
  lonejson_value_limits value_limits;
  test_json_value_doc doc;

  poison_bytes(&error, sizeof(error), 0xA5u);
  lonejson_error_init(&error);
  EXPECT(error.code == LONEJSON_STATUS_OK);
  EXPECT(error.line == 0u);
  EXPECT(error.column == 0u);
  EXPECT(error.offset == 0u);
  EXPECT(error.system_errno == 0);
  EXPECT(error.truncated == 0);
  EXPECT(error.message[0] == '\0');

  read_result = lonejson_default_read_result();
  EXPECT(read_result.bytes_read == 0u);
  EXPECT(read_result.eof == 0);
  EXPECT(read_result.would_block == 0);
  EXPECT(read_result.error_code == 0);

  visitor = lonejson_default_value_visitor();
  EXPECT(visitor.object_begin == NULL);
  EXPECT(visitor.object_end == NULL);
  EXPECT(visitor.object_key_begin == NULL);
  EXPECT(visitor.object_key_chunk == NULL);
  EXPECT(visitor.object_key_end == NULL);
  EXPECT(visitor.array_begin == NULL);
  EXPECT(visitor.array_end == NULL);
  EXPECT(visitor.string_begin == NULL);
  EXPECT(visitor.string_chunk == NULL);
  EXPECT(visitor.string_end == NULL);
  EXPECT(visitor.number_begin == NULL);
  EXPECT(visitor.number_chunk == NULL);
  EXPECT(visitor.number_end == NULL);
  EXPECT(visitor.boolean_value == NULL);
  EXPECT(visitor.null_value == NULL);

  parse_options = lonejson_default_parse_options();
  EXPECT(parse_options.clear_destination == 1);
  EXPECT(parse_options.reject_duplicate_keys == 1);
  EXPECT(parse_options.max_depth == 64u);
  EXPECT(parse_options.max_dynamic_string_bytes ==
         LONEJSON_PARSE_MAX_DYNAMIC_STRING_BYTES);
  EXPECT(parse_options.max_alloc_bytes == LONEJSON_PARSE_MAX_ALLOC_BYTES);

  {
    lonejson_spool_options spool_options = lonejson_default_spool_options();

    EXPECT(spool_options.memory_limit == LONEJSON_SPOOL_MEMORY_LIMIT);
    EXPECT(spool_options.max_bytes == LONEJSON_SPOOL_MAX_BYTES);
    EXPECT(spool_options.temp_dir == NULL);
  }

  write_options = lonejson_default_write_options();
  EXPECT(write_options.overflow_policy == LONEJSON_OVERFLOW_FAIL);
  EXPECT(write_options.pretty == 0);

  EXPECT((int)LONEJSON_FIELD_KIND_STRING == 0);
  EXPECT((int)LONEJSON_FIELD_KIND_STRING_STREAM == 1);
  EXPECT((int)LONEJSON_FIELD_KIND_BASE64_STREAM == 2);
  EXPECT((int)LONEJSON_FIELD_KIND_STRING_SOURCE == 3);
  EXPECT((int)LONEJSON_FIELD_KIND_BASE64_SOURCE == 4);
  EXPECT((int)LONEJSON_FIELD_KIND_JSON_VALUE == 5);
  EXPECT((int)LONEJSON_FIELD_KIND_I64 == 6);
  EXPECT((int)LONEJSON_FIELD_KIND_U64 == 7);
  EXPECT((int)LONEJSON_FIELD_KIND_F64 == 8);
  EXPECT((int)LONEJSON_FIELD_KIND_BOOL == 9);
  EXPECT((int)LONEJSON_FIELD_KIND_OBJECT == 10);
  EXPECT((int)LONEJSON_FIELD_KIND_STRING_ARRAY == 11);
  EXPECT((int)LONEJSON_FIELD_KIND_I64_ARRAY == 12);
  EXPECT((int)LONEJSON_FIELD_KIND_U64_ARRAY == 13);
  EXPECT((int)LONEJSON_FIELD_KIND_F64_ARRAY == 14);
  EXPECT((int)LONEJSON_FIELD_KIND_BOOL_ARRAY == 15);
  EXPECT((int)LONEJSON_FIELD_KIND_OBJECT_ARRAY == 16);
  EXPECT((int)LONEJSON_FIELD_KIND_STRING_ARRAY_STREAM == 17);

  value_limits = lonejson_default_value_limits();

  poison_bytes(&doc, sizeof(doc), 0x5Au);
  lonejson_init(&test_json_value_doc_map, &doc);
  EXPECT(doc.id[0] == '\0');
  EXPECT(doc.selector.kind == LONEJSON_JSON_VALUE_NULL);
  EXPECT(doc.selector.json == NULL);
  EXPECT(doc.selector.len == 0u);
  EXPECT(doc.selector.reader == NULL);
  EXPECT(doc.selector.reader_user == NULL);
  EXPECT(doc.selector.fp == NULL);
  EXPECT(doc.selector.fd == -1);
  EXPECT(doc.selector.path == NULL);
  EXPECT(doc.selector.parse_mode == LONEJSON_JSON_VALUE_PARSE_NONE);
  EXPECT(doc.selector.parse_sink == NULL);
  EXPECT(doc.selector.parse_sink_user == NULL);
  EXPECT(doc.selector.parse_visitor == NULL);
  EXPECT(doc.selector.parse_visitor_user == NULL);
  EXPECT(doc.selector.parse_visitor_limits.max_depth == value_limits.max_depth);
  EXPECT(doc.selector.parse_visitor_limits.max_string_bytes ==
         value_limits.max_string_bytes);
  EXPECT(doc.selector.parse_visitor_limits.max_number_bytes ==
         value_limits.max_number_bytes);
  EXPECT(doc.selector.parse_visitor_limits.max_key_bytes ==
         value_limits.max_key_bytes);
  EXPECT(doc.selector.parse_visitor_limits.max_total_bytes ==
         value_limits.max_total_bytes);
}

static void test_invalid_json_vectors(void) {
  static const char *invalid_cases[] = {
      "spec/invalid_extra_trailing_garbage.json",
      "spec/invalid_leading_zero.json",
      "spec/invalid_fraction_without_digit.json",
      "spec/invalid_exponent_without_digit.json",
      "spec/invalid_plus_sign.json",
      "spec/invalid_double_comma.json",
      "spec/invalid_array_trailing_comma.json",
      "spec/invalid_object_trailing_comma.json",
      "spec/invalid_missing_colon.json",
      "spec/invalid_missing_value.json",
      "spec/invalid_bad_literal.json",
      "spec/invalid_raw_control_char_in_string.json",
      "spec/invalid_bad_escape.json",
      "spec/invalid_bad_unicode.json",
      "spec/invalid_unterminated_array.json",
      "spec/invalid_unterminated_object.json",
      "spec/invalid_unterminated_string.json"};
  size_t i;

  for (i = 0; i < sizeof(invalid_cases) / sizeof(invalid_cases[0]); ++i) {
    char *path = fixture_path(invalid_cases[i]);
    test_person person;
    lonejson_error error;
    lonejson_status status;

    EXPECT(path != NULL);
    if (path == NULL) {
      return;
    }
    lonejson_init(&test_person_map, &person);
    status = lonejson_parse_path(&test_person_map, &person, path, NULL, &error);
    EXPECT(status != LONEJSON_STATUS_OK);
    free(path);
    lonejson_cleanup(&test_person_map, &person);
  }
}

static void assert_fixture_person(const test_person *person) {
  EXPECT(person->name != NULL);
  if (person->name == NULL) {
    return;
  }
  EXPECT(strncmp(person->name, "Fixture Person Example", 22) == 0);
  EXPECT(strcmp(person->nickname, "fixture") == 0);
  EXPECT(person->age == 57);
  EXPECT(person->score > 4321.0 && person->score < 4321.2);
  EXPECT(person->active == true);
  EXPECT(strcmp(person->address.city, "Stockholm") == 0);
  EXPECT(person->address.zip == 11223344);
  EXPECT(person->lucky_numbers.count == 96);
  if (person->lucky_numbers.count >= 96 &&
      person->lucky_numbers.items != NULL) {
    EXPECT(person->lucky_numbers.items[0] == 1);
    EXPECT(person->lucky_numbers.items[11] == 233);
    EXPECT(person->lucky_numbers.items[95] == 233);
  }
  EXPECT(person->tags.count == 10);
  if (person->tags.count >= 10 && person->tags.items != NULL) {
    EXPECT(strcmp(person->tags.items[0], "alpha") == 0);
    EXPECT(strcmp(person->tags.items[9], "kappa") == 0);
  }
  EXPECT(person->items.count == 5);
  if (person->items.count >= 5 && person->items.items != NULL) {
    EXPECT(((test_item *)person->items.items)[0].id == 101);
    EXPECT(((test_item *)person->items.items)[4].id == 505);
    EXPECT(strcmp(((test_item *)person->items.items)[4].label, "fifth-entry") ==
           0);
  }
}

static void test_fixture_documents(void) {
  static const char *fixture_names[] = {
      "profile_compact.json", "profile_pretty.json", "profile_custom.json"};
  size_t i;

  for (i = 0; i < sizeof(fixture_names) / sizeof(fixture_names[0]); ++i) {
    char *path = generated_fixture_path(fixture_names[i]);
    test_person person;
    lonejson_error error;
    lonejson_status status;

    EXPECT(path != NULL);
    if (path == NULL) {
      return;
    }
    status = lonejson_parse_path(&test_person_map, &person, path, NULL, &error);
    EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
    assert_fixture_person(&person);
    free(path);
    lonejson_cleanup(&test_person_map, &person);
  }
}

static void test_large_generated_fixture_sizes(void) {
  static const struct {
    const char *name;
    long min_bytes;
  } cases[] = {
      {"large_object_compact.json", 22L * 1024L * 1024L},
      {"large_object_pretty.json", 16L * 1024L * 1024L},
      {"large_array_objects.json", 18L * 1024L * 1024L},
      {"large_base64_payloads.json", 16L * 1024L * 1024L},
      {"large_stream.jsonl", 18L * 1024L * 1024L},
  };
  size_t i;
  long total_bytes;

  total_bytes = 0L;
  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    char *path = generated_fixture_path(cases[i].name);
    struct stat st;

    EXPECT(path != NULL);
    if (path == NULL) {
      return;
    }
    EXPECT(stat(path, &st) == 0);
    if (stat(path, &st) == 0) {
      EXPECT((long)st.st_size >= cases[i].min_bytes);
      total_bytes += (long)st.st_size;
    }
    free(path);
  }
  EXPECT(total_bytes >= 95L * 1024L * 1024L);
  EXPECT(total_bytes <= 110L * 1024L * 1024L);
}

static void test_large_generated_fixture_parsing(void) {
  static const char *object_fixture_names[] = {
      "large_object_compact.json", "large_object_pretty.json",
      "large_array_objects.json", "large_base64_payloads.json"};
  size_t i;

  for (i = 0;
       i < sizeof(object_fixture_names) / sizeof(object_fixture_names[0]);
       ++i) {
    char *path = generated_fixture_path(object_fixture_names[i]);
    lonejson_error error;
    lonejson_status status;

    EXPECT(path != NULL);
    if (path == NULL) {
      return;
    }
    status = lonejson_validate_path(path, &error);
    EXPECT(status == LONEJSON_STATUS_OK);
    free(path);
  }

  {
    char *path = generated_fixture_path("large_stream.jsonl");
    lonejson_stream *stream;
    lonejson_stream_result result;
    lonejson_error error;
    size_t seen;
    test_large_stream_record record;

    EXPECT(path != NULL);
    if (path == NULL) {
      return;
    }
    stream = lonejson_stream_open_path(&test_large_stream_record_map, path,
                                       NULL, &error);
    EXPECT(stream != NULL);
    if (stream == NULL) {
      free(path);
      return;
    }
    seen = 0u;
    for (;;) {
      result = lonejson_stream_next(stream, &record, &error);
      if (result == LONEJSON_STREAM_EOF) {
        break;
      }
      EXPECT(result == LONEJSON_STREAM_OBJECT);
      if (result != LONEJSON_STREAM_OBJECT) {
        break;
      }
      EXPECT(record.id > 0);
      EXPECT(strncmp(record.name, "stream-", 7) == 0 ||
             strcmp(record.name, "tail") == 0);
      EXPECT(strncmp(record.kind, "jsonl", 5) == 0 || record.id == 999999999);
      EXPECT(record.meta.shard >= 0 || record.id == 999999999);
      ++seen;
    }
    lonejson_stream_close(stream);
    free(path);
    EXPECT(seen > 1000u);
  }
}

static void test_small_valid_spec_fixtures(void) {
  static const char *fixture_names[] = {
      "spec/valid_empty_arrays.json",   "spec/valid_escaped_strings.json",
      "spec/valid_nested_ignored.json", "spec/valid_object_basic.json",
      "spec/valid_object_pretty.json",  "spec/valid_unicode_escapes.json"};
  size_t i;

  for (i = 0; i < sizeof(fixture_names) / sizeof(fixture_names[0]); ++i) {
    char *path = fixture_path(fixture_names[i]);
    test_person person;
    lonejson_error error;
    lonejson_status status;

    EXPECT(path != NULL);
    if (path == NULL) {
      return;
    }
    status = lonejson_parse_path(&test_person_map, &person, path, NULL, &error);
    EXPECT(status == LONEJSON_STATUS_OK || status == LONEJSON_STATUS_TRUNCATED);
    free(path);
    lonejson_cleanup(&test_person_map, &person);
  }
}

static void test_json_test_suite_parsing(void) {
  const char *dir_path =
      LONEJSON_SOURCE_DIR "/tests/fixtures/vendor/json_test_suite/test_parsing";
  DIR *dir;
  struct dirent *entry;
  size_t y_count = 0u;
  size_t n_count = 0u;
  size_t i_count = 0u;
  size_t i_accept = 0u;
  size_t i_reject = 0u;

  dir = opendir(dir_path);
  EXPECT(dir != NULL);
  if (dir == NULL) {
    return;
  }

  while ((entry = readdir(dir)) != NULL) {
    char *path;
    lonejson_error error;
    lonejson_status status;

    if (entry->d_name[0] == '.') {
      continue;
    }
    path = vendor_fixture_path(entry->d_name);
    EXPECT(path != NULL);
    if (path == NULL) {
      closedir(dir);
      return;
    }

    status = lonejson_validate_path(path, &error);
    if (strncmp(entry->d_name, "y_", 2u) == 0) {
      ++y_count;
      EXPECT(status == LONEJSON_STATUS_OK);
    } else if (strncmp(entry->d_name, "n_", 2u) == 0) {
      ++n_count;
      EXPECT(status != LONEJSON_STATUS_OK);
    } else if (strncmp(entry->d_name, "i_", 2u) == 0) {
      ++i_count;
      if (status == LONEJSON_STATUS_OK) {
        ++i_accept;
      } else {
        ++i_reject;
      }
    } else {
      EXPECT(0);
    }
    free(path);
  }

  closedir(dir);
  EXPECT(y_count == 95u);
  EXPECT(n_count == 188u);
  EXPECT(i_count == 35u);
  EXPECT(i_accept + i_reject == i_count);
}
