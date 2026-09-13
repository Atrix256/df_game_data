
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

std::string GetProcessTempDirectory()
{
    wchar_t tempPathBuf[MAX_PATH];
    DWORD len = GetTempPathW(MAX_PATH, tempPathBuf);
    if (len == 0 || len > MAX_PATH)
        return std::string();

    std::wstring path(tempPathBuf, len);
    if (!path.empty() && path.back() != L'\\')
        path += L'\\';
    path += L"df_game_data_Editor_" + std::to_wstring(GetCurrentProcessId());
    path += L'\\';

    if (!CreateDirectoryW(path.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        return std::string();

    return WideToUtf8(path);
}

bool RunFlatc(const char* args, bool waitForExit)
{
    std::string cmdLine = "\"..\\vcpkg_installed\\x64-windows\\x64-windows\\tools\\flatbuffers\\flatc.exe\" " + std::string(args);

    STARTUPINFOA si{ sizeof(si) };
    PROCESS_INFORMATION pi{};

    if (!CreateProcessA(nullptr, (char*)cmdLine.c_str(), nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        return false;
    }

    if (!waitForExit)
    {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return true;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return exitCode == 0;
}
