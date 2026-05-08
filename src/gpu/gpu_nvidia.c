#define _GNU_SOURCE
#include "../../include/thermal_monitor.h"

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

typedef void *nvmlDevice_t;
typedef int nvmlReturn_t;
typedef struct {
    unsigned int gpu;
    unsigned int memory;
} nvmlUtilization_t;

#define NVML_SUCCESS 0
#define NVML_TEMPERATURE_GPU 0
#define NVML_TEMPERATURE_MEM 2

static void *nvml_handle;
static nvmlReturn_t (*p_nvmlInit)(void);
static nvmlReturn_t (*p_nvmlShutdown)(void);
static nvmlReturn_t (*p_nvmlDeviceGetCount)(unsigned int *);
static nvmlReturn_t (*p_nvmlDeviceGetHandleByIndex)(unsigned int, nvmlDevice_t *);
static nvmlReturn_t (*p_nvmlDeviceGetName)(nvmlDevice_t, char *, unsigned int);
static nvmlReturn_t (*p_nvmlDeviceGetUUID)(nvmlDevice_t, char *, unsigned int);
static nvmlReturn_t (*p_nvmlDeviceGetTemperature)(nvmlDevice_t, unsigned int, unsigned int *);
static nvmlReturn_t (*p_nvmlDeviceGetPowerUsage)(nvmlDevice_t, unsigned int *);
static nvmlReturn_t (*p_nvmlDeviceGetEnforcedPowerLimit)(nvmlDevice_t, unsigned int *);
static nvmlReturn_t (*p_nvmlDeviceGetUtilizationRates)(nvmlDevice_t, nvmlUtilization_t *);
static nvmlReturn_t (*p_nvmlDeviceGetCurrentClocksThrottleReasons)(nvmlDevice_t, unsigned long long *);

static int load_nvml(void) {
    const char *paths[] = {
        "libnvidia-ml.so.1",
        "libnvidia-ml.so",
        "/usr/local/cuda/lib64/libnvidia-ml.so.1",
        NULL
    };

    for (int i = 0; paths[i] != NULL; ++i) {
        nvml_handle = dlopen(paths[i], RTLD_NOW | RTLD_LOCAL);
        if (nvml_handle) {
            break;
        }
    }
    if (!nvml_handle) {
        return -1;
    }

#define LOAD(name) p_##name = dlsym(nvml_handle, #name)
    LOAD(nvmlInit);
    LOAD(nvmlShutdown);
    LOAD(nvmlDeviceGetCount);
    LOAD(nvmlDeviceGetHandleByIndex);
    LOAD(nvmlDeviceGetName);
    LOAD(nvmlDeviceGetUUID);
    LOAD(nvmlDeviceGetTemperature);
    LOAD(nvmlDeviceGetPowerUsage);
    LOAD(nvmlDeviceGetEnforcedPowerLimit);
    LOAD(nvmlDeviceGetUtilizationRates);
    LOAD(nvmlDeviceGetCurrentClocksThrottleReasons);
#undef LOAD
    return p_nvmlInit && p_nvmlShutdown && p_nvmlDeviceGetCount ? 0 : -1;
}

int tm_collect_nvidia(tm_context_t *ctx, tm_snapshot_t *snap) {
    unsigned int count = 0;

    (void)ctx;

    if (load_nvml() != 0) {
        return 0;
    }
    if (p_nvmlInit() != NVML_SUCCESS) {
        return 0;
    }
    if (p_nvmlDeviceGetCount(&count) != NVML_SUCCESS) {
        p_nvmlShutdown();
        return 0;
    }

    for (unsigned int i = 0; i < count && snap->nvidia_gpu_count < TM_MAX_NVIDIA_GPUS; ++i) {
        tm_nvidia_gpu_t *gpu = &snap->nvidia_gpus[snap->nvidia_gpu_count];
        nvmlDevice_t dev = NULL;
        unsigned int value = 0;
        nvmlUtilization_t util = {0};
        unsigned long long throttle = 0;

        if (p_nvmlDeviceGetHandleByIndex(i, &dev) != NVML_SUCCESS) {
            continue;
        }

        memset(gpu, 0, sizeof(*gpu));
        gpu->gpu_index = (int)i;
        p_nvmlDeviceGetName(dev, gpu->name, sizeof(gpu->name));
        p_nvmlDeviceGetUUID(dev, gpu->uuid, sizeof(gpu->uuid));

        if (p_nvmlDeviceGetTemperature(dev, NVML_TEMPERATURE_GPU, &value) == NVML_SUCCESS) {
            gpu->gpu_temp_c = (double)value;
        }
        if (p_nvmlDeviceGetTemperature(dev, NVML_TEMPERATURE_MEM, &value) == NVML_SUCCESS) {
            gpu->memory_temp_c = (double)value;
        }
        if (p_nvmlDeviceGetPowerUsage(dev, &value) == NVML_SUCCESS) {
            gpu->power_draw_w = (double)value / 1000.0;
        }
        if (p_nvmlDeviceGetEnforcedPowerLimit(dev, &value) == NVML_SUCCESS) {
            gpu->power_limit_w = (double)value / 1000.0;
        }
        if (p_nvmlDeviceGetUtilizationRates(dev, &util) == NVML_SUCCESS) {
            gpu->gpu_util_pct = util.gpu;
            gpu->mem_util_pct = util.memory;
        }
        if (p_nvmlDeviceGetCurrentClocksThrottleReasons(dev, &throttle) == NVML_SUCCESS) {
            gpu->throttle_reasons = throttle;
        }
        snap->nvidia_gpu_count++;
    }

    if (snap->nvidia_gpu_count > 0) {
        snap->capabilities |= TM_CAP_NVIDIA_NVML;
    }
    p_nvmlShutdown();
    return 0;
}

