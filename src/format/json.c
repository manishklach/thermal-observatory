#include "../../include/thermal_monitor.h"

#include <stdio.h>

int tm_snapshot_to_json(const tm_snapshot_t *snap, char *buf, size_t len) {
    int written = 0;

    written += snprintf(buf + written, len - (size_t)written,
                        "{"
                        "\"arch\":%d,"
                        "\"capabilities\":%u,"
                        "\"cpu_package_count\":%d,"
                        "\"arm_cluster_count\":%d,"
                        "\"nvidia_gpu_count\":%d,"
                        "\"amd_gpu_count\":%d,"
                        "\"notes\":\"top-level summary only; per-device JSON expansion is a planned next step\""
                        "}",
                        snap->arch,
                        snap->capabilities,
                        snap->cpu_package_count,
                        snap->arm_cluster_count,
                        snap->nvidia_gpu_count,
                        snap->amd_gpu_count);
    return (written >= 0 && (size_t)written < len) ? written : -1;
}
