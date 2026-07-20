#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "Usage: $0 [--metadata] <agenc-flex-vSEMVER> [VERSION file] [CHANGELOG file]" >&2
  exit 2
}

output_format=version
if [[ "${1:-}" == "--metadata" ]]; then
  output_format=metadata
  shift
fi

if (( $# < 1 || $# > 3 )); then
  usage
fi

tag=$1
version_file=${2:-VERSION}
changelog_file=${3:-CHANGELOG.md}

numeric_identifier='(0|[1-9][0-9]*)'
non_numeric_identifier='[0-9A-Za-z-]*[A-Za-z-][0-9A-Za-z-]*'
prerelease_identifier="(${numeric_identifier}|${non_numeric_identifier})"
build_identifier='[0-9A-Za-z-]+'
semver_regex="^${numeric_identifier}\\.${numeric_identifier}\\.${numeric_identifier}(-${prerelease_identifier}(\\.${prerelease_identifier})*)?(\\+${build_identifier}(\\.${build_identifier})*)?$"

if [[ "${tag}" != agenc-flex-v* ]]; then
  echo "Error: release tag must start with 'agenc-flex-v'." >&2
  exit 1
fi

version=${tag#agenc-flex-v}
if [[ ! "${version}" =~ ${semver_regex} ]]; then
  echo "Error: release tag must be exactly agenc-flex-v<SemVer>." >&2
  exit 1
fi

if [[ ! -f "${version_file}" ]]; then
  echo "Error: release version file is missing: ${version_file}" >&2
  exit 1
fi

mapfile -t version_lines < "${version_file}"
if (( ${#version_lines[@]} != 1 )) || [[ -z "${version_lines[0]}" ]]; then
  echo "Error: ${version_file} must contain exactly one non-empty line." >&2
  exit 1
fi

declared_version=${version_lines[0]}
if [[ ! "${declared_version}" =~ ${semver_regex} ]]; then
  echo "Error: ${version_file} does not contain a valid SemVer." >&2
  exit 1
fi
if [[ "${declared_version}" != "${version}" ]]; then
  echo "Error: tag version ${version} does not match ${version_file} (${declared_version})." >&2
  exit 1
fi

if [[ ! -f "${changelog_file}" ]]; then
  echo "Error: release changelog is missing: ${changelog_file}" >&2
  exit 1
fi

version_heading_prefix="## [${version}]"
heading_prefix="${version_heading_prefix} - "
heading_count=0
while IFS= read -r line; do
  if [[ "${line}" == "${version_heading_prefix}"* ]]; then
    if [[ "${line}" != "${heading_prefix}"* ]]; then
      echo "Error: ${version} changelog heading must include an ISO date." >&2
      exit 1
    fi
    release_date=${line#"${heading_prefix}"}
    if [[ ! "${release_date}" =~ ^[0-9]{4}-[0-9]{2}-[0-9]{2}$ ]] ||
       ! normalized_date=$(date --date="${release_date}" '+%F' 2>/dev/null) ||
       [[ "${normalized_date}" != "${release_date}" ]]; then
      echo "Error: ${version} changelog heading must use an ISO date." >&2
      exit 1
    fi
    (( heading_count += 1 ))
  fi
done < "${changelog_file}"

if (( heading_count != 1 )); then
  echo "Error: ${changelog_file} must contain exactly one dated '## [${version}]' heading." >&2
  exit 1
fi

if [[ "${output_format}" == "metadata" ]]; then
  version_without_build=${version%%+*}
  prerelease=false
  if [[ "${version_without_build}" == *-* ]]; then
    prerelease=true
  fi
  printf 'version=%s\n' "${version}"
  printf 'prerelease=%s\n' "${prerelease}"
else
  printf '%s\n' "${version}"
fi
