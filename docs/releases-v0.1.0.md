# Release Notes: v0.1.0

`thermal-observatory` `v0.1.0` is the first release that feels usable as a hardware thermal observability framework rather than just a repo scaffold.

## Highlights

- versioned JSON schema with per-metric provenance
- Prometheus export and node_exporter textfile compatibility
- Linux fixture-backed collector testing through `TM_SYSROOT`
- NVIDIA telemetry plus CUDA runtime correlation
- AMD ROCm SMI and `amdgpu` fallback paths
- early IPMI, Redfish, and DCGM integration scaffolds

## Notable Changes

### Output and Schema

- upgraded JSON schema to `0.3.0`
- each metric now carries:
  - `value`
  - `unit`
  - `source`
  - `timestamp_ns`
  - `error`
- kept IDs and names as structural fields and moved measurements into `metrics` objects

### Monitoring Integration

- added Prometheus stdout mode with `--prometheus`
- added Prometheus textfile export with `--prometheus-textfile`
- emitted metrics for CPU, GPU, board, fan, PSU, and generic Linux thermal paths

### Testability

- added `TM_SYSROOT` support for Linux path collectors
- added fixture-backed mocked Linux sensor trees
- added sample schema validation with `tests/check_json_schema.py`

### Platform Coverage

- NVIDIA:
  - NVML telemetry
  - CUDA runtime correlation
  - DCGM scaffold
- AMD:
  - ROCm SMI
  - `amdgpu` `hwmon` fallback
- Platform:
  - IPMI scaffold
  - Redfish scaffold

## Known Gaps

- real Redfish JSON parsing is not implemented yet
- DCGM integration is still discovery/scaffold grade
- ROCm runtime correlation is still missing
- real hardware validation samples are still limited

## Recommended Next Steps

1. validate on at least one real datacenter-class system
2. harden IPMI sensor normalization
3. implement real Redfish ingestion
4. deepen DCGM field support
5. add ROCm runtime correlation
