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
};

} // namespace Plugin
} // namespace WPEFramework
