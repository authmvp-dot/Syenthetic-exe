#pragma once
#include <cstdint>
#include <atomic>
#include <thread>
#include <string>
#include "../math/vector3.hpp"
#include "player.h"
#include "esp_globals.h"
#include "../memory/memory.hpp"
#include "../memory/gva_memory_bridge.hpp"
#include "../memory/memoryengine.hpp"
#include "../memory/maps_service.hpp"
#include "../memory/app_config.hpp"

namespace FWork {
    // Shared state flags (matching leakproject's shared_data)
    inline std::atomic<bool> g_espShutDown{ false };
    inline std::atomic<bool> g_modeSelected{ false };
    inline std::string g_connectStatus = "Connect Lib";
    inline std::atomic<bool> g_isConnecting{ false };
    inline std::string g_pvm_str;
    inline std::string g_pvcpu_str;
    inline std::string g_cpu_count_str;

    class Data {
    public:
        static void StartThread();
        static void StopThread();
        static bool ConnectEngine();
        static void DisconnectEngine();
        static bool IsConnected();
        static void TriggerRefresh();  // Async connect (like leakproject's triggerRefresh)

        static void Refresh();
        static void Work();
        static void Reset();

    private:
        static bool EntityData(uint32_t entity, Player& player, Vector3& mainPos);
        static void WorkerThreadFunc();

        inline static std::atomic<bool> s_running{ false };
        inline static std::thread s_workerThread;
        inline static externaltest::MemoryEngine s_memoryEngine;
        inline static externaltest::GvaMemoryBridge s_gvaBridge;
    };
}
