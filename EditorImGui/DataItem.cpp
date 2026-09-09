#include "DataItem.h"

#include "flatbuffers/idl.h"
#include "../loader/JSON.h"
#include "Editor.h"
#include "imgui.h"
#include <vector>

static void MarkDirty(EditorData& editorData, DBTable::JSONData& jsonData)
{
    editorData.m_documentDirty = true;
    editorData.m_updateWindowTitle = true;
    jsonData.m_dirty = true;
}

static void AddUIForType(EditorData& editorData, const flatbuffers::Parser& parser, const flatbuffers::StructDef& structDef, const char* structFieldName, DBTable::JSONData& jsonData, const json_pointer& path);

enum class TypeCategory
{
    Unknown,
    Bool,
    Int,
    Float,
    String,
    Struct,
};

union TypeDetails
{
    struct
    {
        bool isSigned;
        int numBytes;
    }
    Int;

    struct
    {
        bool isDouble;
    }
    Float;

    struct
    {
        flatbuffers::StructDef* structDef;
    }
    Struct;
};

struct Type
{
    TypeCategory category = TypeCategory::Unknown;
    TypeDetails details;

    bool isVector = false;
    uint16_t vectorSize = 0;
};

void FlatBufferBaseTypeToOurType(const flatbuffers::Type& type, const flatbuffers::BaseType& baseType, TypeCategory& category, TypeDetails& details)
{
    switch (baseType)
    {
        case flatbuffers::BaseType::BASE_TYPE_NONE:
        {
            break;
        }
        case flatbuffers::BaseType::BASE_TYPE_UTYPE:
        {
            category = TypeCategory::Int;
            details.Int.isSigned = false;
            details.Int.numBytes = 1;
            break;
        }
        case flatbuffers::BaseType::BASE_TYPE_BOOL:
        {
            category = TypeCategory::Bool;
            break;
        }
        case flatbuffers::BaseType::BASE_TYPE_CHAR:
        {
            category = TypeCategory::Int;
            details.Int.isSigned = true;
            details.Int.numBytes = 1;
            break;
        }
        case flatbuffers::BaseType::BASE_TYPE_UCHAR:
        {
            category = TypeCategory::Int;
            details.Int.isSigned = false;
            details.Int.numBytes = 1;
            break;
        }
        case flatbuffers::BaseType::BASE_TYPE_SHORT:
        {
            category = TypeCategory::Int;
            details.Int.isSigned = true;
            details.Int.numBytes = 2;
            break;
        }
        case flatbuffers::BaseType::BASE_TYPE_USHORT:
        {
            category = TypeCategory::Int;
            details.Int.isSigned = false;
            details.Int.numBytes = 2;
            break;
        }
        case flatbuffers::BaseType::BASE_TYPE_INT:
        {
            category = TypeCategory::Int;
            details.Int.isSigned = true;
            details.Int.numBytes = 4;
            break;
        }
        case flatbuffers::BaseType::BASE_TYPE_UINT:
        {
            category = TypeCategory::Int;
            details.Int.isSigned = false;
            details.Int.numBytes = 4;
            break;
        }
        case flatbuffers::BaseType::BASE_TYPE_LONG:
        {
            category = TypeCategory::Int;
            details.Int.isSigned = true;
            details.Int.numBytes = 8;
            break;
        }
        case flatbuffers::BaseType::BASE_TYPE_ULONG:
        {
            category = TypeCategory::Int;
            details.Int.isSigned = false;
            details.Int.numBytes = 8;
            break;
        }
        case flatbuffers::BaseType::BASE_TYPE_FLOAT:
        {
            category = TypeCategory::Float;
            details.Float.isDouble = false;
            break;
        }
        case flatbuffers::BaseType::BASE_TYPE_DOUBLE:
        {
            category = TypeCategory::Float;
            details.Float.isDouble = true;
            break;
        }
        case flatbuffers::BaseType::BASE_TYPE_STRING:
        {
            category = TypeCategory::String;
            break;
        }
        case flatbuffers::BaseType::BASE_TYPE_STRUCT:
        {
            category = TypeCategory::Struct;
            details.Struct.structDef = type.struct_def;
            break;
        }
    }
}

