#include "Compile.h"

#include "flatbuffers/idl.h"
#include "../loader/JSON.h"
#include "Editor.h"

extern bool RunFlatc(const char* args, bool waitForExit);

std::filesystem::path ApplyPath(const std::filesystem::path& base, const std::filesystem::path& input)
{
    if (input.is_absolute())
        return input;
    return base / input;
}

bool CompileData(EditorData& editorData)
{
    bool ret = true;

    std::filesystem::path dbRootPath = std::filesystem::path(editorData.m_dbroot.GetPath()).remove_filename();

    std::string outputDir = ApplyPath(dbRootPath, editorData.m_settings.compileOutputDir).generic_string();

    if (editorData.m_dbroot.m_tables.count(editorData.m_selectedTableName) > 0)
    {
        DBTable& table = *editorData.m_dbroot.m_tables[editorData.m_selectedTableName].get();

        if (table.m_data.count(editorData.m_selectedDataItemName) > 0)
        {
            DBTable::JSONData& data = *table.m_data[editorData.m_selectedDataItemName].get();

            // Make the generated header
            {
                std::string commandLine = "--cpp -o \"" + outputDir + "\" \"" + std::string(table.GetPath()) + "\"";
                ret |= RunFlatc(commandLine.c_str(), false);
            }

            // Make a binary file
            {
                std::string commandLine = "-b -o \"" + outputDir + "\" \"" + std::string(table.GetPath()) + "\" \"" + data.m_path + "\"";
                ret |= RunFlatc(commandLine.c_str(), false);
            }
        }
    }
    return ret;
}
