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

    if (appendf(&w, "{") != 0) {
        return -1;
    }
    if (appendf(&w, "\"schema_version\":\"0.2.0\",") != 0) {
        return -1;
    }
    if (appendf(&w, "\"timestamp\":{\"sec\":%lld,\"nsec\":%ld},",
                (long long)snap->timestamp.tv_sec,
                snap->timestamp.tv_nsec) != 0) {
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
        if (appendf(&w,
                    "{"
                    "\"package_id\":%d,"
                    "\"source\":\"coretemp+powercap\","
                    "\"package_temp_c\":%.3f,"
                    "\"tjmax_c\":%.3f,"
                    "\"rapl_energy_uj\":%.3f,"
                    "\"power_limit_1_w\":%.3f,"
                    "\"power_limit_2_w\":%.3f,"
                    "\"cores\":[",
                    pkg->package_id,
                    pkg->package_temp_c,
                    pkg->tjmax_c,
                    pkg->rapl_energy_uj,
                    pkg->power_limit_1_w,
                    pkg->power_limit_2_w) != 0) {
            return -1;
        }
        for (int j = 0; j < pkg->core_count; ++j) {
            const tm_cpu_core_t *core = &pkg->cores[j];
            if (j > 0 && appendf(&w, ",") != 0) {
                return -1;
            }
            if (appendf(&w,
                        "{"
                        "\"core_id\":%d,"
                        "\"temp_c\":%.3f,"
                        "\"crit_c\":%.3f,"
                        "\"max_c\":%.3f,"
                        "\"throttling\":%s"
                        "}",
                        core->core_id,
                        core->temp_c,
                        core->crit_c,
                        core->max_c,
                        core->throttling ? "true" : "false") != 0) {
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
        if (appendf(&w,
                    "{"
                    "\"cluster_id\":%d,"
                    "\"source\":\"thermal_zone+cpufreq\","
                    "\"zone_name\":",
                    cluster->cluster_id) != 0 ||
            append_json_string(&w, cluster->zone_name) != 0 ||
            appendf(&w, ",\"zone_type\":") != 0 ||
            append_json_string(&w, cluster->zone_type) != 0 ||
            appendf(&w,
                    ",\"temp_c\":%.3f,"
                    "\"cur_freq_mhz\":%.3f,"
                    "\"throttling\":%s"
                    "}",
                    cluster->temp_c,
                    cluster->cur_freq_mhz,
                    cluster->throttling ? "true" : "false") != 0) {
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
        if (i > 0 && appendf(&w, ",") != 0) {
            return -1;
        }
        if (appendf(&w,
                    "{"
                    "\"gpu_index\":%d,"
                    "\"source\":",
                    gpu->gpu_index) != 0 ||
            append_json_string(&w, nvidia_source(snap)) != 0 ||
            appendf(&w, ",\"name\":") != 0 ||
            append_json_string(&w, gpu->name) != 0 ||
            appendf(&w, ",\"uuid\":") != 0 ||
            append_json_string(&w, gpu->uuid) != 0 ||
            appendf(&w, ",\"pci_bus_id\":") != 0 ||
            append_json_string(&w, gpu->pci_bus_id) != 0 ||
            appendf(&w,
                    ",\"gpu_temp_c\":%.3f,"
                    "\"memory_temp_c\":%.3f,"
                    "\"power_draw_w\":%.3f,"
                    "\"power_limit_w\":%.3f,"
                    "\"sm_clock_mhz\":%u,"
                    "\"mem_clock_mhz\":%u,"
                    "\"gpu_util_pct\":%u,"
                    "\"mem_util_pct\":%u,"
                    "\"throttle_reasons\":%llu,"
                    "\"cuda\":{"
                    "\"ordinal\":%d,"
                    "\"compute_capability\":\"%d.%d\","
                    "\"multiprocessors\":%d,"
                    "\"driver_version\":%d,"
                    "\"runtime_version\":%d,"
                    "\"total_memory_bytes\":%llu"
                    "}"
                    "}",
                    gpu->gpu_temp_c,
                    gpu->memory_temp_c,
                    gpu->power_draw_w,
                    gpu->power_limit_w,
                    gpu->sm_clock_mhz,
                    gpu->mem_clock_mhz,
                    gpu->gpu_util_pct,
                    gpu->mem_util_pct,
                    (unsigned long long)gpu->throttle_reasons,
                    gpu->cuda_ordinal,
                    gpu->cuda_compute_major,
                    gpu->cuda_compute_minor,
                    gpu->cuda_multiprocessors,
                    gpu->cuda_driver_version,
                    gpu->cuda_runtime_version,
                    (unsigned long long)gpu->cuda_total_memory_bytes) != 0) {
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
        if (i > 0 && appendf(&w, ",") != 0) {
            return -1;
        }
        if (appendf(&w,
                    "{"
                    "\"gpu_index\":%d,"
                    "\"source\":",
                    gpu->gpu_index) != 0 ||
            append_json_string(&w, amd_source(snap)) != 0 ||
            appendf(&w, ",\"name\":") != 0 ||
            append_json_string(&w, gpu->name) != 0 ||
            appendf(&w,
                    ",\"edge_temp_c\":%.3f,"
                    "\"junction_temp_c\":%.3f,"
                    "\"memory_temp_c\":%.3f,"
                    "\"avg_power_w\":%.3f,"
                    "\"power_cap_w\":%.3f,"
                    "\"thermal_throttling\":%s"
                    "}",
                    gpu->edge_temp_c,
                    gpu->junction_temp_c,
                    gpu->memory_temp_c,
                    gpu->avg_power_w,
                    gpu->power_cap_w,
                    gpu->thermal_throttling ? "true" : "false") != 0) {
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
        if (appendf(&w, "{\"source\":\"hwmon\",\"name\":") != 0 ||
            append_json_string(&w, sensor->name) != 0 ||
            appendf(&w, ",\"label\":") != 0 ||
            append_json_string(&w, sensor->label) != 0 ||
            appendf(&w, ",\"value\":%.3f,\"unit\":", sensor->value) != 0 ||
            append_json_string(&w, sensor->unit) != 0 ||
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
        if (appendf(&w, "{\"source\":\"thermal_zone\",\"name\":") != 0 ||
            append_json_string(&w, sensor->name) != 0 ||
            appendf(&w, ",\"label\":") != 0 ||
            append_json_string(&w, sensor->label) != 0 ||
            appendf(&w, ",\"value\":%.3f,\"unit\":", sensor->value) != 0 ||
            append_json_string(&w, sensor->unit) != 0 ||
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
            appendf(&w, ",\"source\":") != 0 ||
            append_json_string(&w, sensor->source) != 0 ||
            appendf(&w, ",\"value\":%.3f,\"unit\":", sensor->value) != 0 ||
            append_json_string(&w, sensor->unit) != 0 ||
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
            appendf(&w, ",\"source\":") != 0 ||
            append_json_string(&w, fan->source) != 0 ||
            appendf(&w, ",\"rpm\":%.3f,\"pwm_pct\":%.3f}", fan->rpm, fan->pwm_pct) != 0) {
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
            appendf(&w, ",\"source\":") != 0 ||
            append_json_string(&w, psu->source) != 0 ||
            appendf(&w,
                    ",\"inlet_temp_c\":%.3f,\"exhaust_temp_c\":%.3f,\"power_w\":%.3f,\"present\":%s}",
                    psu->inlet_temp_c,
                    psu->exhaust_temp_c,
                    psu->power_w,
                    psu->present ? "true" : "false") != 0) {
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
