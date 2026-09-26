#pragma once
#include <cstdint>

// ==============================================================================
// JumpMax — External Super Jump & Air Movement via PhysicalCCT Memory Manipulation
// ==============================================================================
// Implements the EXE equivalent of hook_pIsJumpCanMove from Server.cpp, plus
// advanced physics manipulation directly in guest memory using granular offsets
// verified from Dump.cs (COW.GamePlay.Player & PhysicalCCT).
//
// Features:
//   - hook_pIsJumpCanMove equivalent: CanMoveInAir (0x74) + ControlMoveSensibility (0x50)
//   - Super Jump: JumpHeight (0x68) scaled by multiplier
//   - Air Jump / Multi Jump: JumpMaxCount (0x6C) + JumpCount (0x70) + Player.JumpCount (0x180)
//   - Low Gravity / Moon Jump: GravityScale (0x3C)
//   - Mountain / Slope Climber: SlopeLimit (0x10) + ForbidUphillMovementWhenSliding (0x85)
// ==============================================================================

namespace JumpMax {

    // ==================== PLAYER OFFSETS (COW.GamePlay.Player, TypeDefIndex: 31821) ====================
    struct PlayerOffsets {
        static constexpr uintptr_t UGCRequestJump               = 0x17C;  // bool
        static constexpr uintptr_t JumpCount                    = 0x180;  // int32
        static constexpr uintptr_t PhysicalCCT                  = 0x188;  // PhysicalCCT (<CKBBPOHMPGK>k__BackingField)
        static constexpr uintptr_t IsDoubleJumpTriggered        = 0x1010; // bool
        static constexpr uintptr_t ResetSecondJumpRushTime      = 0x1014; // float
        static constexpr uintptr_t DisableJump                  = 0x15F8; // EEKFBNLKAMI
        static constexpr uintptr_t LockMove                     = 0x160C; // EEKFBNLKAMI
        static constexpr uintptr_t FlyUPDistance                = 0x1648; // float
    };

    // ==================== PhysicalCCT OFFSETS (MonoBehaviour, TypeDefIndex: 34486) ====================
    struct CCTOffsets {
        static constexpr uintptr_t SlopeLimit                   = 0x10;   // float (default ~45.0)
        static constexpr uintptr_t StepOffset                   = 0x14;   // float (default ~0.3)
        static constexpr uintptr_t SkinWidth                    = 0x18;   // float
        static constexpr uintptr_t Radius                       = 0x1C;   // float
        static constexpr uintptr_t Height                       = 0x20;   // float
        static constexpr uintptr_t MinMoveDistance              = 0x24;   // float
        static constexpr uintptr_t IsServerDriven               = 0x28;   // bool
        static constexpr uintptr_t Mass                         = 0x2C;   // float
        static constexpr uintptr_t MaxSpeed                     = 0x34;   // float
        static constexpr uintptr_t UseGravity                   = 0x38;   // bool
        static constexpr uintptr_t GravityScale                 = 0x3C;   // float (default 1.0)
        static constexpr uintptr_t EnableRotateWithGravity      = 0x4C;   // bool
        static constexpr uintptr_t SmoothMovement               = 0x4F;   // bool
        static constexpr uintptr_t ControlMoveSensibility       = 0x50;   // float (air control sensitivity)
        static constexpr uintptr_t LoseControlMoveSensibility   = 0x54;   // float
        static constexpr uintptr_t RotateSensibility            = 0x58;   // float
        static constexpr uintptr_t UpDirectionRotateSensibility = 0x5C;   // float
        static constexpr uintptr_t UseCustomPushForce           = 0x60;   // bool
        static constexpr uintptr_t CustomPushForce              = 0x64;   // float
        static constexpr uintptr_t JumpHeight                   = 0x68;   // float (default ~2.0-3.5)
        static constexpr uintptr_t JumpMaxCount                 = 0x6C;   // int32 (default 1)
        static constexpr uintptr_t JumpCount                    = 0x70;   // int32
        static constexpr uintptr_t CanMoveInAir                 = 0x74;   // bool  *** hook_pIsJumpCanMove core ***
        static constexpr uintptr_t InteractiveRigidbodyHandling = 0x75;   // bool
        static constexpr uintptr_t SolveMovementCollisions      = 0x7D;   // bool
        static constexpr uintptr_t ShouldDetectGroundHit        = 0x84;   // bool
        static constexpr uintptr_t ForbidUphillMovementWhenSliding = 0x85; // bool
        static constexpr uintptr_t SlightMove                   = 0x86;   // bool
        static constexpr uintptr_t Velocity                     = 0x150;  // Vector3
        static constexpr uintptr_t AdditionalForceToApply       = 0x170;  // Vector3
    };

    // Legacy alias for backwards compatibility
    static constexpr uintptr_t Player_PhysicalCCT = PlayerOffsets::PhysicalCCT;

    // ==================== STATE ====================
    extern bool  s_enabled;           // Master toggle
    extern bool  s_doubleJump;        // Multi/Air Jump (JumpMaxCount = 9999 + resets)
    extern bool  s_airControl;        // Air Control / hook_pIsJumpCanMove (CanMoveInAir = true)
    extern bool  s_lowGravity;        // Low Gravity / Moon Jump
    extern bool  s_slopeClimb;        // Mountain / Slope Climber (SlopeLimit = 89 deg)
    extern float s_jumpMultiplier;   // Jump height multiplier (1.0x to 10.0x)
    extern float s_gravityScale;     // Custom gravity scale (default 0.35f)

    // ==================== FUNCTIONS ====================

    // Called every ESP tick. Reads LocalPlayer, accesses PhysicalCCT, and applies memory writes.
    void Tick();

    // Restores original values to PhysicalCCT and Player memory.
    void Reset();

} // namespace JumpMax
