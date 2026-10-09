#include "Compile_C.h"

#include "../loader/loader.h"
#include "../Version.h"
#include "../Utils.h"

#include "../loader/output_c_h_string.h"

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
        case DefParser::FieldType::_bool: typeName = "bool"; break;
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
        case DefParser::FieldType::_string: typeName = "uint64_t"; break; // Ptr64<char>
        case DefParser::FieldType::_enum: typeName = field.enumName; break;
        case DefParser::FieldType::_struct: typeName = field.structName; break;
        case DefParser::FieldType::_link: typeName = "uint64_t"; break; // Ptr64<" + field.linkName + ">
        default:
        {
            s_data.error << "Unhandled field type for " << s.name << "." << field.name;
            return false;
            break;
        }
    }
    return true;
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

static bool MakeC_Global(const DBCompileSettings& compilerSettings, const DBRoot& dbRoot, uint64_t hash)
{
    // Write an empty string to conditionally written to tokens, so that they always disappear in the output file.
    s_data.tokenReplacement["/*$LoadFileEnd$*/"] << "";
    s_data.tokenReplacement["/*$LoadMemoryEnd$*/"] << "";
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
            "        uint32_t rng = " << compilerSettings.className << "_pcg_hash(0x1337beef);\n"
            "        uint8_t* data = &((uint8_t*)mem)[contentStart];\n"
            "        while (bytesRemaining > 0)\n"
            "        {\n"
            "            rng = " << compilerSettings.className << "_pcg_hash(rng);\n"
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

    return true;
}

