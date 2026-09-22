#pragma once
// ============================================================
// AimkillProtocol.hpp
// Packed protocol structs matching Android Server.cpp exactly.
// Port: 21405
// Wire format: [4-byte length (network order)] + [payload]
// ============================================================

#include <cstdint>

#pragma pack(push, 1)

struct AimkillVec3 {
    float X, Y, Z;
};

struct AimkillRequest {
    int Mode;
    bool boolean;
    float value;
    int ScreenWidth;
    int ScreenHeight;
    char gamePackage[64];
    int menuMode;
};

#define AIMKILL_MAX_PLAYERS 54

struct PlayerData {
    AimkillVec3 headPosition;
    AimkillVec3 bottomPlayerPosition;
    float health;
    char name[2000];
    int32_t weaponId;
    bool isDieing;
    float distance;
};

struct AimkillResponse {
    bool Success;
    int PlayerCount;
    PlayerData Players[AIMKILL_MAX_PLAYERS];
    int remainingSeconds;
    bool showFinishMessage;
};

#pragma pack(pop)

// ============================================================
// Mode Constants (from Server.cpp)
// ============================================================
namespace AimkillMode {
    // Core modes
    constexpr int Init           = 1;
    constexpr int Hack           = 2;
    constexpr int Stop           = 98;
    constexpr int Esp            = 99;

    // ESP / Visuals
    constexpr int EnableESP      = 3;
    constexpr int DamageTextRGB  = 200;
    constexpr int CrosshairColor = 2000;
    constexpr int EspGrenade     = 1000;
    constexpr int EspFire        = 1001;
    constexpr int EspFireBlue    = 1002;

    // Aimbot
    constexpr int EnableAimbot      = 101;
    constexpr int AimbotShoot       = 102;
    constexpr int AimbotFOV         = 104;
    constexpr int AimbotSmoothness  = 107;
    constexpr int AimbotBody        = 108;
    constexpr int SpeedHack         = 109;
    constexpr int SilentAim         = 111;
    constexpr int SilentAimV2       = 1111;
    constexpr int CameraUp          = 1111;
    constexpr int AimSilent         = 22;    // Silent Kill
    constexpr int AimSilent360      = 1056;  // Silent Kill 360
    constexpr int AimBody           = 21;

    // Aimkill variants
    constexpr int Aimkill           = 333668;
    constexpr int Aimkill360        = 1055;
    constexpr int AutoTarget        = 1055;
    constexpr int AimkillTP         = 500;
    constexpr int AimkillTPv2       = 501;
    constexpr int AimkillRotateV3   = 506;
    constexpr int DownAimkill       = 504;
    constexpr int UnderKill         = 432121;
    constexpr int ShakeKill         = 5045;
    constexpr int ShakeKillUltra    = 2660;
    constexpr int ShakeTagda        = 506666;
    constexpr int ShakeXXX          = 2660112;
    constexpr int CoverKill         = 65561;
    constexpr int AutoSwitch        = 505;
    constexpr int MKC               = 1454;

    // New aimkill modes (9xxx)
    constexpr int NoHitDelay        = 9001;
    constexpr int LundLeLoKill      = 9002;
    constexpr int AutoSwitchNew     = 9003;
    constexpr int CoverShotNew      = 9004;
    constexpr int ShieldBypass      = 9005;

    // Fire / Movement
    constexpr int AutoFire          = 115;
    constexpr int AutoFireV2        = 175;
    constexpr int UnlimitedAmmo     = 950;
    constexpr int MultiTeleport     = 7788;
    constexpr int SequenceTP        = 7777;
    constexpr int MarkTPAnywhere    = 7778;
    constexpr int TeleportLoop      = 15416;
    constexpr int TeleHack          = 19;
    constexpr int SpeedDash         = 3436687;
    constexpr int SpeedHackJoy      = 15;
    constexpr int Speed             = 51407;

    // Fly hacks
    constexpr int FlyHack           = 505505;
    constexpr int FlyUpNew          = 26112;
    constexpr int FlyX80New         = 5194;
    constexpr int FlyLock           = 5195;
    constexpr int FlyJump           = 20012221;
    constexpr int SkyCsTP           = 1057;
    constexpr int TeleMark          = 88471;
    constexpr int StopSpt           = 88472;
    constexpr int FlyHackNew        = 88474;
    constexpr int FlyHackHeight     = 88475;
    constexpr int Invisible         = 2005;
    constexpr int InvisibleKill     = 27556;

    // Misc
    constexpr int GhostHack         = 5110;
    constexpr int GhostOn           = 149;
    constexpr int ClimbUp           = 509;
    constexpr int BackJump          = 1058;
    constexpr int UpPlayerX         = 20;
    constexpr int Underground       = 6;
    constexpr int FBBypass          = 10;
    constexpr int ResetGuest        = 12;
    constexpr int MedikitRun        = 13;
    constexpr int WallHack          = 166;
    constexpr int BlackSky          = 777;
    constexpr int AntiCheat         = 23;
    constexpr int PullEnemy550      = 1333;
    constexpr int EnemyPull         = 102011;
    constexpr int EnemyDance360     = 510010;
    constexpr int AutoPullToggle    = 3434434;
    constexpr int DoubleGun         = 2660235;
    constexpr int Football          = 655611;
    constexpr int InfiniteFootball  = 502;
    constexpr int SpeedFootball     = 503;
    constexpr int FakeLoginError    = 3436686;
    constexpr int GliderBKC         = 4343676;
    constexpr int NickFuck          = 5644;
    constexpr int LevelHack         = 2522;
    constexpr int Fucked            = 2121211;
    constexpr int AutoRevive        = 4510;
    constexpr int FuccccckingGG     = 7799;

    constexpr int LookDreamspace    = 7373;
    constexpr int LookRampage       = 7378;
    constexpr int LookItachi        = 7379;
    constexpr int LookMidnightAce   = 7380;
    constexpr int LookAurora        = 7381;
    constexpr int LookNarutoAscent  = 7382;
    constexpr int LookLastParadox   = 7383;
    constexpr int LookFrostfire     = 7384;
    constexpr int LookScorpio       = 7385;
    constexpr int LookDevilTrigger  = 7386;
    constexpr int LookCannibalHavoc = 7387;
}
