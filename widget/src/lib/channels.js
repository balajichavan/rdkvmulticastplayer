/**
 * channels.js - sample multicast channel line-up.
 *
 * Replace with a provisioned line-up (from the app catalog / config service).
 * These are documentation-range multicast groups for local testing only.
 */
export default [
  { name: 'Channel 1', uri: 'udp://239.1.1.1:5000', transport: 'udp' },
  { name: 'Channel 2', uri: 'udp://239.1.1.2:5000', transport: 'udp' },
  { name: 'Channel 3 (RTP)', uri: 'rtp://239.1.1.3:5004', transport: 'rtp' },
]
