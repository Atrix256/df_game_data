#include "Compile.h"

#include "Editor.h"

static bool MakeHeader(EditorData& editorData, const char* fileName)
{
    // TODO: this
    return false;
}

static bool MakeBin(EditorData& editorData, const char* fileName)
{
    for (auto& table : editorData.m_dbroot.m_tables)
    {

    }
    // TODO: this
    return false;
}

bool Compile(EditorData& editorData)
{
    if (editorData.m_dbroot.m_tables.size() == 0)
        return false;

    std::string dbRootPath = std::filesystem::path(editorData.m_dbroot.GetPath()).remove_filename().generic_string();

    std::string fileNameBin = std::filesystem::weakly_canonical(std::filesystem::path(dbRootPath) / editorData.m_dbroot.m_settings.compiledBinFileName).generic_string();
    std::string fileNameHeader = std::filesystem::weakly_canonical(std::filesystem::path(dbRootPath) / editorData.m_dbroot.m_settings.compiledHeaderFileName).generic_string();

    return MakeBin(editorData, fileNameBin.c_str()) && MakeHeader(editorData, fileNameHeader.c_str());
}

/*
TODO:
* can we make editor data const when it's passed in?
* make a default for the output filenames. like if they are empty i guess?
*/
