#include "app/AppState.h"

HWND g_hwnd = nullptr;

HANDLE g_process = nullptr;
DWORD g_pid = 0;
const GameProfile *g_gameProfile = nullptr;

uintptr_t g_baseAddress = 0;
uint16_t g_inputState = 0;

bool g_connected = false;

unsigned int g_retryCounter = 0;

bool g_debugVisible = false;
