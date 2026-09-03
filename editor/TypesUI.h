#pragma once

#include "../loader/JSONFwd.h"

namespace flatbuffers
{
    class Parser;
    struct StructDef;
};

class wxPanel;
class wxSizer;

void AddUIForType(const flatbuffers::Parser& parser, const flatbuffers::StructDef& structDef, wxPanel* panel, wxSizer* sizer, json& json, int indent = 0);
