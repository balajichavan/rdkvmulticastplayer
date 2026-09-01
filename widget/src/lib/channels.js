/**
 * channels.js - sample multicast channel line-up.
 *
 * Replace with a provisioned line-up (from the app catalog / config service).
 * These are documentation-range multicast groups for local testing only.
 */
export default [
  { name: 'Channel 1 (RTP)', uri: 'rtp://239.100.0.1:8433', transport: 'rtp' },
  { name: 'Channel 2 (RTP)', uri: 'udp://239.100.0.2:8433', transport: 'rtp' },
  { name: 'Channel 3 (RTP)', uri: 'rtp://239.100.0.3:8433', transport: 'rtp' },
]
