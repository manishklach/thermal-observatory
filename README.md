# Thermal Observatory

`thermal-observatory` is a hardware-aware thermal observability framework for CPU, GPU, board, and platform telemetry.

Current release: `v0.1.0`

It is meant to become a generic framework, not just a wrapper around one operating-system path or one vendor stack. The current implementation focus is Linux because that is where the lowest-level server telemetry interfaces are easiest to access, but the repository is structured as a framework that can grow into a broader cross-platform and cross-vendor system.

Today the repo covers:

- CPUs: `x86_64`, `arm64`
- GPUs: NVIDIA, AMD
- Host interfaces: `hwmon`, `thermal_zone`, `powercap`/RAPL, vendor sysfs
- Vendor interfaces: NVML, ROCm SMI, CUDA runtime correlation
- Datacenter interfaces: IPMI scaffold, Redfish scaffold, DCGM scaffold
- Experimental privileged path: kernel module scaffold for future deep collectors

The goal is not to replace vendor tools. The goal is to provide one repository and one normalized API that can:

- discover thermal and power-adjacent interfaces on a host
- collect detailed thermal telemetry with provenance
- correlate runtime device identities with vendor telemetry
- expose one snapshot model for higher-level tooling
- keep risky and privileged paths clearly separated from stable userspace collectors

## Why This Exists

Thermal data is fragmented across:

- generic kernel interfaces
- architecture-specific CPU paths
- vendor GPU libraries
- platform firmware and BMC surfaces

In practice that means engineers end up stitching together `nvidia-smi`, `rocm-smi`, `sensors`, ad hoc `sysfs` reads, and platform-specific scripts. This repo aims to become the clean integration layer on top of those sources.

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
src/gpu/                 NVIDIA NVML/CUDA and AMD ROCm collectors
src/platform/            Generic Linux sysfs and platform helpers
src/format/              Text and JSON rendering
kernel/                  Experimental kernel module
scripts/                 Zero-build helper scripts
examples/                Validation and heatload examples
docs/                    Design and architecture docs
```

## Coverage Matrix

| Component | Primary path | Fallback path |
| --- | --- | --- |
| x86 CPU temperature | `coretemp` hwmon, `thermal_zone` | MSR when permitted |
| x86 package energy/power | `powercap` RAPL | raw MSR |
| arm64 CPU temperature | `thermal_zone`, vendor hwmon | SCMI-specific paths |
| arm64 frequency | `cpufreq` | none |
| NVIDIA GPU telemetry | NVML | `nvidia-smi` script fallback |
| NVIDIA runtime correlation | CUDA runtime | PCI/UUID matching via NVML |
| NVIDIA fleet integration | DCGM scaffold | NVML-only mode |
| AMD GPU | ROCm SMI | `amdgpu` hwmon |
| Chassis / board sensors | `hwmon`, IPMI scaffold, Redfish scaffold | none |

## Architecture Principles

- Authoritative source first: use vendor or kernel-supported APIs before raw register scraping.
- Runtime correlation second: CUDA and future ROCm runtime helpers are there to map execution contexts to telemetry, not replace thermal APIs.
- Snapshot-first design: collectors populate one shared model.
- Capability bits matter: the output should say what the framework truly observed.
- Experimental paths stay isolated until they are validated on real hardware.

## Build

Userspace:

```bash
make
```

Kernel module:

```bash
make -C kernel
```

CUDA heatload example:

```bash
make cuda-example
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

CUDA heatload validation:

```bash
./examples/cuda_heatload 16777216 4000
```

Run the heatload in one terminal and the monitor in another to watch temperature, power, clock, and throttle-reason changes as the GPU warms up.

## NVIDIA Path

For NVIDIA, the framework now has two separate roles:

1. `NVML` is the primary telemetry collector.
   It is the authoritative source here for:
   - GPU die temperature
   - memory temperature when exposed
   - power draw and enforced limit
   - clocks
   - utilization
   - throttle reasons
   - PCI bus identity

2. `CUDA runtime` is a correlation layer.
   It is used for:
   - mapping CUDA ordinal to the NVML device
   - reporting compute capability
   - reporting SM count
   - reporting total global memory
   - reporting CUDA driver/runtime versions

That separation is intentional. CUDA is not the thermal API; NVML is.

## AMD Path

For AMD, the framework prefers:

- `ROCm SMI` for richer telemetry
- `amdgpu` `hwmon` for fallback when ROCm user libraries are unavailable

The next comparable addition on the AMD side is a runtime-correlation layer similar to the new CUDA path.

## Datacenter Path

The framework now has the beginning of a datacenter telemetry layer:

- `IPMI` scaffold via `ipmitool sdr elist all`
- `Redfish` scaffold via `TM_REDFISH_SAMPLE`
- `DCGM` scaffold via `dcgmi`

This is the start of the “silicon plus environment” model:

- GPU temperatures and throttle reasons explain what the accelerator is doing
- board, fan, and PSU telemetry explain whether the node or room is contributing
- DCGM is the natural NVIDIA fleet-side integration point

