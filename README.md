# RDKV IP-Multicast Widget

A **downloadable RDKV carousel widget** (Lightning.js UI) that plays an
unencrypted, constant-bitrate IP-multicast MPEG-TS stream through a **native
GStreamer Thunder plugin**. This implements *delivery mechanism 2 (web widget on
the resident-app carousel)* + *playback option 2 (native GStreamer pipeline)*
from [Plan.md](Plan.md).

The widget is **UI/control only**. All media handling — IGMP join/leave, TS
demux, hardware decode and rendering via `westerossink` — lives in the native
plugin. The widget drives it over Thunder JSON-RPC.

---

## 1. Project overview

Playing an IP-multicast channel on an RDK-V device is split into two independent,
separately-deployable pieces:

| Piece | Runs as | Responsibility |
|-------|---------|----------------|
| **Native plugin** `org.rdk.MulticastPlayer` | WPEFramework/Thunder out-of-process plugin (C++) | Builds and drives the GStreamer pipeline, joins/leaves the IGMP group, hardware-decodes and renders through `westerossink`, and exposes a JSON-RPC API. |
| **Web widget** `Multicast Live` | Downloadable Lightning.js app tile on the resident-app carousel | Renders the channel list + a transparent video window and calls the plugin over Thunder JSON-RPC. Contains **no** media logic. |

This separation is the key RDK pattern: the widget can be updated from a CDN
without a firmware change, while the platform keeps full ownership of the media
pipeline, decoder resources and multicast networking.

### Data / control flow

```
┌────────────────────────┐   Thunder JSON-RPC    ┌──────────────────────────────┐
│  Lightning Widget (UI) │  ───────────────────► │  org.rdk.MulticastPlayer      │
│  carousel tile         │   open/play/stop/     │  (native WPEFramework plugin) │
│  MulticastService.js   │ ◄───────────────────  │  GstMulticastPipeline         │
└────────────────────────┘   onStatusChanged/    └──────────────┬───────────────┘
        │  RDKShell surface + focus                             │ GStreamer
        ▼                                                        ▼
   graphics plane (transparent hole)     udpsrc → (rtpmp2tdepay) → tsdemux → <hw decoder>
                                                         → westerossink → HDMI
```

### Repository layout

| Path | Purpose |
|------|---------|
| [plugin/MulticastPlayer/](plugin/MulticastPlayer) | Native Thunder plugin (C++/GStreamer) |
| [plugin/MulticastPlayer/GstMulticastPipeline.cpp](plugin/MulticastPlayer/GstMulticastPipeline.cpp) | Multicast pipeline + IGMP + geometry |
| [plugin/MulticastPlayer/MulticastPlayer.cpp](plugin/MulticastPlayer/MulticastPlayer.cpp) | JSON-RPC control surface + events |
| [plugin/MulticastPlayer/MulticastPlayer.json](plugin/MulticastPlayer/MulticastPlayer.json) | JSON-RPC interface definition |
| [widget/](widget) | Lightning.js carousel widget |
| [widget/src/App.js](widget/src/App.js) | UI: channel list, video window, status |
| [widget/src/lib/MulticastService.js](widget/src/lib/MulticastService.js) | ThunderJS control client |
| [provisioning/](provisioning) | Packaging, signing, catalog entry |

### JSON-RPC API (callsign `org.rdk.MulticastPlayer`)

| Method | Params | Effect |
|--------|--------|--------|
| `open` | `uri`, `transport` (`auto`\|`udp`\|`rtp`), `interface?` | Build pipeline (no join) |
| `play` | — | PLAYING + IGMP join |
| `stop` | — | NULL + IGMP leave |
| `close` | — | Tear down, release resources |
| `setVideoRectangle` | `x`,`y`,`w`,`h` | Position westerossink |
| `status` | — | Current state |

Events: `onStatusChanged {state}`, `onError {code,message}`, `onEnd`.

---

## 2. Prerequisites

You need a **Linux build host** (Ubuntu 20.04/22.04 recommended) to build the RDK
image, and a **Raspberry Pi 4** (or Pi 3) as the target. RDK-V has an official
Raspberry Pi reference build, so the Pi acts as the "equivalent RDK platform".

