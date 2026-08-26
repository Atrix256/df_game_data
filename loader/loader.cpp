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

bool DBTable::EnsureBFBSExists()
{
    // Find out when the input file was last modified
    std::error_code ec;
    auto inFileTime = std::filesystem::last_write_time(m_path, ec);
    if (ec)
    {
        m_errorText = "Failed to get last write time for db file: " + m_path + "\nError: " + ec.message();
        return false;
    }

    // remake the .bfbs if it doesn't exist or is stale
    std::filesystem::path outFileName = std::filesystem::path(m_path).replace_extension(".bfbs");
    auto outFileTime = std::filesystem::last_write_time(outFileName, ec);
    if (ec || inFileTime > outFileTime)
    {
        std::filesystem::path outDir = std::filesystem::path(m_path).remove_filename();
        std::ostringstream command;
        command << ".\\flatc.exe -b --schema -o " << outDir << " " << m_path;
        LaunchProcessResult result = LaunchProcess(command.str().c_str());
        if (result.exitCode != 0)
        {
            m_errorText = "Failed to run flatc.exe on db file: " + m_path + "\nExit code: " + std::to_string(result.exitCode) + "\nOutput:\n" + result.output;
            return false;
        }
    }

    return true;
}

bool DBTable::Load(const char* path)
{
    m_path = path;

    // make sure the .bfbs file exists
    if (!EnsureBFBSExists())
        return false;

    // Load the binary bfbs file
    std::filesystem::path outFileName = std::filesystem::path(m_path).replace_extension(".bfbs");
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

    //enums->Get(0)->values()->Get(0).
    //objects->Get(0)->fields()->Get(0)->type()

    // Find the root type by looking for a custom attribute "root_type"
    for (flatbuffers::uoffset_t i = 0; i < objects->size(); ++i)
    {
        const reflection::Object* obj = objects->Get(i);
        if (!obj || !obj->attributes())
            continue;

        // Loop through the KeyValue attributes on this object/table
        for (flatbuffers::uoffset_t j = 0; j < obj->attributes()->size(); ++j)
        {
            auto attr = obj->attributes()->Get(j);
            if (attr && attr->key() && attr->key()->str() == "root_type")
            {
                int ijkl = 0;
            }
        }
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

/*
Schemas...

? how to find the root type?
 1) only allow 1 table, and it's the table (not great limiting usage)
 2) Match name of a table to the name of the db (from filename maybe? idk)
 3) Custom attribute.

Yeah, custom attribute!
Near top of file: (maybe you inject this, actually, when loading the file)
attribute "root_type";

Then:
struct Vec3 (root_type) {
  x:float;
  y:float;
  z:float;
}

Then find that custom attribute on a struct or table.
Put this in the instructions

TODO:
* make it add the "root_type" attribute automatically. Will have to move the file over to a tmp directory and work there and copy it back.
* use namespaces feature in the schema?
* use includes feature in the schema, or at least allow it and show it as part of the example code

*/
