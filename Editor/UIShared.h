#pragma once

#include "imgui.h"
#include "FontAwesome/IconsFontAwesome7.h"

class ImGui_Enabled
{
private:
    bool enabled;
public:
    ImGui_Enabled(bool inEnabled)
        : enabled(inEnabled)
    {
        if (!enabled)
            ImGui::BeginDisabled();
    }
    ~ImGui_Enabled()
    {
        if (!enabled)
            ImGui::EndDisabled();
    }
};


static void ShowToolTip(const char* tooltip, bool showQ = true)
{
    if (!tooltip || !tooltip[0])
        return;

    if (showQ)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.0f, 1.0f), "[?]");
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", tooltip);
}
