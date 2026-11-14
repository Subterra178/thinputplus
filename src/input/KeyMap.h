#pragma once
#include <array>
#include <cstdint>
#include "config/GameProfiles.h"

struct KeyInfo {
    const wchar_t *name;
    KeyId id;
    int x;
    int y;
    int width;
    int height;
};

// 表示するキーの座標情報でございます
inline constexpr std::array<KeyInfo, 8> KEYS = {
    {
        {L"SHIFT", KeyId::Shift, 30, 30, 50, 50},
        {L"Z", KeyId::Z, 85, 30, 50, 50},
        {L"X", KeyId::X, 140, 30, 50, 50},
        {L"CTRL", KeyId::Ctrl, 30, 85, 50, 50},
        {L"UP", KeyId::Up, 305, 30, 50, 50},
        {L"DOWN", KeyId::Down, 305, 85, 50, 50},
        {L"LEFT", KeyId::Left, 250, 85, 50, 50},
        {L"RIGHT", KeyId::Right, 360, 85, 50, 50}
    }
};
