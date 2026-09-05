#include <clap/clap.h>
#include <clap/ext/draft/webview.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <string>
#include <vector>
#if defined(__APPLE__)
#import <AppKit/AppKit.h>
#endif

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition); std::abort(); } } while (false)

std::vector<std::vector<uint8_t>> messages;
std::vector<uint16_t> outputTypes;
int flushRequests = 0;
const clap_host_params hostParams {
    [](const clap_host*, uint32_t) {}, [](const clap_host*, clap_id, uint32_t) {},
    [](const clap_host*) { ++flushRequests; }
};
const clap_host_webview hostWebview {
    [](const clap_host*, const void* data, uint32_t size) {
        const auto* bytes = static_cast<const uint8_t*>(data);
        messages.emplace_back(bytes, bytes + size);
        return true;
    }
};
const clap_host host {
    CLAP_VERSION, nullptr, "Saw smoke", "Test", "", "1",
    [](const clap_host*, const char* id) -> const void* {
        if (!std::strcmp(id, CLAP_EXT_PARAMS)) return &hostParams;
        if (!std::strcmp(id, CLAP_EXT_WEBVIEW)) return &hostWebview;
        return nullptr;
    }, [](const clap_host*) {}, [](const clap_host*) {}, [](const clap_host*) {}
};
const clap_input_events emptyEvents {
    nullptr, [](const clap_input_events*) -> uint32_t { return 0; },
    [](const clap_input_events*, uint32_t) -> const clap_event_header* { return nullptr; }
};
const clap_output_events outputEvents {
    nullptr, [](const clap_output_events*, const clap_event_header* event) {
        outputTypes.push_back(event->type); return true;
    }
};

int main(int argc, char** argv)
{
    CHECK(argc == 3);
    auto* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) std::fprintf(stderr, "%s\n", dlerror());
    CHECK(library);
    const auto* entry = static_cast<const clap_plugin_entry*>(dlsym(library, "clap_entry"));
    CHECK(entry && entry->init(argv[2]));
    const auto* factory = static_cast<const clap_plugin_factory*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    CHECK(factory && factory->get_plugin_count(factory) == 1);
    const auto* descriptor = factory->get_plugin_descriptor(factory, 0);
    const auto* plugin = factory->create_plugin(factory, &host, descriptor->id);
    CHECK(plugin && plugin->init(plugin));
    const auto* params = static_cast<const clap_plugin_params*>(plugin->get_extension(plugin, CLAP_EXT_PARAMS));
    const auto* gui = static_cast<const clap_plugin_gui*>(plugin->get_extension(plugin, CLAP_EXT_GUI));
    const auto* web = static_cast<const clap_plugin_webview*>(plugin->get_extension(plugin, CLAP_EXT_WEBVIEW));
    CHECK(params && params->count(plugin) == 10 && gui);
    clap_param_info cutoff {};
    CHECK(params->get_info(plugin, 7, &cutoff) && cutoff.id == 17 && cutoff.default_value == 69);

    if (web)
    {
        std::array<char, 64> uri {}, mime {};
        CHECK(web->get_uri(plugin, uri.data(), uri.size()) == 15
              && !std::strcmp(uri.data(), "/ui/index.html"));
        std::vector<uint8_t> resource;
        const clap_ostream stream { &resource,
            [](const clap_ostream* stream, const void* data, uint64_t size) -> int64_t {
                auto& bytes = *static_cast<std::vector<uint8_t>*>(stream->ctx);
                const auto* first = static_cast<const uint8_t*>(data);
                bytes.insert(bytes.end(), first, first + size);
                return static_cast<int64_t>(size);
            }
        };
        CHECK(web->get_resource(plugin, uri.data(), mime.data(), mime.size(), &stream)
              && !std::strcmp(mime.data(), "text/html") && !resource.empty());
        resource.clear();
        CHECK(web->get_resource(plugin, "/ui/test.wasm", mime.data(), mime.size(), &stream)
              && !std::strcmp(mime.data(), "application/wasm")
              && std::string(resource.begin(), resource.end()) == "wasm resource fixture");
        CHECK(!web->get_resource(plugin, "/../test.wasm", mime.data(), mime.size(), &stream));
        CHECK(gui->create(plugin, CLAP_WINDOW_API_WEBVIEW, false));
        clap_window window {};
        window.api = CLAP_WINDOW_API_WEBVIEW;
        CHECK(gui->set_parent(plugin, &window) && gui->show(plugin));
        uint32_t width = 0, height = 0;
        CHECK(gui->get_size(plugin, &width, &height) && width == 540 && height == 324);
        CHECK(gui->hide(plugin) && gui->show(plugin));
        CHECK(gui->set_size(plugin, 600, 360));
        CHECK(gui->get_size(plugin, &width, &height) && width == 600 && height == 360);
        const uint8_t ready = 1;
        CHECK(web->receive(plugin, &ready, 1) && messages.size() == 11);
        for (const uint8_t command : { 9, 10, 11 })
        {
            std::array<uint8_t, 13> message {};
            message[0] = command;
            std::memcpy(message.data() + 1, &cutoff.id, 4);
            const double value = 72;
            std::memcpy(message.data() + 5, &value, 8);
            CHECK(web->receive(plugin, message.data(), command == 10 ? 13 : 5));
        }
        CHECK(flushRequests == 3);
        params->flush(plugin, &emptyEvents, &outputEvents);
        CHECK((outputTypes == std::vector<uint16_t> { CLAP_EVENT_PARAM_GESTURE_BEGIN,
            CLAP_EVENT_PARAM_VALUE, CLAP_EVENT_PARAM_GESTURE_END }));
        double value = 0;
        CHECK(params->get_value(plugin, cutoff.id, &value) && value == 72);
        plugin->on_main_thread(plugin);
    }
