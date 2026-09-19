#include "Compile.h"

#include "Editor.h"

#include <sstream>

static std::ostringstream s_error;

static bool MakeHeader(EditorData& editorData, const char* fileName)
{
    // TODO: this
    s_error << "Writing header not yet implemented";
    return false;
}

static bool MakeBin(EditorData& editorData, const char* fileName)
{
    FILE* file = nullptr;
    fopen_s(&file, fileName, "wb");
    if (!file)
    {
        s_error << "Could not write to " << fileName;
        return false;
    }

    for (auto& table : editorData.m_dbroot.m_tables)
    {
        int ijkl = 0;
    }

    fclose(file);

    s_error << "Writing bin not yet implemented";
    return false;
}

bool Compile(EditorData& editorData, std::string& error)
{
    s_error = std::ostringstream();

    if (editorData.m_dbroot.m_tables.size() == 0)
    {
        s_error << "No tables in database";
        error = s_error.str();
        return false;
    }

    std::string dbRootPath = std::filesystem::path(editorData.m_dbroot.GetPath()).remove_filename().generic_string();

    std::string fileNameBin = std::filesystem::weakly_canonical(std::filesystem::path(dbRootPath) / editorData.m_dbroot.m_settings.compiledBinFileName).generic_string();
    std::string fileNameHeader = std::filesystem::weakly_canonical(std::filesystem::path(dbRootPath) / editorData.m_dbroot.m_settings.compiledHeaderFileName).generic_string();

    bool ret = MakeBin(editorData, fileNameBin.c_str()) && MakeHeader(editorData, fileNameHeader.c_str());

    error = s_error.str();
    return ret;
}

/*
TODO:
* can we make editor data const when it's passed in?
*/
