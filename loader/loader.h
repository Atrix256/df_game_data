#pragma once

#include <vector>
#include <string>

#include "flatbuffers/idl.h"

#include <nlohmann/json.hpp>
using json = nlohmann::ordered_json;

#include "FileWatcher.h"

class DBTable
{
public:
    bool Load(const char* path);

    const char* GetPath() const
    {
        return m_path.c_str();
    }

    const char* GetName() const
    {
        return m_rootType.c_str();
    }

    const char* GetErrorText() const
    {
        return m_errorText.c_str();
    }

public:
    std::unordered_map<std::string, std::unique_ptr<json>> m_data;

private:
    bool LoadSchema();
    bool LoadData();
    bool LoadFile(const char* fileName);

private:
    std::string m_path;
    std::string m_errorText;

    std::string m_rootType;

    std::string m_fbsFile;
    std::vector<std::string> m_includeDirsStr;
    std::vector<const char*> m_includeDirs;

    // Parser needs all inputs to last as long as it lasts, so:
    // 1) Everything it needs is a member
    // 2) It is last in the class, to be destructed first
    std::unique_ptr<flatbuffers::Parser> m_parser;
};

class DBRoot
{
public:
    bool Load(const char* path);

    void Clear();

    const char* GetErrorText() const
    {
        return m_errorText.c_str();
    }

public:
    std::vector<DBTable> m_tables;

private:
    std::string m_errorText;

    FileWatcher m_fileWatcher;
};
