# AgenC Ledger Flex app notes

This fork adds AgenC-aware clear-signing scaffolding to Ledger's Solana app
codebase.

## Hardware rule

Do not replace the official installed Ledger `Solana` app for AgenC testing.

Hardware builds are packaged and loaded as a separate app:

```text
AgenC Solana
```

The app still uses Solana derivation constraints and Solana APDU behavior, but
the Ledger OS install name is distinct from `Solana`.

Do not run upstream `make load` for AgenC hardware tests, because the upstream
default targets app name `Solana` with `--delete`.

## Build

From this repository root:

```sh
tools/agenc/build-flex.sh
```

The build uses Ledger's dev-tools container and sets:

```text
APPNAME="AgenC Solana"
```

Current Flex build hash:

```text
644d51847f8b85fbe6ab1d2002c3752f4ecccffb54e77ab6ee1369bced85aec8
```

## Icons

The install icon and NBGL home glyphs are generated from the AgenC logo SVG.

Source:

```text
icons/icon_agenc.svg
```

Regenerate local Ledger icon assets:

```sh
tools/agenc/generate-ledger-icons.sh
```

Generated assets:

- `icons/icon_agenc_*`
- `glyphs/home_agenc_*`

## Loading

First confirm the Flex is visible:

```sh
tools/agenc/list-flex-apps.sh
```

Generate an offline APDU:

```sh
tools/agenc/generate-load-apdu.sh
```

Load the side-by-side app only after confirming that replacing any previous
local `AgenC Solana` test build is acceptable:

```sh
AGENC_CONFIRM_SIDE_BY_SIDE_LOAD=1 tools/agenc/load-flex.sh
```

`tools/agenc/load-flex.sh` refuses to run without
`AGENC_CONFIRM_SIDE_BY_SIDE_LOAD=1`.

The loader extracts `dataSize` and `installparamsSize` from `debug/app.map`.
Do not hardcode those values; icon/install-parameter changes alter the
`installparamsSize`.

## Hardware result

The side-by-side app was loaded successfully on a real Ledger Flex with:

```text
appName=AgenC Solana
dataSize=512
installparamsSize=345
app.sha256=644d51847f8b85fbe6ab1d2002c3752f4ecccffb54e77ab6ee1369bced85aec8
```

Post-load `listApps` confirmed:

- `Solana`
- `Bitcoin`
- `Ethereum`
- `Exchange`
- `Ledger Sync`
- `AgenC Solana`

That confirms the official `Solana` app remained installed and the AgenC build
was added side-by-side.
