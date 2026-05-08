#!/usr/bin/env bash
set -euo pipefail

echo "thermal-observatory quick snapshot"
echo "host_arch=$(uname -m)"
echo

if compgen -G "/sys/class/thermal/thermal_zone*" > /dev/null; then
  echo "[thermal_zone]"
  for zone in /sys/class/thermal/thermal_zone*; do
    [[ -d "$zone" ]] || continue
    type=$(cat "$zone/type" 2>/dev/null || true)
    temp=$(cat "$zone/temp" 2>/dev/null || true)
    if [[ "${temp:-}" =~ ^-?[0-9]+$ ]]; then
      printf "%s %s %.1fC\n" "$(basename "$zone")" "$type" "$(awk "BEGIN { print $temp / 1000 }")"
    fi
  done
  echo
fi

if compgen -G "/sys/class/hwmon/hwmon*" > /dev/null; then
  echo "[hwmon]"
  for hw in /sys/class/hwmon/hwmon*; do
    [[ -d "$hw" ]] || continue
    name=$(cat "$hw/name" 2>/dev/null || true)
    for input in "$hw"/temp*_input; do
      [[ -r "$input" ]] || continue
      temp=$(cat "$input" 2>/dev/null || true)
      [[ "${temp:-}" =~ ^-?[0-9]+$ ]] || continue
      label_file="${input/_input/_label}"
      label=$(cat "$label_file" 2>/dev/null || basename "$input")
      printf "%s %s %.1fC\n" "$name" "$label" "$(awk "BEGIN { print $temp / 1000 }")"
    done
  done
  echo
fi

if command -v nvidia-smi >/dev/null 2>&1; then
  echo "[nvidia-smi]"
  nvidia-smi --query-gpu=index,name,temperature.gpu,power.draw,power.limit --format=csv,noheader,nounits || true
  echo
fi

if command -v rocm-smi >/dev/null 2>&1; then
  echo "[rocm-smi]"
  rocm-smi --showtemp --showpower || true
  echo
fi

