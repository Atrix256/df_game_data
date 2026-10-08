#pragma once

#include <stdint.h>
#include <string>

struct DBCompileSettings;
class DBRoot;

bool MakeBin(const DBCompileSettings& compilerSettings, const DBRoot& dbRoot, const char* fileName, uint64_t hash, std::string& error);
