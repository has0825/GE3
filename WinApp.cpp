#include "WinApp.h"
#include "externals/imgui/imgui_impl_win32.h"
#include <cassert>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

WinApp* WinApp::GetInstance() {
    static WinApp instance;
    return &instance;
}

LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return true;
    }

    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

void WinApp::Initialize(const wchar_t* title) {
    wc_.lpfnWndProc = WindowProc;
    wc_.lpszClassName = title;
    wc_.hInstance = GetModuleHandle(nullptr);
    wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);

    RegisterClassW(&wc_);

    RECT wrc = { 0, 0, kWindowWidth, kWindowHeight };
    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

    hwnd_ = CreateWindowW(
        wc_.lpszClassName,
        title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        wrc.right - wrc.left,
        wrc.bottom - wrc.top,
        nullptr,
        nullptr,
        wc_.hInstance,
        nullptr);

    assert(hwnd_ != nullptr);

    ShowWindow(hwnd_, SW_SHOW);
}

void WinApp::Finalize() {
    UnregisterClassW(wc_.lpszClassName, wc_.hInstance);
}

bool WinApp::ProcessMessages() {
    MSG msg{};
    if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (msg.message == WM_QUIT) {
        return true;
    }
    return false;
}