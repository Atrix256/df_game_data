#include "Compile.h"

#include "Editor.h"

#include <sstream>

#include "../loader/hash.h"

#include <type_traits>
#include <unordered_set>

#include "../loader/output_string.h"

struct DataOffset
{
    size_t stackIndex = 0;
    size_t offset = 0;
};

struct StaticData
{
    std::ostringstream error;

    // We start by writing to stack index 0.
    // When writing into a specific data stack...
    // * static sized objects go there
    // * dynamic sized objects go one stack higher and get a pointer in the current stack (strings, dynamic arrays)
    // This makes it so objects are always fixed size, making it easier to iterate through arrays.
    std::vector<std::vector<char>> dataStack;

    // This is where dynamic sized objects are stored
    std::vector<char> dataDynamic;

    struct TableEntryLocation
    {
        std::string table;
        std::string entry;
        DataOffset offset;
    };

    // Places in the file that want offsets to entries in tables
    std::vector<TableEntryLocation> links;

    // Where the entries live in the file
    std::vector<TableEntryLocation> entries;

    struct DataTableOffsets
    {
        DataOffset srcOffset;
        DataOffset destoffset;
    };

    // Places in the file that point at an offset in the data, and what that offset is
    std::vector<DataTableOffsets> dataOffsets;

    // A map to replace tokens in output.h with strings
    std::unordered_map<std::string, std::ostringstream> tokenReplacement;
};
static StaticData s_data;

static bool MakeBin_WriteStruct(DBTable& table, const DefParser::Struct& structDef, const json& json, const json_pointer& path, size_t stackIndex);

inline constexpr uint32_t MakeFourCC(char a, char b, char c, char d)
{
    return (uint32_t)(uint8_t)a
        | ((uint32_t)(uint8_t)b << 8)
        | ((uint32_t)(uint8_t)c << 16)
        | ((uint32_t)(uint8_t)d << 24);
}

