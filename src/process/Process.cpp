#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <cwchar>
#include "app/AppState.h"
#include "config/GameProfiles.h"
#include "process/Process.h"

namespace {
const wchar_t* g_detectedExeName = nullptr;
}

void CloseTargetProcess()
{
    if (g_process) {
        CloseHandle(g_process);
        g_process = nullptr;
    }

    g_pid = 0;
    g_baseAddress = 0;
    g_inputState = 0;
    g_connected = false;
    g_gameProfile = nullptr;
    g_detectedExeName = nullptr;
}

DWORD FindTargetProcess()
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    DWORD result = 0;

    if (Process32FirstW(snapshot, &entry)) {
        do {
            const GameProfile* profile = FindGameProfile(entry.szExeFile);
            if (profile) {
                result = entry.th32ProcessID;
                g_gameProfile = profile;
                g_detectedExeName = profile->exeName;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return result;
}

uintptr_t GetModuleBaseAddress(DWORD pid)
{
    if (!g_detectedExeName)
        return 0;

    HANDLE snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;

    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    uintptr_t result = 0;

    if (Module32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szModule, g_detectedExeName) == 0) {
                result = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
                break;
            }
        } while (Module32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return result;
}

bool ConnectTargetProcess()
{
    CloseTargetProcess();

    const DWORD pid = FindTargetProcess();
    if (pid == 0 || !g_gameProfile) {
        g_gameProfile = nullptr;
        return false;
    }

    HANDLE process = OpenProcess(
        PROCESS_VM_READ | PROCESS_QUERY_INFORMATION | SYNCHRONIZE,
        FALSE, pid);
    if (!process) {
        g_gameProfile = nullptr;
        g_detectedExeName = nullptr;
        return false;
    }

    uintptr_t base = 0;
    if (g_gameProfile->addressMode == AddressMode::Rva) {
        base = GetModuleBaseAddress(pid);
        if (base == 0) {
            CloseHandle(process);
            g_gameProfile = nullptr;
            g_detectedExeName = nullptr;
            return false;
        }
    }

    g_process = process;
    g_pid = pid;
    g_baseAddress = base;
    g_connected = true;
    return true;
}
