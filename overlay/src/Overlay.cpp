#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <tlhelp32.h>
#include <cstdint>
#include <cstddef>
#include <cstdarg>
#include <cwchar>
#include <cmath>
#include <cstdio>
#include <string>

#include "config/GameProfiles.h"
#include "input/KeyMap.h"
#include "config/Constants.h"

namespace {
    using EndSceneFn = HRESULT (WINAPI*)(IDirect3DDevice9 *);
    using ResetFn = HRESULT (WINAPI*)(IDirect3DDevice9 *, D3DPRESENT_PARAMETERS *);
    using CreateDeviceFn = HRESULT (WINAPI*)(IDirect3D9 *, UINT, D3DDEVTYPE, HWND, DWORD,
                                             D3DPRESENT_PARAMETERS *, IDirect3DDevice9 **);

    EndSceneFn g_originalEndScene = nullptr;
    ResetFn g_originalReset = nullptr;
    CreateDeviceFn g_originalCreateDevice = nullptr;
    volatile LONG g_installed = 0;
    volatile LONG g_debugVisible = 0;
    bool g_f12WasDown = false;
    const GameProfile *g_profile = nullptr;
    volatile LONG g_lastInput = 0;
    HINSTANCE g_dllInstance = nullptr;

    FILE *g_log = nullptr;

    void Log(const wchar_t *fmt, ...) {
        if (!g_log)
            return;
        wchar_t message[512]{};
        va_list args;
        va_start(args, fmt);
        vswprintf(message, 512, fmt, args);
        va_end(args);
        SYSTEMTIME st{};
        GetLocalTime(&st);
        fwprintf(g_log, L"[%02u:%02u:%02u.%03u] %ls\n",
                 st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, message);
        fflush(g_log);
    }

    struct OverlayConfig {
        float x = -1.0f;
        float y = -1.0f;
        float width = 440.0f;
        float height = 165.0f;
    };

    struct OverlayLayout {
        float left = 0.0f;
        float top = 0.0f;
        float width = 440.0f;
        float height = 165.0f;
    };

    OverlayConfig g_config;
    OverlayLayout g_layout;
    bool g_configLoaded = false;
    bool g_layoutValid = false;
    float g_layoutViewportWidth = 0.0f;
    float g_layoutViewportHeight = 0.0f;
    bool g_editMode = false;
    bool g_iWasDown = false;
    bool g_mouseWasDown = false;
    bool g_dragging = false;
    int g_resizeCorner = -1;
    float g_dragStartX = 0.0f;
    float g_dragStartY = 0.0f;
    float g_startX = 0.0f;
    float g_startY = 0.0f;
    float g_startWidth = 0.0f;
    float g_startHeight = 0.0f;

    bool g_clipSaved = false;
    RECT g_savedClipRect{};
    float g_cursorX = 0.0f;
    float g_cursorY = 0.0f;
    bool g_cursorValid = false;
    bool g_osCursorVisible = false;

    struct Vertex {
        float x, y, z, rhw;
        D3DCOLOR color;
    };

    constexpr DWORD FVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;
    constexpr float MIN_OVERLAY_WIDTH = 160.0f;
    constexpr float MIN_OVERLAY_HEIGHT = 80.0f;
    constexpr float RESIZE_HANDLE_SIZE = 14.0f;

    bool GetDllDirectory(wchar_t *path, DWORD capacity) {
        if (!g_dllInstance || !GetModuleFileNameW(g_dllInstance, path, capacity))
            return false;
        wchar_t *slash = wcsrchr(path, L'\\');
        if (!slash)
            return false;
        *(slash + 1) = L'\0';
        return true;
    }

