/**
 * MulticastPlayer.cpp
 *
 * JSON-RPC control surface + lifecycle for the
 * native VBO/GStreamer multicast pipeline.
 */

#include "MulticastPlayer.h"

namespace WPEFramework {

namespace {

/*
 * Plugin metadata registration.
 */
static Plugin::Metadata<Plugin::MulticastPlayer> metadata(
    1,
    0,
    0,
    {},
    {},
    {}
);

} // namespace


namespace Plugin {

SERVICE_REGISTRATION(
    MulticastPlayer,
    1,
    0);


MulticastPlayer::MulticastPlayer()
    : _pipeline(
          new GstMulticastPipeline())
{
    RegisterAll();
}


MulticastPlayer::~MulticastPlayer()
{
    UnregisterAll();
}


/*
 * -----------------------------------------------------
 * INITIALIZE
 * -----------------------------------------------------
 */
const string MulticastPlayer::Initialize(
    PluginHost::IShell* service)
{
    ASSERT(service != nullptr);

    _service = service;

    _service->AddRef();


    /*
     * Read plugin configuration.
     */
    Config config;

    config.FromString(
        _service->ConfigLine());


    _defaultInterface =
        config.Interface.Value();

    _defaultTransport =
        config.DefaultTransport.Value();


    /*
     * Configure GStreamer element overrides.
     */
    _pipeline->SetElementProfile(
        config.VideoDecoder.Value(),
        config.AudioDecoder.Value(),
        config.VideoSink.Value());


    /*
     * Pipeline callbacks.
     */
    _pipeline->SetStatusCallback(
        [this](
            GstMulticastPipeline::State state) {

            OnStatus(state);
        });


    _pipeline->SetErrorCallback(
        [this](
            int code,
            const std::string& message) {

            OnError(
                code,
                message);
        });


    _pipeline->SetEosCallback(
        [this]() {

            OnEnd();
        });


    /*
     * Empty string means success.
     */
    return string();
}


/*
 * -----------------------------------------------------
 * DEINITIALIZE
 * -----------------------------------------------------
 */
void MulticastPlayer::Deinitialize(
    PluginHost::IShell* service)
{
    ASSERT(
        _service == service);


    if (_pipeline) {
        _pipeline->Close();
    }


    if (_service != nullptr) {

        _service->Release();

        _service =
            nullptr;
    }
}


/*
 * -----------------------------------------------------
 * INFORMATION
 * -----------------------------------------------------
 */
string MulticastPlayer::Information() const
{
    return string(
        "RDKV Nokia VBO IP-multicast player "
        "(native GStreamer pipeline)");
}


/*
 * -----------------------------------------------------
 * REGISTER JSON-RPC METHODS
 * -----------------------------------------------------
 */
void MulticastPlayer::RegisterAll()
{
    Register(
        "open",
        &MulticastPlayer::endpoint_open,
        this);


    Register(
        "play",
        &MulticastPlayer::endpoint_play,
        this);


    Register(
        "stop",
        &MulticastPlayer::endpoint_stop,
        this);


    Register(
        "close",
        &MulticastPlayer::endpoint_close,
        this);


    /*
     * NEW:
     *
     * Dynamically change the VBO channel.
     */
    Register(
        "tune",
        &MulticastPlayer::endpoint_tune,
        this);


    Register(
        "setVideoRectangle",
        &MulticastPlayer::endpoint_setVideoRectangle,
        this);


    Register(
        "status",
        &MulticastPlayer::endpoint_status,
        this);
}


/*
 * -----------------------------------------------------
 * UNREGISTER JSON-RPC METHODS
 * -----------------------------------------------------
 */
void MulticastPlayer::UnregisterAll()
{
    Unregister("open");

    Unregister("play");

    Unregister("stop");

    Unregister("close");

    Unregister("tune");

    Unregister("setVideoRectangle");

    Unregister("status");
}


/*
 * -----------------------------------------------------
 * OPEN
 * -----------------------------------------------------
 *
 * Example:
 *
 * {
 *     "uri": "udp://239.100.0.1:8433"
 * }
 *
 * The IP/port are passed to feiptsrc.
 */
uint32_t MulticastPlayer::endpoint_open(
    const JsonObject& parameters,
    JsonObject& response)
{
    if (parameters.HasLabel("uri") == false) {

        response["success"] =
            false;

        response["message"] =
            "missing required parameter: uri";

        return Core::ERROR_BAD_REQUEST;
    }


    const string uri =
        parameters["uri"].String();


    const string iface =
        parameters.HasLabel("interface")
            ? parameters["interface"].String()
            : _defaultInterface;


    /*
     * Retained for API compatibility.
     *
     * The VBO implementation internally uses
     * feiptsrc, regardless of UDP/RTP.
     */
    GstMulticastPipeline::Transport transport =
        GstMulticastPipeline::Transport::Auto;


    string t;

    if (parameters.HasLabel("transport")) {

        t =
            parameters["transport"].String();

    } else {

        t =
            _defaultTransport;
    }


    if (t == "udp") {

        transport =
            GstMulticastPipeline::Transport::Udp;

    } else if (t == "rtp") {

        transport =
            GstMulticastPipeline::Transport::Rtp;
    }


    const bool ok =
        _pipeline->Open(
            uri,
            transport,
            iface);


    response["success"] =
        ok;


    if (!ok) {

        response["message"] =
            "Failed to open "
            "VBO/GStreamer pipeline";
    }


    printf(
        "OPEN RESULT=%d\n",
        ok);


    return ok
        ? Core::ERROR_NONE
        : Core::ERROR_GENERAL;
}


/*
 * -----------------------------------------------------
 * PLAY
 * -----------------------------------------------------
 */
uint32_t MulticastPlayer::endpoint_play(
    const JsonObject& /*parameters*/,
    JsonObject& response)
{
    const bool ok =
        _pipeline->Play();


    response["success"] =
        ok;


    if (!ok) {

        response["message"] =
            "Failed to start "
            "VBO playback";
    }


    return ok
        ? Core::ERROR_NONE
        : Core::ERROR_GENERAL;
}


/*
 * -----------------------------------------------------
 * STOP
 * -----------------------------------------------------
 */
uint32_t MulticastPlayer::endpoint_stop(
    const JsonObject& /*parameters*/,
    JsonObject& response)
{
    const bool ok =
        _pipeline->Stop();


    response["success"] =
        ok;


    return ok
        ? Core::ERROR_NONE
        : Core::ERROR_GENERAL;
}


/*
 * -----------------------------------------------------
 * CLOSE
 * -----------------------------------------------------
 */
uint32_t MulticastPlayer::endpoint_close(
    const JsonObject& /*parameters*/,
    JsonObject& response)
{
    _pipeline->Close();


    response["success"] =
        true;


    return Core::ERROR_NONE;
}


/*
 * -----------------------------------------------------
 * TUNE
 * -----------------------------------------------------
 *
 * Dynamically change VBO channel.
 *
 * Request:
 *
 * {
 *     "ip": "239.100.0.2",
 *     "port": 8433
 * }
 *
 * No pipeline recreation is required.
 *
 * The feiptsrc property setters invoke the existing
 * FEIP reconnect logic.
 */
uint32_t MulticastPlayer::endpoint_tune(
    const JsonObject& parameters,
    JsonObject& response)
{
    if (!parameters.HasLabel("ip") ||
        !parameters.HasLabel("port")) {

        response["success"] =
            false;

        response["message"] =
            "ip and port are required";

        return Core::ERROR_BAD_REQUEST;
    }


    const string ip =
        parameters["ip"].String();


    const int port =
        parameters["port"].Number();


    if (ip.empty() ||
        port <= 0 ||
        port > 65535) {

        response["success"] =
            false;

        response["message"] =
            "Invalid IP address or port";

        return Core::ERROR_BAD_REQUEST;
    }


    printf(
        "MulticastPlayer: "
        "Tune requested: %s:%d\n",
        ip.c_str(),
        port);


    const bool ok =
        _pipeline->Tune(
            ip,
            port);


    response["success"] =
        ok;


    if (!ok) {

        response["message"] =
            "Failed to retune "
            "VBO source";
    }


    return ok
        ? Core::ERROR_NONE
        : Core::ERROR_GENERAL;
}


/*
 * -----------------------------------------------------
 * VIDEO RECTANGLE
 * -----------------------------------------------------
 */
uint32_t MulticastPlayer::endpoint_setVideoRectangle(
    const JsonObject& parameters,
    JsonObject& response)
{
    const int x =
        parameters.HasLabel("x")
            ? parameters["x"].Number()
            : 0;


    const int y =
        parameters.HasLabel("y")
            ? parameters["y"].Number()
            : 0;


    const int w =
        parameters.HasLabel("w")
            ? parameters["w"].Number()
            : 0;


    const int h =
        parameters.HasLabel("h")
            ? parameters["h"].Number()
            : 0;


    if (w <= 0 ||
        h <= 0) {

        response["success"] =
            false;

        response["message"] =
            "w and h must be positive";

        return Core::ERROR_BAD_REQUEST;
    }


    const bool ok =
        _pipeline->SetVideoRectangle(
            x,
            y,
            w,
            h);


    response["success"] =
        ok;


    return ok
        ? Core::ERROR_NONE
        : Core::ERROR_GENERAL;
}


/*
 * -----------------------------------------------------
 * STATUS
 * -----------------------------------------------------
 */
uint32_t MulticastPlayer::endpoint_status(
    const JsonObject& /*parameters*/,
    JsonObject& response)
{
    response["state"] =
        StateToString(
            _pipeline->GetState());


    response["success"] =
        true;


    return Core::ERROR_NONE;
}


/*
 * -----------------------------------------------------
 * STATUS CALLBACK
 * -----------------------------------------------------
 */
void MulticastPlayer::OnStatus(
    GstMulticastPipeline::State state)
{
    JsonObject params;

    params["state"] =
        StateToString(state);


    Notify(
        "onStatusChanged",
        params);
}


/*
 * -----------------------------------------------------
 * ERROR CALLBACK
 * -----------------------------------------------------
 */
void MulticastPlayer::OnError(
    int code,
    const std::string& message)
{
    JsonObject params;

    params["code"] =
        code;

    params["message"] =
        message;


    Notify(
        "onError",
        params);
}


/*
 * -----------------------------------------------------
 * EOS CALLBACK
 * -----------------------------------------------------
 */
void MulticastPlayer::OnEnd()
{
    JsonObject params;

    Notify(
        "onEnd",
        params);
}


/*
 * -----------------------------------------------------
 * STATE TO STRING
 * -----------------------------------------------------
 */
const char* MulticastPlayer::StateToString(
    GstMulticastPipeline::State state)
{
    switch (state) {

    case GstMulticastPipeline::State::Idle:
        return "IDLE";

    case GstMulticastPipeline::State::Opened:
        return "OPENED";

    case GstMulticastPipeline::State::Playing:
        return "PLAYING";

    case GstMulticastPipeline::State::Stopped:
        return "STOPPED";

    case GstMulticastPipeline::State::Error:
        return "ERROR";

    default:
        return "UNKNOWN";
    }
}


} // namespace Plugin
} // namespace WPEFramework
