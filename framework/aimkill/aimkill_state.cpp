#include "aimkill_state.h"
#include "AimkillClient.hpp"
#include "AimkillInjector.hpp"
#include "AimkillProtocol.hpp"
#include "../settings/functions.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <thread>
#include <chrono>

namespace AimkillState {

    bool isServerConnected = false;
    bool isServerConnecting = false;
    std::string server_btn = "Connect Server";
    char deviceAddr[64] = "127.0.0.1:5555";
    int selectedPkg = 0;
    const char* pkgNames[2] = { "com.dts.freefireth", "com.dts.freefiremax" };

    // AIM
    bool s_enableAll = false;
    bool s_aimkill = false;
    bool s_autoSwitch = false;
    bool s_coverShot = false;
    bool s_shieldBypass = false;
    bool s_espGrenade = false;
    bool s_speedHackJoy = false;
    bool s_resetGuest = false;
    bool s_speedDash = false;
    bool s_downAimkill = false;
    int  s_downAimkill_key = 0;

    // LOOK
    bool s_lookDreamspace = false;
    bool s_lookRampage = false;
    bool s_lookItachi = false;
    bool s_lookMidnightAce = false;
    bool s_lookAurora = false;
    bool s_lookNarutoAscent = false;
    bool s_lookLastParadox = false;
    bool s_lookFrostfire = false;
    bool s_lookScorpio = false;
    bool s_lookDevilTrigger = false;
    bool s_lookCannibalHavoc = false;

    // BRUTAL
    bool s_flyUpNew = false;
    int  s_flyUpNew_key = 0;
    bool s_medikitRun = false;
    bool s_wallHack = false;
    bool s_cameraUp = false;
    bool s_autoTarget = false;
    bool s_blackSky = false;
    bool s_teleMark = false;
    int  s_teleMark_key = 0;
    bool s_stopSpt = false;
    int  s_stopSpt_key = 0;
    bool s_flyHackNew = false;
    int  s_flyHackNew_key = 0;
    int  s_flyHackHeight = 15;
    bool s_invisible = false;
    bool s_invisibleKill = false;

    static bool* g_lookToggles[] = {
        &s_lookDreamspace, &s_lookRampage, &s_lookItachi, &s_lookMidnightAce,
        &s_lookAurora, &s_lookNarutoAscent, &s_lookLastParadox, &s_lookFrostfire,
        &s_lookScorpio, &s_lookDevilTrigger, &s_lookCannibalHavoc
    };

    static const int g_lookModes[] = {
        AimkillMode::LookDreamspace, AimkillMode::LookRampage, AimkillMode::LookItachi,
        AimkillMode::LookMidnightAce, AimkillMode::LookAurora, AimkillMode::LookNarutoAscent,
        AimkillMode::LookLastParadox, AimkillMode::LookFrostfire, AimkillMode::LookScorpio,
        AimkillMode::LookDevilTrigger, AimkillMode::LookCannibalHavoc
    };

    void ApplyLookToggle(bool* selected, int mode, const char* name)
    {
        if (*selected) {
            for (int i = 0; i < 11; i++) {
                if (g_lookToggles[i] != selected)
                    *g_lookToggles[i] = false;
            }
        }
        if (isServerConnected) {
            AimkillClient::Get().SendToggle(mode, *selected, 0.0f, pkgNames[selectedPkg]);
        }
        Beep(*selected ? 800 : 500, 45);
        if (notify) {
            notify->add_notify(*selected ? (std::string(name) + " On") : (std::string(name) + " Off"), 3, static_cast<notify_position>(var->c_notify.notify_position));
        }
    }

