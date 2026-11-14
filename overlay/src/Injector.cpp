#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <cwchar>
#include <cstdio>
#include <string>
#include <vector>
#include "config/GameProfiles.h"

namespace {
    DWORD FindTarget(const GameProfile **foundProfile) {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
            return 0;
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        DWORD pid = 0;
        if (Process32FirstW(snapshot, &entry)) {
            do {
                if (const auto *profile = FindGameProfile(entry.szExeFile)) {
                    pid = entry.th32ProcessID;
                    *foundProfile = profile;
                    break;
                }
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
        return pid;
    }

    std::uintptr_t RemoteModuleBase(DWORD pid, const wchar_t *moduleName) {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snapshot == INVALID_HANDLE_VALUE)
            return 0;
        MODULEENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        std::uintptr_t base = 0;
        if (Module32FirstW(snapshot, &entry)) {
            do {
                if (_wcsicmp(entry.szModule, moduleName) == 0) {
                    base = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
                    break;
                }
            } while (Module32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
        return base;
    }
}

int wmain(int argc, wchar_t **argv) {
    const GameProfile *profile = nullptr;
    const DWORD pid = FindTarget(&profile);
    if (!pid || !profile) {
        fwprintf(stderr, L"Supported Touhou process was not found.\n");
        return 1;
    }

    wchar_t dllPath[MAX_PATH]{};
    if (argc >= 2) {
        if (!GetFullPathNameW(argv[1], MAX_PATH, dllPath, nullptr)) {
            fwprintf(stderr, L"Invalid DLL path.\n");
            return 2;
        }
    } else {
        GetModuleFileNameW(nullptr, dllPath, MAX_PATH);
        wchar_t *slash = wcsrchr(dllPath, L'\\');
        if (!slash) return 2;
        *(slash + 1) = L'\0';
        wcscat_s(dllPath, L"thinput_overlay.dll");
    }

    HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                                 PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (!process) {
        fwprintf(stderr, L"OpenProcess failed: %lu\n", GetLastError());
        return 3;
    }

    const SIZE_T bytes = (wcslen(dllPath) + 1) * sizeof(wchar_t);
    void *remoteString = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteString || !WriteProcessMemory(process, remoteString, dllPath, bytes, nullptr)) {
        fwprintf(stderr, L"Could not write DLL path into target process.\n");
        if (remoteString) VirtualFreeEx(process, remoteString, 0, MEM_RELEASE);
        CloseHandle(process);
        return 4;
    }

    HMODULE localKernel = GetModuleHandleW(L"kernel32.dll");
    const auto localLoadLibrary = reinterpret_cast<std::uintptr_t>(GetProcAddress(localKernel, "LoadLibraryW"));
    const auto remoteKernel = RemoteModuleBase(pid, L"kernel32.dll");
    if (!localLoadLibrary || !remoteKernel) {
        fwprintf(stderr, L"Could not locate LoadLibraryW in target process.\n");
        VirtualFreeEx(process, remoteString, 0, MEM_RELEASE);
        CloseHandle(process);
        return 5;
    }

    const auto loadLibraryOffset = localLoadLibrary - reinterpret_cast<std::uintptr_t>(localKernel);
    auto remoteLoadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(remoteKernel + loadLibraryOffset);
    HANDLE thread = CreateRemoteThread(process, nullptr, 0, remoteLoadLibrary, remoteString, 0, nullptr);
    if (!thread) {
        fwprintf(stderr, L"CreateRemoteThread failed: %lu\n", GetLastError());
        VirtualFreeEx(process, remoteString, 0, MEM_RELEASE);
        CloseHandle(process);
        return 6;
    }

    WaitForSingleObject(thread, 10000);
    DWORD exitCode = 0;
    GetExitCodeThread(thread, &exitCode);
    CloseHandle(thread);
    VirtualFreeEx(process, remoteString, 0, MEM_RELEASE);
    CloseHandle(process);

    if (!exitCode) {
        fwprintf(stderr, L"LoadLibraryW did not report success. Check DLL bitness and path.\n");
        return 7;
    }
    wprintf(L"Injected thinput_overlay.dll into %ls (PID %lu).\n", profile->exeName, pid);
    return 0;
}
