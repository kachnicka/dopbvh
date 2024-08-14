#include "Benchmark.h"

static std::string to_string(backend::config::BV bv)
{
    switch (bv) {
    case backend::config::BV::eNone:
        return "none";
    case backend::config::BV::eAABB:
        return "aabb";
    case backend::config::BV::eDOP14:
        return "dop14";
    case backend::config::BV::eOBB:
        return "obb";
    }
    return "unknown";
}

static std::string to_string(backend::config::CompressedLayout layout)
{
    switch (layout) {
    case backend::config::CompressedLayout::eBinaryStandard:
        return "bin standard";
    case backend::config::CompressedLayout::eBinaryDOP14Split:
        return "bin dop14 s";
    }
    return "unknown";
}

namespace module {

void Benchmark::ProcessGUI(State& state)
{
    ImGui::Begin("Benchmark");
    ImGui::BeginDisabled(rt.bRunning);

    if (ImGui::Button("Reload benchmark.toml"))
        LoadConfig();

    ImGui::SameLine();
    if (ImGui::Button("Run"))
        SetupBenchmarkRun();

    auto const inComputeMode = backend.state.selectedRenderMode_TMP == std::to_underlying(backend::vulkan::State::Renderer::ePathTracingCompute);

    ImGui::BeginChild("b. list", ImVec2(130, ImGui::GetContentRegionAvail().y), true, ImGuiWindowFlags_HorizontalScrollbar);
    static int bShowPreview { 0 };
    if (!inComputeMode) {
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::Text("Switch to 'Path Tracing compute' mode. Hotkey [f].");;
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }
    ImGui::BeginDisabled(!inComputeMode);
    for (int i { 0 }; i < csize<int>(bPipelines); ++i) {
        if (ImGui::Selectable(bPipelines[i].name.c_str(), rt.currentPipeline == i, ImGuiSelectableFlags_AllowDoubleClick)) {
            bShowPreview = i;
            if (ImGui::IsMouseDoubleClicked(0)) {
                rt.currentPipeline = i;
                backend.pt_compute->SetPipelineConfiguration(bPipelines[i]);
            }
        }
    }
    ImGui::EndDisabled();


    ImGui::NewLine();
    auto& scenes { Config::GetScenes() };
    for (int i { 0 }; i < csize<int>(scenes); ++i) {
        if (ImGui::Selectable(scenes[i].name.c_str(), rt.currentScene == i, ImGuiSelectableFlags_AllowDoubleClick)) {
            if (ImGui::IsMouseDoubleClicked(0)) {
                rt.currentScene = i;
                state.sceneToLoad = scenes[i];
            }
        }
    }
    ImGui::EndChild();

    char buf[32];
    auto const columnWidth { (ImGui::GetContentRegionAvail().x - 130) - 8 };
    static ImGuiTableFlags tableFlags { ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody };
    static auto const printConfigValue { [&buf](char const* label, char const* format, auto value) {
        ImGui::TableNextColumn();
        std::snprintf(buf, sizeof(buf), "%s", label);
        ImGui::TextUnformatted(buf);
        ImGui::TableNextColumn();
        std::snprintf(buf, sizeof(buf), format, value);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(buf).x);
        ImGui::TextUnformatted(buf);
    } };

    ImGui::SameLine();
    ImGui::BeginChild("Preview", ImVec2(columnWidth, ImGui::GetContentRegionAvail().y), true);
    if (ImGui::BeginTable("##preview plocpp", 1, tableFlags)) {
        ImGui::TableSetupColumn("PLOCpp", ImGuiTableColumnFlags_NoHide);
        ImGui::TableHeadersRow();

        printConfigValue("b. volume", "%s", to_string(bPipelines[bShowPreview].plocpp.bv).c_str());
        printConfigValue("s. filling", "%s", "morton32");
        printConfigValue("radius", "%u", bPipelines[bShowPreview].plocpp.radius);

        ImGui::EndTable();
    }
    if (ImGui::BeginTable("##preview collapsing", 1, tableFlags)) {
        ImGui::TableSetupColumn("Collapsing", ImGuiTableColumnFlags_NoHide);
        ImGui::TableHeadersRow();

        printConfigValue("b. volume", "%s", to_string(bPipelines[bShowPreview].collapsing.bv).c_str());
        printConfigValue("max leaf size", "%u", bPipelines[bShowPreview].collapsing.maxLeafSize);
        printConfigValue("c_t", "%.1f", bPipelines[bShowPreview].collapsing.c_t);
        printConfigValue("c_i", "%.1f", bPipelines[bShowPreview].collapsing.c_i);

        ImGui::EndTable();
    }
    if (ImGui::BeginTable("##preview transformation", 1, tableFlags)) {
        ImGui::TableSetupColumn("Transformation", ImGuiTableColumnFlags_NoHide);
        ImGui::TableHeadersRow();

        printConfigValue("b. volume", "%s", to_string(bPipelines[bShowPreview].transformation.bv).c_str());

        ImGui::EndTable();
    }
    if (ImGui::BeginTable("##preview compression", 1, tableFlags)) {
        ImGui::TableSetupColumn("Compression", ImGuiTableColumnFlags_NoHide);
        ImGui::TableHeadersRow();

        printConfigValue("b. volume", "%s", to_string(bPipelines[bShowPreview].compression.bv).c_str());
        printConfigValue("comp. layout", "%s", to_string(bPipelines[bShowPreview].compression.layout).c_str());

        ImGui::EndTable();
    }
    if (ImGui::BeginTable("##preview tracer", 1, tableFlags)) {
        ImGui::TableSetupColumn("Tracer", ImGuiTableColumnFlags_NoHide);
        ImGui::TableHeadersRow();

        printConfigValue("b. volume", "%s", to_string(bPipelines[bShowPreview].tracer.bv).c_str());
        printConfigValue("#workgroups", "%u", bPipelines[bShowPreview].tracer.workgroupCount);

        ImGui::EndTable();
    }
    if (ImGui::BeginTable("##preview stats", 1, tableFlags)) {
        ImGui::TableSetupColumn("Stats", ImGuiTableColumnFlags_NoHide);
        ImGui::TableHeadersRow();

        // printConfigValue("b. volume", "%s", to_string(bPipelines[bShowPreview].stats.bv).c_str());
        printConfigValue("c_t", "%.1f", bPipelines[bShowPreview].stats.c_t);
        printConfigValue("c_i", "%.1f", bPipelines[bShowPreview].stats.c_i);

        ImGui::EndTable();
    }
    ImGui::EndChild();

    ImGui::EndDisabled();
    ImGui::End();
}

}