Type FlatBufferTypeToOurType(const flatbuffers::Type& type)
{
    Type ret;

    if (type.base_type == flatbuffers::BaseType::BASE_TYPE_VECTOR || type.base_type == flatbuffers::BaseType::BASE_TYPE_VECTOR64 || type.base_type == flatbuffers::BaseType::BASE_TYPE_ARRAY)
    {
        ret.isVector = true;
        ret.vectorSize = type.fixed_length;
        FlatBufferBaseTypeToOurType(type, type.element, ret.category, ret.details);
    }
    else
    {
        FlatBufferBaseTypeToOurType(type, type.base_type, ret.category, ret.details);
    }

    return ret;
}

template <typename T>
T GetValueFromString(const char* valueStr);

template <>
bool GetValueFromString<bool>(const char* valueStr)
{
    return (!_stricmp(valueStr, "true") || !_stricmp(valueStr, "1"));
}

template <>
int64_t GetValueFromString<int64_t>(const char* valueStr)
{
    int64_t value;
    sscanf_s(valueStr, "%lld", &value);
    return value;
}

template <>
uint64_t GetValueFromString<uint64_t>(const char* valueStr)
{
    uint64_t value;
    sscanf_s(valueStr, "%llu", &value);
    return value;
}

template <>
double GetValueFromString<double>(const char* valueStr)
{
    double value;
    sscanf_s(valueStr, "%lf", &value);
    return value;
}

template <>
float GetValueFromString<float>(const char* valueStr)
{
    float value;
    sscanf_s(valueStr, "%f", &value);
    return value;
}

