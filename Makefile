CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -std=c11
CPPFLAGS += -Iinclude
LDFLAGS += -ldl

SRC := \
	src/thermal_main.c \
	src/platform/linux_paths.c \
	src/platform/linux_sysfs.c \
	src/platform/ipmi.c \
	src/platform/redfish.c \
	src/cpu/cpu_x86.c \
	src/cpu/cpu_arm64.c \
	src/gpu/gpu_nvidia.c \
	src/gpu/gpu_nvidia_cuda.c \
	src/gpu/nvidia_dcgm.c \
	src/gpu/gpu_amd.c \
	src/format/text.c \
	src/format/json.c

all: thermal_monitor

thermal_monitor: $(SRC) include/thermal_monitor.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

cuda-example:
	nvcc -O2 -o examples/cuda_heatload examples/cuda_heatload.cu

fixture-test:
	bash tests/run_fixture_test.sh

kernel:
	$(MAKE) -C kernel

clean:
	rm -f thermal_monitor
	$(MAKE) -C kernel clean || true

.PHONY: all kernel clean cuda-example fixture-test
