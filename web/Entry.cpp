#include "WebClapSawDemo.h"

#include <cstring>

namespace sst::clap_saw_demo
{
namespace
{

uint32_t pluginCount(const clap_plugin_factory*) { return 1; }

const clap_plugin_descriptor* pluginDescriptor(const clap_plugin_factory*, uint32_t index)
{
    return index == 0 ? &ClapSawDemo::desc : nullptr;
}

const clap_plugin* createPlugin(const clap_plugin_factory*, const clap_host* host,
                                const char* pluginId)
{
    if (!host || !pluginId || std::strcmp(pluginId, ClapSawDemo::desc.id) != 0) return nullptr;
    return (new WebClapSawDemo(host))->clapPlugin();
}

const clap_plugin_factory factory { pluginCount, pluginDescriptor, createPlugin };

bool entryInit(const char* path)
{
    return char_clap::setResourceRoot(path);
}

void entryDeinit() { char_clap::resourceRoot.clear(); }

const void* entryGetFactory(const char* factoryId)
{
    return factoryId && std::strcmp(factoryId, CLAP_PLUGIN_FACTORY_ID) == 0
        ? &factory : nullptr;
}

} // namespace
} // namespace sst::clap_saw_demo

extern "C" CLAP_EXPORT const clap_plugin_entry clap_entry {
    CLAP_VERSION,
    sst::clap_saw_demo::entryInit,
    sst::clap_saw_demo::entryDeinit,
    sst::clap_saw_demo::entryGetFactory
};
