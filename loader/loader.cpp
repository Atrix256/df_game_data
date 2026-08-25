#include "loader.h"
#include <iostream>
#include <fstream>
#include <algorithm>

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
            m_tablePaths.push_back(line);

        std::sort(m_tablePaths.begin(), m_tablePaths.end());

        file.close();
    }

    return true;
}

void DBRoot::Clear()
{
    m_tablePaths.clear();
}
