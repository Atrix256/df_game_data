#include "Editor.h"

#include "imgui.h"
#include <nfd.h>

#include "../loader/loader.h"
#include "DataItem.h"
#include <filesystem>
#include "UIShared.h"

extern void SetWindowTitle(const char* text);

static EditorData s_editorData;

static void LoadFile(const char* fileName)
{
    s_editorData.m_dbroot.Clear();
    s_editorData = EditorData();

    // select the first data item of the first table, if present
    if (s_editorData.m_dbroot.Load(fileName))
    {
        for (auto& pair1 : s_editorData.m_dbroot.m_tables)
        {
            s_editorData.m_selectedTableName = pair1.first;
            for (auto& pair2 : pair1.second->m_data)
            {
                s_editorData.m_selectedDataItemName = pair2.first;
                break;
            }
            break;
        }
    }

    s_editorData.m_updateWindowTitle = true;

    s_editorData.m_recentFiles.AddEntry(fileName);
}

static void OnFileOpen()
{
    nfdchar_t* outPath = NULL;

    nfdu8filteritem_t filters[] =
    {
        { "Database Root Files", "dbroot" }
    };

    nfdresult_t result = NFD_OpenDialogU8(&outPath, filters, IM_COUNTOF(filters), nullptr);

    if (result == NFD_OKAY)
    {
        LoadFile(outPath);
        NFD_FreePathU8(outPath);
    }
}

static void OnFileSave()
{
    if (s_editorData.m_dbroot.m_tables.count(s_editorData.m_selectedTableName) == 0)
        return;
    DBTable& table = *s_editorData.m_dbroot.m_tables[s_editorData.m_selectedTableName].get();

    if (table.m_data.count(s_editorData.m_selectedDataItemName) == 0)
        return;
    DBTable::JSONData& data = *table.m_data[s_editorData.m_selectedDataItemName].get();

    data.m_dirty = false;

    std::string jsonString = data.m_data.dump(4);

    FILE* file = nullptr;
    fopen_s(&file, data.m_path.c_str(), "wb");
    if (file)
    {
        fwrite(jsonString.c_str(), 1, jsonString.size(), file);
        fclose(file);
    }
}

static void OnFileSaveAll()
{
    for (auto& tableIt : s_editorData.m_dbroot.m_tables)
    {
        DBTable& table = *tableIt.second.get();
        for (auto& dataIt : table.m_data)
        {
            DBTable::JSONData& data = *dataIt.second.get();
            data.m_dirty = false;
            std::string jsonString = data.m_data.dump(4);

            FILE* file = nullptr;
            fopen_s(&file, data.m_path.c_str(), "wb");
            if (file)
            {
                fwrite(jsonString.c_str(), 1, jsonString.size(), file);
                fclose(file);
            }
        }
    }
    s_editorData.m_documentDirty = false;
    s_editorData.m_updateWindowTitle = true;
}

void ShowRecentFiles()
{
    if (ImGui::BeginMenu("Recent Files", !s_editorData.m_recentFiles.GetEntries().empty()))
    {
        if (!s_editorData.m_recentFiles.GetEntries().empty())
        {
            for (const auto& path : s_editorData.m_recentFiles.GetEntries())
            {
                if (ImGui::MenuItem(path.c_str()))
                {
                    std::string pathCopy = path;
                    LoadFile(pathCopy.c_str());
                    break;
                }
            }
        }
        ImGui::EndMenu();
    }
}

static bool ShowMenuBar()
{
    bool ret = false;

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open", "Ctrl+O"))
                OnFileOpen();

            if (ImGui::MenuItem("Save", "Ctrl+S"))
                OnFileSave();

            if (ImGui::MenuItem("Save All", "Ctrl+A"))
                OnFileSaveAll();

            ImGui::Separator();

            ShowRecentFiles();

            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Ctrl+X"))
                ret = true;
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O))
        OnFileOpen();

    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_A))
        OnFileSaveAll();

    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S))
        OnFileSave();

    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_X))
        ret = true;

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
            {
                s_editorData.m_selectedTableName = pair.first;
                s_editorData.m_selectedDataItemName = "";

                for (auto& pair2 : pair.second->m_data)
                {
                    s_editorData.m_selectedDataItemName = pair2.first;
                    break;
                }
            }

            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }
}

