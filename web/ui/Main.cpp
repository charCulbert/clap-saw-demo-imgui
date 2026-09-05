/*
 * Browser backend for the shared CLAP Saw ImGui renderer.
 * The plug-in and original ImGui layout are MIT licensed; see the bundled notice.
 */

#include "clap-saw-demo-editor.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <emscripten.h>

#include <cstdint>

// Emscripten's GLFW lacks the gamepad API used by upstream's pinned ImGui backend.
extern "C" int glfwGetGamepadState(int, GLFWgamepadstate*) { return GLFW_FALSE; }

namespace
{

EM_JS(void, sendParameterEdit, (int command, uint32_t paramId, double value, int hasValue), {
    const message = new ArrayBuffer(hasValue ? 13 : 5);
    const data = new DataView(message);
    data.setUint8(0, command);
    data.setUint32(1, paramId, true);
    if (hasValue) data.setFloat64(5, value, true);
    window.parent.postMessage(message, "*");
});

using namespace sst::clap_saw_demo;
ClapSawDemo::SynthToUI_Queue_t inbound;
ClapSawDemo::UIToSynth_Queue_t outbound;
ClapSawDemo::DataCopyForUI synthData;
ClapSawDemoEditor editor(inbound, outbound, synthData, [] {});

void renderFrame()
{
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({ 0.0f, 0.0f });
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    editor.onRender();
    ImGui::End(); // The native backend also closes the window opened by onRender().

    ClapSawDemo::FromUI edit {};
    while (outbound.try_dequeue(edit))
    {
        const auto command = edit.type == ClapSawDemo::FromUI::BEGIN_EDIT ? 9
                           : edit.type == ClapSawDemo::FromUI::END_EDIT ? 11 : 10;
        sendParameterEdit(command, edit.id, edit.value, command == 10);
    }
    ImGui::Render();

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(glfwGetCurrentContext(), &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(0.08f, 0.08f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE void setParameterValue(uint32_t id, double value)
{
    inbound.try_enqueue({ ClapSawDemo::ToUI::PARAM_VALUE, id, value });
}

extern "C" EMSCRIPTEN_KEEPALIVE void setPluginStatus(int voiceCount, int isProcessing)
{
    synthData.polyphony = voiceCount;
    synthData.isProcessing = isProcessing != 0;
}

int main()
{
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    auto* window = glfwCreateWindow(540, 324, "CLAP Saw Demo", nullptr, nullptr);
    if (!window) return 1;
    glfwMakeContextCurrent(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 300 es");
    emscripten_set_main_loop(renderFrame, 0, true);
    return 0;
}