The immediate value is schema and integration-point clarity. The next value is real correlation across those layers.

## Output Model

The public API in [include/thermal_monitor.h](/C:/Users/ManishKL/Documents/Playground/thermal-observatory/include/thermal_monitor.h) is the center of the repo. It currently models:

- CPU packages and cores
- ARM thermal clusters
- NVIDIA GPU telemetry plus CUDA metadata
- AMD GPU telemetry
- generic `hwmon` sensors
- generic thermal zones

The JSON output now emits full structured sections for:

- CPU packages and per-core values
- ARM clusters
- NVIDIA GPUs with CUDA correlation metadata
- AMD GPUs
- `hwmon` sensors
- thermal zones
- board sensors
- fan sensors
- PSU sensors
- capability mask plus capability names
- summary counts

The current schema version is `0.3.0`. Metrics now carry per-metric provenance in the JSON output with:

- `value`
- `unit`
- `source`
- `timestamp_ns`
- `error`

See the synthetic schema example in [samples/synthetic-linux-x86-mock-snapshot.json](/C:/Users/ManishKL/Documents/Playground/thermal-observatory/samples/synthetic-linux-x86-mock-snapshot.json).
See the datacenter direction note in [docs/datacenter-telemetry.md](/C:/Users/ManishKL/Documents/Playground/thermal-observatory/docs/datacenter-telemetry.md).
See the long-form writeup in [docs/blog-why-thermal-observatory.md](/C:/Users/ManishKL/Documents/Playground/thermal-observatory/docs/blog-why-thermal-observatory.md).

## Prometheus

The repo now supports Prometheus-oriented output in two ways:

1. stdout mode:

```bash
./thermal_monitor --prometheus
```

2. textfile collector mode:

```bash
./thermal_monitor --prometheus-textfile /var/lib/node_exporter/thermal.prom
```

Metric families include:

- `thermal_gpu_temperature_celsius`
- `thermal_gpu_power_watts`
- `thermal_gpu_throttle_reason`
- `thermal_cpu_package_temperature_celsius`
- `thermal_board_sensor_value`
- `thermal_fan_rpm`
- `thermal_psu_power_watts`

## Testability

Linux sysfs-based collectors now support `TM_SYSROOT`, which allows the repo to run against mocked fixture trees instead of live `/sys` paths.

That currently covers:

- generic thermal zones
- generic `hwmon`
- x86 `coretemp`
- x86 `powercap` RAPL
- arm64 thermal zones and `cpufreq`
- AMD `amdgpu` hwmon fallback

Fixture scaffolding lives under [tests/](/C:/Users/ManishKL/Documents/Playground/thermal-observatory/tests), with the initial mocked tree at [tests/fixtures/linux_x86_mock](/C:/Users/ManishKL/Documents/Playground/thermal-observatory/tests/fixtures/linux_x86_mock).

Example fixture run on Linux:

```bash
export TM_SYSROOT=$PWD/tests/fixtures/linux_x86_mock
./thermal_monitor --json > output.json
python3 tests/check_json_schema.py output.json
```

## Validation Strategy

Recommended validation matrix:

- `x86_64 + NVIDIA`
- `x86_64 + AMD`
- `arm64 + NVIDIA`
- `arm64 + AMD`

For each system:

1. Compare framework output to vendor tools.
2. Run a controlled heatload.
3. Observe thermal ramps, power changes, clocks, and throttle transitions.
4. Record gaps in metric availability rather than masking them.

## Roadmap

Near-term:

- make the userspace collectors compile and run cleanly on real Linux hosts
- add ROCm runtime correlation similar to CUDA
- add Prometheus textfile export
- add stronger test fixtures and sample captures
- add CI around fixture-backed Linux collector tests
- harden IPMI, Redfish, and DCGM collectors with real platform validation

Later:

- DCGM integration
- BMC/IPMI userspace collector
- validated MSR-assisted x86 collector path
- safer kernel deep-collector design
- possible non-Linux backends if a clean abstraction emerges

## Current Status

This repo is now best described as a usable `v0.1.0` alpha. The architecture is stable enough for experimentation and integration work:

- structured JSON with per-metric provenance
- Prometheus textfile export
- fixture-backed Linux collector tests
- NVIDIA telemetry plus CUDA correlation
- early datacenter platform telemetry scaffolding

What it still needs most is real Linux hardware validation and hardening of the datacenter collectors.

## Notes

- x86 MSR-backed reads may require `modprobe msr` and root.
- NVML requires the NVIDIA driver stack.
- CUDA correlation requires the CUDA runtime to be installed and discoverable.
- ROCm SMI requires the ROCm stack.
- The kernel module is experimental and should be treated as a research path, not production-hardening.

See [docs/design.md](/C:/Users/ManishKL/Documents/Playground/thermal-observatory/docs/design.md), [docs/architecture.md](/C:/Users/ManishKL/Documents/Playground/thermal-observatory/docs/architecture.md), and [docs/review-notes.md](/C:/Users/ManishKL/Documents/Playground/thermal-observatory/docs/review-notes.md).