static std::string GetUniqueDataItemName(const char* baseName)
{
    if (s_editorData.m_dbroot.m_tables.count(s_editorData.m_selectedTableName) == 0)
        return baseName;

    DBTable& table = *s_editorData.m_dbroot.m_tables[s_editorData.m_selectedTableName].get();

    if (!table.m_data.contains(baseName))
        return baseName;

    char buffer[1024];
    int index = 0;
    while(1)
    {
        index++;
        sprintf_s(buffer, "%s_%i", baseName, index);
        if (!table.m_data.contains(buffer))
            return buffer;
    }
}

static void OnDataListNew()
{
    if (s_editorData.m_dbroot.m_tables.count(s_editorData.m_selectedTableName) == 0)
        return;

    DBTable& table = *s_editorData.m_dbroot.m_tables[s_editorData.m_selectedTableName].get();

    std::string itemName = GetUniqueDataItemName("NewEntry");
    std::filesystem::path fileName = (std::filesystem::path(table.GetPath()).remove_filename() / itemName).replace_extension(".json");

    // make the file
    {
        FILE* file = nullptr;
        fopen_s(&file, fileName.string().c_str(), "wb");
        if (!file)
            return;

        fprintf(file, "{\n}\n");
        fclose(file);
    }

    // make the entry in the data
    table.LoadFile(fileName.string().c_str());
}

static void OnDataListDelete()
{
    if (s_editorData.m_dbroot.m_tables.count(s_editorData.m_selectedTableName) == 0)
        return;

    DBTable& table = *s_editorData.m_dbroot.m_tables[s_editorData.m_selectedTableName].get();

    if (table.m_data.count(s_editorData.m_selectedDataItemName) == 0)
        return;

    DBTable::JSONData& data = *table.m_data[s_editorData.m_selectedDataItemName].get();

    // delete file from disk
    std::filesystem::remove(data.m_path);

    // delete from the table
    table.m_data.erase(s_editorData.m_selectedDataItemName);
}

static void ShowDataList()
{
    if (ImGui::Button("New"))
        OnDataListNew();
    ImGui::SameLine();
    if (ImGui::Button("Delete"))
        OnDataListDelete();

    // TODO: too many buttons. make a right click menu
    /*
    ImGui::SameLine();
    ImGui::Button("Duplicate");
    ImGui::SameLine();
    ImGui::Button("Rename");
    ImGui::SameLine();
    ImGui::Button("Save");

    TODO: revert or reload as an option, to go along with save?
    */

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f, 0.10f, 0.10f, 1.0f));
    if (ImGui::BeginListBox("##DataListBox", ImVec2(-FLT_MIN, -FLT_MIN)))
    {
        if (s_editorData.m_dbroot.m_tables.count(s_editorData.m_selectedTableName) > 0)
        {
            DBTable& table = *s_editorData.m_dbroot.m_tables[s_editorData.m_selectedTableName].get();

            for (auto& pair : table.m_data)
            {
                const bool is_selected = (s_editorData.m_selectedDataItemName == pair.first);

                std::string label = pair.first.c_str();
                if (pair.second->m_dirty)
                    label += " *";

                if (ImGui::Selectable(label.c_str(), is_selected))
                    s_editorData.m_selectedDataItemName = pair.first;

                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndListBox();
    }

    ImGui::PopStyleColor();
}

bool ShowEditorWindow()
{
    bool ret = false;

    if (s_editorData.m_updateWindowTitle)
    {
        char buffer[2048];
        const char* path = s_editorData.m_dbroot.GetPath();
        if (path && path[0])
            sprintf_s(buffer, "df_game_data Editor - %s%s", std::filesystem::path(path).filename().string().c_str(), s_editorData.m_documentDirty ? " *" : "");
        else
            strcpy_s(buffer, "df_game_data Editor");
        SetWindowTitle(buffer);
        s_editorData.m_updateWindowTitle = false;
    }

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
            ShowDataList();

            ImGui::TableNextColumn();
            ShowDataEditor(s_editorData);

            ImGui::EndTable();
        }

        ImGui::End();
    }

    ImGui::PopStyleVar(2);

    return ret;
}
/*
TODO:
* when done: get rid of other editor app. rename this one to editor. add imgui to OSS list, remove wxwidgets. update vcpkg script.
* application icon
* undo redo stack
* add text copy/paste?
* look for TODOs
* explain the design decisions (each data item as a json data file for easier merging. flat tables for speed. multiple tables because that's whats needed. table links)
* Explain how to use it
* imgui srgb target? or do we care?
* maybe try the light theme of imgui?
* watch files on disk and react to them for hot loading
*/
