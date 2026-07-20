# Vendored Ledger workflow provenance

The reusable workflow graph in this directory is vendored from
`LedgerHQ/ledger-app-workflows` release `v1.106.0`, commit
`adb54f0c6a2fa93a7f14764f92ad4d09cc4f192a`.

Vendoring is deliberate. A full commit on the outer reusable-workflow call does
not freeze actions or container tags referenced inside that workflow. Keeping
the graph in this repository makes all nested calls part of the reviewed app
commit.

## Immutable dependency inventory

- `actions/checkout` v6: `df4cb1c069e1874edd31b4311f1884172cec0e10`
- `actions/upload-artifact` v6: `b7c566a772e6b6bfb58ed0dc250532a479d7789f`
- `actions/download-artifact` v7: `37930b1c2abaa49bbe596cd826c3c89aef350131`
- `actions/download-artifact` v8: `3e5f45b2cfb9172054b4087a40e8e0b5a5461e7c`
- clang-format 12.0.1 image (Linux/amd64):
  `silkeh/clang@sha256:7e05c8da417eba05234d9a5e9838f6dd7d170910082ba4e43eb915ea182e69f9`
- codespell 2.4.3 wheel:
  `sha256:af2505b335e8573dbd2d384d1c4ef498f4006f4ba2d6fceca01e55b91f52628a`
- Ledger builder lite (Linux/amd64):
  `sha256:02dfec4a79dd5ea1783c534f8e5f104a82a7492ba49d6dfe0360db8fc3b908b7`
- Ledger builder (Linux/amd64):
  `sha256:a823f13dba372213b508870874a2d16dcab898972e212ceb18bab0c44c0c7856`
- Ledger app database: `c25d6a84294cb8bcdc9aaa617935f80109c35f56`
- Ledger secure SDK: `e604abab0187c7bb247e26251b275c5592a4e229`
- Exchange test app: `47ee76b1bc5e89286dd2583fefb0dd08e6d742af`
- Ethereum test app: `07e8f9e5e1ff69046557ff586af33891120eb66c`

The original upstream helper scripts are checked out only at the vendored
workflow commit above. Application release builds use the triggering event's
immutable `github.sha`; the tag name is retained only for version validation
and artifact naming.

All hosted jobs target an explicit supported Ubuntu runner series instead of
the moving `ubuntu-latest` alias. Pull-request jobs run with read-only repository
permissions. Write permissions are isolated to tag-driven release creation and
explicit, manually dispatched snapshot-update jobs; snapshot tests themselves
still run read-only. Checkout credentials are disabled throughout; the dedicated
snapshot write job configures GitHub CLI credentials only when it must push its
review branch.

Release validation fails closed: it requires an exact `x.y.z` tag, a
40-character source commit, and the applicable changelog or Cargo version file.
The release job does not install network packages while holding its write token.
Metadata extraction uses `ledgered` 0.15.0 and its dependencies already embedded
in the digest-pinned Ledger builder image. The optional local coverage artifact
remains available, but the unused Codecov uploader was removed because its
default execution downloaded a mutable `latest` CLI.
Rust builds require the committed lockfile, and optional C SDK overrides accept
only immutable 40-character commit revisions.

## Updating

1. Review the complete upstream diff from the source commit to the proposed
   commit, including every nested workflow and helper script.
2. Re-vendor the full local call graph.
3. Resolve each external action to a full commit and each container to a
   platform-specific digest.
4. Resolve auxiliary repositories used by tests or checks to full commits.
5. Run `.github/scripts/check-workflow-pins.sh`, `actionlint`,
   `shellcheck`, and `zizmor` before committing.
