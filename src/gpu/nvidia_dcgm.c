#define _GNU_SOURCE
#include "../../include/thermal_monitor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int command_exists(const char *cmd) {
    char query[128];
    snprintf(query, sizeof(query), "command -v %s >/dev/null 2>&1", cmd);
    return system(query) == 0;
}

int tm_collect_nvidia_dcgm(tm_context_t *ctx, tm_snapshot_t *snap) {
    FILE *pipe;
    char line[512];

    (void)ctx;

    if (!command_exists("dcgmi")) {
        return 0;
    }

    pipe = popen("dcgmi discovery -l 2>/dev/null", "r");
    if (!pipe) {
        return 0;
    }

    while (fgets(line, sizeof(line), pipe)) {
        int gpu_index = -1;
        if (sscanf(line, "GPU %d", &gpu_index) == 1) {
            for (int i = 0; i < snap->nvidia_gpu_count; ++i) {
                if (snap->nvidia_gpus[i].gpu_index == gpu_index) {
                    snap->capabilities |= TM_CAP_NVIDIA_DCGM;
                    break;
                }
            }
        }
    }

    pclose(pipe);
    return 0;
}

