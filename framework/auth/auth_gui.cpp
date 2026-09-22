#include "auth_gui.h"
#include "syzora_auth.hpp"
#include "../settings/settings.h"
#include "../settings/functions.h"

#include <shellapi.h>

extern HWND g_hwnd;

namespace AuthGui {

    static int s_ActiveTab = 0; // 0 = License Key, 1 = Sign In, 2 = Sign Up
    static char s_LicenseKeyBuf[128] = "";
    static char s_UsernameBuf[64] = "";
    static char s_PasswordBuf[64] = "";
    static char s_RegLicenseBuf[128] = "";
    static bool s_RememberMe = true;
    static std::string s_StatusMessage = "Ready";
    static ImU32 s_StatusColor = IM_COL32(160, 165, 180, 255);
    static bool s_Initialized = false;

    void Init() {
        if (s_Initialized) return;
        s_Initialized = true;

        if (!SyzoraAuth::g_Auth.saved_license_key.empty()) {
            strncpy_s(s_LicenseKeyBuf, SyzoraAuth::g_Auth.saved_license_key.c_str(), sizeof(s_LicenseKeyBuf) - 1);
        }
        if (!SyzoraAuth::g_Auth.saved_username.empty()) {
            strncpy_s(s_UsernameBuf, SyzoraAuth::g_Auth.saved_username.c_str(), sizeof(s_UsernameBuf) - 1);
        }
        s_RememberMe = SyzoraAuth::g_Auth.remember_credentials;

        // Async Init with Syzora Auth server
        std::thread([]() {
            SyzoraAuth::g_Auth.init();
        }).detach();
    }

    bool IsAuthenticated() {
        return SyzoraAuth::g_Auth.is_logged_in.load();
    }

    static void DoLicenseLogin() {
        if (SyzoraAuth::g_Auth.is_busy.load()) return;
        SyzoraAuth::g_Auth.is_busy = true;
        s_StatusMessage = "Authenticating license key...";
        s_StatusColor = IM_COL32(235, 190, 75, 255);

        std::string key = s_LicenseKeyBuf;
        SyzoraAuth::g_Auth.remember_credentials = s_RememberMe;

        std::thread([key]() {
            bool ok = SyzoraAuth::g_Auth.license(key);
            if (ok) {
                s_StatusMessage = "Access Granted! Welcome " + SyzoraAuth::g_Auth.user_data.username;
                s_StatusColor = IM_COL32(46, 213, 115, 255);
            } else {
                s_StatusMessage = SyzoraAuth::g_Auth.response.message.empty() ? "Invalid or expired key!" : SyzoraAuth::g_Auth.response.message;
                s_StatusColor = IM_COL32(235, 75, 75, 255);
            }
            SyzoraAuth::g_Auth.is_busy = false;
        }).detach();
    }

    static void DoUserLogin() {
        if (SyzoraAuth::g_Auth.is_busy.load()) return;
        SyzoraAuth::g_Auth.is_busy = true;
        s_StatusMessage = "Signing in...";
        s_StatusColor = IM_COL32(235, 190, 75, 255);

        std::string u = s_UsernameBuf;
        std::string p = s_PasswordBuf;
        SyzoraAuth::g_Auth.remember_credentials = s_RememberMe;

        std::thread([u, p]() {
            bool ok = SyzoraAuth::g_Auth.login(u, p);
            if (ok) {
                s_StatusMessage = "Access Granted! Welcome " + SyzoraAuth::g_Auth.user_data.username;
                s_StatusColor = IM_COL32(46, 213, 115, 255);
            } else {
                s_StatusMessage = SyzoraAuth::g_Auth.response.message.empty() ? "Invalid credentials!" : SyzoraAuth::g_Auth.response.message;
                s_StatusColor = IM_COL32(235, 75, 75, 255);
            }
            SyzoraAuth::g_Auth.is_busy = false;
        }).detach();
    }

