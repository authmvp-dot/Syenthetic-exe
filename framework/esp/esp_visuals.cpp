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
#include <shared_mutex>

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

ImVec4 HSVtoRGB(float h, float s, float v)
{
    float r = 0, g = 0, b = 0;
    int i = static_cast<int>(h * 6);
    float f = h * 6 - i;
    float p = v * (1 - s);
    float q = v * (1 - f * s);
    float t = v * (1 - (1 - f) * s);

    switch (i % 6) {
    case 0: r = v, g = t, b = p; break;
    case 1: r = q, g = v, b = p; break;
    case 2: r = p, g = v, b = t; break;
    case 3: r = p, g = q, b = v; break;
    case 4: r = t, g = p, b = v; break;
    case 5: r = v, g = p, b = q; break;
    default: r = g = b = 0; break;
    }

    return ImVec4(r, g, b, 1.0f);
}

void DrawGlowLine(const ImVec2& start, const ImVec2& end, ImU32 color, float thickness, float glowRadius, float feather)
{
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList) return;
    if (!IsFinite2(start.x, start.y) || !IsFinite2(end.x, end.y)) return;

    float coreThickness = thickness;
    if (coreThickness < 0.5f) coreThickness = 1.0f;
    if (coreThickness > 3.0f) coreThickness = 3.0f;

    // Soft glow (2 layers) + clean main line
    ImVec4 colorVec = ImGui::ColorConvertU32ToFloat4(color);
    for (int i = 2; i >= 1; --i)
    {
        float alpha = 0.12f * (float)i;
        ImU32 glowColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(colorVec.x, colorVec.y, colorVec.z, alpha));
        drawList->AddLine(start, end, glowColor, coreThickness + (float)i * 0.8f);
    }

    drawList->AddLine(start, end, color, coreThickness);
    drawList->AddCircleFilled(start, 2.5f, color, 8);
}

void DrawGlowLineGradient(const ImVec2& start, const ImVec2& end, float thickness, float glowRadius, float feather)
{
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList) return;
    if (!IsFinite2(start.x, start.y) || !IsFinite2(end.x, end.y)) return;
    if (feather < 0.01f) feather = 0.10f;

    const int segments = 16;
    ImVec2 diff(
        (end.x - start.x) / segments,
        (end.y - start.y) / segments
    );

    float time = (float)ImGui::GetTime();
    float speed = 0.3f;

    int glowLayers = (int)(glowRadius / feather);
    if (glowLayers > 2) glowLayers = 2;
    if (glowLayers < 1) glowLayers = 1;

    float coreThickness = thickness < 0.5f ? 1.0f : thickness;

    for (int i = 0; i < segments; i++)
    {
        ImVec2 segStart(start.x + diff.x * i, start.y + diff.y * i);
        ImVec2 segEnd(start.x + diff.x * (i + 1), start.y + diff.y * (i + 1));

        float t = (float)i / segments;
        float hue = fmodf(t + time * speed, 1.0f);
        ImVec4 colorVec = HSVtoRGB(hue, 1.f, 1.f);

        for (int g = glowLayers; g > 0; g--)
        {
            float alpha = (float)g / glowLayers * 0.12f;
            ImU32 glowColor = ImGui::ColorConvertFloat4ToU32(
                ImVec4(colorVec.x, colorVec.y, colorVec.z, alpha)
            );
            drawList->AddLine(segStart, segEnd, glowColor, coreThickness + g);
        }

        ImU32 mainColor = ImGui::ColorConvertFloat4ToU32(colorVec);
        drawList->AddLine(segStart, segEnd, mainColor, coreThickness);
    }
}

