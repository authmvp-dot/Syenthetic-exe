#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")

#include "../../framework/settings/functions.h"
#include "../../framework/data/font.h"
#include "../../framework/data/texture.h"
#include "../../framework/data/imgui_freetype.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dx11.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "gdiplus.lib")

#include <windows.h>
#include <d3d11.h>
#include <tchar.h>
#include <d3dx11.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <ctime>
#include <string>
#include <vector>

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

void UpdateHudWindow(float framerate)
{
    if (!g_hHudWnd || !IsWindow(g_hHudWnd)) return;

    int width = 330;
    int height = 36;

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

        // 1. Dark frosted pill background
        Gdiplus::GraphicsPath path;
        float r = 6.0f;
        float d = r * 2.0f;
        float w = (float)width - 1.0f;
        float h = (float)height - 1.0f;
        path.AddArc(0.0f, 0.0f, d, d, 180.0f, 90.0f);
        path.AddArc(w - d, 0.0f, d, d, 270.0f, 90.0f);
        path.AddArc(w - d, h - d, d, d, 0.0f, 90.0f);
        path.AddArc(0.0f, h - d, d, d, 90.0f, 90.0f);
        path.CloseFigure();

        Gdiplus::SolidBrush bgBrush(Gdiplus::Color(235, 18, 18, 24));
        g.FillPath(&bgBrush, &path);

        // 2. Subtle border
        Gdiplus::Pen borderPen(Gdiplus::Color(180, 50, 50, 68), 1.0f);
        g.DrawPath(&borderPen, &path);

        // 3. Top accent glow line
        Gdiplus::Pen topGlowPen(Gdiplus::Color(220, 155, 115, 255), 1.2f);
        g.DrawLine(&topGlowPen, 12.0f, 1.0f, w - 12.0f, 1.0f);

        // 4. Typography
        Gdiplus::Font fontBrand(L"Segoe UI", 9.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
        Gdiplus::Font fontRegular(L"Segoe UI", 8.5f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);

        Gdiplus::SolidBrush brandBrush(Gdiplus::Color(255, 175, 135, 255));
        Gdiplus::SolidBrush textBrush(Gdiplus::Color(230, 215, 215, 225));
        Gdiplus::SolidBrush sepBrush(Gdiplus::Color(120, 85, 85, 105));

        // Get current time
        time_t rawtime = time(nullptr);
        struct tm timeinfo;
        localtime_s(&timeinfo, &rawtime);
        char time_str[16];
        strftime(time_str, sizeof(time_str), "%I:%M%p", &timeinfo);
        wchar_t wtime[16];
        MultiByteToWideChar(CP_ACP, 0, time_str, -1, wtime, 16);

        int fps_val = (int)roundf(framerate > 0.f ? framerate : 144.f);
        std::wstring wfps = std::to_wstring(fps_val) + L"FPS";

        struct Segment {
            std::wstring text;
            bool is_brand;
        };
        std::vector<Segment> segs = {
            { L"SYNTHETIC", true },
            { L"Server", false },
            { wfps, false },
            { L"64PING", false },
            { wtime, false }
        };

        float curX = 14.0f;
        float textY = (height - 18) * 0.5f;

        Gdiplus::StringFormat format;
        format.SetAlignment(Gdiplus::StringAlignmentNear);
        format.SetLineAlignment(Gdiplus::StringAlignmentCenter);

        for (size_t i = 0; i < segs.size(); i++)
        {
            Gdiplus::RectF bound;
            g.MeasureString(segs[i].text.c_str(), -1, segs[i].is_brand ? &fontBrand : &fontRegular, Gdiplus::PointF(0, 0), &bound);

            Gdiplus::RectF layoutRect(curX, textY, bound.Width + 2.0f, 18.0f);
            g.DrawString(segs[i].text.c_str(), -1, segs[i].is_brand ? &brandBrush : &textBrush, layoutRect, &format);
            curX += bound.Width + 6.0f;

            if (i + 1 < segs.size())
            {
                Gdiplus::RectF sepRect(curX, textY - 1.0f, 10.0f, 18.0f);
                g.DrawString(L"|", -1, &sepBrush, sepRect, &format);
                curX += 11.0f;
            }
        }
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

int MainApp()
{
    InitGDIPlus();

    int primary_w = GetSystemMetrics(SM_CXSCREEN);
    int primary_h = GetSystemMetrics(SM_CYSCREEN);
    if (primary_w <= 0) primary_w = 1920;
    if (primary_h <= 0) primary_h = 1080;

    int win_w = (int)set->c_window.window_size.x;
    int win_h = (int)set->c_window.window_size.y;
    int win_x = (primary_w - win_w) / 2;
    int win_y = (primary_h - win_h) / 2;

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

    // 2. Dedicated Draggable Watermark / FPS HUD Window
    WNDCLASSEXW wcHud = { sizeof(wcHud), CS_CLASSDC, HudWndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, LoadCursor(0, IDC_ARROW), nullptr, nullptr, L"SyntheticHudClass", nullptr };
    ::RegisterClassExW(&wcHud);

    int hud_w = 330;
    int hud_h = 36;
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

    D3DX11_IMAGE_LOAD_INFO info;
    ID3DX11ThreadPump* pump{ nullptr };
    if (set->c_texture.bg == nullptr) D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, background, sizeof(background), &info, pump, &set->c_texture.bg, 0);
    if (set->c_texture.logo == nullptr) D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, logo, sizeof(logo), &info, pump, &set->c_texture.logo, 0);

    bool done = false;
    bool menu_open = true;
    DWORD lastHudUpdate = 0;

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

        // Press INSERT to toggle menu on/off
        if (GetAsyncKeyState(VK_INSERT) & 1)
        {
            menu_open = !menu_open;
            ShowWindow(g_hwnd, menu_open ? SW_SHOW : SW_HIDE);
        }

        // Press END to exit
        if (GetAsyncKeyState(VK_END) & 1)
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

        if (!menu_open)
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

        gui->render();

        const float clear_color[4] = { 0.f, 0.f, 0.f, 0.f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        HRESULT hr = g_pSwapChain->Present(1, 0);
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

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
