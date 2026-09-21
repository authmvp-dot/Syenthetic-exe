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

#include <windows.h>
#include <d3d11.h>
#include <tchar.h>
#include <d3dx11.h>
#include <dwmapi.h>
#include <dxgi.h>

static ImGuiContext* g_ctxMenu = nullptr;
static ImGuiContext* g_ctxSelection = nullptr;
static bool g_SwapChainMenuOccluded = false;
static bool g_SwapChainSelOccluded = false;
static UINT g_ResizeMenuWidth = 0, g_ResizeMenuHeight = 0;
static UINT g_ResizeSelWidth = 0, g_ResizeSelHeight = 0;

bool CreateDeviceD3D(HWND hWndMenu, HWND hWndSelection);
void CleanupDeviceD3D();
void CreateRenderTargetMenu();
void CleanupRenderTargetMenu();
void CreateRenderTargetSelection();
void CleanupRenderTargetSelection();
void LoadFonts(ImFontAtlas* atlas, float dpi_scale);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void LoadFonts(ImFontAtlas* atlas, float dpi_scale)
{
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    cfg.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint | ImGuiFreeTypeBuilderFlags_LightHinting | ImGuiFreeTypeBuilderFlags_LoadColor;

    set->c_font.inter_medium[0] = atlas->AddFontFromMemoryTTF(inter_medium, sizeof(inter_medium), 15.f * dpi_scale, &cfg, atlas->GetGlyphRangesCyrillic());
    set->c_font.inter_medium[1] = atlas->AddFontFromMemoryTTF(inter_medium, sizeof(inter_medium), 16.f * dpi_scale, &cfg, atlas->GetGlyphRangesCyrillic());

    set->c_font.icon[0] = atlas->AddFontFromMemoryTTF(icon, sizeof(icon), 14.f * dpi_scale, &cfg, atlas->GetGlyphRangesCyrillic());
    set->c_font.icon[1] = atlas->AddFontFromMemoryTTF(icon, sizeof(icon), 16.f * dpi_scale, &cfg, atlas->GetGlyphRangesCyrillic());
    set->c_font.icon[2] = atlas->AddFontFromMemoryTTF(icon, sizeof(icon), 40.f * dpi_scale, &cfg, atlas->GetGlyphRangesCyrillic());
    set->c_font.icon[3] = atlas->AddFontFromMemoryTTF(icon2, sizeof(icon2), 15.f * dpi_scale, &cfg, atlas->GetGlyphRangesCyrillic());
    set->c_font.icon[4] = atlas->AddFontFromMemoryTTF(icon2, sizeof(icon2), 9.f * dpi_scale, &cfg, atlas->GetGlyphRangesCyrillic());
    set->c_font.icon[5] = atlas->AddFontFromMemoryTTF(icon2, sizeof(icon2), 76.f * dpi_scale, &cfg, atlas->GetGlyphRangesCyrillic());
    set->c_font.icon[6] = atlas->AddFontFromMemoryTTF(icon2, sizeof(icon2), 96.f * dpi_scale, &cfg, atlas->GetGlyphRangesCyrillic());

    set->c_font.name = atlas->AddFontFromMemoryTTF(inter_medium, sizeof(inter_medium), 18.f * dpi_scale, &cfg, atlas->GetGlyphRangesCyrillic());
}

