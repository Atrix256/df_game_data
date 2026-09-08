#pragma once

#include "../loader/loader.h"
#include <string>

struct EditorData
{
    DBRoot m_dbroot;
    std::string m_selectedTableName;
    std::string m_selectedDataItemName;
};

bool ShowEditorWindow();
