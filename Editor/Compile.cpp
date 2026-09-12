#include "Compile.h"

#include "flatbuffers/idl.h"
#include "../loader/JSON.h"
#include "Editor.h"

extern bool RunFlatc(const char* args, bool waitForExit);

void CompileData(EditorData& editorData)
{

    if (editorData.m_dbroot.m_tables.count(editorData.m_selectedTableName) > 0)
    {
        DBTable& table = *editorData.m_dbroot.m_tables[editorData.m_selectedTableName].get();

        if (table.m_data.count(editorData.m_selectedDataItemName) > 0)
        {
            DBTable::JSONData& data = *table.m_data[editorData.m_selectedDataItemName].get();

            // Make the generated header
            {
                std::string commandLine = "--cpp " + std::string(table.GetPath());
                RunFlatc(commandLine.c_str(), false);
            }

            // Make a binary file
            {
                std::string commandLine = "-b " + std::string(table.GetPath()) + " " + data.m_path;
                RunFlatc(commandLine.c_str(), false);
            }
        }
    }
}