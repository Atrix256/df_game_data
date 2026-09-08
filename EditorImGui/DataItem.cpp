#include "DataItem.h"

#include "flatbuffers/idl.h"
#include "../loader/JSON.h"
#include "Editor.h"

static void AddUIForType(const flatbuffers::Parser& parser, const flatbuffers::StructDef& structDef, const char* structFieldName, json& json, const json_pointer& path)
{
    int ijkl = 0;
}

void ShowDataEditor(EditorData& editorData)
{
    if (editorData.m_dbroot.m_tables.count(editorData.m_selectedTableName) == 0)
        return;
    DBTable& table = *editorData.m_dbroot.m_tables[editorData.m_selectedTableName].get();

    if (table.m_data.count(editorData.m_selectedDataItemName) == 0)
        return;
    DBTable::JSONData& data = *table.m_data[editorData.m_selectedDataItemName].get();

    const flatbuffers::Parser& parser = table.GetParser();

    AddUIForType(parser, *parser.root_struct_def_, parser.root_struct_def_->name.c_str(), data.m_data, json_pointer(""));
}
