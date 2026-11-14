#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include "config/GameProfiles.h"

extern HWND g_hwnd;

extern HANDLE g_process;
extern DWORD g_pid;
extern const GameProfile* g_gameProfile;

extern uintptr_t g_baseAddress;
extern uint16_t g_inputState;

extern bool g_connected;
extern unsigned int g_retryCounter;

extern bool g_debugVisible;
