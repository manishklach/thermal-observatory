#define _GNU_SOURCE
#include "../../include/thermal_monitor.h"

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

static void collect_coretemp(tm_snapshot_t *snap) {
    glob_t labels;
    int package_id = 0;
    int core_count = 0;

    if (glob("/sys/devices/platform/coretemp.*/hwmon/hwmon*/temp*_label", 0, NULL, &labels) != 0) {
        globfree(&labels);
        return;
    }

    if (snap->cpu_package_count >= TM_MAX_CPU_PACKAGES) {
        globfree(&labels);
        return;
    }

    tm_cpu_package_t *pkg = &snap->cpu_packages[snap->cpu_package_count];
    memset(pkg, 0, sizeof(*pkg));
    pkg->package_id = package_id;

    for (int i = 0; i < (int)labels.gl_pathc && core_count < TM_MAX_CPU_CORES; ++i) {
        FILE *fp = fopen(labels.gl_pathv[i], "r");
        char label[64];
        char input_path[512];
        char crit_path[512];
        char max_path[512];
        long input_mc = 0;
        long crit_mc = 0;
        long max_mc = 0;
        int core_id = -1;

        if (!fp) {
            continue;
        }
        if (!fgets(label, sizeof(label), fp)) {
            fclose(fp);
            continue;
        }
        fclose(fp);
        label[strcspn(label, "\n")] = '\0';

        if (sscanf(label, "Core %d", &core_id) != 1) {
            continue;
        }

        strncpy(input_path, labels.gl_pathv[i], sizeof(input_path) - 1);
        strncpy(crit_path, labels.gl_pathv[i], sizeof(crit_path) - 1);
        strncpy(max_path, labels.gl_pathv[i], sizeof(max_path) - 1);
        strcpy(strstr(input_path, "_label"), "_input");
        strcpy(strstr(crit_path, "_label"), "_crit");
        strcpy(strstr(max_path, "_label"), "_max");

        if (read_long(input_path, &input_mc) != 0) {
            continue;
        }
        read_long(crit_path, &crit_mc);
        read_long(max_path, &max_mc);

        pkg->cores[core_count].core_id = core_id;
        pkg->cores[core_count].temp_c = (double)input_mc / 1000.0;
        pkg->cores[core_count].crit_c = (double)crit_mc / 1000.0;
        pkg->cores[core_count].max_c = (double)max_mc / 1000.0;
        if (pkg->cores[core_count].temp_c > pkg->package_temp_c) {
            pkg->package_temp_c = pkg->cores[core_count].temp_c;
        }
        if (pkg->cores[core_count].crit_c > pkg->tjmax_c) {
            pkg->tjmax_c = pkg->cores[core_count].crit_c;
        }
        core_count++;
    }

    pkg->core_count = core_count;
    if (core_count > 0) {
        snap->cpu_package_count++;
    }
    globfree(&labels);
}

static void collect_rapl(tm_snapshot_t *snap) {
    tm_cpu_package_t *pkg;
    char base[256];
    char path[256];
    long energy_uj = 0;
    long pl1_uw = 0;
    long pl2_uw = 0;

    if (snap->cpu_package_count == 0) {
        return;
    }
    pkg = &snap->cpu_packages[0];

    snprintf(base, sizeof(base), "/sys/class/powercap/intel-rapl/intel-rapl:0");
    snprintf(path, sizeof(path), "%s/energy_uj", base);
    if (read_long(path, &energy_uj) == 0) {
        pkg->rapl_energy_uj = (double)energy_uj;
        snap->capabilities |= TM_CAP_LINUX_RAPL;
    }

    snprintf(path, sizeof(path), "%s/constraint_0_power_limit_uw", base);
    if (read_long(path, &pl1_uw) == 0) {
        pkg->power_limit_1_w = (double)pl1_uw / 1000000.0;
    }

    snprintf(path, sizeof(path), "%s/constraint_1_power_limit_uw", base);
    if (read_long(path, &pl2_uw) == 0) {
        pkg->power_limit_2_w = (double)pl2_uw / 1000000.0;
    }
}

int tm_collect_x86(tm_context_t *ctx, tm_snapshot_t *snap) {
    (void)ctx;
    collect_coretemp(snap);
    collect_rapl(snap);
    return 0;
}

