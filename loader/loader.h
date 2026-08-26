#pragma once

#include <vector>
#include <string>

class DBTable
{
public:
    bool Load(const char* path);

    const char* GetPath() const
    {
        return m_path.c_str();
    }

    const char* GetErrorText() const
    {
        return m_errorText.c_str();
    }

private:
    std::string m_path;
    std::string m_errorText;
};

class DBRoot
{
public:
    bool Load(const char* path);

    void Clear();

    const std::vector<DBTable>& GetTables() const
    {
        return m_tables;
    }

    const char* GetErrorText() const
    {
        return m_errorText.c_str();
    }

private:
    std::vector<DBTable> m_tables;
    std::string m_errorText;
};
