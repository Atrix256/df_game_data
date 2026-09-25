#include "Editor.h"

#include "imgui.h"
#include <nfd.h>

#include "../loader/loader.h"
#include "DataItem.h"
#include <filesystem>
#include "UIShared.h"
#include "Compile.h"
#include "Platform.h"

static EditorData s_editorData;

std::string s_commandLineFileName;
static bool s_loadCommandLine = false;

static void OnFileSaveAll();

static bool DoCompile()
{
    OnFileSaveAll();
    s_editorData.m_compileSucceeded = true;
    s_editorData.m_showCompileResultsWindow = true;

    for (const DBCompileSettings& settings : s_editorData.m_dbroot.m_compileSettings)
    {
        if (!Compile(s_editorData.m_dbroot, settings, s_editorData.m_compileOutput))
        {
            s_editorData.m_compileSucceeded = false;
            break;
        }
    }

    return s_editorData.m_compileSucceeded;
}

static void LoadFile(const char* fileName)
{
    s_editorData.m_dbroot.Clear();
    s_editorData = EditorData();

    // If the file doesn't exist, remove it from recent files, else add it
    std::error_code ec;
    if (!std::filesystem::exists(fileName, ec))
    {
        s_editorData.m_recentFiles.RemoveEntry(fileName);
        s_editorData.m_showLoadingErrors = true;
        std::string errMsg = "File Doesn't Exist: " + std::string(fileName);
        s_editorData.m_dbroot.SetErrorText(errMsg.c_str());
        return;
    }
    s_editorData.m_recentFiles.AddEntry(fileName);

    // select the first data item of the first table, if present
    if (s_editorData.m_dbroot.Load(fileName))
    {
        for (auto& pair1 : s_editorData.m_dbroot.m_tables)
        {
            if (pair1.second->m_loadOrder != 0)
                continue;

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
        s_editorData.m_showLoadingErrors = true;
    }

    s_editorData.m_updateWindowTitle = true;
}

static void OnFileNewDatabase(bool checkDirty)
{
    if (checkDirty && s_editorData.m_documentDirty)
    {
        s_editorData.m_showConfirmNew = true;
        return;
    }

    nfdchar_t* outPath = NULL;

    nfdu8filteritem_t filters[] =
    {
        { "DBRoot Files (*.dbroots)", "dbroot" }
    };

    nfdresult_t result = NFD_SaveDialogU8(&outPath, filters, IM_COUNTOF(filters), nullptr, nullptr);

    if (result != NFD_OKAY)
        return;

    if (!s_editorData.m_dbroot.New(outPath))
        s_editorData.m_showLoadingErrors = true;

    NFD_FreePathU8(outPath);

    s_editorData.m_documentDirty = false;
    s_editorData.m_updateWindowTitle = true;
}

static void OnFileOpen()
{
    nfdchar_t* outPath = NULL;

    nfdu8filteritem_t filters[] =
    {
        { "Supported Files (*.dbroot, *.def)", "dbroot,def" }
    };

    nfdresult_t result = NFD_OpenDialogU8(&outPath, filters, IM_COUNTOF(filters), nullptr);

    if (result == NFD_OKAY)
    {
        LoadFile(outPath);
        NFD_FreePathU8(outPath);
    }
}

// Make the JSON items be in the order that they are defined in the schema.
// Useful for making sure union types come before union fields
void CanonicalizeFieldOrder(nlohmann::ordered_json& value, const DefParser& parser, const DefParser::Struct& structDef)
{
    if (!value.is_object())
        return;

    nlohmann::ordered_json reordered = nlohmann::ordered_json::object();

    // put things in order
    for (auto& field : structDef.fields)
    {
        auto it = value.find(field.name);
        if (it == value.end())
            continue;
        reordered[field.name] = std::move(it.value());
    }

    // Anything not in the schema (shouldn't normally exist) goes last.
    for (auto it = value.begin(); it != value.end(); ++it)
    {
        if (!reordered.contains(it.key()))
            reordered[it.key()] = std::move(it.value());
    }

    value = std::move(reordered);

    // Recurse into nested structs/tables and arrays thereof.
    for (auto& field : structDef.fields)
    {
        auto it = value.find(field.name);
        if (it == value.end())
            continue;

        if (field.fieldType == DefParser::FieldType::_struct)
        {
            const DefParser::Struct& s = *parser.GetStructByName(field.structName.c_str());

            if (field.isArray)
            {
                for (auto& element : it.value())
                    CanonicalizeFieldOrder(element, parser, s);
            }
            else
            {
                CanonicalizeFieldOrder(it.value(), parser, s);
            }
        }
    }
}

static void SaveJSON(DBTable& table, const json& jsonIn, const char* fileName)
{
    json jsonOut = jsonIn;
    CanonicalizeFieldOrder(jsonOut, table.GetParser(), *table.GetParser().GetRootStruct());

    std::string jsonString = jsonOut.dump(4);

    // If the file already exists and hasn't changed, don't touch it again
    FILE* file = nullptr;
    fopen_s(&file, fileName, "rb");
    if (file)
    {
        fseek(file, 0, SEEK_END);
        std::vector<char> fileData(ftell(file));
        fread(fileData.data(), 1, fileData.size(), file);
        fileData.push_back(0);
        fclose(file);

        if (strcmp(jsonString.c_str(), fileData.data()) == 0)
            return;
    }

    fopen_s(&file, fileName, "wb");
    if (file)
    {
        fwrite(jsonString.c_str(), 1, jsonString.size(), file);
        fclose(file);
    }

    // Figure out if the document is dirty or not
    s_editorData.m_documentDirty = false;
    for (const auto& pair1 : s_editorData.m_dbroot.m_tables)
    {
        for (const auto& pair2 : pair1.second->m_data)
        {
            s_editorData.m_documentDirty |= pair2.second->m_dirty;

            if (s_editorData.m_documentDirty)
                break;
        }
        if (s_editorData.m_documentDirty)
            break;
    }
    s_editorData.m_updateWindowTitle = true;
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

    SaveJSON(table, data.m_data, data.m_path.c_str());
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

            SaveJSON(table, data.m_data, data.m_path.c_str());
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
            if (ImGui::MenuItem("New Database", "Ctrl+N"))
                OnFileNewDatabase(true);

            if (ImGui::MenuItem("Open Database", "Ctrl+O"))
                OnFileOpen();

            if (ImGui::MenuItem("Save Data Item", "Ctrl+S", false, s_editorData.m_dbroot.Loaded()))
                OnFileSave();

            if (ImGui::MenuItem("Save All Data Items", "Ctrl+A", false, s_editorData.m_dbroot.Loaded()))
                OnFileSaveAll();

            ImGui::Separator();

            ShowRecentFiles();

            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Ctrl+X"))
                ret = true;
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Settings", nullptr, false, s_editorData.m_dbroot.Loaded()))
                s_editorData.m_openSettingsWindow = true;

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Compile"))
        {
            if (ImGui::MenuItem("Compile", "Ctrl+C", false, s_editorData.m_dbroot.Loaded()))
                DoCompile();

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_N))
        OnFileNewDatabase(true);

    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O))
        OnFileOpen();

    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_A))
        OnFileSaveAll();

    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S))
        OnFileSave();

    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_X))
        ret = true;

    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_C))
        DoCompile();

    return ret;
}

