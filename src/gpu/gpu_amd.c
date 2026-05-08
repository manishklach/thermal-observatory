#define _GNU_SOURCE
#include "../../include/thermal_monitor.h"
#include "../platform/linux_paths.h"

#include <dlfcn.h>
#include <errno.h>
#include <glob.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef int rsmi_status_t;
#define RSMI_STATUS_SUCCESS 0
#define RSMI_TEMP_CURRENT 0
#define RSMI_TEMP_TYPE_EDGE 0
#define RSMI_TEMP_TYPE_JUNCTION 1
#define RSMI_TEMP_TYPE_VRAM 2

static void *rsmi_handle;
static rsmi_status_t (*p_rsmi_init)(uint64_t);
static rsmi_status_t (*p_rsmi_shut_down)(void);
static rsmi_status_t (*p_rsmi_num_monitor_devices)(uint32_t *);
static rsmi_status_t (*p_rsmi_dev_name_get)(uint32_t, char *, size_t);
static rsmi_status_t (*p_rsmi_dev_temp_metric_get)(uint32_t, uint32_t, int, int64_t *);
static rsmi_status_t (*p_rsmi_dev_power_ave_get)(uint32_t, uint32_t, uint64_t *);
static rsmi_status_t (*p_rsmi_dev_power_cap_get)(uint32_t, uint32_t, uint64_t *);

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

static int load_rsmi(void) {
    const char *paths[] = {
        "librocm_smi64.so",
        "librocm_smi64.so.1",
        "/opt/rocm/lib/librocm_smi64.so",
        NULL
    };

    for (int i = 0; paths[i] != NULL; ++i) {
        rsmi_handle = dlopen(paths[i], RTLD_NOW | RTLD_LOCAL);
        if (rsmi_handle) {
            break;
        }
    }
    if (!rsmi_handle) {
        return -1;
    }

#define LOAD(name) p_##name = dlsym(rsmi_handle, #name)
    LOAD(rsmi_init);
    LOAD(rsmi_shut_down);
    LOAD(rsmi_num_monitor_devices);
    LOAD(rsmi_dev_name_get);
    LOAD(rsmi_dev_temp_metric_get);
    LOAD(rsmi_dev_power_ave_get);
    LOAD(rsmi_dev_power_cap_get);
#undef LOAD
    return p_rsmi_init && p_rsmi_shut_down && p_rsmi_num_monitor_devices ? 0 : -1;
}

static int collect_amdgpu_hwmon(tm_snapshot_t *snap) {
    glob_t names;
    char pattern[256];

    tm_glob_join(pattern, sizeof(pattern), "/sys/class/drm/card*/device/hwmon/hwmon*/name");
    if (glob(pattern, 0, NULL, &names) != 0) {
        globfree(&names);
        return 0;
    }

    for (int i = 0; i < (int)names.gl_pathc && snap->amd_gpu_count < TM_MAX_AMD_GPUS; ++i) {
        FILE *fp = fopen(names.gl_pathv[i], "r");
        char name[64] = {0};
        char base[256];
        long value = 0;
        tm_amd_gpu_t *gpu;

        if (!fp) {
            continue;
        }
        if (!fgets(name, sizeof(name), fp)) {
            fclose(fp);
            continue;
        }
        fclose(fp);
        name[strcspn(name, "\n")] = '\0';
        if (strcmp(name, "amdgpu") != 0) {
            continue;
        }

        gpu = &snap->amd_gpus[snap->amd_gpu_count];
        memset(gpu, 0, sizeof(*gpu));
        gpu->gpu_index = snap->amd_gpu_count;
        strncpy(gpu->name, "amdgpu", sizeof(gpu->name) - 1);

        strncpy(base, names.gl_pathv[i], sizeof(base) - 1);
        *strstr(base, "/name") = '\0';

        {
            char path[256];
            snprintf(path, sizeof(path), "%s/temp1_input", base);
            if (read_long(path, &value) == 0) {
                gpu->edge_temp_c = (double)value / 1000.0;
            }
            snprintf(path, sizeof(path), "%s/temp2_input", base);
            if (read_long(path, &value) == 0) {
                gpu->junction_temp_c = (double)value / 1000.0;
            }
            snprintf(path, sizeof(path), "%s/temp3_input", base);
            if (read_long(path, &value) == 0) {
                gpu->memory_temp_c = (double)value / 1000.0;
            }
            snprintf(path, sizeof(path), "%s/power1_average", base);
            if (read_long(path, &value) == 0) {
                gpu->avg_power_w = (double)value / 1000000.0;
            }
            snprintf(path, sizeof(path), "%s/power1_cap", base);
            if (read_long(path, &value) == 0) {
                gpu->power_cap_w = (double)value / 1000000.0;
            }
        }

        snap->amd_gpu_count++;
    }
    if (snap->amd_gpu_count > 0) {
        snap->capabilities |= TM_CAP_AMDGPU_HWMON;
    }
    globfree(&names);
    return 0;
}

int tm_collect_amd(tm_context_t *ctx, tm_snapshot_t *snap) {
    uint32_t count = 0;

    (void)ctx;

    if (load_rsmi() == 0 && p_rsmi_init(0) == RSMI_STATUS_SUCCESS &&
        p_rsmi_num_monitor_devices(&count) == RSMI_STATUS_SUCCESS) {
        for (uint32_t i = 0; i < count && snap->amd_gpu_count < TM_MAX_AMD_GPUS; ++i) {
            tm_amd_gpu_t *gpu = &snap->amd_gpus[snap->amd_gpu_count];
            int64_t temp = 0;
            uint64_t power = 0;

            memset(gpu, 0, sizeof(*gpu));
            gpu->gpu_index = (int)i;
            p_rsmi_dev_name_get(i, gpu->name, sizeof(gpu->name));
            if (p_rsmi_dev_temp_metric_get(i, RSMI_TEMP_TYPE_EDGE, RSMI_TEMP_CURRENT, &temp) == RSMI_STATUS_SUCCESS) {
                gpu->edge_temp_c = (double)temp / 1000.0;
            }
            if (p_rsmi_dev_temp_metric_get(i, RSMI_TEMP_TYPE_JUNCTION, RSMI_TEMP_CURRENT, &temp) == RSMI_STATUS_SUCCESS) {
                gpu->junction_temp_c = (double)temp / 1000.0;
            }
            if (p_rsmi_dev_temp_metric_get(i, RSMI_TEMP_TYPE_VRAM, RSMI_TEMP_CURRENT, &temp) == RSMI_STATUS_SUCCESS) {
                gpu->memory_temp_c = (double)temp / 1000.0;
            }
            if (p_rsmi_dev_power_ave_get(i, 0, &power) == RSMI_STATUS_SUCCESS) {
                gpu->avg_power_w = (double)power / 1000000.0;
            }
            if (p_rsmi_dev_power_cap_get(i, 0, &power) == RSMI_STATUS_SUCCESS) {
                gpu->power_cap_w = (double)power / 1000000.0;
            }
            snap->amd_gpu_count++;
        }
        if (snap->amd_gpu_count > 0) {
            snap->capabilities |= TM_CAP_AMD_ROCM_SMI;
        }
        p_rsmi_shut_down();
        return 0;
    }

    return collect_amdgpu_hwmon(snap);
}
