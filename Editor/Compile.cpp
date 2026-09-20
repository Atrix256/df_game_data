#include "Compile.h"

#include "Editor.h"

#include <sstream>

#include "../loader/hash.h"

#include <type_traits>

static std::ostringstream s_error;

static bool MakeBin_WriteStruct(EditorData& editorData, FILE* file, const DefParser::Struct& structDef, const json& json, const json_pointer& path);

static bool MakeHeader(EditorData& editorData, const char* fileName, uint64_t hash)
{
    // TODO: this. Maybe in a seperate file
    s_error << "Writing header not yet implemented";
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

            const DefParser::Enum& e = *table.GetParser().GetEnumByName(fieldDef.enumName.c_str());

            size_t index = 0;
            e.GetLabelIndex(value.c_str(), index);
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

        // TODO: continue
        int ijkl = 0;

        // TODO: strings
        // TODO: links
        // TODO: structs
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
    // TODO: The compile settings which control the contents of the .bin file should be included in the hash.
    Hasher hash(0xbeefcafe);
    for (auto& it : editorData.m_dbroot.m_tables)
        hash.Add(it.second->GetParser().GetHash());

    std::string dbRootPath = std::filesystem::path(editorData.m_dbroot.GetPath()).remove_filename().generic_string();

    std::string fileNameBin = std::filesystem::weakly_canonical(std::filesystem::path(dbRootPath) / editorData.m_dbroot.m_settings.compiledBinFileName).generic_string();
    std::string fileNameHeader = std::filesystem::weakly_canonical(std::filesystem::path(dbRootPath) / editorData.m_dbroot.m_settings.compiledHeaderFileName).generic_string();

    bool ret = MakeBin(editorData, fileNameBin.c_str(), hash.Result()) && MakeHeader(editorData, fileNameHeader.c_str(), hash.Result());

    error = s_error.str();
    return ret;
}

/*
TODO:
* can we make editor data const when it's passed in?
* may want to move the bin and header code into separate files for organization purposes
*/
