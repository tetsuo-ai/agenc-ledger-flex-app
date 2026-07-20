#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
workflow_dir="${repo_root}/.github/workflows"
failed=0

report() {
  printf 'workflow pin check: %s\n' "$*" >&2
  failed=1
}

check_hash_lock() {
  local lock=$1
  local line line_number=0 current_package='' current_has_hash=0 package_count=0
  local -A seen_packages=()

  if [[ ! -f "${lock}" ]]; then
    report "missing Python dependency lock: ${lock#"${repo_root}/"}"
    return
  fi

  while IFS= read -r line || [[ -n "${line}" ]]; do
    (( line_number += 1 ))
    if [[ -z "${line}" || "${line}" == \#* ||
          "${line}" =~ ^--(only-binary|no-binary)=.+$ ]]; then
      continue
    fi

    if grep -Eq '^[a-z0-9][a-z0-9._-]*==[^[:space:]]+[[:space:]]+\\$' <<< "${line}"; then
      if [[ -n "${current_package}" && "${current_has_hash}" -eq 0 ]]; then
        report "${lock#"${repo_root}/"}: package ${current_package} has no artifact hash"
      fi
      current_package=${line%%==*}
      current_has_hash=0
      (( package_count += 1 ))
      if [[ -n "${seen_packages[${current_package}]:-}" ]]; then
        report "${lock#"${repo_root}/"}:${line_number}: duplicate package ${current_package}"
      fi
      seen_packages[${current_package}]=1
      continue
    fi

    if grep -Eq '^[[:space:]]+--hash=sha256:[0-9a-f]{64}([[:space:]]+\\)?$' <<< "${line}"; then
      if [[ -z "${current_package}" ]]; then
        report "${lock#"${repo_root}/"}:${line_number}: orphan artifact hash"
      fi
      current_has_hash=1
      continue
    fi

    report "${lock#"${repo_root}/"}:${line_number}: unrecognized or unpinned requirement"
  done < "${lock}"

  if [[ -n "${current_package}" && "${current_has_hash}" -eq 0 ]]; then
    report "${lock#"${repo_root}/"}: package ${current_package} has no artifact hash"
  fi
  if (( package_count == 0 )); then
    report "${lock#"${repo_root}/"}: lock contains no packages"
  fi
}

expression_prefix="$(printf '\044{{')"
while IFS=: read -r file line_number source_line; do
  spec="${source_line#*uses:}"
  spec="${spec%%#*}"
  spec="${spec//[[:space:]]/}"

  if [[ "${spec}" == ./* ]]; then
    local_path="${repo_root}/${spec#./}"
    [[ -f "${local_path}" ]] || report "${file}:${line_number}: missing local workflow ${spec}"
    continue
  fi

  if [[ ! "${spec}" =~ @[0-9a-f]{40}$ ]]; then
    report "${file}:${line_number}: external action is not pinned to a full commit: ${spec}"
  fi
done < <(grep -R -n -E '^[[:space:]]+(uses:)[[:space:]]+' "${workflow_dir}" --include='*.yml' --include='*.yaml')

while IFS=: read -r file line_number source_line; do
  image="${source_line#*image:}"
  image="${image%%#*}"

  if [[ "${image}" == *":latest"* ]]; then
    report "${file}:${line_number}: mutable container tag: ${image}"
  fi
  if [[ "${image}" == *"inputs.container_image"* ]]; then
    report "${file}:${line_number}: caller-controlled container image: ${image}"
  fi
  if [[ "${image}" != *@sha256:* ]]; then
    report "${file}:${line_number}: container image lacks an immutable digest: ${image}"
  fi
done < <(grep -R -n -E '^[[:space:]]+image:[[:space:]]+' "${workflow_dir}" --include='*.yml' --include='*.yaml')

while IFS=: read -r file line_number source_line; do
  runner="${source_line#*runs-on:}"
  runner="${runner%%#*}"
  if [[ "${runner}" == *"ubuntu-latest"* ]]; then
    report "${file}:${line_number}: mutable hosted-runner alias: ${runner}"
  fi
done < <(grep -R -n -E '^[[:space:]]+runs-on:[[:space:]]+' "${workflow_dir}" --include='*.yml' --include='*.yaml')

while IFS=: read -r file line_number source_line; do
  ref="${source_line#*ref:}"
  ref="${ref%%#*}"
  ref="${ref//[[:space:]]/}"
  if [[ "${ref}" != *"${expression_prefix}"* && ! "${ref}" =~ ^[0-9a-f]{40}$ ]]; then
    report "${file}:${line_number}: literal checkout ref is not a full commit: ${ref}"
  fi
done < <(grep -R -n -E '^[[:space:]]+ref:[[:space:]]+' "${workflow_dir}" --include='*.yml' --include='*.yaml')

operator_paths=(
  "${repo_root}/README.md"
  "${repo_root}/AGENTS.md"
  "${repo_root}/doc"
  "${repo_root}/tools/agenc"
)

while IFS=: read -r file line_number source_line; do
  report "${file#"${repo_root}/"}:${line_number}: mutable latest reference: ${source_line}"
done < <(grep -R -n -F ':latest' "${operator_paths[@]}" 2>/dev/null || true)

while IFS=: read -r file line_number source_line; do
  if [[ ! "${source_line}" =~ @sha256:[0-9a-f]{64} ]]; then
    report "${file#"${repo_root}/"}:${line_number}: Ledger builder image lacks an immutable digest"
  fi
done < <(grep -R -n -E 'ghcr\.io/ledgerhq/ledger-app-builder/' "${operator_paths[@]}" 2>/dev/null || true)

while IFS=: read -r file line_number source_line; do
  if [[ "${source_line}" != *'--require-hashes'* ]]; then
    report "${file#"${repo_root}/"}:${line_number}: Python install does not require artifact hashes"
  fi
done < <(grep -R -n -E '(^|[[:space:]])(pip3?|python[^[:space:]]*[[:space:]]+-m[[:space:]]+pip)[[:space:]]+install' "${operator_paths[@]}" 2>/dev/null || true)

check_hash_lock "${repo_root}/.github/python-build-requirements.txt"
check_hash_lock "${repo_root}/tests/python/requirements-ci.txt"
check_hash_lock "${repo_root}/tests/swap/requirements-ci.txt"
check_hash_lock "${repo_root}/tools/agenc/requirements-ledgerblue.txt"

if (( failed != 0 )); then
  exit 1
fi

printf 'All workflow and operator dependencies are immutable and hash-locked.\n'
