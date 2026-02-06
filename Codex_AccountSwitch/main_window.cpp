#include "main_window.h"

#include "resource.h"
#include "webview_host.h"

namespace
{
WebViewHost g_webviewHost;
}

LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_webviewHost.HandleWindowMessage(msg, wParam, lParam))
    {
        return 0;
    }

    switch (msg)
    {
    case WM_CREATE:
        g_webviewHost.Initialize(hwnd);
        return 0;

    case WM_SIZE:
        g_webviewHost.Resize(hwnd);
        return 0;

    case WM_DESTROY:
        g_webviewHost.Cleanup();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool RegisterMainWindowClass(HINSTANCE instance)
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = MainWindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = kMainWindowClassName;
    wc.hIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR));
    wc.hIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);

    return RegisterClassExW(&wc) != 0;
}

HWND CreateMainWindow(HINSTANCE instance, int nCmdShow)
{
    constexpr DWORD kWindowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME;

    HWND hwnd = CreateWindowExW(
        0,
        kMainWindowClassName,
        kMainWindowTitle,
        kWindowStyle,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1200,
        800,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (hwnd == nullptr)
    {
        return nullptr;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    return hwnd;
}
