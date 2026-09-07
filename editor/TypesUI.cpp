#include "TypesUI.h"

#include <vector>
#include <string>

#include "flatbuffers/idl.h"
#include "../loader/JSON.h"

#pragma warning(push)
#pragma warning(disable: 4996)
#include <wx/wx.h>
#include <wx/propgrid/propgrid.h>
#include <wx/propgrid/advprops.h>
#include <wx/filedlg.h>
#include <wx/splitter.h>
#include <wx/sysopt.h>
#include <wx/stattext.h>
#pragma warning(pop)

template <typename T>
T GetValueFromString(const char* valueStr);

template <>
bool GetValueFromString<bool>(const char* valueStr)
{
    return (!stricmp(valueStr, "true") || !stricmp(valueStr, "1"));
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

void SetPropertyFromString(json& json, const json_pointer& path, const Type& type, const char* valueStr)
{
    if (type.category == TypeCategory::Bool)
    {
        bool value = (!stricmp(valueStr, "true") || !stricmp(valueStr, "1"));
        json[path] = value;
    }
    else if (type.category == TypeCategory::String)
    {
        json[path] = valueStr;
    }
    else
    {
        json[path] = json::parse(valueStr);
    }
}

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

static void AddUIForType(const flatbuffers::Parser& parser, const flatbuffers::FieldDef& fieldDef, wxPropertyGrid* grid, wxPGProperty* root, json& json, const json_pointer& jsonPath, PropertyMap& propertyMap)
{
    if (fieldDef.deprecated)
        return;

    Type type = FlatBufferTypeToOurType(fieldDef.value.type);

    if (type.category == TypeCategory::Unknown)
        return;

    wxPGProperty* newRoot = root;
    if (type.isVector)
    {
        char buffer[256];
        sprintf_s(buffer, "%s []", fieldDef.name.c_str());
        newRoot = new wxPropertyCategory(buffer);
        grid->AppendIn(root, newRoot);
    }

    // Add the control
    switch (type.category)
    {
        case TypeCategory::Bool:
        {
            bool value = json.value(jsonPath, GetValueFromString<bool>(fieldDef.value.constant.c_str()));
            wxPGProperty* newProperty = grid->AppendIn(newRoot, new wxBoolProperty(fieldDef.name.c_str(), wxPG_LABEL, value));
            propertyMap[newProperty] = PropertyInfo(jsonPath.to_string().c_str(), type);
            break;
        }
        case TypeCategory::Int:
        {
            wxPGProperty* newProperty = nullptr;
            if (type.details.Int.isSigned)
            {
                int64_t value = GetValueFromString<int64_t>(fieldDef.value.constant.c_str());
                value = json.value(jsonPath, value);
                newProperty = grid->AppendIn(newRoot, new wxIntProperty(fieldDef.name.c_str(), wxPG_LABEL, (long)value));
            }
            else
            {
                uint64_t value = GetValueFromString<uint64_t>(fieldDef.value.constant.c_str());
                value = json.value(jsonPath, value);
                newProperty = grid->AppendIn(newRoot, new wxUIntProperty(fieldDef.name.c_str(), wxPG_LABEL, (unsigned long)value));
            }

            propertyMap[newProperty] = PropertyInfo(jsonPath.to_string().c_str(), type);
            break;
        }
        case TypeCategory::Float:
        {
            double value = GetValueFromString<double>(fieldDef.value.constant.c_str());
            value = json.value(jsonPath, value);
            wxPGProperty* newProperty = grid->AppendIn(newRoot, new wxFloatProperty(fieldDef.name.c_str(), wxPG_LABEL, value));
            propertyMap[newProperty] = PropertyInfo(jsonPath.to_string().c_str(), type);
            break;
        }
        case TypeCategory::String:
        {
            std::string value = json.value(jsonPath, fieldDef.value.constant.c_str());
            wxPGProperty* newProperty = grid->AppendIn(newRoot, new wxStringProperty(fieldDef.name.c_str(), wxPG_LABEL, value.c_str()));
            propertyMap[newProperty] = PropertyInfo(jsonPath.to_string().c_str(), type);
            break;
        }
        case TypeCategory::Struct:
        {
            AddUIForType(parser, *type.details.Struct.structDef, fieldDef.name.c_str(), grid, newRoot, json, jsonPath, propertyMap);
            break;
            break;
        }
    }



}

void AddUIForType(const flatbuffers::Parser& parser, const flatbuffers::StructDef& structDef, const char* structFieldName, wxPropertyGrid* grid, wxPGProperty* root, json& json, const json_pointer& jsonPath, PropertyMap& propertyMap)
{
    wxPGProperty* newRoot = root;
    if (structFieldName && structFieldName[0])
    {
        newRoot = new wxPropertyCategory(structFieldName);
        grid->AppendIn(root, newRoot);
    }

    // Add the fields
    for (const flatbuffers::FieldDef* fieldDef : structDef.fields.vec)
    {
        json_pointer fieldPath = jsonPath;
        fieldPath /= fieldDef->name.c_str();
        AddUIForType(parser, *fieldDef, grid, newRoot, json, fieldPath, propertyMap);
    }
}
