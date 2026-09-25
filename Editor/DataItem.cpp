#include "DataItem.h"

#include "../loader/JSON.h"
#include "Editor.h"
#include "imgui.h"
#include <vector>
#include "UIShared.h"

json MakeDefaultArrayItem(const DefParser& parser, const DefParser::StructField& field)
{
    switch (field.fieldType)
    {
        case DefParser::FieldType::_struct:
            return json::object();

        case DefParser::FieldType::_bool:
            return field.dflt == "1" || field.dflt == "true";

        case DefParser::FieldType::_float:
        case DefParser::FieldType::_double:
            return std::stof(field.dflt.c_str());

        case DefParser::FieldType::_enum:
        case DefParser::FieldType::_string:
            return field.dflt;

        default: // integral types
            return std::stoll(field.dflt.c_str());
    }
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

static void AddUIForType(EditorData& editorData, const DefParser& parser, const DefParser::Struct& structDef, const char* structFieldName, DBTable::JSONData& jsonData, const json_pointer& path, bool makeTreeNode);

static void AddUIForType(EditorData& editorData, const DefParser& parser, const DefParser::StructField& fieldDef, DBTable::JSONData& jsonData, const json_pointer& jsonPath)
{
    // Figure out how many items are in this array (1 item for non arrays)
    size_t arrayItemCount = 1;
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
            if (jsonData.m_data.contains(jsonPath))
                arrayItemCount = jsonData.m_data.value(jsonPath, json::array()).size();
            else
                arrayItemCount = 0;
        }

        bool treeNodeOpened = ImGui::TreeNodeEx(fieldDef.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

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

        if (fieldDef.isArray)
        {
            jsonPathItem /= arrayIndex;

            char buffer[256];
            sprintf_s(buffer, "%zu", arrayIndex);
            fieldName = buffer;
        }

        // If this is an enum
        if (fieldDef.fieldType == DefParser::FieldType::_enum)
        {
            std::string value = fieldDef.dflt;
            value = GetOrDefault(jsonData.m_data, jsonPathItem, value);

            const DefParser::Enum* e = parser.GetEnumByName(fieldDef.enumName.c_str());
            const auto& enumLabels = e->labels;

            if (ImGui::BeginCombo(fieldName.c_str(), value.c_str()))
            {
                for (const std::string& label : enumLabels)
                {
                    const bool selected = (value == label);

                    if (ImGui::Selectable(label.c_str(), selected))
                    {
                        jsonData.m_data[jsonPathItem] = label;
                        MarkDirty(editorData, jsonData);
                    }

                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }
        }
        // else it is not an enum
        else
        {
            switch (fieldDef.fieldType)
            {
                case DefParser::FieldType::_bool:
                {
                    bool dflt = GetValueFromString<bool>(fieldDef.dflt.c_str());
                    bool value = GetOrDefault(jsonData.m_data, jsonPathItem, dflt);
                    if (ImGui::Checkbox(fieldName.c_str(), &value))
                    {
                        jsonData.m_data[jsonPathItem] = value;
                        MarkDirty(editorData, jsonData);
                    }
                    break;
                }
                case DefParser::FieldType::_uint8:
                case DefParser::FieldType::_sint8:
                case DefParser::FieldType::_uint16:
                case DefParser::FieldType::_sint16:
                case DefParser::FieldType::_uint32:
                case DefParser::FieldType::_sint32:
                case DefParser::FieldType::_uint64:
                case DefParser::FieldType::_sint64:
                {
                    bool isSigned = IsSigned(fieldDef.fieldType);

                    if (isSigned)
                    {
                        int64_t step_one = 1;
                        int64_t step_fast = 10;

                        int64_t value = GetValueFromString<int64_t>(fieldDef.dflt.c_str());
                        value = GetOrDefault(jsonData.m_data, jsonPathItem, value);
                        if (ImGui::InputScalar(fieldName.c_str(), ImGuiDataType_S64, &value, &step_one, &step_fast, "%zi"))
                        {
                            jsonData.m_data[jsonPathItem] = value;
                            MarkDirty(editorData, jsonData);
                        }
                    }
                    else
                    {
                        uint64_t step_one = 1;
                        uint64_t step_fast = 10;

                        uint64_t value = GetValueFromString<uint64_t>(fieldDef.dflt.c_str());
                        value = GetOrDefault(jsonData.m_data, jsonPathItem, value);
                        if (ImGui::InputScalar(fieldName.c_str(), ImGuiDataType_U64, &value, &step_one, &step_fast, "%zu"))
                        {
                            jsonData.m_data[jsonPathItem] = value;
                            MarkDirty(editorData, jsonData);
                        }
                    }

                    break;
                }
                case DefParser::FieldType::_double:
                {
                    double value = GetValueFromString<double>(fieldDef.dflt.c_str());
                    value = GetOrDefault(jsonData.m_data, jsonPathItem, value);
                    if (ImGui::InputDouble(fieldName.c_str(), &value))
                    {
                        jsonData.m_data[jsonPathItem] = value;
                        MarkDirty(editorData, jsonData);
                    }
                    break;
                }
                case DefParser::FieldType::_float:
                {
                    float value = GetValueFromString<float>(fieldDef.dflt.c_str());
                    value = GetOrDefault(jsonData.m_data, jsonPathItem, value);
                    if (ImGui::InputFloat(fieldName.c_str(), &value))
                    {
                        jsonData.m_data[jsonPathItem] = value;
                        MarkDirty(editorData, jsonData);
                    }
                    break;
                }
                case DefParser::FieldType::_link:
                {
                    std::string dflt = fieldDef.dflt.c_str();
                    std::string value = GetOrDefault(jsonData.m_data, jsonPathItem, dflt);

                    // links have a drop down menu
                    const DBTable& table = *editorData.m_dbroot.m_tables[fieldDef.linkName];

                    if (ImGui::BeginCombo(fieldDef.name.c_str(), value.c_str()))
                    {
                        for (auto& pair : table.m_data)
                        {
                            const bool selected = (value == pair.first);

                            if (ImGui::Selectable(pair.first.c_str(), selected))
                            {
                                jsonData.m_data[jsonPathItem] = pair.first;
                                MarkDirty(editorData, jsonData);
                            }

                            if (selected)
                                ImGui::SetItemDefaultFocus();
                        }

                        ImGui::EndCombo();
                    }

                    ImGui::SameLine();
                    if (ImGui::SmallButton(ICON_FA_CIRCLE_ARROW_RIGHT "##GoToLink"))
                    {
                        editorData.m_selectedTableName = fieldDef.linkName;
                        editorData.m_selectedDataItemName = value;
                    }
                    ShowToolTip("Go To Link", false);
                    break;
                }
                case DefParser::FieldType::_string:
                {
                    std::string dflt = fieldDef.dflt.c_str();
                    std::string value = GetOrDefault(jsonData.m_data, jsonPathItem, dflt);

                    // otherwise enter the string
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
                case DefParser::FieldType::_struct:
                {
                    const DefParser::Struct* structDef = parser.GetStructByName(fieldDef.structName.c_str());
                    AddUIForType(editorData, parser, *structDef, fieldName.c_str(), jsonData, jsonPathItem, true);
                    break;
                }
            }
        }

        if (fieldDef.isArray)
        {
            if (IsScalar(fieldDef.fieldType))
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

            if (!fixedSizedArray)
            {
                ImGui::SameLine();
                if (ImGui::SmallButton("X"))
                    deleteIndex = arrayIndex;
                ShowToolTip("Delete", false);

                ImGui::SameLine();
                if (ImGui::SmallButton("Duplicate"))
                    duplicateIndex = arrayIndex;
                ShowToolTip("Duplicate", false);
            }
        }

        ImGui::PopID();
    }

    if (fieldDef.isArray)
    {
        if (!fixedSizedArray && ImGui::Button("Add Item"))
        {
            jsonData.m_data[jsonPath].push_back(MakeDefaultArrayItem(parser, fieldDef));
            MarkDirty(editorData, jsonData);
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
}

static void AddUIForType(EditorData& editorData, const DefParser& parser, const DefParser::Struct& structDef, const char* structFieldName, DBTable::JSONData& jsonData, const json_pointer& path, bool makeTreeNode)
{
    if (makeTreeNode)
    {
        if (!ImGui::TreeNodeEx(structFieldName, ImGuiTreeNodeFlags_DefaultOpen))
            return;
    }

    // Add the fields
    for (const DefParser::StructField& fieldDef : structDef.fields)
    {
        json_pointer fieldPath = path;
        fieldPath /= fieldDef.name.c_str();

        AddUIForType(editorData, parser, fieldDef, jsonData, fieldPath);
    }

    if (makeTreeNode)
        ImGui::TreePop();
}

void ShowDataEditor(EditorData& editorData)
{
    if (editorData.m_dbroot.m_tables.count(editorData.m_selectedTableName) == 0)
        return;
    DBTable& table = *editorData.m_dbroot.m_tables[editorData.m_selectedTableName].get();

    if (table.m_data.count(editorData.m_selectedDataItemName) == 0)
        return;
    DBTable::JSONData& data = *table.m_data[editorData.m_selectedDataItemName].get();

    const DefParser& parser = table.GetParser();

    AddUIForType(editorData, parser, *parser.GetRootStruct(), parser.GetRootStructName().c_str(), data, json_pointer(""), true);
}
