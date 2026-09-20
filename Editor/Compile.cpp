#include "Compile.h"

#include "Editor.h"

#include <sstream>

#include "../loader/hash.h"

#include <type_traits>

struct StaticData
{
    std::ostringstream error;

    // Data to hold strings and other data that is pointed to, but not stored directly inline.
    std::vector<char> data;
};
static StaticData s_data;

static bool MakeBin_WriteStruct(EditorData& editorData, FILE* file, DBTable& table, const DefParser::Struct& structDef, const json& json, const json_pointer& path);

static bool MakeHeader(EditorData& editorData, const char* fileName, uint64_t hash)
{
    // TODO: this. Maybe in a seperate file
    s_data.error << "Writing header not yet implemented";
    return false;
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

static bool MakeBin_WriteField(EditorData& editorData, FILE* file, DBTable& table, const DefParser::StructField& fieldDef, const json& json, const json_pointer& path)
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

    for (size_t arrayIndex = 0; arrayIndex < arrayItemCount; ++arrayIndex)
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

            size_t index = 0;
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

            // write the data offset to the bin file
            fwrite(&dataOffset, sizeof(dataOffset), 1, file);
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
            MakeBin_WriteStruct(editorData, file, table, *structDef, json, path);
            continue;
        }

        // TODO: continue
        int ijkl = 0;

        // TODO: links
        // TODO: what else?
    }

    // TODO: continue
    return false;
}

static bool MakeBin_WriteStruct(EditorData& editorData, FILE* file, DBTable& table, const DefParser::Struct& structDef, const json& json, const json_pointer& path)
{
    bool ret = true;
    for (const DefParser::StructField& fieldDef : structDef.fields)
    {
        json_pointer fieldPath = path;
        fieldPath /= fieldDef.name.c_str();

        ret &= MakeBin_WriteField(editorData, file, table, fieldDef, json, fieldPath);
    }
    return ret;
}

static bool MakeBin_Table_Item(EditorData& editorData, DBTable& table, const json& json, FILE* file)
{
    const DefParser::Struct& structDef = *table.GetParser().GetRootStruct();
    MakeBin_WriteStruct(editorData, file, table, structDef, json, json_pointer(""));
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

static bool MakeBin(EditorData& editorData, const char* fileName, uint64_t hash)
{
    FILE* file = nullptr;
    fopen_s(&file, fileName, "wb");
    if (!file)
    {
        s_data.error << "Could not write to " << fileName;
        return false;
    }

    // Write a fourcc to verify the file type and endianness.
    const char* fourcc = "DFGD";
    fwrite(fourcc, 1, 4, file);

    // Write schema hash
    fwrite(&hash, sizeof(hash), 1, file);

    bool ret = true;
    for (auto& it : editorData.m_dbroot.m_tables)
    {
        ret &= MakeBin_Table(editorData, *it.second.get(), file);
        if (!ret)
            break;
    }

    // Write the data block
    fwrite(s_data.data.data(), 1, s_data.data.size(), file);

    fclose(file);

    return ret;
}

bool Compile(EditorData& editorData, std::string& error)
{
    s_data = StaticData();

    if (editorData.m_dbroot.m_tables.size() == 0)
    {
        s_data.error << "No tables in database";
        error = s_data.error.str();
        return false;
    }

    // calculate schema hash
    // TODO: The compile settings which control the contents of the .bin file should be included in the hash.
    Hasher hash(0xbeefcafe);
    for (auto& it : editorData.m_dbroot.m_tables)
        hash.Add(it.second->GetParser().GetHash());

    std::string dbRootPath = std::filesystem::path(editorData.m_dbroot.GetPath()).remove_filename().generic_string();

    std::string fileNameBin = std::filesystem::weakly_canonical(std::filesystem::path(dbRootPath) / editorData.m_dbroot.m_settings.compiledBinFileName).generic_string();
    std::string fileNameHeader = std::filesystem::weakly_canonical(std::filesystem::path(dbRootPath) / editorData.m_dbroot.m_settings.compiledHeaderFileName).generic_string();

    bool ret = MakeBin(editorData, fileNameBin.c_str(), hash.Result()) && MakeHeader(editorData, fileNameHeader.c_str(), hash.Result());

    error = s_data.error.str();
    return ret;
}

/*
TODO:
* can we make editor data const when it's passed in?
* may want to move the bin and header code into separate files for organization purposes
*/