Build-host packages (Yocto/RDK requirements):

```bash
sudo apt update
sudo apt install -y gawk wget git diffstat unzip texinfo gcc build-essential \
  chrpath socat cpio python3 python3-pip python3-pexpect xz-utils debianutils \
  iputils-ping python3-git python3-jinja2 libegl1-mesa libsdl1.2-dev \
  python3-subunit zstd liblz4-tool file locales repo bmaptool

sudo locale-gen en_US.UTF-8
```

Also install the RDK code-access tooling and register an account on
`code.rdkcentral.com` (needed for `repo` to pull RDK layers).

For the widget you additionally need **Node.js 18+** and the Lightning CLI:

```bash
npm install -g @lightningjs/cli
```

---

## 3. Build & flash the RDK-V image for Raspberry Pi

> This produces a bootable RDK-V SD-card image. It is a large (~50 GB, multi-hour)
> Yocto build. Skip to §4 if you already have an RDK-V Pi image and only want to
> add the plugin.

**Step 3.1 — Fetch the RDK-V Raspberry Pi manifest**

```bash
mkdir -p ~/rdkv-rpi && cd ~/rdkv-rpi
repo init -u https://code.rdkcentral.com/r/manifests -m rdkv-nosrc.xml -b rdkv-2023
repo sync -j$(nproc) --no-clone-bundle
```

**Step 3.2 — Add this plugin to the build**

Copy the plugin into the RDK Services layer (or a small custom layer) and add a
recipe so BitBake compiles it into the image:

```bash
# Place the source where a recipe can reach it
cp -r ~/proposals/playeruiapp/plugin/MulticastPlayer \
      meta-rdk-video/recipes-extended/rdkservices/files/MulticastPlayer
```

Create `meta-rdk-video/recipes-extended/rdkservices/multicastplayer_1.0.bb`:

```bitbake
SUMMARY = "RDKV IP-multicast player Thunder plugin"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

DEPENDS = "wpeframework gstreamer1.0 gstreamer1.0-plugins-base"
RDEPENDS:${PN} = "gstreamer1.0 gstreamer1.0-plugins-good gstreamer1.0-plugins-bad westeros"

SRC_URI = "file://MulticastPlayer"
S = "${WORKDIR}/MulticastPlayer"

inherit cmake pkgconfig

EXTRA_OECMAKE = "-DCMAKE_INSTALL_PREFIX=/usr"
FILES:${PN} += "${libdir}/wpeframework/plugins/*"
```

Add the recipe to the image by appending to your image/Thunder packagegroup
(e.g. in `meta-rdk-video/recipes-extended/rdkservices/rdkservices.bbappend` or the
image `.bb`):

```bitbake
IMAGE_INSTALL:append = " multicastplayer"
```

**Step 3.3 — Configure the build and bake**

```bash
cd ~/rdkv-rpi
MACHINE=raspberrypi4-64-rdk-mc source meta-cmf-raspberrypi/setup-environment
# choose the rdkv image when prompted, then:
bitbake rdk-generic-mediaclient-wpe-image
```

**Step 3.4 — Flash to SD card**

```bash
cd ~/rdkv-rpi/build-*/tmp/deploy/images/raspberrypi4-64-rdk-mc
sudo bmaptool copy rdk-generic-mediaclient-wpe-image-*.wic.bz2 /dev/sdX
# (replace /dev/sdX with your SD card device — double-check with lsblk!)
```

Insert the card, connect HDMI + Ethernet, and power the Pi.

---

## 4. (Faster path) Cross-compile just the plugin against the RDK SDK

If you already have an RDK-V Pi image, generate its SDK once and cross-compile the
plugin without a full image rebuild.

