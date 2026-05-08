#ifndef TM_LINUX_PATHS_H
#define TM_LINUX_PATHS_H

#include <stddef.h>

const char *tm_sysroot(void);
int tm_path_join(char *buf, size_t len, const char *suffix);
int tm_glob_join(char *buf, size_t len, const char *suffix);
int tm_strip_sysroot_path(const char *path, char *buf, size_t len);

#endif

