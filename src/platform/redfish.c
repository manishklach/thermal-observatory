#define _GNU_SOURCE
#include "../../include/thermal_monitor.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int file_exists(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return 0;
    }
    fclose(fp);
    return 1;
}

static void trim_copy(char *dst, size_t len, const char *src) {
    while (*src && isspace((unsigned char)*src)) {
        ++src;
    }
    snprintf(dst, len, "%s", src);
    while (strlen(dst) > 0 && isspace((unsigned char)dst[strlen(dst) - 1])) {
        dst[strlen(dst) - 1] = '\0';
    }
}

static void add_board_sensor(tm_snapshot_t *snap, const char *name, const char *type, double value, const char *unit) {
    tm_board_sensor_t *sensor;
    if (snap->board_sensor_count >= TM_MAX_BOARD_SENSORS) {
        return;
    }
    sensor = &snap->board_sensors[snap->board_sensor_count++];
    memset(sensor, 0, sizeof(*sensor));
    strncpy(sensor->name, name, sizeof(sensor->name) - 1);
    strncpy(sensor->sensor_type, type, sizeof(sensor->sensor_type) - 1);
    strncpy(sensor->source, "redfish", sizeof(sensor->source) - 1);
    sensor->value = value;
    strncpy(sensor->unit, unit, sizeof(sensor->unit) - 1);
}

int tm_collect_redfish(tm_context_t *ctx, tm_snapshot_t *snap) {
    const char *path = getenv("TM_REDFISH_SAMPLE");
    FILE *fp;
    char line[512];

    (void)ctx;

    if (!path || !file_exists(path)) {
        return 0;
    }

    fp = fopen(path, "r");
    if (!fp) {
        return 0;
    }

    while (fgets(line, sizeof(line), fp)) {
        char name[128] = {0};
        char type[64] = {0};
        char unit[32] = {0};
        double value = 0.0;
        char *name_field;
        char *type_field;
        char *value_field;
        char *unit_field;

        name_field = strtok(line, ",");
        type_field = strtok(NULL, ",");
        value_field = strtok(NULL, ",");
        unit_field = strtok(NULL, ",");
        if (!name_field || !type_field || !value_field || !unit_field) {
            continue;
        }

        trim_copy(name, sizeof(name), name_field);
        trim_copy(type, sizeof(type), type_field);
        trim_copy(unit, sizeof(unit), unit_field);
        value = atof(value_field);
        add_board_sensor(snap, name, type, value, unit);
    }

    fclose(fp);
    if (snap->board_sensor_count > 0) {
        snap->capabilities |= TM_CAP_PLATFORM_REDFISH;
    }
    return 0;
}

