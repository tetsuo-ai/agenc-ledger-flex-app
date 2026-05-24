#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
app_name="AgenC Solana"
builder_image="ghcr.io/ledgerhq/ledger-app-builder/ledger-app-dev-tools:latest"

echo "Building side-by-side Ledger Flex app: $app_name"
docker run --rm -v "$repo_root:/app" -w /app \
  --user "$(id -u):$(id -g)" \
  "$builder_image" \
  sh -lc 'BOLOS_SDK=$FLEX_SDK make clean &&
  BOLOS_SDK=$FLEX_SDK make APPNAME="\"AgenC Solana\""'

if [[ -f "$repo_root/bin/app.sha256" ]]; then
  echo "app.sha256:"
  cat "$repo_root/bin/app.sha256"
fi
