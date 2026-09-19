
#include "Platform.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <string>

std::string WideToUtf8(const std::wstring& wide)
{
    if (wide.empty())
        return std::string();

    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
    if (sizeNeeded <= 0)
        return std::string();

    std::string result(sizeNeeded, 0);
    int written = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), result.data(), sizeNeeded, nullptr, nullptr);
    if (written <= 0)
        return std::string();

    return result;
}
