#include "Compile.h"
#include "Compile_Bin.h"
#include "Compile_CPP.h"
#include "Compile_C.h"
#include "../loader/loader.h"
#include "../loader/hash.h"
#include "../Version.h"

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
    hash.Add(compilerSettings.codeGenLanguage);

    std::string dbRootPath = std::filesystem::path(dbRoot.GetPath()).remove_filename().generic_string();

    std::string fileNameBin = std::filesystem::weakly_canonical(std::filesystem::path(dbRootPath) / compilerSettings.compiledBinFileName).generic_string();
    std::string fileNameHeader = std::filesystem::weakly_canonical(std::filesystem::path(dbRootPath) / compilerSettings.compiledHeaderFileName).generic_string();

    // make binary file
    if (!MakeBin(compilerSettings, dbRoot, fileNameBin.c_str(), hash.Result(), error))
        return false;

    // make code file
    bool ret = true;
    switch (compilerSettings.codeGenLanguage)
    {
        case CodeGenLanguage::CPP: ret = MakeCPP(compilerSettings, dbRoot, fileNameHeader.c_str(), hash.Result(), error); break;
        case CodeGenLanguage::C:ret = MakeC(compilerSettings, dbRoot, fileNameHeader.c_str(), hash.Result(), error); break;
        default:
        {
            error = "Unsupported code generation language";
            return false;
        }
    }

    return ret;
}
