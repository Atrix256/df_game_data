#pragma once

#include "../loader/JSONFwd.h"
#include <unordered_map>
#include <string>

namespace flatbuffers
{
    class Parser;
    struct StructDef;
};
class wxPropertyGrid;
class wxPGProperty;

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

struct PropertyInfo
{
    PropertyInfo()
    {
    }

    PropertyInfo(const char* jsonPath, const Type& type)
        : m_jsonPath(jsonPath)
        , m_type(type)
    {
    }

    std::string m_jsonPath;
    Type m_type;
};

// maps a property path to a json path
using PropertyMap = std::unordered_map<wxPGProperty*, PropertyInfo>;

void AddUIForType(const flatbuffers::Parser& parser, const flatbuffers::StructDef& structDef, const char* structFieldName, wxPropertyGrid* grid, wxPGProperty* root, json& json, const json_pointer& path, PropertyMap& propertyMap);
