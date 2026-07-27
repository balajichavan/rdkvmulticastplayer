/**
 * GstMulticastPipeline.cpp
 *
 * Implementation of the native multicast GStreamer pipeline used by the
 * MulticastPlayer Thunder plugin.
 *
 * Pipeline (UDP-TS):
 *   udpsrc address=<group> port=<port> multicast-iface=<iface>
 *          caps="video/mpegts, systemstream=true"
 *     ! tsdemux name=demux
 *        demux. ! queue ! h264parse ! brcmvideodecoder ! westerossink
 *        demux. ! queue ! aacparse   ! brcmaudiodecoder ! amlhalasink/audiosink
 *
 * Pipeline (RTP-TS): identical but with rtpmp2tdepay between udpsrc and tsdemux
 * and RTP caps on udpsrc.
 *
 * Element names assume a Broadcom SoC. Swap brcm*/westerossink for the target
 * platform's sinks if building for Amlogic/Realtek.
 */
#include "GstMulticastPipeline.h"

#include <cstdio>
#include <cstring>

namespace WPEFramework {
namespace Plugin {

namespace {
constexpr int kErrPipelineBuild = 1001;
constexpr int kErrStateChange = 1002;
constexpr int kErrStream = 1003;

// Link a freshly created tsdemux pad to the decode branch that is waiting for it.
void OnDemuxPadAdded(GstElement* /*demux*/, GstPad* pad, gpointer user_data) {
    GstElement* target = static_cast<GstElement*>(user_data);
    GstPad* sinkPad = gst_element_get_static_pad(target, "sink");
    if (sinkPad != nullptr) {
        if (gst_pad_is_linked(sinkPad) == FALSE) {
            gst_pad_link(pad, sinkPad);
        }
        gst_object_unref(sinkPad);
    }
}
} // namespace

GstMulticastPipeline::GstMulticastPipeline() {
    if (gst_is_initialized() == FALSE) {
        gst_init(nullptr, nullptr);
    }
}

GstMulticastPipeline::~GstMulticastPipeline() {
    Close();
}

bool GstMulticastPipeline::ParseUri(const std::string& uri, std::string& host, int& port) const {
    // Accept "udp://host:port" or "rtp://host:port" or bare "host:port".
    std::string work = uri;
    const auto scheme = work.find("://");
    if (scheme != std::string::npos) {
        work = work.substr(scheme + 3);
    }
    const auto colon = work.rfind(':');
    if (colon == std::string::npos) {
        return false;
    }
    host = work.substr(0, colon);
    port = std::atoi(work.substr(colon + 1).c_str());
    return (!host.empty() && port > 0 && port <= 65535);
}

bool GstMulticastPipeline::Open(const std::string& uri, Transport transport, const std::string& iface) {
    std::lock_guard<std::mutex> guard(_lock);

    if (_pipeline != nullptr) {
        Cleanup();
    }

    if (ParseUri(uri, _host, _port) == false) {
        if (_onError) {
            _onError(kErrPipelineBuild, "Invalid multicast URI: " + uri);
        }
        SetState(State::Error);
        return false;
    }
    _iface = iface;

    Transport effective = transport;
    if (effective == Transport::Auto) {
        effective = (uri.rfind("rtp://", 0) == 0) ? Transport::Rtp : Transport::Udp;
    }

    _pipeline = gst_pipeline_new("multicast-player");
    _source = gst_element_factory_make("udpsrc", "src");
    GstElement* depay = (effective == Transport::Rtp)
        ? gst_element_factory_make("rtpmp2tdepay", "depay")
        : nullptr;
    GstElement* demux = gst_element_factory_make("tsdemux", "demux");

    GstElement* vqueue = gst_element_factory_make("queue", "vqueue");
    GstElement* vparse = gst_element_factory_make("h264parse", "vparse");
    GstElement* vdec = gst_element_factory_make("brcmvideodecoder", "vdec");
    _videoSink = gst_element_factory_make("westerossink", "vsink");

    GstElement* aqueue = gst_element_factory_make("queue", "aqueue");
    GstElement* aparse = gst_element_factory_make("aacparse", "aparse");
    GstElement* adec = gst_element_factory_make("brcmaudiodecoder", "adec");
    GstElement* asink = gst_element_factory_make("amlhalasink", "asink");
    if (asink == nullptr) {
        asink = gst_element_factory_make("autoaudiosink", "asink");
    }

    if (_pipeline == nullptr || _source == nullptr || demux == nullptr ||
        vqueue == nullptr || vdec == nullptr || _videoSink == nullptr) {
        if (_onError) {
            _onError(kErrPipelineBuild, "Failed to create one or more GStreamer elements");
        }
        Cleanup();
        SetState(State::Error);
        return false;
    }

    // Configure the multicast source.
    g_object_set(G_OBJECT(_source),
        "address", _host.c_str(),
        "port", _port,
        "auto-multicast", TRUE,      // join on READY->PAUSED, leave on PAUSED->READY
        "reuse", TRUE,
        nullptr);
    if (!_iface.empty()) {
        g_object_set(G_OBJECT(_source), "multicast-iface", _iface.c_str(), nullptr);
    }

    if (effective == Transport::Rtp) {
        GstCaps* caps = gst_caps_new_simple("application/x-rtp",
            "media", G_TYPE_STRING, "video",
            "clock-rate", G_TYPE_INT, 90000,
            "encoding-name", G_TYPE_STRING, "MP2T",
            nullptr);
        g_object_set(G_OBJECT(_source), "caps", caps, nullptr);
        gst_caps_unref(caps);
    } else {
        GstCaps* caps = gst_caps_new_simple("video/mpegts",
            "systemstream", G_TYPE_BOOLEAN, TRUE,
            nullptr);
        g_object_set(G_OBJECT(_source), "caps", caps, nullptr);
        gst_caps_unref(caps);
    }

    // Apply any previously requested rectangle to westerossink.
    if (_rectW > 0 && _rectH > 0) {
        gchar rect[64];
        std::snprintf(rect, sizeof(rect), "%d,%d,%d,%d", _rectX, _rectY, _rectW, _rectH);
        g_object_set(G_OBJECT(_videoSink), "window-set", rect, nullptr);
    }

    // Assemble the bin.
    gst_bin_add_many(GST_BIN(_pipeline), _source, demux,
        vqueue, vparse, vdec, _videoSink,
        aqueue, aparse, adec, asink, nullptr);
    if (depay != nullptr) {
        gst_bin_add(GST_BIN(_pipeline), depay);
    }

    // Link the source chain up to tsdemux.
    gboolean linked = TRUE;
    if (depay != nullptr) {
        linked = gst_element_link(_source, depay) && gst_element_link(depay, demux);
    } else {
        linked = gst_element_link(_source, demux);
    }

    // Link the static decode branches; demux src pads are dynamic.
    linked = linked && gst_element_link_many(vqueue, vparse, vdec, _videoSink, nullptr);
    linked = linked && gst_element_link_many(aqueue, aparse, adec, asink, nullptr);
    if (linked == FALSE) {
        if (_onError) {
            _onError(kErrPipelineBuild, "Failed to link static pipeline elements");
        }
        Cleanup();
        SetState(State::Error);
        return false;
    }

    // Hook dynamic pads from tsdemux to the correct branch queue.
    g_signal_connect(demux, "pad-added", G_CALLBACK(+[](GstElement* d, GstPad* pad, gpointer data) {
        auto** branches = static_cast<GstElement**>(data);
        GstCaps* caps = gst_pad_get_current_caps(pad);
        if (caps == nullptr) {
            caps = gst_pad_query_caps(pad, nullptr);
        }
        const gchar* name = (caps != nullptr) ? gst_structure_get_name(gst_caps_get_structure(caps, 0)) : "";
        GstElement* target = nullptr;
        if (name != nullptr && std::strstr(name, "audio") != nullptr) {
            target = branches[1]; // aqueue
        } else {
            target = branches[0]; // vqueue
        }
        if (target != nullptr) {
            OnDemuxPadAdded(d, pad, target);
        }
        if (caps != nullptr) {
            gst_caps_unref(caps);
        }
    }), new GstElement*[2]{ vqueue, aqueue });

    // Watch the bus for state, error and EOS messages.
    GstBus* bus = gst_element_get_bus(_pipeline);
    _busWatchId = gst_bus_add_watch(bus, &GstMulticastPipeline::BusCallback, this);
    gst_object_unref(bus);

    SetState(State::Opened);
    return true;
}

bool GstMulticastPipeline::Play() {
    std::lock_guard<std::mutex> guard(_lock);
    if (_pipeline == nullptr) {
        if (_onError) {
            _onError(kErrStateChange, "Play requested with no open pipeline");
        }
        return false;
    }
    // READY -> PAUSED triggers auto-multicast IGMP join on udpsrc.
    const GstStateChangeReturn ret = gst_element_set_state(_pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        if (_onError) {
            _onError(kErrStateChange, "Failed to set pipeline to PLAYING");
        }
        SetState(State::Error);
        return false;
    }
    SetState(State::Playing);
    return true;
}

bool GstMulticastPipeline::Stop() {
    std::lock_guard<std::mutex> guard(_lock);
    if (_pipeline == nullptr) {
        return false;
    }
    // PAUSED -> READY -> NULL triggers the IGMP leave on udpsrc.
    gst_element_set_state(_pipeline, GST_STATE_NULL);
    SetState(State::Stopped);
    return true;
}

void GstMulticastPipeline::Close() {
    std::lock_guard<std::mutex> guard(_lock);
    Cleanup();
    SetState(State::Idle);
}

bool GstMulticastPipeline::SetVideoRectangle(int x, int y, int width, int height) {
    std::lock_guard<std::mutex> guard(_lock);
    _rectX = x;
    _rectY = y;
    _rectW = width;
    _rectH = height;
    if (_videoSink == nullptr) {
        // Stored; applied when the pipeline is built.
        return true;
    }
    gchar rect[64];
    std::snprintf(rect, sizeof(rect), "%d,%d,%d,%d", x, y, width, height);
    g_object_set(G_OBJECT(_videoSink), "window-set", rect, nullptr);
    return true;
}

GstMulticastPipeline::State GstMulticastPipeline::GetState() const {
    std::lock_guard<std::mutex> guard(_lock);
    return _state;
}

gboolean GstMulticastPipeline::BusCallback(GstBus* /*bus*/, GstMessage* message, gpointer user_data) {
    auto* self = static_cast<GstMulticastPipeline*>(user_data);
    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ERROR: {
        GError* err = nullptr;
        gchar* debug = nullptr;
        gst_message_parse_error(message, &err, &debug);
        if (self->_onError) {
            self->_onError(kErrStream, err != nullptr ? err->message : "stream error");
        }
        self->SetState(State::Error);
        if (err != nullptr) {
            g_error_free(err);
        }
        g_free(debug);
        break;
    }
    case GST_MESSAGE_EOS:
        if (self->_onEos) {
            self->_onEos();
        }
        self->SetState(State::Stopped);
        break;
    default:
        break;
    }
    return TRUE;
}

void GstMulticastPipeline::SetState(State state) {
    _state = state;
    if (_onStatus) {
        _onStatus(state);
    }
}

void GstMulticastPipeline::Cleanup() {
    if (_busWatchId != 0) {
        g_source_remove(_busWatchId);
        _busWatchId = 0;
    }
    if (_pipeline != nullptr) {
        gst_element_set_state(_pipeline, GST_STATE_NULL);
        gst_object_unref(_pipeline);
        _pipeline = nullptr;
    }
    _source = nullptr;
    _videoSink = nullptr;
}

} // namespace Plugin
} // namespace WPEFramework
