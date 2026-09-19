#pragma once

#include "../loader/loader.h"
#include <string>
#include "RecentFiles.h"

struct EditorData
{
    EditorData()
        : m_recentFiles("Software\\df_game_data")
    {
        m_recentFiles.LoadAllEntries();
    }

    DBRoot m_dbroot;
    std::string m_selectedTableName;
    std::string m_selectedDataItemName;

    bool m_documentDirty = false;
    bool m_updateWindowTitle = true;
    bool m_openSettingsWindow = false;

    bool m_showLoadingErrors = false;
    bool m_showCompileResultsWindow = false;
    bool m_compileSucceeded = false;
    bool m_showConfirmNew = false;

    std::string m_compileOutput;

    RecentFiles m_recentFiles;
};

bool EditorOnAppLaunch(int argc, char** argv, int& returnCode);
bool ShowEditorWindow();
void OnFileDragDropped(const wchar_t* path);
