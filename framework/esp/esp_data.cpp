#include "esp_data.h"
#include "esp_globals.h"
#include "offsets.h"
#include "../memory/memory.hpp"
#include "../math/tmatrix.hpp"
#include "../math/world_to_screen.hpp"

#include <chrono>
#include <cstdio>
#include <cstdarg>
#include <unordered_set>
#include <algorithm>
#include <iostream>

namespace FWork {

static void EspDbgThrottle(const char* key, const char* fmt, ...)
{
    static std::unordered_map<std::string, std::chrono::steady_clock::time_point> last;
    const auto now = std::chrono::steady_clock::now();
    auto& t = last[key];
    if (t.time_since_epoch().count() != 0 &&
        now - t < std::chrono::milliseconds(1000))
        return;
    t = now;

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    fflush(stdout);
}

static bool IsLikelyGuestPtr(uint32_t p)
{
    if (p < 0x10000 || p > 0xF0000000)
        return false;

    const uint8_t b0 = (uint8_t)(p);
    const uint8_t b1 = (uint8_t)(p >> 8);
    const uint8_t b2 = (uint8_t)(p >> 16);
    const uint8_t b3 = (uint8_t)(p >> 24);
    auto isPrint = [](uint8_t c) { return c >= 0x20 && c <= 0x7E; };
    if (isPrint(b0) && isPrint(b1) && isPrint(b2) && isPrint(b3))
        return false; // ASCII trap filter

    return true;
}

struct InitBaseProbe
{
    bool ok = false;
    uint32_t baseGameFacade = 0;
    uint32_t gameFacade = 0;
    uint32_t staticGameFacade = 0;
    uint32_t currentGame = 0;
    uint32_t currentMatch = 0;
    uint32_t matchStatus = 0;
    uint32_t localPlayer = 0;
    uint32_t entityDict = 0;
    int score = -1000;
};

static InitBaseProbe ProbeInitBase(uintptr_t initBaseOffset)
{
    InitBaseProbe r{};
    if (!Offsets::Il2Cpp || !initBaseOffset)
        return r;

    r.baseGameFacade = Mem.ReadS<uint32_t>(Offsets::Il2Cpp + initBaseOffset);
    if (!IsLikelyGuestPtr(r.baseGameFacade))
        return r;

    r.gameFacade = Mem.ReadS<uint32_t>(r.baseGameFacade);
    if (!IsLikelyGuestPtr(r.gameFacade))
        return r;

    r.staticGameFacade = Mem.ReadS<uint32_t>(r.gameFacade + Offsets::StaticClass);
    if (!IsLikelyGuestPtr(r.staticGameFacade))
        return r;

    r.currentGame = Mem.ReadS<uint32_t>(r.staticGameFacade);
    if (!IsLikelyGuestPtr(r.currentGame))
        return r;

    r.ok = true;
    r.score = 10;

    r.currentMatch = Mem.ReadS<uint32_t>(r.currentGame + Offsets::CurrentMatch);
    if (IsLikelyGuestPtr(r.currentMatch))
    {
        r.score += 50;
        r.matchStatus = Mem.ReadS<uint32_t>(r.currentMatch + Offsets::MatchStatus);
        if (r.matchStatus <= 16)
            r.score += 20;
        if (r.matchStatus == 1)
            r.score += 100;

        r.localPlayer = Mem.ReadS<uint32_t>(r.currentMatch + Offsets::LocalPlayer);
        if (IsLikelyGuestPtr(r.localPlayer))
            r.score += 80;
    }

    r.entityDict = Mem.ReadS<uint32_t>(r.currentGame + Offsets::DictionaryEntities);
    if (IsLikelyGuestPtr(r.entityDict))
    {
        r.score += 30;
        const uint32_t count = Mem.ReadS<uint32_t>(r.entityDict + 0x10);
        if (count >= 1 && count <= 200)
            r.score += 40;
    }

    return r;
}

static bool ResolveInitBase(uint32_t& outCurrentGame)
{
    static uintptr_t s_LastIl2Cpp = 0;
    static int s_LobbyStreak = 0;

    if (Offsets::Il2Cpp != s_LastIl2Cpp)
    {
        s_LastIl2Cpp = Offsets::Il2Cpp;
        Offsets::InitBase = 0;
        s_LobbyStreak = 0;
    }

    if (Offsets::InitBase != 0)
    {
        InitBaseProbe cached = ProbeInitBase(Offsets::InitBase);
        if (cached.ok && cached.score >= 60)
        {
            outCurrentGame = cached.currentGame;
            s_LobbyStreak = 0;
            return true;
        }

        if (cached.ok && cached.currentMatch == 0)
        {
            ++s_LobbyStreak;
            if (s_LobbyStreak < 3)
            {
                outCurrentGame = cached.currentGame;
                return true;
            }
            Offsets::InitBase = 0;
        }
        else if (!cached.ok)
        {
            Offsets::InitBase = 0;
        }
    }

    uintptr_t bestOff = 0;
    InitBaseProbe best{};
    best.score = -1000;

    for (size_t i = 0; i < Offsets::InitBaseCandidateCount; ++i)
    {
        const uintptr_t candidate = Offsets::InitBaseCandidates[i];
        InitBaseProbe p = ProbeInitBase(candidate);
        if (!p.ok)
            continue;

        if (p.score > best.score)
        {
            best = p;
            bestOff = candidate;
        }
    }

    if (bestOff != 0 && best.score >= 40)
    {
        Offsets::InitBase = bestOff;
        outCurrentGame = best.currentGame;
        s_LobbyStreak = 0;
        return true;
    }

    if (bestOff != 0 && best.ok && best.score >= 10)
    {
        Offsets::InitBase = bestOff;
        outCurrentGame = best.currentGame;
        return true;
    }

    Offsets::InitBase = 0;
    return false;
}

bool Data::ConnectEngine()
{
    try
    {
        g_connectStatus = "Refreshing...";
        g_espShutDown = true;
        MemoryUtils::BindBridge(nullptr);
        Offsets::Il2Cpp = 0;
        g_modeSelected = false;

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Step 1: Initialize memory engine with retries (like leakproject)
        bool engineOk = false;
        for (int retry = 0; retry < 5 && !engineOk; retry++) {
            engineOk = s_memoryEngine.Initialize(true);
            if (!engineOk)
                std::this_thread::sleep_for(std::chrono::milliseconds(400));
        }
        if (!engineOk)
        {
            g_connectStatus = s_memoryEngine.LastError().empty() ? "Engine Init Failed" : s_memoryEngine.LastError();
            g_Globals.EspConfig.Connected = false;
            return false;
        }

        // Step 2: Extract VM info
        char buffer[64];
        sprintf_s(buffer, "0x%llX", (unsigned long long)s_memoryEngine.pvm());
        g_pvm_str = buffer;
        sprintf_s(buffer, "0x%llX", (unsigned long long)s_memoryEngine.pvcpu());
        g_pvcpu_str = buffer;
        sprintf_s(buffer, "%d", s_memoryEngine.auto_vm().cpu_count);
        g_cpu_count_str = buffer;

        // Step 3: Find target game process
        g_connectStatus = "Finding Game...";
        externaltest::MapsService maps(s_memoryEngine);

        auto taskOpt = maps.FindTargetTask(externaltest::kDefaultGuestProcessFilter, false);
        if (!taskOpt.has_value())
        {
            g_connectStatus = "Game Not Found";
            g_Globals.EspConfig.Connected = false;
            return false;
        }

        const externaltest::TaskSnapshot& task = *taskOpt;

        // Step 4: Get CR3
        g_connectStatus = "Getting CR3...";
        const auto task_cr3 = maps.GetTaskCr3(task, false);
        if (!task_cr3.has_value())
        {
            g_connectStatus = "CR3 Failed";
            g_Globals.EspConfig.Connected = false;
            return false;
        }

        // Step 5: Get Il2Cpp base
        g_connectStatus = "Getting Il2Cpp...";
        const auto il2cpp_maps = maps.GetTaskMaps(task, externaltest::kDefaultGuestLibraryFilter, false);
        if (il2cpp_maps.empty())
        {
            g_connectStatus = "Il2Cpp Failed";
            g_Globals.EspConfig.Connected = false;
            return false;
        }

        // Step 6: Bind bridge
        g_connectStatus = "Binding Bridge...";
        s_gvaBridge.Attach(&s_memoryEngine, *task_cr3);
        s_gvaBridge.ClearTranslationCaches();

        MemoryUtils::BindBridge(&s_gvaBridge);
        Mem.Cache.clear();

        {
            std::unique_lock<std::shared_mutex> lock(g_Globals.EspConfig.EntitiesMutex);
            g_Globals.EspConfig.Entities.clear();
        }

        Offsets::Il2Cpp = (uintptr_t)il2cpp_maps.front().vm_start;
        Offsets::InitBase = 0; // force auto re-scan
        g_espShutDown = false;

        printf("[CONNECT] Il2Cpp = 0x%llX | maps=%zu | CR3 bind OK\n",
            (unsigned long long)Offsets::Il2Cpp,
            il2cpp_maps.size());
        printf("[CONNECT] InitBase auto-scan will run on next ESP tick\n");
        fflush(stdout);

        g_modeSelected = true;
        g_Globals.EspConfig.Connected = true;
        g_connectStatus = "Connected";
        return true;
    }
    catch (...)
    {
        g_connectStatus = "Connection Error";
        g_Globals.EspConfig.Connected = false;
        return false;
    }
}

void Data::TriggerRefresh()
{
    if (g_isConnecting.load()) return;
    g_isConnecting = true;

    std::thread([]()
    {
        bool ok = ConnectEngine();
        g_isConnecting = false;

        if (!ok)
        {
            printf("[CONNECT] TriggerRefresh failed: %s\n", g_connectStatus.c_str());
            fflush(stdout);
        }
    }).detach();
}

void Data::DisconnectEngine()
{
    g_espShutDown = true;
    g_Globals.EspConfig.Connected = false;
    MemoryUtils::BindBridge(nullptr);
    Offsets::Il2Cpp = 0;
    Offsets::InitBase = 0;
    g_modeSelected = false;
    g_connectStatus = "Connect Lib";
}

bool Data::IsConnected()
{
    return g_Globals.EspConfig.Connected && Mem.IsReady();
}

void Data::Refresh()
{
    Offsets::InitBase = 0;
    Mem.ClearEngineCaches();
    Reset();
}

void Data::StartThread()
{
    if (s_running.load()) return;
    s_running.store(true);
    s_workerThread = std::thread(WorkerThreadFunc);
}

void Data::StopThread()
{
    s_running.store(false);
    if (s_workerThread.joinable())
        s_workerThread.join();
}

void Data::WorkerThreadFunc()
{
    while (s_running.load())
    {
        try
        {
            // Auto-connect if enabled and not ready
            if (g_Globals.EspConfig.AutoRefresh && !IsConnected() && !g_isConnecting.load())
            {
                TriggerRefresh();
            }

            if (IsConnected() && !g_espShutDown.load())
            {
                Work();
            }
        }
        catch (...)
        {
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(8)); // ~120Hz scanning rate
    }
}

void Data::Work()
{
    try
    {
        if (Offsets::Il2Cpp == 0)
        {
            if (s_memoryEngine.IsInitialized() && s_memoryEngine.module().base != 0)
            {
                Offsets::Il2Cpp = static_cast<uintptr_t>(s_memoryEngine.module().base);
            }
            else
            {
                g_Globals.EspConfig.InMatch = false;
                g_Globals.EspConfig.CurrentMatch = 0;
                Reset();
                return;
            }
        }

        uint32_t currentGame = 0;
        if (!ResolveInitBase(currentGame))
        {
            Reset();
            return;
        }

        uint32_t currentMatch = Mem.ReadS<uint32_t>(currentGame + Offsets::CurrentMatch);
        if (!currentMatch)
        {
            g_Globals.EspConfig.InMatch = false;
            g_Globals.EspConfig.CurrentMatch = 0;
            Reset();
            return;
        }

        uint32_t matchStatus = Mem.ReadS<uint32_t>(currentMatch + Offsets::MatchStatus);
        g_Globals.EspConfig.CurrentMatch = currentMatch;
        g_Globals.EspConfig.InMatch = (matchStatus == 1);

        if (matchStatus != 1)
        {
            Reset();
            return;
        }

        uint32_t localPlayer = 0;
        if (!Mem.Read<uint32_t>(currentMatch + Offsets::LocalPlayer, localPlayer) || !localPlayer)
        {
            return;
        }
        g_Globals.EspConfig.LocalPlayer = localPlayer;

        // Camera Transform
        uint32_t mainTransform = Mem.ReadS<uint32_t>(localPlayer + Offsets::MainCameraTransform);
        if (!mainTransform)
            return;

        Vector3 mainPos;
        if (!TransformUtils::GetPosition(mainTransform, mainPos))
            return;
        g_Globals.EspConfig.MainCamera = mainPos;

        // View Matrix
        uint32_t followCamera = Mem.ReadS<uint32_t>(localPlayer + Offsets::FollowCamera);
        if (!followCamera) return;

        uint32_t camera = Mem.ReadS<uint32_t>(followCamera + Offsets::Camera);
        if (!camera) return;

        uint32_t cameraBase = Mem.ReadS<uint32_t>(camera + 0x8);
        if (!cameraBase) return;

        Matrix4x4 viewMatrix = Mem.ReadS<Matrix4x4>(cameraBase + Offsets::ViewMatrix);
        g_Globals.EspConfig.Matrix = true;
        g_Globals.EspConfig.ViewMatrix = viewMatrix;

        // Entity List
        uint32_t entityDictionary = 0;
        if (!Mem.Read<uint32_t>(currentGame + Offsets::DictionaryEntities, entityDictionary) || !entityDictionary)
        {
            Reset();
            return;
        }

        uint32_t entitiesCount = 0;
        if (!Mem.Read<uint32_t>(entityDictionary + 0x10, entitiesCount) || entitiesCount < 1 || entitiesCount > 2000)
        {
            return;
        }

        uint32_t entries = 0;
        if (!Mem.Read<uint32_t>(entityDictionary + 0xC, entries) || !entries)
        {
            return;
        }

        const uint32_t start = entries + 0x10;
        std::unordered_map<uint64_t, Player> tempEntities;
        tempEntities.reserve(entitiesCount);

        for (uint32_t i = 0; i < entitiesCount; ++i)
        {
            const uint32_t entry = start + (i * 0x10);

            int32_t hash = 0;
            if (!Mem.Read<int32_t>(entry + 0x0, hash) || hash < 0)
                continue;

            uint32_t entity = 0;
            if (!Mem.Read<uint32_t>(entry + 0xC, entity) || entity == 0 || entity == localPlayer)
                continue;

            Player entityPtr{};
            {
                std::shared_lock<std::shared_mutex> readLock(g_Globals.EspConfig.EntitiesMutex);
                auto it = g_Globals.EspConfig.Entities.find(entity);
                if (it != g_Globals.EspConfig.Entities.end())
                {
                    entityPtr = it->second;
                }
            }

            if (EntityData(entity, entityPtr, mainPos))
            {
                tempEntities[entity] = entityPtr;
            }
        }

        {
            std::unique_lock<std::shared_mutex> entitiesLock(g_Globals.EspConfig.EntitiesMutex);
            g_Globals.EspConfig.Entities = std::move(tempEntities);
        }
    }
    catch (...)
    {
    }
}

bool Data::EntityData(uint32_t entity, Player& player, Vector3& mainPos)
{
    player.Address = entity;
    try
    {
        uint32_t avatarManager = Mem.ReadS<uint32_t>(entity + Offsets::AvatarManager);
        if (avatarManager != 0)
        {
            uint32_t avatar = Mem.ReadS<uint32_t>(avatarManager + Offsets::Avatar);
            if (avatar != 0)
            {
                player.IsVisible = Mem.ReadS<bool>(avatar + Offsets::Avatar_IsVisible);

                uint32_t avatarData = Mem.ReadS<uint32_t>(avatar + Offsets::Avatar_Data);
                if (avatarData != 0)
                {
                    bool isTeam = Mem.ReadS<bool>(avatarData + Offsets::Avatar_Data_IsTeam);
                    player.IsTeam = isTeam ? Bool3::True : Bool3::False;
                    player.IsKnown = !isTeam;
                }
                else
                {
                    player.IsTeam = Bool3::Unknown;
                    player.IsKnown = true;
                }
            }
            else
            {
                player.IsTeam = Bool3::Unknown;
                player.IsKnown = true;
            }
        }
        else
        {
            player.IsTeam = Bool3::Unknown;
            player.IsKnown = true;
        }

        uint32_t shadowBase = Mem.ReadS<uint32_t>(entity + Offsets::ShadowState);
        if (shadowBase != 0) {
            player.Pose = static_cast<XPose>(Mem.ReadS<int>(shadowBase + Offsets::XPose));
            player.IsKnocked = (player.Pose == XPose::Knocked);
        }

        player.IsDead = Mem.ReadS<bool>(entity + Offsets::Player_IsDead);
        player.IsBot = Mem.ReadS<bool>(entity + Offsets::IsClientBot);

        uint32_t dataPool = Mem.ReadS<uint32_t>(entity + Offsets::Player_Data);
        if (dataPool)
        {
            uint32_t poolObj = Mem.ReadS<uint32_t>(dataPool + 0x8);
            if (poolObj)
            {
                uint32_t weaponPool = Mem.ReadS<uint32_t>(poolObj + 0x20);
                if (weaponPool)
                    player.WeaponID = Mem.ReadS<short>(weaponPool + 0x10);

                uint32_t healthPool = Mem.ReadS<uint32_t>(poolObj + 0x10);
                if (healthPool)
                    player.Health = Mem.ReadS<short>(healthPool + 0x10);
            }
        }

        if (player.IsDead)
        {
            player.Distance = 0.0f;
            return false;
        }

        uint32_t headPtr = 0;
        if (Mem.Read<uint32_t>(entity + Offsets::Bones::Head, headPtr) && headPtr != 0) {
            TransformUtils::GetNodePosition(headPtr, player.Head);
        }
        uint32_t rootPtr = 0;
        if (Mem.Read<uint32_t>(entity + Offsets::Bones::Root, rootPtr) && rootPtr != 0) {
            TransformUtils::GetNodePosition(rootPtr, player.Root);
        }
        if (player.Head == Vector3::Zero()) {
            uint32_t neckPtr = 0;
            if (Mem.Read<uint32_t>(entity + Offsets::Bones::Neck, neckPtr) && neckPtr != 0) {
                TransformUtils::GetNodePosition(neckPtr, player.Neck);
            }
        }

        // Strict rejection: Must have a valid root or head bone in world space
        if (player.Head == Vector3::Zero() && player.Root == Vector3::Zero())
        {
            return false;
        }

        if (g_Globals.Visuals.Skeleton)
        {
            struct BoneEntry {
                uint32_t offset;
                Vector3* target;
            };
            const BoneEntry skeletonBones[] = {
                { (uint32_t)Offsets::Bones::Neck, &player.Neck },
                { (uint32_t)Offsets::Bones::LeftShoulder, &player.LeftShoulder },
                { (uint32_t)Offsets::Bones::RightShoulder, &player.RightShoulder },
                { (uint32_t)Offsets::Bones::LeftElbow, &player.LeftElbow },
                { (uint32_t)Offsets::Bones::RightElbow, &player.RightElbow },
                { (uint32_t)Offsets::Bones::LeftWrist, &player.LeftWrist },
                { (uint32_t)Offsets::Bones::RightWrist, &player.RightWrist },
                { (uint32_t)Offsets::Bones::LeftHand, &player.LeftHand },
                { (uint32_t)Offsets::Bones::RightHand, &player.RightHand },
                { (uint32_t)Offsets::Bones::Hip, &player.Hip },
                { (uint32_t)Offsets::Bones::Groin, &player.Groin },
                { (uint32_t)Offsets::Bones::LeftAnkle, &player.LeftAnkle },
                { (uint32_t)Offsets::Bones::RightAnkle, &player.RightAnkle },
                { (uint32_t)Offsets::Bones::LeftFoot, &player.LeftFoot },
                { (uint32_t)Offsets::Bones::RightFoot, &player.RightFoot },
            };

            for (const auto& entry : skeletonBones) {
                uint32_t bonePtr = 0;
                if (Mem.Read<uint32_t>(entity + entry.offset, bonePtr) && bonePtr != 0) {
                    TransformUtils::GetNodePosition(bonePtr, *(entry.target));
                }
            }
        }

        Vector3 targetPos = (player.Head != Vector3::Zero()) ? player.Head : ((player.Neck != Vector3::Zero()) ? player.Neck : player.Root);
        if (targetPos == Vector3::Zero() || mainPos == Vector3::Zero())
        {
            player.Distance = 0.0f;
            return false;
        }

        player.Distance = Vector3::Distance(mainPos, targetPos);
        if (player.Distance <= 0.5f || player.Distance > 1000.0f)
        {
            return false;
        }

        player.IsFemale = Mem.ReadS<bool>(player.Address + Offsets::Player_IsFemale);

        uint32_t nameAddr = 0;
        if (Mem.Read(entity + Offsets::Player_Name, nameAddr) && nameAddr > 0x10000)
        {
            int32_t nameLen = 0;
            if (Mem.Read(nameAddr + 0x8, nameLen) && nameLen > 0 && nameLen <= 64)
            {
                std::string name = Mem.String(nameAddr + 0xC, nameLen * 2, true);
                if (!name.empty() && name.size() < 128)
                {
                    name.erase(std::remove_if(name.begin(), name.end(), [](unsigned char ch) {
                        return ch < 0x20 && ch != '\t';
                    }), name.end());
                    if (!name.empty())
                        player.Name = std::move(name);
                }
            }
        }

        player.ID = Mem.ReadS<uint64_t>(entity + Offsets::m_PlayerID);

        uint32_t profileInfo = Mem.ReadS<uint32_t>(entity + Offsets::LevelUp);
        if (profileInfo != 0) player.Level = Mem.ReadS<int>(profileInfo + 0x14);

        return true;
    }
    catch (...)
    {
        return false;
    }
}

void Data::Reset()
{
    try
    {
        g_Globals.EspConfig.InMatch = false;
        g_Globals.EspConfig.Matrix = false;
        g_Globals.EspConfig.LocalPlayer = 0;
        g_Globals.EspConfig.CurrentMatch = 0;

        {
            std::unique_lock<std::shared_mutex> lock(g_Globals.EspConfig.EntitiesMutex);
            g_Globals.EspConfig.Entities.clear();
        }
        g_Globals.EspConfig.ViewMatrix = Matrix4x4();
        g_Globals.EspConfig.MainCamera = Vector3(0, 0, 0);

        Mem.Cache.clear();
    }
    catch (...)
    {
    }
}

} // namespace FWork
