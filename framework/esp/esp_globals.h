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

    struct General_t {
        bool Capture = false;
    } General;

    struct Visuals_t {
        bool Enable = true;

        // Enemy Count
        bool ShowNearEnemyCount = true;

        // Box
        bool Box = true;
        int players_box = 2; // 1 = Full Box, 2 = Corner Box
        float BoxColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        bool FilledBox = false;
        float Filledboxcolor[4] = { 0.0f, 0.0f, 0.0f, 0.35f };
        bool FillColorBox = false;
        float FillColor[4] = { 1.0f, 1.0f, 1.0f, 0.2f };

        // Skeleton
        bool Skeleton = false;
        float SkeletonColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        float SkeletonThickness = 1.0f;

        // Snaplines
        bool Lines = true;
        int EspLines = 1; // 1 = Top Middle, 2 = Bottom Middle
        float LinesColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        bool RainbowLines = false;
        bool GlowLines = true;
        float GlowRadius = 0.10f;
        float GlowFeather = 0.10f;
        float LineThickness = 1.0f;

        // Health Bar
        bool HealthBar = true;
        int players_healthbar = 2; // 0 = None, 1 = Left, 2 = Right, 3 = Bottom, 4 = Text
        bool ESPHealthTEXT = false;
        float texthColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

        // Weapon
        bool ESPWeapon = false;
        bool esparmas = false;
        bool ESPWeaponIcon = false;
        float GunColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        float IconScale = 0.92f;
        float IconOffsetX = 85.0f;
        float IconOffsetY = 0.7f;
        float IconOutline = 1.11f;
        float IconOutlineAlpha = 1.0f;

        // Name & Distance & Level
        bool Name = true;
        float NameColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

        bool Distance = true;
        float DistColor[4] = { 0.8f, 0.8f, 0.8f, 1.0f };
        int DistanceEsp = 250;

        bool Level = false;

        // Head Circle / Dot
        bool HeadDot = false;
        float HeadDotSize = 2.5f;
        float HeadDotColor[4] = { 1.0f, 0.2f, 0.2f, 1.0f };

        // In-game radar
        bool Radar = false;
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

        int esp_style = 1; // 1 = Clean White ESP (leakproject default)
    } Visuals;
};

inline EspGlobals g_Globals;
