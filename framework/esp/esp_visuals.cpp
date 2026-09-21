#include "esp_visuals.h"
#include "esp_globals.h"
#include "name_gun.h"
#include "../math/world_to_screen.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>

namespace ESP {

static inline bool IsFinite2(float x, float y)
{
    return std::isfinite(x) && std::isfinite(y);
}

static inline float SafeClamp(float v, float lo, float hi)
{
    if (!(hi >= lo)) return v;
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void DrawCornerBox(ImDrawList* drawList, float x, float y, float w, float h, ImU32 col, float thickness = 1.2f)
{
    float lineW = w * 0.25f;
    float lineH = h * 0.25f;

    // Top-left
    drawList->AddLine(ImVec2(x, y), ImVec2(x + lineW, y), col, thickness);
    drawList->AddLine(ImVec2(x, y), ImVec2(x, y + lineH), col, thickness);

    // Top-right
    drawList->AddLine(ImVec2(x + w, y), ImVec2(x + w - lineW, y), col, thickness);
    drawList->AddLine(ImVec2(x + w, y), ImVec2(x + w, y + lineH), col, thickness);

    // Bottom-left
    drawList->AddLine(ImVec2(x, y + h), ImVec2(x + lineW, y + h), col, thickness);
    drawList->AddLine(ImVec2(x, y + h), ImVec2(x, y + h - lineH), col, thickness);

    // Bottom-right
    drawList->AddLine(ImVec2(x + w, y + h), ImVec2(x + w - lineW, y + h), col, thickness);
    drawList->AddLine(ImVec2(x + w, y + h), ImVec2(x + w, y + h - lineH), col, thickness);
}

static void DrawHealthBar(ImDrawList* drawList, short currentHp, short maxHp, float x, float y, float w, float h, int position)
{
    if (maxHp <= 0) maxHp = 200;
    if (currentHp < 0) currentHp = 0;
    if (currentHp > maxHp) currentHp = maxHp;

    float percent = SafeClamp((float)currentHp / (float)maxHp, 0.0f, 1.0f);
    ImU32 hpColor = (percent > 0.6f) ? IM_COL32(76, 217, 100, 255) :
                    (percent > 0.3f) ? IM_COL32(255, 204, 0, 255) :
                                       IM_COL32(255, 59, 48, 255);

    if (position == 0) // Top
    {
        float barH = 3.5f;
        drawList->AddRectFilled(ImVec2(x - 1, y - barH - 4), ImVec2(x + w + 1, y - 2), IM_COL32(0, 0, 0, 180), 1.0f);
        drawList->AddRectFilled(ImVec2(x, y - barH - 3), ImVec2(x + (w * percent), y - 3), hpColor, 1.0f);
    }
    else if (position == 1) // Left
    {
        float barW = 3.5f;
        drawList->AddRectFilled(ImVec2(x - barW - 4, y - 1), ImVec2(x - 2, y + h + 1), IM_COL32(0, 0, 0, 180), 1.0f);
        float fillH = h * percent;
        drawList->AddRectFilled(ImVec2(x - barW - 3, y + h - fillH), ImVec2(x - 3, y + h), hpColor, 1.0f);
    }
    else // Below
    {
        float barH = 3.5f;
        drawList->AddRectFilled(ImVec2(x - 1, y + h + 3), ImVec2(x + w + 1, y + h + barH + 5), IM_COL32(0, 0, 0, 180), 1.0f);
        drawList->AddRectFilled(ImVec2(x, y + h + 4), ImVec2(x + (w * percent), y + h + barH + 4), hpColor, 1.0f);
    }
}

void DrawCrosshairRadar()
{
    if (!g_Globals.Visuals.Radar) return;

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    if (!drawList) return;

    ImVec2 center = ImVec2(g_Globals.Visuals.RadarPosX, g_Globals.Visuals.RadarPosY);
    float size = g_Globals.Visuals.RadarSize;
    float halfSize = size * 0.5f;

    // Background
    drawList->AddRectFilled(ImVec2(center.x - halfSize, center.y - halfSize),
                            ImVec2(center.x + halfSize, center.y + halfSize),
                            IM_COL32(18, 18, 24, 210), 6.0f);
    drawList->AddRect(ImVec2(center.x - halfSize, center.y - halfSize),
                      ImVec2(center.x + halfSize, center.y + halfSize),
                      IM_COL32(80, 80, 100, 180), 6.0f, 0, 1.0f);

    // Cross axes
    drawList->AddLine(ImVec2(center.x - halfSize + 5, center.y), ImVec2(center.x + halfSize - 5, center.y), IM_COL32(120, 120, 140, 100));
    drawList->AddLine(ImVec2(center.x, center.y - halfSize + 5), ImVec2(center.x, center.y + halfSize - 5), IM_COL32(120, 120, 140, 100));
    drawList->AddCircleFilled(center, 3.0f, IM_COL32(255, 255, 255, 255));

    // Entities on radar
    std::vector<std::pair<uint64_t, Player>> entitiesSnapshot;
    {
        std::shared_lock<std::shared_mutex> lock(g_Globals.EspConfig.EntitiesMutex);
        if (g_Globals.EspConfig.InMatch)
        {
            entitiesSnapshot.reserve(g_Globals.EspConfig.Entities.size());
            for (const auto& kv : g_Globals.EspConfig.Entities)
                entitiesSnapshot.emplace_back(kv.first, kv.second);
        }
    }

    Vector3 camPos = g_Globals.EspConfig.MainCamera;
    float radarRange = (g_Globals.Visuals.RadarRange > 10.0f) ? g_Globals.Visuals.RadarRange : 150.0f;

    for (const auto& [id, player] : entitiesSnapshot)
    {
        if (player.IsDead || !player.IsKnown) continue;

        Vector3 diff = player.Root - camPos;
        float dist = std::sqrt(diff.X * diff.X + diff.Z * diff.Z);
        if (dist > radarRange) continue;

        float relX = (diff.X / radarRange) * (halfSize - 8.0f);
        float relY = (diff.Z / radarRange) * (halfSize - 8.0f);

        ImVec2 pt = ImVec2(center.x + relX, center.y - relY);
        drawList->AddCircleFilled(pt, 3.5f, IM_COL32(245, 60, 60, 255));
        drawList->AddCircle(pt, 3.5f, IM_COL32(255, 255, 255, 200), 0, 1.0f);
    }
}

void Players()
{
    if (!g_Globals.Visuals.Enable) return;
    if (!g_Globals.EspConfig.Matrix || !g_Globals.EspConfig.InMatch) return;

    ImGuiIO& io = ImGui::GetIO();
    float screenW = io.DisplaySize.x;
    float screenH = io.DisplaySize.y;
    if (screenW < 100.0f || screenH < 100.0f) return;

    g_Globals.EspConfig.Width = (int)screenW;
    g_Globals.EspConfig.Height = (int)screenH;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList) return;

    // Snapshot under shared lock to prevent thread race crashes
    std::vector<std::pair<uint64_t, Player>> entitiesSnapshot;
    {
        std::shared_lock<std::shared_mutex> lock(g_Globals.EspConfig.EntitiesMutex);
        if (!g_Globals.EspConfig.InMatch || !g_Globals.EspConfig.Matrix)
            return;
        entitiesSnapshot.reserve(g_Globals.EspConfig.Entities.size());
        for (const auto& kv : g_Globals.EspConfig.Entities)
            entitiesSnapshot.emplace_back(kv.first, kv.second);
    }

    auto W2SFunc = [&](const Vector3& pos) -> ImVec2
    {
        return W2S::WorldToScreenImVec2(g_Globals.EspConfig.ViewMatrix, pos, (int)screenW, (int)screenH);
    };

    ImU32 boxCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
        g_Globals.Visuals.BoxColor[0], g_Globals.Visuals.BoxColor[1],
        g_Globals.Visuals.BoxColor[2], g_Globals.Visuals.BoxColor[3]
    ));

    ImU32 skelCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
        g_Globals.Visuals.SkeletonColor[0], g_Globals.Visuals.SkeletonColor[1],
        g_Globals.Visuals.SkeletonColor[2], g_Globals.Visuals.SkeletonColor[3]
    ));

    ImU32 lineCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
        g_Globals.Visuals.LinesColor[0], g_Globals.Visuals.LinesColor[1],
        g_Globals.Visuals.LinesColor[2], g_Globals.Visuals.LinesColor[3]
    ));

    for (const auto& [entityID, player] : entitiesSnapshot)
    {
        if (player.IsDead || !player.IsKnown || player.Address == 0)
            continue;

        if (player.Distance > g_Globals.Visuals.DistanceEsp)
            continue;

        ImVec2 headPos = W2SFunc(player.Head);
        ImVec2 rootPos = W2SFunc(player.Root);

        // Strict screen boundary test
        if (!IsFinite2(headPos.x, headPos.y) || !IsFinite2(rootPos.x, rootPos.y))
            continue;
        if (headPos.x <= 0.0f || headPos.y <= 0.0f || rootPos.x <= 0.0f || rootPos.y <= 0.0f)
            continue;
        if (headPos.x >= screenW || rootPos.x >= screenW || headPos.y >= screenH || rootPos.y >= screenH)
            continue;

        // Box Dimensions
        float boxH = fabsf(rootPos.y - headPos.y);
        if (boxH < 8.0f) boxH = 40.0f;
        float boxW = boxH * 0.55f;
        float boxX = headPos.x - (boxW * 0.5f);
        float boxY = headPos.y - (boxH * 0.12f);

        // 1. Box ESP
        if (g_Globals.Visuals.Box)
        {
            if (g_Globals.Visuals.FilledBox)
            {
                ImU32 fillCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
                    g_Globals.Visuals.Filledboxcolor[0], g_Globals.Visuals.Filledboxcolor[1],
                    g_Globals.Visuals.Filledboxcolor[2], g_Globals.Visuals.Filledboxcolor[3]));
                drawList->AddRectFilled(ImVec2(boxX, boxY), ImVec2(boxX + boxW, boxY + boxH), fillCol, 2.0f);
            }

            if (g_Globals.Visuals.players_box == 1) // Corner Box
            {
                DrawCornerBox(drawList, boxX, boxY, boxW, boxH, boxCol, 1.4f);
            }
            else // Normal 2D Box
            {
                drawList->AddRect(ImVec2(boxX, boxY), ImVec2(boxX + boxW, boxY + boxH), boxCol, 2.0f, 0, 1.2f);
                drawList->AddRect(ImVec2(boxX - 1, boxY - 1), ImVec2(boxX + boxW + 1, boxY + boxH + 1), IM_COL32(0, 0, 0, 160), 2.0f, 0, 1.0f);
            }
        }

        // 2. Skeleton Bones
        if (g_Globals.Visuals.Skeleton)
        {
            ImVec2 neckPos = W2SFunc(player.Neck);
            ImVec2 lShoulder = W2SFunc(player.LeftShoulder);
            ImVec2 rShoulder = W2SFunc(player.RightShoulder);
            ImVec2 lElbow = W2SFunc(player.LeftElbow);
            ImVec2 rElbow = W2SFunc(player.RightElbow);
            ImVec2 lWrist = W2SFunc(player.LeftWrist);
            ImVec2 rWrist = W2SFunc(player.RightWrist);
            ImVec2 hip = W2SFunc(player.Hip);
            ImVec2 lAnkle = W2SFunc(player.LeftAnkle);
            ImVec2 rAnkle = W2SFunc(player.RightAnkle);
            ImVec2 lFoot = W2SFunc(player.LeftFoot);
            ImVec2 rFoot = W2SFunc(player.RightFoot);

            float thick = g_Globals.Visuals.SkeletonThickness;

            auto DrawBoneLine = [&](ImVec2 a, ImVec2 b) {
                if (IsFinite2(a.x, a.y) && IsFinite2(b.x, b.y) && a.x > 0 && a.y > 0 && b.x > 0 && b.y > 0)
                {
                    drawList->AddLine(a, b, skelCol, thick);
                }
            };

            DrawBoneLine(headPos, neckPos);
            DrawBoneLine(neckPos, lShoulder);
            DrawBoneLine(lShoulder, lElbow);
            DrawBoneLine(lElbow, lWrist);

            DrawBoneLine(neckPos, rShoulder);
            DrawBoneLine(rShoulder, rElbow);
            DrawBoneLine(rElbow, rWrist);

            DrawBoneLine(neckPos, hip);
            DrawBoneLine(hip, lAnkle);
            DrawBoneLine(lAnkle, lFoot);

            DrawBoneLine(hip, rAnkle);
            DrawBoneLine(rAnkle, rFoot);
        }

        // 3. Head Dot / Circle
        if (g_Globals.Visuals.HeadDot && IsFinite2(headPos.x, headPos.y))
        {
            ImU32 headDotCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
                g_Globals.Visuals.HeadDotColor[0], g_Globals.Visuals.HeadDotColor[1],
                g_Globals.Visuals.HeadDotColor[2], g_Globals.Visuals.HeadDotColor[3]));
            drawList->AddCircleFilled(headPos, g_Globals.Visuals.HeadDotSize, headDotCol);
            drawList->AddCircle(headPos, g_Globals.Visuals.HeadDotSize + 0.5f, IM_COL32(0, 0, 0, 180), 0, 1.0f);
        }

        // 4. Snaplines
        if (g_Globals.Visuals.Lines)
        {
            ImVec2 lineStart;
            if (g_Globals.Visuals.EspLines == 0)      lineStart = ImVec2(screenW * 0.5f, 0.0f); // Top
            else if (g_Globals.Visuals.EspLines == 1) lineStart = ImVec2(screenW * 0.5f, screenH); // Bottom
            else if (g_Globals.Visuals.EspLines == 2) lineStart = ImVec2(0.0f, screenH * 0.5f); // Left
            else if (g_Globals.Visuals.EspLines == 3) lineStart = ImVec2(screenW, screenH * 0.5f); // Right
            else                                     lineStart = ImVec2(screenW * 0.5f, screenH * 0.5f); // Crosshair

            drawList->AddLine(lineStart, rootPos, lineCol, 1.1f);
        }

        // 5. Health Bar
        if (g_Globals.Visuals.HealthBar)
        {
            DrawHealthBar(drawList, player.Health, 200, boxX, boxY, boxW, boxH, g_Globals.Visuals.players_healthbar);
        }

        // 6. Name, Weapon & Distance Text Tags
        float textY = boxY - 14.0f;

        // Player Name
        if (g_Globals.Visuals.Name && !player.Name.empty())
        {
            std::string nameStr = player.Name;
            ImVec2 txtSz = ImGui::CalcTextSize(nameStr.c_str());
            ImVec2 txtPos = ImVec2(boxX + (boxW - txtSz.x) * 0.5f, textY);

            drawList->AddRectFilled(ImVec2(txtPos.x - 3, txtPos.y - 1), ImVec2(txtPos.x + txtSz.x + 3, txtPos.y + txtSz.y + 1), IM_COL32(10, 10, 14, 180), 2.0f);
            drawList->AddText(txtPos, IM_COL32(255, 255, 255, 255), nameStr.c_str());
            textY -= (txtSz.y + 3.0f);
        }

        // Bottom Tags: Weapon & Distance
        float bottomY = boxY + boxH + ((g_Globals.Visuals.HealthBar && g_Globals.Visuals.players_healthbar == 2) ? 8.0f : 3.0f);

        if (g_Globals.Visuals.ESPWeapon && player.WeaponID != 0)
        {
            std::string gunName = Namegun::GetGunName(player.WeaponID);
            if (!gunName.empty())
            {
                ImVec2 txtSz = ImGui::CalcTextSize(gunName.c_str());
                ImVec2 txtPos = ImVec2(boxX + (boxW - txtSz.x) * 0.5f, bottomY);

                drawList->AddRectFilled(ImVec2(txtPos.x - 3, txtPos.y - 1), ImVec2(txtPos.x + txtSz.x + 3, txtPos.y + txtSz.y + 1), IM_COL32(10, 10, 14, 180), 2.0f);
                drawList->AddText(txtPos, IM_COL32(255, 215, 0, 255), gunName.c_str());
                bottomY += txtSz.y + 2.0f;
            }
        }

        if (g_Globals.Visuals.Distance)
        {
            std::string distStr = "[" + std::to_string(player.Distance) + "m]";
            ImVec2 txtSz = ImGui::CalcTextSize(distStr.c_str());
            ImVec2 txtPos = ImVec2(boxX + (boxW - txtSz.x) * 0.5f, bottomY);

            drawList->AddRectFilled(ImVec2(txtPos.x - 3, txtPos.y - 1), ImVec2(txtPos.x + txtSz.x + 3, txtPos.y + txtSz.y + 1), IM_COL32(10, 10, 14, 180), 2.0f);
            drawList->AddText(txtPos, IM_COL32(200, 200, 220, 255), distStr.c_str());
        }
    }

    DrawCrosshairRadar();
}

void Render()
{
    Players();
}

} // namespace ESP
