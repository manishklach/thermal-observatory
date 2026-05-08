#ifndef THERMAL_MONITOR_H
#define THERMAL_MONITOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define TM_VERSION_MAJOR 0
#define TM_VERSION_MINOR 1
#define TM_VERSION_PATCH 0

#define TM_MAX_CPU_PACKAGES 16
#define TM_MAX_CPU_CORES 512
#define TM_MAX_ARM_CLUSTERS 64
#define TM_MAX_NVIDIA_GPUS 16
#define TM_MAX_AMD_GPUS 16
#define TM_MAX_HWMON_SENSORS 128
#define TM_MAX_THERMAL_ZONES 128
#define TM_MAX_BOARD_SENSORS 128
#define TM_MAX_FANS 64
#define TM_MAX_PSUS 16

typedef enum {
    TM_CAP_LINUX_HWMON      = 1u << 0,
    TM_CAP_LINUX_THERMAL    = 1u << 1,
    TM_CAP_LINUX_RAPL       = 1u << 2,
    TM_CAP_X86_MSR          = 1u << 3,
    TM_CAP_ARM_SCMI         = 1u << 4,
    TM_CAP_NVIDIA_NVML      = 1u << 5,
    TM_CAP_AMD_ROCM_SMI     = 1u << 6,
    TM_CAP_AMDGPU_HWMON     = 1u << 7,
    TM_CAP_EXPERIMENTAL_KMOD = 1u << 8,
    TM_CAP_NVIDIA_CUDA      = 1u << 9,
    TM_CAP_PLATFORM_IPMI    = 1u << 10,
    TM_CAP_PLATFORM_REDFISH = 1u << 11,
    TM_CAP_NVIDIA_DCGM      = 1u << 12
} tm_capability_t;

typedef enum {
    TM_ARCH_UNKNOWN = 0,
    TM_ARCH_X86_64,
    TM_ARCH_ARM64
} tm_arch_t;

typedef struct {
    int core_id;
    double temp_c;
    double crit_c;
    double max_c;
    bool throttling;
} tm_cpu_core_t;

typedef struct {
    int package_id;
    double package_temp_c;
    double tjmax_c;
    double rapl_energy_uj;
    double power_limit_1_w;
    double power_limit_2_w;
    int core_count;
    tm_cpu_core_t cores[TM_MAX_CPU_CORES];
} tm_cpu_package_t;

typedef struct {
    int cluster_id;
    char zone_name[64];
    char zone_type[64];
    double temp_c;
    double cur_freq_mhz;
    bool throttling;
} tm_arm_cluster_t;

typedef struct {
    int gpu_index;
    char name[128];
    char uuid[96];
    char pci_bus_id[32];
    double gpu_temp_c;
    double memory_temp_c;
    double power_draw_w;
    double power_limit_w;
    uint32_t sm_clock_mhz;
    uint32_t mem_clock_mhz;
    uint32_t gpu_util_pct;
    uint32_t mem_util_pct;
    uint64_t throttle_reasons;
    int cuda_ordinal;
    int cuda_compute_major;
    int cuda_compute_minor;
    int cuda_multiprocessors;
    int cuda_driver_version;
    int cuda_runtime_version;
    size_t cuda_total_memory_bytes;
} tm_nvidia_gpu_t;

typedef struct {
    int gpu_index;
    char name[128];
    double edge_temp_c;
    double junction_temp_c;
    double memory_temp_c;
    double avg_power_w;
    double power_cap_w;
    bool thermal_throttling;
} tm_amd_gpu_t;

typedef struct {
    char name[64];
    char label[64];
    double value;
    char unit[16];
} tm_sensor_t;

typedef struct {
    char name[64];
    char sensor_type[32];
    char source[32];
    double value;
    char unit[16];
} tm_board_sensor_t;

typedef struct {
    char name[64];
    char source[32];
    double rpm;
    double pwm_pct;
} tm_fan_sensor_t;

typedef struct {
    char name[64];
    char source[32];
    double inlet_temp_c;
    double exhaust_temp_c;
    double power_w;
    bool present;
} tm_psu_sensor_t;

typedef struct {
    struct timespec timestamp;
    tm_arch_t arch;
    uint32_t capabilities;

    int cpu_package_count;
    tm_cpu_package_t cpu_packages[TM_MAX_CPU_PACKAGES];

    int arm_cluster_count;
    tm_arm_cluster_t arm_clusters[TM_MAX_ARM_CLUSTERS];

    int nvidia_gpu_count;
    tm_nvidia_gpu_t nvidia_gpus[TM_MAX_NVIDIA_GPUS];

    int amd_gpu_count;
    tm_amd_gpu_t amd_gpus[TM_MAX_AMD_GPUS];

    int hwmon_sensor_count;
    tm_sensor_t hwmon_sensors[TM_MAX_HWMON_SENSORS];

    int thermal_zone_count;
    tm_sensor_t thermal_zones[TM_MAX_THERMAL_ZONES];

    int board_sensor_count;
    tm_board_sensor_t board_sensors[TM_MAX_BOARD_SENSORS];

    int fan_sensor_count;
    tm_fan_sensor_t fan_sensors[TM_MAX_FANS];

    int psu_sensor_count;
    tm_psu_sensor_t psu_sensors[TM_MAX_PSUS];
} tm_snapshot_t;

typedef struct {
    tm_arch_t arch;
    bool want_json;
    bool want_experimental_kernel;
} tm_context_t;

void tm_context_init(tm_context_t *ctx);
int tm_collect_snapshot(tm_context_t *ctx, tm_snapshot_t *snap);
void tm_print_snapshot_text(const tm_snapshot_t *snap);
int tm_snapshot_to_json(const tm_snapshot_t *snap, char *buf, size_t len);
int tm_snapshot_to_prometheus(const tm_snapshot_t *snap, char *buf, size_t len);

int tm_collect_generic_linux(tm_context_t *ctx, tm_snapshot_t *snap);
int tm_collect_x86(tm_context_t *ctx, tm_snapshot_t *snap);
int tm_collect_arm64(tm_context_t *ctx, tm_snapshot_t *snap);
int tm_collect_nvidia(tm_context_t *ctx, tm_snapshot_t *snap);
int tm_collect_amd(tm_context_t *ctx, tm_snapshot_t *snap);
int tm_collect_nvidia_cuda(tm_context_t *ctx, tm_snapshot_t *snap);
int tm_collect_nvidia_dcgm(tm_context_t *ctx, tm_snapshot_t *snap);
int tm_collect_ipmi(tm_context_t *ctx, tm_snapshot_t *snap);
int tm_collect_redfish(tm_context_t *ctx, tm_snapshot_t *snap);

#endif
