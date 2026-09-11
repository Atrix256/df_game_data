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
    bool m_showLoadingErrors = false;

    RecentFiles m_recentFiles;
};

bool ShowEditorWindow();