#if defined(__APPLE__)
    NSWindow* nativeWindow = nil;
    if (!web)
    {
        [NSApplication sharedApplication];
        nativeWindow = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 540, 324)
            styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
        CHECK(gui->is_api_supported(plugin, CLAP_WINDOW_API_COCOA, false));
        CHECK(gui->create(plugin, CLAP_WINDOW_API_COCOA, false));
        clap_window window {};
        window.api = CLAP_WINDOW_API_COCOA;
        window.cocoa = (__bridge void*)nativeWindow.contentView;
        CHECK(gui->set_parent(plugin, &window));
        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
    }
#endif
    CHECK(plugin->activate(plugin, 48000, 1, 128) && plugin->start_processing(plugin));
    clap_event_note note {{ sizeof(note), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_NOTE_ON, 0 },
        1, 0, 0, 60, 1.0};
    clap_input_events notes { &note,
        [](const clap_input_events*) -> uint32_t { return 1; },
        [](const clap_input_events* events, uint32_t) -> const clap_event_header* {
            return &static_cast<clap_event_note*>(events->ctx)->header;
        }
    };
    std::array<float, 128> left {}, right {};
    float* channels[] { left.data(), right.data() };
    clap_audio_buffer buffer { channels, nullptr, 2, 0, 0 };
    clap_process process { 0, 128, nullptr, nullptr, &buffer, 0, 1, &notes, &outputEvents };
    if (web)
    {
        messages.clear();
        const uint8_t ready = 1;
        CHECK(web->receive(plugin, &ready, 1));
    }
    CHECK(plugin->process(plugin, &process) == CLAP_PROCESS_CONTINUE);
    CHECK(std::all_of(left.begin(), left.end(), [](float x) { return std::isfinite(x); }));
    CHECK(std::any_of(left.begin(), left.end(), [](float x) { return std::abs(x) > 0.001f; }));
    plugin->on_main_thread(plugin);
    if (web)
        CHECK(std::count_if(messages.begin(), messages.end(), [](const auto& m) {
            return m.size() == 13 && m[0] == 12;
        }) == 10);
    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);
    gui->destroy(plugin);
    if (web)
    {
        CHECK(gui->create(plugin, CLAP_WINDOW_API_WEBVIEW, false));
        gui->destroy(plugin);
    }
    plugin->destroy(plugin);
    entry->deinit();
    // Native rendering uses asynchronous platform callbacks; let the process unload it.
    std::printf("PASS: %s audio, parameters and GUI lifecycle\n", web ? "web adapter" : "native plugin");
}
