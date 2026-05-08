import json
import pathlib
import sys


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: check_json_schema.py <json-file>")
        return 2

    path = pathlib.Path(sys.argv[1])
    data = json.loads(path.read_text())

    required = [
        "schema_version",
        "timestamp",
        "arch",
        "capability_mask",
        "capabilities",
        "cpu_packages",
        "arm_clusters",
        "nvidia_gpus",
        "amd_gpus",
        "hwmon_sensors",
        "thermal_zones",
        "board_sensors",
        "fan_sensors",
        "psu_sensors",
        "summary",
    ]

    missing = [key for key in required if key not in data]
    if missing:
        print(f"missing keys: {missing}")
        return 1

    assert isinstance(data["cpu_packages"], list)
    assert isinstance(data["hwmon_sensors"], list)
    assert isinstance(data["thermal_zones"], list)
    assert isinstance(data["board_sensors"], list)
    assert isinstance(data["fan_sensors"], list)
    assert isinstance(data["psu_sensors"], list)
    assert "cpu_package_count" in data["summary"]
    assert "hwmon_sensor_count" in data["summary"]
    assert "thermal_zone_count" in data["summary"]
    assert "board_sensor_count" in data["summary"]
    assert "fan_sensor_count" in data["summary"]
    assert "psu_sensor_count" in data["summary"]

    print("json schema check passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
