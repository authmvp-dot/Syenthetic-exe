#include "AimkillClient.hpp"
#include "AimkillInjector.hpp"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include <iostream>
#include <cstring>
#include <vector>
#include <memory>

AimkillClient& AimkillClient::Get()
{
    static AimkillClient instance;
    return instance;
}

AimkillClient::AimkillClient()
{
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

AimkillClient::~AimkillClient()
{
    Disconnect();
    WSACleanup();
}

bool AimkillClient::Connect(const std::string& host, int port)
{
    Disconnect();

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        return false;
    }

    // Set 3-second receive timeout
    DWORD timeout = 3000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(static_cast<u_short>(port));
    inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr);

    if (connect(s, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(s);
        return false;
    }

    m_socket = static_cast<uintptr_t>(s);
    m_connected.store(true);
    m_stopWorker.store(false);

    if (m_connCallback) {
        m_connCallback(true);
    }

    // Start background receiver worker safely
    try {
        m_workerThread = std::thread(&AimkillClient::ReceiverWorker, this);
    } catch (...) {
        closesocket(s);
        m_socket = ~0ULL;
        m_connected.store(false);
        return false;
    }

    // Automatically detect display resolution from emulator / device
    AutoDetectResolution();

    return true;
}

void AimkillClient::Disconnect()
{
    bool wasConnected = m_connected.exchange(false);
    m_stopWorker.store(true);

    SOCKET s = static_cast<SOCKET>(m_socket);
    if (s != INVALID_SOCKET && s != static_cast<SOCKET>(~0ULL)) {
        shutdown(s, SD_BOTH);
        closesocket(s);
        m_socket = ~0ULL;
    }

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    if (wasConnected && m_connCallback) {
        m_connCallback(false);
    }
}

void AimkillClient::SetResolution(int width, int height)
{
    if (width > 0 && height > 0) {
        m_screenWidth.store(width);
        m_screenHeight.store(height);
    }
}

void AimkillClient::GetResolution(int& outWidth, int& outHeight) const
{
    outWidth = m_screenWidth.load();
    outHeight = m_screenHeight.load();
    if (outWidth <= 0 || outHeight <= 0) {
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        outWidth = (sw > 0) ? sw : 1920;
        outHeight = (sh > 0) ? sh : 1080;
    }
}

bool AimkillClient::AutoDetectResolution(const std::string& deviceAddr)
{
    int w = 0, h = 0;

    // 1. Try ADB wm size query directly from emulator/device (exact in-game resolution: 720p, 900p, 1080p, 2K, etc.)
    if (AimkillInjector::QueryDeviceResolution(deviceAddr, w, h) && w > 0 && h > 0) {
        SetResolution(w, h);
        return true;
    }

    // 2. Try emulator render window client rect (BlueStacks / MSI / LDPlayer)
    HWND hTarget = FindWindowW(NULL, L"BlueStacks App Player");
    if (!hTarget) hTarget = FindWindowW(NULL, L"MSI App Player");
    if (!hTarget) hTarget = FindWindowW(L"HD-Player", NULL);
    if (!hTarget) hTarget = FindWindowW(NULL, L"LDPlayer");

    if (hTarget) {
        RECT rc;
        if (GetClientRect(hTarget, &rc)) {
            int cw = rc.right - rc.left;
            int ch = rc.bottom - rc.top;
            if (cw > 200 && ch > 200) {
                SetResolution(cw, ch);
                return true;
            }
        }
    }

    // 3. Monitor metrics fallback
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    if (sw > 0 && sh > 0) {
        SetResolution(sw, sh);
        return true;
    }

    SetResolution(1920, 1080);
    return false;
}

static bool SendAll(SOCKET s, const char* data, int totalBytes)
{
    int bytesSent = 0;
    while (bytesSent < totalBytes) {
        int res = send(s, data + bytesSent, totalBytes - bytesSent, 0);
        if (res <= 0) return false;
        bytesSent += res;
    }
    return true;
}

static bool RecvAll(SOCKET s, char* outBuf, int totalBytes)
{
    int bytesRecv = 0;
    while (bytesRecv < totalBytes) {
        int res = recv(s, outBuf + bytesRecv, totalBytes - bytesRecv, 0);
        if (res <= 0) return false;
        bytesRecv += res;
    }
    return true;
}

bool AimkillClient::SendRequest(const AimkillRequest& req)
{
    if (!m_connected.load()) return false;

    std::lock_guard<std::mutex> lock(m_sendMutex);
    SOCKET s = static_cast<SOCKET>(m_socket);
    if (s == INVALID_SOCKET) return false;

    uint32_t payloadSize = sizeof(AimkillRequest);
    uint32_t netLen = htonl(payloadSize);

    // Send 4-byte big-endian length prefix
    if (!SendAll(s, reinterpret_cast<const char*>(&netLen), sizeof(netLen))) {
        Disconnect();
        return false;
    }

    // Send payload
    if (!SendAll(s, reinterpret_cast<const char*>(&req), payloadSize)) {
        Disconnect();
        return false;
    }

    return true;
}

bool AimkillClient::SendToggle(int mode, bool enabled, float value, const std::string& gamePackage)
{
    AimkillRequest req{};
    req.Mode = mode;
    req.boolean = enabled;
    req.value = value;

    // Dynamically retrieve screen width and height (works on any resolution: 720p, 900p, 1080p, 2K, etc.)
    int w = 0, h = 0;
    GetResolution(w, h);
    req.ScreenWidth = w;
    req.ScreenHeight = h;
    req.menuMode = 0;

    strncpy_s(req.gamePackage, sizeof(req.gamePackage), gamePackage.c_str(), _TRUNCATE);

    return SendRequest(req);
}

bool AimkillClient::GetLatestResponse(AimkillResponse& outResponse)
{
    std::lock_guard<std::mutex> lock(m_respMutex);
    if (!m_hasResponse) return false;
    outResponse = m_latestResponse;
    return true;
}

void AimkillClient::ReceiverWorker()
{
    SOCKET s = static_cast<SOCKET>(m_socket);

    while (!m_stopWorker.load() && m_connected.load()) {
        uint32_t netLen = 0;
        // Wait for 4-byte length
        int res = recv(s, reinterpret_cast<char*>(&netLen), sizeof(netLen), 0);
        if (res == 0) {
            // Graceful shutdown
            break;
        }
        if (res < 0) {
            int err = WSAGetLastError();
            if (err == WSAETIMEDOUT) {
                // Timeout is normal, loop again
                continue;
            }
            break;
        }

        if (res == sizeof(netLen)) {
            uint32_t payloadLen = ntohl(netLen);
            if (payloadLen > 0 && payloadLen <= sizeof(AimkillResponse) * 2) {
                std::vector<char> buffer(payloadLen);
                if (RecvAll(s, buffer.data(), payloadLen)) {
                    if (payloadLen == sizeof(AimkillResponse)) {
                        auto resp = std::make_unique<AimkillResponse>();
                        std::memcpy(resp.get(), buffer.data(), sizeof(AimkillResponse));

                        {
                            std::lock_guard<std::mutex> lock(m_respMutex);
                            m_latestResponse = *resp;
                            m_hasResponse = true;
                        }

                        if (m_respCallback) {
                            m_respCallback(*resp);
                        }
                    }
                } else {
                    break;
                }
            }
        }
    }

    m_connected.store(false);
}
