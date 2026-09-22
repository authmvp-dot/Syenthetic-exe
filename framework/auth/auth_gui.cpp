#include "auth_gui.h"
#include "syzora_auth.hpp"
#include "../settings/functions.h"

#include <shellapi.h>
#include <thread>
#include <string>

using namespace ImGui;
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
            try {
                SyzoraAuth::g_Auth.init();
            } catch (...) {}
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
            try {
                bool ok = SyzoraAuth::g_Auth.license(key);
                if (ok) {
                    s_StatusMessage = "Access Granted! Welcome " + SyzoraAuth::g_Auth.user_data.username;
                    s_StatusColor = IM_COL32(46, 213, 115, 255);
                    Beep(800, 45);
                } else {
                    s_StatusMessage = SyzoraAuth::g_Auth.response.message.empty() ? "Invalid or expired key!" : SyzoraAuth::g_Auth.response.message;
                    s_StatusColor = IM_COL32(235, 75, 75, 255);
                    Beep(500, 45);
                }
            } catch (...) {
                s_StatusMessage = "Authentication error occurred!";
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
            try {
                bool ok = SyzoraAuth::g_Auth.login(u, p);
                if (ok) {
                    s_StatusMessage = "Access Granted! Welcome " + SyzoraAuth::g_Auth.user_data.username;
                    s_StatusColor = IM_COL32(46, 213, 115, 255);
                    Beep(800, 45);
                } else {
                    s_StatusMessage = SyzoraAuth::g_Auth.response.message.empty() ? "Invalid credentials!" : SyzoraAuth::g_Auth.response.message;
                    s_StatusColor = IM_COL32(235, 75, 75, 255);
                    Beep(500, 45);
                }
            } catch (...) {
                s_StatusMessage = "Login error occurred!";
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
            try {
                bool ok = SyzoraAuth::g_Auth.regstr(u, p, k);
                if (ok) {
                    s_StatusMessage = "Account registered! Welcome " + SyzoraAuth::g_Auth.user_data.username;
                    s_StatusColor = IM_COL32(46, 213, 115, 255);
                    Beep(800, 45);
                } else {
                    s_StatusMessage = SyzoraAuth::g_Auth.response.message.empty() ? "Registration failed!" : SyzoraAuth::g_Auth.response.message;
                    s_StatusColor = IM_COL32(235, 75, 75, 255);
                    Beep(500, 45);
                }
            } catch (...) {
                s_StatusMessage = "Registration error occurred!";
                s_StatusColor = IM_COL32(235, 75, 75, 255);
            }
            SyzoraAuth::g_Auth.is_busy = false;
        }).detach();
    }

    void Render() {
        Init();

        // Exact same window dimensions as cheat menu (860 x 630)
        set->c_window.window_size = ImVec2(860.f, 630.f);
        ImVec2 menu_size = SCALE(set->c_window.window_size);
        gui->set_next_window_pos(ImVec2(0, 0));
        gui->set_next_window_size(menu_size);

        gui->begin({ "NAME" }, { 0 }, set->c_window.window_flags | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
        {
            const ImVec2 pos = GetWindowPos();
            const ImVec2 size = GetWindowSize();

            float start_tab_y = pos.y + SCALE(78);
            float tab_h = SCALE(50);
            float tab_spacing = SCALE(10);
            float tab_w = SCALE(76);
            float tab_x = pos.x + (SCALE(110) - tab_w) * 0.5f;

            // Native window dragging from ANY empty header space or sidebar
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || 
               (ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsAnyItemActive() && ImGui::GetIO().MouseDownDuration[0] < 0.15f))
            {
                ImVec2 mouse = ImGui::GetMousePos();
                bool in_header = (mouse.y >= pos.y && mouse.y <= pos.y + SCALE(75.0f) && mouse.x >= pos.x && mouse.x <= pos.x + size.x);
                bool in_sidebar = (mouse.x >= pos.x && mouse.x <= pos.x + SCALE(110.0f) && mouse.y >= pos.y && mouse.y <= pos.y + size.y);

                if (in_header || in_sidebar)
                {
                    bool on_tab = false;
                    for (int t = 0; t < 3; ++t)
                    {
                        float ty = start_tab_y + t * (tab_h + tab_spacing);
                        if (mouse.x >= tab_x && mouse.x <= tab_x + tab_w &&
                            mouse.y >= ty && mouse.y <= ty + tab_h)
                        {
                            on_tab = true;
                            break;
                        }
                    }

                    if (!on_tab && g_hwnd)
                    {
                        ReleaseCapture();
                        SendMessage(g_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);

                        ImGuiIO& io = ImGui::GetIO();
                        io.ClearInputKeys();
                        io.MouseDown[0] = false;
                        io.MouseClicked[0] = false;
                        io.MouseDoubleClicked[0] = false;
                        io.MouseDownDuration[0] = -1.0f;
                        io.MouseDownDurationPrev[0] = -1.0f;
                    }
                }
            }

            ImDrawList* draw_list = GetWindowDrawList();
            ImGuiStyle* style = &GetStyle();

            {
                style->WindowBorderSize = SCALE(set->c_window.border_size);
                style->WindowRounding = SCALE(set->c_window.rounding);
                style->WindowPadding = SCALE(set->c_window.padding);
                style->ScrollbarSize = SCALE(set->c_window.scrollbar_size);
                style->ItemSpacing = SCALE(set->c_window.item_spacing);
            }

            // 1. Outer window background and rounded stroke
            draw->add_rect_filled(draw_list, { pos.x, pos.y }, { pos.x + size.x, pos.y + size.y }, gui->get_clr(clr->c_window.general_layout), SCALE(set->c_window.general_rounding));
            draw->add_rect(draw_list, { pos.x, pos.y }, { pos.x + size.x, pos.y + size.y }, gui->get_clr(clr->c_window.general_stroke), SCALE(set->c_window.general_rounding));

            float content_top = pos.y + SCALE(75.0f);
            float content_bottom = pos.y + (size.y - SCALE(15.0f));
            float content_left = pos.x + SCALE(110.0f);
            float content_right = pos.x + (size.x - SCALE(15.0f));

            // 2. Inner content area container
            draw->add_rect_filled(draw_list, { content_left, content_top }, { content_right, content_bottom }, gui->get_clr(clr->c_window.layout), SCALE(set->c_window.rounding));
            draw->add_rect(draw_list, { content_left, content_top }, { content_right, content_bottom }, gui->get_clr(clr->c_window.stroke), SCALE(set->c_window.rounding));

            draw->rect_filled_multi_color(draw_list, { content_left + (content_right - content_left) * 0.5f, content_top }, { content_right, content_top + 1 }, gui->get_clr(clr->c_other_clr.accent_clr, 0.2f), gui->get_clr(clr->c_other_clr.accent_clr, 0.f), gui->get_clr(clr->c_other_clr.accent_clr, 0.f), gui->get_clr(clr->c_other_clr.accent_clr, 0.2f));
            draw->rect_filled_multi_color(draw_list, { content_left, content_top }, { content_left + (content_right - content_left) * 0.5f, content_top + 1 }, gui->get_clr(clr->c_other_clr.accent_clr, 0.f), gui->get_clr(clr->c_other_clr.accent_clr, 0.2f), gui->get_clr(clr->c_other_clr.accent_clr, 0.2f), gui->get_clr(clr->c_other_clr.accent_clr, 0.f));

            draw->rect_filled_multi_color(draw_list, { pos.x + size.x / 2, pos.y + size.y - 1 }, { pos.x + size.x, pos.y + size.y }, gui->get_clr(clr->c_other_clr.accent_clr, 0.2f), gui->get_clr(clr->c_other_clr.accent_clr, 0.f), gui->get_clr(clr->c_other_clr.accent_clr, 0.f), gui->get_clr(clr->c_other_clr.accent_clr, 0.2f));
            draw->rect_filled_multi_color(draw_list, { pos.x, pos.y + size.y - 1 }, { pos.x + size.x / 2, pos.y + size.y }, gui->get_clr(clr->c_other_clr.accent_clr, 0.f), gui->get_clr(clr->c_other_clr.accent_clr, 0.2f), gui->get_clr(clr->c_other_clr.accent_clr, 0.2f), gui->get_clr(clr->c_other_clr.accent_clr, 0.f));

            // 3. Top Sidebar Logo
            draw->render_text(draw_list, set->c_font.icon[2], { pos.x, pos.y + SCALE(10) }, { pos.x + SCALE(110), pos.y + SCALE(62) }, gui->get_clr(clr->c_other_clr.accent_clr), "B", 0, 0, { 0.5f, 0.5f });
            draw->rect_filled_multi_color(draw_list, { pos.x + SCALE(20), pos.y + SCALE(68) }, { pos.x + SCALE(90), pos.y + SCALE(70) },
                gui->get_clr(clr->c_other_clr.accent_clr, 0.f), gui->get_clr(clr->c_other_clr.accent_clr, 0.5f),
                gui->get_clr(clr->c_other_clr.accent_clr, 0.5f), gui->get_clr(clr->c_other_clr.accent_clr, 0.f));

            // 4. Header Animated 3D Wave Title: "Mvp Cheats Authentication"
            {
                float title_center_x = (content_left + content_right) * 0.5f;
                float title_center_y = pos.y + SCALE(36.0f);

                float time = (float)ImGui::GetTime();
                ImVec4 acc = clr->c_other_clr.accent_clr;

                const char* full_title = "Mvp Cheats Authentication";
                const int total_chars = 25;
                const int split_idx = 11; // Index where "Authentication" begins

                float total_w = set->c_font.name->CalcTextSizeA(set->c_font.name->FontSize, FLT_MAX, -1, full_title).x;
                float start_x = title_center_x - total_w * 0.5f;
                float font_h = set->c_font.name->FontSize;
                float base_y = title_center_y - font_h * 0.5f;

                // Rotating Gyro Diamond Emblems
                auto draw_rotating_emblem = [&](ImVec2 center, float angle, ImVec4 emblem_col)
                {
                    float r = SCALE(6.0f);
                    float cos_a = cosf(angle);
                    float sin_a = sinf(angle);

                    ImVec2 p[4] = {
                        { center.x + r * cos_a, center.y + r * sin_a },
                        { center.x - r * sin_a, center.y + r * cos_a },
                        { center.x - r * cos_a, center.y - r * sin_a },
                        { center.x + r * sin_a, center.y - r * cos_a }
                    };

                    for (int k = 0; k < 4; ++k)
                        draw_list->AddLine(p[k], p[(k + 1) % 4], gui->get_clr(emblem_col, 0.85f), 1.4f);

                    float r_in = r * 0.5f;
                    draw_list->AddLine({ center.x - r_in * cos_a, center.y - r_in * sin_a },
                                       { center.x + r_in * cos_a, center.y + r_in * sin_a },
                                       gui->get_clr(clr->c_other_clr.white_clr, 0.9f), 1.2f);

                    draw_list->AddCircleFilled(center, SCALE(1.4f), gui->get_clr(emblem_col, 1.0f));
                };

                float rot_speed = time * 2.2f;
                draw_rotating_emblem({ start_x - SCALE(20.0f), title_center_y }, rot_speed, acc);
                draw_rotating_emblem({ start_x + total_w + SCALE(20.0f), title_center_y }, -rot_speed, acc);

                // Letter-by-letter 3D revolving wave
                float cur_x = start_x;
                for (int i = 0; i < total_chars; ++i)
                {
                    char ch_str[2] = { full_title[i], '\0' };
                    float ch_w = set->c_font.name->CalcTextSizeA(font_h, FLT_MAX, -1, ch_str).x;

                    if (full_title[i] == ' ')
                    {
                        cur_x += ch_w;
                        continue;
                    }

                    float angle = time * 3.2f - (float)i * 0.28f;
                    float rot_y = sinf(angle) * SCALE(3.5f);
                    float depth = cosf(angle);

                    ImVec4 char_col;
                    if (i < split_idx)
                    {
                        float bright = 0.78f + (depth * 0.5f + 0.5f) * 0.22f;
                        char_col = ImVec4(bright * 0.92f, bright * 0.94f, bright, 1.0f);
                    }
                    else
                    {
                        float bright = 0.75f + (depth * 0.5f + 0.5f) * 0.35f;
                        char_col = ImVec4(ImMin(acc.x * bright, 1.0f), ImMin(acc.y * bright, 1.0f), ImMin(acc.z * bright, 1.0f), 1.0f);
                    }

                    draw_list->AddText(set->c_font.name, font_h, { cur_x, base_y + rot_y }, gui->get_clr(char_col), ch_str);
                    cur_x += ch_w;
                }

                // Laser Light Sweep Line
                float beam_w = (total_w + SCALE(40.0f));
                float beam_y = base_y + font_h + SCALE(7.0f);
                float half_beam = beam_w * 0.5f;

                draw->rect_filled_multi_color(draw_list,
                    { title_center_x - half_beam, beam_y },
                    { title_center_x, beam_y + SCALE(1.0f) },
                    gui->get_clr(acc, 0.0f), gui->get_clr(acc, 0.35f),
                    gui->get_clr(acc, 0.35f), gui->get_clr(acc, 0.0f));

                draw->rect_filled_multi_color(draw_list,
                    { title_center_x, beam_y },
                    { title_center_x + half_beam, beam_y + SCALE(1.0f) },
                    gui->get_clr(acc, 0.35f), gui->get_clr(acc, 0.0f),
                    gui->get_clr(acc, 0.0f), gui->get_clr(acc, 0.35f));

                float sweep_phase = fmodf(time * 0.65f, 1.0f);
                float spark_x = (title_center_x - half_beam) + sweep_phase * (beam_w);
                draw->add_rect_filled(draw_list,
                    { spark_x - SCALE(10.0f), beam_y - SCALE(0.5f) },
                    { spark_x + SCALE(10.0f), beam_y + SCALE(1.5f) },
                    gui->get_clr(acc, 0.70f), SCALE(1.0f));
                draw->add_rect_filled(draw_list,
                    { spark_x - SCALE(3.0f), beam_y - SCALE(1.0f) },
                    { spark_x + SCALE(3.0f), beam_y + SCALE(2.0f) },
                    gui->get_clr(clr->c_other_clr.white_clr, 0.95f), SCALE(1.0f));
            }

            // Top Header Close Button [X]
            {
                float close_sz = SCALE(26.f);
                ImVec2 close_pos = ImVec2(pos.x + size.x - SCALE(38.f), pos.y + SCALE(16.f));
                ImRect close_rect(close_pos, ImVec2(close_pos.x + close_sz, close_pos.y + close_sz));
                ImGuiID close_id = ImGui::GetID("##AuthHeaderClose");
                ItemSize(close_rect, 0);
                if (ItemAdd(close_rect, close_id))
                {
                    bool chover = false, cheld = false;
                    if (ButtonBehavior(close_rect, close_id, &chover, &cheld))
                    {
                        var->c_panel.request_exit = true;
                        PostQuitMessage(0);
                    }
                    draw->add_rect_filled(draw_list, close_rect.Min, close_rect.Max, chover ? IM_COL32(235, 65, 65, 220) : IM_COL32(26, 26, 36, 180), SCALE(5.f));
                    draw->add_rect(draw_list, close_rect.Min, close_rect.Max, chover ? IM_COL32(255, 90, 90, 255) : IM_COL32(45, 45, 65, 200), SCALE(5.f));
                    draw->render_text(set->c_font.inter_medium[0], draw_list, close_rect.Min, close_rect.Max, chover ? IM_COL32(255, 255, 255, 255) : IM_COL32(180, 180, 195, 220), "X", 0, 0, { 0.5f, 0.45f });
                }
            }

            // 5. Sidebar Auth Tabs (0 = License Key "K", 1 = Sign In "F", 2 = Sign Up "D")
            static const char* auth_tab_icons[3] = { "K", "F", "D" };
            for (int i = 0; i < 3; i++)
            {
                float cur_y = start_tab_y + i * (tab_h + tab_spacing);
                ImRect tab_rect(ImVec2(tab_x, cur_y), ImVec2(tab_x + tab_w, cur_y + tab_h));
                ImGuiID tab_id = ImGui::GetID(("##auth_sidebar_tab_" + std::to_string(i)).c_str());

                gui->set_cursor_pos(ImVec2(tab_x - pos.x, cur_y - pos.y));
                ItemSize(tab_rect, 0);
                if (!ItemAdd(tab_rect, tab_id))
                    continue;

                bool hovered = false, held = false;
                bool pressed = ImGui::ButtonBehavior(tab_rect, tab_id, &hovered, &held);
                if (pressed)
                {
                    s_ActiveTab = i;
                }

                struct s_auth_tab { float anim; float hover; };
                s_auth_tab* st = gui->anim_container(&st, tab_id);
                bool is_active = (s_ActiveTab == i);
                st->anim = ImLerp(st->anim, is_active ? 1.f : 0.f, gui->fixed_speed(14.f));
                st->hover = ImLerp(st->hover, hovered ? 1.f : 0.f, gui->fixed_speed(14.f));

                ImVec4 bg_col = ImLerp(clr->c_window.layout, clr->c_other_clr.accent_clr, st->anim * 0.22f);
                if (st->hover > 0.01f && !is_active)
                    bg_col = ImLerp(bg_col, clr->c_element.layout, st->hover * 0.5f);
                draw->add_rect_filled(draw_list, tab_rect.Min, tab_rect.Max, gui->get_clr(bg_col), SCALE(8.f));

                ImVec4 border_col = ImLerp(clr->c_window.stroke, clr->c_other_clr.accent_clr, st->anim * 0.85f);
                if (st->hover > 0.01f && !is_active)
                    border_col = ImLerp(border_col, clr->c_other_clr.accent_clr, st->hover * 0.4f);
                draw->add_rect(draw_list, tab_rect.Min, tab_rect.Max, gui->get_clr(border_col), SCALE(8.f));

                if (st->anim > 0.01f)
                {
                    float bar_pad = SCALE(14.f) * (1.f - st->anim) + SCALE(10.f);
                    draw->add_rect_filled(draw_list,
                        { tab_rect.Min.x + bar_pad, tab_rect.Max.y - SCALE(5.5f) },
                        { tab_rect.Max.x - bar_pad, tab_rect.Max.y - SCALE(2.5f) },
                        gui->get_clr(clr->c_other_clr.accent_clr, st->anim), SCALE(1.5f));
                }

                ImVec4 inactive_icon = ImLerp(ImVec4(0.62f, 0.62f, 0.74f, 1.0f), ImVec4(1.f, 1.f, 1.f, 1.f), st->hover);
                ImVec4 icon_col = is_active ? ImVec4(1.f, 1.f, 1.f, 1.f) : ImLerp(inactive_icon, ImVec4(1.f, 1.f, 1.f, 1.f), st->anim);
                draw->render_text(draw_list, set->c_font.icon[1], tab_rect.Min, tab_rect.Max - ImVec2(0, SCALE(2.f)), gui->get_clr(icon_col), auth_tab_icons[i], 0, 0, { 0.5f, 0.48f });
            }

            // 6. Sidebar Bottom Status Card
            float profile_y = pos.y + size.y - SCALE(65.f);
            ImRect prof_rect(ImVec2(tab_x, profile_y), ImVec2(tab_x + tab_w, profile_y + SCALE(50.f)));
            draw->add_rect_filled(draw_list, prof_rect.Min, prof_rect.Max, IM_COL32(18, 18, 26, 220), SCALE(6.f));
            draw->add_rect(draw_list, prof_rect.Min, prof_rect.Max, IM_COL32(32, 30, 44, 255), SCALE(6.f));
            draw->render_text(set->c_font.inter_medium[0], draw_list, prof_rect.Min + ImVec2(SCALE(4.f), SCALE(6.f)), prof_rect.Max, IM_COL32(240, 240, 255, 255), "Syzora", 0, 0, { 0.5f, 0.0f });

            bool busy = SyzoraAuth::g_Auth.is_busy.load();
            bool online = SyzoraAuth::g_Auth.is_initialized.load();
            const char* badge = busy ? "[BUSY]" : (online ? "[ONLINE]" : "[WAIT]");
            ImU32 badge_col = busy ? IM_COL32(255, 200, 50, 255) : (online ? IM_COL32(46, 213, 115, 255) : IM_COL32(235, 75, 75, 255));
            draw->render_text(set->c_font.inter_medium[0], draw_list, prof_rect.Min + ImVec2(SCALE(4.f), SCALE(26.f)), prof_rect.Max, badge_col, badge, 0, 0, { 0.5f, 0.0f });

            // 7. Main Content Area
            gui->set_cursor_pos(SCALE(115, 75));
            gui->begin_content("auth_content", ImVec2(size.x - SCALE(130), size.y - SCALE(90)), { 15, 15 }, { 15, 15 });
            {
                // Left Column: Credentials Input Card
                gui->begin_group();
                {
                    gui->begin_child("auth_form");
                    {
                        if (s_ActiveTab == 0) // License Key Only
                        {
                            ImGui::PushFont(set->c_font.inter_medium[1]);
                            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "License Key Authentication");
                            ImGui::PopFont();
                            ImGui::TextColored(ImVec4(0.6f, 0.63f, 0.72f, 1.0f), "Enter your single-key license to access Mvp Cheats.");

                            widget->separator();

                            widget->text_field("XXXX-XXXX-XXXX-XXXX", "License Key", s_LicenseKeyBuf, sizeof(s_LicenseKeyBuf), { GetContentRegionAvail().x, SCALE(36) });

                            widget->separator();

                            widget->checkbox("Save key on this device", &s_RememberMe);

                            widget->separator();

                            bool is_busy = SyzoraAuth::g_Auth.is_busy.load();
                            if (widget->button(is_busy ? "Connecting..." : "LOGIN WITH LICENSE KEY", { GetContentRegionAvail().x, SCALE(38) }))
                            {
                                if (!is_busy) DoLicenseLogin();
                            }
                        }
                        else if (s_ActiveTab == 1) // User Sign In
                        {
                            ImGui::PushFont(set->c_font.inter_medium[1]);
                            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "User Account Login");
                            ImGui::PopFont();
                            ImGui::TextColored(ImVec4(0.6f, 0.63f, 0.72f, 1.0f), "Sign in with your registered username and password.");

                            widget->separator();

                            widget->text_field("Enter username", "Username", s_UsernameBuf, sizeof(s_UsernameBuf), { GetContentRegionAvail().x, SCALE(36) });

                            widget->separator();

                            widget->text_field("Enter password", "Password", s_PasswordBuf, sizeof(s_PasswordBuf), { GetContentRegionAvail().x, SCALE(36) }, ImGuiInputTextFlags_Password);

                            widget->separator();

                            widget->checkbox("Remember credentials", &s_RememberMe);

                            widget->separator();

                            bool is_busy = SyzoraAuth::g_Auth.is_busy.load();
                            if (widget->button(is_busy ? "Signing in..." : "SIGN IN", { GetContentRegionAvail().x, SCALE(38) }))
                            {
                                if (!is_busy) DoUserLogin();
                            }
                        }
                        else if (s_ActiveTab == 2) // User Sign Up
                        {
                            ImGui::PushFont(set->c_font.inter_medium[1]);
                            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "Register Account");
                            ImGui::PopFont();
                            ImGui::TextColored(ImVec4(0.6f, 0.63f, 0.72f, 1.0f), "Create a new account using your license key.");

                            widget->separator();

                            widget->text_field("Choose username", "Username", s_UsernameBuf, sizeof(s_UsernameBuf), { GetContentRegionAvail().x, SCALE(36) });

                            widget->separator();

                            widget->text_field("Choose password", "Password", s_PasswordBuf, sizeof(s_PasswordBuf), { GetContentRegionAvail().x, SCALE(36) }, ImGuiInputTextFlags_Password);

                            widget->separator();

                            widget->text_field("License key", "License Key", s_RegLicenseBuf, sizeof(s_RegLicenseBuf), { GetContentRegionAvail().x, SCALE(36) });

                            widget->separator();

                            widget->checkbox("Remember credentials", &s_RememberMe);

                            widget->separator();

                            bool is_busy = SyzoraAuth::g_Auth.is_busy.load();
                            if (widget->button(is_busy ? "Registering..." : "REGISTER & ACTIVATE", { GetContentRegionAvail().x, SCALE(38) }))
                            {
                                if (!is_busy) DoUserRegister();
                            }
                        }
                    }
                    gui->end_child();
                }
                gui->end_group();

                gui->sameline();

                // Right Column: Telemetry and Server Status Card
                gui->begin_group();
                {
                    gui->begin_child("auth_telemetry");
                    {
                        ImGui::PushFont(set->c_font.inter_medium[1]);
                        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "Authentication Telemetry");
                        ImGui::PopFont();
                        ImGui::TextColored(ImVec4(0.6f, 0.63f, 0.72f, 1.0f), "Live server and connection status.");

                        widget->separator();

                        // Live status indicator badge
                        ImVec2 sp0 = ImGui::GetCursorScreenPos();
                        float sb_w = GetContentRegionAvail().x;
                        float sb_h = SCALE(40.f);
                        ImVec2 sp1 = ImVec2(sp0.x + sb_w, sp0.y + sb_h);
                        draw->add_rect_filled(draw_list, sp0, sp1, IM_COL32(16, 17, 24, 230), SCALE(6.f));
                        draw->add_rect(draw_list, sp0, sp1, s_StatusColor, SCALE(6.f), 1.2f);

                        std::string disp_status = s_StatusMessage;
                        if (SyzoraAuth::g_Auth.is_busy.load()) {
                            int dots = ((int)(ImGui::GetTime() * 3.0f)) % 4;
                            disp_status = "Connecting";
                            for (int d = 0; d < dots; ++d) disp_status += ".";
                        }
                        ImVec2 t_sz = ImGui::CalcTextSize(disp_status.c_str());
                        draw_list->AddText(ImVec2(sp0.x + (sb_w - t_sz.x) * 0.5f, sp0.y + (sb_h - t_sz.y) * 0.5f), s_StatusColor, disp_status.c_str());
                        ImGui::Dummy(ImVec2(sb_w, sb_h));

                        widget->separator();

                        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.8f, 1.0f), "Gateway  : ");
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "syzoraauth.mvpcheats.online");

                        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.8f, 1.0f), "Version  : ");
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), "1.3 API REST");

                        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.8f, 1.0f), "Hardware : ");
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.5f, 1.0f), "HWID Locked (SHA-256)");

                        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.8f, 1.0f), "Product  : ");
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "MvpAimExe v1.0");

                        widget->separator();

                        const float full_w = GetContentRegionAvail().x;
                        if (widget->button("Join Discord Community", { full_w, SCALE(35) }))
                        {
                            ShellExecuteA(NULL, "open", "https://discord.gg/BYFfkt78BC", NULL, NULL, SW_SHOWNORMAL);
                        }

                        widget->separator();

                        if (widget->button("Exit Loader", { full_w, SCALE(35) }))
                        {
                            var->c_panel.request_exit = true;
                            PostQuitMessage(0);
                        }
                    }
                    gui->end_child();
                }
                gui->end_group();
            }
            gui->end_content();
        }
        gui->end();
    }
}
