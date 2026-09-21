#pragma once

#include <string>

struct EditorData;

bool Compile(const EditorData& editorData, std::string& error);
