#pragma once

#include "clap-saw-demo.h"

#include <clap/ext/draft/webview.h>

#include <array>
#include <atomic>
#include <string>

namespace sst::clap_saw_demo
{

class WebClapSawDemo final : public ClapSawDemo
{
public:
    explicit WebClapSawDemo(const clap_host* host);

    clap_process_status process(const clap_process* process) noexcept override;
    void paramsFlush(const clap_input_events*, const clap_output_events*) noexcept override;
    bool stateLoad(const clap_istream* stream) noexcept override;
    void onMainThread() noexcept override;

protected:
    bool enableDraftExtensions() const noexcept override { return true; }
    bool implementsWebview() const noexcept override { return true; }
    int32_t webviewGetUri(char* uri, uint32_t capacity) const noexcept override;
    bool webviewGetResource(const char* path, char* mime, uint32_t mimeCapacity,
                            const clap_ostream* stream) override;
    bool webviewReceive(const void* buffer, uint32_t size) const noexcept override;

private:
    bool sendParameter(clap_id id, double value) const noexcept;
    bool sendStatus() const noexcept;
    bool sendAllParameters() const noexcept;
    void requestUiUpdate() const noexcept;

    const clap_host* host = nullptr;
    const clap_host_params* hostParams = nullptr;
    const clap_host_webview* hostWebview = nullptr;
    mutable std::atomic<bool> sendAllPending { false };
    mutable std::atomic<uint32_t> lastUiUpdate { 0 };
};

void setResourceRoot(const char* path);
const std::string& getResourceRoot();

} // namespace sst::clap_saw_demo
