#pragma once

#include <windows.h>
#include <cstdint>

void CloseTargetProcess();
DWORD FindTargetProcess();
uintptr_t GetModuleBaseAddress(DWORD pid);
bool ConnectTargetProcess();
