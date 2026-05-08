#include "../../include/thermal_monitor.h"

#include <stdio.h>

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

void tm_print_snapshot_text(const tm_snapshot_t *snap) {
    printf("Thermal Observatory\n");
    printf("  arch: %s\n", arch_name(snap->arch));
    printf("  capabilities: 0x%08x\n", snap->capabilities);

    for (int i = 0; i < snap->cpu_package_count; ++i) {
        const tm_cpu_package_t *pkg = &snap->cpu_packages[i];
        printf("CPU package %d: temp=%.1fC tjmax=%.1fC rapl_energy=%.0f uJ\n",
               pkg->package_id,
               pkg->package_temp_c,
               pkg->tjmax_c,
               pkg->rapl_energy_uj);
        for (int j = 0; j < pkg->core_count; ++j) {
            const tm_cpu_core_t *core = &pkg->cores[j];
            printf("  core %d: temp=%.1fC crit=%.1fC max=%.1fC\n",
                   core->core_id, core->temp_c, core->crit_c, core->max_c);
        }
    }

    for (int i = 0; i < snap->arm_cluster_count; ++i) {
        const tm_arm_cluster_t *cluster = &snap->arm_clusters[i];
        printf("ARM cluster %d: zone=%s type=%s temp=%.1fC freq=%.0fMHz\n",
               cluster->cluster_id,
               cluster->zone_name,
               cluster->zone_type,
               cluster->temp_c,
               cluster->cur_freq_mhz);
    }

    for (int i = 0; i < snap->nvidia_gpu_count; ++i) {
        const tm_nvidia_gpu_t *gpu = &snap->nvidia_gpus[i];
        printf("NVIDIA GPU %d: %s temp=%.1fC mem=%.1fC power=%.1f/%.1fW util=%u%% mem=%u%%\n",
               gpu->gpu_index,
               gpu->name,
               gpu->gpu_temp_c,
               gpu->memory_temp_c,
               gpu->power_draw_w,
               gpu->power_limit_w,
               gpu->gpu_util_pct,
               gpu->mem_util_pct);
    }

    for (int i = 0; i < snap->amd_gpu_count; ++i) {
        const tm_amd_gpu_t *gpu = &snap->amd_gpus[i];
        printf("AMD GPU %d: %s edge=%.1fC junction=%.1fC mem=%.1fC power=%.1f/%.1fW\n",
               gpu->gpu_index,
               gpu->name,
               gpu->edge_temp_c,
               gpu->junction_temp_c,
               gpu->memory_temp_c,
               gpu->avg_power_w,
               gpu->power_cap_w);
    }
}

