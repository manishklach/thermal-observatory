#include "../../include/thermal_monitor.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char *buf;
    size_t len;
    size_t pos;
    int failed;
} tm_prom_writer_t;

static int appendf(tm_prom_writer_t *w, const char *fmt, ...) {
    va_list args;
    int rc;

    if (w->failed) {
        return -1;
    }

    va_start(args, fmt);
    rc = vsnprintf(w->buf + w->pos, w->len - w->pos, fmt, args);
    va_end(args);
    if (rc < 0 || (size_t)rc >= (w->len - w->pos)) {
        w->failed = 1;
        return -1;
    }
    w->pos += (size_t)rc;
    return 0;
}

static int append_metric(tm_prom_writer_t *w, const char *name, const char *labels, double value) {
    if (labels && labels[0] != '\0') {
        return appendf(w, "%s{%s} %.6f\n", name, labels, value);
    }
    return appendf(w, "%s %.6f\n", name, value);
}

int tm_snapshot_to_prometheus(const tm_snapshot_t *snap, char *buf, size_t len) {
    tm_prom_writer_t w = { .buf = buf, .len = len, .pos = 0, .failed = 0 };
    char labels[512];

    appendf(&w, "# HELP thermal_snapshot_timestamp_seconds Snapshot collection time.\n");
    appendf(&w, "# TYPE thermal_snapshot_timestamp_seconds gauge\n");
    append_metric(&w, "thermal_snapshot_timestamp_seconds", "", (double)snap->timestamp.tv_sec + ((double)snap->timestamp.tv_nsec / 1e9));

    for (int i = 0; i < snap->cpu_package_count; ++i) {
        const tm_cpu_package_t *pkg = &snap->cpu_packages[i];
        snprintf(labels, sizeof(labels), "package=\"%d\",source=\"coretemp_powercap\"", pkg->package_id);
        append_metric(&w, "thermal_cpu_package_temperature_celsius", labels, pkg->package_temp_c);
        append_metric(&w, "thermal_cpu_package_tjmax_celsius", labels, pkg->tjmax_c);
        append_metric(&w, "thermal_cpu_package_rapl_energy_uj", labels, pkg->rapl_energy_uj);
        append_metric(&w, "thermal_cpu_package_power_limit_1_watts", labels, pkg->power_limit_1_w);
        append_metric(&w, "thermal_cpu_package_power_limit_2_watts", labels, pkg->power_limit_2_w);

        for (int j = 0; j < pkg->core_count; ++j) {
            const tm_cpu_core_t *core = &pkg->cores[j];
            snprintf(labels, sizeof(labels), "package=\"%d\",core=\"%d\",source=\"coretemp\"", pkg->package_id, core->core_id);
            append_metric(&w, "thermal_cpu_core_temperature_celsius", labels, core->temp_c);
            append_metric(&w, "thermal_cpu_core_crit_celsius", labels, core->crit_c);
            append_metric(&w, "thermal_cpu_core_max_celsius", labels, core->max_c);
            append_metric(&w, "thermal_cpu_core_throttling", labels, core->throttling ? 1.0 : 0.0);
        }
    }

    for (int i = 0; i < snap->arm_cluster_count; ++i) {
        const tm_arm_cluster_t *cluster = &snap->arm_clusters[i];
        snprintf(labels, sizeof(labels), "cluster=\"%d\",zone=\"%s\",type=\"%s\",source=\"thermal_zone_cpufreq\"",
                 cluster->cluster_id, cluster->zone_name, cluster->zone_type);
        append_metric(&w, "thermal_arm_cluster_temperature_celsius", labels, cluster->temp_c);
        append_metric(&w, "thermal_arm_cluster_frequency_mhz", labels, cluster->cur_freq_mhz);
        append_metric(&w, "thermal_arm_cluster_throttling", labels, cluster->throttling ? 1.0 : 0.0);
    }

    for (int i = 0; i < snap->nvidia_gpu_count; ++i) {
        const tm_nvidia_gpu_t *gpu = &snap->nvidia_gpus[i];
        snprintf(labels, sizeof(labels), "vendor=\"nvidia\",index=\"%d\",uuid=\"%s\",source=\"%s\"",
                 gpu->gpu_index, gpu->uuid, (snap->capabilities & TM_CAP_NVIDIA_NVML) ? "nvml" : "cuda");
        append_metric(&w, "thermal_gpu_temperature_celsius", labels, gpu->gpu_temp_c);
        append_metric(&w, "thermal_gpu_memory_temperature_celsius", labels, gpu->memory_temp_c);
        append_metric(&w, "thermal_gpu_power_watts", labels, gpu->power_draw_w);
        append_metric(&w, "thermal_gpu_power_limit_watts", labels, gpu->power_limit_w);
        append_metric(&w, "thermal_gpu_sm_clock_mhz", labels, gpu->sm_clock_mhz);
        append_metric(&w, "thermal_gpu_memory_clock_mhz", labels, gpu->mem_clock_mhz);
        append_metric(&w, "thermal_gpu_utilization_percent", labels, gpu->gpu_util_pct);
        append_metric(&w, "thermal_gpu_memory_utilization_percent", labels, gpu->mem_util_pct);
        append_metric(&w, "thermal_gpu_cuda_ordinal", labels, gpu->cuda_ordinal);
        append_metric(&w, "thermal_gpu_cuda_total_memory_bytes", labels, (double)gpu->cuda_total_memory_bytes);

        struct reason_map { uint64_t bit; const char *name; } reasons[] = {
            { 1ULL << 0, "gpu_idle" },
            { 1ULL << 1, "applications_clocks" },
            { 1ULL << 2, "sw_power_cap" },
            { 1ULL << 3, "hw_slowdown" },
            { 1ULL << 5, "sw_thermal_slowdown" },
            { 1ULL << 7, "hw_power_brake_slowdown" },
            { 1ULL << 8, "hw_thermal_slowdown" }
        };
        for (size_t r = 0; r < sizeof(reasons) / sizeof(reasons[0]); ++r) {
            snprintf(labels, sizeof(labels), "vendor=\"nvidia\",index=\"%d\",uuid=\"%s\",reason=\"%s\",source=\"nvml\"",
                     gpu->gpu_index, gpu->uuid, reasons[r].name);
            append_metric(&w, "thermal_gpu_throttle_reason", labels, (gpu->throttle_reasons & reasons[r].bit) ? 1.0 : 0.0);
        }
    }

    for (int i = 0; i < snap->amd_gpu_count; ++i) {
        const tm_amd_gpu_t *gpu = &snap->amd_gpus[i];
        snprintf(labels, sizeof(labels), "vendor=\"amd\",index=\"%d\",name=\"%s\",source=\"%s\"",
                 gpu->gpu_index, gpu->name, (snap->capabilities & TM_CAP_AMD_ROCM_SMI) ? "rocm_smi" : "amdgpu_hwmon");
        append_metric(&w, "thermal_gpu_temperature_celsius", labels, gpu->edge_temp_c);
        append_metric(&w, "thermal_gpu_junction_temperature_celsius", labels, gpu->junction_temp_c);
        append_metric(&w, "thermal_gpu_memory_temperature_celsius", labels, gpu->memory_temp_c);
        append_metric(&w, "thermal_gpu_power_watts", labels, gpu->avg_power_w);
        append_metric(&w, "thermal_gpu_power_limit_watts", labels, gpu->power_cap_w);
        append_metric(&w, "thermal_gpu_thermal_throttling", labels, gpu->thermal_throttling ? 1.0 : 0.0);
    }

    for (int i = 0; i < snap->hwmon_sensor_count; ++i) {
        const tm_sensor_t *sensor = &snap->hwmon_sensors[i];
        snprintf(labels, sizeof(labels), "name=\"%s\",label=\"%s\",source=\"hwmon\"", sensor->name, sensor->label);
        append_metric(&w, "thermal_hwmon_value", labels, sensor->value);
    }

    for (int i = 0; i < snap->thermal_zone_count; ++i) {
        const tm_sensor_t *sensor = &snap->thermal_zones[i];
        snprintf(labels, sizeof(labels), "name=\"%s\",label=\"%s\",source=\"thermal_zone\"", sensor->name, sensor->label);
        append_metric(&w, "thermal_zone_value", labels, sensor->value);
    }

    for (int i = 0; i < snap->board_sensor_count; ++i) {
        const tm_board_sensor_t *sensor = &snap->board_sensors[i];
        snprintf(labels, sizeof(labels), "name=\"%s\",sensor_type=\"%s\",source=\"%s\"", sensor->name, sensor->sensor_type, sensor->source);
        append_metric(&w, "thermal_board_sensor_value", labels, sensor->value);
    }

    for (int i = 0; i < snap->fan_sensor_count; ++i) {
        const tm_fan_sensor_t *fan = &snap->fan_sensors[i];
        snprintf(labels, sizeof(labels), "name=\"%s\",source=\"%s\"", fan->name, fan->source);
        append_metric(&w, "thermal_fan_rpm", labels, fan->rpm);
        append_metric(&w, "thermal_fan_pwm_percent", labels, fan->pwm_pct);
    }

    for (int i = 0; i < snap->psu_sensor_count; ++i) {
        const tm_psu_sensor_t *psu = &snap->psu_sensors[i];
        snprintf(labels, sizeof(labels), "name=\"%s\",source=\"%s\"", psu->name, psu->source);
        append_metric(&w, "thermal_psu_inlet_temperature_celsius", labels, psu->inlet_temp_c);
        append_metric(&w, "thermal_psu_exhaust_temperature_celsius", labels, psu->exhaust_temp_c);
        append_metric(&w, "thermal_psu_power_watts", labels, psu->power_w);
        append_metric(&w, "thermal_psu_present", labels, psu->present ? 1.0 : 0.0);
    }

    return w.failed ? -1 : (int)w.pos;
}
