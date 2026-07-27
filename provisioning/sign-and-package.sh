#!/usr/bin/env bash
#
# sign-and-package.sh
#
# Reference flow for turning the freshly built native plugin into a signed,
# downloadable package and staging the web widget bundle for the app catalog.
# Adapt paths, signing key and upload endpoint to your MSO tooling.
#
# Usage: ./sign-and-package.sh <plugin.so> <widget-dist-dir>
set -euo pipefail

PLUGIN_SO="${1:?path to libWPEFrameworkMulticastPlayer.so required}"
WIDGET_DIST="${2:?path to built widget dist/ dir required}"
OUT_DIR="${OUT_DIR:-./out}"
SIGN_KEY="${SIGN_KEY:-./keys/plugin-signing.pem}"

mkdir -p "${OUT_DIR}"

echo "==> Packaging native plugin"
tar -czf "${OUT_DIR}/MulticastPlayer-1.0.0.tgz" \
    -C "$(dirname "${PLUGIN_SO}")" "$(basename "${PLUGIN_SO}")" \
    MulticastPlayer.json 2>/dev/null || \
    tar -czf "${OUT_DIR}/MulticastPlayer-1.0.0.tgz" -C "$(dirname "${PLUGIN_SO}")" "$(basename "${PLUGIN_SO}")"

echo "==> Signing native plugin package"
if [[ -f "${SIGN_KEY}" ]]; then
  openssl dgst -sha256 -sign "${SIGN_KEY}" \
    -out "${OUT_DIR}/MulticastPlayer-1.0.0.tgz.sig" \
    "${OUT_DIR}/MulticastPlayer-1.0.0.tgz"
  echo "    signature: ${OUT_DIR}/MulticastPlayer-1.0.0.tgz.sig"
else
  echo "    WARNING: signing key not found at ${SIGN_KEY}; skipping signature."
fi

echo "==> Packaging web widget bundle"
tar -czf "${OUT_DIR}/multicast-widget-1.0.0.tgz" -C "${WIDGET_DIST}" .

echo "==> Done. Artifacts in ${OUT_DIR}:"
ls -la "${OUT_DIR}"

cat <<'EOF'

Next manual steps (MSO-specific):
  1. Upload MulticastPlayer-1.0.0.tgz(+ .sig) to the Download Manager / firmware
     bundle and add org.rdk.MulticastPlayer to the platform plugin allowlist.
  2. Upload the widget bundle to the app CDN and register
     provisioning/app-catalog-entry.json in the app catalog so it appears on
     the resident-app carousel as a downloadable widget.
EOF
