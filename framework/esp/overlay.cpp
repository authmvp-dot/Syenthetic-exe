#include "../settings/functions.h"
#include "overlay.hpp"
#include "esp_visuals.h"
#include "esp_globals.h"
#include "esp_data.h"
#include "offsets.h"

#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <dwmapi.h>
#include <tlhelp32.h>
#include <iostream>
#include <vector>
#include <string>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwmapi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace FWork {

namespace {

BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam)
{
    char class_name[256] = {};
    GetClassNameA(hwnd, class_name, sizeof(class_name));

    char title[256] = {};
    GetWindowTextA(hwnd, title, sizeof(title));

    bool looks_like_game_view =
        strstr(class_name, "BlueStacks") ||
        strstr(class_name, "Qt5") ||
        strstr(class_name, "RenderWindow") ||
        strstr(class_name, "subWin") ||
        strstr(title, "BlueStacks") ||
        strstr(title, "MEmu") ||
        strstr(title, "LDPlayer") ||
        strstr(title, "Android");

    if (looks_like_game_view)
    {
        RECT r{};
        GetWindowRect(hwnd, &r);
        int w = r.right - r.left;
        int h = r.bottom - r.top;

        if (w >= 100 && h >= 100)
        {
            *((HWND*)lParam) = hwnd;
            return FALSE;
        }
    }
    return TRUE;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == (DWORD)lParam)
    {
        EnumChildWindows(hwnd, EnumChildProc, (LPARAM)&Overlay::target_hwnd);
        if (Overlay::target_hwnd)
            return FALSE;
    }
    return TRUE;
}

} // anonymous namespace

HWND Overlay::FindTarget()
{
    target_hwnd = nullptr;
    DWORD pid = 0;

    const std::vector<std::wstring> targets = {
        L"HD-Player.exe",       // BlueStacks
        L"MEmu.exe",            // MEmu
        L"dnplayer.exe"         // LDPlayer
    };

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE)
    {
        if (Process32FirstW(snapshot, &entry))
        {
            do
            {
                for (const auto& name : targets)
                {
                    if (_wcsicmp(entry.szExeFile, name.c_str()) == 0)
                    {
                        pid = entry.th32ProcessID;
                        break;
                    }
                }
                if (pid != 0)
                    break;
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
    }

    if (pid != 0)
    {
        EnumWindows(EnumWindowsProc, (LPARAM)pid);
    }

    return target_hwnd;
}

void Overlay::UpdatePosition()
{
    if (!target_hwnd || !IsWindow(target_hwnd))
    {
        target_hwnd = FindTarget();
    }

    if (target_hwnd && IsWindow(target_hwnd))
    {
        if (IsIconic(target_hwnd) || !IsWindowVisible(target_hwnd))
        {
            ShowWindow(hWnd, SW_HIDE);
            return;
        }

        HWND activeWnd = GetForegroundWindow();
        DWORD activePid = 0;
        GetWindowThreadProcessId(activeWnd, &activePid);

        DWORD targetPid = 0;
        GetWindowThreadProcessId(target_hwnd, &targetPid);

        bool isGameActive = (activePid == targetPid && activePid != 0);
        bool isOverlayActive = (activeWnd == hWnd);
        bool isMenuActive = (activeWnd == g_hwnd);

        if (isGameActive || isOverlayActive || isMenuActive)
        {
            ShowWindow(hWnd, SW_SHOWNOACTIVATE);
        }
        else
        {
            ShowWindow(hWnd, SW_HIDE);
            return;
        }

        RECT r{};
        if (GetWindowRect(target_hwnd, &r))
        {
            SetWindowPos(hWnd, HWND_TOPMOST, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOACTIVATE);
        }
    }
    else
    {
        // Fallback: If no emulator target found yet, cover primary display
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, screenW, screenH, SWP_NOACTIVATE);
        ShowWindow(hWnd, SW_SHOWNOACTIVATE);
    }
}

LRESULT WINAPI Overlay::WndProc(HWND hWindow, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWindow, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (device != nullptr && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            if (swap_chain)
                swap_chain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hWindow, msg, wParam, lParam);
}

