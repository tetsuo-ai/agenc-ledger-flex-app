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

Do not run `make load` for AgenC hardware tests. It bypasses the explicit
side-by-side confirmation and reviewed Python environment described below.

## Build

From this repository root:

```sh
tools/agenc/build-flex.sh
```

The build uses the reviewed Linux/amd64 Ledger builder image pinned by digest in
`tools/agenc/build-flex.sh`. The Makefile defaults to:

```text
APPNAME="AgenC Solana"
```

Prior hardware-loaded Flex application hash (not the current revision-5
candidate):

```text
f3da723b7b8ad700598e072a7a30cd6526efab78d4b4cb32e4c3d81617d739c1
```

Current clear-signing coverage for the user-facing marketplace flow:

- register agent
- create task with CreatorReview configuration
- attach job spec
- claim task
- submit result
- accept result
- reject result
- cancel task
- expire claim

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
tools/agenc/setup-python-env.sh .venv-ledgerblue \
  tools/agenc/requirements-ledgerblue.txt
PYTHON=.venv-ledgerblue/bin/python tools/agenc/list-flex-apps.sh
```

Generate an offline APDU:

```sh
PYTHON=.venv-ledgerblue/bin/python tools/agenc/generate-load-apdu.sh
```

Load the side-by-side app only after confirming that replacing any previous
local `AgenC Solana` test build is acceptable:

```sh
AGENC_CONFIRM_SIDE_BY_SIDE_LOAD=1 \
  PYTHON=.venv-ledgerblue/bin/python \
  tools/agenc/load-flex.sh
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
app.sha256=f3da723b7b8ad700598e072a7a30cd6526efab78d4b4cb32e4c3d81617d739c1
```

`app.sha256` is Ledger's installable-application hash, not a normal checksum of
the ELF or HEX file.

Post-load `listApps` confirmed:

- `Solana`
- `Bitcoin`
- `Ethereum`
- `Exchange`
- `Ledger Sync`
- `AgenC Solana`

That confirms the official `Solana` app remained installed and the AgenC build
was added side-by-side.
