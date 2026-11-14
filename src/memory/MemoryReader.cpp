#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include "app/AppState.h"
#include "memory/MemoryReader.h"

bool ReadInputState() {
    if (!g_process || !g_gameProfile)
        return false;

    if (WaitForSingleObject(g_process, 0) != WAIT_TIMEOUT)
        return false;

    const uintptr_t address =
            (g_gameProfile->addressMode == AddressMode::Rva)
                ? g_baseAddress + g_gameProfile->inputAddress
                : g_gameProfile->inputAddress;

    uint16_t value = 0;
    SIZE_T bytesRead = 0;
    const BOOL success = ReadProcessMemory(
        g_process,
        reinterpret_cast<LPCVOID>(address),
        &value,
        sizeof(value),
        &bytesRead);

    if (!success || bytesRead != sizeof(value))
        return false;

    g_inputState = value;
    return true;
}