static void ShowTableList()
{
    int selectedIndex = -1;
    std::vector<std::string> tableOrder(s_editorData.m_dbroot.m_tables.size());
    int index = 0;
    for (const auto& pair : s_editorData.m_dbroot.m_tables)
    {
        tableOrder[pair.second->m_loadOrder] = pair.first;
        if (pair.first == s_editorData.m_selectedTableName)
            selectedIndex = pair.second->m_loadOrder;
        index++;
    }

    if (ImGui::BeginCombo("Table", s_editorData.m_selectedTableName.c_str()))
    {
        for (const std::string& tableName : tableOrder)
        {
            const bool is_selected = (s_editorData.m_selectedTableName == tableName);

            if (ImGui::Selectable(tableName.c_str(), is_selected))
            {
                s_editorData.m_selectedTableName = tableName;
                s_editorData.m_selectedDataItemName = "";

                for (auto& pair2 : s_editorData.m_dbroot.m_tables[tableName]->m_data)
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

    {
        ImGui_Enabled enabled(s_editorData.m_dbroot.m_tables.contains(s_editorData.m_selectedTableName));
        ImGui::SameLine();
        if (ImGui::Button("Remove"))
        {
            if (s_editorData.m_dbroot.m_tables.contains(s_editorData.m_selectedTableName))
            {
                s_editorData.m_dbroot.RemoveTable(s_editorData.m_selectedTableName.c_str());
                s_editorData.m_dbroot.SaveDBRoot();

                // set a new selected table since we deleted the old selection
                s_editorData.m_selectedTableName = "";
                for (auto& pair : s_editorData.m_dbroot.m_tables)
                {
                    s_editorData.m_selectedTableName = pair.first;
                    for (auto& pair2 : s_editorData.m_dbroot.m_tables[s_editorData.m_selectedTableName]->m_data)
                    {
                        s_editorData.m_selectedDataItemName = pair2.first;
                        break;
                    }
                    break;
                }
            }
        }
        ShowToolTip("Removes this table from the database", false);
    }

    {
        ImGui_Enabled enabled(s_editorData.m_dbroot.Loaded());
        ImGui::SameLine();
        if (ImGui::Button("Add"))
        {
            nfdchar_t* outPath = NULL;

            nfdu8filteritem_t filters[] =
            {
                { "Flatbuffer Schema (*.def)", "def" }
            };

            nfdresult_t result = NFD_OpenDialogU8(&outPath, filters, IM_COUNTOF(filters), nullptr);

            if (result == NFD_OKAY)
            {
                if (s_editorData.m_dbroot.AddTable(outPath))
                {
                    s_editorData.m_selectedTableName = s_editorData.m_dbroot.m_tables.rbegin()->first;
                    for (auto& pair2 : s_editorData.m_dbroot.m_tables[s_editorData.m_selectedTableName]->m_data)
                    {
                        s_editorData.m_selectedDataItemName = pair2.first;
                        break;
                    }
                    s_editorData.m_dbroot.SaveDBRoot();
                }
                else
                    s_editorData.m_showLoadingErrors = true;
            }
        }
        ShowToolTip("Adds a table to the database", false);
    }

    {
        ImGui::SameLine();
        ImGui_Enabled enabled(selectedIndex > 0);
        if (ImGui::SmallButton(ICON_FA_ANGLE_UP "##Up"))
        {
            std::swap(
                s_editorData.m_dbroot.m_tables[tableOrder[selectedIndex]]->m_loadOrder,
                s_editorData.m_dbroot.m_tables[tableOrder[selectedIndex - 1]]->m_loadOrder
            );
            s_editorData.m_dbroot.SaveDBRoot();
        }
        ShowToolTip("Move this table up in loading order.", false);
    }
    {
        ImGui::SameLine();
        ImGui_Enabled enabled(selectedIndex >= 0 && selectedIndex + 1 < tableOrder.size());
        if (ImGui::SmallButton(ICON_FA_ANGLE_DOWN "##Down"))
        {
            std::swap(
                s_editorData.m_dbroot.m_tables[tableOrder[selectedIndex]]->m_loadOrder,
                s_editorData.m_dbroot.m_tables[tableOrder[selectedIndex + 1]]->m_loadOrder
            );
            s_editorData.m_dbroot.SaveDBRoot();
        }
        ShowToolTip("Move this table down in loading order.", false);
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

    table.Load(src.generic_string().c_str());
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
    table.Load(src.generic_string().c_str());

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
    table.Load(src.generic_string().c_str());

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

    // make a dummy file
    {
        FILE* file = nullptr;
        fopen_s(&file, fileName.generic_string().c_str(), "wb");
        if (!file)
            return;

        fprintf(file, "{\n}\n");
        fclose(file);
    }

    // make the entry in the data
    table.LoadFile(fileName.generic_string().c_str());

    // select the new item
    s_editorData.m_selectedDataItemName = itemName;
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

    // Select the first item in the table
    for (const auto& pair : table.m_data)
    {
        s_editorData.m_selectedDataItemName = pair.first;
        break;
    }
}

static void ShowDataList()
{
    {
        ImGui_Enabled enabled(s_editorData.m_dbroot.m_tables.contains(s_editorData.m_selectedTableName));

        if (ImGui::Button("New"))
            OnDataListNew();
        ImGui::SameLine();
        if (ImGui::Button("Delete"))
            OnDataListDelete();
    }

    //ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f, 0.10f, 0.10f, 1.0f));
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

    //ImGui::PopStyleColor();

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

void HandleConfirmNew()
{
    if (s_editorData.m_showConfirmNew)
    {
        ImGui::OpenPopup("Create New Database?");
        s_editorData.m_showConfirmNew = false;
    }

    if (ImGui::BeginPopupModal("Create New Database?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Data is unsaved, continue?");

        ImGui::Separator();

        if (ImGui::Button("OK", ImVec2(120, 0)))
        {
            OnFileNewDatabase(false);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}

void HandleCompileResults()
{
    if (s_editorData.m_showCompileResultsWindow)
    {
        ImGui::OpenPopup("Compile Finished");
        s_editorData.m_showCompileResultsWindow = false;
    }

    if (ImGui::BeginPopupModal("Compile Finished", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (s_editorData.m_compileSucceeded)
            ImGui::Text(" Data Compilation Succeeded");
        else
            ImGui::Text(ICON_FA_CIRCLE_EXCLAMATION " Data Compilation Failed");

        if (!s_editorData.m_compileOutput.empty())
            ImGui::TextUnformatted(s_editorData.m_compileOutput.c_str());

        ImGui::Separator();

        if (ImGui::Button("OK", ImVec2(120, 0)))
            ImGui::CloseCurrentPopup();

        ImGui::SetItemDefaultFocus();

        ImGui::EndPopup();
    }
}

void HandleSettingsWindow()
{
    static std::vector<DBCompileSettings> settings;

    if (s_editorData.m_openSettingsWindow)
    {
        ImGui::OpenPopup("Settings");
        s_editorData.m_openSettingsWindow = false;
        settings = s_editorData.m_dbroot.m_compileSettings;
    }

    if (ImGui::BeginPopupModal("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        // Make sure there is always at least one default row
        if (settings.size() == 0)
            settings.resize(1);

        static std::vector<char> tmpBuffer;
        tmpBuffer.resize(4096);

        int deleteIndex = -1;
        int index = -1;
        for (DBCompileSettings& s : settings)
        {
            index++;
            bool treeOpened = ImGui::TreeNodeEx(std::to_string(index + 1).c_str(), ImGuiTreeNodeFlags_DefaultOpen);

            ImGui::SameLine();
            if (ImGui::SmallButton("Delete"))
                deleteIndex = index;

            if (!treeOpened)
                continue;

            // Compile Output Header File
            strcpy_s(tmpBuffer.data(), tmpBuffer.size(), s.compiledHeaderFileName.c_str());
            if (ImGui::InputText("Compile Output Header", tmpBuffer.data(), tmpBuffer.size()))
                s.compiledHeaderFileName = tmpBuffer.data();
            ShowToolTip("Where to put the generated .h file.");

            // Compile Output Bin File
            strcpy_s(tmpBuffer.data(), tmpBuffer.size(), s.compiledBinFileName.c_str());
            if (ImGui::InputText("Compile Output Bin", tmpBuffer.data(), tmpBuffer.size()))
                s.compiledBinFileName = tmpBuffer.data();
            ShowToolTip("Where to put the compiled data .bin file.");

            // Namespace
            strcpy_s(tmpBuffer.data(), tmpBuffer.size(), s.className.c_str());
            if (ImGui::InputText("Class Name", tmpBuffer.data(), tmpBuffer.size()))
                s.className = tmpBuffer.data();
            ShowToolTip("The name of the object in the generated header file");

            // Entry LUT
            ImGui::Checkbox("Entry Look Up Table", &s.includeEntryLUT);
            ShowToolTip("If true, the generated header will have functions to look up table entries by name\n"
                        "by using look up tables in the bin file.  If you don't look up table entries by name,\n"
                        "turning this off makes entry names not appear in the bin file, which can help deter\n"
                        "casual data editing by users.\n"
                        "Hot reloading requires this to be on.");

            ImGui::TreePop();
        }

        // delete a row if we should
        if (deleteIndex >= 0)
            settings.erase(settings.begin() + deleteIndex);

        if (ImGui::Button("Add"))
            settings.emplace_back();


        ImGui::Separator();

        if (ImGui::Button("OK", ImVec2(120, 0)))
        {
            s_editorData.m_dbroot.m_compileSettings = settings;
            s_editorData.m_dbroot.SaveDBRoot();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}

bool ShowEditorWindow()
{
    if (s_loadCommandLine)
    {
        LoadFile(s_commandLineFileName.c_str());
        s_loadCommandLine = false;
    }

    bool ret = false;

    if (s_editorData.m_updateWindowTitle)
    {
        char buffer[2048];
        const char* path = s_editorData.m_dbroot.GetPath();
        if (path && path[0])
            sprintf_s(buffer, "df_game_data Editor - %s%s", std::filesystem::path(path).filename().generic_string().c_str(), s_editorData.m_documentDirty ? " *" : "");
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

    HandleSettingsWindow();
    HandleCompileResults();
    HandleConfirmNew();

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

    static bool showLoadingErrors = false;
    if (s_editorData.m_showLoadingErrors)
    {
        s_editorData.m_showLoadingErrors = false;
        showLoadingErrors = true;
        ImGui::OpenPopup("Loading Error");
    }

    if (ImGui::BeginPopupModal("Loading Error", &showLoadingErrors, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Error:\n%s", s_editorData.m_dbroot.GetErrorText());

        ImGui::Separator();

        if (ImGui::Button("OK", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
            showLoadingErrors = false;
        }

        ImGui::EndPopup();
    }

    return ret;
}

void OnFileDragDropped(const wchar_t* path)
{
    LoadFile(std::filesystem::path(path).generic_string().c_str());
}

bool EditorOnAppLaunch(int argc, char** argv, int &returnCode)
{
    returnCode = 0;

    bool wantsCompile = false;
    if (argc > 1)
    {
        s_commandLineFileName = argv[1];
        if (argc > 2 && (!_stricmp(s_commandLineFileName.c_str(), "-c") || !_stricmp(s_commandLineFileName.c_str(), "--compile")))
        {
            s_commandLineFileName = argv[2];
            wantsCompile = true;
        }
    }
    s_loadCommandLine = !s_commandLineFileName.empty();

    if (wantsCompile)
    {
        LoadFile(s_commandLineFileName.c_str());

        if (s_editorData.m_showLoadingErrors)
        {
            printf("Error: %s", s_editorData.m_dbroot.GetErrorText());
            returnCode = 1;
            return false;
        }

        if (!DoCompile())
        {
            printf("Error: could not compile data");
            if (!s_editorData.m_compileOutput.empty())
                printf("%s\n", s_editorData.m_compileOutput.c_str());
            returnCode = 1;
            return false;
        }
    }

    return !wantsCompile;
}

/*
TODO:

* the example data needs a small c++ main.cpp that loads the data and prints something from it.
 * ExampleData\example_code\main.cpp when it's time to do this again.

 ! look for TODOs

* runtime file watching:
 * Have a get() function on an entry pointer which returns an object.
 * Internally checks load version # to see if it needs to look entry up again by name.
 * If entry not found by name return default object with a invalid flag thay can be checked.
 * What if schema hash changes? Maybe need a way to detect that incompatibility? Or does flatbuffer handle that with backwards and forwards compatibility?
 * maybe still need to check for massive schema changes that don't follow the rules of what's allowed for flatbuffer loading to continue working

* use it a bit before announcing it and making builds available
! make a blog post about the binary serialization stack. static size go in stack N, dynamic is written into N+1 and a pointer is written into N.
 * could also talk about the multi pass approach, for final pointer fixup
 * also talk about pointer fixup on load

* Make a script to make binary releases.
* installer
 * with option to add to path (for binary compilation)
 * nah. just binaries i think?
* Also need version number in app and installer.
* add contributors list and how to contribute
* Let people Add their name. Alpha sort. Along with a description of what they did?
 * Or a link to a page with their check ins or something.
* Add a help about with version and contributor list.
 * could also put the larger df.png on there
* make flatc.exe get copied to where the editor exe is, on compile and make it .gitignored. call it from there. needed for binaries / installer?

Contributing:
* I can't tell whether you made a contribution with AI or not, other than by the quality of the work.
* poor quality changes will be met with guidance. If the guidance isn't fruitful, the changes will be rejected.
* There are tasks up for grab in the issues list

Schema documentation:
* Tries to be familiar to C++ programmers, the target user
* #include
* #root
* struct {};
* enum {};
* types
* dynamic / fixed arrays
* how namespaces work (including types using :: syntax)
 * if no namespace on a type, first searches current namespace, then global namespace
 * if there is a namespace on a type, only searches that namespace.

Notes:
* This works as a flatbuffer data editor too (can open def or dbroot files)
 * not quite. a json editor where the schema is defined as flatbuffers.
* explain the design decisions (each data item as a json data file for easier merging. flat tables for speed. multiple tables because that's whats needed. table links)
* Explain how to use it
* mention drag and drop working
* if you use table links, the table you reference must come before the current table in the dbroot list
* don't use namespace in your files, but you can put a namespace in the settings.
* C++ is the main target language - that's what i use it for! - but other languages are supported
* explain how to use enums to look up items by name (and string to enum to do a lookup by string name in some languages?)
 * string lookup doesnt work in c++ though. maybe need to add it.
 * explain that it makes an enum for the entry_names
* explain the simple interface (only use generated headers), and the one that does file watching.
* explain how order of tables in the db can affect things. table must come before things use types from that table (like, table links)
* explain you can move up and down the tables in the dbroot.
 * useful if one table schema defines types used by another table schema
 * better to have a shared schema include file though.
* command line options:
 * put a filename on command line to load it
 * If there is a -c or --compile before it, compiles the data file without making a window

* compile options can change the contents of the bin file, which affects whether a header can load it or not.
* hot reloading requires the entry LUT

! make a way for people to support the project with $ if they want to

*/
