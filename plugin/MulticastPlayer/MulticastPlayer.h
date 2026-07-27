/**
 * MulticastPlayer.h
 *
 * Thunder / WPEFramework plugin that exposes a JSON-RPC control surface for the
 * native multicast GStreamer pipeline. The downloadable Lightning widget calls
 * these methods over the Thunder WebSocket; the widget never touches the media
 * path itself.
 *
 * JSON-RPC methods:   open, play, stop, close, setVideoRectangle, status
 * JSON-RPC events:     onStatusChanged, onError, onEnd
 */
#pragma once

#include "Module.h"
#include "GstMulticastPipeline.h"

#include <memory>

namespace WPEFramework {
namespace Plugin {

class MulticastPlayer : public PluginHost::IPlugin, public PluginHost::JSONRPC {
public:
    // Plugin configuration (from the Thunder plugin config JSON). Empty element
    // names keep automatic platform detection; set them to force a factory
    // (e.g. the emulator profile uses avdec_h264 / autovideosink).
    class Config : public Core::JSON::Container {
    public:
        Config(const Config&) = delete;
        Config& operator=(const Config&) = delete;
        Config()
            : Core::JSON::Container()
            , Interface()
            , DefaultTransport()
            , VideoDecoder()
            , AudioDecoder()
            , VideoSink() {
            Add(_T("interface"), &Interface);
            Add(_T("defaulttransport"), &DefaultTransport);
            Add(_T("videoDecoder"), &VideoDecoder);
            Add(_T("audioDecoder"), &AudioDecoder);
            Add(_T("videoSink"), &VideoSink);
        }
        Core::JSON::String Interface;
        Core::JSON::String DefaultTransport;
        Core::JSON::String VideoDecoder;
        Core::JSON::String AudioDecoder;
        Core::JSON::String VideoSink;
    };

    MulticastPlayer();
    ~MulticastPlayer() override;

    MulticastPlayer(const MulticastPlayer&) = delete;
    MulticastPlayer& operator=(const MulticastPlayer&) = delete;

    // PluginHost::IPlugin
    const string Initialize(PluginHost::IShell* service) override;
    void Deinitialize(PluginHost::IShell* service) override;
    string Information() const override;

    BEGIN_INTERFACE_MAP(MulticastPlayer)
        INTERFACE_ENTRY(PluginHost::IPlugin)
        INTERFACE_ENTRY(PluginHost::IDispatcher)
    END_INTERFACE_MAP

private:
    void RegisterAll();
    void UnregisterAll();

    // JSON-RPC endpoint handlers.
    uint32_t endpoint_open(const JsonObject& parameters, JsonObject& response);
    uint32_t endpoint_play(const JsonObject& parameters, JsonObject& response);
    uint32_t endpoint_stop(const JsonObject& parameters, JsonObject& response);
    uint32_t endpoint_close(const JsonObject& parameters, JsonObject& response);
    uint32_t endpoint_setVideoRectangle(const JsonObject& parameters, JsonObject& response);
    uint32_t endpoint_status(const JsonObject& parameters, JsonObject& response);

    // Pipeline callbacks -> JSON-RPC events.
    void OnStatus(GstMulticastPipeline::State state);
    void OnError(int code, const std::string& message);
    void OnEnd();

    static const char* StateToString(GstMulticastPipeline::State state);

    PluginHost::IShell* _service{ nullptr };
    std::unique_ptr<GstMulticastPipeline> _pipeline;
    std::string _defaultInterface;
    std::string _defaultTransport;
};

} // namespace Plugin
} // namespace WPEFramework
