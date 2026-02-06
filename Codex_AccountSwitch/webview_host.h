#pragma once

#include <string>

#include <windows.h>
#include <wrl.h>

#include "WebView2.h"

class WebViewHost
{
public:
    static constexpr UINT kMsgAsyncWebJson = WM_APP + 101;

    void Initialize(HWND hwnd);
    void Resize(HWND hwnd) const;
    void Cleanup();
    bool HandleWindowMessage(UINT msg, WPARAM wParam, LPARAM lParam);

private:
    static void ShowHr(HWND hwnd, const wchar_t* where, HRESULT hr);
    void RegisterWebMessageHandler(HWND hwnd);
    void HandleWebAction(HWND hwnd, const std::wstring& action, const std::wstring& rawMessage);
    void SendWebJson(const std::wstring& json) const;
    void SendWebStatus(const std::wstring& text, const std::wstring& level = L"info", const std::wstring& code = L"") const;
    void SendAccountsList() const;
    void SendAppInfo() const;
    void SendUpdateInfo() const;
    void SendConfig(bool firstRun) const;
    void SendLanguageIndex() const;
    void SendLanguagePack(const std::wstring& languageCode) const;

    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
    Microsoft::WRL::ComPtr<ICoreWebView2> webview_;
    EventRegistrationToken webMessageToken_{};
    std::wstring userDataFolder_;
    HWND hwnd_ = nullptr;
};