static void AddUIForType(EditorData& editorData, const flatbuffers::Parser& parser, const flatbuffers::FieldDef& fieldDef, DBTable::JSONData& jsonData, const json_pointer& jsonPath)
{
    if (fieldDef.deprecated)
        return;

    Type type = FlatBufferTypeToOurType(fieldDef.value.type);

    if (type.category == TypeCategory::Unknown)
        return;

    // Figure out how many items are in this array (1 item for non arrays)
    size_t arrayItemCount = 1;
    if (type.isVector)
    {
        if (type.vectorSize > 0)
            arrayItemCount = type.vectorSize;
        else
            arrayItemCount = jsonData.m_data.value(jsonPath, json::array()).size();

        if (!ImGui::TreeNodeEx(fieldDef.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            return;
    }

    // Add the controls
    for (size_t arrayIndex = 0; arrayIndex < arrayItemCount; ++arrayIndex)
    {
        json_pointer jsonPathItem = jsonPath;
        std::string fieldName = fieldDef.name;

        if (type.isVector)
        {
            jsonPathItem /= arrayIndex;

            char buffer[256];
            sprintf_s(buffer, "%zu", arrayIndex);
            fieldName = buffer;
        }

        switch (type.category)
        {
            case TypeCategory::Bool:
            {
                bool value = jsonData.m_data.value(jsonPathItem, GetValueFromString<bool>(fieldDef.value.constant.c_str()));
                if (ImGui::Checkbox(fieldName.c_str(), &value))
                {
                    jsonData.m_data[jsonPathItem] = value;
                    MarkDirty(editorData, jsonData);
                }
                break;
            }
            case TypeCategory::Int:
            {
                unsigned int step_one = 1;
                unsigned int step_fast = 10;

                if (type.details.Int.isSigned)
                {
                    int64_t value = GetValueFromString<int64_t>(fieldDef.value.constant.c_str());
                    value = jsonData.m_data.value(jsonPathItem, value);
                    if (ImGui::InputScalar(fieldName.c_str(), ImGuiDataType_S64, &value, &step_one, &step_fast, "%zi"))
                    {
                        jsonData.m_data[jsonPathItem] = value;
                        MarkDirty(editorData, jsonData);
                    }
                }
                else
                {
                    int64_t value = GetValueFromString<int64_t>(fieldDef.value.constant.c_str());
                    value = jsonData.m_data.value(jsonPathItem, value);
                    if (ImGui::InputScalar(fieldName.c_str(), ImGuiDataType_U64, &value, &step_one, &step_fast, "%zu"))
                    {
                        jsonData.m_data[jsonPathItem] = value;
                        MarkDirty(editorData, jsonData);
                    }
                }

                break;
            }
            case TypeCategory::Float:
            {
                if (type.details.Float.isDouble)
                {
                    double value = GetValueFromString<double>(fieldDef.value.constant.c_str());
                    value = jsonData.m_data.value(jsonPathItem, value);
                    if (ImGui::InputDouble(fieldName.c_str(), &value))
                    {
                        jsonData.m_data[jsonPathItem] = value;
                        MarkDirty(editorData, jsonData);
                    }
                }
                else
                {
                    float value = GetValueFromString<float>(fieldDef.value.constant.c_str());
                    value = jsonData.m_data.value(jsonPathItem, value);
                    if (ImGui::InputFloat(fieldName.c_str(), &value))
                    {
                        jsonData.m_data[jsonPathItem] = value;
                        MarkDirty(editorData, jsonData);
                    }
                }
                break;
            }
            case TypeCategory::String:
            {
                std::string value = jsonData.m_data.value(jsonPathItem, fieldDef.value.constant.c_str());
                static std::vector<char> tmpBuffer;
                tmpBuffer.resize(4096);
                strcpy_s(tmpBuffer.data(), tmpBuffer.size(), value.c_str());

                if (ImGui::InputText(fieldName.c_str(), tmpBuffer.data(), tmpBuffer.size()))
                {
                    jsonData.m_data[jsonPathItem] = tmpBuffer.data();
                    MarkDirty(editorData, jsonData);
                }
                break;
            }
            case TypeCategory::Struct:
            {
                AddUIForType(editorData, parser, *type.details.Struct.structDef, fieldName.c_str(), jsonData, jsonPathItem);
                break;
            }
        }
    }

    if (type.isVector)
        ImGui::TreePop();

    /*
    TODO:
    * all the types. union, enum, arrays of structs, etc
    */
}

static void AddUIForType(EditorData& editorData, const flatbuffers::Parser& parser, const flatbuffers::StructDef& structDef, const char* structFieldName, DBTable::JSONData& jsonData, const json_pointer& path)
{
    if (ImGui::TreeNodeEx(structFieldName, ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Add the fields
        for (const flatbuffers::FieldDef* fieldDef : structDef.fields.vec)
        {
            json_pointer fieldPath = path;
            fieldPath /= fieldDef->name.c_str();
            AddUIForType(editorData, parser, *fieldDef, jsonData, fieldPath);
        }

        ImGui::TreePop();
    }
}

void ShowDataEditor(EditorData& editorData)
{
    if (editorData.m_dbroot.m_tables.count(editorData.m_selectedTableName) == 0)
        return;
    DBTable& table = *editorData.m_dbroot.m_tables[editorData.m_selectedTableName].get();

    if (table.m_data.count(editorData.m_selectedDataItemName) == 0)
        return;
    DBTable::JSONData& data = *table.m_data[editorData.m_selectedDataItemName].get();

    const flatbuffers::Parser& parser = table.GetParser();

    AddUIForType(editorData, parser, *parser.root_struct_def_, parser.root_struct_def_->name.c_str(), data, json_pointer(""));
}

// TODO: use the documentation field as tooltips
// TODO: buttons for arrays. move up, move down, new, delete