inline void StringReplaceAll(std::string& str, const std::string& from, const std::string& to)
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
                if (enumsWritten.contains(e.FullName()))
                    return true;
                enumsWritten.insert(e.FullName());

                // If this isn't the first item written, make an extra newline to separate them
                if (!os.view().empty())
                    os << "\n";

                std::string indent = "    ";

                if (!e.nameSpace.empty())
                {
                    os << indent << "namespace " << e.nameSpace << "\n" << indent << "{\n";
                    indent = "        ";
                }

                os << indent << "enum class " << e.name << " : uint16_t\n" << indent << "{\n";

                for (const std::string& label : e.labels)
                    os << indent << "    " << label << ",\n";

                os << indent << "};\n";

                if (!e.nameSpace.empty())
                {
                    indent = "    ";
                    os << indent << "};\n";
                }
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
            [&os, &structsWritten](const DefParser::Struct& s)
            {
                // only write the same type once
                if (structsWritten.contains(s.FullName()))
                    return true;
                structsWritten.insert(s.FullName());

                // If this isn't the first item written, make an extra newline to separate them
                if (!os.view().empty())
                    os << "\n";

                std::string indent = "    ";

                if (!s.nameSpace.empty())
                {
                    os << indent << "namespace " << s.nameSpace << "\n" << indent << "{\n";
                    indent = "        ";
                }

                os << indent << "struct " << s.name << "\n" << indent << "{\n";

                // Write the fields
                for (const DefParser::StructField& field : s.fields)
                {
                    // Arrays are a count and a pointer to the data.
                    // Dynamic arrays get their count from the bin file. Static arrays know their count at compile time.
                    if (field.isArray)
                    {
                        if (field.fixedArraySize > 0)
                            os << indent << "    static const uint32_t _" << field.name << "_count = " << field.fixedArraySize << ";\n";
                        else
                            os << indent << "    uint32_t _" << field.name << "_count = 0;\n";
                        os << indent << "    uint64_t " << field.name << ";\n"; // TODO: use Ptr64? need to know the type though. maybe reuse switch below in a "type to string" function?
                        // TODO: what to do when ptr64 is inside ptr64?
                        // TODO: make arrays point into data table instead of being written inline
                        // TODO: fixed sized arrays don't need to write the count
                        // TODO: or maybe fixed sized arrays are written inline?
                        continue;
                    }

                    // TODO: on pointer fixup, dynamic arrays with count of 0 should have their pointer set to null

                    // the field type
                    os << indent << "    ";
                    switch (field.fieldType)
                    {
                        case DefParser::FieldType::_bool: os << "Bool"; break;
                        case DefParser::FieldType::_uint8: os << "uint8_t"; break;
                        case DefParser::FieldType::_sint8: os << "int8_t"; break;
                        case DefParser::FieldType::_uint16: os << "uint16_t"; break;
                        case DefParser::FieldType::_sint16: os << "int16_t"; break;
                        case DefParser::FieldType::_uint32: os << "uint32_t"; break;
                        case DefParser::FieldType::_sint32: os << "int32_t"; break;
                        case DefParser::FieldType::_uint64: os << "uint64_t"; break;
                        case DefParser::FieldType::_sint64: os << "int64_t"; break;
                        case DefParser::FieldType::_float: os << "float"; break;
                        case DefParser::FieldType::_double: os << "double"; break;
                        case DefParser::FieldType::_string: os << "Ptr64<char>"; break;
                        case DefParser::FieldType::_enum: os << field.enumName; break;
                        case DefParser::FieldType::_struct: os << field.structName; break;
                        case DefParser::FieldType::_link: os << "Ptr64<" << field.linkName << ">"; break;
                        default:
                        {
                            s_data.error << "Unhandled field type for " << s.name << "." << field.name;
                            return false;
                            break;
                        }
                    }

                    // field name
                    os << " " << field.name << ";\n";
                }

                os << indent << "};\n";

                if (!s.nameSpace.empty())
                {
                    indent = "    ";
                    os << indent << "};\n";
                }
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
    std::ostringstream& privateStorage = s_data.tokenReplacement["/*$PublicStorage$*/"];
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
            privateStorage << indent << "Ptr64<char*> m_table_" << parser.GetRootStructName() << "_names;\n";

        privateStorage << indent << "Ptr64<" << parser.GetRootStructName() << "> m_table_" << parser.GetRootStructName() << ";\n";
    }

    // make the code to load each table
    std::ostringstream& loadTables = s_data.tokenReplacement["/*$LoadTables$*/"];
    for (const auto& pair : dbRoot.m_tables)
    {
        // seperate table loading with an extra newline
        if (!loadTables.view().empty())
            loadTables << "\n";

        std::string indent = "    ";

        loadTables << indent << "// " << pair.first << " Table\n";
        loadTables << indent << "{\n";
        loadTables << indent << "    if (!Read(m_table_" << pair.first << "_count, mem, memIndex, memSize))\n";
        loadTables << indent << "        return false;\n";
        loadTables << "\n";
        loadTables << indent << "    if (m_table_" << pair.first << "_count > 0)\n";
        loadTables << indent << "    {\n";

        if (compilerSettings.includeEntryLUT)
        {
            loadTables << indent << "        // get char** to LUT and fixup string pointers\n";
            loadTables << indent << "        if (memSize - memIndex < m_table_" << pair.first << "_count * sizeof(uint64_t))\n";
            loadTables << indent << "            return false;\n";
            loadTables << indent << "        m_table_" << pair.first << "_names._64 = reinterpret_cast<uintptr_t>(mem) + memIndex;\n";
            loadTables << indent << "        for (uint32_t i = 0; i < m_table_" << pair.first << "_count; ++i)\n";
            loadTables << indent << "            m_table_" << pair.first << "_names.Get()[i] = (char*)(uint64_t(m_table_" << pair.first << "_names.Get()[i]) + uint64_t(mem));\n";
            loadTables << indent << "        memIndex += m_table_" << pair.first << "_count * sizeof(uint64_t);\n";
            loadTables << "\n";
        }

        loadTables << indent << "        // Get a pointer to the first entry in the table\n";
        loadTables << indent << "        if (memSize - memIndex < m_table_" << pair.first << "_count * sizeof(" << pair.first << "))\n";
        loadTables << indent << "            return false;\n";
        loadTables << indent << "        m_table_" << pair.first << "._64 = reinterpret_cast<uintptr_t>(mem) + memIndex;\n";
        loadTables << indent << "        memIndex += m_table_" << pair.first << "_count * sizeof(" << pair.first << ");\n";

        loadTables << indent << "    }\n";
        loadTables << indent << "}\n";
    }

    return true;
}

