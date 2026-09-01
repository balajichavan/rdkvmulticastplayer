/**
 * GstMulticastPipeline.cpp
 *
 * Native multicast GStreamer pipeline used by the
 * MulticastPlayer Thunder plugin.
 *
 * VBO/FEIP:
 *
 *   feiptsrc -> tsparse -> tsdemux
 *
 * VIDEO:
 *
 *   tsdemux -> queue -> h264parse -> video sink
 *
 * AUDIO:
 *
 *   tsdemux -> queue -> mpegaudioparse -> decoder
 *           -> audioconvert -> audioresample
 *           -> queue -> capsfilter -> alsasink
 *
 * The feiptsrc element is the Nokia VBO/FEIP to
 * GStreamer bridge.
 */

#include "GstMulticastPipeline.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace WPEFramework {
namespace Plugin {

namespace {

constexpr int kErrPipelineBuild = 1001;
constexpr int kErrStateChange   = 1002;
constexpr int kErrStream        = 1003;


/*
 * Link a dynamic tsdemux pad to a queue.
 */
void LinkDemuxPadToQueue(
    GstPad* pad,
    GstElement* queue)
{
    if (pad == nullptr || queue == nullptr) {
        return;
    }

    GstPad* sinkPad =
        gst_element_get_static_pad(
            queue,
            "sink");

    if (sinkPad == nullptr) {
        printf(
            "MulticastPlayer: failed to get "
            "queue sink pad\n");

        return;
    }

    if (!gst_pad_is_linked(sinkPad)) {

        GstPadLinkReturn ret =
            gst_pad_link(
                pad,
                sinkPad);

        printf(
            "MulticastPlayer: pad link result = %d\n",
            ret);

        if (ret != GST_PAD_LINK_OK) {
            printf(
                "MulticastPlayer: failed to "
                "link demux pad\n");
        }
    }

    gst_object_unref(sinkPad);
}

} // anonymous namespace


GstMulticastPipeline::GstMulticastPipeline()
{
    if (gst_is_initialized() == FALSE) {
        gst_init(nullptr, nullptr);
    }
}


GstMulticastPipeline::~GstMulticastPipeline()
{
    Close();
}


void GstMulticastPipeline::SetElementProfile(
    const std::string& videoDecoder,
    const std::string& audioDecoder,
    const std::string& videoSink)
{
    std::lock_guard<std::mutex> guard(_lock);

    _videoDecoderOverride = videoDecoder;
    _audioDecoderOverride = audioDecoder;
    _videoSinkOverride    = videoSink;
}


GstElement* GstMulticastPipeline::MakeFirstAvailable(
    const std::string& override,
    const std::vector<std::string>& candidates,
    const char* elementName,
    std::string& chosen)
{
    std::vector<std::string> order;

    if (!override.empty()) {
        order.push_back(override);
    } else {
        order = candidates;
    }

    for (const std::string& name : order) {

        GstElement* element =
            gst_element_factory_make(
                name.c_str(),
                elementName);

        if (element != nullptr) {

            chosen = name;

            printf(
                "MulticastPlayer: selected %s = %s\n",
                elementName,
                name.c_str());

            return element;
        }
    }

    chosen.clear();

    printf(
        "MulticastPlayer: no element available "
        "for %s\n",
        elementName);

    return nullptr;
}


bool GstMulticastPipeline::ApplyRectangleToSink()
{
    /*
     * Called with _lock held.
     */

    if (_videoSink == nullptr ||
        _rectProperty == nullptr) {

        return true;
    }

    gchar rect[64];

    std::snprintf(
        rect,
        sizeof(rect),
        "%d,%d,%d,%d",
        _rectX,
        _rectY,
        _rectW,
        _rectH);

    g_object_set(
        G_OBJECT(_videoSink),
        _rectProperty,
        rect,
        nullptr);

    printf(
        "MulticastPlayer: video rectangle = %s\n",
        rect);

    return true;
}


bool GstMulticastPipeline::ParseUri(
    const std::string& uri,
    std::string& host,
    int& port) const
{
    std::string work = uri;

    const auto scheme =
        work.find("://");

    if (scheme != std::string::npos) {
        work =
            work.substr(scheme + 3);
    }

    const auto colon =
        work.rfind(':');

    if (colon == std::string::npos) {
        return false;
    }

    host =
        work.substr(0, colon);

    port =
        std::atoi(
            work.substr(
                colon + 1).c_str());

    return (
        !host.empty() &&
        port > 0 &&
        port <= 65535);
}


/*
 * tsdemux creates pads dynamically.
 *
 * We inspect the caps of each pad and decide whether
 * it is a video or audio stream.
 */
void GstMulticastPipeline::OnDemuxPadAdded(
    GstElement* /*demux*/,
    GstPad* pad,
    gpointer userData)
{
    GstMulticastPipeline* self =
        static_cast<GstMulticastPipeline*>(
            userData);

    if (self == nullptr ||
        pad == nullptr) {

        return;
    }

    GstCaps* caps =
        gst_pad_get_current_caps(pad);

    if (caps == nullptr) {

        caps =
            gst_pad_query_caps(
                pad,
                nullptr);
    }

    if (caps == nullptr ||
        gst_caps_is_empty(caps)) {

        printf(
            "MulticastPlayer: demux pad "
            "has no caps\n");

        if (caps != nullptr) {
            gst_caps_unref(caps);
        }

        return;
    }

    const GstStructure* structure =
        gst_caps_get_structure(
            caps,
            0);

    if (structure == nullptr) {

        gst_caps_unref(caps);
        return;
    }

    const gchar* name =
        gst_structure_get_name(
            structure);

    printf(
        "MulticastPlayer: tsdemux "
        "pad-added: %s\n",
        name != nullptr
            ? name
            : "unknown");


    /*
     * -------------------------------------------------
     * VIDEO
     * -------------------------------------------------
     */

    if (name != nullptr &&
        std::strstr(
            name,
            "video") != nullptr) {

        printf(
            "MulticastPlayer: VIDEO "
            "pad detected\n");

        LinkDemuxPadToQueue(
            pad,
            self->_videoQueue);
    }


    /*
     * -------------------------------------------------
     * AUDIO
     * -------------------------------------------------
     */

    else if (name != nullptr &&
             std::strstr(
                 name,
                 "audio") != nullptr) {

        gint mpegVersion = 0;
        gint layer = 0;

        gst_structure_get_int(
            structure,
            "mpegversion",
            &mpegVersion);

        gst_structure_get_int(
            structure,
            "layer",
            &layer);

        printf(
            "MulticastPlayer: AUDIO pad "
            "detected mpegversion=%d "
            "layer=%d\n",
            mpegVersion,
            layer);

        if (std::strstr(
                name,
                "audio/mpeg") != nullptr &&
            mpegVersion == 1 &&
            layer == 2) {

            printf(
                "MulticastPlayer: "
                "MP2 AUDIO pad detected\n");
        }

        /*
         * mpegaudioparse performs the final
         * negotiation.
         */
        LinkDemuxPadToQueue(
            pad,
            self->_audioQueue);
    }


    /*
     * -------------------------------------------------
     * OTHER
     * -------------------------------------------------
     */

    else {

        printf(
            "MulticastPlayer: ignoring "
            "demux stream: %s\n",
            name != nullptr
                ? name
                : "unknown");
    }

    gst_caps_unref(caps);
}


/*
 * -----------------------------------------------------
 * OPEN
 * -----------------------------------------------------
 *
 * New VBO architecture:
 *
 *     feiptsrc
 *         |
 *      tsparse
 *         |
 *      tsdemux
 *       /    \
 *    video   audio
 *
 * The URI is still used to provide the multicast
 * channel IP and port.
 *
 * The actual data source is now feiptsrc.
 */
bool GstMulticastPipeline::Open(
    const std::string& uri,
    Transport /*transport*/,
    const std::string& iface)
{
    std::lock_guard<std::mutex> guard(_lock);

    if (_pipeline != nullptr) {
        Cleanup();
    }

    /*
     * Parse:
     *
     * udp://239.x.x.x:8433
     *
     * or:
     *
     * rtp://239.x.x.x:8433
     *
     *
     * The scheme is no longer used to select
     * udpsrc/rtpmp2tdepay.
     *
     * It is only used to extract IP and port.
     */

    if (!ParseUri(
            uri,
            _host,
            _port)) {

        if (_onError) {

            _onError(
                kErrPipelineBuild,
                "Invalid multicast URI: " + uri);
        }

        SetState(State::Error);

        return false;
    }

    _iface = iface;


    printf(
        "==================================================\n");

    printf(
        "MulticastPlayer: Opening VBO stream\n");

    printf(
        "URI       : %s\n",
        uri.c_str());

    printf(
        "Group     : %s\n",
        _host.c_str());

    printf(
        "Port      : %d\n",
        _port);

    printf(
        "Interface : %s\n",
        _iface.empty()
            ? "default"
            : _iface.c_str());

    printf(
        "Source    : feiptsrc\n");

    printf(
        "Pipeline  : feiptsrc -> tsparse -> tsdemux\n");

    printf(
        "==================================================\n");


    /*
     * -------------------------------------------------
     * CREATE PIPELINE
     * -------------------------------------------------
     */

    _pipeline =
        gst_pipeline_new(
            "multicast-player");


    /*
     * Nokia VBO/FEIP GStreamer source.
     */
    _source =
        gst_element_factory_make(
            "feiptsrc",
            "feip-source");


    /*
     * MPEG-TS parser.
     */
    _tsParse =
        gst_element_factory_make(
            "tsparse",
            "tsparse");


    /*
     * MPEG-TS demux.
     */
    _demux =
        gst_element_factory_make(
            "tsdemux",
            "demux");


    /*
     * No UDP/RTP depayloader is required.
     */
    _depay = nullptr;


    /*
     * -------------------------------------------------
     * VIDEO
     * -------------------------------------------------
     */

    _videoQueue =
        gst_element_factory_make(
            "queue",
            "vqueue");

    _videoParse =
        gst_element_factory_make(
            "h264parse",
            "vparse");

    _videoSink =
        MakeFirstAvailable(
            _videoSinkOverride,
            {
                "westerossink",
                "autovideosink",
                "glimagesink"
            },
            "vsink",
            _chosenVideoSink);


    /*
     * -------------------------------------------------
     * AUDIO
     * -------------------------------------------------
     */

    _audioQueue =
        gst_element_factory_make(
            "queue",
            "aqueue");

    _audioParse =
        gst_element_factory_make(
            "mpegaudioparse",
            "aparse");

    _audioDecoder =
        MakeFirstAvailable(
            _audioDecoderOverride,
            {
                "brcmaudiodecoder",
                "avdec_mp2float"
            },
            "adec",
            _chosenAudioDecoder);

    _audioConvert =
        gst_element_factory_make(
            "audioconvert",
            "aconv");

    _audioResample =
        gst_element_factory_make(
            "audioresample",
            "aresample");

    _audioQueue2 =
        gst_element_factory_make(
            "queue",
            "aqueue2");

    _audioCaps =
        gst_element_factory_make(
            "capsfilter",
            "acaps");

    _audioSink =
        gst_element_factory_make(
            "alsasink",
            "asink");


    /*
     * -------------------------------------------------
     * DEBUG
     * -------------------------------------------------
     */

    printf(
        "MulticastPlayer elements:\n");

    printf(
        "  pipeline     = %p\n",
        _pipeline);

    printf(
        "  feiptsrc     = %p\n",
        _source);

    printf(
        "  tsparse      = %p\n",
        _tsParse);

    printf(
        "  tsdemux      = %p\n",
        _demux);

    printf(
        "  videoQueue   = %p\n",
        _videoQueue);

    printf(
        "  videoParse   = %p\n",
        _videoParse);

    printf(
        "  videoSink    = %p (%s)\n",
        _videoSink,
        _chosenVideoSink.c_str());

    printf(
        "  audioQueue   = %p\n",
        _audioQueue);

    printf(
        "  audioParse   = %p\n",
        _audioParse);

    printf(
        "  audioDecoder = %p (%s)\n",
        _audioDecoder,
        _chosenAudioDecoder.c_str());

    printf(
        "  audioConvert = %p\n",
        _audioConvert);

    printf(
        "  audioResamp  = %p\n",
        _audioResample);

    printf(
        "  audioQueue2  = %p\n",
        _audioQueue2);

    printf(
        "  audioCaps    = %p\n",
        _audioCaps);

    printf(
        "  audioSink    = %p\n",
        _audioSink);


    /*
     * -------------------------------------------------
     * VALIDATE
     * -------------------------------------------------
     */

    if (_pipeline == nullptr ||
        _source == nullptr ||
        _tsParse == nullptr ||
        _demux == nullptr ||
        _videoQueue == nullptr ||
        _videoParse == nullptr ||
        _videoSink == nullptr ||
        _audioQueue == nullptr ||
        _audioParse == nullptr ||
        _audioDecoder == nullptr ||
        _audioConvert == nullptr ||
        _audioResample == nullptr ||
        _audioQueue2 == nullptr ||
        _audioCaps == nullptr ||
        _audioSink == nullptr) {

        if (_onError) {

            _onError(
                kErrPipelineBuild,
                "Failed to create one or more "
                "VBO/GStreamer elements");
        }

        Cleanup();

        SetState(State::Error);

        return false;
    }


    /*
     * -------------------------------------------------
     * CONFIGURE FEIP SOURCE
     * -------------------------------------------------
     *
     * These properties are implemented by
     * gstfeiptsrc.c.
     *
     * gstfeiptsrc handles:
     *
     *     feip_init()
     *     feip_connect()
     *     feip_set_parameters()
     *     feip_start_ts_inject()
     *
     * when the source goes to PLAYING.
     */

    g_object_set(
        G_OBJECT(_source),

        "feip-index",
        0,

        "dmx-id",
        0,

        "dst-ip",
        _host.c_str(),

        "dst-port",
        _port,

        "burst-packets",
        1024,

        nullptr);


    /*
     * -------------------------------------------------
     * TSPARSE
     * -------------------------------------------------
     */

    if (g_object_class_find_property(
            G_OBJECT_GET_CLASS(_tsParse),
            "set-timestamps") != nullptr) {

        g_object_set(
            G_OBJECT(_tsParse),
            "set-timestamps",
            TRUE,
            nullptr);
    }


    /*
     * -------------------------------------------------
     * VIDEO SINK GEOMETRY
     * -------------------------------------------------
     */

    _rectProperty = nullptr;

    if (_videoSink != nullptr) {

        GObjectClass* klass =
            G_OBJECT_GET_CLASS(
                _videoSink);

        if (g_object_class_find_property(
                klass,
                "rectangle") != nullptr) {

            _rectProperty =
                "rectangle";

        } else if (
            g_object_class_find_property(
                klass,
                "window-set") != nullptr) {

            _rectProperty =
                "window-set";
        }
    }

    if (_rectW > 0 &&
        _rectH > 0) {

        ApplyRectangleToSink();
    }


    /*
     * -------------------------------------------------
     * VIDEO QUEUE
     * -------------------------------------------------
     */

    g_object_set(
        G_OBJECT(_videoQueue),

        "leaky",
        0,

        "max-size-buffers",
        2000,

        "max-size-bytes",
        0,

        "max-size-time",
        0,

        nullptr);


    /*
     * -------------------------------------------------
     * AUDIO QUEUES
     * -------------------------------------------------
     */

    g_object_set(
        G_OBJECT(_audioQueue),

        "leaky",
        0,

        "max-size-buffers",
        8000,

        "max-size-bytes",
        0,

        "max-size-time",
        0,

        nullptr);

    g_object_set(
        G_OBJECT(_audioQueue2),

        "leaky",
        0,

        "max-size-buffers",
        8000,

        "max-size-bytes",
        0,

        "max-size-time",
        0,

        nullptr);


    /*
     * -------------------------------------------------
     * H264 PARSER
     * -------------------------------------------------
     */

    g_object_set(
        G_OBJECT(_videoParse),

        "config-interval",
        1,

        "disable-passthrough",
        TRUE,

        nullptr);


    /*
     * -------------------------------------------------
     * AUDIO CAPS
     * -------------------------------------------------
     */

    GstCaps* audioCaps =
        gst_caps_from_string(
            "audio/x-raw,"
            "channels=2,"
            "format=S16LE,"
            "rate=48000");

    g_object_set(
        G_OBJECT(_audioCaps),
        "caps",
        audioCaps,
        nullptr);

    gst_caps_unref(audioCaps);


    /*
     * -------------------------------------------------
     * ALSA
     * -------------------------------------------------
     */

    g_object_set(
        G_OBJECT(_audioSink),
        "device",
        "hw:1,0",
        nullptr);

    GObjectClass* audioSinkClass =
        G_OBJECT_GET_CLASS(
            _audioSink);

    if (g_object_class_find_property(
            audioSinkClass,
            "sync") != nullptr) {

        g_object_set(
            G_OBJECT(_audioSink),
            "sync",
            FALSE,
            nullptr);
    }

    if (g_object_class_find_property(
            audioSinkClass,
            "async") != nullptr) {

        g_object_set(
            G_OBJECT(_audioSink),
            "async",
            FALSE,
            nullptr);
    }

    if (g_object_class_find_property(
            audioSinkClass,
            "enable-last-sample") != nullptr) {

        g_object_set(
            G_OBJECT(_audioSink),
            "enable-last-sample",
            FALSE,
            nullptr);
    }


    /*
     * -------------------------------------------------
     * VIDEO SINK
     * -------------------------------------------------
     */

    if (_videoSink != nullptr) {

        GObjectClass* videoSinkClass =
            G_OBJECT_GET_CLASS(
                _videoSink);

        if (g_object_class_find_property(
                videoSinkClass,
                "sync") != nullptr) {

            g_object_set(
                G_OBJECT(_videoSink),
                "sync",
                TRUE,
                nullptr);
        }

        if (g_object_class_find_property(
                videoSinkClass,
                "async") != nullptr) {

            g_object_set(
                G_OBJECT(_videoSink),
                "async",
                FALSE,
                nullptr);
        }

        if (g_object_class_find_property(
                videoSinkClass,
                "enable-last-sample") != nullptr) {

            g_object_set(
                G_OBJECT(_videoSink),
                "enable-last-sample",
                FALSE,
                nullptr);
        }

        if (g_object_class_find_property(
                videoSinkClass,
                "zorder") != nullptr) {

            g_object_set(
                G_OBJECT(_videoSink),
                "zorder",
                1.0f,
                nullptr);
        }

        if (g_object_class_find_property(
                videoSinkClass,
                "opacity") != nullptr) {

            g_object_set(
                G_OBJECT(_videoSink),
                "opacity",
                1.0f,
                nullptr);
        }
    }


    /*
     * -------------------------------------------------
     * ADD ELEMENTS
     * -------------------------------------------------
     */

    gst_bin_add_many(
        GST_BIN(_pipeline),

        _source,
        _tsParse,
        _demux,

        _videoQueue,
        _videoParse,
        _videoSink,

        _audioQueue,
        _audioParse,
        _audioDecoder,
        _audioConvert,
        _audioResample,
        _audioQueue2,
        _audioCaps,
        _audioSink,

        nullptr);


    /*
     * -------------------------------------------------
     * FEIP SOURCE -> TSPARSE -> TSDEMUX
     * -------------------------------------------------
     */

    gboolean linked =
        gst_element_link_many(
            _source,
            _tsParse,
            _demux,
            nullptr);


    /*
     * -------------------------------------------------
     * VIDEO STATIC LINK
     * -------------------------------------------------
     */

    linked =
        linked &&
        gst_element_link_many(
            _videoQueue,
            _videoParse,
            _videoSink,
            nullptr);


    /*
     * -------------------------------------------------
     * AUDIO STATIC LINK
     * -------------------------------------------------
     */

    linked =
        linked &&
        gst_element_link_many(
            _audioQueue,
            _audioParse,
            _audioDecoder,
            _audioConvert,
            _audioResample,
            _audioQueue2,
            _audioCaps,
            _audioSink,
            nullptr);


    if (!linked) {

        printf(
            "MulticastPlayer: "
            "STATIC LINK FAILED\n");

        if (_onError) {

            _onError(
                kErrPipelineBuild,
                "Failed to link "
                "VBO/GStreamer pipeline");
        }

        Cleanup();

        SetState(State::Error);

        return false;
    }

    printf(
        "MulticastPlayer: VBO source and "
        "video/audio links successful\n");


    /*
     * -------------------------------------------------
     * DYNAMIC TSDEMUX PADS
     * -------------------------------------------------
     */

    g_signal_connect(
        _demux,
        "pad-added",
        G_CALLBACK(
            GstMulticastPipeline::OnDemuxPadAdded),
        this);


    /*
     * -------------------------------------------------
     * BUS
     * -------------------------------------------------
     */

    GstBus* bus =
        gst_element_get_bus(
            _pipeline);

    _busWatchId =
        gst_bus_add_watch(
            bus,
            &GstMulticastPipeline::BusCallback,
            this);

    gst_object_unref(bus);


    SetState(State::Opened);

    printf(
        "MulticastPlayer: VBO pipeline "
        "opened successfully\n");

    return true;
}


/*
 * -----------------------------------------------------
 * TUNE
 * -----------------------------------------------------
 *
 * Change the VBO channel while the pipeline is alive.
 *
 * gstfeiptsrc already handles the actual FEIP
 * reconnect when dst-ip/dst-port changes.
 */
bool GstMulticastPipeline::Tune(
    const std::string& ip,
    int port)
{
    std::lock_guard<std::mutex> guard(_lock);

    if (_source == nullptr) {

        printf(
            "MulticastPlayer: Tune failed - "
            "source is not available\n");

        return false;
    }

    if (ip.empty() ||
        port <= 0 ||
        port > 65535) {

        printf(
            "MulticastPlayer: invalid tune "
            "parameters: %s:%d\n",
            ip.c_str(),
            port);

        return false;
    }


    printf(
        "==================================================\n");

    printf(
        "MulticastPlayer: RETUNE\n");

    printf(
        "Old stream : %s:%d\n",
        _host.c_str(),
        _port);

    printf(
        "New stream : %s:%d\n",
        ip.c_str(),
        port);

    printf(
        "==================================================\n");


    /*
     * Changing these properties invokes the
     * existing gstfeiptsrc property setters.
     *
     * When the source is running, those setters
     * perform the FEIP reconnect.
     */

    g_object_set(
        G_OBJECT(_source),
        "dst-ip",
        ip.c_str(),
        nullptr);


    g_object_set(
        G_OBJECT(_source),
        "dst-port",
        port,
        nullptr);


    _host = ip;
    _port = port;


    printf(
        "MulticastPlayer: "
        "FEIP retune requested\n");

    return true;
}


/*
 * -----------------------------------------------------
 * PLAY
 * -----------------------------------------------------
 */
bool GstMulticastPipeline::Play()
{
    std::lock_guard<std::mutex> guard(_lock);

    if (_pipeline == nullptr) {

        if (_onError) {

            _onError(
                kErrStateChange,
                "Play requested with "
                "no open pipeline");
        }

        return false;
    }

    printf(
        "MulticastPlayer: "
        "setting pipeline PLAYING\n");

    const GstStateChangeReturn ret =
        gst_element_set_state(
            _pipeline,
            GST_STATE_PLAYING);

    if (ret == GST_STATE_CHANGE_FAILURE) {

        if (_onError) {

            _onError(
                kErrStateChange,
                "Failed to set pipeline "
                "to PLAYING");
        }

        SetState(State::Error);

        return false;
    }

    SetState(State::Playing);

    return true;
}


/*
 * -----------------------------------------------------
 * STOP
 * -----------------------------------------------------
 */
bool GstMulticastPipeline::Stop()
{
    std::lock_guard<std::mutex> guard(_lock);

    if (_pipeline == nullptr) {
        return false;
    }

    printf(
        "MulticastPlayer: "
        "stopping pipeline\n");

    gst_element_set_state(
        _pipeline,
        GST_STATE_NULL);

    SetState(State::Stopped);

    return true;
}


/*
 * -----------------------------------------------------
 * CLOSE
 * -----------------------------------------------------
 */
void GstMulticastPipeline::Close()
{
    std::lock_guard<std::mutex> guard(_lock);

    Cleanup();

    SetState(State::Idle);
}


/*
 * -----------------------------------------------------
 * VIDEO RECTANGLE
 * -----------------------------------------------------
 */
bool GstMulticastPipeline::SetVideoRectangle(
    int x,
    int y,
    int width,
    int height)
{
    std::lock_guard<std::mutex> guard(_lock);

    _rectX = x;
    _rectY = y;
    _rectW = width;
    _rectH = height;

    return ApplyRectangleToSink();
}


/*
 * -----------------------------------------------------
 * GET STATE
 * -----------------------------------------------------
 */
GstMulticastPipeline::State
GstMulticastPipeline::GetState() const
{
    std::lock_guard<std::mutex> guard(_lock);

    return _state;
}


/*
 * -----------------------------------------------------
 * GST BUS CALLBACK
 * -----------------------------------------------------
 */
gboolean GstMulticastPipeline::BusCallback(
    GstBus* /*bus*/,
    GstMessage* message,
    gpointer user_data)
{
    auto* self =
        static_cast<GstMulticastPipeline*>(
            user_data);

    if (self == nullptr ||
        message == nullptr) {

        return TRUE;
    }

    switch (GST_MESSAGE_TYPE(message)) {

    case GST_MESSAGE_ERROR:
    {
        GError* err = nullptr;
        gchar* debug = nullptr;

        gst_message_parse_error(
            message,
            &err,
            &debug);

        printf(
            "MulticastPlayer "
            "GStreamer ERROR:\n");

        if (err != nullptr) {

            printf(
                "  Error : %s\n",
                err->message);
        }

        if (debug != nullptr) {

            printf(
                "  Debug : %s\n",
                debug);
        }

        if (self->_onError) {

            self->_onError(
                kErrStream,
                err != nullptr
                    ? err->message
                    : "stream error");
        }

        self->SetState(
            State::Error);

        if (err != nullptr) {
            g_error_free(err);
        }

        g_free(debug);

        break;
    }


    case GST_MESSAGE_EOS:

        printf(
            "MulticastPlayer: EOS\n");

        if (self->_onEos) {
            self->_onEos();
        }

        self->SetState(
            State::Stopped);

        break;


    default:
        break;
    }

    return TRUE;
}


/*
 * -----------------------------------------------------
 * STATE
 * -----------------------------------------------------
 */
void GstMulticastPipeline::SetState(
    State state)
{
    _state = state;

    if (_onStatus) {
        _onStatus(state);
    }
}


/*
 * -----------------------------------------------------
 * CLEANUP
 * -----------------------------------------------------
 */
void GstMulticastPipeline::Cleanup()
{
    printf(
        "MulticastPlayer: Cleanup()\n");

    if (_busWatchId != 0) {

        g_source_remove(
            _busWatchId);

        _busWatchId = 0;
    }


    if (_pipeline != nullptr) {

        gst_element_set_state(
            _pipeline,
            GST_STATE_NULL);

        gst_object_unref(
            _pipeline);

        _pipeline = nullptr;
    }


    /*
     * These elements are owned by the pipeline.
     * Do not unref them individually.
     */

    _source =
        nullptr;

    _tsParse =
        nullptr;

    _depay =
        nullptr;

    _demux =
        nullptr;


    _videoQueue =
        nullptr;

    _videoParse =
        nullptr;

    _videoDecoder =
        nullptr;

    _videoSink =
        nullptr;


    _audioQueue =
        nullptr;

    _audioParse =
        nullptr;

    _audioDecoder =
        nullptr;

    _audioConvert =
        nullptr;

    _audioResample =
        nullptr;

    _audioQueue2 =
        nullptr;

    _audioCaps =
        nullptr;

    _audioSink =
        nullptr;


    _rectProperty =
        nullptr;


    _chosenVideoDecoder.clear();

    _chosenAudioDecoder.clear();

    _chosenVideoSink.clear();


    printf(
        "MulticastPlayer: "
        "Cleanup complete\n");
}

} // namespace Plugin
} // namespace WPEFramework
