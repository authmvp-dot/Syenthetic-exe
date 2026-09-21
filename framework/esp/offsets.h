#pragma once
#include <cstdint>
#include <cstddef>

class Offsets {
public:

    static inline uintptr_t Il2Cpp = 0x0;

    // Auto-selected at runtime (see Data.cpp ResolveInitBase)
    static inline uintptr_t InitBase = 0xA342EFC;

    // Free Fire version InitBase candidates — scored at runtime (match/local/dict)
    static inline constexpr uintptr_t InitBaseCandidates[] = {
        0xA342EFC,
        0xA986E9C, // 442MB FF
        0xA986B7C, // COW_GameFacade_c
        0xA988FDC,
        0xA98D0CC,
        0xA997484,
        0xA9870BC,
        0xABFF3C0, // often false-positive on some builds — scored last
    };
    static inline constexpr size_t InitBaseCandidateCount =
        sizeof(InitBaseCandidates) / sizeof(InitBaseCandidates[0]);

    // ==================== CLASS OFFSETS ====================
    static inline uintptr_t StaticClass = 0x5C;

    // ==================== MATCH & GAME STATE ====================
    static inline uintptr_t CurrentMatch = 0x50;
    static inline uintptr_t MatchStatus = 0x8C;
    static inline uintptr_t LocalPlayer = 0x94;
    static inline uintptr_t DictionaryEntities = 0x68;
    static inline uintptr_t GameTimer = 0x10;
    static inline uintptr_t FixedDeltaTime = 0x24;

    // ==================== PLAYER BASE ====================
    static inline uintptr_t Player_IsDead = 0x50;
    static inline uintptr_t Player_Name = 0x31C;
    static inline uintptr_t Player_Data = 0x48;
    static inline uintptr_t ShadowState = 0x1A60;
    static inline uintptr_t Player_ShadowBase = 0x1A60;
    static inline uintptr_t XPose = 0x78;
    static inline uintptr_t InSnowSlideWayDashing = 0x15E8;

    // ==================== AVATAR & VISIBILITY ====================
    static inline uintptr_t AvatarManager = 0x504;
    static inline uintptr_t Avatar = 0xA8;
    static inline uintptr_t Avatar_IsVisible = 0x95;
    static inline uintptr_t Avatar_Data = 0x14;
    static inline uintptr_t Avatar_Data_IsTeam = 0x59;

    // ==================== CAMERA SYSTEM ====================
    static inline uintptr_t FollowCamera = 0x494;
    static inline uintptr_t Camera = 0x18;
    static inline uintptr_t AimRotation = 0x440;
    static inline uintptr_t MainCameraTransform = 0x28C;
    static inline uintptr_t ViewMatrix = 0xE8;

    // ==================== WEAPON SYSTEM ====================
    static inline uintptr_t Weapon = 0x434;
    static inline uintptr_t WeaponData = 0x58;
    static inline uintptr_t WeaponRecoil = 0x0C;

    // ==================== PLAYER ATTRIBUTES / STATE ====================
    static inline uintptr_t IsClientBot = 0x324;
    static inline uintptr_t Player_IsFemale = 0x7D8;
    static inline uintptr_t m_PlayerID = 0x268;
    static inline uintptr_t LevelUp = 0x18CC;

    // ==================== FIRING STATE ====================
    static inline uintptr_t isFiring = 0x58C;

    // ==================== MAP MARK TELEPORT ====================
    static inline uintptr_t UIBaseScene = 0x8;
    static inline uintptr_t m_BigMapCtrl = 0x218;
    static inline uintptr_t m_MapContentCtrl = 0x54;
    static inline uintptr_t m_LocalMapMarkController = 0x90;
    static inline uintptr_t markpos = 0x58;

    class Bones
    {
    public:
        // ===== HEAD & NECK =====
        static inline uintptr_t Head = 0x49C;
        static inline uintptr_t Neck = 0x4A4;

        // ===== SHOULDERS / ARMS =====
        static inline uintptr_t RightShoulder = 0x4D4;
        static inline uintptr_t LeftShoulder = 0x4D0;
        static inline uintptr_t RightElbow = 0x4E0;
        static inline uintptr_t LeftElbow = 0x4E4;
        static inline uintptr_t RightWrist = 0x4D8;
        static inline uintptr_t LeftWrist = 0x4DC;
        static inline uintptr_t RightHand = 0x498;
        static inline uintptr_t LeftHand = 0x4C8;

        // ===== BODY =====
        static inline uintptr_t Hip = 0x4A0;
        static inline uintptr_t Groin = 0x4AC;

        // ===== LEGS =====
        static inline uintptr_t RightAnkle = 0x4BC;
        static inline uintptr_t LeftAnkle = 0x4B8;
        static inline uintptr_t RightFoot = 0x4C4;
        static inline uintptr_t LeftFoot = 0x4C0;

        // ===== ROOT =====
        static inline uintptr_t Root = 0x4B0;
    };
};