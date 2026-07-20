# AGENTS.md — AgenC Solana (clear-signing) Ledger app

Operational guide for AI agents and engineers working with the **AgenC Solana**
Ledger device app: how to build it, install it on a Ledger Flex, and drive it from
the AgenC marketplace CLI over BLE + the Ledger Device Management Kit (DMK) so AgenC
mainnet transactions are **clear-signed** (the decoded action is shown on the device
secure screen) instead of **blind-signed**.

This repository is a fork of [`LedgerHQ/app-solana`](https://github.com/LedgerHQ/app-solana).
The AgenC additions are isolated and additive — see [Repository layout](#repository-layout).

> **Hard safety rules (read first)**
> - This app is named **"AgenC Solana"** and installs **side-by-side** with the
>   official Ledger "Solana" app. **Never build or load it under the name "Solana"**;
>   it must never replace the official app. `tools/agenc/load-flex.sh` refuses to
>   target the name "Solana".
> - Loading a custom (unsigned) app requires the device to be **unlocked** and the
>   user to **approve on-device** ("non-genuine app" prompt). An agent cannot bypass
>   that — surface it to the human.
> - Do not install a custom CA on a device that must also run official apps (it
>   breaks the genuine check). See [Notes](#notes).

---

## Repository layout (AgenC additions)

| Path | What it is |
|------|------------|
| `libsol/agenc_instruction.h` | AgenC instruction model + the two recognised program ids (mainnet preset + artifact/devnet). |
| `libsol/agenc_instruction.c` | On-device decoder + NBGL display for AgenC instructions. Classifies by on-chain program id, parses the 8-byte Anchor discriminator + Borsh data, renders the human-readable action. **Zero host trust** — no host-provided strings. |
| `libsol/agenc_instruction_test.c` | Host unit tests for the decoder + display. |
| `tools/agenc/build-flex.sh` | Build the app via the Ledger Docker builder. |
| `tools/agenc/load-flex.sh` | Side-load the built app to a Ledger Flex (guarded). |
| `tools/agenc/list-flex-apps.sh` | List apps installed on the connected Flex. |
| `doc/agenc-ledger-flex.md` | Design + clear-signing notes. |

---

## Prerequisites

- **Docker** (for the Ledger app builder image — no local ARM toolchain needed).
- **Python 3.12** with the hash-locked `ledgerblue` environment used for
  side-loading and listing apps:
  `tools/agenc/setup-python-env.sh .venv-ledgerblue tools/agenc/requirements-ledgerblue.txt`.
  Pass `PYTHON=.venv-ledgerblue/bin/python` to the load/list helper scripts.
- A **Ledger Flex**, firmware up to date, **unlocked**, connected over **USB** for
  loading (side-loading is USB-only).

---

## 1. Build

```bash
# from the repo root (the app-solana fork)
tools/agenc/build-flex.sh
```

This runs the reviewed Linux/amd64 Ledger builder image pinned by digest in the
script and builds with `APPNAME="AgenC Solana"`. Outputs:
- `bin/app.hex` — the loadable app
- `debug/app.map` — symbol map (needed by the loader)
- `bin/app.sha256` — Ledger installable-application hash (not the ELF/HEX file
  checksum)

For other devices, build inside the same image with the matching SDK env var
(`$FLEX_SDK`, `$STAX_SDK`, `$NANOX_SDK`, `$NANOSP_SDK`, `$APEX_P_SDK`):

```bash
LEDGER_BUILDER_LITE='ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite@sha256:02dfec4a79dd5ea1783c534f8e5f104a82a7492ba49d6dfe0360db8fc3b908b7'
docker run --rm -v "$PWD:/app" -w /app --user "$(id -u):$(id -g)" \
  --platform linux/amd64 "$LEDGER_BUILDER_LITE" \
  sh -lc 'BOLOS_SDK=$STAX_SDK make clean && BOLOS_SDK=$STAX_SDK make APPNAME="\"AgenC Solana\""'
```

## 2. Test

Host unit tests for the decoder (inside the builder image so the SDK headers resolve):

```bash
LEDGER_BUILDER_LITE='ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite@sha256:02dfec4a79dd5ea1783c534f8e5f104a82a7492ba49d6dfe0360db8fc3b908b7'
docker run --rm -v "$PWD:/app" -w /app --user "$(id -u):$(id -g)" \
  --platform linux/amd64 "$LEDGER_BUILDER_LITE" \
  sh -lc 'BOLOS_SDK=$FLEX_SDK make -C libsol clean && BOLOS_SDK=$FLEX_SDK make -C libsol'
```

Ragger golden-image (snapshot) tests for the on-device screens live under
`tests/python/` with snapshots in `tests/python/snapshots/<device>/`.

## 3. Install on a Ledger Flex

```bash
# Build first (step 1), then, with the Flex unlocked and ready:
AGENC_CONFIRM_SIDE_BY_SIDE_LOAD=1 \
  PYTHON=.venv-ledgerblue/bin/python \
  tools/agenc/load-flex.sh
```

The loader installs/updates **only** the side-by-side app named "AgenC Solana"
(target id `0x33300004` = Flex). You will approve the install on the device.

## 4. Verify

```bash
PYTHON=.venv-ledgerblue/bin/python tools/agenc/list-flex-apps.sh
# Expect "AgenC Solana" alongside "Solana".
```

---

## 5. Drive it from the AgenC marketplace CLI over BLE + DMK

Once "AgenC Solana" is installed, the AgenC marketplace kit can open and sign with
it over Bluetooth through the Device Management Kit. Two environment variables turn
this on (both opt-in; defaults are unchanged):

| Env var | Value | Effect |
|---------|-------|--------|
| `AGENC_LEDGER_DMK_BLE` | `1` | Enable the experimental DMK Node-BLE transport (DMK over Bluetooth). |
| `AGENC_LEDGER_APP_NAME` | `AgenC Solana` | Make the DMK signer **open this app** (instead of the stock "Solana") so AgenC instructions are clear-signed. |

```bash
# Example: clear-sign an AgenC action over BLE+DMK with this app
AGENC_LEDGER_DMK_BLE=1 AGENC_LEDGER_APP_NAME="AgenC Solana" \
  agenc-marketplace --network mainnet \
  --signer ledger --ledger-backend dmk --ledger-transport ble --ledger-key 0/0 \
  <command>
```

How the kit wires this (in `packages/cli/src/ledgerSigner.ts`):
- `AGENC_LEDGER_APP_NAME` (or `--ledger appName` option) → `ledgerAppNameOverride()`.
  When it is a non-"Solana" name, the DMK session runs `OpenAppDeviceAction({ appName })`
  itself to open "AgenC Solana", and every signer call passes `skipOpenApp: true` so the
  Solana signer does not switch back to the stock app.
- Without the override the kit opens the stock "Solana" app (blind-signs AgenC actions),
  so clear-signing is strictly opt-in and depends on this app being installed.

When the device shows a generic "Blind signing ahead / Unrecognized format" screen for
an AgenC mainnet instruction, the wrong app is open (stock "Solana") or this app is not
installed — **reject on the device and fix the setup**, do not approve blind.

---

## AgenC instructions decoded by this app

`register_agent`, `create_task` (standalone and as the `create_task` +
`configure_task_validation` review pair), `configure_task_validation`,
`set_task_job_spec`, `claim_task_with_job_spec`, `submit_task_result`,
`accept_task_result`, `reject_task_result`, `cancel_task`, `expire_claim`.

`create_task` renders reward (SOL **or** SPL token with the mint), task & creator
accounts, the 32-byte content-commitment hash, deadline, max workers, task type
(when non-default), min reputation, and the program id.

Recognised program ids (`libsol/agenc_instruction.h`):
- mainnet preset: `HJsZ53Zb27b8QMRbQpuDngE44AdwCGxvEZr61Zmxw1xK`
- artifact / devnet: `2jdBSJ8U5ixfwgs1bRLPtRRnpZAPm8Xv1tEdu8yjHJC7`

---

## Notes

- **Side-loaded apps are wiped on firmware updates** and show a "non-genuine"
  prompt on each open — this is expected for an unsigned custom app. The path to
  removing those caveats for end users is Ledger signing/listing the app or
  upstreaming the AgenC parsing into the official Solana app.
- **Never** install a custom certificate authority (CA) on a device that must run
  official apps — it breaks the device-wide genuine check until reset.
