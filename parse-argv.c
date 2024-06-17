#include "parse-argv.h"
#include <linux/openat2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define FLAGS                                                                  \
  X(RESOLVE_BENEATH)                                                           \
  X(RESOLVE_CACHED)                                                            \
  X(RESOLVE_NO_XDEV)                                                           \
  X(RESOLVE_NO_SYMLINKS)                                                       \
  X(RESOLVE_NO_MAGICLINKS)                                                     \
  X(RESOLVE_IN_ROOT)

int openat2_flag_int[] = {
#define X(N) N,
    FLAGS
#undef X
};

char const *const openat2_flag_string[] = {
#define X(N) #N,
    FLAGS
#undef X
};

static int lookup_resolve_flag(char *str) {
  int num_flags = (int)(sizeof(openat2_flag_int) / sizeof(openat2_flag_int[0]));
  for (int i = 0; i < num_flags; i++) {
    if (strcmp(str, openat2_flag_string[i]) == 0) {
      return openat2_flag_int[i];
    }
  }
  return 0; // lookup failed
}

int parse_argv(int argc, char **argv, struct Parsed_input *parsed_input) {
  if (argc < 2) {
    fprintf(stderr, "error: at least one argument is required\n");
    return EXIT_FAILURE;
  }
  parsed_input->root_path = argv[1];
  if (strlen(parsed_input->root_path) == 0) {
    fprintf(stderr, "error: root_path argument is an empty string\n");
    return EXIT_FAILURE;
  }
  int num_flags = (int)(sizeof(openat2_flag_int) / sizeof(openat2_flag_int[0]));
  for (int i = 2; i < argc; i++) {
    int resolve_flag = lookup_resolve_flag(argv[i]);
    if (resolve_flag == 0) {
      fprintf(stderr, "error: invalid resolve flag: %s\n", argv[i]);
      return EXIT_FAILURE;
    }
    parsed_input->resolve_opts |= resolve_flag;
  }
  return EXIT_SUCCESS;
}
