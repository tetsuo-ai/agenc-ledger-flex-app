# AgenC Ledger Flex App

Private firmware workspace for an AgenC-aware Ledger Flex signing app.

This repository is based on Ledger's upstream
[`app-solana`](https://github.com/LedgerHQ/app-solana) codebase. It keeps the
Solana transaction parsing, signing, derivation, and APDU foundations, then adds
native AgenC instruction parsing so supported AgenC actions can be reviewed on
the Ledger secure screen.

## Status

Current milestone:

- AgenC parser/display scaffold is implemented in `libsol`
- Flex Ragger/Speculos snapshots exist for supported v1 AgenC review flows
- a side-by-side app named `AgenC Solana` has been built and installed on a real
  Ledger Flex
- the official Ledger `Solana` app remains installed separately

Current Flex build hash:

```text
644d51847f8b85fbe6ab1d2002c3752f4ecccffb54e77ab6ee1369bced85aec8
```

This is an engineering fork, not a production Ledger Live release.

## Safety Rule

Do not replace the official installed Ledger `Solana` app.

For hardware testing, this fork is packaged as a separate Ledger app:

```text
AgenC Solana
```

Do not use upstream `make load` for this fork. The upstream default targets app
name `Solana` with `--delete`. Use the guarded scripts under `tools/agenc/`
instead.

## Clear-Signing Scope

The v1 parser focuses on the AgenC marketplace actions that matter for a first
secure-screen workflow:

- create task with review configuration
- attach job spec
- claim task
- submit result
- accept result
- reject result
- cancel task

The device derives display fields from signed Solana transaction bytes:

- program id
- Anchor discriminator
- instruction data
- account indexes and account keys

The device does not trust host-provided display strings for security-critical
review text.

Some fields are intentionally shown as incomplete when they are not present in
the transaction bytes. For example, the current scaffold does not infer
settlement reward or cancellation refund amounts from account state.

## Repository Layout

- `libsol/agenc_instruction.*`
  Native AgenC instruction parser and display model.
- `libsol/agenc_instruction_test.c`
  Direct parser tests and serialized Solana message fixtures.
- `tests/application_client/agenc_cmd_builder.py`
  Python fixture builder for AgenC transactions.
- `tests/python/test_agenc_clear_signing.py`
  Ragger/Speculos coverage for Flex secure-screen flows.
- `tests/python/snapshots/flex/test_agenc_*`
  Golden Flex screenshots for supported AgenC actions.
- `icons/icon_agenc_*` and `glyphs/home_agenc_*`
  Ledger icon and NBGL home glyph assets generated from the AgenC logo.
- `tools/agenc/`
  Safe build, load, APDU, and icon helper scripts for the side-by-side app.
- `doc/agenc-ledger-flex.md`
  Hardware loading notes and real-device result log.

## Build

Build the side-by-side Flex app:

```sh
tools/agenc/build-flex.sh
```

This uses Ledger's dev-tools container and builds with:

```text
APPNAME="AgenC Solana"
```

The output is written to:

- `bin/app.elf`
- `bin/app.hex`
- `bin/app.apdu`
- `bin/app.sha256`

## Test

Run the C parser/message tests:

```sh
docker run --rm -v "$PWD:/app" -w /app \
  ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest \
  make -C libsol clean

docker run --rm -v "$PWD:/app" -w /app \
  ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest \
  make -C libsol
```

Run focused Flex Ragger/Speculos tests:

```sh
docker run --rm -v "$PWD:/app" -w /app \
  ghcr.io/ledgerhq/ledger-app-builder/ledger-app-dev-tools:latest \
  sh -lc 'rm -rf .tmp-ragger && trap "rm -rf .tmp-ragger" EXIT &&
  mkdir -p .tmp-ragger/tmp .tmp-ragger/cache &&
  TMPDIR=/app/.tmp-ragger/tmp python3 -m venv --system-site-packages .tmp-ragger/venv &&
  . .tmp-ragger/venv/bin/activate &&
  TMPDIR=/app/.tmp-ragger/tmp PIP_CACHE_DIR=/app/.tmp-ragger/cache \
    python -m pip install --no-cache-dir base58 ecdsa solders solana "ragger[tests]" &&
  pytest tests/python/test_agenc_clear_signing.py --tb=short -v --device flex'
```

## Hardware Loading

Confirm the Flex is visible:

```sh
tools/agenc/list-flex-apps.sh
```

Generate a side-by-side offline APDU:

```sh
tools/agenc/generate-load-apdu.sh
```

Load the app only after confirming the target is `AgenC Solana`:

```sh
AGENC_CONFIRM_SIDE_BY_SIDE_LOAD=1 tools/agenc/load-flex.sh
```

The load script refuses to run without `AGENC_CONFIRM_SIDE_BY_SIDE_LOAD=1`.
It calculates `dataSize` and `installparamsSize` from `debug/app.map`, matching
Ledger SDK behavior.

Successful hardware load parameters from the current build:

```text
appName=AgenC Solana
dataSize=512
installparamsSize=345
app.sha256=644d51847f8b85fbe6ab1d2002c3752f4ecccffb54e77ab6ee1369bced85aec8
```

Post-load `listApps` confirmed both `Solana` and `AgenC Solana` installed on
the same Ledger Flex.

## Program ID Policy

The current code recognizes the verified AgenC mainnet program id by default:

```text
HJsZ53Zb27b8QMRbQpuDngE44AdwCGxvEZr61Zmxw1xK
```

That id now matches the kit mainnet preset and the generated kit Ledger
fixtures. If the deployed program id changes, regenerate the kit Ledger
fixtures and update this parser constant in the same release.

## Relationship To The Kit

This repository owns firmware parsing, display, and Ledger device tests.

The AgenC Marketplace Agent Kit owns transaction construction, policy checks,
CLI UX, and Ledger transport integration.

The intended boundary is:

- kit builds and policy-checks the Solana transaction
- Ledger app parses the signed bytes and displays trusted review fields
- private keys remain on the Ledger device

## Upstream

Upstream base:

```text
https://github.com/LedgerHQ/app-solana
```

Local remote convention:

- `origin`: Ledger upstream
- `agenc`: private AgenC app repo
