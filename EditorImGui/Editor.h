#pragma once

#include "../loader/loader.h"
#include <string>

struct EditorData
{
    DBRoot m_dbroot;
    std::string m_selectedTableName;
    std::string m_selectedDataItemName;

    bool m_documentDirty = false;
    bool m_updateWindowTitle = true;

    void MarkDirty()
    {
        m_documentDirty = true;
        m_updateWindowTitle = true;
    }
};

bool ShowEditorWindow();
