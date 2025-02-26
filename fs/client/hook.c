#define _GNU_SOURCE

#include <stdio.h>
#include <stddef.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/stat.h>

#include "fs/client/posix_wrapper.h"

#define FUSEEFS_MOUNT_POINT "/fuseefs/"
#define FUSEEFS_FD_BASE 65536

__attribute__((constructor)) static void setup(void) { InitWrapper(); }

__attribute__((destructor)) static void teardown(void) { DestroyWrapper(); }


static inline void assign_fnptr(void **fnptr, void *fn) {
  *fnptr = fn;
}

#define ASSIGN_FN(fn)                                                          \
  do {                                                                         \
    if (libc_##fn == NULL) {                                                   \
      assign_fnptr((void **)&libc_##fn, dlsym(RTLD_NEXT, #fn));                \
    }                                                                          \
  } while (0)

static inline int is_fuseefs_path(const char* path) {
  return strncmp(path, FUSEEFS_MOUNT_POINT, strlen(FUSEEFS_MOUNT_POINT)) == 0;
}

static inline int is_fuseefs_fd(int fd) {
  return fd >= FUSEEFS_FD_BASE;
}

static inline const char* get_fuseefs_path(const char* path) {
  return path + strlen(FUSEEFS_MOUNT_POINT) - 1;
}

typedef int (*mkdir_t)(const char *path, mode_t mode);
static mkdir_t libc_mkdir = NULL;

int mkdir(const char *path, mode_t mode) {
  ASSIGN_FN(mkdir);
  if (is_fuseefs_path(path)) {
    return fuseefs_mkdir(get_fuseefs_path(path), mode);
  }
  return libc_mkdir(path, mode);
}

typedef int (*rmdir_t)(const char *path);
static rmdir_t libc_rmdir = NULL;

int rmdir(const char *path) {
  ASSIGN_FN(rmdir);
  if (is_fuseefs_path(path)) {
    return fuseefs_rmdir(get_fuseefs_path(path));
  }
  return libc_rmdir(path);
}

typedef int (*creat_t)(const char *path, mode_t mode);
static creat_t libc_creat = NULL;

int creat(const char *path, mode_t mode) {
  ASSIGN_FN(creat);
  if (is_fuseefs_path(path)) {
    return fuseefs_creat(get_fuseefs_path(path), mode);
  }
  return libc_creat(path, mode);
}

typedef int (*unlink_t)(const char *path);
static unlink_t libc_unlink = NULL;

int unlink(const char *path) {
  ASSIGN_FN(unlink);
  if (is_fuseefs_path(path)) {
    return fuseefs_unlink(get_fuseefs_path(path));
  }
  return libc_unlink(path);
}

typedef int (*stat_t)(const char* path, struct stat* st);
static stat_t libc_stat = NULL;

int stat(const char* path, struct stat* st) {
  ASSIGN_FN(stat);
  if (is_fuseefs_path(path)) {
    return fuseefs_stat(get_fuseefs_path(path), st);
  }
  return libc_stat(path, st);
}

typedef int (*open64_t)(const char* path, int flags, ...);
static open64_t libc_open64 = NULL;

int open64(const char* path, int flags, ...) {
  ASSIGN_FN(open64);
  va_list args;
  va_start(args, flags);
  if (!is_fuseefs_path) {
    return libc_open64(path, flags);
  }
  if (flags & O_CREAT) {
    mode_t mode = va_arg(args, mode_t);
    va_end(args);
    struct stat st;
    if (fuseefs_stat(get_fuseefs_path(path), &st) == 0) {
      return FUSEEFS_FD_BASE + st.st_ino;
    } else if (fuseefs_creat(get_fuseefs_path(path), mode) >= 0) {
      return FUSEEFS_FD_BASE;
    }
    return -1;
  } else {
    struct stat st;
    if (fuseefs_stat(get_fuseefs_path(path), &st) == 0) {
      return FUSEEFS_FD_BASE + st.st_ino;
    }
    return -1;
  }
  return -1;
}

typedef int (*open_t)(const char* path, int flags, mode_t mode);
static open_t libc_open = NULL;

int open(const char* path, int flags, ...) {
  ASSIGN_FN(open);
  va_list args;
  va_start(args, flags);
  if (flags & O_CREAT) {
    mode_t mode = va_arg(args, mode_t);
    va_end(args);
    return open64(path, flags, mode);
  } 
  return open64(path, flags);
}

typedef int (*close_t)(int fd);
static close_t libc_close = NULL;

int close(int fd) {
  ASSIGN_FN(close);
  if (is_fuseefs_fd(fd)) {
    return 0;
  }
  return libc_close(fd);
}