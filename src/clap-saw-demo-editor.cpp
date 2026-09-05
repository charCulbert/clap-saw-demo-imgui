//
// Created by Paul Walker on 6/10/22.
//

#include "clap-saw-demo-editor.h"
#include "clap-saw-demo.h"
#include "clap/clap.h"

#include "imgui.h"

#include <clap/helpers/host-proxy.hxx>

#define STR_INDIR(x) #x
#define STR(x) STR_INDIR(x)

namespace sst::clap_saw_demo
{
ClapSawDemoEditor::ClapSawDemoEditor(ClapSawDemo::SynthToUI_Queue_t &i,
                                     ClapSawDemo::UIToSynth_Queue_t &o,
                                     const ClapSawDemo::DataCopyForUI &d, std::function<void()> pf)
: inbound(i), outbound(o), synthData(d), paramRequestFlush(std::move(pf))
{

}

void ClapSawDemoEditor::addSliderForParam(clap_id pid, const char* label, float min, float max)
{
    float co = paramCopy[pid];
    auto wasInEdit = paramInEdit[pid];
    if (ImGui::SliderFloat(label, &co, min, max))
    {
        if (!wasInEdit)
        {
            paramInEdit[pid] = true;
            auto q = ClapSawDemo::FromUI();
            q.id = pid;
            q.type = ClapSawDemo::FromUI::MType::BEGIN_EDIT;
            q.value = co;
            outbound.try_enqueue(q);
        }
        if (co != paramCopy[pid])
        {
            auto q = ClapSawDemo::FromUI();
            q.id = pid;
            q.type = ClapSawDemo::FromUI::MType::ADJUST_VALUE;
            q.value = co;
            outbound.try_enqueue(q);
            paramCopy[pid] = co;
        }
    }
    else
    {
        if (wasInEdit)
        {
            paramInEdit[pid] = false;
            auto q = ClapSawDemo::FromUI();
            q.id = pid;
            q.type = ClapSawDemo::FromUI::MType::END_EDIT;
            q.value = co;
            outbound.try_enqueue(q);
        }
    }
}

void ClapSawDemoEditor::addSwitchForParam(clap_id pid, const char* label, bool reverse)
{
    bool co;
    
    if (reverse)
        co = paramCopy[pid] > 0.5f ? false : true;
    else
        co = paramCopy[pid] < 0.5f ? false : true;
    
    if (ImGui::Checkbox(label, &co))
    {
        auto q = ClapSawDemo::FromUI();
        q.id = pid;
        q.type = ClapSawDemo::FromUI::MType::ADJUST_VALUE;
        if (reverse)
            q.value = co ? 0.f : 1.f;
        else
            q.value = co ? 1.f : 0.f;
        outbound.try_enqueue(q);
        paramCopy[pid] = q.value;
    }
}

void ClapSawDemoEditor::addRadioButtonForParam(clap_id pid, std::vector<std::pair<int, const char*>> modes)
{
    int prevMode = paramCopy[pid];
    int editMode = prevMode;
    for (const auto& mode : modes)
    {
        ImGui::RadioButton(mode.second, &editMode, mode.first); ImGui::SameLine();
    }
    ImGui::NewLine();
    
    if (prevMode != editMode)
    {
        auto q = ClapSawDemo::FromUI();
        q.id = pid;
        q.type = ClapSawDemo::FromUI::MType::ADJUST_VALUE;
        q.value = editMode;
        outbound.try_enqueue(q);
        paramCopy[pid] = editMode;
    }
    
}

void ClapSawDemoEditor::dequeueParamUpdates()
{
    ClapSawDemo::ToUI r;
    while (inbound.try_dequeue(r))
    {
        if (r.type == ClapSawDemo::ToUI::MType::PARAM_VALUE)
        {
            paramCopy[r.id] = r.value;
            paramInEdit[r.id] = false;
        }
    }
}

void ClapSawDemoEditor::onRender()
{
    dequeueParamUpdates(); // Do not remove this

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    bool is_open = true;
    ImGui::Begin("Imgui Saw Demo", &is_open , ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoDecoration);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    // HEADER
    std::string titleStr = "CLAP SAW DEMO IMGUI";
        
    ImColor col32(.16f, .29f, .48f , 0.54f * 0.5f);
    draw_list->AddRectFilled(ImVec2(0, 0), ImVec2(ImGui::GetWindowWidth(), 26.f), col32);
    
    const char* title = titleStr.c_str();
    auto titleSize = ImGui::CalcTextSize(title);
    ImGui::SetCursorPosX( (ImGui::GetWindowWidth() - titleSize.x) / 2.f);

    ImGui::Text( "%s", title );

    ImGui::Separator();
    
    // PARAMETER UI
    
    ImGui::Text("Osc (Polyphony %d)", (int)synthData.polyphony);

    addSliderForParam(ClapSawDemo::pmUnisonCount, "uni count", 1, SawDemoVoice::max_uni);
    addSliderForParam(ClapSawDemo::pmUnisonSpread, "uni spread", 0, 100);
    addSliderForParam(ClapSawDemo::pmOscDetune, "osc detune", -200, 200);

    ImGui::Separator();
    
    addSliderForParam(ClapSawDemo::pmPreFilterVCA, "VCA", 0, 1);
    addSwitchForParam(ClapSawDemo::pmAmpIsGate, "Amp Envelope", true);
    
    ImGui::BeginDisabled(paramCopy[ClapSawDemo::pmAmpIsGate] > 0.5f);
    addSliderForParam(ClapSawDemo::pmAmpAttack, "Attack", 0, 1);
    addSliderForParam(ClapSawDemo::pmAmpRelease, "Release", 0, 1);
    ImGui::EndDisabled();
    
    ImGui::Separator();
    
    ImGui::Text("Filter");
    
    addRadioButtonForParam(ClapSawDemo::pmFilterMode, {
        { SawDemoVoice::StereoSimperSVF::Mode::LP, "LP"},
        { SawDemoVoice::StereoSimperSVF::Mode::BP, "BP"},
        { SawDemoVoice::StereoSimperSVF::Mode::HP, "HP"},
        { SawDemoVoice::StereoSimperSVF::Mode::NOTCH, "Notch"},
        { SawDemoVoice::StereoSimperSVF::Mode::PEAK, "Peak"},
        { SawDemoVoice::StereoSimperSVF::Mode::ALL, "All"} } );
    
    addSliderForParam(ClapSawDemo::pmCutoff, "cutoff", 1, 127);
    addSliderForParam(ClapSawDemo::pmResonance, "resonance", 0, 1);

    ImGui::Separator();

    // FOOTER
    
    std::string footerStr = "CLAP v";
    footerStr += std::to_string(CLAP_VERSION_MAJOR);
    footerStr += ".";
    footerStr += std::to_string(CLAP_VERSION_MINOR);
    footerStr += ".";
    footerStr += std::to_string(CLAP_VERSION_REVISION);
    footerStr += " - ";
    footerStr += io.BackendRendererName;
    footerStr += " - ";
    footerStr += io.BackendPlatformName;
    
    draw_list->AddRectFilled(ImVec2(0, ImGui::GetCursorPosY() - 6.f), ImVec2(ImGui::GetWindowWidth(), ImGui::GetCursorPosY()+26.f-6.f), col32);
 
    const char* footer = footerStr.c_str();
    ImGui::SetCursorPosX( (ImGui::GetWindowWidth() - ImGui::CalcTextSize(footer).x) / 2.f);
    ImGui::Text( "%s", footer );
}

} // namespace sst::clap_saw_demo
