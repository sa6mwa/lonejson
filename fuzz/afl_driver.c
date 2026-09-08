#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>

int lonejson_fuzz_one_input(const uint8_t *data, size_t size);

int main(int argc, char **argv) {
  FILE *input = stdin;
  unsigned char *data;
  long length;
  size_t read_count;

  /* Applied in each child, before reading or executing a fuzz input. Unlike
   * RLIMIT_CORE, this also prevents piped system crash collectors from running.
   * Fail closed if the host cannot provide this process-local isolation. */
  if (prctl(PR_SET_DUMPABLE, 0L, 0L, 0L, 0L) != 0 ||
      prctl(PR_GET_DUMPABLE, 0L, 0L, 0L, 0L) != 0) {
    fputs("cannot disable core dumps for fuzz input execution\n", stderr);
    abort();
  }
  if (argc == 2 && strcmp(argv[1], "--check-fuzz-isolation") == 0) {
    puts("lonejson-fuzz-no-core-v1");
    return 0;
  }

  if (argc == 2) {
    input = fopen(argv[1], "rb");
    if (input == NULL) {
      return errno == 0 ? 1 : errno;
    }
  } else if (argc != 1) {
    return 1;
  }
  if (fseek(input, 0L, SEEK_END) != 0 || (length = ftell(input)) < 0 ||
      fseek(input, 0L, SEEK_SET) != 0 || length > 1048576L) {
    if (input != stdin) {
      fclose(input);
    }
    return 1;
  }
  data = (unsigned char *)malloc(length == 0 ? 1u : (size_t)length);
  if (data == NULL) {
    if (input != stdin) {
      fclose(input);
    }
    return 1;
  }
  read_count = fread(data, 1u, (size_t)length, input);
  if (input != stdin) {
    fclose(input);
  }
  if (read_count != (size_t)length) {
    free(data);
    return 1;
  }
  (void)lonejson_fuzz_one_input(data, (size_t)length);
  free(data);
  return 0;
}
