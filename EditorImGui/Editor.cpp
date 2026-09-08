#include "Editor.h"

#include "imgui.h"
#include <nfd.h>

#include "../loader/loader.h"

struct EditorData
{
    DBRoot m_dbroot;
    std::string m_selectedTableName;
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
                    if (!s_editorData.m_dbroot.Load(outPath))
                    {
                        s_editorData.m_dbroot.Clear();
                        s_editorData = EditorData();
                    }

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

static void ShowTableList()
{
    if (ImGui::BeginCombo("Table", s_editorData.m_selectedTableName.c_str()))
    {
        for (auto& pair : s_editorData.m_dbroot.m_tables)
        {
            const bool is_selected = (s_editorData.m_selectedTableName == pair.first);

            if (ImGui::Selectable(pair.first.c_str(), is_selected))
                s_editorData.m_selectedTableName = pair.first;

            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }
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
        ret |= ShowMenuBar();
        ShowTableList();

        ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
        ImVec2 outer_size = ImVec2(0.0f, -1.0f);
        if (ImGui::BeginTable("DataItems", 2, flags, outer_size))
        {
            // Make left column take 20% of the width
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 0.2f);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 0.8f);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Data Item List");

            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Data Item Editor");

            ImGui::EndTable();
        }

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