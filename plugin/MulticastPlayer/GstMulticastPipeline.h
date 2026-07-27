/**
 * GstMulticastPipeline.h
 *
 * Thin C++ wrapper around a GStreamer pipeline that plays an unencrypted,
 * constant-bitrate MPEG-TS IP-multicast stream (UDP-TS or RTP-TS) through the
 * platform hardware decode path (brcmvideodecoder -> westerossink).
 *
 * The pipeline is intentionally CBR / non-ABR (playback option 2). It performs
 * the IGMP join on PLAY and the IGMP leave on STOP via the udpsrc socket.
 */
#pragma once

#include <functional>
#include <mutex>
#include <string>

#include <gst/gst.h>

namespace WPEFramework {
namespace Plugin {

class GstMulticastPipeline {
public:
    enum class State {
        Idle,      // no pipeline
        Opened,    // pipeline built, not playing
        Playing,   // PLAYING, IGMP joined
        Stopped,   // stopped, IGMP left
        Error
    };

    enum class Transport {
        Auto,   // infer from URI scheme
        Udp,    // udp://group:port  -> udpsrc ! tsdemux
        Rtp     // rtp://group:port  -> udpsrc ! rtpmp2tdepay ! tsdemux
    };

    // Callbacks are invoked from the GStreamer bus thread.
    using StatusCallback = std::function<void(State)>;
    using ErrorCallback = std::function<void(int code, const std::string& message)>;
    using EosCallback = std::function<void()>;

    GstMulticastPipeline();
    ~GstMulticastPipeline();

    GstMulticastPipeline(const GstMulticastPipeline&) = delete;
    GstMulticastPipeline& operator=(const GstMulticastPipeline&) = delete;

    void SetStatusCallback(StatusCallback cb) { _onStatus = std::move(cb); }
    void SetErrorCallback(ErrorCallback cb) { _onError = std::move(cb); }
    void SetEosCallback(EosCallback cb) { _onEos = std::move(cb); }

    // Build the pipeline for the given multicast locator.
    // uri examples: "udp://239.1.1.1:5000", "rtp://239.1.1.1:5000".
    // Optional multicast interface (e.g. "eth0"); empty means system default.
    bool Open(const std::string& uri, Transport transport, const std::string& iface);

    // Set the pipeline to PLAYING (joins the IGMP group).
    bool Play();

    // Set the pipeline to NULL and release the socket (leaves the IGMP group).
    bool Stop();

    // Tear down the pipeline completely.
    void Close();

    // Update the on-screen video rectangle handled by westerossink.
    bool SetVideoRectangle(int x, int y, int width, int height);

    State GetState() const;

private:
    static gboolean BusCallback(GstBus* bus, GstMessage* message, gpointer user_data);
    bool ParseUri(const std::string& uri, std::string& host, int& port) const;
    void SetState(State state);
    void Cleanup();

    mutable std::mutex _lock;
    GstElement* _pipeline{ nullptr };
    GstElement* _source{ nullptr };       // udpsrc
    GstElement* _videoSink{ nullptr };     // westerossink
    guint _busWatchId{ 0 };
    GMainContext* _busContext{ nullptr };

    State _state{ State::Idle };

    std::string _host;
    int _port{ 0 };
    std::string _iface;
    int _rectX{ 0 };
    int _rectY{ 0 };
    int _rectW{ 0 };
    int _rectH{ 0 };

    StatusCallback _onStatus;
    ErrorCallback _onError;
    EosCallback _onEos;
};

} // namespace Plugin
} // namespace WPEFramework
