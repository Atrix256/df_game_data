#include "Editor.h"

#include "imgui.h"
#include <nfd.h>

struct EditorData
{

};

static EditorData s_editorData;

static bool ShowMenuBar()
{
    bool ret = false;

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open", "Ctrl+O"))
            {
                nfdchar_t* outPath = NULL;

                nfdu8filteritem_t filters[] =
                {
                    { "Database Root Files", "dbroot" }
                };

                nfdresult_t result = NFD_OpenDialogU8(&outPath, filters, IM_COUNTOF(filters), nullptr);

                if (result == NFD_OKAY)
                {
                    // TODO: load using the loader
                    NFD_FreePathU8(outPath);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit"))
                ret = true;
            ImGui::EndMenu();
        }

        // Always call EndMenuBar if BeginMenuBar returns true
        ImGui::EndMenuBar();
    }

    return ret;
}

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
        ret = ShowMenuBar();

        ImGui::TextUnformatted("What is up?!");

        ImGui::End();
    }

    ImGui::PopStyleVar(2);

    return ret;
}
/*
TODO:
* load data
* display data
* edit data
* save data
* when done: get rid of other editor app. rename this one to editor. add imgui to OSS list, remove wxwidgets. update vcpkg script.

*/