#include "TypesUI.h"

#include <vector>
#include <string>

#include "flatbuffers/idl.h"

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

    if (type.base_type == flatbuffers::BaseType::BASE_TYPE_VECTOR || type.base_type == flatbuffers::BaseType::BASE_TYPE_VECTOR64)
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

static void AddUIForType(const flatbuffers::Parser& parser, const flatbuffers::FieldDef& fieldDef, wxPropertyGrid* grid, wxPGProperty* root, json& json)
{
    if (fieldDef.deprecated)
        return;

    Type ourType = FlatBufferTypeToOurType(fieldDef.value.type);

    if (ourType.category == TypeCategory::Unknown)
        return;

    // Add the control
    switch (ourType.category)
    {
        case TypeCategory::Bool:
        {
            grid->AppendIn(root, new wxBoolProperty(fieldDef.name.c_str(), wxPG_LABEL, false));
            break;
        }
        case TypeCategory::Int:
        {
            grid->AppendIn(root, new wxIntProperty(fieldDef.name.c_str(), wxPG_LABEL, 0));
            break;
        }
        case TypeCategory::Float:
        {
            grid->AppendIn(root, new wxFloatProperty(fieldDef.name.c_str(), wxPG_LABEL, 0.0));
            break;
        }
        case TypeCategory::String:
        {
            grid->AppendIn(root, new wxStringProperty(fieldDef.name.c_str(), wxPG_LABEL, ""));
            break;
        }
        case TypeCategory::Struct:
        {
            AddUIForType(parser, *ourType.details.Struct.structDef, fieldDef.name.c_str(), grid, root, json);
            break;
            break;
        }
    }



}

void AddUIForType(const flatbuffers::Parser& parser, const flatbuffers::StructDef& structDef, const char* structFieldName, wxPropertyGrid* grid, wxPGProperty* root, json& json)
{
    wxPGProperty* newRoot = root;
    if (structFieldName && structFieldName[0])
    {
        newRoot = new wxPropertyCategory(structFieldName);
        grid->AppendIn(root, newRoot);
    }

    // Add the fields
    for (const flatbuffers::FieldDef* fieldDef : structDef.fields.vec)
        AddUIForType(parser, *fieldDef, grid, newRoot, json);
}
