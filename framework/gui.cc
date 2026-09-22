#include "settings/functions.h"
#include "shader/blur.hpp"
#include "esp/esp_globals.h"
#include "esp/esp_data.h"
#include "auth/auth_gui.h"
#include "auth/syzora_auth.hpp"
#include "aimkill/aimkill_state.h"
#include "aimkill/AimkillClient.hpp"
#include "aimkill/AimkillProtocol.hpp"

namespace {
    void DrawEspPreview(float child_width)
    {
        const float canvas_w = child_width;
        const float canvas_h = SCALE(430.f);
        const ImVec2 canvas_sz(canvas_w, canvas_h);

        ImGui::InvisibleButton("##esp_preview_canvas", canvas_sz);
        const bool is_hovered = ImGui::IsItemHovered();
        const ImVec2 p0 = ImGui::GetItemRectMin();
        const ImVec2 p1 = ImGui::GetItemRectMax();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        dl->PushClipRect(p0, p1, true);

        // Viewport background gradient
        dl->AddRectFilledMultiColor(p0, p1, IM_COL32(13, 14, 21, 255), IM_COL32(13, 14, 21, 255), IM_COL32(20, 22, 33, 255), IM_COL32(20, 22, 33, 255));
        dl->AddRect(p0, p1, is_hovered ? gui->get_clr(clr->c_other_clr.accent_clr, 0.5f) : IM_COL32(38, 42, 58, 200), SCALE(6.f), 0, 1.2f);

        // Faint tactical radar grid
        for (float gx = p0.x + SCALE(32.f); gx < p1.x; gx += SCALE(32.f))
            dl->AddLine(ImVec2(gx, p0.y), ImVec2(gx, p1.y), IM_COL32(255, 255, 255, 7), 1.0f);
        for (float gy = p0.y + SCALE(32.f); gy < p1.y; gy += SCALE(32.f))
            dl->AddLine(ImVec2(p0.x, gy), ImVec2(p1.x, gy), IM_COL32(255, 255, 255, 7), 1.0f);

        // Concentric radar rings
        const ImVec2 center(p0.x + canvas_w * 0.5f, p0.y + canvas_h * 0.50f);
        dl->AddCircle(center, SCALE(85.f), IM_COL32(255, 255, 255, 12), 48, 1.0f);
        dl->AddCircle(center, SCALE(130.f), IM_COL32(255, 255, 255, 7), 48, 1.0f);

        // Corner HUD brackets
        const float blen = SCALE(12.f);
        const float bpad = SCALE(8.f);
        const ImU32 bcol = IM_COL32(100, 120, 180, 80);
        dl->AddLine(p0 + ImVec2(bpad, bpad), p0 + ImVec2(bpad + blen, bpad), bcol, 1.5f);
        dl->AddLine(p0 + ImVec2(bpad, bpad), p0 + ImVec2(bpad, bpad + blen), bcol, 1.5f);
        dl->AddLine(ImVec2(p1.x - bpad, p0.y + bpad), ImVec2(p1.x - bpad - blen, p0.y + bpad), bcol, 1.5f);
        dl->AddLine(ImVec2(p1.x - bpad, p0.y + bpad), ImVec2(p1.x - bpad, p0.y + bpad + blen), bcol, 1.5f);
        dl->AddLine(ImVec2(p0.x + bpad, p1.y - bpad), ImVec2(p0.x + bpad + blen, p1.y - bpad), bcol, 1.5f);
        dl->AddLine(ImVec2(p0.x + bpad, p1.y - bpad), ImVec2(p0.x + bpad, p1.y - bpad - blen), bcol, 1.5f);
        dl->AddLine(ImVec2(p1.x - bpad, p1.y - bpad), ImVec2(p1.x - bpad - blen, p1.y - bpad), bcol, 1.5f);
        dl->AddLine(ImVec2(p1.x - bpad, p1.y - bpad), ImVec2(p1.x - bpad, p1.y - bpad - blen), bcol, 1.5f);



        // Keypoints for anatomical mannequin
        const float cx = center.x;
        const float cy = center.y + SCALE(12.f);

        const float head_r = SCALE(12.f);
        const ImVec2 head_pt(cx, cy - SCALE(95.f));
        const ImVec2 neck_pt(cx, head_pt.y + SCALE(17.f));
        const ImVec2 chest_pt(cx, neck_pt.y + SCALE(26.f));
        const ImVec2 pelvis_pt(cx, chest_pt.y + SCALE(38.f));

        const ImVec2 left_shoulder(cx - SCALE(25.f), neck_pt.y + SCALE(6.f));
        const ImVec2 right_shoulder(cx + SCALE(25.f), neck_pt.y + SCALE(6.f));
        const ImVec2 left_elbow(cx - SCALE(34.f), chest_pt.y + SCALE(6.f));
        const ImVec2 right_elbow(cx + SCALE(34.f), chest_pt.y + SCALE(6.f));
        const ImVec2 left_hand(cx - SCALE(22.f), pelvis_pt.y - SCALE(6.f));
        const ImVec2 right_hand(cx + SCALE(22.f), pelvis_pt.y - SCALE(8.f));

        const ImVec2 left_knee(cx - SCALE(16.f), pelvis_pt.y + SCALE(45.f));
        const ImVec2 right_knee(cx + SCALE(16.f), pelvis_pt.y + SCALE(45.f));
        const ImVec2 left_foot(cx - SCALE(20.f), pelvis_pt.y + SCALE(95.f));
        const ImVec2 right_foot(cx + SCALE(20.f), pelvis_pt.y + SCALE(95.f));

        const ImVec2 box_min(cx - SCALE(46.f), head_pt.y - head_r - SCALE(8.f));
        const ImVec2 box_max(cx + SCALE(46.f), left_foot.y + SCALE(7.f));

        // 1. Mannequin Silhouette
        dl->AddCircleFilled(head_pt, head_r, IM_COL32(32, 36, 52, 230));
        dl->AddCircle(head_pt, head_r, IM_COL32(60, 70, 95, 240), 32, 1.2f);
        dl->AddLine(head_pt - ImVec2(head_r * 0.7f, 0), head_pt + ImVec2(head_r * 0.7f, 0), IM_COL32(100, 160, 230, 200), 2.0f);

        dl->AddLine(head_pt + ImVec2(0, head_r * 0.8f), neck_pt, IM_COL32(40, 46, 65, 230), SCALE(5.0f));

        ImVec2 chest_poly[4] = {
            left_shoulder,
            right_shoulder,
            ImVec2(cx + SCALE(18.f), pelvis_pt.y - SCALE(10.f)),
            ImVec2(cx - SCALE(18.f), pelvis_pt.y - SCALE(10.f))
        };
        dl->AddConvexPolyFilled(chest_poly, 4, IM_COL32(32, 36, 52, 230));
        dl->AddPolyline(chest_poly, 4, IM_COL32(55, 65, 90, 240), ImDrawFlags_Closed, 1.2f);

        ImVec2 pelvis_poly[4] = {
            ImVec2(cx - SCALE(18.f), pelvis_pt.y - SCALE(10.f)),
            ImVec2(cx + SCALE(18.f), pelvis_pt.y - SCALE(10.f)),
            ImVec2(cx + SCALE(20.f), pelvis_pt.y + SCALE(12.f)),
            ImVec2(cx - SCALE(20.f), pelvis_pt.y + SCALE(12.f))
        };
        dl->AddConvexPolyFilled(pelvis_poly, 4, IM_COL32(28, 32, 46, 230));
        dl->AddPolyline(pelvis_poly, 4, IM_COL32(50, 60, 85, 240), ImDrawFlags_Closed, 1.2f);

        dl->AddLine(left_shoulder, left_elbow, IM_COL32(35, 40, 58, 220), SCALE(6.0f));
        dl->AddLine(left_elbow, left_hand, IM_COL32(35, 40, 58, 220), SCALE(5.0f));
        dl->AddLine(right_shoulder, right_elbow, IM_COL32(35, 40, 58, 220), SCALE(6.0f));
        dl->AddLine(right_elbow, right_hand, IM_COL32(35, 40, 58, 220), SCALE(5.0f));

        dl->AddLine(ImVec2(cx - SCALE(10.f), pelvis_pt.y + SCALE(10.f)), left_knee, IM_COL32(32, 36, 52, 220), SCALE(7.0f));
        dl->AddLine(left_knee, left_foot, IM_COL32(32, 36, 52, 220), SCALE(6.0f));
        dl->AddLine(ImVec2(cx + SCALE(10.f), pelvis_pt.y + SCALE(10.f)), right_knee, IM_COL32(32, 36, 52, 220), SCALE(7.0f));
        dl->AddLine(right_knee, right_foot, IM_COL32(32, 36, 52, 220), SCALE(6.0f));

        dl->AddLine(left_foot - ImVec2(SCALE(5.f), 0), left_foot + ImVec2(SCALE(7.f), 0), IM_COL32(50, 58, 80, 240), SCALE(4.0f));
        dl->AddLine(right_foot - ImVec2(SCALE(3.f), 0), right_foot + ImVec2(SCALE(9.f), 0), IM_COL32(50, 58, 80, 240), SCALE(4.0f));

        // Weapon silhouette
        dl->AddLine(left_hand - ImVec2(SCALE(10.f), -SCALE(4.f)), right_hand + ImVec2(SCALE(22.f), -SCALE(14.f)), IM_COL32(55, 62, 85, 230), SCALE(4.5f));
        dl->AddLine(right_hand + ImVec2(SCALE(10.f), -SCALE(8.f)), right_hand + ImVec2(SCALE(32.f), -SCALE(20.f)), IM_COL32(70, 80, 105, 230), SCALE(2.0f));

        // 2. ESP Skeleton
        if (g_Globals.Visuals.Skeleton)
        {
            ImU32 skel_col = ImGui::ColorConvertFloat4ToU32(ImVec4(g_Globals.Visuals.SkeletonColor[0], g_Globals.Visuals.SkeletonColor[1], g_Globals.Visuals.SkeletonColor[2], g_Globals.Visuals.SkeletonColor[3]));
            const float bone_thick = SCALE(1.8f);

            auto draw_bone = [&](const ImVec2& a, const ImVec2& b) {
                dl->AddLine(a, b, IM_COL32(0, 0, 0, 200), bone_thick + 1.2f);
                dl->AddLine(a, b, skel_col, bone_thick);
            };

            draw_bone(head_pt, neck_pt);
            draw_bone(neck_pt, chest_pt);
            draw_bone(chest_pt, pelvis_pt);

            draw_bone(neck_pt, left_shoulder);
            draw_bone(left_shoulder, left_elbow);
            draw_bone(left_elbow, left_hand);

            draw_bone(neck_pt, right_shoulder);
            draw_bone(right_shoulder, right_elbow);
            draw_bone(right_elbow, right_hand);

            draw_bone(pelvis_pt, left_knee);
            draw_bone(left_knee, left_foot);

            draw_bone(pelvis_pt, right_knee);
            draw_bone(right_knee, right_foot);

            const ImVec2 joints[] = { head_pt, neck_pt, chest_pt, pelvis_pt, left_shoulder, right_shoulder, left_elbow, right_elbow, left_hand, right_hand, left_knee, right_knee, left_foot, right_foot };
            for (const auto& j : joints) {
                dl->AddCircleFilled(j, SCALE(2.8f), skel_col);
                dl->AddCircle(j, SCALE(2.8f), IM_COL32(0, 0, 0, 200), 0, 1.0f);
            }
        }

        // 3. ESP Box
        if (g_Globals.Visuals.Box)
        {
            ImU32 box_col = ImGui::ColorConvertFloat4ToU32(ImVec4(g_Globals.Visuals.BoxColor[0], g_Globals.Visuals.BoxColor[1], g_Globals.Visuals.BoxColor[2], g_Globals.Visuals.BoxColor[3]));
            dl->AddRectFilled(box_min, box_max, (box_col & 0x00FFFFFF) | (18 << 24));

            if (g_Globals.Visuals.players_box == 1) // Full Box
            {
                dl->AddRect(box_min - ImVec2(1, 1), box_max + ImVec2(1, 1), IM_COL32(0, 0, 0, 200), 0, 0, 1.0f);
                dl->AddRect(box_min + ImVec2(1, 1), box_max - ImVec2(1, 1), IM_COL32(0, 0, 0, 200), 0, 0, 1.0f);
                dl->AddRect(box_min, box_max, box_col, 0, 0, 1.5f);
            }
            else // Corner Box (players_box == 2)
            {
                const float cl = SCALE(14.f);
                auto draw_corner = [&](const ImVec2& corner, const ImVec2& d1, const ImVec2& d2) {
                    dl->AddLine(corner - ImVec2(1, 1), corner + d1 - ImVec2(1, 1), IM_COL32(0, 0, 0, 200), 2.5f);
                    dl->AddLine(corner - ImVec2(1, 1), corner + d2 - ImVec2(1, 1), IM_COL32(0, 0, 0, 200), 2.5f);
                    dl->AddLine(corner, corner + d1, box_col, 1.5f);
                    dl->AddLine(corner, corner + d2, box_col, 1.5f);
                };

                draw_corner(box_min, ImVec2(cl, 0), ImVec2(0, cl));
                draw_corner(ImVec2(box_max.x, box_min.y), ImVec2(-cl, 0), ImVec2(0, cl));
                draw_corner(ImVec2(box_min.x, box_max.y), ImVec2(cl, 0), ImVec2(0, -cl));
                draw_corner(box_max, ImVec2(-cl, 0), ImVec2(0, -cl));
            }
        }

        // 4. ESP Snapline
        if (g_Globals.Visuals.Lines)
        {
            ImVec2 line_start;
            if (g_Globals.Visuals.EspLines == 1) // Top Screen
                line_start = ImVec2(p0.x + canvas_w * 0.5f, p0.y + SCALE(6.f));
            else // Bottom Screen
                line_start = ImVec2(p0.x + canvas_w * 0.5f, p1.y - SCALE(6.f));

            ImVec2 line_end = ImVec2(cx, box_min.y);

            ImU32 line_col = 0;
            if (g_Globals.Visuals.RainbowLines) {
                float t = (float)ImGui::GetTime() * 1.5f;
                ImVec4 rb = ImColor::HSV(fmodf(t, 1.0f), 0.85f, 1.0f);
                line_col = ImGui::ColorConvertFloat4ToU32(rb);
            } else {
                line_col = ImGui::ColorConvertFloat4ToU32(ImVec4(g_Globals.Visuals.LinesColor[0], g_Globals.Visuals.LinesColor[1], g_Globals.Visuals.LinesColor[2], g_Globals.Visuals.LinesColor[3]));
            }

            if (g_Globals.Visuals.GlowLines) {
                dl->AddLine(line_start, line_end, (line_col & 0x00FFFFFF) | (35 << 24), SCALE(5.5f));
                dl->AddLine(line_start, line_end, (line_col & 0x00FFFFFF) | (70 << 24), SCALE(3.5f));
            }
            dl->AddLine(line_start, line_end, line_col, SCALE(1.5f));
            dl->AddCircleFilled(line_start, SCALE(3.0f), line_col);
        }

        // 5. ESP Health Bar
        if (g_Globals.Visuals.HealthBar)
        {
            const float bar_thick = SCALE(4.0f);
            const float bar_gap = SCALE(4.5f);

            if (g_Globals.Visuals.players_healthbar == 1) // Left
            {
                ImVec2 bg_min(box_min.x - bar_gap - bar_thick, box_min.y);
                ImVec2 bg_max(box_min.x - bar_gap, box_max.y);
                dl->AddRectFilled(bg_min, bg_max, IM_COL32(20, 20, 25, 230));
                dl->AddRect(bg_min - ImVec2(1, 1), bg_max + ImVec2(1, 1), IM_COL32(0, 0, 0, 220));
                dl->AddRectFilledMultiColor(bg_min, bg_max, IM_COL32(46, 213, 115, 255), IM_COL32(46, 213, 115, 255), IM_COL32(32, 180, 90, 255), IM_COL32(32, 180, 90, 255));
                dl->AddText(set->c_font.inter_medium[0], set->c_font.inter_medium[0]->FontSize * 0.82f, ImVec2(bg_min.x - SCALE(18.f), bg_min.y - SCALE(2.f)), IM_COL32(46, 213, 115, 240), "100");
            }
            else if (g_Globals.Visuals.players_healthbar == 2) // Right
            {
                ImVec2 bg_min(box_max.x + bar_gap, box_min.y);
                ImVec2 bg_max(box_max.x + bar_gap + bar_thick, box_max.y);
                dl->AddRectFilled(bg_min, bg_max, IM_COL32(20, 20, 25, 230));
                dl->AddRect(bg_min - ImVec2(1, 1), bg_max + ImVec2(1, 1), IM_COL32(0, 0, 0, 220));
                dl->AddRectFilledMultiColor(bg_min, bg_max, IM_COL32(46, 213, 115, 255), IM_COL32(46, 213, 115, 255), IM_COL32(32, 180, 90, 255), IM_COL32(32, 180, 90, 255));
                dl->AddText(set->c_font.inter_medium[0], set->c_font.inter_medium[0]->FontSize * 0.82f, ImVec2(bg_max.x + SCALE(3.f), bg_min.y - SCALE(2.f)), IM_COL32(46, 213, 115, 240), "100");
            }
            else if (g_Globals.Visuals.players_healthbar == 3) // Bottom
            {
                ImVec2 bg_min(box_min.x, box_max.y + bar_gap);
                ImVec2 bg_max(box_max.x, box_max.y + bar_gap + bar_thick);
                dl->AddRectFilled(bg_min, bg_max, IM_COL32(20, 20, 25, 230));
                dl->AddRect(bg_min - ImVec2(1, 1), bg_max + ImVec2(1, 1), IM_COL32(0, 0, 0, 220));
                dl->AddRectFilledMultiColor(bg_min, bg_max, IM_COL32(46, 213, 115, 255), IM_COL32(32, 180, 90, 255), IM_COL32(32, 180, 90, 255), IM_COL32(46, 213, 115, 255));
                dl->AddText(set->c_font.inter_medium[0], set->c_font.inter_medium[0]->FontSize * 0.82f, ImVec2(bg_max.x + SCALE(4.f), bg_min.y - SCALE(3.f)), IM_COL32(46, 213, 115, 240), "100");
            }
            else if (g_Globals.Visuals.players_healthbar == 4) // Text / Top
            {
                const char* hp_txt = "HP: 100";
                ImVec2 txt_sz = set->c_font.inter_medium[0]->CalcTextSizeA(set->c_font.inter_medium[0]->FontSize * 0.9f, FLT_MAX, -1, hp_txt);
                ImVec2 hp_pos(cx - txt_sz.x * 0.5f, box_min.y - txt_sz.y - (g_Globals.Visuals.Name ? SCALE(18.f) : SCALE(5.f)));
                dl->AddText(set->c_font.inter_medium[0], set->c_font.inter_medium[0]->FontSize * 0.9f, hp_pos + ImVec2(1, 1), IM_COL32(0, 0, 0, 220), hp_txt);
                dl->AddText(set->c_font.inter_medium[0], set->c_font.inter_medium[0]->FontSize * 0.9f, hp_pos, IM_COL32(46, 213, 115, 255), hp_txt);
            }
        }

        // Interactive Click to Reposition Health Bar
        if (is_hovered && g_Globals.Visuals.HealthBar && ImGui::IsMouseClicked(0))
        {
            ImVec2 mouse = ImGui::GetIO().MousePos;
            if (mouse.x >= p0.x && mouse.x <= p1.x && mouse.y >= p0.y && mouse.y <= p1.y)
            {
                if (mouse.y < box_min.y)
                    g_Globals.Visuals.players_healthbar = 4; // Top/Text
                else if (mouse.y > box_max.y)
                    g_Globals.Visuals.players_healthbar = 3; // Bottom
                else if (mouse.x < cx)
                    g_Globals.Visuals.players_healthbar = 1; // Left
                else
                    g_Globals.Visuals.players_healthbar = 2; // Right
            }
        }
        if (is_hovered && g_Globals.Visuals.HealthBar) {
            ImGui::SetTooltip("Click sides around model to position Health Bar (Left / Right / Bottom / Top)");
        }

        // 6. Name & Level
        if (g_Globals.Visuals.Name || g_Globals.Visuals.Level)
        {
            std::string full_title = "";
            if (g_Globals.Visuals.Level)
                full_title += "[Lv.75] ";
            if (g_Globals.Visuals.Name)
                full_title += "Player_01";

            if (!full_title.empty())
            {
                ImVec2 name_sz = set->c_font.inter_medium[0]->CalcTextSizeA(set->c_font.inter_medium[0]->FontSize, FLT_MAX, -1, full_title.c_str());
                float name_y = box_min.y - name_sz.y - SCALE(4.f);
                if (g_Globals.Visuals.HealthBar && g_Globals.Visuals.players_healthbar == 4)
                    name_y -= SCALE(14.f);

                ImVec2 name_pos(cx - name_sz.x * 0.5f, name_y);
                dl->AddText(set->c_font.inter_medium[0], set->c_font.inter_medium[0]->FontSize, name_pos + ImVec2(1, 1), IM_COL32(0, 0, 0, 220), full_title.c_str());
                dl->AddText(set->c_font.inter_medium[0], set->c_font.inter_medium[0]->FontSize, name_pos, IM_COL32(240, 242, 250, 255), full_title.c_str());
            }
        }

        // 7. Distance & Weapon
        float bottom_cur_y = box_max.y + SCALE(6.f);
        if (g_Globals.Visuals.HealthBar && g_Globals.Visuals.players_healthbar == 3)
            bottom_cur_y += SCALE(12.f);

        if (g_Globals.Visuals.Distance)
        {
            char dist_str[32];
            snprintf(dist_str, sizeof(dist_str), "[%d m]", g_Globals.Visuals.DistanceEsp > 0 ? (g_Globals.Visuals.DistanceEsp / 2) : 35);
            ImVec2 d_sz = set->c_font.inter_medium[0]->CalcTextSizeA(set->c_font.inter_medium[0]->FontSize * 0.9f, FLT_MAX, -1, dist_str);
            ImVec2 d_pos(cx - d_sz.x * 0.5f, bottom_cur_y);
            dl->AddText(set->c_font.inter_medium[0], set->c_font.inter_medium[0]->FontSize * 0.9f, d_pos + ImVec2(1, 1), IM_COL32(0, 0, 0, 200), dist_str);
            dl->AddText(set->c_font.inter_medium[0], set->c_font.inter_medium[0]->FontSize * 0.9f, d_pos, IM_COL32(165, 195, 245, 240), dist_str);
            bottom_cur_y += d_sz.y + SCALE(2.f);
        }

        if (g_Globals.Visuals.ESPWeaponIcon)
        {
            const char* ak_icon = "\ue074";
            ImFont* icFont = set->c_font.icon_weapon;
            if (icFont)
            {
                float icScale = (g_Globals.Visuals.IconScale > 0.1f) ? g_Globals.Visuals.IconScale : 0.92f;
                float icSize = icFont->FontSize * icScale;
                if (icSize < 12.0f) icSize = 16.0f;
                ImVec2 icSz = icFont->CalcTextSizeA(icSize, FLT_MAX, 0.0f, ak_icon);
                ImVec2 icPos(cx - icSz.x * 0.5f, bottom_cur_y);
                dl->AddText(icFont, icSize, icPos + ImVec2(1, 1), IM_COL32(0, 0, 0, 200), ak_icon);
                dl->AddText(icFont, icSize, icPos, IM_COL32(255, 255, 255, 240), ak_icon);
                bottom_cur_y += icSz.y + SCALE(2.f);
            }
        }

        if (g_Globals.Visuals.ESPWeapon)
        {
            const char* wpn_name = "AK-47";
            ImVec2 w_sz = set->c_font.inter_medium[0]->CalcTextSizeA(set->c_font.inter_medium[0]->FontSize * 0.9f, FLT_MAX, -1, wpn_name);
            ImVec2 w_pos(cx - w_sz.x * 0.5f, bottom_cur_y);
            dl->AddText(set->c_font.inter_medium[0], set->c_font.inter_medium[0]->FontSize * 0.9f, w_pos + ImVec2(1, 1), IM_COL32(0, 0, 0, 200), wpn_name);
            dl->AddText(set->c_font.inter_medium[0], set->c_font.inter_medium[0]->FontSize * 0.9f, w_pos, IM_COL32(235, 180, 80, 240), wpn_name);
            bottom_cur_y += w_sz.y + SCALE(2.f);
        }

        if (!g_Globals.Visuals.Enable)
        {
            ImVec2 banner_min(p0.x + SCALE(20.f), center.y - SCALE(18.f));
            ImVec2 banner_max(p1.x - SCALE(20.f), center.y + SCALE(18.f));
            dl->AddRectFilled(banner_min, banner_max, IM_COL32(18, 20, 28, 230), SCALE(6.f));
            dl->AddRect(banner_min, banner_max, IM_COL32(235, 75, 75, 200), SCALE(6.f), 0, 1.2f);
            const char* dis_msg = "ESP IS DISABLED";
            ImVec2 msg_sz = set->c_font.inter_medium[1]->CalcTextSizeA(set->c_font.inter_medium[1]->FontSize, FLT_MAX, -1, dis_msg);
            dl->AddText(set->c_font.inter_medium[1], set->c_font.inter_medium[1]->FontSize, ImVec2(center.x - msg_sz.x * 0.5f, center.y - msg_sz.y * 0.5f), IM_COL32(245, 90, 90, 255), dis_msg);
        }

        dl->PopClipRect();

        // Bottom quick selector chips for health bar position
        gui->set_cursor_pos_y(gui->get_cursor_pos_y() + SCALE(10.f));
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(gui->get_clr(clr->c_text.text)), "Healthbar Side:");
        ImGui::SameLine();

