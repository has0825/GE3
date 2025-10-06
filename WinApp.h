#pragma once
#include <Windows.h>

class WinApp {
public:
    static const int kWindowWidth = 1280;
    static const int kWindowHeight = 720;

    static WinApp* GetInstance();

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    void Initialize(const wchar_t* title);
    void Finalize();
    bool ProcessMessages();

    HWND GetHwnd() const { return hwnd_; }

private:
    WinApp() = default;
    ~WinApp() = default;
    WinApp(const WinApp&) = delete;
    const WinApp& operator=(const WinApp&) = delete;

    HWND hwnd_ = nullptr;
    WNDCLASSW wc_{};
};