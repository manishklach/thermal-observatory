# Architecture

## High-level flow

```text
CLI
  -> context init
  -> generic Linux discovery
  -> CPU collectors
  -> GPU collectors
  -> optional experimental kernel collector
  -> text or JSON formatter
```

## Modules

### `include/thermal_monitor.h`

Public API and normalized snapshot structures.

### `src/platform`

Common Linux helpers:

- sysfs readers
- arch detection
- generic `hwmon` and `thermal_zone` discovery

### `src/cpu`

- `cpu_x86.c`: `coretemp`, `powercap`, optional MSR-assisted reads
- `cpu_arm64.c`: `thermal_zone`, `cpufreq`, SCMI-oriented discovery

### `src/gpu`

- `gpu_nvidia.c`: NVML via `dlopen`
- `gpu_amd.c`: ROCm SMI via `dlopen`, `amdgpu` hwmon fallback

### `src/format`

Text and JSON rendering only. No probing logic.

### `kernel`

Experimental privileged path for deeper research work. Isolated on purpose.

## Trust Model

- `hwmon`, `thermal_zone`, `powercap`, NVML, and ROCm SMI are the preferred data sources.
- Raw MSR access is optional and secondary.
- Any future “deep” metric should record provenance so downstream users know whether it came from a vendor API, sysfs, or an experimental path.

## Future Extensions

- Prometheus textfile exporter
- long-running daemon mode
- rate-based energy to power derivation
- BMC/IPMI userspace collector
- DCGM integration for NVIDIA clusters
- perf-event-backed ARM AMU support where available

