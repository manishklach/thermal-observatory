#include "../../include/thermal_monitor.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char *buf;
    size_t len;
    size_t pos;
    int failed;
} tm_json_writer_t;

static int appendf(tm_json_writer_t *w, const char *fmt, ...) {
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

static int append_json_string(tm_json_writer_t *w, const char *s) {
    const unsigned char *p = (const unsigned char *)s;

    if (appendf(w, "\"") != 0) {
        return -1;
    }
    while (*p != '\0') {
        if (*p == '\\' || *p == '"') {
            if (appendf(w, "\\%c", *p) != 0) {
                return -1;
            }
        } else if (*p == '\n') {
            if (appendf(w, "\\n") != 0) {
                return -1;
            }
        } else if (*p == '\r') {
            if (appendf(w, "\\r") != 0) {
                return -1;
            }
        } else if (*p == '\t') {
            if (appendf(w, "\\t") != 0) {
                return -1;
            }
        } else {
            if (appendf(w, "%c", *p) != 0) {
                return -1;
            }
        }
        ++p;
    }
    return appendf(w, "\"");
}

static long long snapshot_timestamp_ns(const tm_snapshot_t *snap) {
    return ((long long)snap->timestamp.tv_sec * 1000000000LL) + snap->timestamp.tv_nsec;
}

static int append_metric_number(tm_json_writer_t *w, double value, const char *unit, const char *source, long long ts_ns) {
    return appendf(w,
                   "{"
                   "\"value\":%.6f,"
                   "\"unit\":", value) == 0 &&
           append_json_string(w, unit ? unit : "") == 0 &&
           appendf(w, ",\"source\":") == 0 &&
           append_json_string(w, source ? source : "unknown") == 0 &&
           appendf(w, ",\"timestamp_ns\":%lld,\"error\":null}", ts_ns) == 0 ? 0 : -1;
}

static int append_metric_bool(tm_json_writer_t *w, bool value, const char *source, long long ts_ns) {
    return appendf(w,
                   "{"
                   "\"value\":%s,"
                   "\"unit\":\"bool\",",
                   value ? "true" : "false") == 0 &&
           appendf(w, "\"source\":") == 0 &&
           append_json_string(w, source ? source : "unknown") == 0 &&
           appendf(w, ",\"timestamp_ns\":%lld,\"error\":null}", ts_ns) == 0 ? 0 : -1;
}

static const char *arch_name(tm_arch_t arch) {
    switch (arch) {
    case TM_ARCH_X86_64:
        return "x86_64";
    case TM_ARCH_ARM64:
        return "arm64";
    default:
        return "unknown";
    }
}

static const char *nvidia_source(const tm_snapshot_t *snap) {
    if ((snap->capabilities & TM_CAP_NVIDIA_NVML) && (snap->capabilities & TM_CAP_NVIDIA_CUDA)) {
        return "nvml+cuda";
    }
    if (snap->capabilities & TM_CAP_NVIDIA_NVML) {
        return "nvml";
    }
    if (snap->capabilities & TM_CAP_NVIDIA_CUDA) {
        return "cuda";
    }
    return "unknown";
}

static const char *amd_source(const tm_snapshot_t *snap) {
    if (snap->capabilities & TM_CAP_AMD_ROCM_SMI) {
        return "rocm_smi";
    }
    if (snap->capabilities & TM_CAP_AMDGPU_HWMON) {
        return "amdgpu_hwmon";
    }
    return "unknown";
}

static int append_capabilities(tm_json_writer_t *w, uint32_t caps) {
    int first = 1;
    struct cap_name {
        uint32_t bit;
        const char *name;
    } cap_names[] = {
        { TM_CAP_LINUX_HWMON, "linux_hwmon" },
        { TM_CAP_LINUX_THERMAL, "linux_thermal" },
        { TM_CAP_LINUX_RAPL, "linux_rapl" },
        { TM_CAP_X86_MSR, "x86_msr" },
        { TM_CAP_ARM_SCMI, "arm_scmi" },
        { TM_CAP_NVIDIA_NVML, "nvidia_nvml" },
        { TM_CAP_AMD_ROCM_SMI, "amd_rocm_smi" },
        { TM_CAP_AMDGPU_HWMON, "amdgpu_hwmon" },
        { TM_CAP_EXPERIMENTAL_KMOD, "experimental_kmod" },
        { TM_CAP_NVIDIA_CUDA, "nvidia_cuda" },
        { TM_CAP_PLATFORM_IPMI, "platform_ipmi" },
        { TM_CAP_PLATFORM_REDFISH, "platform_redfish" },
        { TM_CAP_NVIDIA_DCGM, "nvidia_dcgm" }
    };

    if (appendf(w, "[") != 0) {
        return -1;
    }
    for (size_t i = 0; i < sizeof(cap_names) / sizeof(cap_names[0]); ++i) {
        if ((caps & cap_names[i].bit) == 0) {
            continue;
        }
        if (!first && appendf(w, ",") != 0) {
            return -1;
        }
        first = 0;
        if (append_json_string(w, cap_names[i].name) != 0) {
            return -1;
        }
    }
    return appendf(w, "]");
}

int tm_snapshot_to_json(const tm_snapshot_t *snap, char *buf, size_t len) {
    tm_json_writer_t w = { .buf = buf, .len = len, .pos = 0, .failed = 0 };
    long long ts_ns = snapshot_timestamp_ns(snap);

    if (appendf(&w, "{") != 0) {
        return -1;
    }
    if (appendf(&w, "\"schema_version\":\"0.3.0\",") != 0) {
        return -1;
    }
    if (appendf(&w, "\"release_version\":\"%d.%d.%d\",", TM_VERSION_MAJOR, TM_VERSION_MINOR, TM_VERSION_PATCH) != 0) {
        return -1;
    }
    if (appendf(&w, "\"timestamp\":{\"sec\":%lld,\"nsec\":%ld,\"timestamp_ns\":%lld},",
                (long long)snap->timestamp.tv_sec, snap->timestamp.tv_nsec, ts_ns) != 0) {
        return -1;
    }
    if (appendf(&w, "\"arch\":") != 0 || append_json_string(&w, arch_name(snap->arch)) != 0) {
        return -1;
    }
    if (appendf(&w, ",\"capability_mask\":%u,\"capabilities\":", snap->capabilities) != 0) {
        return -1;
    }
    if (append_capabilities(&w, snap->capabilities) != 0) {
        return -1;
    }

    if (appendf(&w, ",\"cpu_packages\":[") != 0) {
        return -1;
    }
    for (int i = 0; i < snap->cpu_package_count; ++i) {
        const tm_cpu_package_t *pkg = &snap->cpu_packages[i];
        if (i > 0 && appendf(&w, ",") != 0) {
            return -1;
        }
        if (appendf(&w, "{\"package_id\":%d,\"metrics\":{", pkg->package_id) != 0) {
            return -1;
        }
        if (appendf(&w, "\"package_temp_c\":") != 0 || append_metric_number(&w, pkg->package_temp_c, "celsius", "coretemp", ts_ns) != 0 ||
            appendf(&w, ",\"tjmax_c\":") != 0 || append_metric_number(&w, pkg->tjmax_c, "celsius", "coretemp", ts_ns) != 0 ||
            appendf(&w, ",\"rapl_energy_uj\":") != 0 || append_metric_number(&w, pkg->rapl_energy_uj, "microjoules", "powercap", ts_ns) != 0 ||
            appendf(&w, ",\"power_limit_1_w\":") != 0 || append_metric_number(&w, pkg->power_limit_1_w, "watts", "powercap", ts_ns) != 0 ||
            appendf(&w, ",\"power_limit_2_w\":") != 0 || append_metric_number(&w, pkg->power_limit_2_w, "watts", "powercap", ts_ns) != 0 ||
            appendf(&w, "},\"cores\":[") != 0) {
            return -1;
        }
        for (int j = 0; j < pkg->core_count; ++j) {
            const tm_cpu_core_t *core = &pkg->cores[j];
            if (j > 0 && appendf(&w, ",") != 0) {
                return -1;
            }
            if (appendf(&w, "{\"core_id\":%d,\"metrics\":{", core->core_id) != 0) {
                return -1;
            }
            if (appendf(&w, "\"temp_c\":") != 0 || append_metric_number(&w, core->temp_c, "celsius", "coretemp", ts_ns) != 0 ||
                appendf(&w, ",\"crit_c\":") != 0 || append_metric_number(&w, core->crit_c, "celsius", "coretemp", ts_ns) != 0 ||
                appendf(&w, ",\"max_c\":") != 0 || append_metric_number(&w, core->max_c, "celsius", "coretemp", ts_ns) != 0 ||
                appendf(&w, ",\"throttling\":") != 0 || append_metric_bool(&w, core->throttling, "coretemp", ts_ns) != 0 ||
                appendf(&w, "}}") != 0) {
                return -1;
            }
        }
        if (appendf(&w, "]}") != 0) {
            return -1;
        }
    }
    if (appendf(&w, "]") != 0) {
        return -1;
    }

    if (appendf(&w, ",\"arm_clusters\":[") != 0) {
        return -1;
    }
    for (int i = 0; i < snap->arm_cluster_count; ++i) {
        const tm_arm_cluster_t *cluster = &snap->arm_clusters[i];
        if (i > 0 && appendf(&w, ",") != 0) {
            return -1;
        }
        if (appendf(&w, "{\"cluster_id\":%d,\"zone_name\":", cluster->cluster_id) != 0 ||
            append_json_string(&w, cluster->zone_name) != 0 ||
            appendf(&w, ",\"zone_type\":") != 0 ||
            append_json_string(&w, cluster->zone_type) != 0 ||
            appendf(&w, ",\"metrics\":{") != 0 ||
            appendf(&w, "\"temp_c\":") != 0 || append_metric_number(&w, cluster->temp_c, "celsius", "thermal_zone", ts_ns) != 0 ||
            appendf(&w, ",\"cur_freq_mhz\":") != 0 || append_metric_number(&w, cluster->cur_freq_mhz, "mhz", "cpufreq", ts_ns) != 0 ||
            appendf(&w, ",\"throttling\":") != 0 || append_metric_bool(&w, cluster->throttling, "thermal_zone", ts_ns) != 0 ||
            appendf(&w, "}}") != 0) {
            return -1;
        }
    }
    if (appendf(&w, "]") != 0) {
        return -1;
    }

    if (appendf(&w, ",\"nvidia_gpus\":[") != 0) {
        return -1;
    }
    for (int i = 0; i < snap->nvidia_gpu_count; ++i) {
        const tm_nvidia_gpu_t *gpu = &snap->nvidia_gpus[i];
        const char *src = nvidia_source(snap);
        if (i > 0 && appendf(&w, ",") != 0) {
            return -1;
        }
        if (appendf(&w, "{\"gpu_index\":%d,\"name\":", gpu->gpu_index) != 0 ||
            append_json_string(&w, gpu->name) != 0 ||
            appendf(&w, ",\"uuid\":") != 0 ||
            append_json_string(&w, gpu->uuid) != 0 ||
            appendf(&w, ",\"pci_bus_id\":") != 0 ||
            append_json_string(&w, gpu->pci_bus_id) != 0 ||
            appendf(&w, ",\"metrics\":{") != 0 ||
            appendf(&w, "\"gpu_temp_c\":") != 0 || append_metric_number(&w, gpu->gpu_temp_c, "celsius", src, ts_ns) != 0 ||
            appendf(&w, ",\"memory_temp_c\":") != 0 || append_metric_number(&w, gpu->memory_temp_c, "celsius", src, ts_ns) != 0 ||
            appendf(&w, ",\"power_draw_w\":") != 0 || append_metric_number(&w, gpu->power_draw_w, "watts", src, ts_ns) != 0 ||
            appendf(&w, ",\"power_limit_w\":") != 0 || append_metric_number(&w, gpu->power_limit_w, "watts", src, ts_ns) != 0 ||
            appendf(&w, ",\"sm_clock_mhz\":") != 0 || append_metric_number(&w, gpu->sm_clock_mhz, "mhz", src, ts_ns) != 0 ||
            appendf(&w, ",\"mem_clock_mhz\":") != 0 || append_metric_number(&w, gpu->mem_clock_mhz, "mhz", src, ts_ns) != 0 ||
            appendf(&w, ",\"gpu_util_pct\":") != 0 || append_metric_number(&w, gpu->gpu_util_pct, "percent", src, ts_ns) != 0 ||
            appendf(&w, ",\"mem_util_pct\":") != 0 || append_metric_number(&w, gpu->mem_util_pct, "percent", src, ts_ns) != 0 ||
            appendf(&w, ",\"throttle_reasons\":") != 0 || append_metric_number(&w, (double)gpu->throttle_reasons, "bitmask", src, ts_ns) != 0 ||
            appendf(&w, "},\"cuda\":{") != 0 ||
            appendf(&w, "\"ordinal\":") != 0 || append_metric_number(&w, gpu->cuda_ordinal, "index", "cuda", ts_ns) != 0 ||
            appendf(&w, ",\"compute_major\":") != 0 || append_metric_number(&w, gpu->cuda_compute_major, "version_component", "cuda", ts_ns) != 0 ||
            appendf(&w, ",\"compute_minor\":") != 0 || append_metric_number(&w, gpu->cuda_compute_minor, "version_component", "cuda", ts_ns) != 0 ||
            appendf(&w, ",\"multiprocessors\":") != 0 || append_metric_number(&w, gpu->cuda_multiprocessors, "count", "cuda", ts_ns) != 0 ||
            appendf(&w, ",\"driver_version\":") != 0 || append_metric_number(&w, gpu->cuda_driver_version, "version", "cuda", ts_ns) != 0 ||
            appendf(&w, ",\"runtime_version\":") != 0 || append_metric_number(&w, gpu->cuda_runtime_version, "version", "cuda", ts_ns) != 0 ||
            appendf(&w, ",\"total_memory_bytes\":") != 0 || append_metric_number(&w, (double)gpu->cuda_total_memory_bytes, "bytes", "cuda", ts_ns) != 0 ||
            appendf(&w, "}}") != 0) {
            return -1;
        }
    }
    if (appendf(&w, "]") != 0) {
        return -1;
    }

    if (appendf(&w, ",\"amd_gpus\":[") != 0) {
        return -1;
    }
    for (int i = 0; i < snap->amd_gpu_count; ++i) {
        const tm_amd_gpu_t *gpu = &snap->amd_gpus[i];
        const char *src = amd_source(snap);
        if (i > 0 && appendf(&w, ",") != 0) {
            return -1;
        }
        if (appendf(&w, "{\"gpu_index\":%d,\"name\":", gpu->gpu_index) != 0 ||
            append_json_string(&w, gpu->name) != 0 ||
            appendf(&w, ",\"metrics\":{") != 0 ||
            appendf(&w, "\"edge_temp_c\":") != 0 || append_metric_number(&w, gpu->edge_temp_c, "celsius", src, ts_ns) != 0 ||
            appendf(&w, ",\"junction_temp_c\":") != 0 || append_metric_number(&w, gpu->junction_temp_c, "celsius", src, ts_ns) != 0 ||
            appendf(&w, ",\"memory_temp_c\":") != 0 || append_metric_number(&w, gpu->memory_temp_c, "celsius", src, ts_ns) != 0 ||
            appendf(&w, ",\"avg_power_w\":") != 0 || append_metric_number(&w, gpu->avg_power_w, "watts", src, ts_ns) != 0 ||
            appendf(&w, ",\"power_cap_w\":") != 0 || append_metric_number(&w, gpu->power_cap_w, "watts", src, ts_ns) != 0 ||
            appendf(&w, ",\"thermal_throttling\":") != 0 || append_metric_bool(&w, gpu->thermal_throttling, src, ts_ns) != 0 ||
            appendf(&w, "}}") != 0) {
            return -1;
        }
    }
    if (appendf(&w, "]") != 0) {
        return -1;
    }

    if (appendf(&w, ",\"hwmon_sensors\":[") != 0) {
        return -1;
    }
    for (int i = 0; i < snap->hwmon_sensor_count; ++i) {
        const tm_sensor_t *sensor = &snap->hwmon_sensors[i];
        if (i > 0 && appendf(&w, ",") != 0) {
            return -1;
        }
        if (appendf(&w, "{\"name\":") != 0 ||
            append_json_string(&w, sensor->name) != 0 ||
            appendf(&w, ",\"label\":") != 0 ||
            append_json_string(&w, sensor->label) != 0 ||
            appendf(&w, ",\"metric\":") != 0 ||
            append_metric_number(&w, sensor->value, sensor->unit, "hwmon", ts_ns) != 0 ||
            appendf(&w, "}") != 0) {
            return -1;
        }
    }
    if (appendf(&w, "]") != 0) {
        return -1;
    }

    if (appendf(&w, ",\"thermal_zones\":[") != 0) {
        return -1;
    }
    for (int i = 0; i < snap->thermal_zone_count; ++i) {
        const tm_sensor_t *sensor = &snap->thermal_zones[i];
        if (i > 0 && appendf(&w, ",") != 0) {
            return -1;
        }
        if (appendf(&w, "{\"name\":") != 0 ||
            append_json_string(&w, sensor->name) != 0 ||
            appendf(&w, ",\"label\":") != 0 ||
            append_json_string(&w, sensor->label) != 0 ||
            appendf(&w, ",\"metric\":") != 0 ||
            append_metric_number(&w, sensor->value, sensor->unit, "thermal_zone", ts_ns) != 0 ||
            appendf(&w, "}") != 0) {
            return -1;
        }
    }
    if (appendf(&w, "]") != 0) {
        return -1;
    }

    if (appendf(&w, ",\"board_sensors\":[") != 0) {
        return -1;
    }
    for (int i = 0; i < snap->board_sensor_count; ++i) {
        const tm_board_sensor_t *sensor = &snap->board_sensors[i];
        if (i > 0 && appendf(&w, ",") != 0) {
            return -1;
        }
        if (appendf(&w, "{\"name\":") != 0 ||
            append_json_string(&w, sensor->name) != 0 ||
            appendf(&w, ",\"sensor_type\":") != 0 ||
            append_json_string(&w, sensor->sensor_type) != 0 ||
            appendf(&w, ",\"metric\":") != 0 ||
            append_metric_number(&w, sensor->value, sensor->unit, sensor->source, ts_ns) != 0 ||
            appendf(&w, "}") != 0) {
            return -1;
        }
    }
    if (appendf(&w, "]") != 0) {
        return -1;
    }

    if (appendf(&w, ",\"fan_sensors\":[") != 0) {
        return -1;
    }
    for (int i = 0; i < snap->fan_sensor_count; ++i) {
        const tm_fan_sensor_t *fan = &snap->fan_sensors[i];
        if (i > 0 && appendf(&w, ",") != 0) {
            return -1;
        }
        if (appendf(&w, "{\"name\":") != 0 ||
            append_json_string(&w, fan->name) != 0 ||
            appendf(&w, ",\"metrics\":{\"rpm\":") != 0 ||
            append_metric_number(&w, fan->rpm, "rpm", fan->source, ts_ns) != 0 ||
            appendf(&w, ",\"pwm_pct\":") != 0 ||
            append_metric_number(&w, fan->pwm_pct, "percent", fan->source, ts_ns) != 0 ||
            appendf(&w, "}}") != 0) {
            return -1;
        }
    }
    if (appendf(&w, "]") != 0) {
        return -1;
    }

    if (appendf(&w, ",\"psu_sensors\":[") != 0) {
        return -1;
    }
    for (int i = 0; i < snap->psu_sensor_count; ++i) {
        const tm_psu_sensor_t *psu = &snap->psu_sensors[i];
        if (i > 0 && appendf(&w, ",") != 0) {
            return -1;
        }
        if (appendf(&w, "{\"name\":") != 0 ||
            append_json_string(&w, psu->name) != 0 ||
            appendf(&w, ",\"metrics\":{\"inlet_temp_c\":") != 0 ||
            append_metric_number(&w, psu->inlet_temp_c, "celsius", psu->source, ts_ns) != 0 ||
            appendf(&w, ",\"exhaust_temp_c\":") != 0 ||
            append_metric_number(&w, psu->exhaust_temp_c, "celsius", psu->source, ts_ns) != 0 ||
            appendf(&w, ",\"power_w\":") != 0 ||
            append_metric_number(&w, psu->power_w, "watts", psu->source, ts_ns) != 0 ||
            appendf(&w, ",\"present\":") != 0 ||
            append_metric_bool(&w, psu->present, psu->source, ts_ns) != 0 ||
            appendf(&w, "}}") != 0) {
            return -1;
        }
    }
    if (appendf(&w, "]") != 0) {
        return -1;
    }

    if (appendf(&w,
                ",\"summary\":{"
                "\"cpu_package_count\":%d,"
                "\"arm_cluster_count\":%d,"
                "\"nvidia_gpu_count\":%d,"
                "\"amd_gpu_count\":%d,"
                "\"hwmon_sensor_count\":%d,"
                "\"thermal_zone_count\":%d,"
                "\"board_sensor_count\":%d,"
                "\"fan_sensor_count\":%d,"
                "\"psu_sensor_count\":%d"
                "}"
                "}",
                snap->cpu_package_count,
                snap->arm_cluster_count,
                snap->nvidia_gpu_count,
                snap->amd_gpu_count,
                snap->hwmon_sensor_count,
                snap->thermal_zone_count,
                snap->board_sensor_count,
                snap->fan_sensor_count,
                snap->psu_sensor_count) != 0) {
        return -1;
    }

    return w.failed ? -1 : (int)w.pos;
}
