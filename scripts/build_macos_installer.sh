#!/usr/bin/env bash
set -euo pipefail
export COPYFILE_DISABLE=1

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
DIST_DIR="${ROOT_DIR}/dist"
PKG_ROOT="${BUILD_DIR}/pkgroot"
SCRIPTS_ROOT="${BUILD_DIR}/installerscripts"
PRODUCT_NAME="MicEmulation"
PAYLOAD_ARCHIVE="${BUILD_DIR}/${PRODUCT_NAME}Payload.tar.gz"
VERSION="0.1.9"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" --config Release -- -j"$(sysctl -n hw.ncpu)"

AU_SOURCE="${BUILD_DIR}/MicEmulation_artefacts/Release/AU/${PRODUCT_NAME}.component"
VST3_SOURCE="${BUILD_DIR}/MicEmulation_artefacts/Release/VST3/${PRODUCT_NAME}.vst3"

if [[ ! -d "${AU_SOURCE}" ]]; then
    echo "Missing AU build output: ${AU_SOURCE}" >&2
    exit 1
fi

if [[ ! -d "${VST3_SOURCE}" ]]; then
    echo "Missing VST3 build output: ${VST3_SOURCE}" >&2
    exit 1
fi

rm -rf "${PKG_ROOT}" "${SCRIPTS_ROOT}" "${DIST_DIR}" "${PAYLOAD_ARCHIVE}"
mkdir -p \
    "${PKG_ROOT}/Library/Audio/Plug-Ins/Components" \
    "${PKG_ROOT}/Library/Audio/Plug-Ins/VST3" \
    "${SCRIPTS_ROOT}" \
    "${DIST_DIR}"

/usr/bin/ditto --norsrc --noextattr --noqtn "${AU_SOURCE}" "${PKG_ROOT}/Library/Audio/Plug-Ins/Components/${PRODUCT_NAME}.component"
/usr/bin/ditto --norsrc --noextattr --noqtn "${VST3_SOURCE}" "${PKG_ROOT}/Library/Audio/Plug-Ins/VST3/${PRODUCT_NAME}.vst3"

find "${PKG_ROOT}" \( -name '._*' -o -name '.DS_Store' \) -delete
xattr -cr "${PKG_ROOT}" 2>/dev/null || true

tar -C "${PKG_ROOT}" -czf "${PAYLOAD_ARCHIVE}" Library

cat > "${SCRIPTS_ROOT}/postinstall" <<'POSTINSTALL'
#!/bin/sh
set -e

/usr/bin/base64 -D <<'MICEMULATION_PAYLOAD' | /usr/bin/tar -xzf - -C /
POSTINSTALL

/usr/bin/base64 -i "${PAYLOAD_ARCHIVE}" >> "${SCRIPTS_ROOT}/postinstall"

cat >> "${SCRIPTS_ROOT}/postinstall" <<'POSTINSTALL'
MICEMULATION_PAYLOAD

/usr/sbin/chown -R root:wheel \
    "/Library/Audio/Plug-Ins/Components/MicEmulation.component" \
    "/Library/Audio/Plug-Ins/VST3/MicEmulation.vst3"
/bin/chmod -R a+rX,go-w \
    "/Library/Audio/Plug-Ins/Components/MicEmulation.component" \
    "/Library/Audio/Plug-Ins/VST3/MicEmulation.vst3"
/usr/bin/killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true

exit 0
POSTINSTALL

chmod 755 "${SCRIPTS_ROOT}/postinstall"

pkgbuild \
    --nopayload \
    --scripts "${SCRIPTS_ROOT}" \
    --identifier "com.raulgomez.micemulation.pkg" \
    --version "${VERSION}" \
    --install-location "/" \
    "${DIST_DIR}/${PRODUCT_NAME}-macOS.pkg"

echo "Built ${DIST_DIR}/${PRODUCT_NAME}-macOS.pkg"
