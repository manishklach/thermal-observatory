#define _GNU_SOURCE
#include "../../include/thermal_monitor.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void trim(char *s) {
    char *end;
    while (*s && isspace((unsigned char)*s)) {
        ++s;
    }
    end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) {
        --end;
    }
    *end = '\0';
}

static int command_exists(const char *cmd) {
    char query[128];
    snprintf(query, sizeof(query), "command -v %s >/dev/null 2>&1", cmd);
    return system(query) == 0;
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
    strncpy(sensor->source, "ipmi", sizeof(sensor->source) - 1);
    sensor->value = value;
    strncpy(sensor->unit, unit, sizeof(sensor->unit) - 1);
}

static void add_fan_sensor(tm_snapshot_t *snap, const char *name, double rpm) {
    tm_fan_sensor_t *fan;
    if (snap->fan_sensor_count >= TM_MAX_FANS) {
        return;
    }
    fan = &snap->fan_sensors[snap->fan_sensor_count++];
    memset(fan, 0, sizeof(*fan));
    strncpy(fan->name, name, sizeof(fan->name) - 1);
    strncpy(fan->source, "ipmi", sizeof(fan->source) - 1);
    fan->rpm = rpm;
    fan->pwm_pct = -1.0;
}

int tm_collect_ipmi(tm_context_t *ctx, tm_snapshot_t *snap) {
    FILE *pipe;
    char line[512];

    (void)ctx;

    if (!command_exists("ipmitool")) {
        return 0;
    }

    pipe = popen("ipmitool sdr elist all 2>/dev/null", "r");
    if (!pipe) {
        return 0;
    }

    while (fgets(line, sizeof(line), pipe)) {
        char *name = strtok(line, "|");
        char *reading = strtok(NULL, "|");
        char *status = strtok(NULL, "|");
        char *entity = strtok(NULL, "|");
        double value = 0.0;
        char unit[32] = {0};

        (void)status;
        (void)entity;

        if (!name || !reading) {
            continue;
        }
        trim(name);
        trim(reading);

        if (sscanf(reading, "%lf %31s", &value, unit) < 2) {
            continue;
        }

        if (strstr(unit, "degrees") || strstr(unit, "C")) {
            add_board_sensor(snap, name, "temperature", value, "celsius");
        } else if (strstr(unit, "RPM")) {
            add_fan_sensor(snap, name, value);
        } else if (strstr(unit, "Watts")) {
            add_board_sensor(snap, name, "power", value, "watts");
        }
    }

    pclose(pipe);
    if (snap->board_sensor_count > 0 || snap->fan_sensor_count > 0) {
        snap->capabilities |= TM_CAP_PLATFORM_IPMI;
    }
    return 0;
}

