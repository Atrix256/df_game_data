#include "Editor.h"

#include "imgui.h"

bool ShowEditorWindow()
{
    bool ret = false;

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_MenuBar;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (ImGui::Begin("Fullscreen Window", nullptr, window_flags))
    {
        // menu bar
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New", "Ctrl+N")) { /* Handle action */ }
                if (ImGui::MenuItem("Open", "Ctrl+O")) { /* Handle action */ }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit"))
                    ret = true;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Undo", "Ctrl+Z")) { /* Handle action */ }
                ImGui::EndMenu();
            }
        
            // Always call EndMenuBar if BeginMenuBar returns true
            ImGui::EndMenuBar(); 
        }
        ImGui::TextUnformatted("What is up?!");

        ImGui::End();
    }

    ImGui::PopStyleVar(2);

    return ret;
}
