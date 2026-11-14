#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "src/app/AppState.h"
#include "src/config/Constants.h"
#include "src/gui/Window.h"

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    int showCommand
) {
    const wchar_t CLASS_NAME[] = L"TH20InputDisplayWindow";

    WNDCLASSW wc{};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.hbrBackground = nullptr;

    if (!RegisterClassW(&wc))
        return 1;

    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST,
        CLASS_NAME,
        L"Touhou Input Display",
        WS_POPUP,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        nullptr,
        nullptr,
        instance,
        nullptr
    );

    if (!hwnd)
        return 1;

    g_hwnd = hwnd;

    SetWindowPos(
        hwnd, HWND_TOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE
    );

    ShowWindow(hwnd, showCommand);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}
