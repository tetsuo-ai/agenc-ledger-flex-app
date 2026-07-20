#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd "${script_dir}/../.." && pwd)
validator="${script_dir}/validate-release-version.sh"
fixtures=$(mktemp -d)
trap 'rm -rf -- "${fixtures}"' EXIT

tests_run=0

write_fixture() {
  local version=$1
  local heading=$2
  printf '%s\n' "${version}" > "${fixtures}/VERSION"
  printf '# Changelog\n\n%s\n\n- Test release.\n' "${heading}" > "${fixtures}/CHANGELOG.md"
}

expect_ok() {
  local tag=$1
  local version=$2
  local prerelease=$3
  write_fixture "${version}" "## [${version}] - 2026-07-20"
  actual=$(bash "${validator}" "${tag}" "${fixtures}/VERSION" "${fixtures}/CHANGELOG.md")
  if [[ "${actual}" != "${version}" ]]; then
    echo "Expected ${version}, got ${actual}." >&2
    exit 1
  fi
  metadata=$(bash "${validator}" --metadata \
    "${tag}" "${fixtures}/VERSION" "${fixtures}/CHANGELOG.md")
  expected_metadata=$(printf 'version=%s\nprerelease=%s' "${version}" "${prerelease}")
  if [[ "${metadata}" != "${expected_metadata}" ]]; then
    echo "Unexpected metadata for ${tag}: ${metadata}." >&2
    exit 1
  fi
  (( tests_run += 1 ))
}

expect_fail() {
  local tag=$1
  if bash "${validator}" "${tag}" "${fixtures}/VERSION" "${fixtures}/CHANGELOG.md" >/dev/null 2>&1; then
    echo "Expected validation to reject ${tag}." >&2
    exit 1
  fi
  (( tests_run += 1 ))
}

expect_recorded_release() {
  local version=$1
  printf '%s\n' "${version}" > "${fixtures}/VERSION"
  actual=$(bash "${validator}" "agenc-flex-v${version}" \
    "${fixtures}/VERSION" "${repo_root}/CHANGELOG.md")
  if [[ "${actual}" != "${version}" ]]; then
    echo "Recorded release ${version} did not validate." >&2
    exit 1
  fi
  (( tests_run += 1 ))
}

expect_ok 'agenc-flex-v0.3.0' '0.3.0' false
expect_ok 'agenc-flex-v0.3.0-rc.1' '0.3.0-rc.1' true
expect_ok 'agenc-flex-v0.1.0-hw.1' '0.1.0-hw.1' true
expect_ok 'agenc-flex-v1.2.3-rc.1+build.7' '1.2.3-rc.1+build.7' true

write_fixture '0.3.0' '## [0.3.0] - 2026-07-20'
expect_fail '0.3.1'
expect_fail 'v0.3.0'
expect_fail 'agenc-flex-v01.3.0'
expect_fail 'agenc-flex-v0.3.0-rc.01'
expect_fail 'agenc-flex-v0.3.0-'
expect_fail 'agenc-flex-v0.3.0/evil'

write_fixture '0.3.0' '## [0.2.0] - 2026-07-20'
expect_fail 'agenc-flex-v0.3.0'

write_fixture '0.3.0' '## [0.3.0] - Unreleased'
expect_fail 'agenc-flex-v0.3.0'

write_fixture '0.3.0' '## [0.3.0] - 2026-02-30'
expect_fail 'agenc-flex-v0.3.0'

write_fixture '0.3.0' '## [0.3.0] - 2026-07-20'
printf '\n## [0.3.0] - 2026-07-20\n' >> "${fixtures}/CHANGELOG.md"
expect_fail 'agenc-flex-v0.3.0'

write_fixture '0.3.0' '## [0.3.0] - 2026-07-20'
printf '\n## [0.3.0] - Unreleased\n' >> "${fixtures}/CHANGELOG.md"
expect_fail 'agenc-flex-v0.3.0'

printf '0.3.0\nextra\n' > "${fixtures}/VERSION"
expect_fail 'agenc-flex-v0.3.0'

mapfile -t current_version_lines < "${repo_root}/VERSION"
if (( ${#current_version_lines[@]} != 1 )); then
  echo 'Repository VERSION must contain exactly one line.' >&2
  exit 1
fi
current_version=${current_version_lines[0]}
current_prerelease=false
if [[ "${current_version%%+*}" == *-* ]]; then
  current_prerelease=true
fi
expect_ok "agenc-flex-v${current_version}" "${current_version}" "${current_prerelease}"

unreleased_count=$(grep -cFx '## [Unreleased]' "${repo_root}/CHANGELOG.md" || true)
if (( unreleased_count != 1 )); then
  echo 'CHANGELOG.md must contain exactly one Unreleased section.' >&2
  exit 1
fi
(( tests_run += 1 ))

expect_recorded_release '0.2.0'
expect_recorded_release '0.1.0-hw.1'

app_name_count=$(grep -Ec \
  '^APPNAME[[:space:]]*=[[:space:]]*"AgenC Solana"[[:space:]]*$' \
  "${repo_root}/Makefile" || true)
if (( app_name_count != 1 )); then
  echo 'Makefile must default to the side-by-side app name "AgenC Solana".' >&2
  exit 1
fi
(( tests_run += 1 ))

printf 'Release-version validation: %d tests passed.\n' "${tests_run}"
