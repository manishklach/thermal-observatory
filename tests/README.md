# Test Scaffold

This repo now supports mocked Linux sensor trees through the `TM_SYSROOT` environment variable.

That means collectors which read:

- `/sys/class/thermal`
- `/sys/class/hwmon`
- `/sys/class/powercap`
- `/sys/devices/platform/coretemp.*`
- `/sys/devices/system/cpu/.../cpufreq`
- `/sys/class/drm/.../hwmon`

can be exercised against fixtures without live hardware.

## Fixture Layout

The initial fixture tree is:

```text
tests/fixtures/linux_x86_mock/
```

It contains a synthetic:

- thermal zone
- hwmon CPU sensor
- x86 coretemp sensor
- powercap RAPL package

Because this repository is edited on Windows too, the mocked RAPL fixture uses `intel-rapl_0` instead of `intel-rapl:0`. The collector accepts that alias only as a fixture-friendly fallback; real Linux systems still use the normal `intel-rapl:0` path.

## Manual Use

On Linux:

```bash
export TM_SYSROOT=$PWD/tests/fixtures/linux_x86_mock
./thermal_monitor --json > output.json
python3 tests/check_json_schema.py output.json
```

## Current Test Scope

- fixture path redirection works
- JSON output is full and machine-parseable
- top-level summary counts match the mocked tree

The next step is to wire this into CI on a Linux runner and add separate fixture sets for:

- arm64
- NVIDIA
- AMD
- IPMI
- Redfish
