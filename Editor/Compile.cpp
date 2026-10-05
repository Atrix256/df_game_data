#include "Compile.h"
#include "../loader/loader.h"
#include "../loader/hash.h"
#include "../Version.h"

extern bool MakeCPP(const DBCompileSettings& compilerSettings, const DBRoot& dbRoot, const char* fileName, uint64_t hash, std::string& error);
extern bool MakeBin(const DBCompileSettings& compilerSettings, const DBRoot& dbRoot, const char* fileName, uint64_t hash, std::string& error);

bool Compile(const DBRoot& dbRoot, const DBCompileSettings& compilerSettings_, std::string& error)
{
    // Hot reloading requires the entry LUT
    DBCompileSettings compilerSettings = compilerSettings_;
    if (compilerSettings.hotReloading)
        compilerSettings.includeEntryLUT = true;

    if (compilerSettings.className.empty())
        compilerSettings.className = "dfgd";

    if (dbRoot.m_tables.size() == 0)
    {
        error = "No tables in database";
        return false;
    }

    // calculate schema hash
    Hasher hash(0xbeefcafe);
    hash.Add(VERSION_MAJOR);
    hash.Add(VERSION_MINOR);
    hash.Add(VERSION_PATCH);
    for (auto& it : dbRoot.m_tables)
        hash.Add(it.second->GetParser().GetHash());
    hash.Add(compilerSettings.includeEntryLUT);
    hash.Add(compilerSettings.obfuscation);

    std::string dbRootPath = std::filesystem::path(dbRoot.GetPath()).remove_filename().generic_string();

    std::string fileNameBin = std::filesystem::weakly_canonical(std::filesystem::path(dbRootPath) / compilerSettings.compiledBinFileName).generic_string();
    std::string fileNameHeader = std::filesystem::weakly_canonical(std::filesystem::path(dbRootPath) / compilerSettings.compiledHeaderFileName).generic_string();

    bool ret =
        MakeBin(compilerSettings, dbRoot, fileNameBin.c_str(), hash.Result(), error) &&
        MakeCPP(compilerSettings, dbRoot, fileNameHeader.c_str(), hash.Result(), error);

    return ret;
}
