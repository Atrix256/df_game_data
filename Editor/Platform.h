#pragma once

#include <string>

void SetWindowTitle(const char* text);
extern bool RunFlatc(const char* args, bool waitForExit);
extern std::string GetProcessTempDirectory();
