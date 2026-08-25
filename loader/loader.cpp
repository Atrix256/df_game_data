#include "loader.h"
#include <iostream>
#include <fstream>
#include <algorithm>

bool DBTable::Load(const char* path)
{
    m_path = path;
    return true;
}

bool DBRoot::Load(const char* path)
{
    Clear();

    // Load m_tablePaths
    {
        std::ifstream file(path);
        if (!file.is_open())
            return false;

        std::string line;
        while (std::getline(file, line))
        {
            DBTable& newTable = m_tables.emplace_back();
            if (!newTable.Load(line.c_str()))
            {
                Clear();
                file.close();
                return false;
            }
        }

        std::sort(m_tables.begin(), m_tables.end(), [](const DBTable& a, const DBTable& b) {
            return strcmp(a.GetPath(), b.GetPath()) < 0;
        });

        file.close();
    }

    return true;
}

void DBRoot::Clear()
{
    m_tables.clear();
}
