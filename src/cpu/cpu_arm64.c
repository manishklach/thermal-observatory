#define _GNU_SOURCE
#include "../../include/thermal_monitor.h"
#include "../platform/linux_paths.h"

#include <errno.h>
#include <glob.h>
#include <stdio.h>
#include <string.h>

static int read_long(const char *path, long *value) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return -errno;
    }
    if (fscanf(fp, "%ld", value) != 1) {
        fclose(fp);
        return -EIO;
    }
    fclose(fp);
    return 0;
}

static int read_string(const char *path, char *buf, size_t len) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return -errno;
    }
    if (!fgets(buf, (int)len, fp)) {
        fclose(fp);
        return -EIO;
    }
    fclose(fp);
    buf[strcspn(buf, "\n")] = '\0';
    return 0;
}

int tm_collect_arm64(tm_context_t *ctx, tm_snapshot_t *snap) {
    glob_t zones;
    char pattern[256];

    (void)ctx;

    tm_glob_join(pattern, sizeof(pattern), "/sys/class/thermal/thermal_zone*/temp");
    if (glob(pattern, 0, NULL, &zones) != 0) {
        globfree(&zones);
        return 0;
    }

    for (int i = 0; i < (int)zones.gl_pathc && snap->arm_cluster_count < TM_MAX_ARM_CLUSTERS; ++i) {
        tm_arm_cluster_t *cluster = &snap->arm_clusters[snap->arm_cluster_count];
        char zone_name[64] = {0};
        char virtual_path[512];
        char type_path[256];
        char freq_path[256];
        long temp_mc = 0;
        long freq_khz = 0;

        if (read_long(zones.gl_pathv[i], &temp_mc) != 0) {
            continue;
        }
        tm_strip_sysroot_path(zones.gl_pathv[i], virtual_path, sizeof(virtual_path));
        if (sscanf(virtual_path, "/sys/class/thermal/%63[^/]/temp", zone_name) != 1) {
            continue;
        }

        memset(cluster, 0, sizeof(*cluster));
        cluster->cluster_id = snap->arm_cluster_count;
        strncpy(cluster->zone_name, zone_name, sizeof(cluster->zone_name) - 1);
        cluster->temp_c = (double)temp_mc / 1000.0;

        snprintf(type_path, sizeof(type_path), "%s/sys/class/thermal/%s/type", tm_sysroot(), zone_name);
        read_string(type_path, cluster->zone_type, sizeof(cluster->zone_type));

        snprintf(freq_path, sizeof(freq_path), "%s/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", tm_sysroot());
        if (read_long(freq_path, &freq_khz) == 0) {
            cluster->cur_freq_mhz = (double)freq_khz / 1000.0;
        }
        snap->arm_cluster_count++;
    }

    if (snap->arm_cluster_count > 0) {
        snap->capabilities |= TM_CAP_ARM_SCMI | TM_CAP_LINUX_THERMAL;
    }
    globfree(&zones);
    return 0;
}
