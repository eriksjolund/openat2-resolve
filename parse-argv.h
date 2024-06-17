#ifndef PARSE_ARGV_H
#define PARSE_ARGV_H

#include <stdint.h>

struct Parsed_input {
  char *root_path;
  uint64_t resolve_opts;
};

int parse_argv(int argc, char **argv, struct Parsed_input *parsed_input);

#endif
