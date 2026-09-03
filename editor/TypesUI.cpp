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

static void AddUIForType(const flatbuffers::Parser& parser, const flatbuffers::FieldDef& fieldDef, wxPanel* panel, wxSizer* sizer, int indent)
{
    if (fieldDef.deprecated)
        return;

    const flatbuffers::Type& type = fieldDef.value.type;

    enum class TypeCategory
    {
        Unknown,
        Bool,
        Int,
        Float,
        String
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
    };

    TypeCategory category = TypeCategory::Unknown;
    TypeDetails details;

    switch (type.base_type)
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
    }

    if (category == TypeCategory::Unknown)
        return;


    // Add the label for the field
    wxBoxSizer* labelSizer = new wxBoxSizer(wxHORIZONTAL);

    labelSizer->Add(20 * indent, 0);
    wxStaticText* label = new wxStaticText(panel, wxID_ANY, fieldDef.name.c_str(), wxDefaultPosition, wxDefaultSize, 0);
    label->Wrap(-1);
    labelSizer->Add(label, 0, wxEXPAND | wxALL, 0);

    sizer->Add(labelSizer, 0, wxEXPAND | wxALL, 0);

    // add the control

    wxTextCtrl* textCtrl = new wxTextCtrl(panel, wxID_ANY, "");
    sizer->Add(textCtrl, 1, wxEXPAND | wxALL, 0);
}

void AddUIForType(const flatbuffers::Parser& parser, const flatbuffers::StructDef& structDef, wxPanel* panel, wxSizer* sizer, int indent)
{
    // Add the label for the struct
    wxBoxSizer* labelSizer = new wxBoxSizer(wxHORIZONTAL);

    labelSizer->Add(20 * indent, 0);
    wxStaticText* label = new wxStaticText(panel, wxID_ANY, structDef.name.c_str(), wxDefaultPosition, wxDefaultSize, 0);
    label->Wrap(-1);
    labelSizer->Add(label, 0, wxEXPAND | wxALL, 0);

    sizer->Add(labelSizer, 0, wxEXPAND | wxALL, 0);

    sizer->AddSpacer(0);

    // Add the fields, indented
    for (const flatbuffers::FieldDef* fieldDef : structDef.fields.vec)
        AddUIForType(parser, *fieldDef, panel, sizer, indent+1);
}
