#!/usr/bin/env bash
#
# jsonrpc-test.sh
#
# Exercises the org.rdk.MulticastPlayer JSON-RPC API end to end against a running
# Thunder instance (on a device or locally). Use this to validate control flow
# without the widget UI.
#
# Usage:
#   HOST=<pi-ip> ./jsonrpc-test.sh
#   HOST=192.168.1.50 URI=udp://239.1.1.1:5000 ./jsonrpc-test.sh
#
set -euo pipefail

HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-9998}"
URI="${URI:-udp://239.1.1.1:5000}"
TRANSPORT="${TRANSPORT:-udp}"
RPC="http://${HOST}:${PORT}/jsonrpc"
API="org.rdk.MulticastPlayer.1"

have_jq() { command -v jq >/dev/null 2>&1; }
pretty() { if have_jq; then jq .; else cat; fi; }

call() {
    local id="$1" method="$2" params="${3:-}"
    local body
    if [[ -n "${params}" ]]; then
        body="{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"${API}.${method}\",\"params\":${params}}"
    else
        body="{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"${API}.${method}\"}"
    fi
    echo "--> ${method} ${params}"
    curl -s -H 'Content-Type: application/json' -d "${body}" "${RPC}" | pretty
    echo
}

echo "=== MulticastPlayer JSON-RPC test against ${RPC} ==="
echo

echo "[1] Controller status (is the plugin activated?)"
curl -s -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":0,"method":"Controller.1.status@org.rdk.MulticastPlayer"}' \
  "${RPC}" | pretty
echo

call 1 open   "{\"uri\":\"${URI}\",\"transport\":\"${TRANSPORT}\"}"
call 2 setVideoRectangle '{"x":0,"y":0,"w":1920,"h":1080}'
call 3 play
call 4 status

echo "=== Streaming for 15s (watch the screen / check IGMP join) ==="
if command -v ip >/dev/null 2>&1; then
    ip maddr show 2>/dev/null | grep -E "$(echo "${URI}" | sed -E 's#.*://([0-9.]+):.*#\1#')" \
        && echo "IGMP group joined" || echo "(group not visible via 'ip maddr' on this host)"
fi
sleep 15

call 5 stop
call 6 close
call 7 status

echo "=== Done ==="
