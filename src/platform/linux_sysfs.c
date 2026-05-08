#define _GNU_SOURCE
#include "../../include/thermal_monitor.h"

#include <errno.h>
#include <glob.h>
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>

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

void tm_context_init(tm_context_t *ctx) {
    struct utsname uts;

    memset(ctx, 0, sizeof(*ctx));
    if (uname(&uts) != 0) {
        ctx->arch = TM_ARCH_UNKNOWN;
        return;
    }

    if (strcmp(uts.machine, "x86_64") == 0) {
        ctx->arch = TM_ARCH_X86_64;
    } else if (strcmp(uts.machine, "aarch64") == 0 || strcmp(uts.machine, "arm64") == 0) {
        ctx->arch = TM_ARCH_ARM64;
    } else {
        ctx->arch = TM_ARCH_UNKNOWN;
    }
}

int tm_collect_generic_linux(tm_context_t *ctx, tm_snapshot_t *snap) {
    glob_t zones;
    glob_t hwmons;
    int i;

    (void)ctx;

    if (glob("/sys/class/thermal/thermal_zone*/temp", 0, NULL, &zones) == 0) {
        for (i = 0; i < (int)zones.gl_pathc && snap->thermal_zone_count < TM_MAX_THERMAL_ZONES; ++i) {
            tm_sensor_t *sensor = &snap->thermal_zones[snap->thermal_zone_count];
            char zone_name[64] = {0};
            char base[256];
            char type_path[256];
            long temp_mc = 0;

            if (read_long(zones.gl_pathv[i], &temp_mc) != 0) {
                continue;
            }

            if (sscanf(zones.gl_pathv[i], "/sys/class/thermal/%63[^/]/temp", zone_name) != 1) {
                continue;
            }

            snprintf(base, sizeof(base), "/sys/class/thermal/%s", zone_name);
            snprintf(type_path, sizeof(type_path), "%s/type", base);

            memset(sensor, 0, sizeof(*sensor));
            strncpy(sensor->name, zone_name, sizeof(sensor->name) - 1);
            read_string(type_path, sensor->label, sizeof(sensor->label));
            sensor->value = (double)temp_mc / 1000.0;
            strncpy(sensor->unit, "celsius", sizeof(sensor->unit) - 1);
            snap->thermal_zone_count++;
        }
        snap->capabilities |= TM_CAP_LINUX_THERMAL;
    }
    globfree(&zones);

    if (glob("/sys/class/hwmon/hwmon*/temp*_input", 0, NULL, &hwmons) == 0) {
        for (i = 0; i < (int)hwmons.gl_pathc && snap->hwmon_sensor_count < TM_MAX_HWMON_SENSORS; ++i) {
            tm_sensor_t *sensor = &snap->hwmon_sensors[snap->hwmon_sensor_count];
            char hwmon[64] = {0};
            char input_path[512];
            char label_path[512];
            char name_path[512];
            long temp_mc = 0;

            if (read_long(hwmons.gl_pathv[i], &temp_mc) != 0) {
                continue;
            }
            if (sscanf(hwmons.gl_pathv[i], "/sys/class/hwmon/%63[^/]/", hwmon) != 1) {
                continue;
            }

            memset(sensor, 0, sizeof(*sensor));
            strncpy(input_path, hwmons.gl_pathv[i], sizeof(input_path) - 1);
            strncpy(label_path, hwmons.gl_pathv[i], sizeof(label_path) - 1);
            snprintf(name_path, sizeof(name_path), "/sys/class/hwmon/%s/name", hwmon);

            if (strstr(label_path, "_input")) {
                strcpy(strstr(label_path, "_input"), "_label");
            }

            read_string(name_path, sensor->name, sizeof(sensor->name));
            if (read_string(label_path, sensor->label, sizeof(sensor->label)) != 0) {
                const char *basename = strrchr(input_path, '/');
                strncpy(sensor->label, basename ? basename + 1 : "temp", sizeof(sensor->label) - 1);
            }
            sensor->value = (double)temp_mc / 1000.0;
            strncpy(sensor->unit, "celsius", sizeof(sensor->unit) - 1);
            snap->hwmon_sensor_count++;
        }
        snap->capabilities |= TM_CAP_LINUX_HWMON;
    }
    globfree(&hwmons);

    return 0;
}