static bool MakeHeader(const DBCompileSettings& compilerSettings, const DBRoot& dbRoot, const char* fileName, uint64_t hash)
{
    if (!MakeHeader_EnumAndStructDefs(dbRoot))
        return false;

    if (!MakeHeader_StructLoading(compilerSettings, dbRoot))
        return false;

    if (!compilerSettings.className.empty())
        s_data.tokenReplacement["/*$ClassName$*/"] << compilerSettings.className;
    else
        s_data.tokenReplacement["/*$ClassName$*/"] << "dfgd";

    s_data.tokenReplacement["/*$SchemaHash*/"] << "0x" << std::hex << std::setfill('0') << std::setw(16) << hash << "ULL";

    // Do token replacement on output.h
    std::string out = c_output_h;
    for (const auto& pair : s_data.tokenReplacement)
        StringReplaceAll(out, pair.first, pair.second.str());

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

static DataOffset MakeBin_GetOffset(size_t stackIndex)
{
    if (stackIndex + 1 > s_data.dataStack.size())
        s_data.dataStack.resize(stackIndex + 1);

    std::vector<char>& dest = s_data.dataStack[stackIndex];

    DataOffset ret;
    ret.stackIndex = stackIndex;
    ret.offset = dest.size();
    return ret;
}

static DataOffset MakeBin_Write(size_t stackIndex, void* data, size_t size)
{
    if (stackIndex + 1 > s_data.dataStack.size())
        s_data.dataStack.resize(stackIndex + 1);

    std::vector<char>& dest = s_data.dataStack[stackIndex];
    size_t offset = dest.size();
    dest.resize(offset + size);
    memcpy(&dest[offset], data, size);

    DataOffset ret;
    ret.stackIndex = stackIndex;
    ret.offset = offset;
    return ret;
}

static DataOffset MakeBin_Write(size_t stackIndex, const char* data)
{
    return MakeBin_Write(stackIndex, (void*)data, strlen(data) + 1);
}

template <typename T>
static DataOffset MakeBin_Write(size_t stackIndex, const T& data)
{
    return MakeBin_Write(stackIndex, (void*)&data, sizeof(T));
}

template <typename T>
void MakeBin_WriteInt(size_t stackIndex, const std::string& dflt, const json& json, const json_pointer& path)
{
    T actualValue;
    if (std::is_signed_v<T>)
    {
        int64_t value = GetValueFromString<int64_t>(dflt.c_str());
        value = GetOrDefault(json, path, value);
        actualValue = (T)value;
    }
    else
    {
        uint64_t value = GetValueFromString<uint64_t>(dflt.c_str());
        value = GetOrDefault(json, path, value);
        actualValue = (T)value;
    }

    MakeBin_Write(stackIndex, actualValue);
}

template <typename T>
void MakeBin_WriteFloat(size_t stackIndex, const std::string& dflt, const json& json, const json_pointer& path)
{
    T value = GetValueFromString<T>(dflt.c_str());
    value = GetOrDefault(json, path, value);
    MakeBin_Write(stackIndex, value);
}

static bool MakeBin_WriteField(DBTable& table, const DefParser::StructField& fieldDef, const json& json, const json_pointer& path, size_t stackIndex)
{
    // Figure out how many items are in this array (1 item for non arrays)
    // If this is a dynamic array, write out the number of items.
    uint32_t  arrayItemCount = 1;
    bool fixedSizedArray = false;
    if (fieldDef.isArray)
    {
        if (fieldDef.fixedArraySize > 0)
        {
            fixedSizedArray = true;
            arrayItemCount = fieldDef.fixedArraySize;
        }
        else
        {
            if (json.contains(path))
                arrayItemCount = (uint32_t)json.value(path, json::array()).size();
            else
                arrayItemCount = 0;
            MakeBin_Write(stackIndex, arrayItemCount);
        }
    }

    // Dynamic arrays have their contents written to stackIndex + 1.
    // A pointer is written to stackIndex.
    // That pointer is put into the fixup list for post flattening.
    size_t fieldStackIndex = stackIndex;
    DataOffset dynamicArrayPtr;
    if (fieldDef.isArray && fieldDef.fixedArraySize == 0)
    {
        fieldStackIndex = stackIndex + 1;
        dynamicArrayPtr = MakeBin_Write(stackIndex, (uint64_t)0);
        s_data.dataOffsets.push_back({ MakeBin_GetOffset(fieldStackIndex), dynamicArrayPtr });
    }

    for (uint64_t arrayIndex = 0; arrayIndex < arrayItemCount; ++arrayIndex)
    {
        json_pointer jsonPathItem = path;
        if (fieldDef.isArray)
            jsonPathItem /= arrayIndex;

        // enums are strings in json. store them as a uint16 index in the bin file.
        if (fieldDef.fieldType == DefParser::FieldType::_enum)
        {
            std::string value = fieldDef.dflt;
            value = GetOrDefault(json, jsonPathItem, value);

            const DefParser::Enum* enumDef = table.GetParser().GetEnumByName(fieldDef.enumName.c_str());
            if (!enumDef)
            {
                s_data.error << "Could not find enum \"" << fieldDef.enumName << "\"";
                return false;
            }

            uint64_t index = 0;
            enumDef->GetLabelIndex(value.c_str(), index);
            uint16_t enumValue = (uint16_t)index;

            MakeBin_Write(fieldStackIndex, enumValue);
            continue;
        }

        // write bools as uint8
        if (fieldDef.fieldType == DefParser::FieldType::_bool)
        {
            bool dflt = GetValueFromString<bool>(fieldDef.dflt.c_str());
            bool value = GetOrDefault(json, jsonPathItem, dflt);

            uint8_t boolValue = (value != false);
            MakeBin_Write(fieldStackIndex, boolValue);
            continue;
        }

        // Integers and floats
        switch (fieldDef.fieldType)
        {
            case DefParser::FieldType::_uint8:  MakeBin_WriteInt<uint8_t> (fieldStackIndex, fieldDef.dflt, json, jsonPathItem);  continue;
            case DefParser::FieldType::_sint8:  MakeBin_WriteInt<int8_t>  (fieldStackIndex, fieldDef.dflt, json, jsonPathItem);   continue;
            case DefParser::FieldType::_uint16: MakeBin_WriteInt<uint16_t>(fieldStackIndex, fieldDef.dflt, json, jsonPathItem); continue;
            case DefParser::FieldType::_sint16: MakeBin_WriteInt<int16_t> (fieldStackIndex, fieldDef.dflt, json, jsonPathItem);  continue;
            case DefParser::FieldType::_uint32: MakeBin_WriteInt<uint32_t>(fieldStackIndex, fieldDef.dflt, json, jsonPathItem); continue;
            case DefParser::FieldType::_sint32: MakeBin_WriteInt<int32_t> (fieldStackIndex, fieldDef.dflt, json, jsonPathItem);  continue;
            case DefParser::FieldType::_uint64: MakeBin_WriteInt<uint64_t>(fieldStackIndex, fieldDef.dflt, json, jsonPathItem); continue;
            case DefParser::FieldType::_sint64: MakeBin_WriteInt<int8_t>  (fieldStackIndex, fieldDef.dflt, json, jsonPathItem);   continue;
            case DefParser::FieldType::_float:  MakeBin_WriteFloat<float> (fieldStackIndex, fieldDef.dflt, json, jsonPathItem);  continue;
            case DefParser::FieldType::_double: MakeBin_WriteFloat<double>(fieldStackIndex, fieldDef.dflt, json, jsonPathItem); continue;
        }

        // If it's a string, we write the string to the data block and point to it
        if (fieldDef.fieldType == DefParser::FieldType::_string)
        {
            // Get the string value and write it to dynamic memory
            std::string value = GetOrDefault(json, jsonPathItem, fieldDef.dflt);
            DataOffset stringOffset = MakeBin_Write(fieldStackIndex + 1, value.c_str());

            // write a null for now
            DataOffset ptrOffset = MakeBin_Write(fieldStackIndex, (uint64_t)0);

            // Remember that we want a data offset here, and what it is, so we can fill it in later
            s_data.dataOffsets.push_back({ stringOffset, ptrOffset });

            continue;
        }

        if (fieldDef.fieldType == DefParser::FieldType::_struct)
        {
            const DefParser::Struct* structDef = table.GetParser().GetStructByName(fieldDef.structName.c_str());
            if (!structDef)
            {
                s_data.error << "Could not find struct \"" << fieldDef.structName << "\"";
                return false;
            }
            MakeBin_WriteStruct(table, *structDef, json, path, fieldStackIndex);
            continue;
        }

        if (fieldDef.fieldType == DefParser::FieldType::_link)
        {
            // Remember where this link is and what entry it wants, so we can fill it in later
            std::string value = GetOrDefault(json, jsonPathItem, fieldDef.dflt);

            // Write a null for now
            DataOffset linkOffset = MakeBin_Write(fieldStackIndex, (uint64_t)0);
            s_data.links.push_back({ fieldDef.linkName, value, linkOffset });
            continue;
        }

        s_data.error << "Unhandled field type for entry \"" << fieldDef.name << "\" in table \"" << table.m_rootType << "\"";
        return false;
    }

    return true;
}

static bool MakeBin_WriteStruct(DBTable& table, const DefParser::Struct& structDef, const json& json, const json_pointer& path, size_t stackIndex)
{
    bool ret = true;
    for (const DefParser::StructField& fieldDef : structDef.fields)
    {
        json_pointer fieldPath = path;
        fieldPath /= fieldDef.name.c_str();

        ret &= MakeBin_WriteField(table, fieldDef, json, fieldPath, stackIndex);
    }
    return ret;
}

static bool MakeBin_Table_Item(DBTable& table, const json& json, size_t stackindex)
{
    const DefParser::Struct& structDef = *table.GetParser().GetRootStruct();
    MakeBin_WriteStruct(table, structDef, json, json_pointer(""), stackindex);
    return true;
}

static bool MakeBin_Table(const DBCompileSettings& compilerSettings, DBTable& table)
{
    size_t stackIndex = 0;

    // Write how many items are in this table
    uint32_t numItems = (uint32_t)table.m_data.size();
    MakeBin_Write(stackIndex, numItems);
    if (numItems == 0)
        return true;

    // Write the table entry LUT if we are supposed to include it.
    // It's just a list of names. Their position is their index. They are alpha sorted.
    //
    // In the static data, we write a pointer to N pointers in the dynamic data.
    // Those N pointers point to strings in the dynamic data.
    if (compilerSettings.includeEntryLUT)
    {
        // Write N pointers (as null for right now) into static memory and store their addresses
        std::vector<DataOffset> pointers;
        for (auto& it : table.m_data)
            pointers.push_back(MakeBin_Write(stackIndex, (uint64_t)0));

        // Write the N strings into dynamic memory
        // also remember that we want to put their offsets into those N pointers
        size_t i = 0;
        for (auto& it : table.m_data)
        {
            DataOffset stringOffset = MakeBin_Write(stackIndex+1, it.first.c_str());
            s_data.dataOffsets.push_back({ stringOffset, pointers[i] });
            i++;
        }
    }

    // Write the data
    bool ret = true;
    for (auto& it : table.m_data)
    {
        // remember where this entry is, in the file
        s_data.entries.push_back({ table.m_rootType, it.first, MakeBin_GetOffset(stackIndex) });

        ret &= MakeBin_Table_Item(table, it.second->m_data, stackIndex);
        if (!ret)
            break;
    }

    return ret;
}

static bool MakeBin(const DBCompileSettings& compilerSettings, const DBRoot& dbRoot, const char* fileName, uint64_t hash)
{
    size_t stackIndex = 0;

    // Write a fourcc to verify the file type and endianness when loading
    uint32_t fourcc = MakeFourCC('D', 'F', 'G', 'D');
    MakeBin_Write(stackIndex, fourcc);

    // Write schema hash
    MakeBin_Write(stackIndex, hash);

    bool ret = true;
    for (const auto& it : dbRoot.m_tables)
    {
        ret &= MakeBin_Table(compilerSettings , *it.second.get());
        if (!ret)
            break;
    }

    // Flatten the data stack
    std::vector<uint64_t> stackStart;
    stackStart.push_back(0); // data stack 0 starts at byte 0
    for (size_t i = 1; i < s_data.dataStack.size(); ++i)
    {
        // remember where data stack i starts
        stackStart.push_back(s_data.dataStack[0].size());

        // append data stack i to data stack 0 and clear it out
        std::vector<char>& src = s_data.dataStack[i];
        s_data.dataStack[0].reserve(s_data.dataStack[0].size() + src.size());
        s_data.dataStack[0].insert(s_data.dataStack[0].end(), src.begin(), src.end());
        src.clear();
    }

    // Do fixups for data pointers
    for (const StaticData::DataTableOffsets& dataOffset : s_data.dataOffsets)
    {
        uint64_t dest = dataOffset.destoffset.offset + stackStart[dataOffset.destoffset.stackIndex];
        uint64_t src = dataOffset.srcOffset.offset + stackStart[dataOffset.srcOffset.stackIndex];
        ((uint64_t*)(&s_data.dataStack[0][dest]))[0] = src;
    }

    // Do the fixups for links
    for (const StaticData::TableEntryLocation& linkLocation : s_data.links)
    {
        bool found = false;
        for (const StaticData::TableEntryLocation& entryLocation : s_data.entries)
        {
            if (linkLocation.table != entryLocation.table || linkLocation.entry != entryLocation.entry)
                continue;

            found = true;

            // write the offset
            uint64_t dest = linkLocation.offset.offset + stackStart[linkLocation.offset.stackIndex];
            uint64_t src = entryLocation.offset.offset + stackStart[entryLocation.offset.stackIndex];
            ((uint64_t*)(&s_data.dataStack[0][dest]))[0] = src;
        }
        if (found)
            continue;

        s_data.error << "Could not find entry \"" << linkLocation.entry << "\" in table \"" << linkLocation.table << "\"";
        ret = false;
        break;
    }

    if (!ret)
        return false;

    // write the data to disk
    FILE* file = nullptr;
    fopen_s(&file, fileName, "wb");
    if (!file)
    {
        s_data.error << "Could not write to " << fileName;
        return false;
    }
    fwrite(s_data.dataStack[0].data(), s_data.dataStack[0].size(), 1, file);
    fclose(file);

    return true;
}

bool Compile(const DBRoot& dbRoot, const DBCompileSettings& compilerSettings, std::string& error)
{
    s_data = StaticData();

    if (dbRoot.m_tables.size() == 0)
    {
        s_data.error << "No tables in database";
        error = s_data.error.str();
        return false;
    }

    // calculate schema hash
    Hasher hash(0xbeefcafe);
    for (auto& it : dbRoot.m_tables)
        hash.Add(it.second->GetParser().GetHash());
    hash.Add(compilerSettings.includeEntryLUT);

    std::string dbRootPath = std::filesystem::path(dbRoot.GetPath()).remove_filename().generic_string();

    std::string fileNameBin = std::filesystem::weakly_canonical(std::filesystem::path(dbRootPath) / compilerSettings.compiledBinFileName).generic_string();
    std::string fileNameHeader = std::filesystem::weakly_canonical(std::filesystem::path(dbRootPath) / compilerSettings.compiledHeaderFileName).generic_string();

    bool ret = MakeBin(compilerSettings, dbRoot, fileNameBin.c_str(), hash.Result()) && MakeHeader(compilerSettings, dbRoot, fileNameHeader.c_str(), hash.Result());

    error = s_data.error.str();
    return ret;
}

/*
TODO:
* do pointer fixup on the things you load at the root, then have some functions for each type to do pointer fixup
* dynamic arrays should go in dynamic data. static arrays should go inline. every function that writes needs to get a bool for if it's writing to static or not.
* need to use it for a bit before announcing. adding array items in the editor is crashing
*/

/*
TODO:
* in loader, fixing up endianness is a conditional pass on the data.
*/
