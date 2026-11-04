#!/usr/bin/env bash
# Validates a built plug-in with pluginval at the given strictness level and
# keeps the full log under validation-logs/.
#
# Usage: scripts/run_pluginval.sh <path/to/Plugin.vst3|.component> [strictness=10]
# On headless Linux, run it under xvfb-run so the editor tests can open a window.
set -euo pipefail

plugin="${1:?usage: run_pluginval.sh <plugin path> [strictness]}"
strictness="${2:-10}"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tools="${repo_root}/tools/pluginval"

case "$(uname -s)" in
    Darwin*) exe="${tools}/pluginval.app/Contents/MacOS/pluginval" ;;
    MINGW*|MSYS*|CYGWIN*) exe="${tools}/pluginval.exe" ;;
    *) exe="${tools}/pluginval" ;;
esac

if [[ ! -x "${exe}" ]]; then
    "${repo_root}/scripts/fetch_pluginval.sh"
fi

logs="${repo_root}/validation-logs"
mkdir -p "${logs}"
name="$(basename "${plugin}")"
log="${logs}/pluginval-${name}-strictness${strictness}.log"

echo "Running pluginval (strictness ${strictness}) on ${plugin}"
set +e
"${exe}" --strictness-level "${strictness}" \
         --validate-in-process \
         --timeout-ms 600000 \
         --output-dir "${logs}" \
         --validate "${plugin}" 2>&1 | tee "${log}"
status=${PIPESTATUS[0]}
set -e

echo "pluginval exit code: ${status} (log: ${log})"
exit "${status}"
