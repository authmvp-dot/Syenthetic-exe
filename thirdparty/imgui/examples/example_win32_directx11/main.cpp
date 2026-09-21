#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")

#include "../../framework/settings/functions.h"
#include "../../framework/data/font.h"
#include "../../framework/data/texture.h"
#include "../../framework/data/imgui_freetype.h"
#include "../../framework/esp/esp_data.h"
#include "../../framework/esp/esp_visuals.h"
#include "../../framework/esp/esp_globals.h"
#include "../../framework/esp/name_gun.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dx11.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "iphlpapi.lib")

#include <windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <tchar.h>
#include <d3dx11.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <ctime>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <thread>

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK HudWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static ULONG_PTR s_gdiplusToken = 0;
void InitGDIPlus()
{
    if (s_gdiplusToken == 0)
    {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        Gdiplus::GdiplusStartup(&s_gdiplusToken, &gdiplusStartupInput, nullptr);
    }
}

void ShutdownGDIPlus()
{
    if (s_gdiplusToken != 0)
    {
        Gdiplus::GdiplusShutdown(s_gdiplusToken);
        s_gdiplusToken = 0;
    }
}

// ----------------------------------------------------
// Real System Performance Monitoring (CPU & GPU)
// ----------------------------------------------------
static float GetRealCpuUsage()
{
    static ULONGLONG prev_idle = 0, prev_kernel = 0, prev_user = 0;
    static float cpu_percent = 0.0f;
    static DWORD last_tick = 0;
    DWORD now = GetTickCount();

    if (now - last_tick >= 400 || last_tick == 0)
    {
        FILETIME idleTime, kernelTime, userTime;
        if (GetSystemTimes(&idleTime, &kernelTime, &userTime))
        {
            ULARGE_INTEGER idl, krn, usr;
            idl.LowPart = idleTime.dwLowDateTime; idl.HighPart = idleTime.dwHighDateTime;
            krn.LowPart = kernelTime.dwLowDateTime; krn.HighPart = kernelTime.dwHighDateTime;
            usr.LowPart = userTime.dwLowDateTime; usr.HighPart = userTime.dwHighDateTime;

            if (prev_kernel != 0 || prev_user != 0)
            {
                ULONGLONG sys_diff = (krn.QuadPart - prev_kernel) + (usr.QuadPart - prev_user);
                ULONGLONG idle_diff = idl.QuadPart - prev_idle;
                if (sys_diff > 0)
                {
                    float raw = (float)(sys_diff - idle_diff) * 100.0f / (float)sys_diff;
                    if (raw < 0.0f) raw = 0.0f;
                    if (raw > 100.0f) raw = 100.0f;
                    cpu_percent = raw;
                }
            }
            prev_idle = idl.QuadPart;
            prev_kernel = krn.QuadPart;
            prev_user = usr.QuadPart;
        }
        last_tick = now;
    }
    return cpu_percent;
}

// ----------------------------------------------------
// Real Internet Ping Monitoring (ms) via ICMP
// ----------------------------------------------------
static std::atomic<int> s_currentPingMs(-1);
static std::atomic<bool> s_pingThreadRunning(true);
static std::thread s_pingThread;

static void PingWorkerThreadFunc()
{
    while (s_pingThreadRunning.load())
    {
        HANDLE hIcmp = IcmpCreateFile();
        if (hIcmp != INVALID_HANDLE_VALUE)
        {
            char sendData[8] = "synth";
            BYTE replyBuffer[sizeof(ICMP_ECHO_REPLY) + 32] = { 0 };

            // 1.1.1.1 (Cloudflare DNS - 0x01010101)
            DWORD ret = IcmpSendEcho(hIcmp, 0x01010101, sendData, sizeof(sendData), NULL, replyBuffer, sizeof(replyBuffer), 700);
            if (ret != 0)
            {
                PICMP_ECHO_REPLY pEcho = (PICMP_ECHO_REPLY)replyBuffer;
                if (pEcho->Status == IP_SUCCESS)
                    s_currentPingMs.store((int)pEcho->RoundTripTime);
                else
                    s_currentPingMs.store(-1);
            }
            else
            {
                // Fallback to 8.8.8.8 (Google DNS - 0x08080808)
                DWORD ret2 = IcmpSendEcho(hIcmp, 0x08080808, sendData, sizeof(sendData), NULL, replyBuffer, sizeof(replyBuffer), 700);
                if (ret2 != 0)
                {
                    PICMP_ECHO_REPLY pEcho2 = (PICMP_ECHO_REPLY)replyBuffer;
                    if (pEcho2->Status == IP_SUCCESS)
                        s_currentPingMs.store((int)pEcho2->RoundTripTime);
                    else
                        s_currentPingMs.store(-1);
                }
                else
                {
                    s_currentPingMs.store(-1);
                }
            }
            IcmpCloseHandle(hIcmp);
        }
        else
        {
            s_currentPingMs.store(-1);
        }

        // Sleep ~1.5s between ping updates with responsive exit check
        for (int i = 0; i < 15 && s_pingThreadRunning.load(); ++i)
        {
            Sleep(100);
        }
    }
}

