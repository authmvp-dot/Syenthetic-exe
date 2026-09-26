#include "jumpmax.h"
#include "esp_globals.h"
#include "offsets.h"
#include "../memory/memory.hpp"

// External MemoryUtils instance (used throughout ESP and memory engine)
extern MemoryUtils Mem;

namespace JumpMax {

    // ==================== STATE DEFINITIONS ====================
    bool  s_enabled        = false;
    bool  s_doubleJump     = true;   // Air Jump / Multi Jump
    bool  s_airControl     = true;   // hook_pIsJumpCanMove (CanMoveInAir + ControlMoveSensibility)
    bool  s_lowGravity     = false;  // Low Gravity / Moon Jump
    bool  s_slopeClimb     = true;   // Hill & Mountain Climber
    float s_jumpMultiplier = 3.0f;   // 3x jump height default
    float s_gravityScale   = 0.35f;  // Float jump gravity scale

    // ==================== ORIGINAL VALUE CACHE ====================
    static float  s_origJumpHeight               = 3.0f;
    static int    s_origJumpMaxCount             = 1;
    static bool   s_origCanMoveInAir             = false;
    static float  s_origGravityScale             = 1.0f;
    static float  s_origControlMoveSensibility   = 1.0f;
    static float  s_origSlopeLimit               = 45.0f;
    static float  s_origStepOffset               = 0.3f;
    static bool   s_origForbidUphill             = true;
    static bool   s_hasOriginals                 = false;
    static uint32_t s_lastCCT                    = 0;
    static uint32_t s_lastPlayer                 = 0;

