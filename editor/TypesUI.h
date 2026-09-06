#pragma once

#include "../loader/JSONFwd.h"

namespace flatbuffers
{
    class Parser;
    struct StructDef;
};

class wxPropertyGrid;
class wxPGProperty;

void AddUIForType(const flatbuffers::Parser& parser, const flatbuffers::StructDef& structDef, const char* structFieldName, wxPropertyGrid* grid, wxPGProperty* root, json& json);
