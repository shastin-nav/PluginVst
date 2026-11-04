#!/usr/bin/env bash
# Downloads the pinned pluginval release into tools/pluginval and verifies its
# SHA-256. pluginval (GPLv3) is a test tool only; it is never shipped.
#
# Usage: scripts/fetch_pluginval.sh [Linux|macOS|Windows]
# (Compatible with the bash 3.2 that ships with macOS.)
set -euo pipefail

PLUGINVAL_VERSION="v1.0.4"

platform="${1:-}"
if [[ -z "${platform}" ]]; then
    case "$(uname -s)" in
        Linux*) platform=Linux ;;
        Darwin*) platform=macOS ;;
        MINGW*|MSYS*|CYGWIN*) platform=Windows ;;
        *) echo "Unsupported platform: $(uname -s)" >&2; exit 1 ;;
    esac
fi

case "${platform}" in
    Linux) expected="c01c49d8063965c4c2dea8324468336768f5c9139e0b1caebde14c2400b55352" ;;
    macOS) expected="3c4c533bda0c5059eea3ddaea752d757ee2025041f0f47e6bcb0e87f6082b29f" ;;
    Windows) expected="c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab" ;;
    *) echo "Unknown platform '${platform}'" >&2; exit 1 ;;
esac

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
dest="${repo_root}/tools/pluginval"
mkdir -p "${dest}"

archive="${dest}/pluginval_${platform}.zip"
url="https://github.com/Tracktion/pluginval/releases/download/${PLUGINVAL_VERSION}/pluginval_${platform}.zip"

echo "Fetching pluginval ${PLUGINVAL_VERSION} (${platform})"
curl -fsSL --retry 4 --retry-delay 2 -o "${archive}" "${url}"

if command -v sha256sum >/dev/null 2>&1; then
    actual="$(sha256sum "${archive}" | awk '{print $1}')"
else
    actual="$(shasum -a 256 "${archive}" | awk '{print $1}')"
fi

if [[ "${actual}" != "${expected}" ]]; then
    echo "SHA-256 mismatch for ${archive}" >&2
    echo "  expected ${expected}" >&2
    echo "  actual   ${actual}" >&2
    exit 1
fi

unzip -o -q "${archive}" -d "${dest}"
rm -f "${archive}"
echo "pluginval installed in ${dest}"
