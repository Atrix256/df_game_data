#include "Compile.h"

#include "Editor.h"

#include <sstream>

static std::ostringstream s_error;

static bool MakeHeader(EditorData& editorData, const char* fileName, size_t hash)
{
    // TODO: this
    s_error << "Writing header not yet implemented";
    return false;
}

static bool MakeBin_Table_Item(EditorData& editorData, DBTable& table, json& json, FILE* file)
{
    // TODO: continue
    //s_error << "Writing bin not yet implemented";
    //return false;
    return true;
}

static bool MakeBin_Table(EditorData& editorData, DBTable& table, FILE* file)
{
    // Write how many items are in this table
    uint32_t numItems = (uint32_t)table.m_data.size();
    fwrite(&numItems, sizeof(numItems), 1, file);

    bool ret = true;
    for (auto& it : table.m_data)
    {
        ret &= MakeBin_Table_Item(editorData, table, it.second->m_data, file);
        if (!ret)
            break;
    }

    return ret;
}

static bool MakeBin(EditorData& editorData, const char* fileName, size_t hash)
{
    FILE* file = nullptr;
    fopen_s(&file, fileName, "wb");
    if (!file)
    {
        s_error << "Could not write to " << fileName;
        return false;
    }

    // Write schema hash
    fwrite(&hash, sizeof(hash), 1, file);

    bool ret = true;
    for (auto& it : editorData.m_dbroot.m_tables)
    {
        ret &= MakeBin_Table(editorData, *it.second.get(), file);
        if (!ret)
            break;
    }

    fclose(file);

    return ret;
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

    // calculate schema hash
    size_t hash = 0xbeefcafe;
    for (auto& it : editorData.m_dbroot.m_tables)
        hash_combine(hash, it.second->GetParser().GetHash());

    std::string dbRootPath = std::filesystem::path(editorData.m_dbroot.GetPath()).remove_filename().generic_string();

    std::string fileNameBin = std::filesystem::weakly_canonical(std::filesystem::path(dbRootPath) / editorData.m_dbroot.m_settings.compiledBinFileName).generic_string();
    std::string fileNameHeader = std::filesystem::weakly_canonical(std::filesystem::path(dbRootPath) / editorData.m_dbroot.m_settings.compiledHeaderFileName).generic_string();

    bool ret = MakeBin(editorData, fileNameBin.c_str(), hash) && MakeHeader(editorData, fileNameHeader.c_str(), hash);

    error = s_error.str();
    return ret;
}

/*
TODO:
* can we make editor data const when it's passed in?
* may want to move the bin and header code into separate files for organization purposes
*/