void UpdateHudWindow(float framerate)
{
    if (!g_hHudWnd || !IsWindow(g_hHudWnd)) return;

    int width = 455;
    int height = 32;

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pvBits = nullptr;
    HBITMAP hBmp = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pvBits, NULL, 0);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hBmp);

    memset(pvBits, 0, width * height * 4);

    {
        Gdiplus::Bitmap bmp(width, height, width * 4, PixelFormat32bppPARGB, (BYTE*)pvBits);
        Gdiplus::Graphics g(&bmp);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        float c = 6.0f; // Chamfer cut size
        float w = (float)width - 1.0f;
        float h = (float)height - 1.0f;

        // 1. Octagonal Chamfered Path
        Gdiplus::GraphicsPath path;
        path.AddLine(c, 0.0f, w - c, 0.0f);
        path.AddLine(w - c, 0.0f, w, c);
        path.AddLine(w, c, w, h - c);
        path.AddLine(w, h - c, w - c, h);
        path.AddLine(w - c, h, c, h);
        path.AddLine(c, h, 0.0f, h - c);
        path.AddLine(0.0f, h - c, 0.0f, c);
        path.AddLine(0.0f, c, c, 0.0f);
        path.CloseFigure();

        // 2. Background: Deep obsidian black
        Gdiplus::SolidBrush bgBrush(Gdiplus::Color(245, 12, 12, 16));
        g.FillPath(&bgBrush, &path);

        // 3. Base subtle outline
        Gdiplus::Pen baseBorderPen(Gdiplus::Color(160, 42, 42, 54), 1.0f);
        g.DrawPath(&baseBorderPen, &path);

        // 4. Glowing Neon Accents:
        // Left chamfers & left edge: Magenta / Neon Pink
        Gdiplus::Pen pinkPen(Gdiplus::Color(255, 235, 45, 150), 1.6f);
        g.DrawLine(&pinkPen, 0.0f, c, c, 0.0f);
        g.DrawLine(&pinkPen, 0.0f, h - c, c, h);
        g.DrawLine(&pinkPen, 0.0f, c, 0.0f, h - c);

        // Right chamfers & right edge: Electric Blue / Purple
        Gdiplus::Pen bluePen(Gdiplus::Color(255, 56, 175, 255), 1.6f);
        g.DrawLine(&bluePen, w - c, h, w, h - c);
        g.DrawLine(&bluePen, w - c, 0.0f, w, c);
        g.DrawLine(&bluePen, w, c, w, h - c);

        // 5. Brushes & Fonts
        Gdiplus::Font fontBold(L"Segoe UI", 8.5f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);

        Gdiplus::SolidBrush whiteBrush(Gdiplus::Color(255, 240, 240, 245));
        BYTE accR = (BYTE)ImClamp((int)(clr->c_other_clr.accent_clr.x * 255), 0, 255);
        BYTE accG = (BYTE)ImClamp((int)(clr->c_other_clr.accent_clr.y * 255), 0, 255);
        BYTE accB = (BYTE)ImClamp((int)(clr->c_other_clr.accent_clr.z * 255), 0, 255);
        Gdiplus::SolidBrush purpleBrush(Gdiplus::Color(255, accR, accG, accB));
        Gdiplus::SolidBrush iconBrush(Gdiplus::Color(255, accR, accG, accB));
        Gdiplus::Pen iconPen(Gdiplus::Color(255, accR, accG, accB), 1.4f);
        Gdiplus::Pen divPen(Gdiplus::Color(90, 75, 75, 95), 1.0f);

        Gdiplus::StringFormat fmt;
        fmt.SetAlignment(Gdiplus::StringAlignmentNear);
        fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);

        float curX = 13.0f;
        float centerY = height * 0.5f;

        // [Element 1] 2x2 Purple Grid Icon + "Synthetic"
        g.FillRectangle(&iconBrush, curX, centerY - 4.5f, 4.2f, 4.2f);
        g.FillRectangle(&iconBrush, curX + 5.6f, centerY - 4.5f, 4.2f, 4.2f);
        g.FillRectangle(&iconBrush, curX, centerY + 1.2f, 4.2f, 4.2f);
        g.FillRectangle(&iconBrush, curX + 5.6f, centerY + 1.2f, 4.2f, 4.2f);
        curX += 9.8f + 5.0f;

        Gdiplus::RectF bound;
        g.MeasureString(L"Synthetic", -1, &fontBold, Gdiplus::PointF(0, 0), &bound);
        g.DrawString(L"Synthetic", -1, &fontBold, Gdiplus::RectF(curX, centerY - 9.0f, bound.Width + 2.0f, 18.0f), &fmt, &whiteBrush);
        curX += bound.Width + 6.0f;

        // Divider
        g.DrawLine(&divPen, curX, centerY - 5.5f, curX, centerY + 5.5f);
        curX += 6.0f;

        // [Element 2] 3-Bar Chart Icon + Dynamic FPS
        g.FillRectangle(&purpleBrush, curX, centerY - 1.0f, 2.2f, 6.5f);
        g.FillRectangle(&purpleBrush, curX + 3.4f, centerY - 5.5f, 2.2f, 11.0f);
        g.FillRectangle(&purpleBrush, curX + 6.8f, centerY - 3.5f, 2.2f, 9.0f);
        curX += 9.0f + 4.5f;

        int fps_val = (int)roundf(framerate > 0.f ? framerate : 144.f);
        std::wstring wfps = std::to_wstring(fps_val) + L" FPS";
        g.MeasureString(wfps.c_str(), -1, &fontBold, Gdiplus::PointF(0, 0), &bound);
        g.DrawString(wfps.c_str(), -1, &fontBold, Gdiplus::RectF(curX, centerY - 9.0f, bound.Width + 2.0f, 18.0f), &fmt, &whiteBrush);
        curX += bound.Width + 6.0f;

        // Divider
        g.DrawLine(&divPen, curX, centerY - 5.5f, curX, centerY + 5.5f);
        curX += 6.0f;

        // [Element 4] Mini CPU Chip Icon + Real CPU %
        g.DrawRectangle(&iconPen, curX + 1.0f, centerY - 4.5f, 8.0f, 8.0f);
        g.FillRectangle(&iconBrush, curX + 3.0f, centerY - 2.5f, 4.0f, 4.0f);
        curX += 9.0f + 4.5f;

        int cpu_val = (int)roundf(GetRealCpuUsage());
        std::wstring wcpu = L"CPU " + std::to_wstring(cpu_val) + L" %";
        g.MeasureString(wcpu.c_str(), -1, &fontBold, Gdiplus::PointF(0, 0), &bound);
        g.DrawString(wcpu.c_str(), -1, &fontBold, Gdiplus::RectF(curX, centerY - 9.0f, bound.Width + 2.0f, 18.0f), &fmt, &whiteBrush);
        curX += bound.Width + 6.0f;

        // Divider
        g.DrawLine(&divPen, curX, centerY - 5.5f, curX, centerY + 5.5f);
        curX += 6.0f;

        // [Element 5] Internet Ping Icon + Real Ping
        g.FillEllipse(&iconBrush, curX + 3.8f, centerY + 2.2f, 2.4f, 2.4f);
        g.DrawArc(&iconPen, curX + 1.8f, centerY - 1.6f, 6.4f, 5.0f, 215, 110);
        g.DrawArc(&iconPen, curX - 0.2f, centerY - 5.2f, 10.4f, 8.4f, 215, 110);
        curX += 10.2f + 4.5f;

        int ping_val = s_currentPingMs.load();
        std::wstring wping;
        if (ping_val >= 0)
            wping = L"Ping " + std::to_wstring(ping_val) + L" ms";
        else
            wping = L"Ping N/A";
        g.MeasureString(wping.c_str(), -1, &fontBold, Gdiplus::PointF(0, 0), &bound);
        g.DrawString(wping.c_str(), -1, &fontBold, Gdiplus::RectF(curX, centerY - 9.0f, bound.Width + 2.0f, 18.0f), &fmt, &whiteBrush);
        curX += bound.Width + 6.0f;

        // Divider
        g.DrawLine(&divPen, curX, centerY - 5.5f, curX, centerY + 5.5f);
        curX += 6.0f;

        // [Element 6] Clock Icon + Real Time (Indian 12-hr AM/PM format, plenty of room)
        g.DrawEllipse(&iconPen, curX, centerY - 5.0f, 10.0f, 10.0f);
        g.DrawLine(&iconPen, curX + 5.0f, centerY, curX + 5.0f, centerY - 3.2f);
        g.DrawLine(&iconPen, curX + 5.0f, centerY, curX + 7.2f, centerY);
        curX += 10.0f + 4.5f;

        char time_str[32] = { 0 };
        // Query user's exact Windows clock format (e.g. "9.53.18 PM" / "9:53:18 PM")
        if (!GetTimeFormatA(LOCALE_USER_DEFAULT, 0, NULL, NULL, time_str, sizeof(time_str)) || (strstr(time_str, "M") == nullptr && strstr(time_str, "m") == nullptr))
        {
            time_t rawtime = time(nullptr);
            struct tm timeinfo;
            localtime_s(&timeinfo, &rawtime);
            int hour12 = timeinfo.tm_hour % 12;
            if (hour12 == 0) hour12 = 12;
            const char* ampm = (timeinfo.tm_hour >= 12) ? "PM" : "AM";
            sprintf_s(time_str, sizeof(time_str), "%d:%02d:%02d %s", hour12, timeinfo.tm_min, timeinfo.tm_sec, ampm);
        }
        wchar_t wtime[64] = { 0 };
        MultiByteToWideChar(CP_ACP, 0, time_str, -1, wtime, 64);

        g.MeasureString(wtime, -1, &fontBold, Gdiplus::PointF(0, 0), &bound);
        g.DrawString(wtime, -1, &fontBold, Gdiplus::RectF(curX, centerY - 9.0f, bound.Width + 8.0f, 18.0f), &fmt, &whiteBrush);
    }

    POINT ptSrc = { 0, 0 };
    SIZE wndSize = { width, height };

    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(g_hHudWnd, hdcScreen, nullptr, &wndSize, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(hdcMem, hOldBmp);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
}

