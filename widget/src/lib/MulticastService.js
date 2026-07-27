/**
 * MulticastService.js
 *
 * Thin wrapper around the Thunder (WPEFramework) JSON-RPC bridge that talks to
 * the native `org.rdk.MulticastPlayer` plugin. The widget never touches the
 * media pipeline directly; it only issues control calls and listens to events.
 */
import ThunderJS from 'ThunderJS'

const CALLSIGN = 'org.rdk.MulticastPlayer'

export default class MulticastService {
  constructor(config = {}) {
    this._thunder = ThunderJS({
      host: config.host || '127.0.0.1',
      port: config.port || 9998,
      token: config.token || undefined,
    })
    this._listeners = {}
    this._subscribed = false
  }

  /**
   * Subscribe to plugin notifications and forward them to the caller.
   * @param {(event: string, payload: object) => void} handler
   */
  async listen(handler) {
    this._handler = handler
    if (this._subscribed) return

    const forward = event => payload => {
      if (typeof this._handler === 'function') {
        this._handler(event, payload || {})
      }
    }

    // ThunderJS event subscription for the plugin's notifications.
    await this._thunder.on(CALLSIGN, 'onStatusChanged', forward('onStatusChanged'))
    await this._thunder.on(CALLSIGN, 'onError', forward('onError'))
    await this._thunder.on(CALLSIGN, 'onEnd', forward('onEnd'))
    this._subscribed = true
  }

  /** Build the pipeline for a locator (does not join the group). */
  open({ uri, transport = 'auto', iface }) {
    const params = { uri, transport }
    if (iface) params.interface = iface
    return this._call('open', params)
  }

  /** Start playback and join the IGMP group. */
  play() {
    return this._call('play')
  }

  /** Stop playback and leave the IGMP group. */
  stop() {
    return this._call('stop')
  }

  /** Tear down the pipeline and release resources. */
  close() {
    return this._call('close')
  }

  /** Position the on-screen video rectangle (device pixels). */
  setVideoRectangle({ x = 0, y = 0, w, h }) {
    return this._call('setVideoRectangle', { x, y, w, h })
  }

  /** Query the current pipeline state. */
  status() {
    return this._call('status')
  }

  _call(method, params = {}) {
    return this._thunder
      .call(CALLSIGN, method, params)
      .catch(err => {
        // Normalise Thunder errors so the UI can show a message.
        return { success: false, message: (err && err.message) || String(err) }
      })
  }
}
