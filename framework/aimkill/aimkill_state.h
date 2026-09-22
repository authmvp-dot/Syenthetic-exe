#pragma once
#include <string>
#include <vector>

namespace AimkillState {

    // Server connection state
    extern bool isServerConnected;
    extern bool isServerConnecting;
    extern std::string server_btn;
    extern char deviceAddr[64];
    extern int selectedPkg;
    extern const char* pkgNames[2];

    // AIM Features
    extern bool s_enableAll;
    extern bool s_aimkill;
    extern bool s_autoSwitch;
    extern bool s_coverShot;
    extern bool s_shieldBypass;
    extern bool s_espGrenade;
    extern bool s_speedHackJoy;
    extern bool s_resetGuest;
    extern bool s_speedDash;
    extern bool s_downAimkill;
    extern int  s_downAimkill_key;

    // LOOK Features (Skin Changer)
    extern bool s_lookDreamspace;
    extern bool s_lookRampage;
    extern bool s_lookItachi;
    extern bool s_lookMidnightAce;
    extern bool s_lookAurora;
    extern bool s_lookNarutoAscent;
    extern bool s_lookLastParadox;
    extern bool s_lookFrostfire;
    extern bool s_lookScorpio;
    extern bool s_lookDevilTrigger;
    extern bool s_lookCannibalHavoc;

    // BRUTAL Features
    extern bool s_flyUpNew;
    extern int  s_flyUpNew_key;
    extern bool s_medikitRun;
    extern bool s_wallHack;
    extern bool s_cameraUp;
    extern bool s_autoTarget;
    extern bool s_blackSky;
    extern bool s_teleMark;
    extern int  s_teleMark_key;
    extern bool s_stopSpt;
    extern int  s_stopSpt_key;
    extern bool s_flyHackNew;
    extern int  s_flyHackNew_key;
    extern int  s_flyHackHeight;
    extern bool s_invisible;
    extern bool s_invisibleKill;

    // Helpers
    void ApplyLookToggle(bool* selected, int mode, const char* name);
    void ConnectServerAsync();
    void DisconnectServer();
    void PollHotkeys();

} // namespace AimkillState
