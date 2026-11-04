#!/usr/bin/env bash
# Builds Steinberg's official VST3 validator from a pinned VST3 SDK release
# (MIT licence) into tools/vst3validator. It is an independent, non-JUCE host
# used only for testing; nothing from it is shipped.
#
# The SDK version matches the VST3 SDK bundled with the pinned JUCE (3.8.0).
set -euo pipefail

SDK_TAG="v3.8.0_build_66"
SDK_COMMIT="9fad9770f2ae8542ab1a548a68c1ad1ac690abe0"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
src="${repo_root}/tools/vst3sdk"
out="${repo_root}/tools/vst3validator"

if [[ ! -d "${src}/.git" ]]; then
    git -c advice.detachedHead=false clone --quiet --depth 1 --branch "${SDK_TAG}" \
        https://github.com/steinbergmedia/vst3sdk.git "${src}"
fi

actual_commit="$(git -C "${src}" rev-parse HEAD)"
if [[ "${actual_commit}" != "${SDK_COMMIT}" ]]; then
    echo "VST3 SDK checkout is at ${actual_commit}, expected ${SDK_COMMIT}" >&2
    exit 1
fi

# Only the non-GUI parts of the SDK are needed. Submodule commits are pinned by the SDK tag.
git -C "${src}" -c advice.detachedHead=false submodule update --quiet --init --depth 1 \
    base pluginterfaces public.sdk cmake

# The validator needs no GUI. Skip the GUI host samples, which would pull in
# extra system packages (GTK, X11 dev files) without being used.
for sample in editorhost audiohost; do
    list="${src}/public.sdk/samples/vst-hosting/${sample}/CMakeLists.txt"
    if [[ -f "${list}" ]]; then
        mv "${list}" "${list}.disabled"
    fi
done

cmake -S "${src}" -B "${src}/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DSMTG_ENABLE_VSTGUI_SUPPORT=OFF \
    -DSMTG_ADD_VSTGUI=OFF \
    -DSMTG_ENABLE_VST3_PLUGIN_EXAMPLES=OFF \
    -DSMTG_ENABLE_VST3_HOSTING_EXAMPLES=ON \
    -DSMTG_RUN_VST_VALIDATOR=OFF \
    -DSMTG_CREATE_PLUGIN_LINK=OFF
cmake --build "${src}/build" --config Release --target validator --parallel

mkdir -p "${out}"
exe="$(find "${src}/build" -type f \( -name validator -o -name validator.exe \) | head -n 1)"
if [[ -z "${exe}" ]]; then
    echo "validator executable not found after build" >&2
    exit 1
fi
cp "${exe}" "${out}/"
echo "VST3 validator installed in ${out}"