void DrawGlowCorneredBox(float x, float y, float w, float h, ImColor color, float thickness, float glowRadius, float feather, bool fillEnabled)
{
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList || !IsFinite2(x, y) || !IsFinite2(w, h)) return;
    if (w < 4.0f || h < 4.0f) return;

    if (fillEnabled)
    {
        drawList->AddRectFilled(
            ImVec2(x, y),
            ImVec2(x + w, y + h),
            ImColor(
                g_Globals.Visuals.FillColor[0],
                g_Globals.Visuals.FillColor[1],
                g_Globals.Visuals.FillColor[2],
                g_Globals.Visuals.FillColor[3]
            ),
            5.0f
        );
    }

    ImU32 boxColorU32 = color;
    float lineW = w / 3.0f;
    float lineH = h / 3.0f;
    float t = (thickness > 0.5f) ? thickness : 1.2f;

    // Top Left
    drawList->AddLine(ImVec2(x, y - t * 0.5f), ImVec2(x, y + lineH), boxColorU32, t);
    drawList->AddLine(ImVec2(x - t * 0.5f, y), ImVec2(x + lineW, y), boxColorU32, t);

    // Top Right
    drawList->AddLine(ImVec2(x + w - lineW, y), ImVec2(x + w + t * 0.5f, y), boxColorU32, t);
    drawList->AddLine(ImVec2(x + w, y - t * 0.5f), ImVec2(x + w, y + lineH), boxColorU32, t);

    // Bottom Left
    drawList->AddLine(ImVec2(x, y + h - lineH), ImVec2(x, y + h + t * 0.5f), boxColorU32, t);
    drawList->AddLine(ImVec2(x - t * 0.5f, y + h), ImVec2(x + lineW, y + h), boxColorU32, t);

    // Bottom Right
    drawList->AddLine(ImVec2(x + w - lineW, y + h), ImVec2(x + w + t * 0.5f, y + h), boxColorU32, t);
    drawList->AddLine(ImVec2(x + w, y + h - lineH), ImVec2(x + w, y + h + t * 0.5f), boxColorU32, t);
}

void DrawFullBox(float x, float y, float w, float h, ImColor color, float thickness)
{
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList || !IsFinite2(x, y) || !IsFinite2(w, h)) return;
    if (w < 2.0f || h < 2.0f) return;

    float t = (thickness > 0.5f) ? thickness : 1.5f;

    if (g_Globals.Visuals.FillColorBox) {
        drawList->AddRectFilled(
            ImVec2(x, y),
            ImVec2(x + w, y + h),
            ImColor(
                g_Globals.Visuals.FillColor[0],
                g_Globals.Visuals.FillColor[1],
                g_Globals.Visuals.FillColor[2],
                g_Globals.Visuals.FillColor[3]
            ),
            5.0f
        );
    }

    drawList->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), color, 5.0f, 0, t);
}

static void DrawMvpGlowHealthBar(ImDrawList* drawList, ImVec2 min, ImVec2 max, ImU32 color)
{
    if (!drawList) return;
    ImVec4 c = ImGui::ColorConvertU32ToFloat4(color);
    for (int i = 2; i >= 1; i--) {
        float expand = i * 0.55f;
        float alpha = 0.06f + 0.05f * (3 - i);
        drawList->AddRectFilled(
            ImVec2(min.x - expand, min.y - expand * 0.2f),
            ImVec2(max.x + expand, max.y + expand * 0.2f),
            ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, alpha)),
            1.0f);
    }
    drawList->AddRectFilled(min, max, color, 1.0f);
}

void DrawVerticalHealthBar(short CurrentHealth, short MaxHealth, ImVec2 Position, float TotalHeight)
{
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    if (!drawList || MaxHealth <= 0 || TotalHeight <= 1.0f || !IsFinite2(Position.x, Position.y)) return;

    float healthPercent = static_cast<float>(CurrentHealth) / static_cast<float>(MaxHealth);
    healthPercent = SafeClamp(healthPercent, 0.0f, 1.0f);
    const float barWidth = 3.0f;
    float filledHeight = TotalHeight * healthPercent;
    float top = Position.y + (TotalHeight - filledHeight);

    ImU32 healthColor =
        (healthPercent > 0.66f) ? IM_COL32(46, 204, 113, 255) :
        (healthPercent > 0.33f) ? IM_COL32(241, 196, 15, 255) :
                                  IM_COL32(231, 76, 60, 255);

    drawList->AddRectFilled(
        ImVec2(Position.x, Position.y),
        ImVec2(Position.x + barWidth, Position.y + TotalHeight),
        IM_COL32(80, 80, 80, 120), 1.0f);

    drawList->AddRectFilled(
        ImVec2(Position.x, top),
        ImVec2(Position.x + barWidth, Position.y + TotalHeight),
        healthColor, 1.0f);
}

