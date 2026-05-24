#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
app_hex="$repo_root/bin/app.hex"
app_map="$repo_root/debug/app.map"
target_id="0x33300004"
app_name="AgenC Solana"

if [[ "$app_name" == "Solana" ]]; then
  echo "Refusing to target the official Ledger Solana app." >&2
  exit 1
fi

if [[ "${AGENC_CONFIRM_SIDE_BY_SIDE_LOAD:-}" != "1" ]]; then
  cat >&2 <<'EOF'
Refusing to load without explicit confirmation.

This command installs/replaces only the side-by-side app named "AgenC Solana".
It must never target app name "Solana".

After confirming the Ledger Flex is awake, unlocked, and ready for custom app
loading, rerun with:

  AGENC_CONFIRM_SIDE_BY_SIDE_LOAD=1 tools/agenc/load-flex.sh
EOF
  exit 2
fi

if [[ ! -f "$app_hex" ]]; then
  echo "Missing app hex: $app_hex" >&2
  echo "Run tools/agenc/build-flex.sh first." >&2
  exit 1
fi

if [[ ! -f "$app_map" ]]; then
  echo "Missing app map: $app_map" >&2
  echo "Run tools/agenc/build-flex.sh first." >&2
  exit 1
fi

map_symbol() {
  local symbol="$1"
  awk -v symbol="$symbol" '$5 == symbol {print "0x" $1; exit}' "$app_map"
}

hex_to_dec() {
  printf '%d' "$(( $1 ))"
}

nvram_data="$(map_symbol _nvram_data)"
envram_data="$(map_symbol _envram_data)"
install_parameters="$(map_symbol _install_parameters)"
einstall_parameters="$(map_symbol _einstall_parameters)"

if [[ -z "$nvram_data" || -z "$envram_data" || -z "$install_parameters" ||
      -z "$einstall_parameters" ]]; then
  echo "Could not extract load sizes from app map: $app_map" >&2
  exit 1
fi

data_size="$(hex_to_dec "$((envram_data - nvram_data))")"
installparams_size="$(hex_to_dec "$((einstall_parameters - install_parameters))")"

python_bin="${PYTHON:-python3}"

"$python_bin" -m ledgerblue.listApps --targetId "$target_id"

echo "Loading $app_name with dataSize=$data_size installparamsSize=$installparams_size"

"$python_bin" -m ledgerblue.loadApp \
  --targetId "$target_id" \
  --targetVersion="" \
  --apiLevel 26 \
  --fileName "$app_hex" \
  --appName "$app_name" \
  --appFlags 0xa00 \
  --delete \
  --tlv \
  --dataSize "$data_size" \
  --installparamsSize "$installparams_size"