static bool MakeC_Structs(const DBCompileSettings& compilerSettings, const DBRoot& dbRoot)
{
    // make storage for each table
    std::ostringstream& privateStorage = s_data.tokenReplacement["/*$StructFields$*/"];
    std::ostringstream& init = s_data.tokenReplacement["/*$StructInit$*/"];
    for (const auto& pair : dbRoot.m_tables)
    {
        std::string indent = "    ";

        const DefParser& parser = pair.second->GetParser();

        init << indent << "db->m_table_" << parser.GetRootStructName() << "_count = 0;\n";

        // Make an extra newline to separate them
        privateStorage << "\n";

        privateStorage << indent << "uint32_t m_table_" << parser.GetRootStructName() << "_count;\n";

        // The sorted list of table names. Tables are written in sorted order
        if (compilerSettings.includeEntryLUT)
            privateStorage << indent << "uint64_t m_table_" << parser.GetRootStructName() << "_names; // Ptr64<Ptr64<char>>\n";

        privateStorage << indent << "uint64_t m_table_" << parser.GetRootStructName() << ";       // Ptr64<" << parser.GetRootStructName() << ">\n";
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
        loadTables << indent << "    if (!" << compilerSettings.className << "_Read_U32(&db->m_table_" << pair.first << "_count, mem, &memIndex, memSize, endianSwap))\n";
        loadTables << indent << "        return false;\n";
        loadTables << "\n";
        loadTables << indent << "    if (db->m_table_" << pair.first << "_count > 0)\n";
        loadTables << indent << "    {\n";

        if (compilerSettings.includeEntryLUT)
        {
            loadTables << indent << "        // get char** to LUT and fixup string pointers\n";
            loadTables << indent << "        if (memSize - memIndex < db->m_table_" << pair.first << "_count * sizeof(uint64_t))\n";
            loadTables << indent << "            return false;\n";
            loadTables << indent << "        db->m_table_" << pair.first << "_names = memIndex;\n";
            loadTables << indent << "        " << compilerSettings.className << "_DoEndianSwapAndPointerFixup_Ptr(&db->m_table_" << pair.first << "_names, mem, endianSwap);\n";
            loadTables << indent << "        for (uint32_t i = 0; i < db->m_table_" << pair.first << "_count; ++i)\n";
            loadTables << indent << "            " << compilerSettings.className << "_DoEndianSwapAndPointerFixup_Ptr(&((uint64_t*)db->m_table_" << pair.first << "_names)[i], mem, endianSwap);\n";
            loadTables << indent << "        memIndex += db->m_table_" << pair.first << "_count * sizeof(uint64_t);\n";
            loadTables << "\n";
        }

        loadTables << indent << "        // Get a pointer to the first entry in the table\n";
        loadTables << indent << "        if (memSize - memIndex < db->m_table_" << pair.first << "_count * sizeof(" << compilerSettings.className << "_" << pair.first << "))\n";
        loadTables << indent << "            return false;\n";
        loadTables << indent << "        db->m_table_" << pair.first << " = memIndex;\n";
        loadTables << indent << "        " << compilerSettings.className << "_DoEndianSwapAndPointerFixup_Ptr(&db->m_table_" << pair.first << ", mem, endianSwap);\n";
        loadTables << indent << "        memIndex += db->m_table_" << pair.first << "_count * sizeof(" << compilerSettings.className << "_" << pair.first << ");\n";
        loadTables << "\n";
        loadTables << indent << "        // Do pointer fixup\n";
        loadTables << indent << "        for (uint32_t i = 0; i < db->m_table_" << pair.first << "_count; ++i)\n";
        loadTables << indent << "            " << compilerSettings.className << "_DoEndianSwapAndPointerFixup_" << pair.first << "(&((" << compilerSettings.className << "_" << pair.first << "*)db->m_table_" << pair.first << ")[i], mem, endianSwap);\n";

        loadTables << indent << "    }\n";
        loadTables << indent << "}\n";
    }

    std::ostringstream& enumsAndStructDefs = s_data.tokenReplacement["/*$EnumAndStructDefs$*/"];

    // Make struct defs
    std::unordered_set<std::string> structsWritten;
    for (const auto& pair : dbRoot.m_tables)
    {
        const DefParser& parser = pair.second->GetParser();

        bool ret = parser.ForEachStruct(
            [&enumsAndStructDefs, &structsWritten, &parser, &dbRoot, &compilerSettings](const DefParser::Struct& s)
            {
                // only write the same type once
                if (structsWritten.contains(s.name))
                    return true;
                structsWritten.insert(s.name);

                // If this isn't the first item written, make an extra newline to separate them
                if (!enumsAndStructDefs.view().empty())
                    enumsAndStructDefs << "\n";

                std::string indent = "";

                if (s.isUnion)
                {
                    enumsAndStructDefs << indent << "enum class " << s.name << "_type : uint16_t\n" << indent << "{\n";
                    enumsAndStructDefs << indent << "    None,\n";
                    for (const DefParser::StructField& field : s.fields)
                        enumsAndStructDefs << indent << "    " << field.name << ",\n";
                    enumsAndStructDefs << indent << "};\n\n";

                    enumsAndStructDefs <<
                        indent << "typedef struct " << compilerSettings.className << "_" << s.name << "\n" <<
                        indent << "{\n" <<
                        indent << "    " << s.name << "_type type;\n" <<
                        indent << "    uint64_t ptr; // Ptr64<void>\n"
                        "\n"
                        ;

                    // TODO: union interface for C? maybe global functions?

                    /*
                    // make type aliases
                    for (const DefParser::StructField& field : s.fields)
                    {
                        std::string typeName;
                        if (!FieldToCPPType(s, field, typeName))
                            return false;
                        enumsAndStructDefs << indent << "    using " << field.name << "_type = " << typeName << ";\n";
                    }

                    // accessor functions
                    for (const DefParser::StructField& field : s.fields)
                    {
                        enumsAndStructDefs << "\n";
                        enumsAndStructDefs << indent << "    " << field.name << "_type* " << field.name << "() { return type == " << s.name << "_type::" << field.name << " ? reinterpret_cast<" << field.name << "_type*>(ptr.ptr) : nullptr; }\n";
                        enumsAndStructDefs << indent << "    const " << field.name << "_type* " << field.name << "() const { return type == " << s.name << "_type::" << field.name << " ? reinterpret_cast<const " << field.name << "_type*>(ptr.ptr) : nullptr; }\n";
                    }
                    */

                    enumsAndStructDefs <<
                        indent << "} " << compilerSettings.className << "_" << s.name << ";\n"
                        ;
                }
                else
                {
                    enumsAndStructDefs << indent << "typedef struct " << compilerSettings.className << "_" << s.name << "\n" << indent << "{\n";

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
                                enumsAndStructDefs << indent << "    static const uint32_t " << field.name << "_count = " << field.fixedArraySize << ";\n";
                                enumsAndStructDefs << indent << "    " << typeName << " " << field.name << "[" << field.fixedArraySize << "];\n";
                            }
                            else
                            {
                                enumsAndStructDefs << indent << "    uint32_t " << field.name << "_count = 0;\n";
                                enumsAndStructDefs << indent << "    uint64_t " << field.name << "; // Ptr64<" << typeName << ">\n";
                            }
                            continue;
                        }

                        enumsAndStructDefs << indent << "    " << typeName << " " << field.name << ";\n";
                    }

                    enumsAndStructDefs << indent << "} " << compilerSettings.className << "_" << s.name << ";\n";
                }

                // TODO: record interface?
                //if (IsARootStruct(dbRoot, s.name.c_str()))
                    //enumsAndStructDefs << "\n" << indent << "using " << s.name << "Record = Record<" << s.name << ">;\n";

                return true;
            }
        );
        if (!ret)
            return false;
    }

    return true;
}

bool MakeC(const DBCompileSettings& compilerSettings, const DBRoot& dbRoot, const char* fileName, uint64_t hash, std::string& error)
{
    s_data = StaticData();

    // Grab the error string as we exit this function
    AT_EXIT(error = s_data.error.str());

    if (!MakeC_Global(compilerSettings, dbRoot, hash))
        return false;

    if (!MakeC_Structs(compilerSettings, dbRoot))
        return false;

    // TODO: implement!

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
/*
TODO:
DoEndianSwapAndPointerFixup and other global functions need to have class name prefix
*/