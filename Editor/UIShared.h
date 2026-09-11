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