    std::wstring GameConfigName(const wchar_t *exeName) {
        std::wstring name(exeName ? exeName : L"");
        const std::wstring::size_type dot = name.find_last_of(L'.');
        if (dot != std::wstring::npos)
            name.erase(dot);

        if (name.size() > 2 && (name[0] == L't' || name[0] == L'T') &&
            (name[1] == L'h' || name[1] == L'H')) {
            std::wstring id = L"th";
            std::wstring::size_type i = 2;
            while (i < name.size() && name[i] >= L'0' && name[i] <= L'9') {
                id.push_back(name[i]);
                ++i;
            }
            if (id.size() > 2)
                return id;
        }

        if (name == L"東方紅魔郷")
            return L"th06";

        std::wstring fallback;
        for (const wchar_t c: name) {
            if ((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'z') ||
                (c >= L'A' && c <= L'Z'))
                fallback.push_back(c);
            else
                fallback.push_back(L'_');
        }
        return fallback.empty() ? std::wstring(L"unknown") : fallback;
    }

    bool GetConfigPath(wchar_t *path, DWORD capacity) {
        wchar_t directory[MAX_PATH]{};
        if (!GetDllDirectory(directory, MAX_PATH))
            return false;

        wchar_t configDirectory[MAX_PATH]{};
        if (swprintf(configDirectory, MAX_PATH, L"%lsconfig", directory) < 0)
            return false;
        CreateDirectoryW(configDirectory, nullptr);

        const std::wstring gameName = GameConfigName(g_profile ? g_profile->exeName : nullptr);
        return swprintf(path, capacity, L"%ls\\%ls.ini",
                        configDirectory, gameName.c_str()) >= 0;
    }

    bool GetLegacyConfigPath(wchar_t *path, DWORD capacity) {
        wchar_t directory[MAX_PATH]{};
        if (!GetDllDirectory(directory, MAX_PATH))
            return false;
        return swprintf(path, capacity, L"%lsthinput.ini", directory) >= 0;
    }

    void LoadConfig() {
        if (g_configLoaded)
            return;
        g_configLoaded = true;

        wchar_t path[MAX_PATH]{};
        if (!GetConfigPath(path, MAX_PATH))
            return;

        const wchar_t *source = path;
        wchar_t legacyPath[MAX_PATH]{};
        if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES &&
            GetLegacyConfigPath(legacyPath, MAX_PATH) &&
            GetFileAttributesW(legacyPath) != INVALID_FILE_ATTRIBUTES) {
            source = legacyPath;
        }