void DrawHorizontalHealthBarTop(short CurrentHealth, short MaxHealth, float x, float y, float w)
{
    ImDrawList* DrawList = ImGui::GetForegroundDrawList();
    if (!DrawList || MaxHealth <= 0 || w <= 1.0f || !IsFinite2(x, y)) return;

    float healthPercent = static_cast<float>(CurrentHealth) / static_cast<float>(MaxHealth);
    healthPercent = SafeClamp(healthPercent, 0.0f, 1.0f);
    float healthBarHeight = 3.0f;
    float filledW = w * healthPercent;

    ImU32 healthColor =
        (healthPercent > 0.66f) ? IM_COL32(46, 204, 113, 255) :
        (healthPercent > 0.33f) ? IM_COL32(241, 196, 15, 255) :
                                  IM_COL32(231, 76, 60, 255);

    DrawList->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + healthBarHeight), IM_COL32(80, 80, 80, 120), 1.0f);
    DrawList->AddRectFilled(ImVec2(x, y), ImVec2(x + filledW, y + healthBarHeight), healthColor, 1.0f);
}

void DrawHealthBarBelowFullBox(short CurrentHealth, short MaxHealth, float x, float y, float w, float h)
{
    ImDrawList* DrawList = ImGui::GetForegroundDrawList();
    if (!DrawList || MaxHealth <= 0 || w <= 1.0f || !IsFinite2(x, y)) return;

    float healthPercent = static_cast<float>(CurrentHealth) / static_cast<float>(MaxHealth);
    healthPercent = SafeClamp(healthPercent, 0.0f, 1.0f);
    float BarHeight = 3.0f;
    float offsetY = 4.0f;
    float filledW = w * healthPercent;

    ImU32 healthColor =
        (healthPercent > 0.66f) ? IM_COL32(46, 204, 113, 255) :
        (healthPercent > 0.33f) ? IM_COL32(241, 196, 15, 255) :
                                  IM_COL32(231, 76, 60, 255);

    DrawList->AddRectFilled(
        ImVec2(x, y + h + offsetY),
        ImVec2(x + w, y + h + offsetY + BarHeight),
        IM_COL32(80, 80, 80, 120), 1.0f);

    DrawList->AddRectFilled(
        ImVec2(x, y + h + offsetY),
        ImVec2(x + filledW, y + h + offsetY + BarHeight),
        healthColor, 1.0f);
}

