#pragma once
#include <windows.h>
#include <d3d11.h>
#include <imgui.h>

namespace FWork {

class Overlay {
public:
    static bool Initialize();
    static void Cleanup();
    static void UpdatePosition();
    static void RenderFrame();

    // Target emulator window
    static inline HWND target_hwnd = nullptr;
    static HWND FindTarget();

    // Overlay DirectX11 resources
    static inline HWND hWnd = nullptr;
    static inline ID3D11Device* device = nullptr;
    static inline ID3D11DeviceContext* device_context = nullptr;
    static inline IDXGISwapChain* swap_chain = nullptr;
    static inline ID3D11RenderTargetView* render_target_view = nullptr;
    static inline ImGuiContext* overlay_context = nullptr;

    static inline bool is_initialized = false;

private:
    static bool CreateDeviceD3D(HWND hWindow);
    static void CleanupDeviceD3D();
    static void CreateRenderTarget();
    static void CleanupRenderTarget();
    static LRESULT WINAPI WndProc(HWND hWindow, UINT msg, WPARAM wParam, LPARAM lParam);
};

} // namespace FWork
