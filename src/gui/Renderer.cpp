#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstddef>
#include <cstdio>

#include "app/AppState.h"
#include "config/Constants.h"
#include "input/KeyMap.h"
#include "gui/Renderer.h"

namespace {
    void DrawKey(HDC hdc, const KeyInfo &key, bool pressed) {
        RECT rect{
            key.x,
            key.y,
            key.x + key.width,
            key.y + key.height
        };

        const COLORREF background = pressed
                                        ? RGB(255, 255, 255)
                                        : RGB(65, 67, 69);

        const COLORREF foreground = pressed
                                        ? RGB(28, 29, 31)
                                        : RGB(235, 235, 235);

        HBRUSH brush = CreateSolidBrush(background);
        HPEN pen = CreatePen(PS_SOLID, 1, background);

        HGDIOBJ oldBrush = SelectObject(hdc, brush);
        HGDIOBJ oldPen = SelectObject(hdc, pen);

        RoundRect(
            hdc,
            rect.left,
            rect.top,
            rect.right,
            rect.bottom,
            10,
            10
        );

        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);

        DeleteObject(brush);
        DeleteObject(pen);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, foreground);

        // DrawTextW(
        // hdc,
        // key.name,
        // -1,
        // &rect,
        // DT_CENTER | DT_VCENTER | DT_SINGLELINE
        // );
    }
}

void DrawDisplay(HDC hdc, const RECT &client) {
    HBRUSH background = CreateSolidBrush(PANEL_BACKGROUND);
    FillRect(hdc, &client, background);
    DeleteObject(background);

    SetBkMode(hdc, TRANSPARENT);

    if (g_debugVisible) {
        SetTextColor(hdc, RGB(235, 235, 235));

        RECT statusRect{
            20, 15,
            WINDOW_WIDTH - 20, 45
        };

        const wchar_t *statusText = g_connected
                                        ? L"INPUT DISPLAY - CONNECTED"
                                        : L"INPUT DISPLAY - WAITING";

        DrawTextW(
            hdc,
            statusText,
            -1,
            &statusRect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE
        );

        wchar_t info[128];

        const uintptr_t inputAddress = g_gameProfile
                                           ? (g_gameProfile->addressMode == AddressMode::Rva
                                                  ? g_baseAddress + g_gameProfile->inputAddress
                                                  : g_gameProfile->inputAddress)
                                           : 0;

        swprintf_s(
            info,
            L"PID: %lu   INPUT: %p",
            g_pid,
            reinterpret_cast<void *>(inputAddress)
        );

        RECT infoRect{
            20, 48,
            WINDOW_WIDTH - 20, 70
        };

        DrawTextW(
            hdc,
            info,
            -1,
            &infoRect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE
        );
    }

    for (const auto &key: KEYS) {
        const auto bit = g_gameProfile
                             ? g_gameProfile->keyBits[static_cast<std::size_t>(key.id)]
                             : 0;
        const bool pressed = (g_inputState & (1u << bit)) != 0;
        DrawKey(hdc, key, pressed);
    }

    if (g_debugVisible) {
        wchar_t raw[64];

        swprintf_s(
            raw,
            L"INPUT: 0x%04X (%u)",
            g_inputState,
            g_inputState
        );

        RECT rawRect{
            20, 260,
            WINDOW_WIDTH - 20, 290
        };

        DrawTextW(
            hdc,
            raw,
            -1,
            &rawRect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE
        );
    }
}