struct HitRect
{
    int left, top, right, bottom;
    bool operator==(const HitRect& o) const
    {
        return left == o.left && top == o.top && right == o.right && bottom == o.bottom;
    }
    bool operator!=(const HitRect& o) const
    {
        return !(*this == o);
    }
};

static void UpdateHitTestRegion(HWND hWnd)
{
    if (!hWnd) return;

    RECT rcClient;
    if (!GetClientRect(hWnd, &rcClient)) return;
    int win_w = rcClient.right;
    int win_h = rcClient.bottom;

    float dpi = (var && var->c_dpi.dpi > 0.0f) ? var->c_dpi.dpi : 1.0f;
    int menu_w = (int)((set ? set->c_window.window_size.x : 860.f) * dpi);
    int menu_h = (int)((set ? set->c_window.window_size.y : 630.f) * dpi);
    if (menu_w > win_w) menu_w = win_w;
    if (menu_h > win_h) menu_h = win_h;

    std::vector<HitRect> rects;
    rects.push_back({ 0, 0, menu_w, menu_h });

    ImGuiContext* g = GImGui;
    if (g)
    {
        for (ImGuiWindow* w : g->Windows)
        {
            if (w && w->Active && !w->Hidden && !(w->Flags & ImGuiWindowFlags_NoInputs) && w->Name &&
                strcmp(w->Name, "NAME") != 0 && strcmp(w->Name, "watermark") != 0)
            {
                if (w->Size.x > 5.0f && w->Size.y > 5.0f)
                {
                    int l = (std::max)(0, (int)w->Pos.x - 2);
                    int t = (std::max)(0, (int)w->Pos.y - 2);
                    int r = (std::min)(win_w, (int)(w->Pos.x + w->Size.x + 2));
                    int b = (std::min)(win_h, (int)(w->Pos.y + w->Size.y + 2));

                    if (r > menu_w || b > menu_h || l < 0 || t < 0)
                    {
                        if (r > l && b > t)
                        {
                            rects.push_back({ l, t, r, b });
                        }
                    }
                }
            }
        }
    }

    static std::vector<HitRect> s_last_rects;
    if (rects != s_last_rects)
    {
        HRGN hCombined = CreateRectRgn(rects[0].left, rects[0].top, rects[0].right, rects[0].bottom);
        for (size_t i = 1; i < rects.size(); ++i)
        {
            HRGN hSub = CreateRectRgn(rects[i].left, rects[i].top, rects[i].right, rects[i].bottom);
            CombineRgn(hCombined, hCombined, hSub, RGN_OR);
            DeleteObject(hSub);
        }
        SetWindowRgn(hWnd, hCombined, TRUE);
        s_last_rects = rects;
    }
}

