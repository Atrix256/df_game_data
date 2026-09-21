#pragma once

#include <string>

class DBRoot;
struct DBCompileSettings;

bool Compile(const DBRoot& dbRoot, const DBCompileSettings& compilerSettings, std::string& error);
