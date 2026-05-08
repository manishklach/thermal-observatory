# Datacenter Telemetry

## Why This Layer Matters

Silicon-level telemetry answers:

- how hot is the GPU?
- is the GPU thermally throttling?
- what are clocks and power doing?

Platform telemetry answers:

- is inlet air already hot?
- are chassis fans saturated?
- is a PSU overheating?
- is the rack environment driving throttling instead of the accelerator itself?

The strategic value of `thermal-observatory` is the correlation between those layers.

## Current Direction

This repo now includes early scaffolding for:

- `IPMI` userspace collection through `ipmitool sdr elist all`
- `Redfish` fixture/sample collection through a simple environment-driven ingest path
- `DCGM` presence discovery through `dcgmi`

These are intentionally early collectors. The immediate purpose is to:

1. extend the snapshot model beyond silicon sensors
2. create a place for board/chassis/fan/PSU telemetry in the schema
3. establish the integration points before deeper vendor-specific refinement

## Collection Layers

### GPU / CPU Silicon

- NVML
- CUDA runtime correlation
- ROCm SMI
- Linux `hwmon`
- Linux `thermal_zone`
- Linux `powercap`

### Chassis / Platform

- IPMI SDR sensors
- Redfish thermal and power resources

### Fleet / NVIDIA Datacenter

- DCGM

## JSON Direction

The snapshot schema now has dedicated sections for:

- `board_sensors`
- `fan_sensors`
- `psu_sensors`

That allows future correlation logic such as:

- GPU hotspot high + inlet normal -> local cooling issue
- GPU hotspot high + inlet high -> room or rack thermal issue
- throttling + PSU exhaust high -> power delivery/cooling interaction

## Next Hardening Steps

1. Replace the Redfish CSV-style sample ingest with real JSON parsing.
2. Normalize IPMI sensor names into stable categories.
3. Add DCGM field ingestion instead of simple discovery.
4. Introduce `correlation_hints` or `diagnostics` into the JSON output.
5. Capture real sample outputs from datacenter-class systems.

