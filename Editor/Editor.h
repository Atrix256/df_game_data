#pragma once

#include "../loader/loader.h"
#include <string>
#include "RecentFiles.h"

struct Settings
{
    std::string compileOutputDir;
};

struct EditorData
{
    EditorData()
        : m_recentFiles("Software\\df_game_data")
    {
        m_recentFiles.LoadAllEntries();
    }

    Settings m_settings;

    DBRoot m_dbroot;
    std::string m_selectedTableName;
    std::string m_selectedDataItemName;

    bool m_documentDirty = false;
    bool m_updateWindowTitle = true;
    bool m_showLoadingErrors = false;
    bool m_openSettingsWindow = false;

    bool m_showCompileResultsWindow = false;
    bool m_compileSucceeded = false;

    RecentFiles m_recentFiles;
};

bool ShowEditorWindow();
void OnFileDragDropped(const wchar_t* path);
