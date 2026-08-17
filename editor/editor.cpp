#include "imgui.h"

#include "loader.h"

struct StaticData
{
    YAMLLoader loader;
    bool fileOpenError = false;
};

static StaticData g_data;

bool ShowEditorMenu()
{
    bool ret = true;

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("Open"))
        {
            if (!g_data.loader.Load("../example/example.root.yaml"))
            {
                g_data.fileOpenError = true;
            }
        }

        if (ImGui::MenuItem("Save"))
        {
            int ijkl = 0;
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Exit"))
            ret = false;

        ImGui::EndMenu();
    }

    return ret;
}

void ShowEditorWindow()
{
    if (g_data.fileOpenError)
    {
        ImGui::OpenPopup("Load Failed");
        g_data.fileOpenError = false;
    }
    if (ImGui::BeginPopupModal("Load Failed", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Could not open file");
        if (ImGui::Button("OK", ImVec2(120, 0)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    // Table fills all remaining space in the window automatically
    // when size is (0,0) and it's the last/only content below the menu bar.
    if (ImGui::BeginTable("MainSplit", 2,
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthStretch, 0.3f);

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        // Wrap in a child so this column scrolls independently and
        // definitely fills the row's vertical space
        ImGui::BeginChild("LeftPane", ImVec2(0, 0), false);
        ImGui::Text("Left content");
        ImGui::EndChild();

        ImGui::TableSetColumnIndex(1);
        ImGui::BeginChild("RightPane", ImVec2(0, 0), false);
        ImGui::Text("Right content");
        ImGui::EndChild();

        ImGui::EndTable();
    }
}