#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
workflow_dir="${repo_root}/.github/workflows"
failed=0

report() {
  printf 'workflow pin check: %s\n' "$*" >&2
  failed=1
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

if (( failed != 0 )); then
  exit 1
fi

printf 'All workflow actions, reusable workflows, containers, runners, and literal checkout refs are pinned.\n'
