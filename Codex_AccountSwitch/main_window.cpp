#include "main_window.h"

#include "resource.h"
#include "webview_host.h"
#include "../window-settings.h"

#include <algorithm>

namespace {
WebViewHost g_webviewHost;

void FitWindowToWorkArea(HWND hwnd, RECT bounds) {
  MONITORINFO monitor{sizeof(MONITORINFO)};
  GetMonitorInfoW(MonitorFromRect(&bounds, MONITOR_DEFAULTTONEAREST), &monitor);
  const auto &work = monitor.rcWork;
  const LONG width = (std::min)(bounds.right - bounds.left, work.right - work.left);
  const LONG height = (std::min)(bounds.bottom - bounds.top, work.bottom - work.top);
  const LONG left = (std::clamp)(bounds.left, work.left, work.right - width);
  const LONG top = (std::clamp)(bounds.top, work.top, work.bottom - height);
  SetWindowPos(hwnd, nullptr, left, top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}
}

LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT msg, WPARAM wParam,
                                LPARAM lParam) {
  if (g_webviewHost.HandleWindowMessage(msg, wParam, lParam)) {
    return 0;
  }

  switch (msg) {
  case kActivateExistingInstanceMessage:
    if (IsIconic(hwnd)) {
      ShowWindow(hwnd, SW_RESTORE);
    } else {
      ShowWindow(hwnd, SW_SHOW);
    }
    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    return 0;

  case WM_CREATE:
    g_webviewHost.Initialize(hwnd);
    return 0;

  case WM_SIZE:
    g_webviewHost.Resize(hwnd);
    return 0;

  case WM_DPICHANGED:
    FitWindowToWorkArea(hwnd, *reinterpret_cast<const RECT *>(lParam));
    g_webviewHost.Resize(hwnd);
    return 0;

  case WM_CLOSE:
    if (GetPropW(hwnd, kExitWindowPropName) == nullptr) {
      if (g_webviewHost.ShouldCloseToTray()) {
        ShowWindow(hwnd, SW_HIDE);
        return 0;
      }
    }
    RemovePropW(hwnd, kExitWindowPropName);
    break;

  case WM_GETMINMAXINFO: {
    auto *mm = reinterpret_cast<MINMAXINFO *>(lParam);
    if (mm != nullptr) {
      MONITORINFO monitor{sizeof(MONITORINFO)};
      GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor);
      const auto dpi = GetDpiForWindow(hwnd);
      mm->ptMinTrackSize.x = (std::min)(MulDiv(WindowSettings::minimumWidth, dpi, USER_DEFAULT_SCREEN_DPI),
                                      static_cast<int>(monitor.rcWork.right - monitor.rcWork.left));
      mm->ptMinTrackSize.y = (std::min)(MulDiv(WindowSettings::minimumHeight, dpi, USER_DEFAULT_SCREEN_DPI),
                                      static_cast<int>(monitor.rcWork.bottom - monitor.rcWork.top));
    }
    return 0;
  }

  case WM_DESTROY:
    g_webviewHost.Cleanup();
    PostQuitMessage(0);
    return 0;
  }

  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool RegisterMainWindowClass(HINSTANCE instance) {
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = MainWindowProc;
  wc.hInstance = instance;
  wc.lpszClassName = kMainWindowClassName;
  wc.hIcon = static_cast<HICON>(
      LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 32, 32,
                 LR_DEFAULTCOLOR | LR_SHARED));
  wc.hIconSm = static_cast<HICON>(
      LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 16, 16,
                 LR_DEFAULTCOLOR | LR_SHARED));
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);

  return RegisterClassExW(&wc) != 0;
}

HWND CreateMainWindow(HINSTANCE instance, int nCmdShow) {
  constexpr DWORD kWindowStyle =
      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME;

  HWND hwnd = CreateWindowExW(0, kMainWindowClassName, kMainWindowTitle,
                              kWindowStyle, CW_USEDEFAULT, CW_USEDEFAULT, WindowSettings::initialWidth,
                              WindowSettings::initialHeight, nullptr, nullptr, instance, nullptr);

  if (hwnd == nullptr) {
    return nullptr;
  }

  RECT bounds{};
  GetWindowRect(hwnd, &bounds);
  const auto dpi = GetDpiForWindow(hwnd);
  bounds.right = bounds.left + MulDiv(WindowSettings::initialWidth, dpi, USER_DEFAULT_SCREEN_DPI);
  bounds.bottom = bounds.top + MulDiv(WindowSettings::initialHeight, dpi, USER_DEFAULT_SCREEN_DPI);
  FitWindowToWorkArea(hwnd, bounds);
  ShowWindow(hwnd, nCmdShow);
  UpdateWindow(hwnd);
  return hwnd;
}
