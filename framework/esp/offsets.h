#pragma once
#include <cstdint>
#include <cstddef>
#include "../memory/app_config.hpp"

class Offsets {
public:
    enum class GameType {
        FreeFire,
        FreeFireMax
    };

    static inline GameType CurrentGameType = GameType::FreeFire;

    static inline uintptr_t Il2Cpp = 0x0;

    // Auto-selected at runtime (see Data.cpp ResolveInitBase)
    static inline uintptr_t InitBase = 0x0;

    // Active candidate array (populated dynamically by SetGameType)
    static inline uintptr_t InitBaseCandidates[16] = { 0 };
    static inline size_t InitBaseCandidateCount = 0;

    // ==================== CLASS OFFSETS ====================
    static inline uintptr_t StaticClass = 0x0;

    // ==================== MATCH & GAME STATE ====================
    static inline uintptr_t CurrentMatch = 0x0;
    static inline uintptr_t MatchStatus = 0x0;
    static inline uintptr_t LocalPlayer = 0x0;
    static inline uintptr_t DictionaryEntities = 0x0;
    static inline uintptr_t GameTimer = 0x0;
    static inline uintptr_t FixedDeltaTime = 0x0;

    // ==================== PLAYER BASE ====================
    static inline uintptr_t Player_IsDead = 0x0;
    static inline uintptr_t Player_Name = 0x0;
    static inline uintptr_t Player_Data = 0x0;
    static inline uintptr_t ShadowState = 0x0;
    static inline uintptr_t Player_ShadowBase = 0x0;
    static inline uintptr_t XPose = 0x0;
    static inline uintptr_t InSnowSlideWayDashing = 0x0;

    // ==================== AVATAR & VISIBILITY ====================
    static inline uintptr_t AvatarManager = 0x0;
    static inline uintptr_t Avatar = 0x0;
    static inline uintptr_t Avatar_IsVisible = 0x0;
    static inline uintptr_t Avatar_Data = 0x0;
    static inline uintptr_t Avatar_Data_IsTeam = 0x0;

    // ==================== CAMERA SYSTEM ====================
    static inline uintptr_t FollowCamera = 0x0;
    static inline uintptr_t Camera = 0x0;
    static inline uintptr_t AimRotation = 0x0;
    static inline uintptr_t MainCameraTransform = 0x0;
    static inline uintptr_t ViewMatrix = 0x0;

    // ==================== WEAPON SYSTEM ====================
    static inline uintptr_t Weapon = 0x0;
    static inline uintptr_t WeaponData = 0x0;
    static inline uintptr_t WeaponRecoil = 0x0;

    // ==================== PLAYER ATTRIBUTES / STATE ====================
    static inline uintptr_t IsClientBot = 0x0;
    static inline uintptr_t Player_IsFemale = 0x0;
    static inline uintptr_t m_PlayerID = 0x0;
    static inline uintptr_t LevelUp = 0x0;

    // ==================== FIRING STATE ====================
    static inline uintptr_t isFiring = 0x0;

    // ==================== MAP MARK TELEPORT ====================
    static inline uintptr_t UIBaseScene = 0x0;
    static inline uintptr_t m_BigMapCtrl = 0x0;
    static inline uintptr_t m_MapContentCtrl = 0x0;
    static inline uintptr_t m_LocalMapMarkController = 0x0;
    static inline uintptr_t markpos = 0x0;

    class Bones
    {
    public:
        // ===== HEAD & NECK =====
        static inline uintptr_t Head = 0x0;
        static inline uintptr_t Neck = 0x0;

        // ===== SHOULDERS / ARMS =====
        static inline uintptr_t RightShoulder = 0x0;
        static inline uintptr_t LeftShoulder = 0x0;
        static inline uintptr_t RightElbow = 0x0;
        static inline uintptr_t LeftElbow = 0x0;
        static inline uintptr_t RightWrist = 0x0;
        static inline uintptr_t LeftWrist = 0x0;
        static inline uintptr_t RightHand = 0x0;
        static inline uintptr_t LeftHand = 0x0;

        // ===== BODY =====
        static inline uintptr_t Hip = 0x0;
        static inline uintptr_t Groin = 0x0;

        // ===== LEGS =====
        static inline uintptr_t RightAnkle = 0x0;
        static inline uintptr_t LeftAnkle = 0x0;
        static inline uintptr_t RightFoot = 0x0;
        static inline uintptr_t LeftFoot = 0x0;

        // ===== ROOT =====
        static inline uintptr_t Root = 0x0;
    };