int MainApp()
{
    int primary_w = GetSystemMetrics(SM_CXSCREEN);
    int primary_h = GetSystemMetrics(SM_CYSCREEN);
    if (primary_w <= 0) primary_w = 1920;
    if (primary_h <= 0) primary_h = 1080;

    int menu_w = (int)set->c_window.window_size.x;
    int menu_h = (int)set->c_window.window_size.y;
    int sel_w = 270;
    int sel_h = 260;

    int total_combined_w = sel_w + 20 + menu_w;
    int start_x = (primary_w - total_combined_w) / 2;
    if (start_x < 20) start_x = 20;

    int sel_x = start_x;
    int sel_y = (primary_h - menu_h) / 2 + 185;

    int menu_x = start_x + sel_w + 20;
    int menu_y = (primary_h - menu_h) / 2;

    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"SyntheticWindowClass", nullptr };
    ::RegisterClassExW(&wc);

    g_hMenuWnd = ::CreateWindowExW(
        WS_EX_LAYERED,
        wc.lpszClassName,
        L"Synthetic Menu",
        WS_POPUP,
        menu_x, menu_y, menu_w, menu_h,
        nullptr, nullptr, wc.hInstance, nullptr
    );

    g_hSelectionWnd = ::CreateWindowExW(
        WS_EX_LAYERED,
        wc.lpszClassName,
        L"Synthetic Tab",
        WS_POPUP,
        sel_x, sel_y, sel_w, sel_h,
        nullptr, nullptr, wc.hInstance, nullptr
    );

    if (!g_hMenuWnd || !g_hSelectionWnd)
    {
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(g_hMenuWnd, &margins);
    DwmExtendFrameIntoClientArea(g_hSelectionWnd, &margins);

    int menuRadius = (int)(set->c_window.general_rounding * 2);
    HRGN hMenuRgn = CreateRoundRectRgn(0, 0, menu_w + 1, menu_h + 1, menuRadius, menuRadius);
    SetWindowRgn(g_hMenuWnd, hMenuRgn, TRUE);

    HRGN hSelRgn = CreateRoundRectRgn(0, 0, sel_w + 1, sel_h + 1, 40, 40);
    SetWindowRgn(g_hSelectionWnd, hSelRgn, TRUE);

    if (!CreateDeviceD3D(g_hMenuWnd, g_hSelectionWnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(g_hMenuWnd, SW_SHOWDEFAULT);
    ::UpdateWindow(g_hMenuWnd);
    ::ShowWindow(g_hSelectionWnd, SW_SHOWDEFAULT);
    ::UpdateWindow(g_hSelectionWnd);

    IMGUI_CHECKVERSION();
    ImFontAtlas* shared_atlas = new ImFontAtlas();
    LoadFonts(shared_atlas, var->c_dpi.dpi);
    shared_atlas->Build();

    // 1. Menu ImGui Context
    g_ctxMenu = ImGui::CreateContext(shared_atlas);
    ImGui::SetCurrentContext(g_ctxMenu);
    ImGuiIO& ioMenu = ImGui::GetIO();
    ioMenu.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ioMenu.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    ImGui_ImplWin32_Init(g_hMenuWnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // 2. Selection ImGui Context
    g_ctxSelection = ImGui::CreateContext(shared_atlas);
    ImGui::SetCurrentContext(g_ctxSelection);
    ImGuiIO& ioSel = ImGui::GetIO();
    ioSel.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ioSel.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    ImGui_ImplWin32_Init(g_hSelectionWnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    D3DX11_IMAGE_LOAD_INFO info;
    ID3DX11ThreadPump* pump{ nullptr };
    if (set->c_texture.bg == nullptr) D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, background, sizeof(background), &info, pump, &set->c_texture.bg, 0);
    if (set->c_texture.logo == nullptr) D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, logo, sizeof(logo), &info, pump, &set->c_texture.logo, 0);

    bool done = false;
    bool menu_open = true;

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
            ShowWindow(g_hMenuWnd, menu_open ? SW_SHOW : SW_HIDE);
            ShowWindow(g_hSelectionWnd, menu_open ? SW_SHOW : SW_HIDE);
        }

        // Press END to exit
        if (GetAsyncKeyState(VK_END) & 1)
        {
            done = true;
            break;
        }

        if (!menu_open)
        {
            ::Sleep(16);
            continue;
        }

        // Handle window resizing if needed
        if (g_ResizeMenuWidth != 0 && g_ResizeMenuHeight != 0)
        {
            CleanupRenderTargetMenu();
            g_pSwapChainMenu->ResizeBuffers(0, g_ResizeMenuWidth, g_ResizeMenuHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeMenuWidth = g_ResizeMenuHeight = 0;
            CreateRenderTargetMenu();
        }

        if (g_ResizeSelWidth != 0 && g_ResizeSelHeight != 0)
        {
            CleanupRenderTargetSelection();
            g_pSwapChainSelection->ResizeBuffers(0, g_ResizeSelWidth, g_ResizeSelHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeSelWidth = g_ResizeSelHeight = 0;
            CreateRenderTargetSelection();
        }

        // Handle DPI changes
        if (var->c_dpi.dpi_changed)
        {
            var->c_dpi.dpi = var->c_dpi.dpi_saved / 100.f;

            shared_atlas->Clear();
            LoadFonts(shared_atlas, var->c_dpi.dpi);
            shared_atlas->Build();

            ImGui::SetCurrentContext(g_ctxMenu);
            ImGui_ImplDX11_CreateDeviceObjects();

            ImGui::SetCurrentContext(g_ctxSelection);
            ImGui_ImplDX11_CreateDeviceObjects();

            var->c_dpi.dpi_changed = false;
        }

        const float clear_color[4] = { 0.f, 0.f, 0.f, 0.f };

        // 1. Render Menu Window
        if (!g_SwapChainMenuOccluded)
        {
            ImGui::SetCurrentContext(g_ctxMenu);
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();

            gui->render_menu();

            g_pd3dDeviceContext->OMSetRenderTargets(1, &g_pRenderTargetMenu, nullptr);
            g_pd3dDeviceContext->ClearRenderTargetView(g_pRenderTargetMenu, clear_color);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

            HRESULT hrMenu = g_pSwapChainMenu->Present(1, 0);
            g_SwapChainMenuOccluded = (hrMenu == DXGI_STATUS_OCCLUDED);
        }
        else
        {
            if (g_pSwapChainMenu->Present(0, DXGI_PRESENT_TEST) != DXGI_STATUS_OCCLUDED)
                g_SwapChainMenuOccluded = false;
        }

        // 2. Render Selection Window (Circle Tab)
        if (!g_SwapChainSelOccluded)
        {
            ImGui::SetCurrentContext(g_ctxSelection);
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();

            gui->render_selection();

            g_pd3dDeviceContext->OMSetRenderTargets(1, &g_pRenderTargetSelection, nullptr);
            g_pd3dDeviceContext->ClearRenderTargetView(g_pRenderTargetSelection, clear_color);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

            HRESULT hrSel = g_pSwapChainSelection->Present(1, 0);
            g_SwapChainSelOccluded = (hrSel == DXGI_STATUS_OCCLUDED);
        }
        else
        {
            if (g_pSwapChainSelection->Present(0, DXGI_PRESENT_TEST) != DXGI_STATUS_OCCLUDED)
                g_SwapChainSelOccluded = false;
        }
    }

    ImGui::SetCurrentContext(g_ctxSelection);
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext(g_ctxSelection);

    ImGui::SetCurrentContext(g_ctxMenu);
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext(g_ctxMenu);

    CleanupDeviceD3D();
    ::DestroyWindow(g_hSelectionWnd);
    ::DestroyWindow(g_hMenuWnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

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

bool CreateDeviceD3D(HWND hWndMenu, HWND hWndSelection)
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
    sd.OutputWindow = hWndMenu;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChainMenu, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChainMenu, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    g_pSwapChain = g_pSwapChainMenu;

    IDXGIDevice* pDXGIDevice = nullptr;
    if (SUCCEEDED(g_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&pDXGIDevice)))
    {
        IDXGIAdapter* pDXGIAdapter = nullptr;
        if (SUCCEEDED(pDXGIDevice->GetAdapter(&pDXGIAdapter)))
        {
            IDXGIFactory* pIDXGIFactory = nullptr;
            if (SUCCEEDED(pDXGIAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&pIDXGIFactory)))
            {
                DXGI_SWAP_CHAIN_DESC sd_sel = sd;
                sd_sel.OutputWindow = hWndSelection;
                pIDXGIFactory->CreateSwapChain(g_pd3dDevice, &sd_sel, &g_pSwapChainSelection);
                pIDXGIFactory->Release();
            }
            pDXGIAdapter->Release();
        }
        pDXGIDevice->Release();
    }

    CreateRenderTargetMenu();
    CreateRenderTargetSelection();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTargetMenu();
    CleanupRenderTargetSelection();
    if (g_pSwapChainSelection) { g_pSwapChainSelection->Release(); g_pSwapChainSelection = nullptr; }
    if (g_pSwapChainMenu) { g_pSwapChainMenu->Release(); g_pSwapChainMenu = nullptr; }
    g_pSwapChain = nullptr;
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTargetMenu()
{
    ID3D11Texture2D* pBackBuffer = nullptr;
    if (g_pSwapChainMenu && SUCCEEDED(g_pSwapChainMenu->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer))))
    {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetMenu);
        g_mainRenderTargetView = g_pRenderTargetMenu;
        pBackBuffer->Release();
    }
}