void DrawCrosshairRadar()
{
    if (!g_Globals.Visuals.Radar) return;

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    if (!drawList) return;

    ImVec2 center = ImVec2(g_Globals.Visuals.RadarPosX, g_Globals.Visuals.RadarPosY);
    float size = g_Globals.Visuals.RadarSize;
    float halfSize = size * 0.5f;

    drawList->AddRectFilled(ImVec2(center.x - halfSize, center.y - halfSize),
                            ImVec2(center.x + halfSize, center.y + halfSize),
                            IM_COL32(18, 18, 24, 210), 6.0f);
    drawList->AddRect(ImVec2(center.x - halfSize, center.y - halfSize),
                      ImVec2(center.x + halfSize, center.y + halfSize),
                      IM_COL32(80, 80, 100, 180), 6.0f, 0, 1.0f);

    drawList->AddLine(ImVec2(center.x - halfSize + 5, center.y), ImVec2(center.x + halfSize - 5, center.y), IM_COL32(120, 120, 140, 100));
    drawList->AddLine(ImVec2(center.x, center.y - halfSize + 5), ImVec2(center.x, center.y + halfSize - 5), IM_COL32(120, 120, 140, 100));
    drawList->AddCircleFilled(center, 3.0f, IM_COL32(255, 255, 255, 255));

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
        if (player.IsDead || !player.IsKnown || player.Address == 0) continue;
        if (player.Distance <= 0.5f || player.Distance > radarRange) continue;

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
    try
    {
        if (!g_Globals.EspConfig.Matrix) return;
        if (!g_Globals.EspConfig.InMatch) return;
        if (g_Globals.EspConfig.Width < 64 || g_Globals.EspConfig.Height < 64) return;

        // Snapshot under shared lock to prevent thread race condition
        std::vector<std::pair<uint64_t, Player>> entitiesSnapshot;
        {
            std::shared_lock<std::shared_mutex> lock(g_Globals.EspConfig.EntitiesMutex);
            if (!g_Globals.EspConfig.InMatch || !g_Globals.EspConfig.Matrix)
                return;
            entitiesSnapshot.reserve(g_Globals.EspConfig.Entities.size());
            for (const auto& kv : g_Globals.EspConfig.Entities)
                entitiesSnapshot.emplace_back(kv.first, kv.second);
        }

        if (entitiesSnapshot.empty())
            return;

        const float screenW = (float)g_Globals.EspConfig.Width;
        const float screenH = (float)g_Globals.EspConfig.Height;

        // ----------------------------------------------------
        // Enemy Count Badge (leakproject style)
        // ----------------------------------------------------
        if (g_Globals.Visuals.ShowNearEnemyCount)
        {
            int totalAlive = 0;
            for (const auto& pair : entitiesSnapshot)
            {
                const auto& entity = pair.second;
                if (entity.IsDead || entity.IsKnocked || entity.Pose == XPose::Knocked || !entity.IsKnown || entity.Address == 0)
                    continue;
                if (entity.Head == Vector3::Zero() && entity.Root == Vector3::Zero())
                    continue;
                if (entity.Distance <= 0.5f)
                    continue;

                totalAlive++;
            }

            ImDrawList* drawList = ImGui::GetBackgroundDrawList();
            if (drawList)
            {
                std::string countText = std::to_string(totalAlive);
                ImVec2 textSize = ImGui::CalcTextSize(countText.c_str());

                float paddingLeft = 10.0f;
                float paddingRight = 12.0f;
                float iconWidth = 14.0f;
                float spacing = 8.0f;

                float totalWidth = paddingLeft + iconWidth + spacing + textSize.x + paddingRight;
                float badgeHeight = 26.0f;

                float badgeX = (screenW - totalWidth) * 0.5f;
                float badgeY = 75.0f;

                ImVec2 badgeMin(badgeX, badgeY);
                ImVec2 badgeMax(badgeX + totalWidth, badgeY + badgeHeight);

                // Background
                drawList->AddRectFilled(badgeMin, badgeMax, IM_COL32(24, 28, 36, 230), 6.0f);
                drawList->AddRect(badgeMin, badgeMax, IM_COL32(50, 58, 70, 255), 6.0f, 0, 1.2f);

                // Blue player icon
                ImVec2 iconCenter(badgeX + paddingLeft + iconWidth * 0.5f, badgeY + badgeHeight * 0.5f);
                ImU32 iconColor = IM_COL32(52, 152, 219, 255);

                // Head circle
                float headRadius = 3.0f;
                ImVec2 headCenter(iconCenter.x, iconCenter.y - 2.8f);
                drawList->AddCircleFilled(headCenter, headRadius, iconColor, 12);

                // Shoulders
                drawList->AddRectFilled(
                    ImVec2(iconCenter.x - 5.0f, iconCenter.y + 0.8f),
                    ImVec2(iconCenter.x + 5.0f, iconCenter.y + 6.8f),
                    iconColor,
                    2.0f,
                    ImDrawFlags_RoundCornersTop
                );

                // Text
                ImVec2 textPos(badgeX + paddingLeft + iconWidth + spacing, badgeY + (badgeHeight - textSize.y) * 0.5f - 0.5f);
                drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), countText.c_str());
            }
        }

        // ----------------------------------------------------
        // Players Loop
        // ----------------------------------------------------
        auto W2SFast = [&](const Vector3& pos) -> ImVec2
        {
            return W2S::WorldToScreenImVec2(
                g_Globals.EspConfig.ViewMatrix,
                pos,
                g_Globals.EspConfig.Width,
                g_Globals.EspConfig.Height
            );
        };

        for (auto& [entityID, player] : entitiesSnapshot)
        {
            if (!g_Globals.EspConfig.InMatch || !g_Globals.EspConfig.Matrix)
                break;

            // Reject dead, unknown, null address entities
            if (player.IsDead || !player.IsKnown || player.Address == 0)
                continue;

            // Strict zero-coordinate ghost entity filter (CRITICAL FOR TRAINING GROUNDS)
            if (player.Head == Vector3::Zero() && player.Root == Vector3::Zero())
                continue;

            // Distance filter: distance <= 0.5m indicates dummy/inactive/camera origin object
            float dist = player.Distance;
            if (dist <= 0.5f || dist > g_Globals.Visuals.DistanceEsp)
                continue;

            // Visibility check
            if (!g_Globals.Visuals.Wukong && !player.IsVisible)
                continue;

            // Core Bones
            ImVec2 headPos = W2SFast(player.Head);
            ImVec2 rootPos = W2SFast(player.Root);

            // Strict off-screen check
            if (!IsFinite2(headPos.x, headPos.y) || !IsFinite2(rootPos.x, rootPos.y))
                continue;

            if (headPos.x <= 0.f || headPos.y <= 0.f ||
                rootPos.x <= 0.f || rootPos.y <= 0.f ||
                headPos.x >= screenW || rootPos.x >= screenW ||
                headPos.y >= screenH || rootPos.y >= screenH)
            {
                continue;
            }

            // Remaining Bones for Skeleton / Box
            ImVec2 neckPos = W2SFast(player.Neck);
            ImVec2 leftShoulderPos = W2SFast(player.LeftShoulder);
            ImVec2 rightShoulderPos = W2SFast(player.RightShoulder);
            ImVec2 leftElbowPos = W2SFast(player.LeftElbow);
            ImVec2 rightElbowPos = W2SFast(player.RightElbow);
            ImVec2 leftWristPos = W2SFast(player.LeftWrist);
            ImVec2 rightWristPos = W2SFast(player.RightWrist);
            ImVec2 hipPos = W2SFast(player.Hip);
            ImVec2 leftAnklePos = W2SFast(player.LeftAnkle);
            ImVec2 rightAnklePos = W2SFast(player.RightAnkle);

            // Hip calculations
            Vector3 ankleDiff = player.LeftAnkle - player.RightAnkle;
            float ankleDistance = ankleDiff.Magnitude(true);
            if (ankleDistance < 0.001f) ankleDistance = 0.001f;
            Vector3 hipDir = ankleDiff / ankleDistance;
            float baseHipWidth = ankleDistance * g_Globals.Visuals.HipWidthScale;
            Vector3 leftBase = Vector3::Lerp(player.LeftAnkle, player.Hip, g_Globals.Visuals.LeftHipHeightOffset);
            Vector3 rightBase = Vector3::Lerp(player.RightAnkle, player.Hip, g_Globals.Visuals.RightHipHeightOffset);
            Vector3 leftHip = leftBase + hipDir * (baseHipWidth * g_Globals.Visuals.HipWidthOffset);
            Vector3 rightHip = rightBase - hipDir * (baseHipWidth * g_Globals.Visuals.HipWidthOffset);
            ImVec2 leftHipPosition = W2SFast(leftHip);
            ImVec2 rightHipPosition = W2SFast(rightHip);

            // Box dimensions
            Vector3 standingHead3D = player.Root;
            standingHead3D.Y += 1.7f;
            ImVec2 standingHeadPos = W2SFast(standingHead3D);
            float standingBoxHeight = fabsf(standingHeadPos.y - rootPos.y);
            if (standingBoxHeight < 5.0f) standingBoxHeight = 50.0f;

            float boxHeight = 0.0f;
            float boxWidth = 0.0f;

            if (player.Pose == XPose::Knocked || player.IsKnocked)
            {
                boxHeight = standingBoxHeight * 0.45f;
                boxWidth = standingBoxHeight * 0.90f;
            }
            else
            {
                boxHeight = fabsf(headPos.y - rootPos.y);
                if (boxHeight < 5.0f) boxHeight = standingBoxHeight;
                boxWidth = boxHeight * 0.65f;
            }

            float ogH = boxHeight;
            float ogW = boxWidth;
            float ogX = headPos.x - (ogW * 0.5f);
            float ogY = headPos.y;

            // ==========================================
            // 1. ESP Snapline
            // ==========================================
            if (g_Globals.Visuals.Lines)
            {
                ImColor snapLineColor = ImColor(
                    g_Globals.Visuals.LinesColor[0],
                    g_Globals.Visuals.LinesColor[1],
                    g_Globals.Visuals.LinesColor[2],
                    g_Globals.Visuals.LinesColor[3]
                );

                if (g_Globals.Visuals.RainbowLines)
                {
                    float t = (float)ImGui::GetTime();
                    float hue = fmodf(t * 0.30f, 1.0f);
                    snapLineColor = ImColor(HSVtoRGB(hue, 1.f, 1.f));
                }
                else
                {
                    if (g_Globals.Visuals.Wukong)
                        snapLineColor = ImColor(0.6f, 0.0f, 1.0f, 1.0f);
                }

                if (player.Pose == XPose::Knocked || player.IsKnocked)
                {
                    float pulse = (sinf((float)ImGui::GetTime() * 5.0f) + 1.0f) * 0.5f;
                    snapLineColor = ImColor(1.f, 0.f, 0.f, 0.5f + 0.5f * pulse);
                }

                ImU32 lineColorU32 = snapLineColor;
                ImVec2 screenTopMid(screenW * 0.5f, 2.5f);
                ImVec2 screenBottom(screenW * 0.5f, screenH - 2.5f);
                ImVec2 attachPoint(headPos.x, headPos.y);

                float thickness = g_Globals.Visuals.LineThickness;
                if (thickness < 0.5f) thickness = 1.0f;
                float glowRadius = g_Globals.Visuals.GlowRadius;
                float glowFeather = g_Globals.Visuals.GlowFeather;

                auto DrawLineFunc = [&](const ImVec2& a, const ImVec2& b)
                {
                    if (g_Globals.Visuals.RainbowLines)
                        DrawGlowLineGradient(a, b, thickness, glowRadius, glowFeather);
                    else if (g_Globals.Visuals.GlowLines)
                        DrawGlowLine(a, b, lineColorU32, thickness, glowRadius, glowFeather);
                    else
                    {
                        ImDrawList* dl = ImGui::GetBackgroundDrawList();
                        if (dl) dl->AddLine(a, b, lineColorU32, thickness);
                    }
                };

                switch (g_Globals.Visuals.EspLines)
                {
                case 1: DrawLineFunc(screenTopMid, attachPoint); break;
                case 2: DrawLineFunc(screenBottom, attachPoint); break;
                default: DrawLineFunc(screenTopMid, attachPoint); break;
                }
            }

            // ==========================================
            // 2. ESP Skeleton
            // ==========================================
            if (g_Globals.Visuals.Skeleton)
            {
                ImColor skeletonColor = ImColor(
                    g_Globals.Visuals.SkeletonColor[0],
                    g_Globals.Visuals.SkeletonColor[1],
                    g_Globals.Visuals.SkeletonColor[2],
                    g_Globals.Visuals.SkeletonColor[3]
                );

                if (player.IsKnocked || player.Pose == XPose::Knocked)
                    skeletonColor = ImColor(1.f, 0.f, 0.f, 1.f);

                float boneThickness = g_Globals.Visuals.SkeletonThickness;
                if (boneThickness < 0.5f) boneThickness = 1.0f;

                ImDrawList* skelDraw = ImGui::GetForegroundDrawList();
                if (skelDraw)
                {
                    auto DrawBone = [&](const ImVec2& from, const ImVec2& to) {
                        float dx = from.x - to.x;
                        float dy = from.y - to.y;
                        if ((dx * dx + dy * dy) >= 500.0f * 500.0f) return;
                        skelDraw->AddLine(from, to, skeletonColor, boneThickness);
                    };

                    float shoulderWidth = std::abs(leftShoulderPos.x - rightShoulderPos.x);
                    if (!std::isfinite(shoulderWidth)) shoulderWidth = 10.0f;
                    float headRadius = SafeClamp(shoulderWidth * 0.15f, 2.5f, 6.0f);

                    skelDraw->AddCircle(headPos, headRadius, skeletonColor, 12, boneThickness);
                    DrawBone(headPos, neckPos);
                    DrawBone(neckPos, leftShoulderPos);
                    DrawBone(neckPos, rightShoulderPos);
                    DrawBone(leftShoulderPos, leftElbowPos);
                    DrawBone(rightShoulderPos, rightElbowPos);
                    DrawBone(leftElbowPos, leftWristPos);
                    DrawBone(rightElbowPos, rightWristPos);
                    DrawBone(neckPos, hipPos);
                    DrawBone(hipPos, leftHipPosition);
                    DrawBone(leftHipPosition, leftAnklePos);
                    DrawBone(hipPos, rightHipPosition);
                    DrawBone(rightHipPosition, rightAnklePos);
                }
            }

            // ==========================================
            // 3. ESP Box
            // ==========================================
            if (g_Globals.Visuals.Box)
            {
                ImColor currentBoxColor = ImColor(
                    g_Globals.Visuals.BoxColor[0],
                    g_Globals.Visuals.BoxColor[1],
                    g_Globals.Visuals.BoxColor[2],
                    g_Globals.Visuals.BoxColor[3]
                );

                if (player.Pose == XPose::Knocked || player.IsKnocked)
                    currentBoxColor = IM_COL32(255, 0, 0, 255);

                float boxThick = 1.2f;
                int currentBoxType = g_Globals.Visuals.players_box;

                if (g_Globals.Visuals.FilledBox)
                {
                    ImColor fillColor = ImColor(
                        g_Globals.Visuals.Filledboxcolor[0],
                        g_Globals.Visuals.Filledboxcolor[1],
                        g_Globals.Visuals.Filledboxcolor[2],
                        g_Globals.Visuals.Filledboxcolor[3]
                    );
                    ImGui::GetForegroundDrawList()->AddRectFilled(
                        ImVec2(ogX, ogY),
                        ImVec2(ogX + ogW, ogY + ogH),
                        fillColor, 5.0f);
                }

                if (currentBoxType == 1)
                    DrawFullBox(ogX, ogY, ogW, ogH, currentBoxColor, boxThick);
                else
                    DrawGlowCorneredBox(ogX, ogY, ogW, ogH, currentBoxColor, boxThick, 0.0f, 0.0f, false);
            }

            // ==========================================
            // 4. ESP Health Bar
            // ==========================================
            if (g_Globals.Visuals.HealthBar)
            {
                ImDrawList* DrawList = ImGui::GetForegroundDrawList();
                if (DrawList)
                {
                    switch (g_Globals.Visuals.players_healthbar)
                    {
                    case 0: // None
                        break;
                    case 1: // Left
                        DrawVerticalHealthBar(player.Health, 200, ImVec2(ogX - 6.0f, ogY), ogH);
                        break;
                    case 2: // Right (Default leakproject style)
                    {
                        float pct = SafeClamp((float)player.Health / 200.0f, 0.0f, 1.0f);
                        if (player.Health > 1000) pct = 1.0f;
                        if (player.Health < 0) pct = 1.0f;
                        ImU32 hc = player.IsKnocked ? IM_COL32(255, 0, 0, 255)
                            : (pct > 0.8f) ? IM_COL32(0, 255, 0, 255)
                            : (pct > 0.4f) ? IM_COL32(255, 255, 0, 255)
                            : IM_COL32(255, 0, 0, 255);
                        if (pct > 0.01f)
                        {
                            float hbW = 3.0f;
                            float hbX = ogX + ogW + 5.0f;
                            DrawMvpGlowHealthBar(DrawList, ImVec2(hbX, ogY + ogH * (1.0f - pct)), ImVec2(hbX + hbW, ogY + ogH), hc);
                        }
                        break;
                    }
                    case 3: // Bottom
                        DrawHealthBarBelowFullBox(player.Health, 200, ogX, ogY, ogW, ogH);
                        break;
                    case 4: // Text
                    {
                        char healthText[16];
                        snprintf(healthText, sizeof(healthText), "%d HP", player.Health);
                        float hpPercent = (float)player.Health / 200.0f;
                        ImU32 mainColor = (hpPercent > 0.66f) ? IM_COL32(128, 255, 0, 255) :
                                          (hpPercent > 0.33f) ? IM_COL32(255, 128, 0, 255) :
                                                                IM_COL32(255, 0, 0, 255);
                        DrawList->AddText(ImVec2(ogX + ogW + 7.0f, ogY + ogH * 0.5f), mainColor, healthText);
                        break;
                    }
                    }
                }
            }

            // ==========================================
            // 5. Name, Distance, Level, Weapon Tags
            // ==========================================
            ImDrawList* drawList = ImGui::GetForegroundDrawList();
            if (drawList)
            {
                float cx = headPos.x;
                const float gap = 3.0f;
                float y = ogY - gap;

                // 1) Name above box
                if (g_Globals.Visuals.Name)
                {
                    std::string displayName = player.Name.empty() ? "Enemy" : player.Name;
                    ImVec2 nSize = ImGui::CalcTextSize(displayName.c_str());
                    y -= nSize.y;
                    ImVec2 namePos(cx - nSize.x * 0.5f, y);

                    ImU32 nameCol = (player.IsKnocked || player.Pose == XPose::Knocked)
                        ? IM_COL32(255, 0, 0, 255)
                        : IM_COL32(255, 255, 255, 255);

                    // Background shadow outline
                    drawList->AddText(ImVec2(namePos.x + 1.f, namePos.y + 1.f), IM_COL32(0, 0, 0, 220), displayName.c_str());
                    drawList->AddText(namePos, nameCol, displayName.c_str());
                    y -= gap;
                }

                // 2) Weapon Name / Icon
                if (g_Globals.Visuals.ESPWeapon || g_Globals.Visuals.ESPWeaponIcon)
                {
                    Namegun::Init();
                    std::string fullName = Namegun::GetGunName(player.WeaponID);
                    if (!fullName.empty())
                    {
                        ImVec2 sz = ImGui::CalcTextSize(fullName.c_str());
                        float pad = 4.0f;
                        float bw = sz.x + pad * 2.0f;
                        float bx = cx - bw * 0.5f;
                        y -= (sz.y + 3.0f);

                        drawList->AddRectFilled(ImVec2(bx, y), ImVec2(bx + bw, y + sz.y + 3.0f), IM_COL32(20, 20, 20, 180), 3.0f);
                        drawList->AddRect(ImVec2(bx, y), ImVec2(bx + bw, y + sz.y + 3.0f), IM_COL32(192, 0, 0, 220), 3.0f);
                        drawList->AddText(ImVec2(bx + pad, y + 1.0f), IM_COL32(255, 255, 255, 255), fullName.c_str());
                        y -= gap;
                    }
                }

                // 3) Level badge on left of box
                if (g_Globals.Visuals.Level && player.Level > 0)
                {
                    char lvBuf[16];
                    snprintf(lvBuf, sizeof(lvBuf), "Lv.%d", player.Level);
                    ImVec2 sz = ImGui::CalcTextSize(lvBuf);
                    float lx = ogX - sz.x - 8.0f;
                    float ly = ogY;

                    ImU32 lvColor = (player.Level >= 60) ? IM_COL32(250, 1, 2, 255)
                        : (player.Level >= 40) ? IM_COL32(255, 165, 0, 255)
                        : IM_COL32(173, 255, 47, 255);

                    drawList->AddRectFilled(ImVec2(lx - 2.f, ly), ImVec2(lx + sz.x + 2.f, ly + sz.y + 2.f), lvColor, 2.0f);
                    drawList->AddText(ImVec2(lx, ly + 1.f), IM_COL32(0, 0, 0, 255), lvBuf);
                }

                // 4) Distance below box
                if (g_Globals.Visuals.Distance)
                {
                    char distBuf[32];
                    snprintf(distBuf, sizeof(distBuf), "[%d m]", static_cast<int>(std::round(dist)));
                    ImVec2 dSize = ImGui::CalcTextSize(distBuf);
                    ImVec2 dp(cx - dSize.x * 0.5f, ogY + ogH + 6.0f);

                    drawList->AddText(ImVec2(dp.x + 1.f, dp.y + 1.f), IM_COL32(0, 0, 0, 220), distBuf);
                    drawList->AddText(dp, IM_COL32(220, 220, 220, 255), distBuf);
                }
            }
        }

        DrawCrosshairRadar();
    }
    catch (...)
    {
    }
}

void Render()
{
    Players();
}

} // namespace ESP
