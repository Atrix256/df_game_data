#pragma once

inline void AddUIForType(const flatbuffers::Parser& parser, const flatbuffers::StructDef& struct_def, wxPanel* panel, wxSizer* sizer)
{
	wxStaticText* label = new wxStaticText(panel, wxID_ANY, struct_def.name.c_str(), wxDefaultPosition, wxDefaultSize, 0);
	label->Wrap(-1);
	sizer->Add(label, 0, wxALL, 5);
}
