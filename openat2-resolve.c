#define _GNU_SOURCE // for O_PATH
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <linux/openat2.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#include <unistd.h>

#define auto_close __attribute__((cleanup(close_wrapper)))
#define auto_closedir __attribute__((cleanup(closedir_wrapper)))

static inline void closedir_wrapper(DIR **p) {
  DIR *dir = *p;
  if (dir)
    closedir(dir);
}

static inline void close_wrapper(int *fd_p) {
  if (*fd_p >= 0)
    close(*fd_p);
}

struct dirlist_s {
  char *name;
  struct dirlist_s *next;
};

typedef struct dirlist_s dirlist_t;

static inline int openat2_with_retry(int dir_fd, char *name,
                                     struct open_how *how, size_t size) {
  int fd = -1;
  while (fd < 0) {
    fd = (int)syscall(__NR_openat2, dir_fd, name, how, size);
    if (fd < 0 && errno != EAGAIN && errno != EINTR) {
      break;
    }
  }
  return fd;
}

static int walk_dirs(int rootdir_fd, DIR *current_p, dirlist_t *dirlist_ptr,
                     uint64_t resolve_opts);

static char *join_path(char *buf, int bufsize, char *filename,
                       char **dir_components, int num_components) {
  buf[0] = '\0';
  int bytes_to_write = 0;

  for (int i = num_components - 1; i >= 0; --i) {
    bytes_to_write += strlen(dir_components[i]);
    bytes_to_write += 1;
  }
  bytes_to_write += strlen(filename);
  bytes_to_write += 1; // NUL terminator

  if (bytes_to_write > bufsize) {
    return NULL;
  }
  for (int i = num_components - 1; i >= 0; --i) {
    strcat(buf, dir_components[i]);
    strcat(buf, "/");
  }
  strcat(buf, filename);
  return buf;
}

static char *join_dir_components_reversed(char *buf, int bufsize,
                                          char *filename,
                                          dirlist_t *dirlist_ptr) {
  // Join the subdirectories and the filename.
  // dirlist_ptr has the subdirectories in reversed order.
  //
  // For example such list
  //
  // .name = "dir3"
  // .next = -----> .name = "dir2"
  //                .next = ------> .name = "dir1"
  //                                .next = NULL
  //
  // would result in
  //
  // dir1/dir2/dir3/filename

  // TODO: Investigate what value MAX_DIR_DEPTH should be used?
  //       An alternative is to rewrite this to use realloc() instead.

#define MAX_DIR_DEPTH PATH_MAX

  char *dir_component_strings[MAX_DIR_DEPTH];
  int i = 0;
  for (dirlist_t *iter = dirlist_ptr; iter; iter = iter->next) {
    if (i > sizeof(dir_component_strings)) {
      fprintf(stderr,
              "error: traversed too deep into the directory structure\n");
      return NULL;
    }
    dir_component_strings[i] = iter->name;
    ++i;
  }
  char *relative_path =
      join_path(buf, bufsize, filename, dir_component_strings, i);
  return relative_path;
}

static int handle_directory(char *name, int root_fd, int current_fd,
                            dirlist_t *dirlist_ptr, uint64_t resolve_opts) {
  struct open_how how = {.flags = O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW,
                         .resolve = resolve_opts};
  auto_close int dir_fd =
      openat2_with_retry(current_fd, name, &how, sizeof(how));

  if (dir_fd < 0) {
    fprintf(stderr, "error: openat2() failed to open directory = %s\n", name);
    return EXIT_FAILURE;
  }
  auto_closedir DIR *dir_p = fdopendir(dir_fd);
  if (dir_p == NULL) {
    fprintf(stderr, "error: fdopendir() failed\n");
    return EXIT_FAILURE;
  }
  dir_fd = -1; // dir_p now owns the file descriptor
  dirlist_t appended_list = {name, dirlist_ptr};
  int ret = walk_dirs(root_fd, dir_p, &appended_list, resolve_opts);
  return ret;
}

static int handle_symlink(char *name, int root_fd, dirlist_t *dirlist_ptr,
                          uint64_t resolve_opts) {
  char buf[PATH_MAX];
  char *relative_path =
      join_dir_components_reversed(buf, sizeof(buf), name, dirlist_ptr);
  if (relative_path == NULL) {
    fprintf(stderr, "error: constructed relative path too big\n");
    return EXIT_FAILURE;
  }
  struct open_how how = {.flags = O_CLOEXEC | O_PATH, .resolve = resolve_opts};
  auto_close int resolved_fd =
      openat2_with_retry(root_fd, relative_path, &how, sizeof(how));
  if (resolved_fd < 0) {
    fprintf(stderr, "error: openat2() failed to open symlink = %s\n",
            relative_path);
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

static int walk_dirs(int root_fd, DIR *current_p, dirlist_t *dirlist_ptr,
                     uint64_t resolve_opts) {
  struct dirent *dirent_p;
  int current_fd = dirfd(current_p);
  while ((dirent_p = readdir(current_p)) != NULL) {
    char *name = dirent_p->d_name;
    if (name[0] == '.' && name[1] == '\0')
      continue;
    if (name[0] == '.' && name[1] == '.' && name[2] == '\0')
      continue;
    switch (dirent_p->d_type) {
    case DT_DIR: {
      int ret = handle_directory(name, root_fd, current_fd, dirlist_ptr,
                                 resolve_opts);
      if (ret == EXIT_FAILURE) {
        return EXIT_FAILURE;
      }
    }
    case DT_LNK: {
      int ret = handle_symlink(name, root_fd, dirlist_ptr, resolve_opts);
      if (ret == EXIT_FAILURE) {
        return EXIT_FAILURE;
      }
    }
    case DT_UNKNOWN: {
      // Some filesystems return DT_UNKWOWN.
      // For more information, see
      // https://man7.org/linux/man-pages/man3/readdir.3.html
      struct stat statbuf;
      int ret = fstatat(current_fd, name, &statbuf, AT_SYMLINK_NOFOLLOW);
      if (ret == -1) {
        fprintf(stderr, "error: fstatat() failed\n");
        return EXIT_FAILURE;
      }
      if ((statbuf.st_mode & S_IFMT) == S_IFLNK) {
        int ret = handle_symlink(name, root_fd, dirlist_ptr, resolve_opts);
        if (ret == EXIT_FAILURE) {
          return EXIT_FAILURE;
        }
      }
      if ((statbuf.st_mode & S_IFMT) == S_IFDIR) {
        int ret = handle_directory(name, root_fd, current_fd, dirlist_ptr,
                                   resolve_opts);
        if (ret == EXIT_FAILURE) {
          return EXIT_FAILURE;
        }
      }
    }
    }
  }
  return EXIT_SUCCESS;
}

int walk_dirs_root(char *root_path, uint64_t resolve_opts) {
  auto_close int root_fd = open(root_path, O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
  if (root_fd < 0) {
    fprintf(stderr, "error: could not open %s\n", root_path);
    return EXIT_FAILURE;
  }
  auto_closedir DIR *root_p = fdopendir(root_fd);
  if (root_p == NULL) {
    fprintf(stderr, "error: fdopendir() failed\n");
    return EXIT_FAILURE;
  }
  root_fd = -1; // root_p now owns the file descriptor
  int root_fd2 = dirfd(root_p);
  if (root_fd2 == -1) {
    fprintf(stderr, "error: dirfd() failed\n");
    return EXIT_FAILURE;
  }
  return walk_dirs(root_fd2, root_p, NULL, resolve_opts);
}
