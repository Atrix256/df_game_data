#pragma once

#include <vector>
#include <string>

#include "JSON.h"

#include "FileWatcher.h"

#include "DefParser.h"

struct DBCompileSettings
{
    std::string compiledHeaderFileName = "out.h";
    std::string compiledBinFileName = "out.bin";
    std::string nameSpace;
    // if true, includes a LUT that maps table entries to names, sorted by name.
    bool includeEntryLUT = true;
};

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

    bool LoadFile(const char* fileName);

    const DefParser& GetParser() const
    {
        return m_parser;
    }

public:
    struct JSONData
    {
        std::string m_path;
        json m_data;
        bool m_dirty = false;
    };

    std::map<std::string, std::unique_ptr<JSONData>> m_data;
    std::string m_rootType;
    int m_loadOrder = 0;

private:
    bool LoadSchema();
    bool LoadData();

private:
    std::string m_path;
    std::string m_errorText;

    DefParser m_parser;
};

class DBRoot
{
public:
    bool Load(const char* path);
    bool New(const char* path);
    bool AddTable(const char* path);
    bool RemoveTable(const char* name);
    void SaveDBRoot();

    void Clear();

    void SetErrorText(const char* text)
    {
        m_errorText = text;
    }

    const char* GetErrorText() const
    {
        return m_errorText.c_str();
    }

    const char* GetPath() const
    {
        return m_path.c_str();
    }

    bool Loaded() const
    {
        return !m_path.empty();
    }

private:
    void LoadSettings(json& data);

public:
    std::map<std::string, std::unique_ptr<DBTable>> m_tables;
    std::vector<DBCompileSettings> m_compileSettings;

private:
    std::string m_errorText;

    FileWatcher m_fileWatcher;

    std::string m_path;
};

bool LoadTextFile(const char* fileName, std::string& contents);
