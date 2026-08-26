#include "LaunchProcess.h"

#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

LaunchProcessResult LaunchProcess(const char* commandLine)
{
    LaunchProcessResult result = { -1, "" };

    // Set up security attributes to allow handle inheritance
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    // Create the anonymous pipe
    HANDLE hReadPipe = nullptr;
    HANDLE hWritePipe = nullptr;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
        return result;

    // Ensure the read handle of the pipe is NOT inherited by the child
    if (!SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0))
    {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return result;
    }

    // Prepare startup configuration
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;         // Hide GUI windows
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    ZeroMemory(&pi, sizeof(pi));

    // Start hidden process
    BOOL success = CreateProcessA(
        nullptr, (char*)commandLine, nullptr, nullptr,
        TRUE, // Must pass TRUE here to inherit handles (hWritePipe)
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi
    );

    if (!success) {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return result;
    }

    // CRITICAL: Close our write end of the pipe so ReadFile knows when stream ends
    CloseHandle(hWritePipe);

    // Read the output loop
    constexpr DWORD BUFFER_SIZE = 4096;
    std::vector<char> buffer(BUFFER_SIZE);
    DWORD bytes_read = 0;

    while (ReadFile(hReadPipe, buffer.data(), BUFFER_SIZE - 1, &bytes_read, nullptr) && bytes_read > 0)
        result.output.append(buffer.data(), bytes_read);

    // Wait for process to clean up and grab exit code
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 0;
    if (GetExitCodeProcess(pi.hProcess, &exit_code))
        result.exitCode = static_cast<int>(exit_code);

    // Clean up handles
    CloseHandle(hReadPipe);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return result;
}
