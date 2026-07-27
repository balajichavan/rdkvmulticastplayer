#!/usr/bin/env bash
#
# multicast-server.sh
#
# Streams a publicly available video as an MPEG-TS IP-multicast feed so you can
# test the MulticastPlayer plugin / widget without a real head-end.
#
# It loops the source forever and pushes CBR MPEG-TS to a multicast group over
# UDP (default) or RTP. Any host on the same L2 network / VLAN can then join.
#
# Requirements: ffmpeg (preferred) or gstreamer1.0-tools.
#
# Usage:
#   ./multicast-server.sh                       # UDP 239.1.1.1:5000, Big Buck Bunny
#   GROUP=239.1.1.2 PORT=5000 ./multicast-server.sh
#   TRANSPORT=rtp ./multicast-server.sh
#   SOURCE=./local.ts ./multicast-server.sh     # use your own .ts / .mp4
#   SOURCE="<url>" ./multicast-server.sh         # any public media URL
#
set -euo pipefail

GROUP="${GROUP:-239.1.1.1}"
PORT="${PORT:-5000}"
TRANSPORT="${TRANSPORT:-udp}"     # udp | rtp
TTL="${TTL:-16}"
BITRATE="${BITRATE:-4000k}"

# --- Publicly available sample sources -------------------------------------
# Reliable, freely redistributable test assets:
#   BBB   Big Buck Bunny (Google CDN, MP4)  -> remuxed to MPEG-TS here
#   APPLE Apple BipBop reference TS segment (already MPEG-TS)
BBB_URL="https://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4"
APPLE_TS="https://devstreaming-cdn.apple.com/videos/streaming/examples/bipbop_16x9/gear1/fileSequence0.ts"

SOURCE="${SOURCE:-$BBB_URL}"

echo "==> Multicast test server"
echo "    source     : ${SOURCE}"
echo "    destination : ${TRANSPORT}://${GROUP}:${PORT} (ttl=${TTL})"
echo "    bitrate     : ${BITRATE}"
echo "    other public TS sample you can try: SOURCE=\"${APPLE_TS}\""
echo

if command -v ffmpeg >/dev/null 2>&1; then
    if [[ "${TRANSPORT}" == "rtp" ]]; then
        OUT="rtp://${GROUP}:${PORT}?ttl=${TTL}"
        FMT="rtp_mpegts"
    else
        OUT="udp://${GROUP}:${PORT}?ttl=${TTL}&pkt_size=1316"
        FMT="mpegts"
    fi
    echo "==> Streaming with ffmpeg (Ctrl-C to stop)"
    # -stream_loop -1 loops forever; -re paces at real time; CBR-ish mux.
    exec ffmpeg -hide_banner -loglevel warning -re -stream_loop -1 -i "${SOURCE}" \
        -c:v libx264 -preset veryfast -tune zerolatency -b:v "${BITRATE}" \
        -x264-params "nal-hrd=cbr:keyint=30:min-keyint=30" \
        -c:a aac -b:a 128k -ar 48000 \
        -f "${FMT}" -muxrate "${BITRATE}" "${OUT}"

elif command -v gst-launch-1.0 >/dev/null 2>&1; then
    echo "==> ffmpeg not found; streaming with gst-launch-1.0 (Ctrl-C to stop)"
    if [[ "${TRANSPORT}" == "rtp" ]]; then
        exec gst-launch-1.0 -v uridecodebin uri="${SOURCE}" ! videoconvert \
            ! x264enc bitrate=4000 key-int-max=30 tune=zerolatency \
            ! mpegtsmux ! rtpmp2tpay ! udpsink host="${GROUP}" port="${PORT}" \
              auto-multicast=true ttl-mc="${TTL}"
    else
        exec gst-launch-1.0 -v uridecodebin uri="${SOURCE}" ! videoconvert \
            ! x264enc bitrate=4000 key-int-max=30 tune=zerolatency \
            ! mpegtsmux ! udpsink host="${GROUP}" port="${PORT}" \
              auto-multicast=true ttl-mc="${TTL}"
    fi
else
    echo "ERROR: neither ffmpeg nor gst-launch-1.0 is installed." >&2
    echo "Install one:  sudo apt install ffmpeg   # or   gstreamer1.0-tools" >&2
    exit 1
fi
