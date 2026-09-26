#pragma once
#include "AimkillProtocol.hpp"
#include "../memory/app_config.hpp"
#include <string>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>

class AimkillClient {
public:
    static AimkillClient& Get();

    // Connect to the injected library's TCP server (forwarded on 127.0.0.1:21405)
    bool Connect(const std::string& host = "127.0.0.1", int port = 21405);
    void Disconnect();

    bool IsConnected() const { return m_connected.load(); }

    // Send generic request
    bool SendRequest(const AimkillRequest& req);

    // Convenience toggle helper
    bool SendToggle(int mode, bool enabled, float value = 0.0f, const std::string& gamePackage = externaltest::kDefaultGuestProcessFilter);

    // Retrieve latest player ESP data received from server
    bool GetLatestResponse(AimkillResponse& outResponse);

    // Screen resolution management (works on all resolutions: 720p, 900p, 1080p, 2K, etc.)
    void SetResolution(int width, int height);
    void GetResolution(int& outWidth, int& outHeight) const;
    bool AutoDetectResolution(const std::string& deviceAddr = "");

    // Callbacks
    using ConnectionCallback = std::function<void(bool connected)>;
    using ResponseCallback   = std::function<void(const AimkillResponse& response)>;

    void SetConnectionCallback(ConnectionCallback cb) { m_connCallback = cb; }
    void SetResponseCallback(ResponseCallback cb)     { m_respCallback = cb; }

private:
    AimkillClient();
    ~AimkillClient();

    AimkillClient(const AimkillClient&) = delete;
    AimkillClient& operator=(const AimkillClient&) = delete;

    void ReceiverWorker();

    std::atomic<bool> m_connected{ false };
    std::atomic<bool> m_stopWorker{ false };
    uintptr_t m_socket{ ~0ULL }; // SOCKET type without exposing windows.h everywhere
    std::atomic<int> m_screenWidth{ 0 };
    std::atomic<int> m_screenHeight{ 0 };

    std::mutex m_sendMutex;
    std::mutex m_respMutex;
    AimkillResponse m_latestResponse{};
    bool m_hasResponse{ false };

    std::thread m_workerThread;

    ConnectionCallback m_connCallback;
    ResponseCallback   m_respCallback;
};