    void ConnectServerAsync()
    {
        if (isServerConnecting || isServerConnected) return;

        isServerConnecting = true;
        server_btn = "Starting...";
        std::string dev = deviceAddr;
        std::string pkg = pkgNames[selectedPkg];

        std::thread([dev, pkg]() {
            try {
                // Step 1: Inject embedded binaries via ADB
                bool ok = AimkillInjector::InjectIntoEmulator(dev, pkg, [](const std::string& msg, bool isErr) {
                    server_btn = msg;
                });

                if (!ok) {
                    server_btn = "Inject Failed";
                    isServerConnecting = false;
                    Beep(500, 45);
                    if (notify) {
                        notify->add_notify("Injection Failed! Check ADB/Device.", 4, static_cast<notify_position>(var->c_notify.notify_position));
                    }
                    return;
                }

                // Step 2: Connect socket to port 21405
                server_btn = "Connecting socket...";
                std::this_thread::sleep_for(std::chrono::seconds(1));

                bool connected = false;
                for (int retry = 0; retry < 6; retry++) {
                    if (AimkillClient::Get().Connect("127.0.0.1", 21405)) {
                        connected = true;
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(800));
                }

                if (connected) {
                    isServerConnected = true;
                    isServerConnecting = false;
                    server_btn = "Connected";

                    // Sync active toggles to the newly connected server
                    if (s_enableAll) AimkillClient::Get().SendToggle(AimkillMode::EnableESP, true, 0.0f, pkg);
                    if (s_aimkill) AimkillClient::Get().SendToggle(AimkillMode::LundLeLoKill, true, 0.0f, pkg);
                    if (s_autoSwitch) AimkillClient::Get().SendToggle(AimkillMode::AutoSwitchNew, true, 0.0f, pkg);
                    if (s_coverShot) AimkillClient::Get().SendToggle(AimkillMode::CoverShotNew, true, 0.0f, pkg);
                    if (s_shieldBypass) AimkillClient::Get().SendToggle(AimkillMode::ShieldBypass, true, 0.0f, pkg);
                    if (s_espGrenade) AimkillClient::Get().SendToggle(AimkillMode::EspGrenade, true, 0.0f, pkg);
                    if (s_speedHackJoy) AimkillClient::Get().SendToggle(AimkillMode::SpeedHackJoy, true, 0.0f, pkg);
                    if (s_resetGuest) AimkillClient::Get().SendToggle(AimkillMode::ResetGuest, true, 0.0f, pkg);
                    if (s_speedDash) AimkillClient::Get().SendToggle(AimkillMode::SpeedDash, true, 0.0f, pkg);
                    if (s_downAimkill) AimkillClient::Get().SendToggle(AimkillMode::DownAimkill, true, 0.0f, pkg);

                    for (int li = 0; li < 11; li++) {
                        if (*g_lookToggles[li])
                            AimkillClient::Get().SendToggle(g_lookModes[li], true, 0.0f, pkg);
                    }

                    if (s_flyUpNew) AimkillClient::Get().SendToggle(AimkillMode::FlyUpNew, true, 0.0f, pkg);
                    if (s_medikitRun) AimkillClient::Get().SendToggle(AimkillMode::MedikitRun, true, 0.0f, pkg);
                    if (s_wallHack) AimkillClient::Get().SendToggle(AimkillMode::WallHack, true, 0.0f, pkg);
                    if (s_cameraUp) AimkillClient::Get().SendToggle(AimkillMode::CameraUp, true, 0.0f, pkg);
                    if (s_autoTarget) AimkillClient::Get().SendToggle(AimkillMode::AutoTarget, true, 0.0f, pkg);
                    if (s_blackSky) AimkillClient::Get().SendToggle(AimkillMode::BlackSky, true, 0.0f, pkg);
                    if (s_teleMark) AimkillClient::Get().SendToggle(AimkillMode::TeleMark, true, 0.0f, pkg);
                    if (s_stopSpt) AimkillClient::Get().SendToggle(AimkillMode::StopSpt, true, 0.0f, pkg);
                    if (s_flyHackNew) {
                        AimkillClient::Get().SendToggle(AimkillMode::FlyHackNew, true, (float)s_flyHackHeight, pkg);
                        AimkillClient::Get().SendToggle(AimkillMode::FlyHackHeight, true, (float)s_flyHackHeight, pkg);
                    }
                    if (s_invisible) AimkillClient::Get().SendToggle(AimkillMode::Invisible, true, 0.0f, pkg);
                    if (s_invisibleKill) AimkillClient::Get().SendToggle(AimkillMode::InvisibleKill, true, 0.0f, pkg);

                    Beep(800, 45);
                    if (notify) {
                        notify->add_notify("Server Connected Successfully!", 3, static_cast<notify_position>(var->c_notify.notify_position));
                    }
                } else {
                    isServerConnected = false;
                    isServerConnecting = false;
                    server_btn = "Socket Connect Failed";
                    Beep(500, 45);
                    if (notify) {
                        notify->add_notify("Socket Failed (Port 21405)", 4, static_cast<notify_position>(var->c_notify.notify_position));
                    }
                }
            } catch (...) {
                server_btn = "Inject Error";
                isServerConnecting = false;
                isServerConnected = false;
            }
        }).detach();
    }

