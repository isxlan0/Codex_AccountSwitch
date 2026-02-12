#pragma once

#include <atomic>
#include <chrono>
#include <string>

#include <windows.h>
#include <wrl.h>

#include "WebView2.h"

class WebViewHost
{
public:
    static constexpr UINT kMsgAsyncWebJson = WM_APP + 101;
    static constexpr UINT kMsgTrayNotify = WM_APP + 102;

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
    void SendAccountsList(bool refreshUsage = false, const std::wstring& targetName = L"", const std::wstring& targetGroup = L"") const;
    void SendAppInfo() const;
    void SendUpdateInfo() const;
    void SendConfig(bool firstRun) const;
    void SendLanguageIndex() const;
    void SendLanguagePack(const std::wstring& languageCode) const;
    void SendRefreshTimerState() const;
    void HandleAutoRefreshTick();
    void TriggerRefreshAll(bool notifyStatus = false);
    void TriggerRefreshCurrent();
    void EnsureTrayIcon();
    void RemoveTrayIcon();
    void ShowLowQuotaBalloon(const std::wstring& currentName, int currentQuota, const std::wstring& bestName, const std::wstring& bestGroup, int bestQuota);
    void HandleLowQuotaPromptClick();

    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
    Microsoft::WRL::ComPtr<ICoreWebView2> webview_;
    EventRegistrationToken webMessageToken_{};
    std::wstring userDataFolder_;
    HWND hwnd_ = nullptr;
    bool timerInitialized_ = false;
    bool trayIconAdded_ = false;
    std::atomic<bool> allRefreshRunning_{ false };
    std::atomic<bool> currentRefreshRunning_{ false };
    bool pendingLowQuotaPrompt_ = false;
    int allRefreshIntervalSec_ = 900;
    int currentRefreshIntervalSec_ = 300;
    int allRefreshRemainingSec_ = 900;
    int currentRefreshRemainingSec_ = 300;
    bool currentAutoRefreshEnabled_ = true;
    bool lowQuotaPromptEnabled_ = true;
    std::wstring pendingBestAccountName_;
    std::wstring pendingBestAccountGroup_;
    std::wstring pendingCurrentAccountName_;
    int pendingCurrentQuota_ = -1;
    int pendingBestQuota_ = -1;
    std::wstring lastLowQuotaPromptAccountKey_;
    std::wstring pendingImportAuthPath_;
    std::chrono::steady_clock::time_point lastLowQuotaPromptAt_{};
};