void CleanupRenderTargetMenu()
{
    if (g_pRenderTargetMenu) { g_pRenderTargetMenu->Release(); g_pRenderTargetMenu = nullptr; }
    g_mainRenderTargetView = nullptr;
}

void CreateRenderTargetSelection()
{
    ID3D11Texture2D* pBackBuffer = nullptr;
    if (g_pSwapChainSelection && SUCCEEDED(g_pSwapChainSelection->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer))))
    {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetSelection);
        pBackBuffer->Release();
    }
}

void CleanupRenderTargetSelection()
{
    if (g_pRenderTargetSelection) { g_pRenderTargetSelection->Release(); g_pRenderTargetSelection = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (hWnd == g_hMenuWnd && g_ctxMenu)
    {
        ImGui::SetCurrentContext(g_ctxMenu);
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;
    }
    else if (hWnd == g_hSelectionWnd && g_ctxSelection)
    {
        ImGui::SetCurrentContext(g_ctxSelection);
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;
    }

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        if (hWnd == g_hMenuWnd)
        {
            g_ResizeMenuWidth = (UINT)LOWORD(lParam);
            g_ResizeMenuHeight = (UINT)HIWORD(lParam);
        }
        else if (hWnd == g_hSelectionWnd)
        {
            g_ResizeSelWidth = (UINT)LOWORD(lParam);
            g_ResizeSelHeight = (UINT)HIWORD(lParam);
        }
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