        wchar_t value[64]{};
        if (GetPrivateProfileStringW(L"Overlay", L"x", L"", value, 64, source) > 0) {
            wchar_t *end = nullptr;
            const float x = wcstof(value, &end);
            if (end != value && x >= 0.0f && x <= 1.0f)
                g_config.x = x;
        }
        if (GetPrivateProfileStringW(L"Overlay", L"y", L"", value, 64, source) > 0) {
            wchar_t *end = nullptr;
            const float y = wcstof(value, &end);
            if (end != value && y >= 0.0f && y <= 1.0f)
                g_config.y = y;
        }
        g_config.width = static_cast<float>(GetPrivateProfileIntW(L"Overlay", L"width", 440, source));
        g_config.height = static_cast<float>(GetPrivateProfileIntW(L"Overlay", L"height", 165, source));
        if (g_config.width < MIN_OVERLAY_WIDTH) g_config.width = MIN_OVERLAY_WIDTH;
        if (g_config.height < MIN_OVERLAY_HEIGHT) g_config.height = MIN_OVERLAY_HEIGHT;
    }

    void UpdateConfigFromLayout(float viewportWidth, float viewportHeight) {
        const float maxX = viewportWidth > g_layout.width ? viewportWidth - g_layout.width : 1.0f;
        const float maxY = viewportHeight > g_layout.height ? viewportHeight - g_layout.height : 1.0f;
        g_config.x = g_layout.left / maxX;
        g_config.y = g_layout.top / maxY;
        if (g_config.x < 0.0f) g_config.x = 0.0f;
        if (g_config.x > 1.0f) g_config.x = 1.0f;
        if (g_config.y < 0.0f) g_config.y = 0.0f;
        if (g_config.y > 1.0f) g_config.y = 1.0f;
        g_config.width = g_layout.width;
        g_config.height = g_layout.height;
    }

    float MaxLayoutLeft(float viewportWidth) {
        return viewportWidth > g_layout.width ? viewportWidth - g_layout.width : 0.0f;
    }

    float MaxLayoutTop(float viewportHeight) {
        return viewportHeight > g_layout.height ? viewportHeight - g_layout.height : 0.0f;
    }

    void ClampLayout(float viewportWidth, float viewportHeight) {
        const float maxLeft = MaxLayoutLeft(viewportWidth);
        const float maxTop = MaxLayoutTop(viewportHeight);
        if (g_layout.left < 0.0f) g_layout.left = 0.0f;
        if (g_layout.top < 0.0f) g_layout.top = 0.0f;
        if (g_layout.left > maxLeft) g_layout.left = maxLeft;
        if (g_layout.top > maxTop) g_layout.top = maxTop;
    }

    void UpdateLayoutForViewport(float viewportWidth, float viewportHeight) {
        if (!g_layoutValid) {
            g_layout.width = g_config.width;
            g_layout.height = g_config.height;
            g_layout.left = g_config.x >= 0.0f
                                ? g_config.x * MaxLayoutLeft(viewportWidth)
                                : (viewportWidth - g_layout.width) * 0.5f;
            g_layout.top = g_config.y >= 0.0f
                               ? g_config.y * MaxLayoutTop(viewportHeight)
                               : viewportHeight - g_layout.height - 24.0f;
            g_layoutValid = true;
        } else if (viewportWidth != g_layoutViewportWidth ||
                   viewportHeight != g_layoutViewportHeight) {
            // The viewport resolution changed: re-normalize the current pixel
            // rectangle against the old viewport, then project it onto the new one
            // so the panel keeps its relative position.
            UpdateConfigFromLayout(g_layoutViewportWidth, g_layoutViewportHeight);
            g_layout.width = g_config.width;
            g_layout.height = g_config.height;
            g_layout.left = g_config.x * MaxLayoutLeft(viewportWidth);
            g_layout.top = g_config.y * MaxLayoutTop(viewportHeight);
        }
        g_layoutViewportWidth = viewportWidth;
        g_layoutViewportHeight = viewportHeight;
        ClampLayout(viewportWidth, viewportHeight);
    }

    void SaveConfig(float viewportWidth, float viewportHeight) {
        UpdateConfigFromLayout(viewportWidth, viewportHeight);

        wchar_t path[MAX_PATH]{};
        if (!GetConfigPath(path, MAX_PATH))
            return;

        wchar_t value[64]{};
        swprintf(value, 64, L"%.6f", g_config.x);
        WritePrivateProfileStringW(L"Overlay", L"x", value, path);
        swprintf(value, 64, L"%.6f", g_config.y);
        WritePrivateProfileStringW(L"Overlay", L"y", value, path);
        swprintf(value, 64, L"%.0f", g_config.width);
        WritePrivateProfileStringW(L"Overlay", L"width", value, path);
        swprintf(value, 64, L"%.0f", g_config.height);
        WritePrivateProfileStringW(L"Overlay", L"height", value, path);
    }

    void DrawRoundedRect(IDirect3DDevice9 *device, float x, float y,
                         float width, float height, float radius, D3DCOLOR color) {
        constexpr int segmentsPerCorner = 5;
        constexpr float pi = 3.14159265358979323846f;
        Vertex vertices[1 + 4 * (segmentsPerCorner + 1) + 1]{};
        int count = 0;
        vertices[count++] = {x + width * 0.5f, y + height * 0.5f, 0.0f, 1.0f, color};

        const float centersX[4] = {
            x + width - radius, x + width - radius,
            x + radius, x + radius
        };
        const float centersY[4] = {
            y + radius, y + height - radius,
            y + height - radius, y + radius
        };
        const float starts[4] = {-pi * 0.5f, 0.0f, pi * 0.5f, pi};

        for (int corner = 0; corner < 4; ++corner) {
            for (int i = 0; i <= segmentsPerCorner; ++i) {
                const float angle = starts[corner] + (pi * 0.5f) *
                                    static_cast<float>(i) / segmentsPerCorner;
                vertices[count++] = {
                    centersX[corner] + std::cos(angle) * radius,
                    centersY[corner] + std::sin(angle) * radius,
                    0.0f, 1.0f, color
                };
            }
        }
        vertices[count] = vertices[1];
        ++count;

        device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, count - 2, vertices, sizeof(Vertex));
    }

    void DrawEditFrame(IDirect3DDevice9 *device, float left, float top, float width, float height) {
        const D3DCOLOR frame = D3DCOLOR_ARGB(235, 70, 190, 255);
        const float t = 2.0f;
        DrawRoundedRect(device, left, top, width, t, 1.0f, frame);
        DrawRoundedRect(device, left, top + height - t, width, t, 1.0f, frame);
        DrawRoundedRect(device, left, top, t, height, 1.0f, frame);
        DrawRoundedRect(device, left + width - t, top, t, height, 1.0f, frame);
        const float h = RESIZE_HANDLE_SIZE;
        const float points[4][2] = {
            {left, top}, {left + width - h, top},
            {left + width - h, top + height - h}, {left, top + height - h}
        };
        for (const auto &point: points)
            DrawRoundedRect(device, point[0], point[1], h, h, 3.0f,
                            D3DCOLOR_ARGB(255, 70, 190, 255));
    }

    void DrawTriangle(IDirect3DDevice9 *device, float x0, float y0, float x1, float y1,
                      float x2, float y2, D3DCOLOR color) {
        Vertex vertices[3] = {
            {x0, y0, 0.0f, 1.0f, color},
            {x1, y1, 0.0f, 1.0f, color},
            {x2, y2, 0.0f, 1.0f, color}
        };
        device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, vertices, sizeof(Vertex));
    }

    void DrawCursorShape(IDirect3DDevice9 *device, float x, float y, float scale,
                         D3DCOLOR color) {
        const float p[7][2] = {
            {0.0f, 0.0f}, {0.0f, 16.0f}, {4.0f, 12.0f}, {6.5f, 18.0f},
            {9.0f, 16.8f}, {6.3f, 11.0f}, {11.5f, 11.0f}
        };
        auto px = [&](int index) { return x + p[index][0] * scale; };
        auto py = [&](int index) { return y + p[index][1] * scale; };

        DrawTriangle(device, px(0), py(0), px(1), py(1), px(2), py(2), color);
        DrawTriangle(device, px(0), py(0), px(2), py(2), px(5), py(5), color);
        DrawTriangle(device, px(2), py(2), px(3), py(3), px(4), py(4), color);
        DrawTriangle(device, px(2), py(2), px(4), py(4), px(5), py(5), color);
        DrawTriangle(device, px(0), py(0), px(5), py(5), px(6), py(6), color);
    }

    void DrawCursor(IDirect3DDevice9 *device, float x, float y) {
        DrawCursorShape(device, x - 1.5f, y - 1.5f, 1.25f, D3DCOLOR_ARGB(255, 0, 0, 0));
        DrawCursorShape(device, x, y, 1.0f, D3DCOLOR_ARGB(255, 255, 255, 255));
    }

    void UpdateEditorInput(HWND hwnd, float viewportWidth, float viewportHeight) {
        const bool iDown = (GetAsyncKeyState('I') & 0x8000) != 0;
        if (iDown && !g_iWasDown) {
            if (g_editMode) {
                SaveConfig(viewportWidth, viewportHeight);
                g_editMode = false;
                if (g_clipSaved) {
                    ClipCursor(&g_savedClipRect);
                    g_clipSaved = false;
                }
            } else {
                g_editMode = true;
                if (GetClipCursor(&g_savedClipRect))
                    g_clipSaved = true;
            }
            g_dragging = false;
            g_resizeCorner = -1;
            g_mouseWasDown = false;
            g_cursorValid = false;
        }
        g_iWasDown = iDown;
        if (!g_editMode || !hwnd)
            return;

        ClipCursor(nullptr);

        POINT cursor{};
        if (!GetCursorPos(&cursor) || !ScreenToClient(hwnd, &cursor))
            return;

        RECT client{};
        if (!GetClientRect(hwnd, &client))
            return;
        const float clientWidth = static_cast<float>(client.right - client.left);
        const float clientHeight = static_cast<float>(client.bottom - client.top);
        if (clientWidth <= 0.0f || clientHeight <= 0.0f)
            return;

        const float mx = static_cast<float>(cursor.x) * viewportWidth / clientWidth;
        const float my = static_cast<float>(cursor.y) * viewportHeight / clientHeight;
        g_cursorX = mx;
        g_cursorY = my;
        g_cursorValid = mx >= 0.0f && mx <= viewportWidth &&
                        my >= 0.0f && my <= viewportHeight;

        CURSORINFO cursorInfo{};
        cursorInfo.cbSize = sizeof(CURSORINFO);
        g_osCursorVisible = GetCursorInfo(&cursorInfo) != 0 &&
                            (cursorInfo.flags & CURSOR_SHOWING) != 0 &&
                            cursorInfo.hCursor != nullptr;

        const bool mouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        const float w = g_layout.width, h = g_layout.height, handle = RESIZE_HANDLE_SIZE;

        if (mouseDown && !g_mouseWasDown) {
            g_resizeCorner = -1;
            const float corners[4][2] = {
                {g_layout.left, g_layout.top},
                {g_layout.left + w - handle, g_layout.top},
                {g_layout.left + w - handle, g_layout.top + h - handle},
                {g_layout.left, g_layout.top + h - handle}
            };
            for (int i = 0; i < 4; ++i) {
                if (mx >= corners[i][0] - 3 && mx <= corners[i][0] + handle + 3 &&
                    my >= corners[i][1] - 3 && my <= corners[i][1] + handle + 3) {
                    g_resizeCorner = i;
                    break;
                }
            }
            if (g_resizeCorner >= 0 ||
                (mx >= g_layout.left && mx <= g_layout.left + w &&
                 my >= g_layout.top && my <= g_layout.top + h)) {
                g_dragging = true;
                g_dragStartX = mx;
                g_dragStartY = my;
                g_startX = g_layout.left;
                g_startY = g_layout.top;
                g_startWidth = w;
                g_startHeight = h;
            }
        }

        if (mouseDown && g_dragging) {
            const float dx = mx - g_dragStartX, dy = my - g_dragStartY;
            if (g_resizeCorner < 0) {
                g_layout.left = g_startX + dx;
                g_layout.top = g_startY + dy;
            } else {
                float right = g_startX + g_startWidth, bottom = g_startY + g_startHeight;
                float newLeft = g_startX, newTop = g_startY;
                if (g_resizeCorner == 0 || g_resizeCorner == 3) newLeft += dx;
                if (g_resizeCorner == 0 || g_resizeCorner == 1) newTop += dy;
                if (g_resizeCorner == 1 || g_resizeCorner == 2) right += dx;
                if (g_resizeCorner == 2 || g_resizeCorner == 3) bottom += dy;
                if (right - newLeft >= MIN_OVERLAY_WIDTH) {
                    g_layout.left = newLeft;
                    g_layout.width = right - newLeft;
                }
                if (bottom - newTop >= MIN_OVERLAY_HEIGHT) {
                    g_layout.top = newTop;
                    g_layout.height = bottom - newTop;
                }
            }
            ClampLayout(viewportWidth, viewportHeight);
        }
        if (!mouseDown) {
            if (g_dragging)
                SaveConfig(viewportWidth, viewportHeight);
            g_dragging = false;
        }
        g_mouseWasDown = mouseDown;
    }

    void RenderOverlay(IDirect3DDevice9 *device) {
        if (!g_profile)
            return;

        const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        const std::uintptr_t address = g_profile->addressMode == AddressMode::Rva
                                           ? base + g_profile->inputAddress
                                           : g_profile->inputAddress;

        std::uint16_t inputValue = 0;
        SIZE_T bytesRead = 0;
        static bool loggedRender = false;
        if (ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(address),
                              &inputValue, sizeof(inputValue), &bytesRead) &&
            bytesRead == sizeof(inputValue)) {
            InterlockedExchange(&g_lastInput, inputValue);
        } else {
            InterlockedExchange(&g_lastInput, 0);
        }
        if (!loggedRender) {
            loggedRender = true;
            Log(L"RenderOverlay first call: address=0x%zx base=0x%zx read=%ls",
                address, base,
                (bytesRead == sizeof(inputValue)) ? L"ok" : L"failed");
        }

        const std::uint16_t state = static_cast<std::uint16_t>(InterlockedCompareExchange(&g_lastInput, 0, 0));
        D3DVIEWPORT9 viewport{};
        if (FAILED(device->GetViewport(&viewport)))
            return;

        IDirect3DStateBlock9 *stateBlock = nullptr;
        if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &stateBlock)) || !stateBlock)
            return;
        stateBlock->Capture();

        device->SetVertexShader(nullptr);
        device->SetPixelShader(nullptr);
        device->SetFVF(FVF);
        device->SetTexture(0, nullptr);
        device->SetRenderState(D3DRS_LIGHTING, FALSE);
        device->SetRenderState(D3DRS_ZENABLE, FALSE);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

        LoadConfig();
        const float viewportWidth = static_cast<float>(viewport.Width);
        const float viewportHeight = static_cast<float>(viewport.Height);
        UpdateLayoutForViewport(viewportWidth, viewportHeight);

        D3DDEVICE_CREATION_PARAMETERS creation{};
        HWND gameWindow = nullptr;
        if (SUCCEEDED(device->GetCreationParameters(&creation)))
            gameWindow = creation.hFocusWindow;
        if (!gameWindow) {
            IDirect3DSwapChain9 *swapChain = nullptr;
            if (SUCCEEDED(device->GetSwapChain(0, &swapChain)) && swapChain) {
                D3DPRESENT_PARAMETERS present{};
                if (SUCCEEDED(swapChain->GetPresentParameters(&present)))
                    gameWindow = present.hDeviceWindow;
                swapChain->Release();
            }
        }
        UpdateEditorInput(gameWindow, viewportWidth, viewportHeight);

        DrawRoundedRect(device, g_layout.left, g_layout.top, g_layout.width, g_layout.height, 9.0f,
                        D3DCOLOR_ARGB(PANEL_BACKGROUND_ALPHA, 32, 33, 36));

        for (const auto &key: KEYS) {
            const auto bit = g_profile->keyBits[static_cast<std::size_t>(key.id)];
            const bool pressed = (state & (1u << bit)) != 0;
            const D3DCOLOR color = pressed
                                       ? D3DCOLOR_ARGB(255, 255, 255, 255)
                                       : D3DCOLOR_ARGB(255, 65, 67, 69);
            const float scaleX = g_layout.width / 440.0f;
            const float scaleY = g_layout.height / 165.0f;
            DrawRoundedRect(device, g_layout.left + key.x * scaleX, g_layout.top + key.y * scaleY,
                            static_cast<float>(key.width) * scaleX,
                            static_cast<float>(key.height) * scaleY,
                            8.0f * (scaleX < scaleY ? scaleX : scaleY), color);
        }

        if (g_editMode) {
            DrawEditFrame(device, g_layout.left, g_layout.top, g_layout.width, g_layout.height);
            if (g_cursorValid && !g_osCursorVisible)
                DrawCursor(device, g_cursorX, g_cursorY);
        }

        const bool f12Down = (GetAsyncKeyState(VK_F12) & 0x8000) != 0;
        if (f12Down && !g_f12WasDown)
            InterlockedXor(&g_debugVisible, 1);
        g_f12WasDown = f12Down;

        stateBlock->Apply();
        stateBlock->Release();
    }

    HRESULT WINAPI HookEndScene(IDirect3DDevice9 *device) {
        static bool logged = false;
        if (!logged) {
            logged = true;
            Log(L"EndScene hooked, device=%p", reinterpret_cast<void *>(device));
        }
        RenderOverlay(device);
        return g_originalEndScene ? g_originalEndScene(device) : D3D_OK;
    }

    HRESULT WINAPI HookReset(IDirect3DDevice9 *device, D3DPRESENT_PARAMETERS *params) {
        return g_originalReset ? g_originalReset(device, params) : D3DERR_INVALIDCALL;
    }

    bool PatchVtable(void **table, std::size_t index, void *replacement, void **original);

    HRESULT WINAPI HookCreateDevice(IDirect3D9 *api, UINT adapter, D3DDEVTYPE type,
                                    HWND focusWindow, DWORD behaviorFlags,
                                    D3DPRESENT_PARAMETERS *params, IDirect3DDevice9 **device) {
        CreateDeviceFn original = g_originalCreateDevice;
        if (!original)
            return D3DERR_INVALIDCALL;
        const HRESULT hr = original(api, adapter, type, focusWindow, behaviorFlags, params, device);
        Log(L"CreateDevice called: adapter=%u type=%d flags=%lx hwnd=%p -> hr=0x%lx device=%p",
            adapter, static_cast<int>(type), behaviorFlags,
            reinterpret_cast<void *>(focusWindow), static_cast<unsigned long>(hr),
            (device && SUCCEEDED(hr)) ? reinterpret_cast<void *>(*device) : nullptr);
        if (SUCCEEDED(hr) && device && *device) {
            void **deviceTable = *reinterpret_cast<void ***>(*device);
            PatchVtable(deviceTable, 16, reinterpret_cast<void *>(HookReset),
                        reinterpret_cast<void **>(&g_originalReset));
            PatchVtable(deviceTable, 42, reinterpret_cast<void *>(HookEndScene),
                        reinterpret_cast<void **>(&g_originalEndScene));

            void **apiTable = *reinterpret_cast<void ***>(api);
            PatchVtable(apiTable, 15, reinterpret_cast<void *>(original), nullptr);
            g_originalCreateDevice = nullptr;
            InterlockedExchange(&g_installed, 1);
            Log(L"Device vtable hooked (EndScene/Reset)");
        }
        return hr;
    }

    bool ArmCreateDeviceHook() {
        HMODULE d3d9 = LoadLibraryW(L"d3d9.dll");
        if (!d3d9) {
            Log(L"ArmCreateDeviceHook: LoadLibraryW(d3d9.dll) failed: %lu", GetLastError());
            return false;
        }
        wchar_t d3d9Path[MAX_PATH]{};
        GetModuleFileNameW(d3d9, d3d9Path, MAX_PATH);
        using Direct3DCreate9Fn = IDirect3D9* (WINAPI*)(UINT);
        auto create9 = reinterpret_cast<Direct3DCreate9Fn>(GetProcAddress(d3d9, "Direct3DCreate9"));
        if (!create9) {
            Log(L"ArmCreateDeviceHook: Direct3DCreate9 not found in %ls", d3d9Path);
            return false;
        }
        IDirect3D9 *api = create9(D3D_SDK_VERSION);
        if (!api) {
            Log(L"ArmCreateDeviceHook: Direct3DCreate9 returned null (%ls)", d3d9Path);
            return false;
        }
        void **table = *reinterpret_cast<void ***>(api);
        const bool ok = PatchVtable(table, 15, reinterpret_cast<void *>(HookCreateDevice),
                                    reinterpret_cast<void **>(&g_originalCreateDevice));
        Log(L"ArmCreateDeviceHook: d3d9=%ls api=%p armed=%d", d3d9Path,
            reinterpret_cast<void *>(api), ok ? 1 : 0);
        api->Release();
        return ok;
    }

    bool PatchVtable(void **table, std::size_t index, void *replacement, void **original) {
        if (table[index] == replacement)
            return true;
        DWORD oldProtect = 0;
        if (!VirtualProtect(&table[index], sizeof(void *), PAGE_EXECUTE_READWRITE, &oldProtect))
            return false;
        if (original)
            *original = table[index];
        table[index] = replacement;
        DWORD ignored = 0;
        VirtualProtect(&table[index], sizeof(void *), oldProtect, &ignored);
        FlushInstructionCache(GetCurrentProcess(), &table[index], sizeof(void *));
        return true;
    }

    LRESULT CALLBACK DummyWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    bool InstallHooks() {
        if (InterlockedCompareExchange(&g_installed, 0, 0))
            return true;
        HMODULE d3d9 = LoadLibraryW(L"d3d9.dll");
        if (!d3d9)
            return false;
        using Direct3DCreate9Fn = IDirect3D9* (WINAPI*)(UINT);
        auto create9 = reinterpret_cast<Direct3DCreate9Fn>(GetProcAddress(d3d9, "Direct3DCreate9"));
        if (!create9)
            return false;

        WNDCLASSW wc{};
        wc.lpfnWndProc = DummyWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"thinput_overlay_dummy";
        RegisterClassW(&wc);
        HWND window = CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW,
                                    0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);
        if (!window)
            return false;

        IDirect3D9 *api = create9(D3D_SDK_VERSION);
        if (!api)
            return false;
        D3DPRESENT_PARAMETERS pp{};
        pp.Windowed = TRUE;
        pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        pp.hDeviceWindow = window;
        pp.BackBufferFormat = D3DFMT_UNKNOWN;
        IDirect3DDevice9 *device = nullptr;
        HRESULT hr = api->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
                                       D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device);
        Log(L"InstallHooks: HAL CreateDevice hr=0x%lx", static_cast<unsigned long>(hr));
        if (FAILED(hr)) {
            hr = api->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, window,
                                   D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device);
            Log(L"InstallHooks: REF CreateDevice hr=0x%lx", static_cast<unsigned long>(hr));
        }
        if (FAILED(hr) || !device)
            return false;
        if (InterlockedCompareExchange(&g_installed, 0, 0)) {
            device->Release();
            api->Release();
            DestroyWindow(window);
            return true;
        }

        void **table = *reinterpret_cast<void ***>(device);
        const bool resetOk = PatchVtable(table, 16, reinterpret_cast<void *>(HookReset),
                                         reinterpret_cast<void **>(&g_originalReset));
        const bool endSceneOk = PatchVtable(table, 42, reinterpret_cast<void *>(HookEndScene),
                                            reinterpret_cast<void **>(&g_originalEndScene));
        device->Release();
        api->Release();
        DestroyWindow(window);
        return resetOk && endSceneOk;
    }

    DWORD WINAPI InitThread(void *) {
        wchar_t exePath[MAX_PATH]{};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        const wchar_t *exeName = wcsrchr(exePath, L'\\');
        exeName = exeName ? exeName + 1 : exePath;
        g_profile = FindGameProfile(exeName);
        if (!g_profile) {
            Log(L"InitThread: no profile for %ls", exeName);
            return 0;
        }

        wchar_t logPath[MAX_PATH]{};
        if (GetDllDirectory(logPath, MAX_PATH))
            wcscat_s(logPath, L"thinput_overlay.log");
        g_log = _wfopen(logPath, L"w");
        Log(L"--- attach: exe=%ls profile=%ls", exePath, g_profile->exeName);

        const bool armed = ArmCreateDeviceHook();
        for (int attempt = 0; attempt < 100; ++attempt) {
            if (InterlockedCompareExchange(&g_installed, 0, 0))
                return 0;
            if (!armed || attempt >= 20) {
                if (InstallHooks()) {
                    InterlockedExchange(&g_installed, 1);
                    return 0;
                }
            }
            Sleep(250);
        }
        return 0;
    }
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_dllInstance = instance;
        DisableThreadLibraryCalls(instance);
        HANDLE thread = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
        if (thread)
            CloseHandle(thread);
    }
    return TRUE;
}
