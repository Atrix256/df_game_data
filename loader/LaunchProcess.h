#pragma once

#include <string>

struct LaunchProcessResult
{
    int exitCode;
    std::string output;
};

LaunchProcessResult LaunchProcess(const char* commandLine);
