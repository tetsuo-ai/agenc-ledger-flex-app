#!/usr/bin/env bash
set -euo pipefail

target_id="0x33300004"
python_bin="${PYTHON:-python3}"

"$python_bin" -m ledgerblue.listApps --targetId "$target_id"