bool Overlay::Initialize()
{
    if (is_initialized)
        return true;

    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "SyntheticOverlay";
    RegisterClassExA(&wc);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    hWnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
        wc.lpszClassName,
        "Synthetic Overlay",
        WS_POPUP,
        0, 0, screenWidth, screenHeight,
        nullptr, nullptr, wc.hInstance, nullptr
    );

    if (!hWnd)
    {
        return false;
    }

    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hWnd, &margins);
    SetLayeredWindowAttributes(hWnd, 0, 255, LWA_ALPHA);

    if (!CreateDeviceD3D(hWnd))
    {
        Cleanup();
        return false;
    }

    ShowWindow(hWnd, SW_SHOWNOACTIVATE);
    UpdateWindow(hWnd);

    ImGuiContext* prevContext = ImGui::GetCurrentContext();
    overlay_context = ImGui::CreateContext();
    ImGui::SetCurrentContext(overlay_context);

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.Fonts->AddFontDefault();

    ImGuiStyle& style = ImGui::GetStyle();
    style.AntiAliasedLines = true;
    style.AntiAliasedFill = true;

    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX11_Init(device, device_context);

    if (prevContext)
        ImGui::SetCurrentContext(prevContext);

    is_initialized = true;
    return true;
}

void Overlay::Cleanup()
{
    if (!is_initialized)
        return;

    if (overlay_context)
    {
        ImGui::SetCurrentContext(overlay_context);
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(overlay_context);
        overlay_context = nullptr;
    }

    CleanupDeviceD3D();

    if (hWnd)
    {
        DestroyWindow(hWnd);
        hWnd = nullptr;
    }

    UnregisterClassA("SyntheticOverlay", GetModuleHandleA(nullptr));
    is_initialized = false;
}

void Overlay::RenderFrame()
{
    if (!hWnd || !IsWindow(hWnd) || !is_initialized)
        return;

    UpdatePosition();

    ImGuiContext* prevContext = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(overlay_context);

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);

    ImGui::Begin("##synthetic_esp_overlay", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoSavedSettings
    );

    g_Globals.EspConfig.Width = (int)io.DisplaySize.x;
    g_Globals.EspConfig.Height = (int)io.DisplaySize.y;

    static bool lastCapture = false;
    if (lastCapture != g_Globals.General.Capture && hWnd)
    {
        lastCapture = g_Globals.General.Capture;
        SetWindowDisplayAffinity(hWnd, lastCapture ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
    }

    if (Offsets::Il2Cpp != 0 && !g_espShutDown.load() && g_Globals.Visuals.Enable && g_Globals.EspConfig.Matrix && g_Globals.EspConfig.InMatch)
    {
        ESP::Players();
    }

    ImGui::End();
    ImGui::Render();

    if (device_context && render_target_view)
    {
        const float clear_color[4] = { 0.f, 0.f, 0.f, 0.f };
        device_context->OMSetRenderTargets(1, &render_target_view, nullptr);
        device_context->ClearRenderTargetView(render_target_view, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    if (swap_chain)
    {
        swap_chain->Present(1, 0);
    }

    if (prevContext)
    {
        ImGui::SetCurrentContext(prevContext);
    }
}

bool Overlay::CreateDeviceD3D(HWND hWindow)
{
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWindow;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &swap_chain, &device, &featureLevel, &device_context) != S_OK)
    {
        return false;
    }

    CreateRenderTarget();
    return true;
}

void Overlay::CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (swap_chain) { swap_chain->Release(); swap_chain = nullptr; }
    if (device_context) { device_context->Release(); device_context = nullptr; }
    if (device) { device->Release(); device = nullptr; }
}

void Overlay::CreateRenderTarget()
{
    if (!swap_chain || !device) return;
    ID3D11Texture2D* pBackBuffer = nullptr;
    if (SUCCEEDED(swap_chain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer))) && pBackBuffer)
    {
        device->CreateRenderTargetView(pBackBuffer, nullptr, &render_target_view);
        pBackBuffer->Release();
    }
}

void Overlay::CleanupRenderTarget()
{
    if (render_target_view)
    {
        render_target_view->Release();
        render_target_view = nullptr;
    }
}

} // namespace FWork