int MainApp()
{
    InitGDIPlus();

    int primary_w = GetSystemMetrics(SM_CXSCREEN);
    int primary_h = GetSystemMetrics(SM_CYSCREEN);
    if (primary_w <= 0) primary_w = 1920;
    if (primary_h <= 0) primary_h = 1080;

    int extra_canvas_w = 220;
    int extra_canvas_h = 160;
    int menu_w = (int)set->c_window.window_size.x;
    int menu_h = (int)set->c_window.window_size.y;
    int win_w = menu_w + extra_canvas_w;
    int win_h = menu_h + extra_canvas_h;
    int win_x = (primary_w - menu_w) / 2;
    int win_y = (primary_h - menu_h) / 2;

    // 1. Main Menu Window Class
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"SyntheticWindowClass", nullptr };
    ::RegisterClassExW(&wc);

    g_hwnd = ::CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED,
        wc.lpszClassName,
        L"Synthetic",
        WS_POPUP,
        win_x, win_y, win_w, win_h,
        nullptr, nullptr, wc.hInstance, nullptr
    );

    if (!g_hwnd)
    {
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    SetLayeredWindowAttributes(g_hwnd, RGB(0, 0, 0), 255, LWA_ALPHA);

    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(g_hwnd, &margins);

    // Strictly constrain initial click region to menu rectangle so desktop is never blocked
    HRGN hInitialRgn = CreateRectRgn(0, 0, menu_w, menu_h);
    SetWindowRgn(g_hwnd, hInitialRgn, TRUE);

    // 2. Dedicated Draggable Watermark / FPS HUD Window
    WNDCLASSEXW wcHud = { sizeof(wcHud), CS_CLASSDC, HudWndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, LoadCursor(0, IDC_ARROW), nullptr, nullptr, L"SyntheticHudClass", nullptr };
    ::RegisterClassExW(&wcHud);

    int hud_w = 455;
    int hud_h = 32;
    int hud_x = primary_w - hud_w - 30;
    int hud_y = 25;

    g_hHudWnd = ::CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        wcHud.lpszClassName,
        L"Synthetic HUD",
        WS_POPUP,
        hud_x, hud_y, hud_w, hud_h,
        nullptr, nullptr, wcHud.hInstance, nullptr
    );

    if (g_hHudWnd)
    {
        ShowWindow(g_hHudWnd, SW_SHOWNOACTIVATE);
        UpdateWindow(g_hHudWnd);
    }

    if (!CreateDeviceD3D(g_hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(g_hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(g_hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    cfg.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint | ImGuiFreeTypeBuilderFlags_LightHinting | ImGuiFreeTypeBuilderFlags_LoadColor;

    set->c_font.inter_medium[0] = io.Fonts->AddFontFromMemoryTTF(inter_medium, sizeof(inter_medium), 15.f, &cfg, io.Fonts->GetGlyphRangesCyrillic());
    set->c_font.inter_medium[1] = io.Fonts->AddFontFromMemoryTTF(inter_medium, sizeof(inter_medium), 16.f, &cfg, io.Fonts->GetGlyphRangesCyrillic());

    set->c_font.icon[0] = io.Fonts->AddFontFromMemoryTTF(icon, sizeof(icon), 14.f, &cfg, io.Fonts->GetGlyphRangesCyrillic());
    set->c_font.icon[1] = io.Fonts->AddFontFromMemoryTTF(icon, sizeof(icon), 16.f, &cfg, io.Fonts->GetGlyphRangesCyrillic());
    set->c_font.icon[2] = io.Fonts->AddFontFromMemoryTTF(icon, sizeof(icon), 40.f, &cfg, io.Fonts->GetGlyphRangesCyrillic());
    set->c_font.icon[3] = io.Fonts->AddFontFromMemoryTTF(icon2, sizeof(icon2), 15.f, &cfg, io.Fonts->GetGlyphRangesCyrillic());
    set->c_font.icon[4] = io.Fonts->AddFontFromMemoryTTF(icon2, sizeof(icon2), 9.f, &cfg, io.Fonts->GetGlyphRangesCyrillic());
    set->c_font.icon[5] = io.Fonts->AddFontFromMemoryTTF(icon2, sizeof(icon2), 76.f, &cfg, io.Fonts->GetGlyphRangesCyrillic());
    set->c_font.icon[6] = io.Fonts->AddFontFromMemoryTTF(icon2, sizeof(icon2), 96.f, &cfg, io.Fonts->GetGlyphRangesCyrillic());

    set->c_font.name = io.Fonts->AddFontFromMemoryTTF(inter_medium, sizeof(inter_medium), 18.f, &cfg, io.Fonts->GetGlyphRangesCyrillic());

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    D3DX11_IMAGE_LOAD_INFO img_info;
    ID3DX11ThreadPump* thread_pump{ nullptr };
    if (set->c_texture.bg == nullptr) D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, background, sizeof(background), &img_info, thread_pump, &set->c_texture.bg, 0);
    if (set->c_texture.logo == nullptr) D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, logo, sizeof(logo), &img_info, thread_pump, &set->c_texture.logo, 0);

    Namegun::Init();
    FWork::Data::StartThread();

    bool done = false;
    bool menu_open = true;
    DWORD lastHudUpdate = 0;

    s_pingThreadRunning.store(true);
    s_pingThread = std::thread(PingWorkerThreadFunc);

    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done) break;

        // Toggle menu on/off via configurable keybind (default INSERT) or in-panel request
        int hide_k = (var->c_panel.hide_key != 0) ? var->c_panel.hide_key : VK_INSERT;
        if ((var->c_panel.enable_hide_key && (GetAsyncKeyState(hide_k) & 1)) || var->c_panel.request_hide)
        {
            var->c_panel.request_hide = false;
            var->c_panel.menu_open = !var->c_panel.menu_open;
            ShowWindow(g_hwnd, var->c_panel.menu_open ? SW_SHOW : SW_HIDE);
        }

        // Exit panel via configurable keybind (default END) or in-panel request
        int exit_k = (var->c_panel.exit_key != 0) ? var->c_panel.exit_key : VK_END;
        if ((var->c_panel.enable_exit_key && (GetAsyncKeyState(exit_k) & 1)) || var->c_panel.request_exit)
        {
            done = true;
            break;
        }

        // Update floating HUD watermark periodically
        DWORD now = GetTickCount();
        if (now - lastHudUpdate >= 50)
        {
            UpdateHudWindow(io.Framerate);
            lastHudUpdate = now;
        }

        if (!var->c_panel.menu_open)
        {
            ::Sleep(16);
            continue;
        }

        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
        {
            ::Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // DPI reload if requested
        if (var->c_dpi.dpi_changed)
        {
            var->c_dpi.dpi = var->c_dpi.dpi_saved / 100.f;

            io.Fonts->Clear();
            set->c_font.inter_medium[0] = io.Fonts->AddFontFromMemoryTTF(inter_medium, sizeof(inter_medium), 15.f * var->c_dpi.dpi, &cfg, io.Fonts->GetGlyphRangesCyrillic());
            set->c_font.inter_medium[1] = io.Fonts->AddFontFromMemoryTTF(inter_medium, sizeof(inter_medium), 16.f * var->c_dpi.dpi, &cfg, io.Fonts->GetGlyphRangesCyrillic());

            set->c_font.icon[0] = io.Fonts->AddFontFromMemoryTTF(icon, sizeof(icon), 14.f * var->c_dpi.dpi, &cfg, io.Fonts->GetGlyphRangesCyrillic());
            set->c_font.icon[1] = io.Fonts->AddFontFromMemoryTTF(icon, sizeof(icon), 16.f * var->c_dpi.dpi, &cfg, io.Fonts->GetGlyphRangesCyrillic());
            set->c_font.icon[2] = io.Fonts->AddFontFromMemoryTTF(icon, sizeof(icon), 40.f * var->c_dpi.dpi, &cfg, io.Fonts->GetGlyphRangesCyrillic());
            set->c_font.icon[3] = io.Fonts->AddFontFromMemoryTTF(icon2, sizeof(icon2), 15.f * var->c_dpi.dpi, &cfg, io.Fonts->GetGlyphRangesCyrillic());
            set->c_font.icon[4] = io.Fonts->AddFontFromMemoryTTF(icon2, sizeof(icon2), 9.f * var->c_dpi.dpi, &cfg, io.Fonts->GetGlyphRangesCyrillic());
            set->c_font.icon[5] = io.Fonts->AddFontFromMemoryTTF(icon2, sizeof(icon2), 76.f * var->c_dpi.dpi, &cfg, io.Fonts->GetGlyphRangesCyrillic());
            set->c_font.icon[6] = io.Fonts->AddFontFromMemoryTTF(icon2, sizeof(icon2), 96.f * var->c_dpi.dpi, &cfg, io.Fonts->GetGlyphRangesCyrillic());

            set->c_font.name = io.Fonts->AddFontFromMemoryTTF(inter_medium, sizeof(inter_medium), 18.f * var->c_dpi.dpi, &cfg, io.Fonts->GetGlyphRangesCyrillic());

            io.Fonts->Build();
            ImGui_ImplDX11_CreateDeviceObjects();
            var->c_dpi.dpi_changed = false;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();

        ESP::Render();
        gui->render();

        UpdateHitTestRegion(g_hwnd);

        const float clear_color[4] = { 0.f, 0.f, 0.f, 0.f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        HRESULT hr = g_pSwapChain->Present(1, 0);
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    FWork::Data::StopThread();

    s_pingThreadRunning.store(false);
    if (s_pingThread.joinable())
        s_pingThread.join();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    if (g_hHudWnd) ::DestroyWindow(g_hHudWnd);
    if (g_hwnd) ::DestroyWindow(g_hwnd);
    ::UnregisterClassW(wcHud.lpszClassName, wcHud.hInstance);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    ShutdownGDIPlus();

    return 0;
}

int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return MainApp();
}

int main(int, char**)
{
    return MainApp();
}

bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer = nullptr;
    if (g_pSwapChain && SUCCEEDED(g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer))))
    {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_MOUSEACTIVATE:
        return MA_ACTIVATE;

    case WM_NCHITTEST:
    {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hWnd, &pt);

        float dpi = (var && var->c_dpi.dpi > 0.0f) ? var->c_dpi.dpi : 1.0f;
        float header_h = 75.0f * dpi;
        float sidebar_w = 110.0f * dpi;
        float menu_w = (set ? set->c_window.window_size.x : 860.f) * dpi;
        float menu_h = (set ? set->c_window.window_size.y : 630.f) * dpi;

        if (pt.x < 0 || pt.x > menu_w || pt.y < 0 || pt.y > menu_h)
            return HTCLIENT;

        // Top transparent header (with "Mvp Cheats Aimkill" title) is 100% draggable on click 1
        if (pt.y >= 0 && pt.y <= header_h)
            return HTCAPTION;

        // Sidebar empty areas outside tabs
        if (pt.x >= 0 && pt.x <= sidebar_w)
        {
            float start_tab_y = 80.0f * dpi;
            float tab_h = 62.0f * dpi;
            float tab_spacing = 14.0f * dpi;
            float tab_w = 76.0f * dpi;
            float tab_x = (sidebar_w - tab_w) * 0.5f;

            bool on_tab = false;
            for (int t = 0; t < 3; ++t)
            {
                float ty = start_tab_y + t * (tab_h + tab_spacing);
                if (pt.x >= tab_x && pt.x <= tab_x + tab_w && pt.y >= ty && pt.y <= ty + tab_h)
                {
                    on_tab = true;
                    break;
                }
            }

            if (!on_tab)
                return HTCAPTION;
        }

        return HTCLIENT;
    }
    case WM_GETMINMAXINFO:
    {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        int w = (int)set->c_window.window_size.x + 220;
        int h = (int)set->c_window.window_size.y + 160;
        mmi->ptMinTrackSize.x = w;
        mmi->ptMinTrackSize.y = h;
        mmi->ptMaxTrackSize.x = w;
        mmi->ptMaxTrackSize.y = h;
        return 0;
    }
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK HudWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_LBUTTONDOWN:
        ReleaseCapture();
        SendMessage(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return 0;
    case WM_DESTROY:
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
