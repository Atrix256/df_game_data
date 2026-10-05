#include "../loader/loader.h"
#include "../Version.h"
#include "../Utils.h"

struct DataOffset
{
    size_t stackIndex = 0;
    size_t offset = 0;
};

namespace
{
    struct StaticData
    {
        std::ostringstream error;

        // We start by writing to stack index 0.
        // When writing into a specific data stack...
        // * static sized objects go there
        // * dynamic sized objects go one stack higher and get a pointer in the current stack (strings, dynamic arrays)
        // This makes it so objects are always fixed size, making it easier to iterate through arrays.
        std::vector<std::vector<char>> dataStack;

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
    };
    static StaticData s_data;
}

static bool MakeBin_WriteStruct(DBTable& table, const DefParser::Struct& structDef, const json& json, const json_pointer& path, size_t stackIndex);

static inline constexpr uint32_t MakeFourCC(char a, char b, char c, char d)
{
    return (uint32_t)(uint8_t)a
        | ((uint32_t)(uint8_t)b << 8)
        | ((uint32_t)(uint8_t)c << 16)
        | ((uint32_t)(uint8_t)d << 24);
}

// From https://jcgt.org/published/0009/03/02/
// supplemental material uint pcg(uint v)
static inline uint32_t pcg_hash(uint32_t input)
{
    uint32_t state = input * 747796405u + 2891336453u;
    uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
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
        // if no items, leave it as a null ptr
        if (arrayItemCount != 0)
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
            case DefParser::FieldType::_uint8:  MakeBin_WriteInt<uint8_t> (fieldStackIndex, fieldDef.dflt, json, jsonPathItem); continue;
            case DefParser::FieldType::_sint8:  MakeBin_WriteInt<int8_t>  (fieldStackIndex, fieldDef.dflt, json, jsonPathItem); continue;
            case DefParser::FieldType::_uint16: MakeBin_WriteInt<uint16_t>(fieldStackIndex, fieldDef.dflt, json, jsonPathItem); continue;
            case DefParser::FieldType::_sint16: MakeBin_WriteInt<int16_t> (fieldStackIndex, fieldDef.dflt, json, jsonPathItem); continue;
            case DefParser::FieldType::_uint32: MakeBin_WriteInt<uint32_t>(fieldStackIndex, fieldDef.dflt, json, jsonPathItem); continue;
            case DefParser::FieldType::_sint32: MakeBin_WriteInt<int32_t> (fieldStackIndex, fieldDef.dflt, json, jsonPathItem); continue;
            case DefParser::FieldType::_uint64: MakeBin_WriteInt<uint64_t>(fieldStackIndex, fieldDef.dflt, json, jsonPathItem); continue;
            case DefParser::FieldType::_sint64: MakeBin_WriteInt<int64_t> (fieldStackIndex, fieldDef.dflt, json, jsonPathItem); continue;
            case DefParser::FieldType::_float:  MakeBin_WriteFloat<float> (fieldStackIndex, fieldDef.dflt, json, jsonPathItem); continue;
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

            // unions write a uint16 for which they are, then a dynamic pointer to the data for that field.
            if (structDef->isUnion)
            {
                // Write the type, which is a uint16: We write fieldIndex + 1, so that 0 is "none"
                // Then write a pointer to the data in the next data stack down, since the size is variable.
                json_pointer pathType = jsonPathItem / "_type";

                std::string value = fieldDef.dflt;
                value = GetOrDefault(json, pathType, value);

                uint16_t index = 0;
                bool found = false;
                for (const DefParser::StructField& field : structDef->fields)
                {
                    index++;
                    if (field.name == value)
                    {
                        // Write the type, and a pointer.
                        // Tell it to do pointer fixup to make this pointer point at where we are going to write the field.
                        MakeBin_Write(fieldStackIndex, index);
                        DataOffset dst = MakeBin_Write(fieldStackIndex, (uint64_t)0);
                        DataOffset src = MakeBin_GetOffset(fieldStackIndex + 1);
                        s_data.dataOffsets.push_back({ src, dst });

                        json_pointer pathValue = jsonPathItem / field.name;

                        MakeBin_WriteField(table, field, json, pathValue, fieldStackIndex + 1);

                        found = true;
                        break;
                    }
                }

                // If no type chosen:
                // 1) write the uint16_t type out as none
                // 2) write a nullptr for the data
                if (!found)
                {
                    MakeBin_Write(fieldStackIndex, (uint16_t)0);
                    MakeBin_Write(fieldStackIndex, (uint64_t)0);
                }
            }
            else
            {
                MakeBin_WriteStruct(table, *structDef, json, jsonPathItem, fieldStackIndex);
            }
            continue;
        }

        if (fieldDef.fieldType == DefParser::FieldType::_link)
        {
            // Remember where this link is and what entry it wants, so we can fill it in later
            std::string value = GetOrDefault(json, jsonPathItem, fieldDef.dflt);

            // Write a null for now. Update later if there is an actual link specified. if not, leave null.
            DataOffset linkOffset = MakeBin_Write(fieldStackIndex, (uint64_t)0);
            if (!value.empty())
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

bool MakeBin(const DBCompileSettings& compilerSettings, const DBRoot& dbRoot, const char* fileName, uint64_t hash, std::string& error)
{
    s_data = StaticData();

    // Grab the error string as we exit this function
    AT_EXIT(error = s_data.error.str());

    size_t stackIndex = 0;

    // Write a fourcc to verify the file type and endianness when loading
    uint32_t fourcc = MakeFourCC('D', 'F', 'G', 'D');
    MakeBin_Write(stackIndex, fourcc);

    // Write the version
    MakeBin_Write(stackIndex, (uint8_t)VERSION_MAJOR);
    MakeBin_Write(stackIndex, (uint8_t)VERSION_MINOR);
    MakeBin_Write(stackIndex, (uint8_t)VERSION_PATCH);

    // Write schema hash
    MakeBin_Write(stackIndex, hash);

    bool ret = true;
    for (const auto& it : dbRoot.m_tables)
    {
        ret &= MakeBin_Table(compilerSettings, *it.second.get());
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

    // Obfuscate the data if that setting is on.
    if (compilerSettings.obfuscation)
    {
        size_t contentStart = 7; // start after the fourcc and the 3 byte version
        size_t bytesRemaining = s_data.dataStack[0].size() - contentStart;
        uint32_t rng = pcg_hash(0x1337beef);
        uint8_t* data = (uint8_t*)&s_data.dataStack[0][contentStart];
        while (bytesRemaining > 0)
        {
            rng = pcg_hash(rng);

            for (size_t i = 0; i < ((bytesRemaining < 4) ? bytesRemaining : 4); ++i)
                data[i] = data[i] ^ ((uint8_t*)&rng)[i];

            data += 4;

            if (bytesRemaining >= 4)
                bytesRemaining -= 4;
            else
                bytesRemaining = 0;
        }
    }

    // Create any directories needed
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(fileName).remove_filename().generic_string().c_str(), ec);

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
