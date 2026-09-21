#pragma once
#include <windows.h>
#include <cstdint>
#include <unordered_map>
#include <shared_mutex>
#include <chrono>
#include <string>
#include <vector>

#include "../math/vector2.hpp"
#include "../math/vector3.hpp"
#include "../math/matrix4x4.hpp"
#include "player.h"

class EspGlobals {
public:
    struct EspConfig_t {
        mutable std::shared_mutex EntitiesMutex;
        std::unordered_map<uint64_t, Player> Entities;

        Matrix4x4 ViewMatrix{};
        Vector3 MainCamera{};
        Player Local{};
        uint64_t LocalPlayer = 0;

        bool Matrix = false;
        int Width = 0;
        int Height = 0;

        bool InMatch = false;
        uint32_t CurrentMatch = 0;
        uint64_t LocalUID = 0;

        bool AutoRefresh = false;
        bool Connected = false;
    } EspConfig;

    struct Visuals_t {
        bool Enable = false;

        // Box
        bool Box = true;
        int players_box = 1; // 0 = Normal 2D Box, 1 = Corner Box
        float BoxColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        bool FilledBox = false;
        float Filledboxcolor[4] = { 0.0f, 0.0f, 0.0f, 0.35f };

        // Skeleton
        bool Skeleton = true;
        float SkeletonColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        float SkeletonThickness = 1.2f;

        // Snaplines
        bool Lines = true;
        int EspLines = 1; // 0 = Top, 1 = Bottom, 2 = Left, 3 = Right, 4 = Crosshair
        float LinesColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        bool RainbowLines = false;

        // Health Bar
        bool HealthBar = true;
        int players_healthbar = 2; // 0 = Top, 1 = Left, 2 = Below Box
        float texthColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

        // Weapon
        bool ESPWeapon = true;
        float ESPWeaponColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

        // Name & Distance
        bool Name = true;
        float NameColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

        bool Distance = true;
        float DistColor[4] = { 0.8f, 0.8f, 0.8f, 1.0f };
        int DistanceEsp = 250;

        // Head Circle / Dot
        bool HeadDot = true;
        float HeadDotSize = 2.5f;
        float HeadDotColor[4] = { 1.0f, 0.2f, 0.2f, 1.0f };

        // In-game radar
        bool Radar = true;
        float inagame_color[4] = { 0.87f, 0.46f, 0.46f, 1.0f };
        float RadarSize = 120.0f;
        float RadarRange = 150.0f;
        float RadarPosX = 120.0f;
        float RadarPosY = 120.0f;

        // Offsets & Tuning
        float HipWidthScale = 0.19f;
        float HipWidthOffset = 0.33f;
        float LeftHipHeightOffset = 0.82f;
        float RightHipHeightOffset = 0.98f;
        bool Wukong = false;
    } Visuals;
};

inline EspGlobals g_Globals;