    static void DoUserRegister() {
        if (SyzoraAuth::g_Auth.is_busy.load()) return;
        SyzoraAuth::g_Auth.is_busy = true;
        s_StatusMessage = "Creating account & activating key...";
        s_StatusColor = IM_COL32(235, 190, 75, 255);

        std::string u = s_UsernameBuf;
        std::string p = s_PasswordBuf;
        std::string k = s_RegLicenseBuf;
        SyzoraAuth::g_Auth.remember_credentials = s_RememberMe;

        std::thread([u, p, k]() {
            bool ok = SyzoraAuth::g_Auth.regstr(u, p, k);
            if (ok) {
                s_StatusMessage = "Account registered! Welcome " + SyzoraAuth::g_Auth.user_data.username;
                s_StatusColor = IM_COL32(46, 213, 115, 255);
            } else {
                s_StatusMessage = SyzoraAuth::g_Auth.response.message.empty() ? "Registration failed!" : SyzoraAuth::g_Auth.response.message;
                s_StatusColor = IM_COL32(235, 75, 75, 255);
            }
            SyzoraAuth::g_Auth.is_busy = false;
        }).detach();
    }

    static bool CustomInputText(const char* label, const char* hint, char* buf, size_t buf_size, ImGuiInputTextFlags flags = 0) {
        bool res = ImGui::InputText(label, buf, buf_size, flags);
        if (hint && buf[0] == '\0' && !ImGui::IsItemActive()) {
            ImVec2 p_min = ImGui::GetItemRectMin();
            ImVec2 p_max = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddText(ImVec2(p_min.x + SCALE(10.f), p_min.y + (p_max.y - p_min.y - ImGui::GetFontSize()) * 0.5f), IM_COL32(130, 135, 150, 180), hint);
        }
        return res;
    }

