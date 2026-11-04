#!/usr/bin/env bash
# Runs Steinberg's VST3 validator on a .vst3 bundle (standard + extensive
# suites) and keeps the logs under validation-logs/.
#
# Usage: scripts/run_vst3_validator.sh <path/to/Plugin.vst3>
#
# Note: pass the .vst3 bundle directory. pluginval's built-in hook
# (--vst3validator) passes the inner Linux .so instead, which the validator
# rejects ("is not a module directory"), so the validator is run separately.
set -euo pipefail

plugin="${1:?usage: run_vst3_validator.sh <path/to/Plugin.vst3>}"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exe="${repo_root}/tools/vst3validator/validator"
[[ -x "${exe}" ]] || exe="${exe}.exe"
if [[ ! -x "${exe}" ]]; then
    "${repo_root}/scripts/build_vst3_validator.sh"
    exe="${repo_root}/tools/vst3validator/validator"
    [[ -x "${exe}" ]] || exe="${exe}.exe"
fi

logs="${repo_root}/validation-logs"
mkdir -p "${logs}"

overall=0
for mode in standard extensive; do
    log="${logs}/vst3-validator-${mode}.log"
    echo "Running VST3 validator (${mode}) on ${plugin}"
    set +e
    if [[ "${mode}" == "extensive" ]]; then
        "${exe}" -e "${plugin}" > "${log}" 2>&1
    else
        "${exe}" "${plugin}" > "${log}" 2>&1
    fi
    status=$?
    set -e

    grep -E "^Result:|ERROR" "${log}" || true
    echo "VST3 validator (${mode}) exit code: ${status} (log: ${log})"
    if [[ ${status} -ne 0 ]]; then
        overall=${status}
    fi
done

exit "${overall}"