```bash
# On the build host, generate & install the cross SDK
bitbake rdk-generic-mediaclient-wpe-image -c populate_sdk
./tmp/deploy/sdk/rdk-glibc-x86_64-*-raspberrypi4-64-toolchain-*.sh   # installs to /opt/rdk/...

# Cross-compile the plugin
source /opt/rdk/*/environment-setup-*-rdk-linux
cd ~/proposals/playeruiapp/plugin/MulticastPlayer
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE="$OECORE_NATIVE_SYSROOT/usr/share/cmake/OEToolchainConfig.cmake" \
  -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build -j$(nproc)
# -> build/libWPEFrameworkMulticastPlayer.so
```

Copy the artifacts to the Pi and register the plugin:

```bash
scp build/libWPEFrameworkMulticastPlayer.so \
    root@<pi-ip>:/usr/lib/wpeframework/plugins/
scp ../../provisioning/MulticastPlayer.plugin-config.json \
    root@<pi-ip>:/etc/WPEFramework/plugins/MulticastPlayer.json
ssh root@<pi-ip> 'systemctl restart wpeframework'
```

> **Raspberry Pi decoder note:** the pipeline in
> [GstMulticastPipeline.cpp](plugin/MulticastPlayer/GstMulticastPipeline.cpp)
> defaults to Broadcom STB elements (`brcmvideodecoder` / `brcmaudiodecoder`).
> On the Raspberry Pi RDK build use the V4L2 stateful decoder instead — change
> the video decoder factory from `brcmvideodecoder` to **`v4l2h264dec`** (and the
> audio decoder to a software `avdec_aac`). `westerossink` is the same on both.

---

## 5. Build & deploy the widget

```bash
cd ~/proposals/playeruiapp/widget
npm install
npm run build            # lng build -> dist/
```

Side-load onto the Pi for a quick test (served by the resident browser/WPE):

```bash
scp -r dist/* root@<pi-ip>:/opt/www/multicast-widget/
```

Or register it as a real downloadable carousel app:

```bash
scp provisioning/app-catalog-entry.json root@<pi-ip>:/opt/persistent/appcatalog/
ssh root@<pi-ip> 'systemctl restart com.comcast.xdiscovery 2>/dev/null || true'
```

---

## 6. Run & verify on the Pi

**Step 6.1 — Confirm the plugin is loaded**

```bash
curl -s http://<pi-ip>:9998/jsonrpc \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":1,"method":"Controller.1.status@org.rdk.MulticastPlayer"}' | jq
```

**Step 6.2 — Drive playback directly over JSON-RPC** (sanity check before the UI)

```bash
PI=http://<pi-ip>:9998/jsonrpc
API=org.rdk.MulticastPlayer.1

curl -s $PI -d '{"jsonrpc":"2.0","id":1,"method":"'$API'.open","params":{"uri":"udp://239.1.1.1:5000","transport":"udp"}}'
curl -s $PI -d '{"jsonrpc":"2.0","id":2,"method":"'$API'.setVideoRectangle","params":{"x":0,"y":0,"w":1920,"h":1080}}'
curl -s $PI -d '{"jsonrpc":"2.0","id":3,"method":"'$API'.play"}'
# ... watch the stream ...
curl -s $PI -d '{"jsonrpc":"2.0","id":4,"method":"'$API'.stop"}'
curl -s $PI -d '{"jsonrpc":"2.0","id":5,"method":"'$API'.close"}'
```

**Step 6.3 — Feed a test multicast stream** (from any host on the same LAN)

```bash
gst-launch-1.0 videotestsrc is-live=true ! x264enc bitrate=4000 key-int-max=30 \
  ! mpegtsmux ! udpsink host=239.1.1.1 port=5000 auto-multicast=true
```

**Step 6.4 — Launch the widget** from the carousel and pick a channel; video
should appear in the window and the status line should read `PLAYING`.

**Step 6.5 — Verify IGMP join/leave**

```bash
ssh root@<pi-ip> 'ip maddr show dev eth0 | grep 239.1.1.1'   # present while playing
# after stop/close the group should disappear
```

---

## 7. Testing

Two helper scripts under [test/](test) let you validate the whole path without a
real head-end.

### 7.1 Start a sample multicast server (publicly available media)

[test/multicast-server.sh](test/multicast-server.sh) loops a freely
redistributable sample and pushes CBR MPEG-TS to a multicast group. Run it on a
host on the **same L2 network / VLAN** as the Pi (multicast is not routed by
default).

