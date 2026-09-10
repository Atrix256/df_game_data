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
    else
    {
        // TODO: show error message in a popup window
    }

    s_editorData.m_updateWindowTitle = true;

    s_editorData.m_recentFiles.AddEntry(fileName);
}

static void OnFileOpen()
{
    nfdchar_t* outPath = NULL;

    nfdu8filteritem_t filters[] =
    {
        { "Database Root Files", "dbroot" },
        { "FlatBuffers Schema", "fbs" }
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

static void OnDataListReload()
{
    if (s_editorData.m_dbroot.m_tables.count(s_editorData.m_selectedTableName) == 0)
        return;

    DBTable& table = *s_editorData.m_dbroot.m_tables[s_editorData.m_selectedTableName].get();

    if (table.m_data.count(s_editorData.m_selectedDataItemName) == 0)
        return;

    DBTable::JSONData& data = *table.m_data[s_editorData.m_selectedDataItemName].get();

    std::filesystem::path src(data.m_path);

    table.m_data.erase(s_editorData.m_selectedDataItemName);

    table.Load(src.string().c_str());
}

static void OnDataListRename(const char* newName)
{
    if (s_editorData.m_dbroot.m_tables.count(s_editorData.m_selectedTableName) == 0)
        return;

    DBTable& table = *s_editorData.m_dbroot.m_tables[s_editorData.m_selectedTableName].get();

    if (table.m_data.count(s_editorData.m_selectedDataItemName) == 0)
        return;

    DBTable::JSONData& data = *table.m_data[s_editorData.m_selectedDataItemName].get();

    // Copy the file
    std::filesystem::path src(data.m_path);

    std::filesystem::path dst = src;
    dst.replace_filename(newName).replace_extension(".json");

    std::error_code ec;
    std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);

    // Load the file
    table.Load(src.string().c_str());

    // delete old file from disk
    std::filesystem::remove(src);

    // delete old from the table
    table.m_data.erase(s_editorData.m_selectedDataItemName);

    // select the new item
    s_editorData.m_selectedDataItemName = newName;
}

static void OnDataListDuplicate()
{
    if (s_editorData.m_dbroot.m_tables.count(s_editorData.m_selectedTableName) == 0)
        return;

    DBTable& table = *s_editorData.m_dbroot.m_tables[s_editorData.m_selectedTableName].get();

    if (table.m_data.count(s_editorData.m_selectedDataItemName) == 0)
        return;

    DBTable::JSONData& data = *table.m_data[s_editorData.m_selectedDataItemName].get();

    std::string newItemName = GetUniqueDataItemName(s_editorData.m_selectedDataItemName.c_str());

    // Copy the file
    std::filesystem::path src(data.m_path);

    std::filesystem::path dst = src;
    dst.replace_filename(newItemName).replace_extension(".json");

    std::error_code ec;
    std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);

    // Load the file
    table.Load(src.string().c_str());

    // select the new item
    s_editorData.m_selectedDataItemName = newItemName;
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

    static bool showRename = false;
    static std::string newName;
    bool wantShowRename = false;
    if (ImGui::BeginPopupContextItem("my_item_context"))
    {
        if (ImGui::Selectable("New"))
            OnDataListNew();

        if (ImGui::Selectable("Duplicate"))
            OnDataListDuplicate();

        if (ImGui::Selectable("Rename"))
            wantShowRename = true;

        if (ImGui::Selectable("Save"))
            OnFileSave();

        if (ImGui::Selectable("Delete"))
            OnDataListDelete();

        if (ImGui::Selectable("Reload"))
            OnDataListReload();

        ImGui::EndPopup();
    }

    if (wantShowRename)
    {
        showRename = true;
        ImGui::OpenPopup("Rename Item");
        newName = s_editorData.m_selectedDataItemName;
    }

    if (ImGui::BeginPopupModal("Rename Item", &showRename, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Please enter new name");

        static std::vector<char> tmpBuffer;
        tmpBuffer.resize(4096);
        strcpy_s(tmpBuffer.data(), tmpBuffer.size(), newName.c_str());

        if (ImGui::InputText("##RenameDataItem", tmpBuffer.data(), tmpBuffer.size()))
            newName = tmpBuffer.data();

        ImGui::Separator();

        if (ImGui::Button("OK", ImVec2(120, 0)))
        {
            OnDataListRename(newName.c_str());
            ImGui::CloseCurrentPopup();
            showRename = false;
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
            showRename = false;
        }

        ImGui::EndPopup();
    }
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

    ret |= ImGui::GetMainViewport()->PlatformRequestClose;
    static bool showConfirmExit = false;
    if (ret && s_editorData.m_documentDirty)
    {
        ImGui::GetMainViewport()->PlatformRequestClose = false;
        ret = false;
        showConfirmExit = true;
        ImGui::OpenPopup("Exit Confirmation");
    }

    if (ImGui::BeginPopupModal("Exit Confirmation", &showConfirmExit, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Data is unsaved, exit anyway?");
        ImGui::Separator();

        if (ImGui::Button("Yes", ImVec2(120, 0)))
        {
            ret = true;
            ImGui::CloseCurrentPopup();
            showConfirmExit = false;
        }

        ImGui::SameLine();

        if (ImGui::Button("No", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
            showConfirmExit = false;
        }

        ImGui::EndPopup();
    }

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
// TODO: Maybe dbroot is json with a hard coded schema and make file menu options to.make.a new one, save, save as? and edit in the editor in a window
// * no: Have a user file next to dbroot or other file extension. with a hard coded schema and a window to edit it
// * this is for settings like "where do we compile the output to?" etc
// TODO: make a test dataset that has all the things in it. move example into DataSets and have this other one be "Test"
// test data set has all types exhaustively
// TODO: note in the docs that this acts as a flatbuffer editor too because of how it works
// TODO: remove file from recent if loading fails
// TODO: support drag/drop of fbs and dbroot files onto window
// TODO: why does a string without a default just default to "0"? should figure that out and maybe give a fix patch