    void Render() {
        Init();

        const ImVec2 card_size = ImVec2(SCALE(410.f), SCALE(500.f));
        ImGui::SetNextWindowSize(card_size);
        ImGui::SetNextWindowPos(ImVec2(0, 0));

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(SCALE(20.f), SCALE(18.f)));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, SCALE(12.f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.2f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(13, 13, 18, 255));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(32, 30, 44, 255));

        if (ImGui::Begin("##SyzoraAuthWindow", nullptr, flags)) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 pos = ImGui::GetWindowPos();
            ImVec2 size = ImGui::GetWindowSize();

            // Native Windows Window Dragging on Header
            if (ImGui::IsMouseClicked(0)) {
                ImVec2 m = ImGui::GetMousePos();
                if (m.x >= pos.x && m.x <= pos.x + size.x && m.y >= pos.y && m.y <= pos.y + SCALE(60.f)) {
                    if (g_hwnd) {
                        ReleaseCapture();
                        SendMessage(g_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                        ImGuiIO& io = ImGui::GetIO();
                        io.ClearInputKeys();
                        io.MouseDown[0] = false;
                        io.MouseClicked[0] = false;
                    }
                }
            }

            // Top Header Glowing Accent Bar
            dl->AddRectFilledMultiColor(
                ImVec2(pos.x + SCALE(12.f), pos.y),
                ImVec2(pos.x + size.x - SCALE(12.f), pos.y + SCALE(3.f)),
                IM_COL32(142, 132, 255, 255),
                IM_COL32(185, 125, 255, 255),
                IM_COL32(185, 125, 255, 255),
                IM_COL32(142, 132, 255, 255)
            );

            // App Brand & Title
            ImFont* title_font = set->c_font.name ? set->c_font.name : set->c_font.inter_medium[1];
            if (title_font) ImGui::PushFont(title_font);
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "MvpAimExe");
            if (title_font) ImGui::PopFont();

            ImGui::SameLine();
            // Version badge
            ImVec2 cur = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(cur.x, cur.y + SCALE(2.f)));
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(142, 132, 255, 60));
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(185, 175, 255, 255));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, SCALE(4.f));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(SCALE(6.f), SCALE(2.f)));
            ImGui::SmallButton("v1.0");
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);

            // Server connection pulse dot on the right
            float dot_x = pos.x + size.x - SCALE(35.f);
            float dot_y = pos.y + SCALE(25.f);
            bool is_online = SyzoraAuth::g_Auth.is_initialized.load();
            float pulse = (sinf((float)ImGui::GetTime() * 4.0f) + 1.0f) * 0.5f;
            ImU32 dot_col = is_online ? IM_COL32(46, 213, 115, 255) : IM_COL32(240, 180, 50, (int)(150 + 105 * pulse));
            dl->AddCircleFilled(ImVec2(dot_x, dot_y), SCALE(4.5f), dot_col);

            // Close button [ X ]
            float close_x = pos.x + size.x - SCALE(22.f);
            float close_y = pos.y + SCALE(16.f);
            ImGui::SetCursorScreenPos(ImVec2(close_x - SCALE(6.f), close_y - SCALE(2.f)));
            if (ImGui::InvisibleButton("##CloseBtn", ImVec2(SCALE(16.f), SCALE(16.f)))) {
                PostQuitMessage(0);
            }
            bool close_hover = ImGui::IsItemHovered();
            dl->AddText(ImVec2(close_x - SCALE(2.f), close_y), close_hover ? IM_COL32(255, 80, 80, 255) : IM_COL32(140, 140, 150, 255), "X");

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - SCALE(2.f));
            ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.68f, 1.0f), "SECURE LOADER // AUTHORIZATION");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // -------------------------------------------------------------
            // Nav Tabs (License Login / Sign In / Sign Up)
            // -------------------------------------------------------------
            const char* tabs[] = { "License Key", "Sign In", "Sign Up" };
            float tab_avail = ImGui::GetContentRegionAvail().x;
            float single_w = (tab_avail - SCALE(10.f)) / 3.0f;
            float tab_h = SCALE(28.f);

            for (int i = 0; i < 3; ++i) {
                if (i > 0) ImGui::SameLine(0, SCALE(5.f));
                bool selected = (s_ActiveTab == i);

                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, SCALE(6.f));
                if (selected) {
                    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(142, 132, 255, 230));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(142, 132, 255, 255));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(120, 110, 240, 255));
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(22, 22, 30, 220));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(32, 32, 44, 255));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(40, 40, 56, 255));
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(160, 165, 180, 255));
                }

                if (ImGui::Button(tabs[i], ImVec2(single_w, tab_h))) {
                    s_ActiveTab = i;
                }
                ImGui::PopStyleColor(4);
                ImGui::PopStyleVar();
            }

            ImGui::Spacing();
            ImGui::Spacing();

            // Input Fields Style
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(SCALE(10.f), SCALE(8.f)));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, SCALE(6.f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(20, 20, 28, 255));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(28, 28, 40, 255));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(32, 32, 46, 255));
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(245, 245, 255, 255));

            float btn_w = ImGui::GetContentRegionAvail().x;
            float action_btn_h = SCALE(36.f);

            // =============================================================
            // TAB 0: License Key Only
            // =============================================================
            if (s_ActiveTab == 0) {
                ImGui::TextColored(ImVec4(0.7f, 0.72f, 0.82f, 1.0f), "ENTER LICENSE KEY");
                ImGui::SetNextItemWidth(-1);
                CustomInputText("##LicKeyInput", "XXXX-XXXX-XXXX-XXXX", s_LicenseKeyBuf, IM_ARRAYSIZE(s_LicenseKeyBuf));

                ImGui::Spacing();
                ImGui::Checkbox("Save Key on this device", &s_RememberMe);

                ImGui::Spacing();
                ImGui::Spacing();

                bool is_busy = SyzoraAuth::g_Auth.is_busy.load();
                ImGui::PushStyleColor(ImGuiCol_Button, is_busy ? IM_COL32(70, 70, 95, 255) : IM_COL32(142, 132, 255, 255));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(160, 150, 255, 255));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(120, 110, 240, 255));
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));

                if (ImGui::Button(is_busy ? "CONNECTING..." : "ACTIVATE & LOGIN", ImVec2(btn_w, action_btn_h)) && !is_busy) {
                    DoLicenseLogin();
                }
                ImGui::PopStyleColor(4);
            }
            // =============================================================
            // TAB 1: User Sign In
            // =============================================================
            else if (s_ActiveTab == 1) {
                ImGui::TextColored(ImVec4(0.7f, 0.72f, 0.82f, 1.0f), "USERNAME");
                ImGui::SetNextItemWidth(-1);
                CustomInputText("##UserLoginName", "Enter username", s_UsernameBuf, IM_ARRAYSIZE(s_UsernameBuf));

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.7f, 0.72f, 0.82f, 1.0f), "PASSWORD");
                ImGui::SetNextItemWidth(-1);
                CustomInputText("##UserLoginPass", "Enter password", s_PasswordBuf, IM_ARRAYSIZE(s_PasswordBuf), ImGuiInputTextFlags_Password);

                ImGui::Spacing();
                ImGui::Checkbox("Remember Me", &s_RememberMe);

                ImGui::Spacing();
                ImGui::Spacing();

                bool is_busy = SyzoraAuth::g_Auth.is_busy.load();
                ImGui::PushStyleColor(ImGuiCol_Button, is_busy ? IM_COL32(70, 70, 95, 255) : IM_COL32(142, 132, 255, 255));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(160, 150, 255, 255));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(120, 110, 240, 255));
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));

                if (ImGui::Button(is_busy ? "SIGNING IN..." : "SIGN IN", ImVec2(btn_w, action_btn_h)) && !is_busy) {
                    DoUserLogin();
                }
                ImGui::PopStyleColor(4);
            }
            // =============================================================
            // TAB 2: User Sign Up
            // =============================================================
            else if (s_ActiveTab == 2) {
                ImGui::TextColored(ImVec4(0.7f, 0.72f, 0.82f, 1.0f), "DESIRED USERNAME");
                ImGui::SetNextItemWidth(-1);
                CustomInputText("##RegUser", "Choose username", s_UsernameBuf, IM_ARRAYSIZE(s_UsernameBuf));

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.7f, 0.72f, 0.82f, 1.0f), "DESIRED PASSWORD");
                ImGui::SetNextItemWidth(-1);
                CustomInputText("##RegPass", "Choose password", s_PasswordBuf, IM_ARRAYSIZE(s_PasswordBuf), ImGuiInputTextFlags_Password);

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.7f, 0.72f, 0.82f, 1.0f), "LICENSE ACTIVATION KEY");
                ImGui::SetNextItemWidth(-1);
                CustomInputText("##RegKey", "Enter license key", s_RegLicenseBuf, IM_ARRAYSIZE(s_RegLicenseBuf));

                ImGui::Spacing();
                ImGui::Spacing();

                bool is_busy = SyzoraAuth::g_Auth.is_busy.load();
                ImGui::PushStyleColor(ImGuiCol_Button, is_busy ? IM_COL32(70, 70, 95, 255) : IM_COL32(142, 132, 255, 255));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(160, 150, 255, 255));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(120, 110, 240, 255));
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));

                if (ImGui::Button(is_busy ? "CREATING ACCOUNT..." : "REGISTER & ACTIVATE", ImVec2(btn_w, action_btn_h)) && !is_busy) {
                    DoUserRegister();
                }
                ImGui::PopStyleColor(4);
            }

            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar(2);

            // -------------------------------------------------------------
            // Status Feedback Card
            // -------------------------------------------------------------
            ImGui::Spacing();
            ImGui::Spacing();

            ImVec2 status_p0 = ImGui::GetCursorScreenPos();
            ImVec2 status_p1 = ImVec2(status_p0.x + btn_w, status_p0.y + SCALE(34.f));
            dl->AddRectFilled(status_p0, status_p1, IM_COL32(18, 18, 24, 230), SCALE(6.f));
            dl->AddRect(status_p0, status_p1, s_StatusColor, SCALE(6.f), 0, 1.0f);

            std::string display_status = s_StatusMessage;
            if (SyzoraAuth::g_Auth.is_busy.load()) {
                int dots = ((int)(ImGui::GetTime() * 3.0f)) % 4;
                display_status = "Connecting";
                for (int d = 0; d < dots; ++d) display_status += ".";
            }

            ImVec2 text_sz = ImGui::CalcTextSize(display_status.c_str());
            ImVec2 text_pos(
                status_p0.x + (btn_w - text_sz.x) * 0.5f,
                status_p0.y + (SCALE(34.f) - text_sz.y) * 0.5f
            );
            dl->AddText(text_pos, s_StatusColor, display_status.c_str());

            ImGui::Dummy(ImVec2(btn_w, SCALE(36.f)));

            // -------------------------------------------------------------
            // Footer Community Link
            // -------------------------------------------------------------
            ImGui::Spacing();
            const char* help_txt = "Need a license key? Join our Discord";
            ImVec2 hsz = ImGui::CalcTextSize(help_txt);
            ImGui::SetCursorPosX((size.x - hsz.x) * 0.5f);

            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(142, 132, 255, 230));
            if (ImGui::SmallButton(help_txt)) {
                ShellExecuteA(NULL, "open", "https://discord.gg/BYFfkt78BC", NULL, NULL, SW_SHOWNORMAL);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Open community link in browser");
            }
            ImGui::PopStyleColor();
        }
        ImGui::End();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
    }
}