```bash
# Default: Big Buck Bunny (Google CDN) -> udp://239.1.1.1:5000
test/multicast-server.sh

# Pick a group/port or RTP transport
GROUP=239.1.1.2 PORT=5000 test/multicast-server.sh
TRANSPORT=rtp test/multicast-server.sh

# Use Apple's public BipBop MPEG-TS sample instead
SOURCE="https://devstreaming-cdn.apple.com/videos/streaming/examples/bipbop_16x9/gear1/fileSequence0.ts" \
  test/multicast-server.sh

# Or your own local file (.ts or .mp4)
SOURCE=./mystream.ts test/multicast-server.sh
```

Public sample sources used:

| Name | URL | Format |
|------|-----|--------|
| Big Buck Bunny (default) | `https://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4` | MP4 → remuxed to TS |
| Apple BipBop segment | `https://devstreaming-cdn.apple.com/videos/streaming/examples/bipbop_16x9/gear1/fileSequence0.ts` | MPEG-TS |

> Requires `ffmpeg` (preferred) or `gstreamer1.0-tools` on the streaming host:
> `sudo apt install ffmpeg`.

### 7.2 Confirm the feed locally (optional)

Before involving the device, verify the multicast feed plays on the streaming
LAN with any standard player:

```bash
ffplay -fflags nobuffer "udp://239.1.1.1:5000"
# or
vlc udp://@239.1.1.1:5000
```

### 7.3 Drive the plugin over JSON-RPC

[test/jsonrpc-test.sh](test/jsonrpc-test.sh) runs the full
`open → play → status → stop → close` sequence against a running Thunder and
checks the IGMP join.

```bash
HOST=<pi-ip> test/jsonrpc-test.sh
HOST=<pi-ip> URI=udp://239.1.1.2:5000 TRANSPORT=udp test/jsonrpc-test.sh
```

### 7.4 End-to-end widget test

1. Start the server (§7.1) on a LAN host.
2. Ensure the channel line-up in
   [widget/src/lib/channels.js](widget/src/lib/channels.js) matches the group/port
   you streamed to.
3. Launch the **Multicast Live** tile on the Pi carousel and select the channel.
4. Expect: video in the window, status `PLAYING`, and the group visible in
   `ip maddr show`. Pressing Back stops playback and leaves the group.

---

## 8. Troubleshooting

| Symptom | Likely cause / fix |
|---------|--------------------|
| Plugin missing from `Controller.status` | `.so` not in `/usr/lib/wpeframework/plugins/` or config JSON absent; restart `wpeframework`. |
| Black video, status `PLAYING` | Wrong decoder element for the SoC — set `v4l2h264dec` on Raspberry Pi (see §4 note). |
| `open` returns `success:false` | Bad locator; use `udp://<group>:<port>` and check the group is in the multicast range. |
| No video plane behind graphics | Widget stage `clearColor` must be transparent (already set in [settings.json](widget/settings.json)); confirm RDKShell z-order. |
| Group never joined | Wrong `interface`; set it in the plugin config (`eth0` default) or `open` params. |

---

## 9. Package for production (downloadable)

```bash
provisioning/sign-and-package.sh \
  plugin/MulticastPlayer/build/libWPEFrameworkMulticastPlayer.so \
  widget/dist
```
Then:
1. Upload the signed plugin package to the Download Manager / firmware bundle and
   add `org.rdk.MulticastPlayer` to the platform plugin allowlist.
2. Upload the widget bundle to the app CDN and register
   [provisioning/app-catalog-entry.json](provisioning/app-catalog-entry.json) so
   the tile appears on the resident-app carousel.

---

## 10. Scope / assumptions
- Unencrypted **CBR** multicast (no ABR, no DRM) — consistent with playback
  option 2. For ABR/DRM use AAMP (option 1).
- The widget requests a surface via RDKShell; the plugin owns the decoder.
- IGMP join happens on `play`, leave on `stop`/`close`.
- Broadcom decoder element names are the default; use `v4l2h264dec` on the
  Raspberry Pi reference build.
