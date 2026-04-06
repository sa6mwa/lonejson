typedef enum lonejson__json_pull_pending_kind {
  LONEJSON__JSON_PULL_PENDING_NONE = 0,
  LONEJSON__JSON_PULL_PENDING_BYTES = 1,
  LONEJSON__JSON_PULL_PENDING_REPEAT = 2
} lonejson__json_pull_pending_kind;

typedef enum lonejson__json_pull_frame_kind {
  LONEJSON__JSON_PULL_FRAME_OBJECT = 1,
  LONEJSON__JSON_PULL_FRAME_ARRAY = 2
} lonejson__json_pull_frame_kind;

typedef enum lonejson__json_pull_frame_phase {
  LONEJSON__JSON_PULL_OBJECT_KEY_OR_END = 1,
  LONEJSON__JSON_PULL_OBJECT_PRE_MEMBER_NL = 2,
  LONEJSON__JSON_PULL_OBJECT_PRE_MEMBER_INDENT = 3,
  LONEJSON__JSON_PULL_OBJECT_KEY = 4,
  LONEJSON__JSON_PULL_OBJECT_COLON = 5,
  LONEJSON__JSON_PULL_OBJECT_VALUE = 6,
  LONEJSON__JSON_PULL_OBJECT_COMMA_OR_END = 7,
  LONEJSON__JSON_PULL_OBJECT_PRE_CLOSE_NL = 8,
  LONEJSON__JSON_PULL_OBJECT_PRE_CLOSE_INDENT = 9,
  LONEJSON__JSON_PULL_OBJECT_CLOSE = 10,
  LONEJSON__JSON_PULL_ARRAY_VALUE_OR_END = 11,
  LONEJSON__JSON_PULL_ARRAY_PRE_VALUE_NL = 12,
  LONEJSON__JSON_PULL_ARRAY_PRE_VALUE_INDENT = 13,
  LONEJSON__JSON_PULL_ARRAY_VALUE = 14,
  LONEJSON__JSON_PULL_ARRAY_COMMA_OR_END = 15,
  LONEJSON__JSON_PULL_ARRAY_PRE_CLOSE_NL = 16,
  LONEJSON__JSON_PULL_ARRAY_PRE_CLOSE_INDENT = 17,
  LONEJSON__JSON_PULL_ARRAY_CLOSE = 18
} lonejson__json_pull_frame_phase;

typedef struct lonejson__json_pull_frame {
  lonejson__json_pull_frame_kind kind;
  lonejson__json_pull_frame_phase phase;
  size_t depth;
  size_t count;
  size_t aux;
} lonejson__json_pull_frame;

typedef struct lonejson__json_pull_state {
  lonejson__json_cursor cursor;
  lonejson_allocator allocator;
  lonejson_value_limits limits;
  lonejson__json_pull_frame *frames;
  size_t frame_count;
  size_t frame_capacity;
  unsigned char *out;
  size_t out_capacity;
  size_t out_length;
  lonejson__json_pull_pending_kind pending_kind;
  const unsigned char *pending_ptr;
  size_t pending_len;
  size_t pending_off;
  unsigned char pending_repeat_byte;
  size_t pending_repeat_count;
  int pretty;
  size_t base_depth;
  size_t total_bytes;
  int initialized;
  int finished;
  int root_started;
  int root_finished;
  int synthetic_null;
  lonejson_lex_mode lex_mode;
  int lex_is_key;
  int lex_escape;
  lonejson_uint32 unicode_pending_high;
  lonejson_uint32 unicode_accum;
  int unicode_digits_needed;
  int unicode_expect_low_slash;
  int unicode_expect_low_u;
  int unicode_parsing_low;
  char number_buf[256];
  size_t number_len;
  char literal_buf[8];
  unsigned char scratch[8];
  const char *literal_rest;
  size_t literal_pos;
  size_t literal_len;
  int has_pushback;
  int pushback;
  lonejson_error error;
} lonejson__json_pull_state;

static lonejson_status lonejson__json_pull_set_pending_bytes(
    lonejson__json_pull_state *state, const void *data, size_t len);
static lonejson_status lonejson__json_pull_set_pending_repeat(
    lonejson__json_pull_state *state, unsigned char byte, size_t count);
static LONEJSON__NOINLINE LONEJSON__COLD lonejson_status lonejson__json_pull_init(
    lonejson__json_pull_state *state, const lonejson_json_value *value,
    int pretty, size_t base_depth, lonejson_error *error);
static LONEJSON__NOINLINE LONEJSON__COLD lonejson_status lonejson__json_pull_read(
    lonejson__json_pull_state *state, unsigned char *buffer, size_t capacity,
    size_t *out_len, int *out_eof, lonejson_error *error);
static void lonejson__json_pull_cleanup(lonejson__json_pull_state *state);
