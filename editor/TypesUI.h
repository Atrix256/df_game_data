#pragma once

namespace flatbuffers
{
    class Parser;
    struct StructDef;
};

class wxPanel;
class wxSizer;

void AddUIForType(const flatbuffers::Parser& parser, const flatbuffers::StructDef& structDef, wxPanel* panel, wxSizer* sizer, int indent = 0);