        static const char* sides[] = { "None", "Left", "Right", "Bottom", "Text" };
        for (int s = 1; s <= 4; ++s)
        {
            if (s > 1) ImGui::SameLine();
            bool active = (g_Globals.Visuals.players_healthbar == s);
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button, gui->get_clr(clr->c_other_clr.accent_clr, 0.75f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, gui->get_clr(clr->c_element.layout));
            }
            if (ImGui::Button(sides[s], ImVec2(SCALE(54.f), SCALE(24.f))))
            {
                g_Globals.Visuals.players_healthbar = s;
                g_Globals.Visuals.HealthBar = true;
            }
            ImGui::PopStyleColor();
        }
    }
}

void c_gui::render()
{

	gui->new_frame();
	{
		// Keep AutoRefresh in sync
		g_Globals.EspConfig.AutoRefresh = var->c_settings.connect_lib || var->c_esp.auto_refresh;

		notify->setup_notify();

		// Syzora Auth Gate: Only render cheat menu after successful login
		static bool s_auth_transition_done = false;
		if (!AuthGui::IsAuthenticated())
		{
			set->c_window.window_size = ImVec2(860, 630);
			AuthGui::Render();
			goto do_end_frame; // Must not skip gui->end_frame() (calls ImGui::Render)
		}
		else if (!s_auth_transition_done)
		{
			s_auth_transition_done = true;
			set->c_window.window_size = ImVec2(860, 630);
		}

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

			// Native window dragging from ANY empty / transparent space or logo in the sidebar OR the top header
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || 
			   (ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsAnyItemActive() && ImGui::GetIO().MouseDownDuration[0] < 0.15f))
			{
				ImVec2 mouse = ImGui::GetMousePos();
				bool in_header = (mouse.y >= pos.y && mouse.y <= pos.y + SCALE(75.0f) && mouse.x >= pos.x && mouse.x <= pos.x + size.x);
				bool in_sidebar = (mouse.x >= pos.x && mouse.x <= pos.x + SCALE(110.0f) && mouse.y >= pos.y && mouse.y <= pos.y + size.y);

				if (in_header || in_sidebar)
				{
					bool on_tab = false;
					for (int t = 0; t < 4; ++t)
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

						// CRITICAL: Reset ImGui mouse state immediately!
						// When Windows finishes the modal drag loop, ImGui missed WM_LBUTTONUP.
						// Resetting these fields prevents the "requires 2 clicks to drag again" issue.
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

			draw->add_rect_filled(draw_list, { pos.x, pos.y }, { pos.x + size.x, pos.y + size.y }, gui->get_clr(clr->c_window.general_layout), SCALE(set->c_window.general_rounding));
			draw->add_rect(draw_list, { pos.x, pos.y }, { pos.x + size.x, pos.y + size.y }, gui->get_clr(clr->c_window.general_stroke), SCALE(set->c_window.general_rounding));

			float content_top = pos.y + SCALE(75.0f);
			float content_bottom = pos.y + (size.y - SCALE(15.0f));
			float content_left = pos.x + SCALE(110.0f);
			float content_right = pos.x + (size.x - SCALE(15.0f));

			// Main black panel pushed down to start at tab level, leaving top header fully transparent
			draw->add_rect_filled(draw_list, { content_left, content_top }, { content_right, content_bottom }, gui->get_clr(clr->c_window.layout), SCALE(set->c_window.rounding));
			draw->add_rect(draw_list, { content_left, content_top }, { content_right, content_bottom }, gui->get_clr(clr->c_window.stroke), SCALE(set->c_window.rounding));

			draw->rect_filled_multi_color(draw_list, { content_left + (content_right - content_left) * 0.5f, content_top }, { content_right, content_top + 1 }, gui->get_clr(clr->c_other_clr.accent_clr, 0.2f), gui->get_clr(clr->c_other_clr.accent_clr, 0.f), gui->get_clr(clr->c_other_clr.accent_clr, 0.f), gui->get_clr(clr->c_other_clr.accent_clr, 0.2f));
			draw->rect_filled_multi_color(draw_list, { content_left, content_top }, { content_left + (content_right - content_left) * 0.5f, content_top + 1 }, gui->get_clr(clr->c_other_clr.accent_clr, 0.f), gui->get_clr(clr->c_other_clr.accent_clr, 0.2f), gui->get_clr(clr->c_other_clr.accent_clr, 0.2f), gui->get_clr(clr->c_other_clr.accent_clr, 0.f));

			draw->rect_filled_multi_color(draw_list, { pos.x + size.x / 2, pos.y + size.y - 1 }, { pos.x + size.x, pos.y + size.y }, gui->get_clr(clr->c_other_clr.accent_clr, 0.2f), gui->get_clr(clr->c_other_clr.accent_clr, 0.f), gui->get_clr(clr->c_other_clr.accent_clr, 0.f), gui->get_clr(clr->c_other_clr.accent_clr, 0.2f));
			draw->rect_filled_multi_color(draw_list, { pos.x, pos.y + size.y - 1 }, { pos.x + size.x / 2, pos.y + size.y }, gui->get_clr(clr->c_other_clr.accent_clr, 0.f), gui->get_clr(clr->c_other_clr.accent_clr, 0.2f), gui->get_clr(clr->c_other_clr.accent_clr, 0.2f), gui->get_clr(clr->c_other_clr.accent_clr, 0.f));

			// 1. Top Logo in sidebar
			draw->render_text(draw_list, set->c_font.icon[2], { pos.x, pos.y + SCALE(10) }, { pos.x + SCALE(110), pos.y + SCALE(62) }, gui->get_clr(clr->c_other_clr.accent_clr), "B", 0, 0, { 0.5f, 0.5f });
			draw->rect_filled_multi_color(draw_list, { pos.x + SCALE(20), pos.y + SCALE(68) }, { pos.x + SCALE(90), pos.y + SCALE(70) },
				gui->get_clr(clr->c_other_clr.accent_clr, 0.f), gui->get_clr(clr->c_other_clr.accent_clr, 0.5f),
				gui->get_clr(clr->c_other_clr.accent_clr, 0.5f), gui->get_clr(clr->c_other_clr.accent_clr, 0.f));

			// 2. Top Middle Panel Title: Modern 3D Revolving Rotary Wave & Dual 360° Gyro Emblems
			{
				float title_center_x = (content_left + content_right) * 0.5f;
				float title_center_y = pos.y + SCALE(36.0f);

				float time = (float)ImGui::GetTime();
				ImVec4 acc = clr->c_other_clr.accent_clr;

				const char* full_title = "Mvp Cheats Aimkill";
				const int total_chars = 18;
				const int split_idx = 11; // Index where "Aimkill" begins

				float total_w = set->c_font.name->CalcTextSizeA(set->c_font.name->FontSize, FLT_MAX, -1, full_title).x;
				float start_x = title_center_x - total_w * 0.5f;
				float font_h = set->c_font.name->FontSize;
				float base_y = title_center_y - font_h * 0.5f;

				// --- Dual 360° Rotating Gyro Diamond Emblems on Left & Right ---
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
					{
						draw_list->AddLine(p[k], p[(k + 1) % 4], gui->get_clr(emblem_col, 0.85f), 1.4f);
					}

					// Inner revolving accent cross
					float r_in = r * 0.5f;
					draw_list->AddLine({ center.x - r_in * cos_a, center.y - r_in * sin_a },
					                   { center.x + r_in * cos_a, center.y + r_in * sin_a },
					                   gui->get_clr(clr->c_other_clr.white_clr, 0.9f), 1.2f);

					draw_list->AddCircleFilled(center, SCALE(1.4f), gui->get_clr(emblem_col, 1.0f));
				};

				float rot_speed = time * 2.2f;
				draw_rotating_emblem({ start_x - SCALE(20.0f), title_center_y }, rot_speed, acc);
				draw_rotating_emblem({ start_x + total_w + SCALE(20.0f), title_center_y }, -rot_speed, acc);

				// --- Letter-by-Letter 3D Revolving Rotary Wave ---
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

					// Staggered rotary cylinder angle
					float angle = time * 3.2f - (float)i * 0.28f;
					float rot_y = sinf(angle) * SCALE(3.5f);
					float depth = cosf(angle); // 3D depth lighting factor (-1 to +1)

					ImVec4 char_col;
					if (i < split_idx)
					{
						// "Mvp Cheats" - Metallic Silver with dynamic rotary depth lighting
						float bright = 0.78f + (depth * 0.5f + 0.5f) * 0.22f;
						char_col = ImVec4(bright * 0.92f, bright * 0.94f, bright, 1.0f);
					}
					else
					{
						// "Aimkill" - Glowing accent with smooth cylindrical highlight
						float bright = 0.75f + (depth * 0.5f + 0.5f) * 0.35f;
						char_col = ImVec4(ImMin(acc.x * bright, 1.0f), ImMin(acc.y * bright, 1.0f), ImMin(acc.z * bright, 1.0f), 1.0f);
					}

					// Crisp, razor-sharp rendering with zero blurry ghosting
					draw_list->AddText(set->c_font.name, font_h, { cur_x, base_y + rot_y }, gui->get_clr(char_col), ch_str);
					cur_x += ch_w;
				}

				// --- Linear Laser Light Sweep Beneath Title ---
				float beam_w = (total_w + SCALE(40.0f));
				float beam_y = base_y + font_h + SCALE(7.0f);
				float half_beam = beam_w * 0.5f;

				// Subtle base guide track line
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

				// High-speed specular traveling light sweep
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

			// 3. Vertical tabs stacked smoothly from top to bottom
			for (int i = 0; i < 4; i++)
			{
				float cur_y = start_tab_y + i * (tab_h + tab_spacing);

				ImRect tab_rect(ImVec2(tab_x, cur_y), ImVec2(tab_x + tab_w, cur_y + tab_h));
				ImGuiID tab_id = ImGui::GetID(("##sidebar_tab_" + std::to_string(i)).c_str());

				gui->set_cursor_pos(ImVec2(tab_x - pos.x, cur_y - pos.y));
				ItemSize(tab_rect, 0);
				if (!ItemAdd(tab_rect, tab_id))
					continue;

				bool hovered = false, held = false;
				bool pressed = ImGui::ButtonBehavior(tab_rect, tab_id, &hovered, &held);
				if (pressed)
				{
					var->c_selection.selection = i;
				}

				struct s_side_tab { float anim; float hover; };
				s_side_tab* st = gui->anim_container(&st, tab_id);
				bool is_active = (var->c_selection.selection == i);
				st->anim = ImLerp(st->anim, is_active ? 1.f : 0.f, gui->fixed_speed(14.f));
				st->hover = ImLerp(st->hover, hovered ? 1.f : 0.f, gui->fixed_speed(14.f));

				// Button background with smooth accent glow
				ImVec4 bg_col = ImLerp(clr->c_window.layout, clr->c_other_clr.accent_clr, st->anim * 0.22f);
				if (st->hover > 0.01f && !is_active)
					bg_col = ImLerp(bg_col, clr->c_element.layout, st->hover * 0.5f);
				draw->add_rect_filled(draw_list, tab_rect.Min, tab_rect.Max, gui->get_clr(bg_col), SCALE(8.f));

				// Button border
				ImVec4 border_col = ImLerp(clr->c_window.stroke, clr->c_other_clr.accent_clr, st->anim * 0.85f);
				if (st->hover > 0.01f && !is_active)
					border_col = ImLerp(border_col, clr->c_other_clr.accent_clr, st->hover * 0.4f);
				draw->add_rect(draw_list, tab_rect.Min, tab_rect.Max, gui->get_clr(border_col), SCALE(8.f));

				// Horizontal glowing indicator bar across the width (at bottom of tab)
				if (st->anim > 0.01f)
				{
					float bar_pad = SCALE(14.f) * (1.f - st->anim) + SCALE(10.f);
					draw->add_rect_filled(draw_list,
						{ tab_rect.Min.x + bar_pad, tab_rect.Max.y - SCALE(5.5f) },
						{ tab_rect.Max.x - bar_pad, tab_rect.Max.y - SCALE(2.5f) },
						gui->get_clr(clr->c_other_clr.accent_clr, st->anim), SCALE(1.5f));
				}

				// Tab icon
				ImVec4 inactive_icon = ImLerp(ImVec4(0.62f, 0.62f, 0.74f, 1.0f), ImVec4(1.f, 1.f, 1.f, 1.f), st->hover);
				ImVec4 icon_col = is_active ? ImVec4(1.f, 1.f, 1.f, 1.f) : ImLerp(inactive_icon, ImVec4(1.f, 1.f, 1.f, 1.f), st->anim);
				draw->render_text(draw_list, set->c_font.icon[1], tab_rect.Min, tab_rect.Max - ImVec2(0, SCALE(2.f)), gui->get_clr(icon_col), var->c_selection.selection_icon[i].c_str(), 0, 0, { 0.5f, 0.48f });
			}

			// 4. Authenticated User Profile Card at bottom of sidebar
			float profile_y = pos.y + size.y - SCALE(65.f);
			ImRect prof_rect(ImVec2(tab_x, profile_y), ImVec2(tab_x + tab_w, profile_y + SCALE(50.f)));
			draw->add_rect_filled(draw_list, prof_rect.Min, prof_rect.Max, IM_COL32(18, 18, 26, 220), SCALE(6.f));
			draw->add_rect(draw_list, prof_rect.Min, prof_rect.Max, IM_COL32(32, 30, 44, 255), SCALE(6.f));
			std::string u_name = SyzoraAuth::g_Auth.user_data.username.empty() ? "User" : SyzoraAuth::g_Auth.user_data.username;
			if (u_name.length() > 8) u_name = u_name.substr(0, 7) + "..";
			draw->render_text(set->c_font.inter_medium[0], draw_list, prof_rect.Min + ImVec2(SCALE(4.f), SCALE(6.f)), prof_rect.Max, IM_COL32(240, 240, 255, 255), u_name.c_str(), 0, 0, { 0.5f, 0.0f });
			draw->render_text(set->c_font.inter_medium[0], draw_list, prof_rect.Min + ImVec2(SCALE(4.f), SCALE(26.f)), prof_rect.Max, IM_COL32(46, 213, 115, 255), "[ACTIVE]", 0, 0, { 0.5f, 0.0f });

			gui->set_cursor_pos(SCALE(115, 75));

			float anim_dt = ImClamp(ImGui::GetIO().DeltaTime, 0.001f, 0.05f);
			var->c_selection.selection_alpha = ImClamp(var->c_selection.selection_alpha + (10.f * anim_dt * (var->c_selection.selection == var->c_selection.selection_active ? 1.f : -1.f)), 0.f, 1.f);
			if (var->c_selection.selection_alpha <= 0.05f) var->c_selection.selection_active = var->c_selection.selection;

			gui->push_style_var(ImGuiStyleVar_Alpha, var->c_selection.selection_alpha * style->Alpha);

			gui->begin_content("content", ImVec2(size.x - SCALE(130), size.y - SCALE(90)), { 15, 15 }, { 15, 15 });
			{
				if (var->c_selection.selection_active == 0) // Aim
				{
					gui->begin_group();
					{
						gui->begin_child("aimkill_core");
						{
							if (!AimkillState::isServerConnected)
							{
								ImGui::PushFont(set->c_font.inter_medium[1]);
								ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Server Disconnected");
								ImGui::PopFont();
								ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Please connect server in Setting tab first.");
								widget->separator();
							}

							if (widget->checkbox("Enable All", &AimkillState::s_enableAll))
							{
								if (AimkillState::isServerConnected)
									AimkillClient::Get().SendToggle(AimkillMode::EnableESP, AimkillState::s_enableAll, 0.0f, AimkillState::pkgNames[AimkillState::selectedPkg]);
								Beep(AimkillState::s_enableAll ? 800 : 500, 45);
								if (notify) notify->add_notify(AimkillState::s_enableAll ? "Enable All Activated" : "Enable All Deactivated", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}

							widget->separator();

							if (widget->checkbox("Aimkill", &AimkillState::s_aimkill))
							{
								if (AimkillState::isServerConnected)
									AimkillClient::Get().SendToggle(AimkillMode::LundLeLoKill, AimkillState::s_aimkill, 0.0f, AimkillState::pkgNames[AimkillState::selectedPkg]);
								Beep(AimkillState::s_aimkill ? 800 : 500, 45);
								if (notify) notify->add_notify(AimkillState::s_aimkill ? "Aimkill Activated" : "Aimkill Deactivated", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}

							widget->separator();

							if (widget->checkbox("Auto Switch", &AimkillState::s_autoSwitch))
							{
								if (AimkillState::isServerConnected)
									AimkillClient::Get().SendToggle(AimkillMode::AutoSwitchNew, AimkillState::s_autoSwitch, 0.0f, AimkillState::pkgNames[AimkillState::selectedPkg]);
								Beep(AimkillState::s_autoSwitch ? 800 : 500, 45);
								if (notify) notify->add_notify(AimkillState::s_autoSwitch ? "Auto Switch On" : "Auto Switch Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}

							widget->separator();

							if (widget->checkbox("Cover Shots", &AimkillState::s_coverShot))
							{
								if (AimkillState::isServerConnected)
									AimkillClient::Get().SendToggle(AimkillMode::CoverShotNew, AimkillState::s_coverShot, 0.0f, AimkillState::pkgNames[AimkillState::selectedPkg]);
								Beep(AimkillState::s_coverShot ? 800 : 500, 45);
								if (notify) notify->add_notify(AimkillState::s_coverShot ? "Cover Shots On" : "Cover Shots Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}

							widget->separator();

							if (widget->checkbox("Shield Bypass", &AimkillState::s_shieldBypass))
							{
								if (AimkillState::isServerConnected)
									AimkillClient::Get().SendToggle(AimkillMode::ShieldBypass, AimkillState::s_shieldBypass, 0.0f, AimkillState::pkgNames[AimkillState::selectedPkg]);
								Beep(AimkillState::s_shieldBypass ? 800 : 500, 45);
								if (notify) notify->add_notify(AimkillState::s_shieldBypass ? "Shield Bypass On" : "Shield Bypass Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}
						}
						gui->end_child();
					}
					gui->end_group();

					gui->sameline();

					gui->begin_group();
					{
						gui->begin_child("tactical_movement");
						{
							if (widget->checkbox("Grenade ESP", &AimkillState::s_espGrenade))
							{
								if (AimkillState::isServerConnected)
									AimkillClient::Get().SendToggle(AimkillMode::EspGrenade, AimkillState::s_espGrenade, 0.0f, AimkillState::pkgNames[AimkillState::selectedPkg]);
								Beep(AimkillState::s_espGrenade ? 800 : 500, 45);
								if (notify) notify->add_notify(AimkillState::s_espGrenade ? "Grenade ESP On" : "Grenade ESP Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}

							widget->separator();

							if (widget->checkbox("Speed Hack Joystick", &AimkillState::s_speedHackJoy))
							{
								if (AimkillState::isServerConnected)
									AimkillClient::Get().SendToggle(AimkillMode::SpeedHackJoy, AimkillState::s_speedHackJoy, 0.0f, AimkillState::pkgNames[AimkillState::selectedPkg]);
								Beep(AimkillState::s_speedHackJoy ? 800 : 500, 45);
								if (notify) notify->add_notify(AimkillState::s_speedHackJoy ? "Speed Hack Joy On" : "Speed Hack Joy Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}

							widget->separator();

							if (widget->checkbox("Speed Dash", &AimkillState::s_speedDash))
							{
								if (AimkillState::isServerConnected)
									AimkillClient::Get().SendToggle(AimkillMode::SpeedDash, AimkillState::s_speedDash, 0.0f, AimkillState::pkgNames[AimkillState::selectedPkg]);
								Beep(AimkillState::s_speedDash ? 800 : 500, 45);
								if (notify) notify->add_notify(AimkillState::s_speedDash ? "Speed Dash On" : "Speed Dash Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}

							widget->separator();

							if (widget->checkbox("Down Aimkill", &AimkillState::s_downAimkill))
							{
								if (AimkillState::isServerConnected)
									AimkillClient::Get().SendToggle(AimkillMode::DownAimkill, AimkillState::s_downAimkill, 0.0f, AimkillState::pkgNames[AimkillState::selectedPkg]);
								Beep(AimkillState::s_downAimkill ? 800 : 500, 45);
								if (notify) notify->add_notify(AimkillState::s_downAimkill ? "Down Aimkill On" : "Down Aimkill Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}

							widget->separator();

							widget->keybind("Down Aimkill Key", &AimkillState::s_downAimkill_key);

							widget->separator();

							if (widget->checkbox("Reset Guest", &AimkillState::s_resetGuest))
							{
								if (AimkillState::isServerConnected)
									AimkillClient::Get().SendToggle(AimkillMode::ResetGuest, AimkillState::s_resetGuest, 0.0f, AimkillState::pkgNames[AimkillState::selectedPkg]);
								Beep(AimkillState::s_resetGuest ? 800 : 500, 45);
								if (notify) notify->add_notify(AimkillState::s_resetGuest ? "Reset Guest On" : "Reset Guest Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}
						}
						gui->end_child();
					}
					gui->end_group();
				}
				else if (var->c_selection.selection_active == 1) // Visuals
				{
					const float avail_w = GetContentRegionAvail().x;
					const float avail_h = GetContentRegionAvail().y;
					const float col_w = (avail_w - SCALE(15.f)) * 0.5f;

					// Left Column: Scrollable sub-container so all controls slide smoothly
					ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
					ImGui::BeginChild("##visuals_left_scroll", ImVec2(col_w, avail_h), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
					ImGui::PopStyleVar();
					{
						gui->begin_group();
						{
							gui->begin_child("esp_main", ImVec2(col_w, 0));
							{
							if (widget->checkbox("Enable Streamer ESP", &g_Globals.General.Capture))
							{
								Beep(g_Globals.General.Capture ? 800 : 500, 45);
								notify->add_notify(g_Globals.General.Capture ? "Stream Mode On" : "Stream Mode Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}

							widget->separator();

							if (widget->checkbox("Enable Esp", &g_Globals.Visuals.Enable))
							{
								Beep(g_Globals.Visuals.Enable ? 800 : 500, 45);
								notify->add_notify(g_Globals.Visuals.Enable ? "ESP On" : "ESP Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}

							widget->separator();

							if (widget->checkbox("Count Enemy", &g_Globals.Visuals.ShowNearEnemyCount))
							{
								Beep(g_Globals.Visuals.ShowNearEnemyCount ? 800 : 500, 45);
								notify->add_notify(g_Globals.Visuals.ShowNearEnemyCount ? "Count Enemy On" : "Count Enemy Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}

							widget->separator();

							if (widget->checkbox_with_color("ESP Box", &g_Globals.Visuals.Box, g_Globals.Visuals.BoxColor, true))
							{
								Beep(g_Globals.Visuals.Box ? 800 : 500, 45);
								notify->add_notify(g_Globals.Visuals.Box ? "ESP Box On" : "ESP Box Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}

							if (g_Globals.Visuals.Box)
							{
								widget->separator();
								static std::vector<std::string> box_styles = { "Full Box", "Corner Box" };
								int boxIdx = g_Globals.Visuals.players_box - 1;
								if (boxIdx < 0 || boxIdx > 1) boxIdx = 1;
								if (widget->dropdown("Box Style", &boxIdx, box_styles, (int)box_styles.size()))
								{
									g_Globals.Visuals.players_box = boxIdx + 1;
								}
							}

							widget->separator();

							if (widget->checkbox_with_color("ESP Skeleton", &g_Globals.Visuals.Skeleton, g_Globals.Visuals.SkeletonColor, true))
							{
								Beep(g_Globals.Visuals.Skeleton ? 800 : 500, 45);
								notify->add_notify(g_Globals.Visuals.Skeleton ? "ESP Skeleton On" : "ESP Skeleton Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}

							widget->separator();

							if (widget->checkbox("ESP Level", &g_Globals.Visuals.Level))
							{
								Beep(g_Globals.Visuals.Level ? 800 : 500, 45);
								notify->add_notify(g_Globals.Visuals.Level ? "ESP Level On" : "ESP Level Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}

							widget->separator();

							if (widget->checkbox_with_color("ESP Line", &g_Globals.Visuals.Lines, g_Globals.Visuals.LinesColor, true))
							{
								Beep(g_Globals.Visuals.Lines ? 800 : 500, 45);
								notify->add_notify(g_Globals.Visuals.Lines ? "ESP Line On" : "ESP Line Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}

							if (g_Globals.Visuals.Lines)
							{
								widget->separator();

								if (widget->checkbox("Rainbow Lines", &g_Globals.Visuals.RainbowLines))
								{
									Beep(g_Globals.Visuals.RainbowLines ? 800 : 500, 45);
									notify->add_notify(g_Globals.Visuals.RainbowLines ? "Rainbow Lines On" : "Rainbow Lines Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
								}

								widget->separator();

								if (widget->checkbox("Glow Lines", &g_Globals.Visuals.GlowLines))
								{
									Beep(g_Globals.Visuals.GlowLines ? 800 : 500, 45);
									notify->add_notify(g_Globals.Visuals.GlowLines ? "Glow Lines On" : "Glow Lines Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
								}

								widget->separator();

								static std::vector<std::string> line_pos = { "Top Screen", "Bottom Screen" };
								int lineIdx = g_Globals.Visuals.EspLines - 1;
								if (lineIdx < 0 || lineIdx > 1) lineIdx = 0;
								if (widget->dropdown("Line Position", &lineIdx, line_pos, (int)line_pos.size()))
								{
									g_Globals.Visuals.EspLines = lineIdx + 1;
								}
							}
						}
						gui->end_child();

						gui->begin_child("esp_elements", ImVec2(col_w, 0));
						{
							if (widget->checkbox("ESP Health Bar", &g_Globals.Visuals.HealthBar))
							{
								Beep(g_Globals.Visuals.HealthBar ? 800 : 500, 45);
								notify->add_notify(g_Globals.Visuals.HealthBar ? "Health Bar On" : "Health Bar Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}

							if (g_Globals.Visuals.HealthBar)
							{
								widget->separator();
								static std::vector<std::string> healthBarTypes = {
									"None",
									"Left",
									"Right",
									"Bottom",
									"Text"
								};
								widget->dropdown("Health Bar Type", &g_Globals.Visuals.players_healthbar, healthBarTypes, (int)healthBarTypes.size());
							}

							widget->separator();

							if (widget->checkbox("ESP Weapon Name", &g_Globals.Visuals.ESPWeapon))
							{
								g_Globals.Visuals.esparmas = g_Globals.Visuals.ESPWeapon;
								Beep(g_Globals.Visuals.ESPWeapon ? 800 : 500, 45);
								notify->add_notify(g_Globals.Visuals.ESPWeapon ? "ESP Weapon Name On" : "ESP Weapon Name Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}

							widget->separator();

							if (widget->checkbox("Show Gun Icons", &g_Globals.Visuals.ESPWeaponIcon))
							{
								Beep(g_Globals.Visuals.ESPWeaponIcon ? 800 : 500, 45);
								notify->add_notify(g_Globals.Visuals.ESPWeaponIcon ? "Gun Icons On" : "Gun Icons Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}

							widget->separator();

							if (widget->checkbox("ESP Distance", &g_Globals.Visuals.Distance))
							{
								Beep(g_Globals.Visuals.Distance ? 800 : 500, 45);
								notify->add_notify(g_Globals.Visuals.Distance ? "ESP Distance On" : "ESP Distance Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}

							widget->separator();

							widget->slider_int("Max Distance", &g_Globals.Visuals.DistanceEsp, 10, 500, 1, "%d m");

							widget->separator();

							if (widget->checkbox("ESP Name", &g_Globals.Visuals.Name))
							{
								Beep(g_Globals.Visuals.Name ? 800 : 500, 45);
								notify->add_notify(g_Globals.Visuals.Name ? "ESP Name On" : "ESP Name Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}

							widget->separator();

							const float child_width = GetContentRegionAvail().x;
							widget->button("Refresh ESP", { child_width, SCALE(32) });
							if (ImGui::IsItemClicked())
							{
								FWork::Data::Refresh();
								notify->add_notify("ESP cache refreshed!", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}
						}
						gui->end_child();
					}
					gui->end_group();
				}
				ImGui::EndChild();

					gui->sameline();

					gui->begin_group();
					{
						gui->begin_child("esp_preview", ImVec2(col_w, avail_h));
						{
							DrawEspPreview(GetContentRegionAvail().x);
						}
						gui->end_child();
					}
					gui->end_group();
				}
				else if (var->c_selection.selection_active == 2) // Brutal & Look
				{
					// Left Column: BRUTAL Features
					gui->begin_group();
					{
						gui->begin_child("brutal_features");
						{
							if (!AimkillState::isServerConnected)
							{
								ImGui::PushFont(set->c_font.inter_medium[1]);
								ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Server Disconnected");
								ImGui::PopFont();
								ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Please connect server in Setting tab first.");
								widget->separator();
							}

							if (widget->checkbox("Fly Up", &AimkillState::s_flyUpNew))
							{
								if (AimkillState::isServerConnected)
									AimkillClient::Get().SendToggle(AimkillMode::FlyUpNew, AimkillState::s_flyUpNew, 0.0f, AimkillState::pkgNames[AimkillState::selectedPkg]);
								Beep(AimkillState::s_flyUpNew ? 800 : 500, 45);
								if (notify) notify->add_notify(AimkillState::s_flyUpNew ? "Fly Up On" : "Fly Up Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}
							widget->keybind("Fly Up Key", &AimkillState::s_flyUpNew_key);

							widget->separator();

							if (widget->checkbox("Tele Mark", &AimkillState::s_teleMark))
							{
								if (AimkillState::isServerConnected)
									AimkillClient::Get().SendToggle(AimkillMode::TeleMark, AimkillState::s_teleMark, 0.0f, AimkillState::pkgNames[AimkillState::selectedPkg]);
								Beep(AimkillState::s_teleMark ? 800 : 500, 45);
								if (notify) notify->add_notify(AimkillState::s_teleMark ? "Tele Mark On" : "Tele Mark Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}
							widget->keybind("Tele Mark Key", &AimkillState::s_teleMark_key);

							widget->separator();

							if (widget->checkbox("Stop Spt", &AimkillState::s_stopSpt))
							{
								if (AimkillState::isServerConnected)
									AimkillClient::Get().SendToggle(AimkillMode::StopSpt, AimkillState::s_stopSpt, 0.0f, AimkillState::pkgNames[AimkillState::selectedPkg]);
								Beep(AimkillState::s_stopSpt ? 800 : 500, 45);
								if (notify) notify->add_notify(AimkillState::s_stopSpt ? "Stop Spt On" : "Stop Spt Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}
							widget->keybind("Stop Spt Key", &AimkillState::s_stopSpt_key);

							widget->separator();

							if (widget->checkbox("Fly Hack", &AimkillState::s_flyHackNew))
							{
								if (AimkillState::isServerConnected) {
									AimkillClient::Get().SendToggle(AimkillMode::FlyHackNew, AimkillState::s_flyHackNew, (float)AimkillState::s_flyHackHeight, AimkillState::pkgNames[AimkillState::selectedPkg]);
									AimkillClient::Get().SendToggle(AimkillMode::FlyHackHeight, true, (float)AimkillState::s_flyHackHeight, AimkillState::pkgNames[AimkillState::selectedPkg]);
								}
								Beep(AimkillState::s_flyHackNew ? 800 : 500, 45);
								if (notify) notify->add_notify(AimkillState::s_flyHackNew ? "Fly Hack On" : "Fly Hack Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}
							widget->keybind("Fly Hack Key", &AimkillState::s_flyHackNew_key);

							widget->separator();

							static int s_lastFlyH = AimkillState::s_flyHackHeight;
							if (widget->slider_int("Fly Hack Height", &AimkillState::s_flyHackHeight, 15, 100, 1, "%d M"))
							{
								if (AimkillState::isServerConnected && AimkillState::s_flyHackHeight != s_lastFlyH) {
									AimkillClient::Get().SendToggle(AimkillMode::FlyHackHeight, true, (float)AimkillState::s_flyHackHeight, AimkillState::pkgNames[AimkillState::selectedPkg]);
									s_lastFlyH = AimkillState::s_flyHackHeight;
								}
							}

							widget->separator();

							if (widget->checkbox("Medikit Run", &AimkillState::s_medikitRun))
							{
								if (AimkillState::isServerConnected)
									AimkillClient::Get().SendToggle(AimkillMode::MedikitRun, AimkillState::s_medikitRun, 0.0f, AimkillState::pkgNames[AimkillState::selectedPkg]);
								Beep(AimkillState::s_medikitRun ? 800 : 500, 45);
								if (notify) notify->add_notify(AimkillState::s_medikitRun ? "Medikit Run On" : "Medikit Run Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}

							widget->separator();

							if (widget->checkbox("Wall Hack", &AimkillState::s_wallHack))
							{
								if (AimkillState::isServerConnected)
									AimkillClient::Get().SendToggle(AimkillMode::WallHack, AimkillState::s_wallHack, 0.0f, AimkillState::pkgNames[AimkillState::selectedPkg]);
								Beep(AimkillState::s_wallHack ? 800 : 500, 45);
								if (notify) notify->add_notify(AimkillState::s_wallHack ? "Wall Hack On" : "Wall Hack Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}

							widget->separator();

							if (widget->checkbox("Camera Up", &AimkillState::s_cameraUp))
							{
								if (AimkillState::isServerConnected)
									AimkillClient::Get().SendToggle(AimkillMode::CameraUp, AimkillState::s_cameraUp, 0.0f, AimkillState::pkgNames[AimkillState::selectedPkg]);
								Beep(AimkillState::s_cameraUp ? 800 : 500, 45);
								if (notify) notify->add_notify(AimkillState::s_cameraUp ? "Camera Up On" : "Camera Up Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}

							widget->separator();

							if (widget->checkbox("Auto Target", &AimkillState::s_autoTarget))
							{
								if (AimkillState::isServerConnected)
									AimkillClient::Get().SendToggle(AimkillMode::AutoTarget, AimkillState::s_autoTarget, 0.0f, AimkillState::pkgNames[AimkillState::selectedPkg]);
								Beep(AimkillState::s_autoTarget ? 800 : 500, 45);
								if (notify) notify->add_notify(AimkillState::s_autoTarget ? "Auto Target On" : "Auto Target Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}

							widget->separator();

							if (widget->checkbox("Black Sky", &AimkillState::s_blackSky))
							{
								if (AimkillState::isServerConnected)
									AimkillClient::Get().SendToggle(AimkillMode::BlackSky, AimkillState::s_blackSky, 0.0f, AimkillState::pkgNames[AimkillState::selectedPkg]);
								Beep(AimkillState::s_blackSky ? 800 : 500, 45);
								if (notify) notify->add_notify(AimkillState::s_blackSky ? "Black Sky On" : "Black Sky Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}

							widget->separator();

							if (widget->checkbox("Invisible", &AimkillState::s_invisible))
							{
								if (AimkillState::isServerConnected)
									AimkillClient::Get().SendToggle(AimkillMode::Invisible, AimkillState::s_invisible, 0.0f, AimkillState::pkgNames[AimkillState::selectedPkg]);
								Beep(AimkillState::s_invisible ? 800 : 500, 45);
								if (notify) notify->add_notify(AimkillState::s_invisible ? "Invisible On" : "Invisible Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}

							widget->separator();

							if (widget->checkbox("Invisible Kill", &AimkillState::s_invisibleKill))
							{
								if (AimkillState::isServerConnected)
									AimkillClient::Get().SendToggle(AimkillMode::InvisibleKill, AimkillState::s_invisibleKill, 0.0f, AimkillState::pkgNames[AimkillState::selectedPkg]);
								Beep(AimkillState::s_invisibleKill ? 800 : 500, 45);
								if (notify) notify->add_notify(AimkillState::s_invisibleKill ? "Invisible Kill On" : "Invisible Kill Off", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}
						}
						gui->end_child();
					}
					gui->end_group();

					gui->sameline();

					// Right Column: LOOK - Skin Changer
					gui->begin_group();
					{
						gui->begin_child("look_changer");
						{
							if (!AimkillState::isServerConnected)
							{
								ImGui::PushFont(set->c_font.inter_medium[1]);
								ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Server Disconnected");
								ImGui::PopFont();
								ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Please connect server in Setting tab first.");
								widget->separator();
							}

							if (widget->checkbox("Dreamspace", &AimkillState::s_lookDreamspace))
								AimkillState::ApplyLookToggle(&AimkillState::s_lookDreamspace, AimkillMode::LookDreamspace, "Dreamspace");
							widget->separator();

							if (widget->checkbox("Rampage", &AimkillState::s_lookRampage))
								AimkillState::ApplyLookToggle(&AimkillState::s_lookRampage, AimkillMode::LookRampage, "Rampage");
							widget->separator();

							if (widget->checkbox("Itachi", &AimkillState::s_lookItachi))
								AimkillState::ApplyLookToggle(&AimkillState::s_lookItachi, AimkillMode::LookItachi, "Itachi");
							widget->separator();

							if (widget->checkbox("Midnight Ace", &AimkillState::s_lookMidnightAce))
								AimkillState::ApplyLookToggle(&AimkillState::s_lookMidnightAce, AimkillMode::LookMidnightAce, "Midnight Ace");
							widget->separator();

							if (widget->checkbox("Aurora", &AimkillState::s_lookAurora))
								AimkillState::ApplyLookToggle(&AimkillState::s_lookAurora, AimkillMode::LookAurora, "Aurora");
							widget->separator();

							if (widget->checkbox("Naruto's Ascent", &AimkillState::s_lookNarutoAscent))
								AimkillState::ApplyLookToggle(&AimkillState::s_lookNarutoAscent, AimkillMode::LookNarutoAscent, "Naruto's Ascent");
							widget->separator();

							if (widget->checkbox("Last Paradox", &AimkillState::s_lookLastParadox))
								AimkillState::ApplyLookToggle(&AimkillState::s_lookLastParadox, AimkillMode::LookLastParadox, "Last Paradox");
							widget->separator();

							if (widget->checkbox("Frostfire", &AimkillState::s_lookFrostfire))
								AimkillState::ApplyLookToggle(&AimkillState::s_lookFrostfire, AimkillMode::LookFrostfire, "Frostfire");
							widget->separator();

							if (widget->checkbox("Scorpio", &AimkillState::s_lookScorpio))
								AimkillState::ApplyLookToggle(&AimkillState::s_lookScorpio, AimkillMode::LookScorpio, "Scorpio");
							widget->separator();

							if (widget->checkbox("Devil Trigger", &AimkillState::s_lookDevilTrigger))
								AimkillState::ApplyLookToggle(&AimkillState::s_lookDevilTrigger, AimkillMode::LookDevilTrigger, "Devil Trigger");
							widget->separator();

							if (widget->checkbox("Cannibal Havoc", &AimkillState::s_lookCannibalHavoc))
								AimkillState::ApplyLookToggle(&AimkillState::s_lookCannibalHavoc, AimkillMode::LookCannibalHavoc, "Cannibal Havoc");
						}
						gui->end_child();
					}
					gui->end_group();
				}
				else if (var->c_selection.selection_active == 3) // Settings
				{
					gui->begin_group();
					{
						// 1. Aimkill Server Connect
						gui->begin_child("server_connect");
						{
							ImGui::PushFont(set->c_font.inter_medium[1]);
							if (AimkillState::isServerConnected) {
								ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.4f, 1.0f), "Server : Connected");
							} else if (AimkillState::isServerConnecting) {
								ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Server : %s", AimkillState::server_btn.c_str());
							} else {
								ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Server : Disconnected");
							}
							ImGui::PopFont();

							widget->separator();

							static std::vector<std::string> gameList = { "com.dts.freefireth", "com.dts.freefiremax" };
							widget->dropdown("Target Game", &AimkillState::selectedPkg, gameList, 2);

							widget->separator();

							widget->text_field("127.0.0.1:5555", "ADB Address", AimkillState::deviceAddr, sizeof(AimkillState::deviceAddr), { GetContentRegionAvail().x, SCALE(35) });

							widget->separator();

							const float btn_w = GetContentRegionAvail().x;
							if (!AimkillState::isServerConnecting && !AimkillState::isServerConnected) {
								if (widget->button(AimkillState::server_btn, { btn_w, SCALE(35) }) || ImGui::IsItemClicked()) {
									AimkillState::ConnectServerAsync();
								}
							} else if (AimkillState::isServerConnecting) {
								widget->button(AimkillState::server_btn, { btn_w, SCALE(35) });
							} else if (AimkillState::isServerConnected) {
								if (widget->button("Disconnect Server", { btn_w, SCALE(35) }) || ImGui::IsItemClicked()) {
									AimkillState::DisconnectServer();
								}
							}
						}
						gui->end_child();

						// 2. Lib Management
						gui->begin_child("lib_management");
						{
							if (widget->checkbox("Connect Lib", &var->c_settings.connect_lib))
							{
								if (var->c_settings.connect_lib)
								{
									if (!FWork::g_modeSelected.load() && !FWork::g_isConnecting.load())
										FWork::Data::TriggerRefresh();
								}
								else
								{
									FWork::Data::DisconnectEngine();
									notify->add_notify("ESP Disconnected", 4, static_cast<notify_position>(var->c_notify.notify_position));
								}
							}

							widget->separator();

							const float width = GetContentRegionAvail().x;
							static bool autoRefresh = false;
							static float lastAutoRefreshTime = 0.0f;

							if (autoRefresh && FWork::g_modeSelected.load() && !FWork::g_isConnecting.load())
							{
								float now = (float)ImGui::GetTime();
								if (now - lastAutoRefreshTime >= 4.0f)
								{
									lastAutoRefreshTime = now;
									FWork::Data::TriggerRefresh();
								}
							}

							if (!FWork::g_modeSelected.load())
							{
								ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "pVM     : %s", FWork::g_pvm_str.c_str());
								ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "pVCpu   : %s", FWork::g_pvcpu_str.c_str());
								ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "CPU     : %s", FWork::g_cpu_count_str.c_str());

								ImGui::Dummy(ImVec2(0.0f, 5.0f));

								if (!FWork::g_isConnecting.load())
								{
									if (widget->button(FWork::g_connectStatus.c_str(), { width, SCALE(35) }) || ImGui::IsItemClicked())
									{
										FWork::Data::TriggerRefresh();
									}
								}
								else
								{
									ImGui::TextColored(ImVec4(1, 1, 0, 1), "Status: %s", FWork::g_connectStatus.c_str());
								}
							}
							else
							{
								ImGui::TextColored(ImVec4(0, 1, 0, 1), "Connected");

								if (widget->button("Stop ESP", { width, SCALE(35) }) || ImGui::IsItemClicked())
								{
									var->c_settings.connect_lib = false;
									FWork::Data::DisconnectEngine();
									notify->add_notify("ESP Disconnected", 4, static_cast<notify_position>(var->c_notify.notify_position));
								}
							}

							ImGui::Dummy(ImVec2(0.0f, 5.0f));

							if (widget->button("Refresh ESP", { width, SCALE(35) }) || ImGui::IsItemClicked())
							{
								FWork::Data::TriggerRefresh();
							}

							ImGui::Dummy(ImVec2(0.0f, 3.0f));
							widget->checkbox("Auto Refresh", &autoRefresh);

							static bool wasConnected = false;
							bool nowConnected = FWork::g_modeSelected.load() && !FWork::g_isConnecting.load();
							if (nowConnected && !wasConnected)
							{
								var->c_settings.connect_lib = true;
								var->c_esp.esp = true;
								var->c_esp.box_selection = 1;
								var->c_skeleton.snaplines_selection = 1;
								var->c_skeleton.nickname = true;
								var->c_esp.healthbar = true;
								var->c_esp.distance = true;
								var->c_skeleton.skeleton = false;
								var->c_skeleton.weapon = false;
								var->c_esp.headdot = false;
								var->c_esp.ingame_radar = false;

								notify->add_notify("Memory Engine connected successfully!", 4, static_cast<notify_position>(var->c_notify.notify_position));
							}
							else if (!nowConnected && wasConnected)
							{
								var->c_settings.connect_lib = false;
							}
							wasConnected = nowConnected;
						}
						gui->end_child();
					}
					gui->end_group();

					gui->sameline();

					gui->begin_group();
					{
						// 3. Theme
						gui->begin_child("theme");
						{
							static bool theme_color_enabled = true;
							widget->checkbox_with_color("UI Theme Color", &theme_color_enabled, (float*)&clr->c_other_clr.accent_clr, false);

							widget->separator();

							widget->slider_int("UI Brightness", &var->c_glow.power, 0, 100, 1, "%d%%");

							widget->separator();

							static char buf[128] = "Default User";
							widget->text_field("Custom Tag", "M", buf, 128, { GetContentRegionAvail().x, SCALE(35) });
						}
						gui->end_child();

						// 4. Panel Management
						gui->begin_child("panel_management");
						{
							widget->checkbox_with_key("Hide panel", &var->c_panel.enable_hide_key, &var->c_panel.hide_key, &var->c_panel.hide_holding, &var->c_panel.hide_value, &var->c_panel.hide_show_binds);

							widget->separator();

							widget->checkbox_with_key("Exit panel", &var->c_panel.enable_exit_key, &var->c_panel.exit_key, &var->c_panel.exit_holding, &var->c_panel.exit_value, &var->c_panel.exit_show_binds);

							widget->separator();

							const float width = GetContentRegionAvail().x;

							widget->button("Hide UI Now", { (width - style->ItemSpacing.x) / 2, SCALE(35) });
							if (ImGui::IsItemClicked())
							{
								var->c_panel.request_hide = true;
							}

							gui->sameline();

							widget->button("Exit Panel Now", { (width - style->ItemSpacing.x) / 2, SCALE(35) });
							if (ImGui::IsItemClicked())
							{
								var->c_panel.request_exit = true;
							}

							widget->separator();

							widget->button("Reset Color", { (width - style->ItemSpacing.x) / 2, SCALE(35) });
							if (ImGui::IsItemClicked())
							{
								clr->c_other_clr.accent_clr = ImColor(142, 132, 255, 255);
							}

							gui->sameline();

							widget->button("Max Bright", { (width - style->ItemSpacing.x) / 2, SCALE(35) });
							if (ImGui::IsItemClicked())
							{
								var->c_glow.power = 100;
							}
						}
						gui->end_child();
					}
					gui->end_group();
				}
			}
			gui->end_content();

			gui->pop_style_var(1);

		}
		gui->end();

	}
do_end_frame:
	gui->end_frame();

}