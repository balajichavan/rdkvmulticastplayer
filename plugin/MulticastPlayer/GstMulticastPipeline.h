/**
 * GstMulticastPipeline.h
 *
 * Native GStreamer multicast MPEG-TS player for the
 * MulticastPlayer Thunder plugin.
 *
 * Supports:
 *   UDP-TS:
 *      udpsrc ! tsdemux
 *
 *   RTP-TS:
 *      udpsrc ! rtpmp2tdepay ! tsdemux
 *
 * Video:
 *      tsdemux -> queue -> h264parse -> video sink
 *
 * Audio:
 *      tsdemux -> queue -> mpegaudioparse -> decoder
 *              -> audioconvert -> audioresample
 *              -> queue -> capsfilter -> audiosink
 */

#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <gst/gst.h>

namespace WPEFramework {
namespace Plugin {

class GstMulticastPipeline {
public:

    enum class State {
        Idle,
        Opened,
        Playing,
        Stopped,
        Error
    };

    enum class Transport {
        Auto,
        Udp,
        Rtp
    };

    using StatusCallback =
        std::function<void(State)>;

    using ErrorCallback =
        std::function<void(int code,
                           const std::string& message)>;

    using EosCallback =
        std::function<void()>;

    GstMulticastPipeline();
    ~GstMulticastPipeline();

    GstMulticastPipeline(
        const GstMulticastPipeline&) = delete;

    GstMulticastPipeline& operator=(
        const GstMulticastPipeline&) = delete;

    /*
     * Callbacks
     */
    void SetStatusCallback(StatusCallback cb)
    {
        _onStatus = std::move(cb);
    }

    void SetErrorCallback(ErrorCallback cb)
    {
        _onError = std::move(cb);
    }

    void SetEosCallback(EosCallback cb)
    {
        _onEos = std::move(cb);
    }

    /*
     * Configure decoder/sink profile.
     *
     * Empty string = automatic detection.
     */
    void SetElementProfile(
        const std::string& videoDecoder,
        const std::string& audioDecoder,
        const std::string& videoSink);

    /*
     * Open multicast stream.
     *
     * Examples:
     *   udp://239.1.1.1:5000
     *   rtp://239.1.1.1:5000
     */
    bool Open(
        const std::string& uri,
        Transport transport,
        const std::string& iface);

    /*
     * Start playback.
     */
    bool Play();

    /*
     * Stop playback.
     */
    bool Stop();

    /*
     * Close and destroy pipeline.
     */
    void Close();

    bool Tune(
    const std::string& ip,
    int port);

    /*
     * Set video rectangle.
     */
    bool SetVideoRectangle(
        int x,
        int y,
        int width,
        int height);

    /*
     * Get current state.
     */
    State GetState() const;

private:

    /*
     * GStreamer bus callback.
     */
    static gboolean BusCallback(
        GstBus* bus,
        GstMessage* message,
        gpointer user_data);

    /*
     * GStreamer tsdemux dynamic pad callback.
     *
     * This is a class member so it can access private
     * pipeline elements such as _videoQueue and _audioQueue.
     */
    static void OnDemuxPadAdded(
        GstElement* demux,
        GstPad* pad,
        gpointer userData);

    /*
     * URI parser.
     */
    bool ParseUri(
        const std::string& uri,
        std::string& host,
        int& port) const;

    /*
     * State management.
     */
    void SetState(State state);

    /*
     * Destroy GStreamer objects.
     */
    void Cleanup();

    /*
     * Create first available GStreamer element.
     *
     * If override is specified, only that element is attempted.
     */
    GstElement* MakeFirstAvailable(
        const std::string& override,
        const std::vector<std::string>& candidates,
        const char* elementName,
        std::string& chosen);

    /*
     * Apply video rectangle to the selected video sink.
     */
    bool ApplyRectangleToSink();

private:

    /*
     * Synchronization.
     */
    mutable std::mutex _lock;

    /*
     * Main GStreamer pipeline.
     */
    GstElement* _pipeline{ nullptr };

    /*
     * UDP/RTP source.
     */
    GstElement* _source{ nullptr };

    GstElement* _tsParse{ nullptr };

    /*
     * RTP depayloader.
     *
     * Only used for RTP transport.
     */
    GstElement* _depay{ nullptr };


    /*
     * MPEG-TS demuxer.
     */
    GstElement* _demux{ nullptr };

    /*
     * ============================
     * VIDEO PIPELINE
     * ============================
     */
    GstElement* _videoQueue{ nullptr };

    GstElement* _videoParse{ nullptr };

    /*
     * Kept for API/profile compatibility.
     *
     * Current RDK-E video path uses the selected video
     * sink directly after h264parse.
     */
    GstElement* _videoDecoder{ nullptr };

    GstElement* _videoSink{ nullptr };

    /*
     * ============================
     * AUDIO PIPELINE
     * ============================
     */
    GstElement* _audioQueue{ nullptr };

    GstElement* _audioParse{ nullptr };

    GstElement* _audioDecoder{ nullptr };

    GstElement* _audioConvert{ nullptr };

    GstElement* _audioResample{ nullptr };

    GstElement* _audioQueue2{ nullptr };

    GstElement* _audioCaps{ nullptr };

    GstElement* _audioSink{ nullptr };

    /*
     * GStreamer bus watch.
     */
    guint _busWatchId{ 0 };

    GMainContext* _busContext{ nullptr };

    /*
     * Current player state.
     */
    State _state{ State::Idle };

    /*
     * Multicast information.
     */
    std::string _host;

    int _port{ 0 };

    std::string _iface;

    /*
     * Video rectangle.
     */
    int _rectX{ 0 };

    int _rectY{ 0 };

    int _rectW{ 0 };

    int _rectH{ 0 };

    /*
     * Element overrides.
     */
    std::string _videoDecoderOverride;

    std::string _audioDecoderOverride;

    std::string _videoSinkOverride;

    /*
     * Actual elements selected at runtime.
     */
    std::string _chosenVideoDecoder;

    std::string _chosenAudioDecoder;

    std::string _chosenVideoSink;

    /*
     * Video sink geometry property.
     */
    const char* _rectProperty{ nullptr };

    /*
     * Callbacks.
     */
    StatusCallback _onStatus;

    ErrorCallback _onError;

    EosCallback _onEos;
};

} // namespace Plugin
} // namespace WPEFramework
