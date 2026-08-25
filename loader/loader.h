#pragma once

#include <vector>
#include <string>

class DBRoot
{
public:
    bool Load(const char* path);

    void Clear();

    const std::vector<std::string>& GetTablePaths() const
    {
        return m_tablePaths;
    }

private:

    std::vector<std::string> m_tablePaths;
};
