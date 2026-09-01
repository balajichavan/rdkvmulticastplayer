/**
 * App.js
 *
 * Lightning.js carousel widget for IP-multicast playback. It renders a channel
 * list plus a transparent video window and drives the native MulticastPlayer
 * plugin over Thunder JSON-RPC. The Lightning stage clear color is transparent
 * so the underlying westerossink video punches through the graphics plane.
 */
import { Lightning, Utils } from '@lightningjs/sdk'
import MulticastService from './lib/MulticastService.js'
import channels from './lib/channels.js'

// 1080p design canvas. westerossink renders on the video plane behind graphics.
const VIDEO_RECT = { x: 0, y: 0, w: 1920, h: 1080 }

export default class App extends Lightning.Component {
  static _template() {
    return {
      Background: {
        w: 1920,
        h: 1080,
        rect: true,
        color: 0xff0b1f3a,
      },
      Sidebar: {
        x: 40,
        y: 40,
        w: 380,
        h: 1000,
        rect: true,
        color: 0xcc0f2440,
        Title: {
          x: 24,
          y: 24,
          text: { text: 'Multicast Live', fontSize: 34, textColor: 0xffffffff },
        },
        List: { x: 20, y: 100 },
      },
      // Transparent hole the video plane shows through.
      VideoWindow: {
        x: VIDEO_RECT.x,
        y: VIDEO_RECT.y,
        w: VIDEO_RECT.w,
        h: VIDEO_RECT.h,
        rect: true,
        color: 0x00000000,
        Placeholder: {
          mount: 0.5,
          x: VIDEO_RECT.w / 2,
          y: VIDEO_RECT.h / 2,
          text: { text: 'Select a channel', fontSize: 28, textColor: 0x88ffffff },
        },
      },
      StatusBar: {
        x: VIDEO_RECT.x,
        y: VIDEO_RECT.y + VIDEO_RECT.h + 20,
        w: VIDEO_RECT.w,
        h: 48,
        Status: {
          text: { text: 'IDLE', fontSize: 24, textColor: 0xff00acc1 },
        },
      },
      ErrorOverlay: {
        alpha: 0,
        x: VIDEO_RECT.x,
        y: VIDEO_RECT.y,
        w: VIDEO_RECT.w,
        h: VIDEO_RECT.h,
        rect: true,
        color: 0xcc7a1414,
        Message: {
          mount: 0.5,
          x: VIDEO_RECT.w / 2,
          y: VIDEO_RECT.h / 2,
          text: { text: '', fontSize: 26, textColor: 0xffffffff, wordWrap: true, wordWrapWidth: VIDEO_RECT.w - 80 },
        },
      },
    }
  }

  _construct() {
    this._index = 0
    this._service = new MulticastService()
  }

  async _init() {
    this._renderList()
    this._service.listen((event, payload) => this._onPluginEvent(event, payload))
    // Reserve the video rectangle up front so geometry is correct on first play.
    await this._service.setVideoRectangle(VIDEO_RECT)
  }

  _renderList() {
    const children = channels.map((ch, i) => ({
      type: ChannelItem,
      x: 0,
      y: i * 76,
      w: 340,
      label: ch.name,
      selected: i === this._index,
    }))
    this.tag('List').children = children
  }

  _focusList() {
    this.tag('List').children.forEach((c, i) => c.selected = i === this._index)
  }

  _getFocused() {
    return this
  }

  // --- Key handling ---------------------------------------------------------

  _handleUp() {
    if (this._index > 0) {
      this._index--
      this._focusList()
    }
  }

  _handleDown() {
    if (this._index < channels.length - 1) {
      this._index++
      this._focusList()
    }
  }

  async _handleEnter() {
    await this._playSelected()
  }

  async _handleBack() {
    //await this._service.stop()
    await this._service.stop()
    await this._service.close()
    this.tag('Placeholder').alpha = 1
    this._setStatus('STOPPED')
  }

  // --- Playback control -----------------------------------------------------

  async _playSelected() {
    const channel = channels[this._index]
    this._hideError()
    this._setStatus('OPENING')

    const opened = await this._service.open({ uri: channel.uri, transport: channel.transport })
    if (!opened || opened.success === false) {
      this._showError(opened && opened.message ? opened.message : 'Failed to open stream')
      return
    }

    await this._service.setVideoRectangle(VIDEO_RECT)
    const played = await this._service.play()
    if (!played || played.success === false) {
      this._showError(played && played.message ? played.message : 'Failed to start playback')
    }
  }

  _onPluginEvent(event, payload) {
    if (event === 'onStatusChanged') {
      this._setStatus(payload.state || 'UNKNOWN')
      if (payload.state === 'PLAYING') {
        this.tag('Placeholder').alpha = 0
      }
    } else if (event === 'onError') {
      this._showError(`[${payload.code}] ${payload.message}`)
    } else if (event === 'onEnd') {
      this._setStatus('STOPPED')
    }
  }

  // --- UI helpers -----------------------------------------------------------

  _setStatus(text) {
    this.tag('Status').text.text = text
  }

  _showError(message) {
    this.tag('Message').text.text = message
    this.tag('ErrorOverlay').setSmooth('alpha', 1)
    this._setStatus('ERROR')
  }

  _hideError() {
    this.tag('ErrorOverlay').setSmooth('alpha', 0)
  }

  async _detach() {
    // Release the multicast group and decoder when the widget is torn down.
    await this._service.stop()
    await this._service.close()
  }
}

class ChannelItem extends Lightning.Component {
  static _template() {
    return {
      h: 68,
      rect: true,
      color: 0x00000000,
      Label: {
        x: 20,
        y: 18,
        text: { text: '', fontSize: 26, textColor: 0xffffffff },
      },
    }
  }

  set label(v) {
    this.tag('Label').text.text = v
  }

  set selected(v) {
    this.patch({ color: v ? 0xff1e88e5 : 0x00000000 })
  }
}
