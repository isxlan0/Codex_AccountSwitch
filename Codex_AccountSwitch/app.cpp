#include "app.h"

#include "main_window.h"

#include <objbase.h>

#pragma comment(lib, "Ole32.lib")

namespace {
constexpr wchar_t kSingleInstanceMutexName[] =
    L"Local\\Codex_AccountSwitch_SingleInstanceMutex";

void ActivateExistingInstanceWindow()
{
    HWND hwnd = nullptr;
    for (int i = 0; i < 50 && hwnd == nullptr; ++i)
    {
        hwnd = FindWindowW(kMainWindowClassName, nullptr);
        if (hwnd == nullptr)
        {
            Sleep(50);
        }
    }

    if (hwnd != nullptr)
    {
        PostMessageW(hwnd, kActivateExistingInstanceMessage, 0, 0);
    }
}
} // namespace

int RunApplication(HINSTANCE instance, int nCmdShow)
{
    HANDLE singleInstanceMutex = CreateMutexW(nullptr, FALSE, kSingleInstanceMutexName);
    if (singleInstanceMutex == nullptr)
    {
        return 0;
    }

    const DWORD mutexError = GetLastError();
    if (mutexError == ERROR_ALREADY_EXISTS || mutexError == ERROR_ACCESS_DENIED)
    {
        ActivateExistingInstanceWindow();
        CloseHandle(singleInstanceMutex);
        return 0;
    }

    const HRESULT initResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initResult))
    {
        CloseHandle(singleInstanceMutex);
        return 0;
    }

    if (!RegisterMainWindowClass(instance))
    {
        CoUninitialize();
        CloseHandle(singleInstanceMutex);
        return 0;
    }

    if (CreateMainWindow(instance, nCmdShow) == nullptr)
    {
        CoUninitialize();
        CloseHandle(singleInstanceMutex);
        return 0;
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    CloseHandle(singleInstanceMutex);
    return static_cast<int>(msg.wParam);
}