    void DisconnectServer()
    {
        if (!isServerConnected) return;

        AimkillClient::Get().Disconnect();
        isServerConnected = false;
        server_btn = "Connect Server";
        Beep(500, 45);
        if (notify) {
            notify->add_notify("Server Disconnected", 3, static_cast<notify_position>(var->c_notify.notify_position));
        }
    }

    void PollHotkeys()
    {
        if (s_downAimkill_key != 0 && (GetAsyncKeyState(s_downAimkill_key) & 1)) {
            s_downAimkill = !s_downAimkill;
            if (isServerConnected) AimkillClient::Get().SendToggle(AimkillMode::DownAimkill, s_downAimkill, 0.0f, pkgNames[selectedPkg]);
            Beep(s_downAimkill ? 800 : 500, 45);
            if (notify) notify->add_notify(s_downAimkill ? "Down Aimkill On" : "Down Aimkill Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
        }

        if (s_flyUpNew_key != 0 && (GetAsyncKeyState(s_flyUpNew_key) & 1)) {
            s_flyUpNew = !s_flyUpNew;
            if (isServerConnected) AimkillClient::Get().SendToggle(AimkillMode::FlyUpNew, s_flyUpNew, 0.0f, pkgNames[selectedPkg]);
            Beep(s_flyUpNew ? 800 : 500, 45);
            if (notify) notify->add_notify(s_flyUpNew ? "Fly Up On" : "Fly Up Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
        }

        if (s_teleMark_key != 0 && (GetAsyncKeyState(s_teleMark_key) & 1)) {
            s_teleMark = !s_teleMark;
            if (isServerConnected) AimkillClient::Get().SendToggle(AimkillMode::TeleMark, s_teleMark, 0.0f, pkgNames[selectedPkg]);
            Beep(s_teleMark ? 800 : 500, 45);
            if (notify) notify->add_notify(s_teleMark ? "Tele Mark On" : "Tele Mark Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
        }

        if (s_stopSpt_key != 0 && (GetAsyncKeyState(s_stopSpt_key) & 1)) {
            s_stopSpt = !s_stopSpt;
            if (isServerConnected) AimkillClient::Get().SendToggle(AimkillMode::StopSpt, s_stopSpt, 0.0f, pkgNames[selectedPkg]);
            Beep(s_stopSpt ? 800 : 500, 45);
            if (notify) notify->add_notify(s_stopSpt ? "Stop Spt On" : "Stop Spt Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
        }

        if (s_flyHackNew_key != 0 && (GetAsyncKeyState(s_flyHackNew_key) & 1)) {
            s_flyHackNew = !s_flyHackNew;
            if (isServerConnected) AimkillClient::Get().SendToggle(AimkillMode::FlyHackNew, s_flyHackNew, (float)s_flyHackHeight, pkgNames[selectedPkg]);
            Beep(s_flyHackNew ? 800 : 500, 45);
            if (notify) notify->add_notify(s_flyHackNew ? "Fly Hack On" : "Fly Hack Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
        }
    }

} // namespace AimkillState
