#pragma once

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>

constexpr wchar_t kMainWindowClassName[] = L"Codex_AccountSwitch_MainWindow";
constexpr wchar_t kMainWindowTitle[] = L"Codex Account Switch";
constexpr UINT kActivateExistingInstanceMessage = WM_APP + 1;

bool RegisterMainWindowClass(HINSTANCE instance);
HWND CreateMainWindow(HINSTANCE instance, int nCmdShow);
