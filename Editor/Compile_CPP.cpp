#include "Compile_CPP.h"

#include "../loader/loader.h"
#include "../Version.h"
#include "../Utils.h"
#include "../loader/output_cpp_h_string.h"

#include <unordered_set>

namespace
{
    struct StaticData
    {
        std::ostringstream error;

        // A map to replace tokens in output.h with strings
        std::unordered_map<std::string, std::ostringstream> tokenReplacement;
    };
    static StaticData s_data;
}

inline static void StringReplaceAll(std::string& str, const std::string& from, const std::string& to)
{
    if (from.empty())
        return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

// Helper function to say if a struct is any table's root struct
static bool IsARootStruct(const DBRoot& dbRoot, const char* name)
{
    for (const auto& pair : dbRoot.m_tables)
    {
        if (pair.second->GetParser().GetRootStructName() == name)
            return true;
    }

    return false;
}

static const bool FieldToCPPType(const DefParser::Struct& s, const DefParser::StructField& field, std::string& typeName)
{
    switch (field.fieldType)
    {
        case DefParser::FieldType::_bool: typeName = "Bool"; break;
        case DefParser::FieldType::_uint8: typeName = "uint8_t"; break;
        case DefParser::FieldType::_sint8: typeName = "int8_t"; break;
        case DefParser::FieldType::_uint16: typeName = "uint16_t"; break;
        case DefParser::FieldType::_sint16: typeName = "int16_t"; break;
        case DefParser::FieldType::_uint32: typeName = "uint32_t"; break;
        case DefParser::FieldType::_sint32: typeName = "int32_t"; break;
        case DefParser::FieldType::_uint64: typeName = "uint64_t"; break;
        case DefParser::FieldType::_sint64: typeName = "int64_t"; break;
        case DefParser::FieldType::_float: typeName = "float"; break;
        case DefParser::FieldType::_double: typeName = "double"; break;
        case DefParser::FieldType::_string: typeName = "Ptr64<char>"; break;
        case DefParser::FieldType::_enum: typeName = field.enumName; break;
        case DefParser::FieldType::_struct: typeName = field.structName; break;
        case DefParser::FieldType::_link: typeName = "Ptr64<" + field.linkName + ">"; break;
        default:
        {
            s_data.error << "Unhandled field type for " << s.name << "." << field.name;
            return false;
            break;
        }
    }
    return true;
}

static bool MakeHeader_EnumAndStructDefs(const DBRoot& dbRoot)
{
    std::ostringstream& os = s_data.tokenReplacement["/*$EnumAndStructDefs$*/"];

    // Make enum defs
    std::unordered_set<std::string> enumsWritten;
    for (const auto& pair : dbRoot.m_tables)
    {
        const DefParser& parser = pair.second->GetParser();

        bool ret = parser.ForEachEnum(
            [&os, &enumsWritten](const DefParser::Enum& e)
            {
                // only write the same type once
                if (enumsWritten.contains(e.name))
                    return true;
                enumsWritten.insert(e.name);

                // make an extra newline to separate them
                os << "\n";

                std::string indent = "    ";

                os << indent << "enum class " << e.name << " : uint16_t\n" << indent << "{\n";

                for (const std::string& label : e.labels)
                    os << indent << "    " << label << ",\n";

                os << indent << "};\n";

                return true;
            }
        );
    }

    // Make struct defs
    std::unordered_set<std::string> structsWritten;
    for (const auto& pair : dbRoot.m_tables)
    {
        const DefParser& parser = pair.second->GetParser();

        bool ret = parser.ForEachStruct(
            [&os, &structsWritten, &parser, &dbRoot](const DefParser::Struct& s)
            {
                // only write the same type once
                if (structsWritten.contains(s.name))
                    return true;
                structsWritten.insert(s.name);

                // If this isn't the first item written, make an extra newline to separate them
                if (!os.view().empty())
                    os << "\n";

                std::string indent = "    ";

                if (s.isUnion)
                {
                    os << indent << "enum class " << s.name << "_type : uint16_t\n" << indent << "{\n";
                    os << indent << "    None,\n";
                    for (const DefParser::StructField& field : s.fields)
                        os << indent << "    " << field.name << ",\n";
                    os << indent << "};\n\n";

                    os <<
                        indent << "struct " << s.name << "\n" <<
                        indent << "{\n" <<
                        indent << "    " << s.name << "_type type;\n" <<
                        indent << "    Ptr64<void> ptr;\n"
                        "\n"
                        ;

                    // make type aliases
                    for (const DefParser::StructField& field : s.fields)
                    {
                        std::string typeName;
                        if (!FieldToCPPType(s, field, typeName))
                            return false;
                        os << indent << "    using " << field.name << "_type = " << typeName << ";\n";
                    }

                    // accessor functions
                    for (const DefParser::StructField& field : s.fields)
                    {
                        os << "\n";
                        os << indent << "    " << field.name << "_type* " << field.name << "() { return type == " << s.name << "_type::" << field.name << " ? reinterpret_cast<" << field.name << "_type*>(ptr.ptr) : nullptr; }\n";
                        os << indent << "    const " << field.name << "_type* " << field.name << "() const { return type == " << s.name << "_type::" << field.name << " ? reinterpret_cast<const " << field.name << "_type*>(ptr.ptr) : nullptr; }\n";
                    }

                    os <<
                        indent << "};\n"
                        ;
                }
                else
                {
                    os << indent << "struct " << s.name << "\n" << indent << "{\n";

                    // Write the fields
                    for (const DefParser::StructField& field : s.fields)
                    {
                        // Get the C++ name of the type
                        std::string typeName;
                        if (!FieldToCPPType(s, field, typeName))
                            return false;

                        bool isUnion = false;
                        if (field.fieldType == DefParser::FieldType::_struct)
                        {
                            const DefParser::Struct* fieldStruct = parser.GetStructByName(field.structName.c_str());
                            if (!fieldStruct)
                            {
                                s_data.error << "Could not find struct " << field.structName << " for " << s.name << "." << field.name;
                                return false;
                            }
                            isUnion = fieldStruct->isUnion;
                        }

                        // Arrays are a count and a pointer to the data.
                        // Dynamic arrays get their count from the bin file. Static arrays know their count at compile time.
                        if (field.isArray)
                        {
                            if (field.fixedArraySize > 0)
                            {
                                os << indent << "    static const uint32_t " << field.name << "_count = " << field.fixedArraySize << ";\n";
                                os << indent << "    " << typeName << " " << field.name << "[" << field.fixedArraySize << "];\n";
                            }
                            else
                            {
                                os << indent << "    uint32_t " << field.name << "_count = 0;\n";
                                os << indent << "    Ptr64<" << typeName << "> " << field.name << ";\n";
                            }
                            continue;
                        }

                        os << indent << "    " << typeName << " " << field.name << ";\n";
                    }

                    os << indent << "};\n";
                }

                if (IsARootStruct(dbRoot, s.name.c_str()))
                    os << "\n" << indent << "using " << s.name << "Record = Record<" << s.name << ">;\n";

                return true;
            }
        );
        if (!ret)
            return false;
    }

    return true;
}

static bool MakeHeader_StructLoading(const DBCompileSettings& compilerSettings, const DBRoot& dbRoot)
{
    // make storage for each table
    std::ostringstream& privateStorage = s_data.tokenReplacement["/*$PrivateStorage$*/"];
    for (const auto& pair : dbRoot.m_tables)
    {
        std::string indent = "    ";

        const DefParser& parser = pair.second->GetParser();

        // Make an extra newline to separate them
        if (!privateStorage.view().empty())
            privateStorage << "\n";

        privateStorage << indent << "uint32_t m_table_" << parser.GetRootStructName() << "_count = 0;\n";

        // The sorted list of table names. Tables are written in sorted order
        if (compilerSettings.includeEntryLUT)
            privateStorage << indent << "Ptr64<Ptr64<char>> m_table_" << parser.GetRootStructName() << "_names;\n";

        privateStorage << indent << "Ptr64<" << parser.GetRootStructName() << "> m_table_" << parser.GetRootStructName() << ";\n";
    }

    // make the public interface to get records
    std::ostringstream& publicInterface = s_data.tokenReplacement["/*$PublicInterface$*/"];
    for (const auto& pair : dbRoot.m_tables)
    {
        const DefParser& parser = pair.second->GetParser();

        // Make an extra newline to separate them
        if (!publicInterface.view().empty())
            publicInterface << "\n";

        publicInterface << "template <>\n";
        publicInterface << "inline uint32_t " << compilerSettings.className << "::GetCount<" << compilerSettings.className << "::" << pair.first << ">() const\n";
        publicInterface << "{\n";
        publicInterface << "    return m_table_" << pair.first << "_count;\n";
        publicInterface << "}\n";
        publicInterface << "\n";
        publicInterface << "template <>\n";
        publicInterface << "inline " << compilerSettings.className << "::" << pair.first << "Record " << compilerSettings.className << "::Get<" << compilerSettings.className << "::" << pair.first << ">(uint32_t index) const\n";
        publicInterface << "{\n";
        publicInterface << "    " << pair.first << "Record ret;\n";
        publicInterface << "    if (index < m_table_" << pair.first << "_count)\n";
        publicInterface << "    {\n";
        publicInterface << "        ret.m_record = &m_table_" << pair.first << ".ptr[index];\n";

        if (compilerSettings.hotReloading)
        {
            publicInterface <<
                "        size_t nameLen = strlen(m_table_" << pair.first << "_names.ptr[index].ptr);\n" <<
                "        ret.m_recordName = new char[nameLen + 1];\n" <<
                "        memcpy(ret.m_recordName, m_table_" << pair.first << "_names.ptr[index].ptr, nameLen + 1);\n"
                ;
        }

        publicInterface << "    }\n";

        if (compilerSettings.hotReloading)
        {
            publicInterface <<
                "\n" <<
                "    ret.m_parent = this;\n" <<
                "    ret.m_generation = m_generation;\n" <<
                "\n"
                ;
        }

        publicInterface << "    return ret;\n";
        publicInterface << "}\n";

        // If we have the name entries, have an interface to look up by name
        if (compilerSettings.includeEntryLUT)
        {
            publicInterface << "\n";
            publicInterface << "template <>\n";
            publicInterface << "inline " << compilerSettings.className << "::" << pair.first << "Record " << compilerSettings.className << "::Get<" << compilerSettings.className << "::" << pair.first << ">(const char* name) const\n";
            publicInterface << "{\n";
            publicInterface << "    " << pair.first << "Record ret;\n";
            publicInterface << "\n";
            publicInterface << "    Ptr64<char>* array = m_table_" << pair.first << "_names.ptr;\n";
            publicInterface << "    const uint32_t count = m_table_" << pair.first << "_count;\n";
            publicInterface << "\n";
            publicInterface << "    auto it = std::lower_bound(\n";
            publicInterface << "        array,\n";
            publicInterface << "        array + count,\n";
            publicInterface << "        name,\n";
            publicInterface << "        [](const Ptr64<char>& item, const char* val)\n";
            publicInterface << "        {\n";
            publicInterface << "            return strcmp(item.ptr, val) < 0;\n";
            publicInterface << "        }\n";
            publicInterface << "    );\n";
            publicInterface << "\n";
            publicInterface << "    uint32_t index = uint32_t(it - array);\n";
            publicInterface << "\n";
            publicInterface << "    if (index < count && !strcmp(it->ptr, name))\n";
            publicInterface << "    {\n";
            publicInterface << "        ret.m_record = &m_table_" << pair.first << ".ptr[index];\n";

            if (compilerSettings.hotReloading)
            {
                publicInterface <<
                    "        size_t nameLen = strlen(m_table_" << pair.first << "_names.ptr[index].ptr);\n" <<
                    "        ret.m_recordName = new char[nameLen + 1];\n" <<
                    "        memcpy(ret.m_recordName, m_table_" << pair.first << "_names.ptr[index].ptr, nameLen + 1);\n"
                    ;
            }

            publicInterface << "    }\n";

            if (compilerSettings.hotReloading)
            {
                publicInterface <<
                    "\n" <<
                    "    ret.m_parent = this;\n" <<
                    "    ret.m_generation = m_generation;\n"
                    ;
            }

            publicInterface << "\n";
            publicInterface << "    return ret;\n";
            publicInterface << "}\n";
        }

        // make the friendly, named interface
        publicInterface <<
            "\n" <<
            "inline uint32_t " << compilerSettings.className << "::Get" << pair.first << "Count() const\n" <<
            "{\n" <<
            "    return GetCount<" << pair.first << ">();\n" <<
            "}\n" <<
            "\n" <<
            "inline " << compilerSettings.className << "::" << pair.first << "Record " << compilerSettings.className << "::Get" << pair.first << "(uint32_t index) const\n" <<
            "{\n" <<
            "    return Get<" << pair.first << ">(index);\n" <<
            "}\n"
            ;

        if (compilerSettings.includeEntryLUT)
        {
            publicInterface <<
                "\n" <<
                "inline " << compilerSettings.className << "::" << pair.first << "Record " << compilerSettings.className << "::Get" << pair.first << "(const char* name) const\n" <<
                "{\n" <<
                "    return Get<" << pair.first << ">(name);\n" <<
                "}\n"
                ;
        }
    }

    // make the code to load each table
    std::ostringstream& loadTables = s_data.tokenReplacement["/*$LoadTables$*/"];
    for (const auto& pair : dbRoot.m_tables)
    {
        // seperate table loading with an extra newline
        loadTables << "\n";

        std::string indent = "    ";

        loadTables << indent << "// " << pair.first << " Table\n";
        loadTables << indent << "{\n";
        loadTables << indent << "    if (!Read(m_table_" << pair.first << "_count, mem, memIndex, memSize, endianSwap))\n";
        loadTables << indent << "        return false;\n";
        loadTables << "\n";
        loadTables << indent << "    if (m_table_" << pair.first << "_count > 0)\n";
        loadTables << indent << "    {\n";

        if (compilerSettings.includeEntryLUT)
        {
            loadTables << indent << "        // get char** to LUT and fixup string pointers\n";
            loadTables << indent << "        if (memSize - memIndex < m_table_" << pair.first << "_count * sizeof(uint64_t))\n";
            loadTables << indent << "            return false;\n";
            loadTables << indent << "        m_table_" << pair.first << "_names._64 = memIndex;\n";
            loadTables << indent << "        DoEndianSwapAndPointerFixup(m_table_" << pair.first << "_names, mem, endianSwap);\n";
            loadTables << indent << "        for (uint32_t i = 0; i < m_table_" << pair.first << "_count; ++i)\n";
            loadTables << indent << "            DoEndianSwapAndPointerFixup(m_table_" << pair.first << "_names.ptr[i], mem, endianSwap);\n";
            loadTables << indent << "        memIndex += m_table_" << pair.first << "_count * sizeof(uint64_t);\n";
            loadTables << "\n";
        }

        loadTables << indent << "        // Get a pointer to the first entry in the table\n";
        loadTables << indent << "        if (memSize - memIndex < m_table_" << pair.first << "_count * sizeof(" << pair.first << "))\n";
        loadTables << indent << "            return false;\n";
        loadTables << indent << "        m_table_" << pair.first << "._64 = memIndex;\n";
        loadTables << indent << "        DoEndianSwapAndPointerFixup(m_table_" << pair.first << ", mem, endianSwap);\n";
        loadTables << indent << "        memIndex += m_table_" << pair.first << "_count * sizeof(" << pair.first << ");\n";
        loadTables << "\n";
        loadTables << indent << "        // Do pointer fixup\n";
        loadTables << indent << "        for (uint32_t i = 0; i < m_table_" << pair.first << "_count; ++i)\n";
        loadTables << indent << "            DoEndianSwapAndPointerFixup(m_table_" << pair.first << ".ptr[i], mem, endianSwap);\n";

        loadTables << indent << "    }\n";
        loadTables << indent << "}\n";
    }

    // make pointer fixup code for each table
    {
        std::unordered_set<std::string> structsHandled;
        std::ostringstream& pointerFixup = s_data.tokenReplacement["/*$PointerFixup$*/"];
        std::ostringstream& pointerFixupFwd = s_data.tokenReplacement["/*$PointerFixupForwardDeclare$*/"];
        for (const auto& pair : dbRoot.m_tables)
        {
            bool ret = pair.second->GetParser().ForEachStruct(
                [&structsHandled, &pointerFixup, &pointerFixupFwd, &compilerSettings](const DefParser::Struct& structDef)
                {
                    // Only need to write each DoEndianSwapAndPointerFixup() function once
                    if (structsHandled.contains(structDef.name))
                        return true;
                    structsHandled.insert(structDef.name);

                    pointerFixupFwd << "    static void DoEndianSwapAndPointerFixup(" << structDef.name << "& v, void* base, bool endianSwap);\n";

                    std::string indent = "";
                    pointerFixup << "\n";
                    pointerFixup << indent << "inline void " << compilerSettings.className << "::DoEndianSwapAndPointerFixup(" << structDef.name << "& v, void* base, bool endianSwap)\n";
                    pointerFixup << indent << "{\n";

                    if (structDef.isUnion)
                    {
                        pointerFixup <<
                            indent << "    DoEndianSwapAndPointerFixup(v.type, base, endianSwap);\n" <<
                            indent << "    DoEndianSwapAndPointerFixup(v.ptr, base, endianSwap);\n" <<
                            indent << "    switch(v.type)\n" <<
                            indent << "    {\n"
                            ;

                        for (const DefParser::StructField& field : structDef.fields)
                        {
                            pointerFixup <<
                                indent << "        case " << structDef.name << "_type::" << field.name << ":" <<
                                "DoEndianSwapAndPointerFixup(*v." << field.name << "()" <<
                                ", base, endianSwap); break;\n"
                                ;
                        }

                        pointerFixup <<
                            indent << "    }\n"
                            ;
                    }
                    else
                    {
                        for (const DefParser::StructField& field : structDef.fields)
                        {
                            if (field.isArray)
                            {
                                if (field.fixedArraySize == 0)
                                {
                                    pointerFixup << indent << "    DoEndianSwapAndPointerFixup(v." << field.name << "_count, base, endianSwap);\n";
                                    pointerFixup << indent << "    DoEndianSwapAndPointerFixup(v." << field.name << ", base, endianSwap);\n";
                                }

                                pointerFixup << indent << "    for (uint32_t i = 0; i < v." << field.name << "_count; ++i)\n";
                                if (field.fixedArraySize != 0)
                                    pointerFixup << indent << "        DoEndianSwapAndPointerFixup(v." << field.name << "[i], base, endianSwap);\n";
                                else
                                    pointerFixup << indent << "        DoEndianSwapAndPointerFixup(v." << field.name << ".ptr[i], base, endianSwap);\n";
                            }
                            else
                            {
                                pointerFixup << indent << "    DoEndianSwapAndPointerFixup(v." << field.name << ", base, endianSwap);\n";
                            }
                        }
                    }

                    pointerFixup << indent << "}\n";

                    return true;
                }
            );
            if (!ret)
                return false;
        }
    }

    return true;
}

static bool MakeHeader_Global(const DBCompileSettings& compilerSettings, const DBRoot& dbRoot, uint64_t hash)
{
    // Write an empty string to conditionally written to tokens, so that they always disappear in the output file.
    s_data.tokenReplacement["/*$LoadFileEnd$*/"] << "";
    s_data.tokenReplacement["/*$LoadMemoryEnd$*/"] << "";
    s_data.tokenReplacement["/*$Includes$*/"] << "";
    s_data.tokenReplacement["/*$Tick$*/"] << "";
    s_data.tokenReplacement["/*$Dtor$*/"] << "";
    s_data.tokenReplacement["/*$RecordGetFwd$*/"] << "";
    s_data.tokenReplacement["/*$Obfuscation$*/"] << "";

    s_data.tokenReplacement["/*$ClassName$*/"] << compilerSettings.className;

    s_data.tokenReplacement["/*$SchemaHash$*/"] << "0x" << std::hex << std::setfill('0') << std::setw(16) << hash << "ULL";

    s_data.tokenReplacement["/*$Version$*/"] << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_PATCH;

    // Obfuscation
    if (compilerSettings.obfuscation)
    {
        s_data.tokenReplacement["/*$Obfuscation$*/"] <<
            "\n"
            "\n"
            "    // De obfuscate the data\n"
            "    {\n"
            "        size_t contentStart = 7; // start after the fourcc and the 3 byte version\n"
            "        size_t bytesRemaining = memSize - contentStart;\n"
            "        uint32_t rng = pcg_hash(0x1337beef);\n"
            "        uint8_t* data = &((uint8_t*)mem)[contentStart];\n"
            "        while (bytesRemaining > 0)\n"
            "        {\n"
            "            rng = pcg_hash(rng);\n"
            "\n"
            "            for (size_t i = 0; i < ((bytesRemaining < 4) ? bytesRemaining : 4); ++i)\n"
            "                data[i] = data[i] ^ ((uint8_t*)&rng)[i];\n"
            "\n"
            "            data += 4;\n"
            "\n"
            "            if (bytesRemaining >= 4)\n"
            "                bytesRemaining -= 4;\n"
            "            else\n"
            "                bytesRemaining = 0;\n"
            "        }\n"
            "    }"
            ;
    }

    // LUT functionality not covered by hot reloading
    if (compilerSettings.includeEntryLUT)
    {
        s_data.tokenReplacement["/*$RecordGetFwd$*/"] <<
            "\n"
            "    template <typename T>\n"
            "    inline Record<T> Get(const char* name) const;\n"
            ;
    }

    // make the nicer human interfaces
    {
        std::unordered_set<std::string> structsWritten;
        for (const auto& pair : dbRoot.m_tables)
        {
            const DefParser& parser = pair.second->GetParser();

            bool ret = parser.ForEachStruct(
                [&structsWritten, &parser, &dbRoot, &compilerSettings](const DefParser::Struct& s)
                {
                    // only write the same type once
                    if (structsWritten.contains(s.name))
                        return true;
                    structsWritten.insert(s.name);

                    if (!IsARootStruct(dbRoot, s.name.c_str()))
                        return true;

                    s_data.tokenReplacement["/*$RecordGetFwd$*/"] <<
                        "\n" <<
                        "    inline uint32_t Get" << s.name << "Count() const;\n" <<
                        "    inline " << s.name << "Record Get" << s.name << "(uint32_t index) const;\n"
                        "    inline " << s.name << "Record Get" << s.name << "(int index) const\n"
                        "    {\n"
                        "        // An int version to catch index 0 not being ambiguous with nullptr\n"
                        "        return Get" << s.name << "((uint32_t)index);\n"
                        "    }\n"
                        ;

                    if (compilerSettings.includeEntryLUT)
                    {
                        s_data.tokenReplacement["/*$RecordGetFwd$*/"] <<
                            "    inline " << s.name << "Record Get" << s.name << "(const char* name) const;\n"
                            ;
                    }

                    return true;
                }
            );
        }
    }

    if (compilerSettings.includeEntryLUT)
    {
        s_data.tokenReplacement["/*$Includes$*/"] <<
            "#include <algorithm>\n"
            ;
    }

    // Hot reloading support
    if (compilerSettings.hotReloading)
    {
        s_data.tokenReplacement["/*$PrivateStorage$*/"] <<
            "    // Incremented each time the data is reloaded\n"
            "    uint32_t m_generation = 0;\n"
            "\n"
            "    // The last modification time of the file, for hot reloading.\n"
            "    std::filesystem::file_time_type m_fileTime;\n"
            "\n"
            "    // The filename so we can get its file time again later.\n"
            "    char* m_fileName = nullptr;\n"
            ;

        s_data.tokenReplacement["/*$LoadMemoryEnd$*/"] <<
            "\n"
            "    // Track that the data was (re)loaded to invalidate stale records\n"
            "    m_generation++;\n"
            ;

        s_data.tokenReplacement["/*$LoadFileEnd$*/"] <<
            "\n"
            "    // Save off the file name and get the file time, for hot reloading\n"
            "    if (!m_fileName || strcmp(m_fileName, fileName))\n"
            "    {\n"
            "        if (m_fileName)\n"
            "            delete[] m_fileName;\n"
            "        size_t fileNameLen = strlen(fileName);\n"
            "        m_fileName = new char[fileNameLen + 1];\n"
            "        memcpy(m_fileName, fileName, fileNameLen + 1);\n"
            "    }\n"
            "    m_fileTime = std::filesystem::last_write_time(m_fileName);\n"
            ;

        s_data.tokenReplacement["/*$Dtor$*/"] <<
            "    if (m_fileName != nullptr)\n"
            "    {\n"
            "        delete[] m_fileName;\n"
            "        m_fileName = nullptr;\n"
            "    }\n"
            ;

        s_data.tokenReplacement["/*$Tick$*/"] <<
            "\n"
            "        std::filesystem::file_time_type fileTime = std::filesystem::last_write_time(m_fileName);\n"
            "        if (fileTime > m_fileTime)\n"
            "        {\n"
            "            LoadFromFile(m_fileName);\n"
            "            return true;\n"
            "        }\n"
            ;

        s_data.tokenReplacement["/*$Includes$*/"] <<
            "#include <filesystem>\n"
            ;
    }

    // Make the templated Record struct
    {
        std::string indent = "    ";

        std::ostringstream& os = s_data.tokenReplacement["/*$RecordDef$*/"];

        os <<
            indent << "template <typename T>\n" <<
            indent << "struct Record\n" <<
            indent << "{\n" <<
            indent << "public:\n";

        if (compilerSettings.hotReloading)
        {
            // All this to properly handle m_fileName not leaking or getting double freed.
            // Avoiding std::string.
            os <<
                indent << "    Record()\n" <<
                indent << "    {\n" <<
                indent << "    }\n" <<
                "\n" <<
                indent << "    Record(const Record& other)\n" <<
                indent << "    {\n" <<
                indent << "        m_record = other.m_record;\n" <<
                indent << "        m_parent = other.m_parent;\n" <<
                indent << "        m_generation = other.m_generation;\n" <<
                indent << "        size_t nameLen = strlen(other.m_recordName);\n" <<
                indent << "        m_recordName = new char[nameLen + 1];\n" <<
                indent << "        memcpy(m_recordName, other.m_recordName, nameLen+1);\n" <<
                indent << "    }\n" <<
                "\n" <<
                indent << "    Record(Record&& other) noexcept\n" <<
                indent << "    {\n" <<
                indent << "        std::swap(m_record, other.m_record);\n" <<
                indent << "        std::swap(m_parent, other.m_parent);\n" <<
                indent << "        std::swap(m_generation, other.m_generation);\n" <<
                indent << "        std::swap(m_recordName, other.m_recordName);\n" <<
                indent << "    }\n" <<
                "\n" <<
                indent << "    ~Record()\n" <<
                indent << "    {\n" <<
                indent << "        if (m_recordName)\n" <<
                indent << "        {\n" <<
                indent << "            delete[] m_recordName;\n" <<
                indent << "            m_recordName = nullptr;\n" <<
                indent << "        }\n" <<
                indent << "    }\n" <<
                "\n" <<
                indent << "    Record& operator=(Record other)\n" <<
                indent << "    {\n" <<
                indent << "        std::swap(m_record, other.m_record);\n" <<
                indent << "        std::swap(m_parent, other.m_parent);\n" <<
                indent << "        std::swap(m_generation, other.m_generation);\n" <<
                indent << "        std::swap(m_recordName, other.m_recordName);\n" <<
                indent << "        return *this;\n" <<
                indent << "    }\n" <<
                "\n"
                ;
        }

        os <<
            indent << "    const T& Get()" << (compilerSettings.hotReloading ? "" : " const") << "\n" <<
            indent << "    {\n";

        if (compilerSettings.hotReloading)
        {
            os <<
                indent << "        if (m_parent && m_parent->m_generation != m_generation)\n" <<
                indent << "            *this = m_parent->Get<T>(m_recordName);\n"
                ;
        }

        os <<
            indent << "        static const T s_dummy = T();\n" <<
            indent << "        return m_record ? *m_record : s_dummy;\n" <<
            indent << "    }\n" <<
            "\n" <<
            indent << "    bool Valid() const\n" <<
            indent << "    {\n"
            ;

        if (compilerSettings.hotReloading)
            os << indent << "        Get();\n";

        os <<
            indent << "        return m_record != nullptr;\n" <<
            indent << "    }\n" <<
            "\n" <<
            indent << "private:\n" <<
            indent << "    friend class " << compilerSettings.className << ";\n" <<
            indent << "    T* m_record = nullptr;\n";

        if (compilerSettings.hotReloading)
        {
            os <<
                "\n" <<
                indent << "    const " << compilerSettings.className << "* m_parent = nullptr;\n" <<
                indent << "    uint32_t m_generation = ~0;\n" <<
                indent << "    char* m_recordName = nullptr;\n";
        }

        os << indent << "};\n";
    }

    return true;
}

bool MakeCPP(const DBCompileSettings& compilerSettings, const DBRoot& dbRoot, const char* fileName, uint64_t hash, std::string& error)
{
    s_data = StaticData();

    // Grab the error string as we exit this function
    AT_EXIT(error = s_data.error.str());

    if (!MakeHeader_Global(compilerSettings, dbRoot, hash))
        return false;

    if (!MakeHeader_EnumAndStructDefs(dbRoot))
        return false;

    if (!MakeHeader_StructLoading(compilerSettings, dbRoot))
        return false;

    // Do token replacement on output.h
    std::string out = c_file_text;
    for (const auto& pair : s_data.tokenReplacement)
        StringReplaceAll(out, pair.first, pair.second.str());

    // Create any directories needed
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(fileName).remove_filename().generic_string().c_str(), ec);

    // write file out
    FILE* file = nullptr;
    fopen_s(&file, fileName, "wb");
    if (!file)
    {
        s_data.error << "Could not write to " << fileName;
        return false;
    }
    fwrite(out.data(), out.length(), 1, file);
    fclose(file);

    return true;
}
