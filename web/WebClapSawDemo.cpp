#include "WebClapSawDemo.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <cmath>
#include <fstream>
#include <iterator>
#include <vector>

namespace sst::clap_saw_demo
{
namespace
{

constexpr uint8_t readyCommand = 1;
constexpr uint8_t beginEditCommand = 9;
constexpr uint8_t setValueCommand = 10;
constexpr uint8_t endEditCommand = 11;
constexpr uint8_t parameterChangedCommand = 12;
constexpr uint8_t statusChangedCommand = 13;

constexpr std::array<clap_id, ClapSawDemo::nParams> parameterIds {
    ClapSawDemo::pmUnisonCount,
    ClapSawDemo::pmUnisonSpread,
    ClapSawDemo::pmOscDetune,
    ClapSawDemo::pmAmpAttack,
    ClapSawDemo::pmAmpRelease,
    ClapSawDemo::pmAmpIsGate,
    ClapSawDemo::pmPreFilterVCA,
    ClapSawDemo::pmCutoff,
    ClapSawDemo::pmResonance,
    ClapSawDemo::pmFilterMode,
};

std::string resourceRoot;

const char* mediaTypeFor(const std::string& path)
{
    const auto extension = path.substr(path.find_last_of('.') + 1);
    if (extension == "js") return "text/javascript";
    if (extension == "wasm") return "application/wasm";
    if (extension == "txt") return "text/plain";
    return "text/html";
}

} // namespace

struct ClapSawDemoEditor
{
    uint32_t width = 540;
    uint32_t height = 324;
    bool parented = false;
    bool visible = false;
};

WebClapSawDemo::WebClapSawDemo(const clap_host* hostIn)
    : ClapSawDemo(hostIn), host(hostIn)
{
    hostParams = static_cast<const clap_host_params*>(host->get_extension(host, CLAP_EXT_PARAMS));
    hostWebview = static_cast<const clap_host_webview*>(
        host->get_extension(host, CLAP_EXT_WEBVIEW));
}

clap_process_status WebClapSawDemo::process(const clap_process* processIn) noexcept
{
    const auto status = ClapSawDemo::process(processIn);
    if (toUiQ.size_approx() != 0
        || dataCopyForUI.updateCount.load(std::memory_order_relaxed) != lastUiUpdate.load())
        requestUiUpdate();
    return status;
}

void WebClapSawDemo::paramsFlush(const clap_input_events* in, const clap_output_events* out) noexcept
{
    ClapSawDemo::paramsFlush(in, out);
    if (!dataCopyForUI.isProcessing.load()) sendAllPending.store(true);
    requestUiUpdate();
}

bool WebClapSawDemo::stateLoad(const clap_istream* stream) noexcept
{
    if (!ClapSawDemo::stateLoad(stream)) return false;
    sendAllPending.store(true, std::memory_order_release);
    requestUiUpdate();
    return true;
}

void WebClapSawDemo::onMainThread() noexcept
{
    ToUI update;
    while (toUiQ.try_dequeue(update))
        if (update.type == ToUI::PARAM_VALUE) sendParameter(update.id, update.value);

    if (sendAllPending.exchange(false, std::memory_order_acq_rel))
        sendAllParameters();

    const auto updateCount = dataCopyForUI.updateCount.load(std::memory_order_acquire);
    if (updateCount != lastUiUpdate)
    {
        lastUiUpdate = updateCount;
        sendStatus();
    }
}

int32_t WebClapSawDemo::webviewGetUri(char* uri, uint32_t capacity) const noexcept
{
    constexpr char path[] = "/ui/index.html";
    if (uri && capacity) std::snprintf(uri, capacity, "%s", path);
    return sizeof(path);
}

bool WebClapSawDemo::webviewGetResource(const char* path, char* mime, uint32_t mimeCapacity,
                                        const clap_ostream* stream)
{
    if (!path || !stream || std::strstr(path, "..")) return false;
    std::string relative = path;
    while (!relative.empty() && relative.front() == '/') relative.erase(relative.begin());
    if (relative.empty()) return false;

    auto filePath = resourceRoot + "/" + relative;
#if defined(__wasi__)
    if (!filePath.empty() && filePath.front() == '/') filePath.erase(filePath.begin());
#endif
    std::ifstream input(filePath, std::ios::binary);
    if (!input) return false;
    std::vector<uint8_t> bytes(std::istreambuf_iterator<char>(input), {});
    if (mime && mimeCapacity)
        std::snprintf(mime, mimeCapacity, "%s", mediaTypeFor(relative));
    return bytes.empty() || stream->write(stream, bytes.data(), bytes.size())
        == static_cast<int64_t>(bytes.size());
}

bool WebClapSawDemo::webviewReceive(const void* buffer, uint32_t size) const noexcept
{
    if (!buffer || size == 0) return false;
    const auto* bytes = static_cast<const uint8_t*>(buffer);
    if (bytes[0] == readyCommand && size == 1)
        return sendAllParameters() && sendStatus();

    if ((size != 5 && size != 13) || (bytes[0] != beginEditCommand && bytes[0] != setValueCommand
                     && bytes[0] != endEditCommand))
        return false;

    clap_id id = CLAP_INVALID_ID;
    std::memcpy(&id, bytes + 1, sizeof(id));
    if (!isValidParamId(id)) return false;

    if (bytes[0] != setValueCommand && size != 5) return false;

    FromUI update {};
    update.id = id;
    update.type = bytes[0] == beginEditCommand ? FromUI::BEGIN_EDIT : FromUI::END_EDIT;
    if (bytes[0] == setValueCommand)
    {
        if (size != 13) return false;
        update.type = FromUI::ADJUST_VALUE;
        std::memcpy(&update.value, bytes + 5, sizeof(update.value));
        if (!std::isfinite(update.value)) return false;
    }

    auto& self = const_cast<WebClapSawDemo&>(*this);
    if (!self.fromUiQ.try_enqueue(update)) return false;
    if (hostParams) hostParams->request_flush(host);
    return hostParams != nullptr;
}

bool WebClapSawDemo::sendParameter(clap_id id, double value) const noexcept
{
    std::array<uint8_t, 13> message {};
    message[0] = parameterChangedCommand;
    std::memcpy(message.data() + 1, &id, sizeof(id));
    std::memcpy(message.data() + 5, &value, sizeof(value));
    return hostWebview && hostWebview->send(host, message.data(), message.size());
}

bool WebClapSawDemo::sendStatus() const noexcept
{
    std::array<uint8_t, 9> message {};
    message[0] = statusChangedCommand;
    const auto polyphony = dataCopyForUI.polyphony.load(std::memory_order_relaxed);
    const auto processing = static_cast<int32_t>(
        dataCopyForUI.isProcessing.load(std::memory_order_relaxed));
    std::memcpy(message.data() + 1, &polyphony, sizeof(polyphony));
    std::memcpy(message.data() + 5, &processing, sizeof(processing));
    return hostWebview && hostWebview->send(host, message.data(), message.size());
}

bool WebClapSawDemo::sendAllParameters() const noexcept
{
    auto& self = const_cast<WebClapSawDemo&>(*this);
    // Active parameter values belong to the audio thread: ask it for a queued snapshot.
    if (dataCopyForUI.isProcessing.load(std::memory_order_acquire))
    {
        self.refreshUIValues.store(true);
        if (hostParams) hostParams->request_flush(host);
        return hostParams != nullptr;
    }

    bool sent = true;
    for (const auto id : parameterIds)
    {
        double value = 0;
        sent = self.paramsValue(id, &value) && sendParameter(id, value) && sent;
    }
    return sent;
}

void WebClapSawDemo::requestUiUpdate() const noexcept
{
    if (host) host->request_callback(host);
}

void setResourceRoot(const char* path) { resourceRoot = path ? path : ""; }
const std::string& getResourceRoot() { return resourceRoot; }

bool ClapSawDemo::guiIsApiSupported(const char* api, bool isFloating) noexcept
{
    return !isFloating && api && std::strcmp(api, CLAP_WINDOW_API_WEBVIEW) == 0;
}

bool ClapSawDemo::guiGetPreferredApi(const char** api, bool* isFloating) noexcept
{
    if (!api || !isFloating) return false;
    *api = CLAP_WINDOW_API_WEBVIEW;
    *isFloating = false;
    return true;
}

bool ClapSawDemo::guiCreate(const char* api, bool isFloating) noexcept
{
    if (editor || !guiIsApiSupported(api, isFloating)) return false;
    editor = new ClapSawDemoEditor;
    return true;
}

void ClapSawDemo::guiDestroy() noexcept
{
    delete editor;
    editor = nullptr;
}

bool ClapSawDemo::guiSetParent(const clap_window* window) noexcept
{
    if (!editor || !window || !window->api
        || std::strcmp(window->api, CLAP_WINDOW_API_WEBVIEW) != 0)
        return false;
    editor->parented = window->ptr == nullptr;
    return editor->parented;
}

bool ClapSawDemo::guiShow() noexcept
{
    return editor && editor->parented && (editor->visible = true);
}

bool ClapSawDemo::guiHide() noexcept
{
    if (!editor) return false;
    editor->visible = false;
    return true;
}

bool ClapSawDemo::guiSetScale(double) noexcept { return false; }

bool ClapSawDemo::guiGetResizeHints(clap_gui_resize_hints* hints) noexcept
{
    if (!editor || !hints) return false;
    *hints = { true, true, true, 5, 3 };
    return true;
}

bool ClapSawDemo::guiAdjustSize(uint32_t* width, uint32_t* height) noexcept
{
    if (!editor || !width || !height) return false;
    *width = std::clamp(*width, 360u, 960u);
    *height = *width * 3 / 5;
    return true;
}

bool ClapSawDemo::guiSetSize(uint32_t width, uint32_t height) noexcept
{
    if (!editor) return false;
    editor->width = width;
    editor->height = height;
    return true;
}

bool ClapSawDemo::guiGetSize(uint32_t* width, uint32_t* height) noexcept
{
    if (!editor || !width || !height) return false;
    *width = editor->width;
    *height = editor->height;
    return true;
}

} // namespace sst::clap_saw_demo
