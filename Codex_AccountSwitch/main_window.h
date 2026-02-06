#pragma once

#include <windows.h>

constexpr wchar_t kMainWindowClassName[] = L"Codex_AccountSwitch_MainWindow";
constexpr wchar_t kMainWindowTitle[] = L"Codex Account Switch";

bool RegisterMainWindowClass(HINSTANCE instance);
HWND CreateMainWindow(HINSTANCE instance, int nCmdShow);