    static void SetGameType(GameType type)
    {
        CurrentGameType = type;
        InitBase = 0; // Trigger auto re-scan

        if (type == GameType::FreeFireMax)
        {
            // Candidates for Free Fire MAX
            InitBaseCandidates[0] = 0xA4632F0; // InitBase
            InitBaseCandidates[1] = 0xA6D17D0; // GameFacade
            InitBaseCandidates[2] = 0xA6E2070; // InitBase_Single / GameFacadeClass
            InitBaseCandidates[3] = 0xA6E20C4; // _GameVarDefOff
            InitBaseCandidates[4] = 0xA342EFC;
            InitBaseCandidates[5] = 0xA986E9C;
            InitBaseCandidateCount = 6;

            StaticClass = 0x5C;
            CurrentMatch = 0x50;
            MatchStatus = 0x8C;
            LocalPlayer = 0x94;
            DictionaryEntities = 0xC8; // MAX
            GameTimer = 0x10;
            FixedDeltaTime = 0x24;

            Player_IsDead = 0x50;
            Player_Name = 0x324; // MAX
            Player_Data = 0x48;
            ShadowState = 0x1A70; // MAX
            Player_ShadowBase = 0x1A70; // MAX
            XPose = 0x0; // MAX
            InSnowSlideWayDashing = 0x15E8;

            AvatarManager = 0x50C; // MAX
            Avatar = 0xA8;
            Avatar_IsVisible = 0x0; // MAX
            Avatar_Data = 0x14;
            Avatar_Data_IsTeam = 0x59;

            FollowCamera = 0x49C; // MAX
            Camera = 0x18;
            AimRotation = 0x448; // MAX
            MainCameraTransform = 0x290; // MAX
            ViewMatrix = 0xE4; // MAX

            Weapon = 0x43C; // MAX
            WeaponData = 0x64; // MAX
            WeaponRecoil = 0x0C;

            IsClientBot = 0x32C; // MAX
            Player_IsFemale = 0x7D8;
            m_PlayerID = 0x268;
            LevelUp = 0x18CC;

            isFiring = 0x594; // MAX

            UIBaseScene = 0x8;
            m_BigMapCtrl = 0x240; // MAX
            m_MapContentCtrl = 0x5C; // MAX
            m_LocalMapMarkController = 0x94; // MAX
            markpos = 0x60; // MAX

            // Bones for MAX
            Bones::Head = 0x4A4;
            Bones::Neck = 0x4AC;
            Bones::RightShoulder = 0x4DC;
            Bones::LeftShoulder = 0x4D8;
            Bones::RightElbow = 0x4EC;
            Bones::LeftElbow = 0x4E8;
            Bones::RightWrist = 0x4E0;
            Bones::LeftWrist = 0x4E4;
            Bones::RightHand = 0x4D0;
            Bones::LeftHand = 0x4D0;
            Bones::Hip = 0x4B4;
            Bones::Groin = 0x4B8;
            Bones::RightAnkle = 0x4C4;
            Bones::LeftAnkle = 0x4C0;
            Bones::RightFoot = 0x4C8;
            Bones::LeftFoot = 0x4C4;
            Bones::Root = 0x4B8;
        }
        else
        {
            // Candidates for Free Fire Normal
            InitBaseCandidates[0] = 0xA342EFC;
            InitBaseCandidates[1] = 0xA986E9C;
            InitBaseCandidates[2] = 0xA986B7C;
            InitBaseCandidates[3] = 0xA988FDC;
            InitBaseCandidates[4] = 0xA98D0CC;
            InitBaseCandidates[5] = 0xA997484;
            InitBaseCandidates[6] = 0xA9870BC;
            InitBaseCandidates[7] = 0xABFF3C0;
            InitBaseCandidateCount = 8;

            StaticClass = 0x5C;
            CurrentMatch = 0x50;
            MatchStatus = 0x8C;
            LocalPlayer = 0x94;
            DictionaryEntities = 0x68;
            GameTimer = 0x10;
            FixedDeltaTime = 0x24;

            Player_IsDead = 0x50;
            Player_Name = 0x31C;
            Player_Data = 0x48;
            ShadowState = 0x1A60;
            Player_ShadowBase = 0x1A60;
            XPose = 0x78;
            InSnowSlideWayDashing = 0x15E8;

            AvatarManager = 0x504;
            Avatar = 0xA8;
            Avatar_IsVisible = 0x95;
            Avatar_Data = 0x14;
            Avatar_Data_IsTeam = 0x59;

            FollowCamera = 0x494;
            Camera = 0x18;
            AimRotation = 0x440;
            MainCameraTransform = 0x28C;
            ViewMatrix = 0xE8;

            Weapon = 0x434;
            WeaponData = 0x58;
            WeaponRecoil = 0x0C;

            IsClientBot = 0x324;
            Player_IsFemale = 0x7D8;
            m_PlayerID = 0x268;
            LevelUp = 0x18CC;

            isFiring = 0x58C;

            UIBaseScene = 0x8;
            m_BigMapCtrl = 0x218;
            m_MapContentCtrl = 0x54;
            m_LocalMapMarkController = 0x90;
            markpos = 0x58;

            // Bones for Normal FF
            Bones::Head = 0x49C;
            Bones::Neck = 0x4A4;
            Bones::RightShoulder = 0x4D4;
            Bones::LeftShoulder = 0x4D0;
            Bones::RightElbow = 0x4E0;
            Bones::LeftElbow = 0x4E4;
            Bones::RightWrist = 0x4D8;
            Bones::LeftWrist = 0x4DC;
            Bones::RightHand = 0x498;
            Bones::LeftHand = 0x4C8;
            Bones::Hip = 0x4A0;
            Bones::Groin = 0x4AC;
            Bones::RightAnkle = 0x4BC;
            Bones::LeftAnkle = 0x4B8;
            Bones::RightFoot = 0x4C4;
            Bones::LeftFoot = 0x4C0;
            Bones::Root = 0x4B0;
        }
    }

    static inline const char* GetCurrentPackageName() {
        return (CurrentGameType == GameType::FreeFireMax) ? externaltest::kDefaultGuestProcessFilterMax : externaltest::kDefaultGuestProcessFilter;
    }

    static inline const char* GetCurrentGameName() {
        return (CurrentGameType == GameType::FreeFireMax) ? "Free Fire MAX" : "Free Fire";
    }

    struct Initializer {
        Initializer() {
            SetGameType(GameType::FreeFire);
        }
    };
    static inline Initializer _init{};
};