#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FIXTURE_ROOT="$ROOT_DIR/tests/fixtures/linux_x86_mock"
OUT_JSON="${TMPDIR:-/tmp}/thermal-observatory-fixture.json"

export TM_SYSROOT="$FIXTURE_ROOT"
"$ROOT_DIR/thermal_monitor" --json > "$OUT_JSON"
python3 "$ROOT_DIR/tests/check_json_schema.py" "$OUT_JSON"
echo "fixture test passed: $OUT_JSON"
