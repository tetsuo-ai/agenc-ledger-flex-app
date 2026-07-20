#!/usr/bin/env bash
set -euo pipefail

if (( $# != 2 )); then
  echo "Usage: $0 <new-venv-directory> <requirements-lock>" >&2
  exit 2
fi

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
venv_path=$(realpath -m -- "$1")
requirements_lock=$(realpath -m -- "$2")
bootstrap_lock="${repo_root}/.github/python-build-requirements.txt"
python_bin=${PYTHON:-python3.12}

case "${requirements_lock}" in
  "${repo_root}"/*) ;;
  *)
    echo "Requirements lock must be inside the repository." >&2
    exit 1
    ;;
esac
if [[ ! -f "${requirements_lock}" ]]; then
  echo "Requirements lock does not exist: ${requirements_lock}" >&2
  exit 1
fi

case "${venv_path}" in
  "${repo_root}"|"${repo_root}/.git"|"${repo_root}/.git"/*)
    echo "Refusing to create a virtual environment at ${venv_path}." >&2
    exit 1
    ;;
esac
if [[ -e "${venv_path}" ]]; then
  echo "Virtual-environment path already exists: ${venv_path}" >&2
  echo "Choose a new path or remove the old environment manually." >&2
  exit 1
fi

python_version=$("${python_bin}" -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')
if [[ "${python_version}" != "3.12" ]]; then
  echo "The dependency locks require Python 3.12, found ${python_version}." >&2
  exit 1
fi

"${python_bin}" -m venv "${venv_path}"
"${venv_path}/bin/python" -m pip install --disable-pip-version-check --no-deps --require-hashes -r "${bootstrap_lock}"
"${venv_path}/bin/python" -m pip install --disable-pip-version-check --no-build-isolation --require-hashes -r "${requirements_lock}"
"${venv_path}/bin/python" -m pip check

printf 'Created hash-locked Python environment: %s\n' "${venv_path}"
