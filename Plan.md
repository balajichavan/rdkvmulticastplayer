# IP Multicast Playback in RDKV Player App — Plan

## Objective
Play an IP multicast stream (MPEG-TS over UDP/RTP) inside an RDKV player UI app using the platform's hardware demux/decode pipeline, controlled from the UI via Thunder (WPEFramework) JSON-RPC.

## Key Principles
- Do **not** feed a raw multicast URL to a generic HTML `<video>` element.
- Route playback through the RDK media stack for hardware decode (westeros/brcm sinks).
- Manage IGMP join/leave and decoder resources at the platform, not the UI.
- Keep the UI app portable: it only issues JSON-RPC control calls.

## Options Overview

### Option 1 — AAMP with Multicast/MABR (Recommended)
- AAMP (Advanced Adaptive Media Player) is the standard RDK-V player.
- Uses built-in Multicast/MABR helper (multicast-to-unicast proxy).
- Supports ABR, DRM/CAS (OCDM), and hardware decode.
- UI drives it via `org.rdk.MediaPlayer` JSON-RPC.
- Locator example: `mcast://<group-ip>:<port>`.

**Flow:** UI App → Thunder (AAMP Player Plugin) → AAMP Core (MABR + TS Demux + DRM) → GStreamer (brcmvideodecoder → westerossink) → Display.

### Option 2 — Native GStreamer Pipeline via Custom Thunder Plugin
- For simple, unencrypted CBR multicast.
- Pipeline: `udpsrc/rtpbin → (rtpmp2tdepay) → tsdemux → brcmvideodecoder → westerossink`.
- No ABR, manual DRM handling.
- Wrap pipeline in a custom Thunder plugin exposing play/stop/tune.

**Flow:** UI App → Thunder (Custom Multicast Plugin) → Native GStreamer Pipeline → Display.

### Option 3 — MSO Edge Multicast-to-Unicast (ABR)
- Network edge converts multicast to HLS/DASH unicast.
- Device does no IGMP join; standard AAMP ABR fetch.
- Best when the network already has an edge packager.

**Flow:** MSO Edge (M2U / ABR packager) → AAMP (standard ABR) → GStreamer → Display.

## Comparison

| Aspect | Option 1: AAMP MABR | Option 2: Native GStreamer | Option 3: Edge M2U ABR |
|---|---|---|---|
| Best for | Standard RDK multicast IPTV | Simple unencrypted CBR | Networks with edge packagers |
| IGMP join | On device (AAMP helper) | On device (udpsrc) | At network edge |
| ABR support | Yes (MABR) | No | Yes |
| DRM/CAS | Yes (OCDM) | Manual | Yes |
| Portability across SoC | High | Medium | High |
| Dev effort | Low–Medium | Medium–High | Low (device) |

## Recommendation
- **Primary:** Option 1 (AAMP with MABR) for production RDKV apps.
- **If network has an edge packager:** Option 3.
- **For lightweight unencrypted CBR trials:** Option 2.

## Key Platform Considerations
- IGMP join/leave managed by platform (network + firewall/QoS config).
- Hardware decode path (westeros/brcm sinks) required — no software video element.
- DRM/CAS: integrate via RDK OCDM/PlayReady/Verimatrix path in AAMP.

## Next Steps
1. Confirm current player engine (AAMP, custom Thunder plugin, or Lightning template).
2. Define multicast source format (UDP vs RTP, CBR vs ABR, encrypted vs clear).
3. Implement Option 1 JSON-RPC integration and AAMP locator wiring.
4. Validate IGMP join, decoder resource handling, and DRM path.
