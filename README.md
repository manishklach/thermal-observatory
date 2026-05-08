# Thermal Observatory

`thermal-observatory` is a Linux-first thermal telemetry toolkit for servers with:

- CPUs: x86_64 and arm64
- GPUs: NVIDIA and AMD
- Interfaces: `hwmon`, `thermal_zone`, `powercap`/RAPL, NVML, ROCm SMI
- Optional deep path: an experimental kernel module for privileged low-level probing

The goal is not to replace vendor tools. It is to provide a single repository and API that can:

- discover what thermal interfaces exist on a host
- collect detailed thermal and power-adjacent telemetry
- expose one normalized snapshot model
- keep risky or platform-specific paths clearly separated

## Scope

This repo is intentionally split into two layers:

1. Stable userspace collectors for interfaces that are already supported and safe to read on production systems.
2. Experimental kernel work for deeper visibility such as direct MSR-assisted reads or future BMC/IPMI hooks.

Nothing here is structured as an LKML submission. This is a GitHub-oriented research/engineering repo.

## Layout

```text
include/                 Public snapshot model and API
src/                     Userspace collectors and output formatting
src/cpu/                 x86 and arm64 CPU collectors
src/gpu/                 NVIDIA and AMD GPU collectors
src/platform/            Generic Linux sysfs and platform helpers
src/format/              Text and JSON rendering
kernel/                  Experimental kernel module
scripts/                 Zero-build helper scripts
docs/                    Design and architecture docs
```

## Coverage Matrix

| Component | Primary path | Fallback path |
| --- | --- | --- |
| x86 CPU temperature | `coretemp` hwmon, `thermal_zone` | MSR when permitted |
| x86 package energy/power | `powercap` RAPL | raw MSR |
| arm64 CPU temperature | `thermal_zone`, vendor hwmon | SCMI-specific paths |
| arm64 frequency | `cpufreq` | none |
| NVIDIA GPU | NVML | `nvidia-smi` script fallback |
| AMD GPU | ROCm SMI | `amdgpu` hwmon |
| Chassis / board sensors | `hwmon` | IPMI left experimental |

## Build

Userspace:

```bash
make
```

Kernel module:

```bash
make -C kernel
```

## Run

Single snapshot:

```bash
./thermal_monitor
```

JSON:

```bash
./thermal_monitor --json
```

Watch mode:

```bash
./thermal_monitor --watch --interval 2
```

Quick no-build script:

```bash
./scripts/thermal_quick.sh
```

## Notes

- x86 MSR-backed reads may require `modprobe msr` and root.
- NVML requires the NVIDIA driver stack.
- ROCm SMI requires the ROCm stack.
- The kernel module is experimental and should be treated as a research path, not production-hardening.

See [docs/design.md](/C:/Users/ManishKL/Documents/Playground/thermal-observatory/docs/design.md) and [docs/architecture.md](/C:/Users/ManishKL/Documents/Playground/thermal-observatory/docs/architecture.md).

