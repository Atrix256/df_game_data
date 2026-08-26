#include "loader.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <filesystem>

#include "flatbuffers/flatbuffers.h"
#include "flatbuffers/idl.h"
#include "flatbuffers/util.h"
#include "flatbuffers/reflection.h"

#include "LaunchProcess.h"

bool DBTable::Load(const char* path)
{
    m_path = path;

    // Find out when the input file was last modified
    std::error_code ec;
    auto inFileTime = std::filesystem::last_write_time(path, ec);
    if (ec)
    {
        m_errorText = "Failed to get last write time for db file: " + std::string(path) + "\nError: " + ec.message();
        return false;
    }

    // remake the .bfbs if it doesn't exist or is stale
    std::filesystem::path outFileName = std::filesystem::path(path).replace_extension(".bfbs");
    auto outFileTime = std::filesystem::last_write_time(outFileName, ec);
    if (ec || inFileTime > outFileTime)
    {
        std::filesystem::path outDir = std::filesystem::path(path).remove_filename();
        std::ostringstream command;
        command << ".\\flatc.exe -b --schema -o " << outDir << " " << path;
        LaunchProcessResult result = LaunchProcess(command.str().c_str());
        if (result.exitCode != 0)
        {
            m_errorText = "Failed to run flatc.exe on db file: " + std::string(path) + "\nExit code: " + std::to_string(result.exitCode) + "\nOutput:\n" + result.output;
            return false;
        }
    }

    // Load the binary bfbs file
    std::string bfbs_file_contents;
    bool success = flatbuffers::LoadFile(outFileName.string().c_str(), true, &bfbs_file_contents);
    if (!success)
    {
        m_errorText = "Failed to load bfbs file: " + outFileName.string();
        return false;
    }

    // Get the items from the schema
    const reflection::Schema& schema = *reflection::GetSchema(bfbs_file_contents.c_str());
    auto objects = schema.objects();
    auto enums = schema.enums();

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
            std::filesystem::path full_path = std::filesystem::weakly_canonical(base_path / line);

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
