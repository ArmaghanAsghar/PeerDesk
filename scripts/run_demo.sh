#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${ROOT}/build"
if [[ ! -x "${BUILD}/peerdesk-server" || ! -x "${BUILD}/peerdesk-client" ]]; then
  echo "Build first: cmake -S . -B build && cmake --build build -j"
  exit 1
fi
DATA="${PEERDESK_DATA:-${ROOT}/.peerdesk-demo}"
PORT="${PEERDESK_PORT:-4473}"
MODE="${1:-synthetic}"

if [[ "${MODE}" == "x11" ]]; then
  EXTRA=()
else
  EXTRA=(--synthetic --no-inject)
fi

echo "Starting peerdesk-server on port ${PORT} (${MODE})"
"${BUILD}/peerdesk-server" --port "${PORT}" --data-dir "${DATA}" --user jordan --password peerdesk "${EXTRA[@]}" &
SID=$!
trap 'kill "${SID}" 2>/dev/null || true' EXIT
sleep 0.4
echo "Starting peerdesk-client (jordan / peerdesk)"
exec "${BUILD}/peerdesk-client"
