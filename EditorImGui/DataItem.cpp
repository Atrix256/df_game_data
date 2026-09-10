#include "DataItem.h"

#include "flatbuffers/idl.h"
#include "../loader/JSON.h"
#include "Editor.h"
#include "imgui.h"
#include <vector>
#include "UIShared.h"

template <typename T>
T GetOrDefault(const json& json, const json_pointer& path, T& defaultValue)
{
    if (!json.contains(path))
        return defaultValue;

    const auto& v = json.at(path);
    if (v.is_null())
        return defaultValue;

    return json.value<T>(path, defaultValue);
}

static void ShowToolTip(const char* tooltip, bool showQ = true)
{
    if (!tooltip || !tooltip[0])
        return;

    if (showQ)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "[?]");
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", tooltip);
}

static void ShowToolTip(const std::vector<std::string>& comments, bool showQ = true)
{
    std::string text;
    for (const std::string& s : comments)
    {
        text += s;
        text += "\n";
    }
    ShowToolTip(text.c_str(), showQ);
}

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

        bool treeNodeOpened = ImGui::TreeNodeEx(fieldDef.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

        ShowToolTip(fieldDef.doc_comment);

        if (!treeNodeOpened)
            return;
    }

    size_t deleteIndex = arrayItemCount;
    size_t moveUpIndex = arrayItemCount;
    size_t moveDownIndex = arrayItemCount;
    size_t moveTopIndex = arrayItemCount;
    size_t moveBottomIndex = arrayItemCount;
    size_t duplicateIndex = arrayItemCount;

    // Add the controls
    for (size_t arrayIndex = 0; arrayIndex < arrayItemCount; ++arrayIndex)
    {
        ImGui::PushID((int)arrayIndex);

        json_pointer jsonPathItem = jsonPath;
        std::string fieldName = fieldDef.name;

        if (type.isVector)
        {
            jsonPathItem /= arrayIndex;

            char buffer[256];
            sprintf_s(buffer, "%zu", arrayIndex);
            fieldName = buffer;
        }

        // If this is an enum
        if (fieldDef.value.type.enum_def)
        {
            if (fieldDef.value.type.enum_def->attributes.Lookup("bit_flags") == nullptr)
            {
                int64_t value = GetValueFromString<int64_t>(fieldDef.value.constant.c_str());
                value = GetOrDefault(jsonData.m_data, jsonPathItem, value);

                const auto& enumVals = fieldDef.value.type.enum_def->Vals();

                std::string selectedValue = "";
                if (value >= 0 && value < (int64_t)enumVals.size())
                    selectedValue = enumVals[value]->name;

                if (ImGui::BeginCombo(fieldName.c_str(), selectedValue.c_str()))
                {
                    for (const flatbuffers::EnumVal* enumItem : fieldDef.value.type.enum_def->Vals())
                    {
                        int64_t enumValue = enumItem->GetAsInt64();

                        const bool selected = (value == enumValue);

                        if (ImGui::Selectable(enumItem->name.c_str(), selected))
                        {
                            jsonData.m_data[jsonPathItem] = enumValue;
                            MarkDirty(editorData, jsonData);
                        }

                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }

                    ImGui::EndCombo();
                }
            }
            else
            {
                uint64_t value = GetValueFromString<uint64_t>(fieldDef.value.constant.c_str());
                value = GetOrDefault(jsonData.m_data, jsonPathItem, value);

                bool valueChanged = false;

                ImGui::TextUnformatted(fieldName.c_str());

                for (const flatbuffers::EnumVal* enumItem : fieldDef.value.type.enum_def->Vals())
                {
                    ImGui::PushID(enumItem);

                    uint64_t enumValue = enumItem->GetAsUInt64();

                    bool checked = ((value & enumValue) != 0);
                    if (ImGui::Checkbox(enumItem->name.c_str(), &checked))
                    {
                        if (checked)
                            value = value | enumValue;
                        else
                            value = value & (~enumValue);

                        valueChanged = true;
                    }

                    ImGui::PopID();
                }

                if (valueChanged)
                {
                    jsonData.m_data[jsonPathItem] = value;
                    MarkDirty(editorData, jsonData);
                }
            }
        }
        // else it is not an enum
        else
        {
            switch (type.category)
            {
                case TypeCategory::Bool:
                {
                    bool dflt = GetValueFromString<bool>(fieldDef.value.constant.c_str());
                    bool value = GetOrDefault(jsonData.m_data, jsonPathItem, dflt);
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
                        value = GetOrDefault(jsonData.m_data, jsonPathItem, value);
                        if (ImGui::InputScalar(fieldName.c_str(), ImGuiDataType_S64, &value, &step_one, &step_fast, "%zi"))
                        {
                            jsonData.m_data[jsonPathItem] = value;
                            MarkDirty(editorData, jsonData);
                        }
                    }
                    else
                    {
                        uint64_t value = GetValueFromString<uint64_t>(fieldDef.value.constant.c_str());
                        value = GetOrDefault(jsonData.m_data, jsonPathItem, value);
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
                        value = GetOrDefault(jsonData.m_data, jsonPathItem, value);
                        if (ImGui::InputDouble(fieldName.c_str(), &value))
                        {
                            jsonData.m_data[jsonPathItem] = value;
                            MarkDirty(editorData, jsonData);
                        }
                    }
                    else
                    {
                        float value = GetValueFromString<float>(fieldDef.value.constant.c_str());
                        value = GetOrDefault(jsonData.m_data, jsonPathItem, value);
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
                    std::string dflt = fieldDef.value.constant.c_str();
                    std::string value = GetOrDefault(jsonData.m_data, jsonPathItem, dflt);
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

        if (!type.isVector)
            ShowToolTip(fieldDef.doc_comment);

        if (type.isVector)
        {
            ImGui::SameLine();
            if (ImGui::SmallButton("X"))
                deleteIndex = arrayIndex;
            ShowToolTip("Delete", false);
            ImGui::SameLine();
            {
                ImGui_Enabled enabled(arrayIndex != 0);
                if (ImGui::SmallButton(ICON_FA_ANGLE_UP "##Up"))
                    moveUpIndex = arrayIndex;
                ShowToolTip("Up", false);
                ImGui::SameLine();
            }
            {
                ImGui_Enabled enabled(arrayIndex + 1 != arrayItemCount);
                if (ImGui::SmallButton(ICON_FA_ANGLE_DOWN "##Down"))
                    moveDownIndex = arrayIndex;
                ShowToolTip("Down", false);
                ImGui::SameLine();
            }
            {
                ImGui_Enabled enabled(arrayIndex != 0);
                if (ImGui::SmallButton(ICON_FA_ANGLES_UP "##Up to Top"))
                    moveTopIndex = arrayIndex;
                ShowToolTip("Up To Top", false);
                ImGui::SameLine();
            }
            {
                ImGui_Enabled enabled(arrayIndex + 1 != arrayItemCount);
                if (ImGui::SmallButton(ICON_FA_ANGLES_DOWN "##Down to Bottom"))
                    moveBottomIndex = arrayIndex;
                ShowToolTip("Down to Bottom", false);
            }

            {
                ImGui::SameLine();
                if (ImGui::SmallButton("Duplicate"))
                    duplicateIndex = arrayIndex;
                ShowToolTip("Duplicate", false);
            }
        }

        ImGui::PopID();
    }

    if (type.isVector)
    {
        if (ImGui::Button("Add Item"))
        {
            jsonData.m_data[jsonPath].push_back(nullptr);
        }

        ImGui::TreePop();
    }

    if (deleteIndex < arrayItemCount)
    {
        jsonData.m_data[jsonPath].erase(deleteIndex);
        MarkDirty(editorData, jsonData);
    }

    if (moveUpIndex < arrayItemCount)
    {
        std::swap(jsonData.m_data[jsonPath].at(moveUpIndex), jsonData.m_data[jsonPath].at(moveUpIndex - 1));
        MarkDirty(editorData, jsonData);
    }

    if (moveDownIndex < arrayItemCount)
    {
        std::swap(jsonData.m_data[jsonPath].at(moveDownIndex), jsonData.m_data[jsonPath].at(moveDownIndex + 1));
        MarkDirty(editorData, jsonData);
    }

    if (moveTopIndex < arrayItemCount)
    {
        std::swap(jsonData.m_data[jsonPath].at(moveTopIndex), jsonData.m_data[jsonPath].at(0));
        MarkDirty(editorData, jsonData);
    }

    if (moveBottomIndex < arrayItemCount)
    {
        std::swap(jsonData.m_data[jsonPath].at(moveBottomIndex), jsonData.m_data[jsonPath].at(arrayItemCount-1));
        MarkDirty(editorData, jsonData);
    }

    if (duplicateIndex < arrayItemCount)
    {
        jsonData.m_data[jsonPath].push_back(jsonData.m_data[jsonPath][duplicateIndex]);
        MarkDirty(editorData, jsonData);
    }

    /*
    TODO:
    * union needs to show the union thing itself too, not just the type selector
    * hitting +/- buttons on byte/ubyte and others make it go nuts. look into it
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
