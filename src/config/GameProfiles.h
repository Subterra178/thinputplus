#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cwchar>

enum class AddressMode {
    Absolute,
    Rva
};

enum class KeyId : std::uint8_t {
    Z,
    X,
    Shift,
    Up,
    Down,
    Left,
    Right,
    Ctrl,
    Count
};

struct GameProfile {
    const wchar_t *exeName;
    std::uintptr_t inputAddress;
    AddressMode addressMode;
    std::array<std::uint8_t, static_cast<std::size_t>(KeyId::Count)> keyBits;
};

inline constexpr GameProfile GAME_PROFILES[] = {
    {L"東方紅魔郷.exe", 0x0069D904, AddressMode::Absolute, {0, 1, 2, 4, 5, 6, 7, 8}},
    {L"th06e.exe", 0x0069D904, AddressMode::Absolute, {0, 1, 2, 4, 5, 6, 7, 8}},
    {L"th07.exe", 0x004B9E50, AddressMode::Absolute, {0, 1, 2, 4, 5, 6, 7, 8}},
    {L"th07e.exe", 0x004B9E50, AddressMode::Absolute, {0, 1, 2, 4, 5, 6, 7, 8}},
    {L"th08.exe", 0x0164D52C, AddressMode::Absolute, {0, 1, 2, 4, 5, 6, 7, 8}},
    {L"th08e.exe", 0x0164D52C, AddressMode::Absolute, {0, 1, 2, 4, 5, 6, 7, 8}},
    {L"th09.exe", 0x004ACE44, AddressMode::Absolute, {0, 1, 2, 4, 5, 6, 7, 8}},
    {L"th09e.exe", 0x004ACE44, AddressMode::Absolute, {0, 1, 2, 4, 5, 6, 7, 8}},
    {L"th095.exe", 0x004BE244, AddressMode::Absolute, {1, 0, 2, 4, 5, 6, 7, 8}},
    {L"th095e.exe", 0x004BE244, AddressMode::Absolute, {1, 0, 2, 4, 5, 6, 7, 8}},
    {L"th10.exe", 0x00474E5C, AddressMode::Absolute, {0, 1, 2, 4, 5, 6, 7, 8}},
    {L"th10e.exe", 0x00474E5C, AddressMode::Absolute, {0, 1, 2, 4, 5, 6, 7, 8}},
    {L"th11.exe", 0x004C93C0, AddressMode::Absolute, {0, 1, 3, 4, 5, 6, 7, 9}},
    {L"th11e.exe", 0x004C93C0, AddressMode::Absolute, {0, 1, 3, 4, 5, 6, 7, 9}},
    {L"th12.exe", 0x004D49D0, AddressMode::Absolute, {0, 1, 3, 4, 5, 6, 7, 9}},
    {L"th12e.exe", 0x004D49D0, AddressMode::Absolute, {0, 1, 3, 4, 5, 6, 7, 9}},
    {L"th125.exe", 0x004D8FB8, AddressMode::Absolute, {0, 1, 2, 3, 4, 5, 6, 7}},
    {L"th125e.exe", 0x004D8FB8, AddressMode::Absolute, {0, 1, 2, 3, 4, 5, 6, 7}},
    {L"th128.exe", 0x004DB210, AddressMode::Absolute, {0, 1, 3, 4, 5, 6, 7, 9}},
    {L"th128e.exe", 0x004DB210, AddressMode::Absolute, {0, 1, 3, 4, 5, 6, 7, 9}},
    {L"th13.exe", 0x004E4C08, AddressMode::Absolute, {0, 1, 3, 4, 5, 6, 7, 9}},
    {L"th13e.exe", 0x004E4C08, AddressMode::Absolute, {0, 1, 3, 4, 5, 6, 7, 9}},
    {L"th14.exe", 0x004D6A90, AddressMode::Absolute, {0, 1, 3, 4, 5, 6, 7, 9}},
    {L"th143.exe", 0x004E1BE8, AddressMode::Absolute, {0, 1, 3, 4, 5, 6, 7, 9}},
    {L"th15.exe", 0x004E6F28, AddressMode::Absolute, {0, 1, 3, 4, 5, 6, 7, 9}},
    {L"th16.exe", 0x004A52C8, AddressMode::Absolute, {0, 1, 3, 4, 5, 6, 7, 9}},
    {L"th165.exe", 0x004B1308, AddressMode::Absolute, {0, 1, 3, 4, 5, 6, 7, 9}},
    {L"th17.exe", 0x004B3448, AddressMode::Absolute, {0, 1, 3, 4, 5, 6, 7, 9}},
    {L"th18.exe", 0x004CA428, AddressMode::Absolute, {0, 1, 3, 4, 5, 6, 7, 9}},
    {L"th20.exe", 0x001B8B4C, AddressMode::Rva, {0, 2, 3, 4, 5, 6, 7, 9}}
};

inline const GameProfile *FindGameProfile(const wchar_t *exeName) {
    for (const auto &profile: GAME_PROFILES) {
        if (_wcsicmp(profile.exeName, exeName) == 0)
            return &profile;
    }
    return nullptr;
}
