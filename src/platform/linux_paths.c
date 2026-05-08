#define _GNU_SOURCE

#include "linux_paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *tm_sysroot(void) {
    const char *value = getenv("TM_SYSROOT");
    return (value && value[0] != '\0') ? value : "";
}

int tm_path_join(char *buf, size_t len, const char *suffix) {
    return snprintf(buf, len, "%s%s", tm_sysroot(), suffix);
}

int tm_glob_join(char *buf, size_t len, const char *suffix) {
    return tm_path_join(buf, len, suffix);
}

int tm_strip_sysroot_path(const char *path, char *buf, size_t len) {
    const char *root = tm_sysroot();
    size_t root_len = strlen(root);

    if (root_len > 0 && strncmp(path, root, root_len) == 0) {
        return snprintf(buf, len, "%s", path + root_len);
    }
    return snprintf(buf, len, "%s", path);
}

