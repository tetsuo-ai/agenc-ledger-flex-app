#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
app_name="AgenC Solana"
builder_image="ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite@sha256:02dfec4a79dd5ea1783c534f8e5f104a82a7492ba49d6dfe0360db8fc3b908b7"

echo "Building side-by-side Ledger Flex app: $app_name"
docker run --rm --platform linux/amd64 -v "$repo_root:/app" -w /app \
  --user "$(id -u):$(id -g)" \
  "$builder_image" \
  sh -lc 'BOLOS_SDK=$FLEX_SDK make clean &&
  BOLOS_SDK=$FLEX_SDK make APPNAME="\"AgenC Solana\""'

if [[ -f "$repo_root/bin/app.sha256" ]]; then
  echo "app.sha256:"
  cat "$repo_root/bin/app.sha256"
fi
