#pragma once
#include <cstdint>
#include <atomic>
#include <thread>
#include "../math/vector3.hpp"
#include "player.h"
#include "esp_globals.h"
#include "../memory/memory.hpp"
#include "../memory/gva_memory_bridge.hpp"
#include "../memory/memoryengine.hpp"

namespace FWork {
    class Data {
    public:
        static void StartThread();
        static void StopThread();
        static bool ConnectEngine();
        static void DisconnectEngine();
        static bool IsConnected();

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
