#include "Compile.h"

#include "Editor.h"

#include <sstream>

#include "../loader/hash.h"

#include <type_traits>

#include "../loader/output_string.h"

struct StaticData
{
    std::ostringstream error;

    // Data to hold strings and other data that is pointed to, but not stored directly inline.
    std::vector<char> data;

    struct TableEntryLocation
    {
        std::string table;
        std::string entry;
        uint64_t fileOffset;
    };

    // Places in the file that want offsets to entries in tables
    std::vector<TableEntryLocation> links;

    // Where the entries live in the file
    std::vector<TableEntryLocation> entries;

    struct DataTableOffsets
    {
        uint64_t fileOffset;
        uint64_t dataOffset;
    };

    // Places in the file that point at an offset in the data, and what that offset is
    std::vector<DataTableOffsets> dataOffsets;

    // A map to replace tokens in output.h with strings
    std::unordered_map<std::string, std::ostringstream> tokenReplacement;
};
static StaticData s_data;

static bool MakeBin_WriteStruct(FILE* file, DBTable& table, const DefParser::Struct& structDef, const json& json, const json_pointer& path);

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
    for (const auto& pair : dbRoot.m_tables)
    {
        const DefParser& parser = pair.second->GetParser();

        bool ret = parser.ForEachEnum(
            [&os](const DefParser::Enum& e)
            {
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
    for (const auto& pair : dbRoot.m_tables)
    {
        const DefParser& parser = pair.second->GetParser();

        bool ret = parser.ForEachStruct(
            [&os](const DefParser::Struct& s)
            {
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
                    // Arrays are a count, and a pointer into the data block
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

                    // the field type
                    os << indent << "    ";
                    switch (field.fieldType)
                    {
                        case DefParser::FieldType::_bool: os << "uint8_t"; break;
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
    std::ostringstream& privateStorage = s_data.tokenReplacement["/*$PrivateStorage$*/"];

    for (const auto& pair : dbRoot.m_tables)
    {
        std::string indent = "    ";

        const DefParser& parser = pair.second->GetParser();

        // Make an extra newline to separate them
        privateStorage << "\n";

        privateStorage << indent << "uint32_t m_table_" << parser.GetRootStructName() << "_count = 0;\n";

        // The sorted list of table names. Tables are written in sorted order
        if (compilerSettings.includeEntryLUT)
            privateStorage << indent << "Ptr64<char> *m_table_" << parser.GetRootStructName() << "_Names = 0;\n";

        privateStorage << indent << "Ptr64<" << parser.GetRootStructName() << "> m_table_" << parser.GetRootStructName() << ";\n";
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

template <typename T>
void MakeBin_WriteInt(FILE* file, const std::string& dflt, const json& json, const json_pointer& path)
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

    fwrite(&actualValue, sizeof(actualValue), 1, file);
}

template <typename T>
void MakeBin_WriteFloat(FILE* file, const std::string& dflt, const json& json, const json_pointer& path)
{
    T value = GetValueFromString<T>(dflt.c_str());
    value = GetOrDefault(json, path, value);
    fwrite(&value, sizeof(value), 1, file);
}

static bool MakeBin_WriteField(FILE* file, DBTable& table, const DefParser::StructField& fieldDef, const json& json, const json_pointer& path)
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
            fwrite(&arrayItemCount, sizeof(arrayItemCount), 1, file);
        }
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

            fwrite(&enumValue, sizeof(enumValue), 1, file);
            continue;
        }

        // write bools as uint8
        if (fieldDef.fieldType == DefParser::FieldType::_bool)
        {
            bool dflt = GetValueFromString<bool>(fieldDef.dflt.c_str());
            bool value = GetOrDefault(json, jsonPathItem, dflt);

            uint8_t boolValue = (value != false);
            fwrite(&boolValue, sizeof(boolValue), 1, file);
            continue;
        }

        // Integers and floats
        switch (fieldDef.fieldType)
        {
            case DefParser::FieldType::_uint8: MakeBin_WriteInt<uint8_t>(file, fieldDef.dflt, json, jsonPathItem); continue;
            case DefParser::FieldType::_sint8: MakeBin_WriteInt<int8_t>(file, fieldDef.dflt, json, jsonPathItem); continue;
            case DefParser::FieldType::_uint16: MakeBin_WriteInt<uint16_t>(file, fieldDef.dflt, json, jsonPathItem); continue;
            case DefParser::FieldType::_sint16: MakeBin_WriteInt<int16_t>(file, fieldDef.dflt, json, jsonPathItem); continue;
            case DefParser::FieldType::_uint32: MakeBin_WriteInt<uint32_t>(file, fieldDef.dflt, json, jsonPathItem); continue;
            case DefParser::FieldType::_sint32: MakeBin_WriteInt<int32_t>(file, fieldDef.dflt, json, jsonPathItem); continue;
            case DefParser::FieldType::_uint64: MakeBin_WriteInt<uint64_t>(file, fieldDef.dflt, json, jsonPathItem); continue;
            case DefParser::FieldType::_sint64: MakeBin_WriteInt<int8_t>(file, fieldDef.dflt, json, jsonPathItem); continue;
            case DefParser::FieldType::_float: MakeBin_WriteFloat<float>(file, fieldDef.dflt, json, jsonPathItem); continue;
            case DefParser::FieldType::_double: MakeBin_WriteFloat<double>(file, fieldDef.dflt, json, jsonPathItem); continue;
        }

        // If it's a string, we write the string to the data block and point to it
        if (fieldDef.fieldType == DefParser::FieldType::_string)
        {
            std::string value = GetOrDefault(json, jsonPathItem, fieldDef.dflt);
            uint64_t dataOffset = (uint64_t)s_data.data.size();
            uint64_t bytes = (uint64_t)value.length() + 1;

            // Write to the data block
            s_data.data.resize(dataOffset + bytes);
            memcpy(&s_data.data[dataOffset], value.c_str(), bytes);

            // Remember that we want a data offset here, and what it is, so we can fill it in later
            s_data.dataOffsets.push_back({ (uint64_t)ftell(file), dataOffset });

            // write a null for now
            static const uint64_t offset = 0;
            fwrite(&offset, sizeof(offset), 1, file);
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
            MakeBin_WriteStruct(file, table, *structDef, json, path);
            continue;
        }

        if (fieldDef.fieldType == DefParser::FieldType::_link)
        {
            // Remember where this link is and what entry it wants, so we can fill it in later
            std::string value = GetOrDefault(json, jsonPathItem, fieldDef.dflt);
            s_data.links.push_back({ fieldDef.linkName, value, (uint64_t)ftell(file) });

            // Write a null for now
            static const uint64_t offset = 0;
            fwrite(&offset, sizeof(offset), 1, file);
            continue;
        }

        s_data.error << "Unhandled field type for entry \"" << fieldDef.name << "\" in table \"" << table.m_rootType << "\"";
        return false;
    }

    return true;
}

static bool MakeBin_WriteStruct(FILE* file, DBTable& table, const DefParser::Struct& structDef, const json& json, const json_pointer& path)
{
    bool ret = true;
    for (const DefParser::StructField& fieldDef : structDef.fields)
    {
        json_pointer fieldPath = path;
        fieldPath /= fieldDef.name.c_str();

        ret &= MakeBin_WriteField(file, table, fieldDef, json, fieldPath);
    }
    return ret;
}

static bool MakeBin_Table_Item(DBTable& table, const json& json, FILE* file)
{
    const DefParser::Struct& structDef = *table.GetParser().GetRootStruct();
    MakeBin_WriteStruct(file, table, structDef, json, json_pointer(""));
    return true;
}

static bool MakeBin_Table(const DBCompileSettings& compilerSettings, DBTable& table, FILE* file)
{
    // Write how many items are in this table
    uint32_t numItems = (uint32_t)table.m_data.size();
    fwrite(&numItems, sizeof(numItems), 1, file);

    // Write the table entry LUT if we are supposed to include it
    if (compilerSettings.includeEntryLUT)
    {
        struct EntryLutItem
        {
            std::string name;
            uint32_t index;
        };

        std::vector<EntryLutItem> entries;

        uint32_t index = 0;
        for (auto& it : table.m_data)
            entries.push_back({ it.first, index++ });

        std::sort(entries.begin(), entries.end(),
            [](const EntryLutItem& a, const EntryLutItem& b)
            {
                return a.name < b.name;
            }
        );

        for (EntryLutItem& entry : entries)
        {
            // First write the name (pointer into data block)
            {
                uint64_t dataOffset = (uint64_t)s_data.data.size();
                uint64_t bytes = (uint64_t)entry.name.length() + 1;

                // write the string to the data block
                s_data.data.resize(dataOffset + bytes);
                memcpy(&s_data.data[dataOffset], entry.name.c_str(), bytes);

                // Remember that we want a data offset here, and what it is, so we can fill it in later
                s_data.dataOffsets.push_back({ (uint64_t)ftell(file), dataOffset });

                // write a null for now
                static const uint64_t offset = 0;
                fwrite(&offset, sizeof(offset), 1, file);
            }

            // Then write the index
            fwrite(&entry.index, sizeof(entry.index), 1, file);
        }
    }

    bool ret = true;
    for (auto& it : table.m_data)
    {
        // remember where this entry is, in the file
        s_data.entries.push_back({ table.m_rootType, it.first, (uint64_t)ftell(file) });

        ret &= MakeBin_Table_Item(table, it.second->m_data, file);
        if (!ret)
            break;
    }

    return ret;
}

static bool MakeBin(const DBCompileSettings& compilerSettings, const DBRoot& dbRoot, const char* fileName, uint64_t hash)
{
    FILE* file = nullptr;
    fopen_s(&file, fileName, "wb");
    if (!file)
    {
        s_data.error << "Could not write to " << fileName;
        return false;
    }

    // Write a fourcc to verify the file type and endianness when loading
    uint32_t fourcc = MakeFourCC('D', 'F', 'G', 'D');
    fwrite(&fourcc, sizeof(fourcc), 1, file);

    // Write schema hash
    fwrite(&hash, sizeof(hash), 1, file);

    bool ret = true;
    for (const auto& it : dbRoot.m_tables)
    {
        ret &= MakeBin_Table(compilerSettings , *it.second.get(), file);
        if (!ret)
            break;
    }

    // Write the data block
    uint64_t dataStart = (uint64_t)ftell(file);
    fwrite(s_data.data.data(), 1, s_data.data.size(), file);

    // Do fixups for data pointers
    for (const StaticData::DataTableOffsets& dataOffset : s_data.dataOffsets)
    {
        fseek(file, (long)dataOffset.fileOffset, SEEK_SET);
        uint64_t offset = dataStart + dataOffset.dataOffset;
        fwrite(&offset, sizeof(offset), 1, file);
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
            fseek(file, (long)linkLocation.fileOffset, SEEK_SET);
            fwrite(&entryLocation.fileOffset, sizeof(entryLocation.fileOffset), 1, file);
        }
        if (found)
            continue;

        s_data.error << "Could not find entry \"" << linkLocation.entry << "\" in table \"" << linkLocation.table << "\"";
        ret = false;
        break;
    }

    fclose(file);

    return ret;
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
* when opening test.def, it makes a dbroot without compile settings, is that ok?
* need to use it for a bit before announcing. adding array items in the editor is crashing
* If the names are written in sorted order, don't need to write index, for LUT. I think they are. verify. comment that in the code if so
*/

/*
TODO:
* in loader, fixing up endianness is a conditional pass on the data.
*/
