#include "openat2-resolve.h"
#include "parse-argv.h"
#include <stdlib.h>

int main(int argc, char **argv) {
  struct Parsed_input parsed_input = {};
  int res = parse_argv(argc, argv, &parsed_input);
  if (res == EXIT_FAILURE) {
    return EXIT_FAILURE;
  }
  return walk_dirs_root(parsed_input.root_path, parsed_input.resolve_opts);
}
