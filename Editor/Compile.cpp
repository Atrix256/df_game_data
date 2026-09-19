#include "Compile.h"

#include "Editor.h"

static bool MakeHeader(EditorData& editorData)
{
    // TODO: this
    return false;
}

static bool MakeBin(EditorData& editorData)
{
    // TODO: this
    return false;
}

bool Compile(EditorData& editorData)
{
    if (editorData.m_dbroot.m_tables.size() == 0)
        return false;

    return MakeBin(editorData) && MakeHeader(editorData);
}
