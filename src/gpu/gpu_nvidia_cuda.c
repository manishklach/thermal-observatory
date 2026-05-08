#define _GNU_SOURCE
#include "../../include/thermal_monitor.h"

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

typedef int cudaError_t;
typedef struct cudaDeviceProp {
    char name[256];
    size_t totalGlobalMem;
    size_t sharedMemPerBlock;
    int regsPerBlock;
    int warpSize;
    size_t memPitch;
    int maxThreadsPerBlock;
    int maxThreadsDim[3];
    int maxGridSize[3];
    int clockRate;
    size_t totalConstMem;
    int major;
    int minor;
    size_t textureAlignment;
    size_t texturePitchAlignment;
    int deviceOverlap;
    int multiProcessorCount;
    int kernelExecTimeoutEnabled;
    int integrated;
    int canMapHostMemory;
    int computeMode;
    int maxTexture1D;
    int maxTexture1DMipmap;
    int maxTexture1DLinear;
    int maxTexture2D[2];
    int maxTexture2DMipmap[2];
    int maxTexture2DLinear[3];
    int maxTexture2DGather[2];
    int maxTexture3D[3];
    int maxTexture3DAlt[3];
    int maxTextureCubemap;
    int maxTexture1DLayered[2];
    int maxTexture2DLayered[3];
    int maxTextureCubemapLayered[2];
    int maxSurface1D;
    int maxSurface2D[2];
    int maxSurface3D[3];
    int maxSurface1DLayered[2];
    int maxSurface2DLayered[3];
    int maxSurfaceCubemap;
    int maxSurfaceCubemapLayered[2];
    size_t surfaceAlignment;
    int concurrentKernels;
    int ECCEnabled;
    int pciBusID;
    int pciDeviceID;
    int pciDomainID;
    int tccDriver;
    int asyncEngineCount;
    int unifiedAddressing;
    int memoryClockRate;
    int memoryBusWidth;
    int l2CacheSize;
    int persistingL2CacheMaxSize;
    int maxThreadsPerMultiProcessor;
    int streamPrioritiesSupported;
    int globalL1CacheSupported;
    int localL1CacheSupported;
    size_t sharedMemPerMultiprocessor;
    int regsPerMultiprocessor;
    int managedMemory;
    int isMultiGpuBoard;
    int multiGpuBoardGroupID;
    int hostNativeAtomicSupported;
    int singleToDoublePrecisionPerfRatio;
    int pageableMemoryAccess;
    int concurrentManagedAccess;
    int computePreemptionSupported;
    int canUseHostPointerForRegisteredMem;
    int cooperativeLaunch;
    int cooperativeMultiDeviceLaunch;
    size_t sharedMemPerBlockOptin;
    int pageableMemoryAccessUsesHostPageTables;
    int directManagedMemAccessFromHost;
    int maxBlocksPerMultiProcessor;
    int accessPolicyMaxWindowSize;
    size_t reservedSharedMemPerBlock;
} cudaDeviceProp;

#define CUDA_SUCCESS 0

static void *cuda_handle;
static cudaError_t (*p_cudaGetDeviceCount)(int *);
static cudaError_t (*p_cudaGetDeviceProperties)(cudaDeviceProp *, int);
static cudaError_t (*p_cudaDriverGetVersion)(int *);
static cudaError_t (*p_cudaRuntimeGetVersion)(int *);

static int load_cuda_runtime(void) {
    const char *paths[] = {
        "libcudart.so",
        "libcudart.so.12",
        "/usr/local/cuda/lib64/libcudart.so",
        NULL
    };

    for (int i = 0; paths[i] != NULL; ++i) {
        cuda_handle = dlopen(paths[i], RTLD_NOW | RTLD_LOCAL);
        if (cuda_handle) {
            break;
        }
    }
    if (!cuda_handle) {
        return -1;
    }

#define LOAD(name) p_##name = dlsym(cuda_handle, #name)
    LOAD(cudaGetDeviceCount);
    LOAD(cudaGetDeviceProperties);
    LOAD(cudaDriverGetVersion);
    LOAD(cudaRuntimeGetVersion);
#undef LOAD
    return p_cudaGetDeviceCount && p_cudaGetDeviceProperties ? 0 : -1;
}

static int match_by_pci(const tm_snapshot_t *snap, int domain, int bus, int device) {
    char wanted[32];

    snprintf(wanted, sizeof(wanted), "%04x:%02x:%02x.0", domain, bus, device);
    for (int i = 0; i < snap->nvidia_gpu_count; ++i) {
        if (snap->nvidia_gpus[i].pci_bus_id[0] != '\0' &&
            strcmp(snap->nvidia_gpus[i].pci_bus_id, wanted) == 0) {
            return i;
        }
    }
    return -1;
}

int tm_collect_nvidia_cuda(tm_context_t *ctx, tm_snapshot_t *snap) {
    int count = 0;
    int driver_version = 0;
    int runtime_version = 0;

    (void)ctx;

    if (load_cuda_runtime() != 0) {
        return 0;
    }
    if (p_cudaGetDeviceCount(&count) != CUDA_SUCCESS) {
        return 0;
    }

    if (p_cudaDriverGetVersion) {
        p_cudaDriverGetVersion(&driver_version);
    }
    if (p_cudaRuntimeGetVersion) {
        p_cudaRuntimeGetVersion(&runtime_version);
    }

    for (int ordinal = 0; ordinal < count; ++ordinal) {
        cudaDeviceProp prop;
        int index;

        memset(&prop, 0, sizeof(prop));
        if (p_cudaGetDeviceProperties(&prop, ordinal) != CUDA_SUCCESS) {
            continue;
        }

        index = match_by_pci(snap, prop.pciDomainID, prop.pciBusID, prop.pciDeviceID);
        if (index < 0) {
            continue;
        }

        snap->nvidia_gpus[index].cuda_ordinal = ordinal;
        snap->nvidia_gpus[index].cuda_compute_major = prop.major;
        snap->nvidia_gpus[index].cuda_compute_minor = prop.minor;
        snap->nvidia_gpus[index].cuda_multiprocessors = prop.multiProcessorCount;
        snap->nvidia_gpus[index].cuda_driver_version = driver_version;
        snap->nvidia_gpus[index].cuda_runtime_version = runtime_version;
        snap->nvidia_gpus[index].cuda_total_memory_bytes = prop.totalGlobalMem;
    }

    if (count > 0) {
        snap->capabilities |= TM_CAP_NVIDIA_CUDA;
    }
    return 0;
}