    // ==================== TICK ====================
    void Tick()
    {
        if (!s_enabled) return;
        if (!Mem.IsReady()) return;
        if (!g_Globals.EspConfig.Connected) return;
        if (!g_Globals.EspConfig.InMatch) return;

        uint32_t localPlayer = static_cast<uint32_t>(g_Globals.EspConfig.LocalPlayer);
        if (!localPlayer || localPlayer < 0x1000) return;

        // Read PhysicalCCT pointer from Player (<CKBBPOHMPGK>k__BackingField at 0x188)
        uint32_t cctPtr = Mem.ReadS<uint32_t>(localPlayer + PlayerOffsets::PhysicalCCT);
        if (!cctPtr || cctPtr < 0x1000) return;

        // Cache original values (once per CCT instance)
        if (!s_hasOriginals || cctPtr != s_lastCCT)
        {
            s_origJumpHeight             = Mem.ReadS<float>(cctPtr + CCTOffsets::JumpHeight);
            s_origJumpMaxCount           = Mem.ReadS<int>(cctPtr + CCTOffsets::JumpMaxCount);
            s_origCanMoveInAir           = Mem.ReadS<bool>(cctPtr + CCTOffsets::CanMoveInAir);
            s_origGravityScale           = Mem.ReadS<float>(cctPtr + CCTOffsets::GravityScale);
            s_origControlMoveSensibility = Mem.ReadS<float>(cctPtr + CCTOffsets::ControlMoveSensibility);
            s_origSlopeLimit             = Mem.ReadS<float>(cctPtr + CCTOffsets::SlopeLimit);
            s_origStepOffset             = Mem.ReadS<float>(cctPtr + CCTOffsets::StepOffset);
            s_origForbidUphill           = Mem.ReadS<bool>(cctPtr + CCTOffsets::ForbidUphillMovementWhenSliding);

            // Sanity bounds on cached values
            if (s_origJumpHeight < 0.1f || s_origJumpHeight > 100.0f)
                s_origJumpHeight = 3.0f;
            if (s_origGravityScale < 0.01f || s_origGravityScale > 10.0f)
                s_origGravityScale = 1.0f;
            if (s_origControlMoveSensibility < 0.01f || s_origControlMoveSensibility > 10.0f)
                s_origControlMoveSensibility = 1.0f;
            if (s_origSlopeLimit < 1.0f || s_origSlopeLimit > 90.0f)
                s_origSlopeLimit = 45.0f;

            s_lastCCT = cctPtr;
            s_lastPlayer = localPlayer;
            s_hasOriginals = true;
        }

        // 1. Super Jump Height
        float targetJumpHeight = s_origJumpHeight * s_jumpMultiplier;
        Mem.Write<float>(cctPtr + CCTOffsets::JumpHeight, targetJumpHeight);

        // 2. Air Control (Direct EXE equivalent of hook_pIsJumpCanMove)
        if (s_airControl)
        {
            Mem.Write<bool>(cctPtr + CCTOffsets::CanMoveInAir, true);
            Mem.Write<float>(cctPtr + CCTOffsets::ControlMoveSensibility, 2.5f);
        }

        // 3. Air Jump / Multi Jump (Infinite mid-air jumping)
        if (s_doubleJump)
        {
            // Set max jump count high on PhysicalCCT
            Mem.Write<int>(cctPtr + CCTOffsets::JumpMaxCount, 9999);

            // Continuously reset PhysicalCCT jump counter so next jump is always permitted
            int currentCctJumps = Mem.ReadS<int>(cctPtr + CCTOffsets::JumpCount);
            if (currentCctJumps > 0)
            {
                Mem.Write<int>(cctPtr + CCTOffsets::JumpCount, 0);
            }

            // Continuously reset Player-level jump counters
            int currentPlrJumps = Mem.ReadS<int>(localPlayer + PlayerOffsets::JumpCount);
            if (currentPlrJumps > 0)
            {
                Mem.Write<int>(localPlayer + PlayerOffsets::JumpCount, 0);
            }

            // Clear double jump triggered flag so jump button never grays out
            bool doubleJumpTriggered = Mem.ReadS<bool>(localPlayer + PlayerOffsets::IsDoubleJumpTriggered);
            if (doubleJumpTriggered)
            {
                Mem.Write<bool>(localPlayer + PlayerOffsets::IsDoubleJumpTriggered, false);
            }
        }

        // 4. Low Gravity / Moon Jump
        if (s_lowGravity)
        {
            Mem.Write<float>(cctPtr + CCTOffsets::GravityScale, s_gravityScale);
        }

        // 5. Mountain / Slope Climber (Walk up 89-degree surfaces without sliding)
        if (s_slopeClimb)
        {
            Mem.Write<float>(cctPtr + CCTOffsets::SlopeLimit, 89.0f);
            Mem.Write<float>(cctPtr + CCTOffsets::StepOffset, 2.0f);
            Mem.Write<bool>(cctPtr + CCTOffsets::ForbidUphillMovementWhenSliding, false);
        }
    }

    // ==================== RESET ====================
    void Reset()
    {
        if (!s_hasOriginals) return;
        if (!Mem.IsReady()) return;

        if (s_lastCCT && s_lastCCT >= 0x1000)
        {
            // Restore all PhysicalCCT original values
            Mem.Write<float>(s_lastCCT + CCTOffsets::JumpHeight, s_origJumpHeight);
            Mem.Write<int>(s_lastCCT + CCTOffsets::JumpMaxCount, s_origJumpMaxCount);
            Mem.Write<bool>(s_lastCCT + CCTOffsets::CanMoveInAir, s_origCanMoveInAir);
            Mem.Write<float>(s_lastCCT + CCTOffsets::GravityScale, s_origGravityScale);
            Mem.Write<float>(s_lastCCT + CCTOffsets::ControlMoveSensibility, s_origControlMoveSensibility);
            Mem.Write<float>(s_lastCCT + CCTOffsets::SlopeLimit, s_origSlopeLimit);
            Mem.Write<float>(s_lastCCT + CCTOffsets::StepOffset, s_origStepOffset);
            Mem.Write<bool>(s_lastCCT + CCTOffsets::ForbidUphillMovementWhenSliding, s_origForbidUphill);
        }

        s_hasOriginals = false;
        s_lastCCT = 0;
        s_lastPlayer = 0;
    }

} // namespace JumpMax
