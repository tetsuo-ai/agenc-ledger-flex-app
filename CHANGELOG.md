# Changelog

All notable AgenC-specific changes to this Ledger application are documented
here. The AgenC release version is independent from the upstream Solana Ledger
application version in `Makefile`.

## [Unreleased]

### Compatibility

- This release candidate targets AgenC protocol revision 5. Clients that still
  produce older marketplace account layouts must update before using this
  device build.

### Changed

- Updated AgenC clear-sign decoding for protocol revision 5 account layouts,
  including moderation, hire, completion-bond, agent-stat, and treasury
  accounts.
- Vendored and hardened the Ledger workflow graph with immutable action,
  container, SDK, and auxiliary-application revisions.
- Added a 30-day Dependabot cooldown so newly published dependency versions can
  age before automated update proposals.
- Hash-locked standalone and swap functional-test dependencies for Python 3.12.
- Pinned local Ledger build instructions by image digest and added a
  hash-locked Ledgerblue environment for hardware operations.
- Adopted the `agenc-flex-v<semver>` release convention with explicit
  prerelease handling and fail-closed release metadata validation.
- Made `AgenC Solana` the default application identity so CI and release builds
  cannot silently produce an app named `Solana`.
- Preserved the HEX, Ledger application-hash, and map artifacts required for
  reviewed side-loading alongside each release ELF, with a conventional
  `SHA256SUMS` manifest for downloadable-file verification.

### Fixed

- Updated SPL-token test imports for the current Solana Python client API.
- Corrected workflow permissions, artifact validation, cleanup-path checks,
  and release-tag resolution.

## [0.2.0] - 2026-06-03

### Added

- Clear-signing coverage for the AgenC marketplace lifecycle on Flex, Stax,
  Nano X, and Nano S+.
- Clear signing for SOL and SPL-token task rewards, standalone task creation,
  job-spec commitments, and submitted artifact hashes.
- Mainnet AgenC program recognition and guarded side-by-side installation as
  `AgenC Solana`.

## [0.1.0-hw.1] - 2026-05-24

### Added

- Initial private Ledger Flex hardware prerelease of the AgenC-aware Solana
  application.
- Guarded unsigned engineering-build installation alongside the official
  Solana application.

[Unreleased]: https://github.com/tetsuo-ai/agenc-ledger-flex-app/compare/agenc-flex-v0.2.0...HEAD
[0.2.0]: https://github.com/tetsuo-ai/agenc-ledger-flex-app/compare/agenc-flex-v0.1.0-hw.1...agenc-flex-v0.2.0
[0.1.0-hw.1]: https://github.com/tetsuo-ai/agenc-ledger-flex-app/releases/tag/agenc-flex-v0.1.0-hw.1
