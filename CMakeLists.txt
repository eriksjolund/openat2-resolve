cmake_minimum_required(VERSION 3.28)
project(checkdir
  VERSION
    0.0.1
  DESCRIPTION
    "Test openat2 resolve flags"
  LANGUAGES
    C
)

add_library(libopenat2-resolve STATIC openat2-resolve.c openat2-resolve.h)
add_executable(openat2-resolve main.c parse-argv.c parse-argv.h)
target_link_libraries(openat2-resolve PRIVATE libopenat2-resolve)
