#include "app.h"

#include "main_window.h"

#include <objbase.h>

#pragma comment(lib, "Ole32.lib")

int RunApplication(HINSTANCE instance, int nCmdShow)
{
    const HRESULT initResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initResult))
    {
        return 0;
    }

    if (!RegisterMainWindowClass(instance))
    {
        CoUninitialize();
        return 0;
    }

    if (CreateMainWindow(instance, nCmdShow) == nullptr)
    {
        CoUninitialize();
        return 0;
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    return static_cast<int>(msg.wParam);
}
