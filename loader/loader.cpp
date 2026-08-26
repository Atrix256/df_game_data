#include "loader.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <filesystem>

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"

bool DBTable::Load(const char* path)
{
    m_path = path;

    // open the file
    FILE* file = nullptr;
    fopen_s(&file, path, "rb");
    if (!file)
    {
        m_errorText = "Failed to open db file: " + std::string(path);
        return false;
    }

    // load the entire file into memory
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    std::vector<char> buffer(fileSize + 1);
    fread(buffer.data(), 1, fileSize, file);
    buffer[fileSize] = 0;
    fclose(file);

    rapidjson::Document document;
    document.Parse(buffer.data());
    if (document.HasParseError())
    {
        const char* errorString = rapidjson::GetParseError_En(document.GetParseError());
        size_t errorOffset = document.GetErrorOffset();
        m_errorText = "Failed to parse db file: " + std::string(path) + "\nError: " + std::string(errorString) + "\nOffset: " + std::to_string(errorOffset);
        return false;
    }

    return true;
}

bool DBRoot::Load(const char* path)
{
    Clear();

    // Load m_tables
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            m_errorText = "Failed to open dbroot file: " + std::string(path);
            return false;
        }

        std::filesystem::path base_path = std::filesystem::absolute(path).remove_filename();

        std::string line;
        while (std::getline(file, line))
        {
            std::filesystem::path full_path = base_path / line;

            DBTable& newTable = m_tables.emplace_back();
            if (!newTable.Load(full_path.string().c_str()))
            {
                m_errorText = newTable.GetErrorText();
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
