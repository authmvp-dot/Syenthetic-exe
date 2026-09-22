#pragma once
#include <string>
#include <functional>

namespace AimkillInjector {

    // Status callback signature: void(const std::string& msg, bool isError)
    using StatusCallback = std::function<void(const std::string& msg, bool isError)>;

    // Locate the best ADB executable available on the system
    std::string FindAdbExecutable();

    // Query device/emulator resolution via ADB (wm size)
    bool QueryDeviceResolution(const std::string& deviceAddr, int& outWidth, int& outHeight);

    // Prepare payloads: YEROXPAPA GitHub (default) → XOR embed → disk
    // Defaults hardcoded to https://github.com/authmvp-dot/libshitaim
    // Override: AIMKILL_PAYLOAD_BASE_URL / TOKEN / ONLINE_ONLY / INJ_URL / LIB_URL
    bool ExtractEmbeddedFiles(std::string& outInjPath, std::string& outLibPath, std::string& outError);

    // Last payload source: "github" | "embedded" | "disk" | "none"
    const char* LastPayloadSource();

    // Full injection pipeline
    bool InjectIntoEmulator(
        const std::string& deviceAddr,
        const std::string& packageName,
        StatusCallback onStatus = nullptr
    );

    // Schedule safe delayed cleanup of PC staging files
    void ScheduleSafeCleanup(int delaySeconds = 10);

    // Immediate cleanup — delete extracted payloads from PC
    void CleanupStagingFiles();

} // namespace AimkillInjector
