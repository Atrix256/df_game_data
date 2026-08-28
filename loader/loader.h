#pragma once

#include <vector>
#include <string>

#include "flatbuffers/idl.h"

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
    bool LoadSchema();
    bool LoadData();

private:
    flatbuffers::Parser m_parser;
    std::string m_path;
    std::string m_errorText;

    // TODO: sort this out. it's a problem with parser error causing parser to be fubar internally.
    // parser needs all inputs to last as long as it lasts
    inline static std::string m_fbsFile;
    inline static std::vector<std::string> m_includeDirsStr;
    inline static std::vector<const char*> m_includeDirs;
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
