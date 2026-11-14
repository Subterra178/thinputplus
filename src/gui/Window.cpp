#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstddef>
#include "app/AppState.h"
#include "config/Constants.h"
#include "process/Process.h"
#include "memory/MemoryReader.h"
#include "gui/Renderer.h"
#include "gui/Window.h"

namespace {
    void PaintWindow(HWND hwnd) {
        PAINTSTRUCT ps{};
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);

        RECT windowRect{};
        GetWindowRect(hwnd, &windowRect);

        const int width = windowRect.right - windowRect.left;
        const int height = windowRect.bottom - windowRect.top;
        if (width <= 0 || height <= 0)
            return;

        HDC screenDC = GetDC(nullptr);
        HDC memoryDC = CreateCompatibleDC(screenDC);

        BITMAPINFO bitmapInfo{};
        bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmapInfo.bmiHeader.biWidth = width;
        bitmapInfo.bmiHeader.biHeight = -height;
        bitmapInfo.bmiHeader.biPlanes = 1;
        bitmapInfo.bmiHeader.biBitCount = 32;
        bitmapInfo.bmiHeader.biCompression = BI_RGB;

        void *pixels = nullptr;
        HBITMAP bitmap = CreateDIBSection(
            screenDC,
            &bitmapInfo,
            DIB_RGB_COLORS,
            &pixels,
            nullptr,
            0
        );

        if (!bitmap || !pixels) {
            if (bitmap)
                DeleteObject(bitmap);
            DeleteDC(memoryDC);
            ReleaseDC(nullptr, screenDC);
            return;
        }

        HGDIOBJ oldBitmap = SelectObject(memoryDC, bitmap);

        RECT clientRect{0, 0, width, height};
        DrawDisplay(memoryDC, clientRect);

        auto *bgra = static_cast<std::uint8_t *>(pixels);
        const std::uint8_t backgroundB = GetBValue(PANEL_BACKGROUND);
        const std::uint8_t backgroundG = GetGValue(PANEL_BACKGROUND);
        const std::uint8_t backgroundR = GetRValue(PANEL_BACKGROUND);

        const std::size_t pixelCount = static_cast<std::size_t>(width) * height;
        for (std::size_t i = 0; i < pixelCount; ++i) {
            const std::size_t offset = i * 4;
            const bool isPanelBackground =
                    bgra[offset + 0] == backgroundB &&
                    bgra[offset + 1] == backgroundG &&
                    bgra[offset + 2] == backgroundR;

            if (isPanelBackground) {
                bgra[offset + 0] = static_cast<std::uint8_t>(
                    bgra[offset + 0] * PANEL_BACKGROUND_ALPHA / 255
                );
                bgra[offset + 1] = static_cast<std::uint8_t>(
                    bgra[offset + 1] * PANEL_BACKGROUND_ALPHA / 255
                );
                bgra[offset + 2] = static_cast<std::uint8_t>(
                    bgra[offset + 2] * PANEL_BACKGROUND_ALPHA / 255
                );
                bgra[offset + 3] = PANEL_BACKGROUND_ALPHA;
            } else {
                bgra[offset + 3] = 255;
            }
        }

        POINT destination{windowRect.left, windowRect.top};
        SIZE size{width, height};
        POINT source{0, 0};
        BLENDFUNCTION blend{};
        blend.BlendOp = AC_SRC_OVER;
        blend.SourceConstantAlpha = 255;
        blend.AlphaFormat = AC_SRC_ALPHA;

        UpdateLayeredWindow(
            hwnd,
            screenDC,
            &destination,
            &size,
            memoryDC,
            &source,
            0,
            &blend,
            ULW_ALPHA
        );

        SelectObject(memoryDC, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(memoryDC);
        ReleaseDC(nullptr, screenDC);
    }
}

LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
) {
    switch (message) {
        case WM_KEYDOWN:
            if (wParam == VK_F12 && (lParam & (1L << 30)) == 0) {
                g_debugVisible = !g_debugVisible;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            break;

        case WM_NCHITTEST:
            return HTCAPTION;

        case WM_CREATE:
            SetTimer(hwnd, TIMER_ID, TIMER_INTERVAL, nullptr);
            return 0;

        case WM_TIMER:
            if (!g_connected) {
                ++g_retryCounter;

                if (g_retryCounter >= 31) {
                    g_retryCounter = 0;
                    ConnectTargetProcess();
                }
            } else if (!ReadInputState()) {
                CloseTargetProcess();
            }

            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
            PaintWindow(hwnd);
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd, TIMER_ID);
            CloseTargetProcess();
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}
