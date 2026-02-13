#include "webview_host.h"

#include "app_version.h"
#include "file_utils.h"
#include "resource.h"
#include "update_checker.h"

#include <commdlg.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <objbase.h>
#include <regex>
#include <shellapi.h>
#include <tlhelp32.h>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>
#include <dwmapi.h>
#include <winhttp.h>

using namespace Microsoft::WRL;
namespace fs = std::filesystem;

#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Dwmapi.lib")
#pragma comment(lib, "Winhttp.lib")

namespace
{
std::wstring EscapeJsonString(const std::wstring& input);
std::wstring UnescapeJsonString(const std::wstring& input);
std::wstring ToLowerCopy(const std::wstring& value);
std::wstring FormatFileTime(const fs::path& path);
bool ReadUtf8File(const fs::path& file, std::wstring& out);
bool WriteUtf8File(const fs::path& file, const std::wstring& content);
bool OpenExternalUrlByExplorer(const std::wstring& url);

constexpr wchar_t kUsageHost[] = L"chatgpt.com";
constexpr wchar_t kUsagePath[] = L"/backend-api/wham/usage";
constexpr wchar_t kUsageDefaultCodexVersion[] = L"0.98.0";
constexpr wchar_t kUsageVsCodeVersion[] = L"0.4.71";
constexpr UINT_PTR kTimerAutoRefresh = 8201;
constexpr int kDefaultAllRefreshMinutes = 15;
constexpr int kDefaultCurrentRefreshMinutes = 5;
constexpr int kMinRefreshMinutes = 1;
constexpr int kMaxRefreshMinutes = 240;
constexpr int kLowQuotaThresholdPercent = 20;
constexpr int kLowQuotaPromptCooldownSeconds = 30 * 60;

int ClampRefreshMinutes(const int v, const int fallback)
{
    int x = v;
    if (x < kMinRefreshMinutes || x > kMaxRefreshMinutes)
    {
        x = fallback;
    }
    if (x < kMinRefreshMinutes)
    {
        x = kMinRefreshMinutes;
    }
    if (x > kMaxRefreshMinutes)
    {
        x = kMaxRefreshMinutes;
    }
    return x;
}

std::wstring ExtractJsonField(const std::wstring& json, const std::wstring& key)
{
    const std::wstring pattern = L"\"" + key + L"\"";
    const size_t keyPos = json.find(pattern);
    if (keyPos == std::wstring::npos)
    {
        return L"";
    }

    const size_t colonPos = json.find(L':', keyPos + pattern.size());
    if (colonPos == std::wstring::npos)
    {
        return L"";
    }

    const size_t firstQuotePos = json.find(L'"', colonPos + 1);
    if (firstQuotePos == std::wstring::npos)
    {
        return L"";
    }

    const size_t secondQuotePos = json.find(L'"', firstQuotePos + 1);
    if (secondQuotePos == std::wstring::npos)
    {
        return L"";
    }

    return json.substr(firstQuotePos + 1, secondQuotePos - firstQuotePos - 1);
}

std::wstring ToFileUrl(const std::wstring& path)
{
    std::wstring normalized = path;
    for (wchar_t& ch : normalized)
    {
        if (ch == L'\\')
        {
            ch = L'/';
        }
    }
    return L"file:///" + normalized;
}

std::wstring FindWebUiIndexPath()
{
    wchar_t modulePath[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) == 0)
    {
        return L"";
    }

    const fs::path exeDir = fs::path(modulePath).parent_path();
    const fs::path currentDir = fs::current_path();

    const fs::path candidates[] = {
        exeDir / L"webui" / L"index.html",
        exeDir / L".." / L".." / L"webui" / L"index.html",
        currentDir / L"webui" / L"index.html",
    };

    for (const auto& candidate : candidates)
    {
        std::error_code ec;
        if (fs::exists(candidate, ec) && !ec)
        {
            return candidate.lexically_normal().wstring();
        }
    }

    return L"";
}

std::wstring SanitizeAccountName(const std::wstring& name)
{
    if (name.empty())
    {
        return L"";
    }

    std::wstring out;
    out.reserve(name.size());
    for (const wchar_t ch : name)
    {
        if (ch == L'<' || ch == L'>' || ch == L':' || ch == L'"' || ch == L'/' || ch == L'\\' || ch == L'|' || ch == L'?' || ch == L'*')
        {
            out.push_back(L'_');
        }
        else
        {
            out.push_back(ch);
        }
    }
    return out;
}

std::wstring NormalizeGroup(const std::wstring& group)
{
    const std::wstring lower = ToLowerCopy(group);
    if (lower == L"enterprise")
    {
        return L"business";
    }
    if (lower == L"personal" || lower == L"business" ||
        lower == L"free" || lower == L"plus" || lower == L"team" || lower == L"pro")
    {
        return lower;
    }
    return L"personal";
}

bool IsPlanGroup(const std::wstring& group)
{
    const std::wstring lower = ToLowerCopy(group);
    return lower == L"free" || lower == L"plus" || lower == L"team" || lower == L"pro";
}

bool EqualsIgnoreCase(const std::wstring& a, const std::wstring& b)
{
    return _wcsicmp(a.c_str(), b.c_str()) == 0;
}

fs::path GetWorkspaceRoot()
{
    const std::wstring webUiPath = FindWebUiIndexPath();
    if (webUiPath.empty())
    {
        return fs::current_path();
    }

    fs::path webUi = fs::path(webUiPath).lexically_normal();
    return webUi.parent_path().parent_path();
}

fs::path GetLegacyDataRoot()
{
    return GetWorkspaceRoot();
}

fs::path GetUserDataRoot()
{
    wchar_t* localAppData = nullptr;
    size_t required = 0;
    if (_wdupenv_s(&localAppData, &required, L"LOCALAPPDATA") == 0 &&
        localAppData != nullptr && *localAppData != L'\0')
    {
        fs::path root = fs::path(localAppData) / L"Codex Account Switch";
        free(localAppData);
        return root;
    }
    free(localAppData);
    return GetLegacyDataRoot();
}

fs::path GetBackupsDir()
{
    return GetUserDataRoot() / L"backups";
}

fs::path GetGroupDir(const std::wstring& group)
{
    return GetBackupsDir() / NormalizeGroup(group);
}

fs::path GetIndexPath()
{
    return GetBackupsDir() / L"index.json";
}

fs::path GetConfigPath()
{
    return GetUserDataRoot() / L"config.json";
}

fs::path GetLegacyBackupsDir()
{
    return GetLegacyDataRoot() / L"backups";
}

fs::path GetLegacyIndexPath()
{
    return GetLegacyBackupsDir() / L"index.json";
}

fs::path GetLegacyConfigPath()
{
    return GetLegacyDataRoot() / L"config.json";
}

struct AppConfig
{
    std::wstring language = L"zh-CN";
    int languageIndex = 0;
    std::wstring ideExe = L"Code.exe";
    std::wstring theme = L"auto";
    bool autoUpdate = true;
    bool autoRefreshCurrent = true;
    bool lowQuotaAutoPrompt = true;
    int autoRefreshAllMinutes = kDefaultAllRefreshMinutes;
    int autoRefreshCurrentMinutes = kDefaultCurrentRefreshMinutes;
    std::wstring lastSwitchedAccount;
    std::wstring lastSwitchedGroup;
    std::wstring lastSwitchedAt;
};

struct LanguageMeta
{
    std::wstring code;
    std::wstring name;
    std::wstring file;
};

fs::path GetLangIndexPath()
{
    const std::wstring webUiPath = FindWebUiIndexPath();
    if (webUiPath.empty())
    {
        return GetWorkspaceRoot() / L"webui" / L"lang" / L"index.json";
    }
    return fs::path(webUiPath).parent_path() / L"lang" / L"index.json";
}

std::vector<LanguageMeta> LoadLanguageIndexList()
{
    std::vector<LanguageMeta> list;
    std::wstring json;
    if (!ReadUtf8File(GetLangIndexPath(), json))
    {
        return list;
    }

    const std::wregex itemRe(LR"LANG(\{\s*"code"\s*:\s*"((?:\\.|[^"])*)"\s*,\s*"name"\s*:\s*"((?:\\.|[^"])*)"\s*,\s*"file"\s*:\s*"((?:\\.|[^"])*)"\s*\})LANG");
    for (std::wsregex_iterator it(json.begin(), json.end(), itemRe), end; it != end; ++it)
    {
        LanguageMeta row;
        row.code = UnescapeJsonString((*it)[1].str());
        row.name = UnescapeJsonString((*it)[2].str());
        row.file = UnescapeJsonString((*it)[3].str());
        if (!row.code.empty() && !row.file.empty())
        {
            list.push_back(row);
        }
    }
    return list;
}

int FindLanguageIndexByCode(const std::vector<LanguageMeta>& list, const std::wstring& code)
{
    for (size_t i = 0; i < list.size(); ++i)
    {
        if (_wcsicmp(list[i].code.c_str(), code.c_str()) == 0)
        {
            return static_cast<int>(i);
        }
    }
    return 0;
}

std::wstring FindLanguageCodeByIndex(const std::vector<LanguageMeta>& list, const int index)
{
    if (index >= 0 && index < static_cast<int>(list.size()))
    {
        return list[static_cast<size_t>(index)].code;
    }
    return list.empty() ? L"zh-CN" : list.front().code;
}

int ExtractJsonIntField(const std::wstring& json, const std::wstring& key, const int fallback)
{
    const std::wstring pattern = L"\"" + key + L"\"";
    const size_t keyPos = json.find(pattern);
    if (keyPos == std::wstring::npos)
    {
        return fallback;
    }
    const size_t colonPos = json.find(L':', keyPos + pattern.size());
    if (colonPos == std::wstring::npos)
    {
        return fallback;
    }

    size_t p = colonPos + 1;
    while (p < json.size() && iswspace(json[p]))
    {
        ++p;
    }
    bool neg = false;
    if (p < json.size() && json[p] == L'-')
    {
        neg = true;
        ++p;
    }
    int value = 0;
    bool hasDigit = false;
    while (p < json.size() && json[p] >= L'0' && json[p] <= L'9')
    {
        hasDigit = true;
        value = value * 10 + static_cast<int>(json[p] - L'0');
        ++p;
    }
    if (!hasDigit)
    {
        return fallback;
    }
    return neg ? -value : value;
}

bool ExtractJsonBoolField(const std::wstring& json, const std::wstring& key, const bool fallback)
{
    const std::wstring pattern = L"\"" + key + L"\"";
    const size_t keyPos = json.find(pattern);
    if (keyPos == std::wstring::npos)
    {
        return fallback;
    }
    const size_t colonPos = json.find(L':', keyPos + pattern.size());
    if (colonPos == std::wstring::npos)
    {
        return fallback;
    }

    size_t p = colonPos + 1;
    while (p < json.size() && iswspace(json[p]))
    {
        ++p;
    }

    if (json.compare(p, 4, L"true") == 0)
    {
        return true;
    }
    if (json.compare(p, 5, L"false") == 0)
    {
        return false;
    }

    const size_t firstQuotePos = json.find(L'"', p);
    if (firstQuotePos == std::wstring::npos)
    {
        return fallback;
    }
    const size_t secondQuotePos = json.find(L'"', firstQuotePos + 1);
    if (secondQuotePos == std::wstring::npos)
    {
        return fallback;
    }
    const std::wstring text = json.substr(firstQuotePos + 1, secondQuotePos - firstQuotePos - 1);
    if (_wcsicmp(text.c_str(), L"true") == 0 || text == L"1")
    {
        return true;
    }
    if (_wcsicmp(text.c_str(), L"false") == 0 || text == L"0")
    {
        return false;
    }
    return fallback;
}

long long ExtractJsonInt64Field(const std::wstring& json, const std::wstring& key, const long long fallback)
{
    const std::wstring pattern = L"\"" + key + L"\"";
    const size_t keyPos = json.find(pattern);
    if (keyPos == std::wstring::npos)
    {
        return fallback;
    }
    const size_t colonPos = json.find(L':', keyPos + pattern.size());
    if (colonPos == std::wstring::npos)
    {
        return fallback;
    }

    size_t p = colonPos + 1;
    while (p < json.size() && iswspace(json[p]))
    {
        ++p;
    }
    bool neg = false;
    if (p < json.size() && json[p] == L'-')
    {
        neg = true;
        ++p;
    }
    long long value = 0;
    bool hasDigit = false;
    while (p < json.size() && json[p] >= L'0' && json[p] <= L'9')
    {
        hasDigit = true;
        value = value * 10 + static_cast<long long>(json[p] - L'0');
        ++p;
    }
    if (!hasDigit)
    {
        return fallback;
    }
    return neg ? -value : value;
}

bool ExtractJsonObjectField(const std::wstring& json, const std::wstring& key, std::wstring& out)
{
    out.clear();
    const std::wstring pattern = L"\"" + key + L"\"";
    const size_t keyPos = json.find(pattern);
    if (keyPos == std::wstring::npos)
    {
        return false;
    }

    const size_t colonPos = json.find(L':', keyPos + pattern.size());
    if (colonPos == std::wstring::npos)
    {
        return false;
    }

    size_t p = colonPos + 1;
    while (p < json.size() && iswspace(json[p]))
    {
        ++p;
    }
    if (p >= json.size() || json[p] != L'{')
    {
        return false;
    }

    size_t end = p;
    int depth = 0;
    bool inString = false;
    bool escape = false;
    for (; end < json.size(); ++end)
    {
        const wchar_t ch = json[end];
        if (inString)
        {
            if (escape)
            {
                escape = false;
            }
            else if (ch == L'\\')
            {
                escape = true;
            }
            else if (ch == L'"')
            {
                inString = false;
            }
            continue;
        }

        if (ch == L'"')
        {
            inString = true;
            continue;
        }
        if (ch == L'{')
        {
            ++depth;
            continue;
        }
        if (ch == L'}')
        {
            --depth;
            if (depth == 0)
            {
                out = json.substr(p, end - p + 1);
                return true;
            }
        }
    }
    return false;
}

bool ExtractJsonArrayField(const std::wstring& json, const std::wstring& key, std::wstring& out)
{
    out.clear();
    const std::wstring pattern = L"\"" + key + L"\"";
    const size_t keyPos = json.find(pattern);
    if (keyPos == std::wstring::npos)
    {
        return false;
    }

    const size_t colonPos = json.find(L':', keyPos + pattern.size());
    if (colonPos == std::wstring::npos)
    {
        return false;
    }

    size_t p = colonPos + 1;
    while (p < json.size() && iswspace(json[p]))
    {
        ++p;
    }
    if (p >= json.size() || json[p] != L'[')
    {
        return false;
    }

    size_t end = p;
    int depth = 0;
    bool inString = false;
    bool escape = false;
    for (; end < json.size(); ++end)
    {
        const wchar_t ch = json[end];
        if (inString)
        {
            if (escape)
            {
                escape = false;
            }
            else if (ch == L'\\')
            {
                escape = true;
            }
            else if (ch == L'"')
            {
                inString = false;
            }
            continue;
        }

        if (ch == L'"')
        {
            inString = true;
            continue;
        }
        if (ch == L'[')
        {
            ++depth;
            continue;
        }
        if (ch == L']')
        {
            --depth;
            if (depth == 0)
            {
                out = json.substr(p, end - p + 1);
                return true;
            }
        }
    }
    return false;
}

std::vector<std::wstring> ExtractTopLevelObjectsFromArray(const std::wstring& arrayText)
{
    std::vector<std::wstring> objects;
    if (arrayText.empty())
    {
        return objects;
    }

    bool inString = false;
    bool escape = false;
    int depth = 0;
    size_t objStart = std::wstring::npos;

    for (size_t i = 0; i < arrayText.size(); ++i)
    {
        const wchar_t ch = arrayText[i];
        if (inString)
        {
            if (escape)
            {
                escape = false;
            }
            else if (ch == L'\\')
            {
                escape = true;
            }
            else if (ch == L'"')
            {
                inString = false;
            }
            continue;
        }

        if (ch == L'"')
        {
            inString = true;
            continue;
        }
        if (ch == L'{')
        {
            if (depth == 0)
            {
                objStart = i;
            }
            ++depth;
            continue;
        }
        if (ch == L'}')
        {
            --depth;
            if (depth == 0 && objStart != std::wstring::npos)
            {
                objects.push_back(arrayText.substr(objStart, i - objStart + 1));
                objStart = std::wstring::npos;
            }
        }
    }

    return objects;
}

std::wstring ToLowerCopy(const std::wstring& value)
{
    std::wstring out = value;
    std::transform(out.begin(), out.end(), out.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(towlower(ch));
    });
    return out;
}

std::wstring DetectCpuArchTag()
{
    SYSTEM_INFO si{};
    GetNativeSystemInfo(&si);
    switch (si.wProcessorArchitecture)
    {
    case PROCESSOR_ARCHITECTURE_AMD64:
        return L"x86_64";
    case PROCESSOR_ARCHITECTURE_INTEL:
        return L"x86";
    case PROCESSOR_ARCHITECTURE_ARM64:
        return L"arm64";
    case PROCESSOR_ARCHITECTURE_ARM:
        return L"arm";
    default:
        return L"unknown";
    }
}

std::wstring DetectWindowsVersionTag()
{
    struct RtlOsVersionInfo
    {
        ULONG dwOSVersionInfoSize;
        ULONG dwMajorVersion;
        ULONG dwMinorVersion;
        ULONG dwBuildNumber;
        ULONG dwPlatformId;
        WCHAR szCSDVersion[128];
    };

    using RtlGetVersionFn = LONG(WINAPI*)(RtlOsVersionInfo*);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll != nullptr)
    {
        const auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"));
        if (rtlGetVersion != nullptr)
        {
            RtlOsVersionInfo vi{};
            vi.dwOSVersionInfoSize = sizeof(vi);
            if (rtlGetVersion(&vi) == 0)
            {
                return std::to_wstring(vi.dwMajorVersion) + L"." +
                    std::to_wstring(vi.dwMinorVersion) + L"." +
                    std::to_wstring(vi.dwBuildNumber);
            }
        }
    }

    return L"10.0.0";
}

std::wstring ReadCodexLatestVersion()
{
    wchar_t* userProfile = nullptr;
    size_t required = 0;
    if (_wdupenv_s(&userProfile, &required, L"USERPROFILE") != 0 || userProfile == nullptr || *userProfile == L'\0')
    {
        free(userProfile);
        return kUsageDefaultCodexVersion;
    }

    const fs::path versionPath = fs::path(userProfile) / L".codex" / L"version.json";
    free(userProfile);

    std::wstring json;
    if (!ReadUtf8File(versionPath, json))
    {
        return kUsageDefaultCodexVersion;
    }

    const std::wstring latest = ExtractJsonField(json, L"latest_version");
    return latest.empty() ? std::wstring(kUsageDefaultCodexVersion) : latest;
}

std::wstring BuildUsageUserAgent()
{
    return L"codex_vscode/" + ReadCodexLatestVersion() +
        L" (Windows " + DetectWindowsVersionTag() + L"; " + DetectCpuArchTag() +
        L") unknown (VS Code; " + std::wstring(kUsageVsCodeVersion) + L")";
}

std::wstring FromUtf8(const std::string& text)
{
    if (text.empty())
    {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0)
    {
        return {};
    }
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size);
    return out;
}

struct UsageSnapshot
{
    bool ok = false;
    std::wstring error;
    std::wstring planType;
    std::wstring email;
    int primaryUsedPercent = -1;
    int secondaryUsedPercent = -1;
    long long primaryResetAfterSeconds = -1;
    long long secondaryResetAfterSeconds = -1;
    long long primaryResetAt = -1;
    long long secondaryResetAt = -1;
};

int RemainingPercentFromUsed(const int usedPercent)
{
    if (usedPercent < 0 || usedPercent > 100)
    {
        return -1;
    }
    return 100 - usedPercent;
}

std::wstring NormalizePlanType(const std::wstring& planType)
{
    const std::wstring lower = ToLowerCopy(planType);
    if (lower == L"free" || lower == L"plus" || lower == L"team" || lower == L"pro")
    {
        return lower;
    }
    return L"";
}

std::wstring GroupFromPlanType(const std::wstring& planType)
{
    const std::wstring normalized = NormalizePlanType(planType);
    if (!normalized.empty())
    {
        return normalized;
    }
    return NormalizeGroup(planType);
}

bool ParseUsagePayload(const std::wstring& body, UsageSnapshot& out)
{
    out.planType = ExtractJsonField(body, L"plan_type");
    const std::wstring normalizedPlanType = NormalizePlanType(out.planType);
    if (normalizedPlanType.empty())
    {
        out.error = L"unknown_plan_type:" + out.planType;
        return false;
    }
    out.planType = normalizedPlanType;
    out.email = ExtractJsonField(body, L"email");

    std::wstring rateLimitObj;
    if (!ExtractJsonObjectField(body, L"rate_limit", rateLimitObj))
    {
        out.error = L"rate_limit_missing";
        return false;
    }

    std::wstring primaryObj;
    if (ExtractJsonObjectField(rateLimitObj, L"primary_window", primaryObj))
    {
        out.primaryUsedPercent = ExtractJsonIntField(primaryObj, L"used_percent", -1);
        out.primaryResetAfterSeconds = ExtractJsonInt64Field(primaryObj, L"reset_after_seconds", -1);
        out.primaryResetAt = ExtractJsonInt64Field(primaryObj, L"reset_at", -1);
    }

    std::wstring secondaryObj;
    if (ExtractJsonObjectField(rateLimitObj, L"secondary_window", secondaryObj))
    {
        out.secondaryUsedPercent = ExtractJsonIntField(secondaryObj, L"used_percent", -1);
        out.secondaryResetAfterSeconds = ExtractJsonInt64Field(secondaryObj, L"reset_after_seconds", -1);
        out.secondaryResetAt = ExtractJsonInt64Field(secondaryObj, L"reset_at", -1);
    }

    out.ok = true;
    return true;
}

bool RequestUsageByToken(const std::wstring& accessToken, const std::wstring& accountId, std::wstring& responseBody, std::wstring& error)
{
    responseBody.clear();
    error.clear();
    if (accessToken.empty() || accountId.empty())
    {
        error = L"missing_token_or_account_id";
        return false;
    }

    const std::wstring userAgent = BuildUsageUserAgent();

    HINTERNET hSession = WinHttpOpen(
        userAgent.c_str(),
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (hSession == nullptr)
    {
        error = L"WinHttpOpen_failed";
        return false;
    }

    HINTERNET hConnect = WinHttpConnect(hSession, kUsageHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (hConnect == nullptr)
    {
        WinHttpCloseHandle(hSession);
        error = L"WinHttpConnect_failed";
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        kUsagePath,
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (hRequest == nullptr)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        error = L"WinHttpOpenRequest_failed";
        return false;
    }

    DWORD decompression = WINHTTP_DECOMPRESSION_FLAG_GZIP | WINHTTP_DECOMPRESSION_FLAG_DEFLATE;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_DECOMPRESSION, &decompression, sizeof(decompression));

    const std::wstring headers =
        L"host: chatgpt.com\r\n"
        L"connection: keep-alive\r\n"
        L"Authorization: Bearer " + accessToken + L"\r\n"
        L"ChatGPT-Account-Id: " + accountId + L"\r\n"
        L"originator: codex_vscode\r\n"
        L"User-Agent: " + userAgent + L"\r\n"
        L"accept: */*\r\n"
        L"accept-language: *\r\n"
        L"sec-fetch-mode: cors\r\n"
        L"accept-encoding: gzip, deflate\r\n";

    bool ok = WinHttpSendRequest(
        hRequest,
        headers.c_str(),
        static_cast<DWORD>(-1L),
        WINHTTP_NO_REQUEST_DATA,
        0,
        0,
        0) == TRUE;
    if (ok)
    {
        ok = WinHttpReceiveResponse(hRequest, nullptr) == TRUE;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (ok)
    {
        ok = WinHttpQueryHeaders(
                 hRequest,
                 WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                 WINHTTP_HEADER_NAME_BY_INDEX,
                 &statusCode,
                 &statusSize,
                 WINHTTP_NO_HEADER_INDEX) == TRUE &&
             statusCode >= 200 && statusCode < 300;
    }
    if (!ok)
    {
        error = L"http_status_not_ok";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    std::string payload;
    while (ok)
    {
        DWORD size = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &size))
        {
            ok = false;
            break;
        }
        if (size == 0)
        {
            break;
        }

        std::string chunk(size, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(hRequest, chunk.data(), size, &read))
        {
            ok = false;
            break;
        }
        chunk.resize(read);
        payload += chunk;
    }

    if (!ok || payload.empty())
    {
        error = L"empty_response";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    responseBody = FromUtf8(payload);
    if (responseBody.empty())
    {
        error = L"response_utf8_decode_failed";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
}

bool ReadAuthTokenInfo(const fs::path& authPath, std::wstring& accessToken, std::wstring& accountId)
{
    accessToken.clear();
    accountId.clear();
    std::wstring authJson;
    if (!ReadUtf8File(authPath, authJson))
    {
        return false;
    }
    accessToken = ExtractJsonField(authJson, L"access_token");
    accountId = ExtractJsonField(authJson, L"account_id");
    return !accessToken.empty() && !accountId.empty();
}

bool QueryUsageFromAuthFile(const fs::path& authPath, UsageSnapshot& out)
{
    out = UsageSnapshot{};
    std::wstring accessToken;
    std::wstring accountId;
    if (!ReadAuthTokenInfo(authPath, accessToken, accountId))
    {
        out.error = L"auth_token_missing";
        return false;
    }

    std::wstring body;
    std::wstring requestError;
    if (!RequestUsageByToken(accessToken, accountId, body, requestError))
    {
        out.error = requestError;
        return false;
    }

    if (!ParseUsagePayload(body, out))
    {
        if (out.error.empty())
        {
            out.error = L"usage_parse_failed";
        }
        return false;
    }

    out.ok = true;
    return true;
}

bool ReadJsonStringToken(const std::wstring& text, size_t& i, std::wstring& outRaw)
{
    outRaw.clear();
    if (i >= text.size() || text[i] != L'"')
    {
        return false;
    }
    ++i;
    bool escape = false;
    while (i < text.size())
    {
        const wchar_t ch = text[i++];
        if (escape)
        {
            outRaw.push_back(ch);
            escape = false;
            continue;
        }
        if (ch == L'\\')
        {
            outRaw.push_back(ch);
            escape = true;
            continue;
        }
        if (ch == L'"')
        {
            return true;
        }
        outRaw.push_back(ch);
    }
    return false;
}

void SkipJsonValue(const std::wstring& text, size_t& i)
{
    if (i >= text.size())
    {
        return;
    }
    if (text[i] == L'"')
    {
        std::wstring ignored;
        ReadJsonStringToken(text, i, ignored);
        return;
    }

    const wchar_t open = text[i];
    wchar_t close = 0;
    if (open == L'{') close = L'}';
    if (open == L'[') close = L']';
    if (close == 0)
    {
        while (i < text.size() && text[i] != L',' && text[i] != L'}')
        {
            ++i;
        }
        return;
    }

    int depth = 0;
    bool inString = false;
    bool escape = false;
    while (i < text.size())
    {
        const wchar_t ch = text[i++];
        if (inString)
        {
            if (escape)
            {
                escape = false;
            }
            else if (ch == L'\\')
            {
                escape = true;
            }
            else if (ch == L'"')
            {
                inString = false;
            }
            continue;
        }
        if (ch == L'"')
        {
            inString = true;
            continue;
        }
        if (ch == open)
        {
            ++depth;
            continue;
        }
        if (ch == close)
        {
            --depth;
            if (depth <= 0)
            {
                return;
            }
        }
    }
}

bool ParseFlatJsonStringMap(const std::wstring& json, std::vector<std::pair<std::wstring, std::wstring>>& outPairs)
{
    outPairs.clear();
    size_t i = 0;
    while (i < json.size() && json[i] != L'{')
    {
        ++i;
    }
    if (i >= json.size())
    {
        return false;
    }
    ++i;

    while (i < json.size())
    {
        while (i < json.size() && (iswspace(json[i]) || json[i] == L','))
        {
            ++i;
        }
        if (i >= json.size() || json[i] == L'}')
        {
            break;
        }

        std::wstring keyRaw;
        if (!ReadJsonStringToken(json, i, keyRaw))
        {
            ++i;
            continue;
        }
        while (i < json.size() && iswspace(json[i]))
        {
            ++i;
        }
        if (i >= json.size() || json[i] != L':')
        {
            continue;
        }
        ++i;
        while (i < json.size() && iswspace(json[i]))
        {
            ++i;
        }
        if (i >= json.size())
        {
            break;
        }

        if (json[i] != L'"')
        {
            SkipJsonValue(json, i);
            continue;
        }

        std::wstring valueRaw;
        if (!ReadJsonStringToken(json, i, valueRaw))
        {
            continue;
        }
        outPairs.emplace_back(UnescapeJsonString(keyRaw), UnescapeJsonString(valueRaw));
    }

    return !outPairs.empty();
}

bool LoadLanguageStrings(const std::wstring& code, std::vector<std::pair<std::wstring, std::wstring>>& outPairs, std::wstring& resolvedCode)
{
    outPairs.clear();
    resolvedCode.clear();
    const auto list = LoadLanguageIndexList();
    if (list.empty())
    {
        return false;
    }

    int idx = FindLanguageIndexByCode(list, code);
    if (idx < 0 || idx >= static_cast<int>(list.size()))
    {
        idx = 0;
    }
    const auto& lang = list[static_cast<size_t>(idx)];
    resolvedCode = lang.code;

    fs::path langFile = GetLangIndexPath().parent_path() / lang.file;
    std::wstring json;
    if (!ReadUtf8File(langFile, json))
    {
        return false;
    }

    return ParseFlatJsonStringMap(json, outPairs);
}

const std::vector<std::wstring>& GetSupportedIdeList()
{
    static const std::vector<std::wstring> kList = {
        L"Code.exe",
        L"Trae.exe",
        L"Kiro.exe",
        L"Antigravity.exe"
    };
    return kList;
}

std::wstring NormalizeIdeExe(const std::wstring& ideExe)
{
    if (ideExe.empty())
    {
        return L"Code.exe";
    }

    for (const auto& it : GetSupportedIdeList())
    {
        if (_wcsicmp(it.c_str(), ideExe.c_str()) == 0)
        {
            return it;
        }
    }
    return L"Code.exe";
}

std::wstring NormalizeTheme(const std::wstring& theme)
{
    if (_wcsicmp(theme.c_str(), L"light") == 0)
    {
        return L"light";
    }
    if (_wcsicmp(theme.c_str(), L"dark") == 0)
    {
        return L"dark";
    }
    return L"auto";
}

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

bool IsWindowsAppsDarkModeEnabled()
{
    DWORD value = 1;
    DWORD size = sizeof(value);
    const LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme",
        RRF_RT_REG_DWORD,
        nullptr,
        &value,
        &size);
    if (status != ERROR_SUCCESS)
    {
        return false;
    }
    return value == 0;
}

void ApplyWindowTitleTheme(const HWND hwnd, const std::wstring& themeMode)
{
    if (hwnd == nullptr)
    {
        return;
    }

    const std::wstring mode = NormalizeTheme(themeMode);
    const BOOL useDark = (mode == L"dark" || (mode == L"auto" && IsWindowsAppsDarkModeEnabled())) ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));
}

std::wstring Utf8ToWide(const std::string& text)
{
    if (text.empty())
    {
        return L"";
    }

    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0)
    {
        return L"";
    }

    std::wstring out(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size);
    return out;
}

std::string WideToUtf8(const std::wstring& text)
{
    if (text.empty())
    {
        return "";
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0)
    {
        return "";
    }

    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size, nullptr, nullptr);
    return out;
}

bool ReadUtf8File(const fs::path& file, std::wstring& out)
{
    std::ifstream in(file, std::ios::binary);
    if (!in)
    {
        return false;
    }

    std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    out = Utf8ToWide(bytes);
    return true;
}

bool WriteUtf8File(const fs::path& file, const std::wstring& content)
{
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);

    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        return false;
    }

    const std::string bytes = WideToUtf8(content);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return out.good();
}

bool OpenExternalUrlByExplorer(const std::wstring& url)
{
    if (url.empty())
    {
        return false;
    }
    const HINSTANCE result = ShellExecuteW(
        nullptr,
        L"open",
        L"explorer.exe",
        url.c_str(),
        nullptr,
        SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
}

bool LoadConfig(AppConfig& out)
{
    out = AppConfig{};
    std::wstring json;
    if (!ReadUtf8File(GetConfigPath(), json))
    {
        if (!ReadUtf8File(GetLegacyConfigPath(), json))
        {
            return false;
        }
    }

    const std::wstring language = ExtractJsonField(json, L"language");
    const int languageIndex = ExtractJsonIntField(json, L"languageIndex", 0);
    const std::wstring ide = ExtractJsonField(json, L"ideExe");
    const std::wstring theme = ExtractJsonField(json, L"theme");
    const bool autoUpdate = ExtractJsonBoolField(json, L"autoUpdate", true);
    const bool autoRefreshCurrent = ExtractJsonBoolField(json, L"autoRefreshCurrent", true);
    const bool lowQuotaAutoPrompt = ExtractJsonBoolField(json, L"lowQuotaAutoPrompt", true);
    const int autoRefreshAllMinutes = ExtractJsonIntField(json, L"autoRefreshAllMinutes", kDefaultAllRefreshMinutes);
    const int autoRefreshCurrentMinutes = ExtractJsonIntField(json, L"autoRefreshCurrentMinutes", kDefaultCurrentRefreshMinutes);
    const std::wstring lastAccount = ExtractJsonField(json, L"lastSwitchedAccount");
    const std::wstring lastGroup = ExtractJsonField(json, L"lastSwitchedGroup");
    const std::wstring lastAt = ExtractJsonField(json, L"lastSwitchedAt");

    out.languageIndex = languageIndex < 0 ? 0 : languageIndex;
    if (!language.empty())
    {
        out.language = language;
    }
    else
    {
        const auto langs = LoadLanguageIndexList();
        out.language = FindLanguageCodeByIndex(langs, out.languageIndex);
    }
    const auto langs = LoadLanguageIndexList();
    out.languageIndex = FindLanguageIndexByCode(langs, out.language);
    out.ideExe = NormalizeIdeExe(ide);
    out.theme = NormalizeTheme(theme);
    out.autoUpdate = autoUpdate;
    out.autoRefreshCurrent = autoRefreshCurrent;
    out.lowQuotaAutoPrompt = lowQuotaAutoPrompt;
    out.autoRefreshAllMinutes = ClampRefreshMinutes(autoRefreshAllMinutes, kDefaultAllRefreshMinutes);
    out.autoRefreshCurrentMinutes = ClampRefreshMinutes(autoRefreshCurrentMinutes, kDefaultCurrentRefreshMinutes);
    out.lastSwitchedAccount = lastAccount;
    out.lastSwitchedGroup = NormalizeGroup(lastGroup);
    out.lastSwitchedAt = lastAt;
    return true;
}

bool SaveConfig(const AppConfig& in)
{
    const AppConfig cfg = [&]() {
        AppConfig tmp = in;
        if (tmp.language.empty())
        {
            tmp.language = L"zh-CN";
        }
        const auto langs = LoadLanguageIndexList();
        tmp.languageIndex = FindLanguageIndexByCode(langs, tmp.language);
        tmp.ideExe = NormalizeIdeExe(tmp.ideExe);
        tmp.theme = NormalizeTheme(tmp.theme);
        tmp.autoRefreshAllMinutes = ClampRefreshMinutes(tmp.autoRefreshAllMinutes, kDefaultAllRefreshMinutes);
        tmp.autoRefreshCurrentMinutes = ClampRefreshMinutes(tmp.autoRefreshCurrentMinutes, kDefaultCurrentRefreshMinutes);
        tmp.lastSwitchedGroup = NormalizeGroup(tmp.lastSwitchedGroup);
        return tmp;
    }();

    std::wstringstream ss;
    ss << L"{\n";
    ss << L"  \"language\": \"" << EscapeJsonString(cfg.language) << L"\",\n";
    ss << L"  \"languageIndex\": " << cfg.languageIndex << L",\n";
    ss << L"  \"ideExe\": \"" << EscapeJsonString(cfg.ideExe) << L"\",\n";
    ss << L"  \"theme\": \"" << EscapeJsonString(cfg.theme) << L"\",\n";
    ss << L"  \"autoUpdate\": " << (cfg.autoUpdate ? L"true" : L"false") << L",\n";
    ss << L"  \"autoRefreshCurrent\": " << (cfg.autoRefreshCurrent ? L"true" : L"false") << L",\n";
    ss << L"  \"lowQuotaAutoPrompt\": " << (cfg.lowQuotaAutoPrompt ? L"true" : L"false") << L",\n";
    ss << L"  \"autoRefreshAllMinutes\": " << cfg.autoRefreshAllMinutes << L",\n";
    ss << L"  \"autoRefreshCurrentMinutes\": " << cfg.autoRefreshCurrentMinutes << L",\n";
    ss << L"  \"lastSwitchedAccount\": \"" << EscapeJsonString(cfg.lastSwitchedAccount) << L"\",\n";
    ss << L"  \"lastSwitchedGroup\": \"" << EscapeJsonString(cfg.lastSwitchedGroup) << L"\",\n";
    ss << L"  \"lastSwitchedAt\": \"" << EscapeJsonString(cfg.lastSwitchedAt) << L"\"\n";
    ss << L"}\n";
    return WriteUtf8File(GetConfigPath(), ss.str());
}

bool EnsureConfigExists(bool& created)
{
    created = false;
    AppConfig cfg;
    if (LoadConfig(cfg))
    {
        return true;
    }
    created = true;
    return SaveConfig(AppConfig{});
}

std::wstring GetUserAuthPath()
{
    wchar_t* userProfile = nullptr;
    size_t required = 0;
    if (_wdupenv_s(&userProfile, &required, L"USERPROFILE") != 0 || userProfile == nullptr || *userProfile == L'\0')
    {
        free(userProfile);
        return L"";
    }

    fs::path path = fs::path(userProfile) / L".codex" / L"auth.json";
    free(userProfile);
    return path.wstring();
}

std::wstring QuotePowerShellLiteral(const std::wstring& text)
{
    std::wstring escaped;
    escaped.reserve(text.size() + 8);
    for (wchar_t ch : text)
    {
        if (ch == L'\'')
        {
            escaped += L"''";
        }
        else
        {
            escaped.push_back(ch);
        }
    }
    return L"'" + escaped + L"'";
}

bool RunPowerShellCommand(const std::wstring& command)
{
    std::wstring cmdLine = L"powershell -NoProfile -ExecutionPolicy Bypass -Command " + command;
    std::vector<wchar_t> buffer(cmdLine.begin(), cmdLine.end());
    buffer.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    const BOOL ok = CreateProcessW(
        nullptr,
        buffer.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi);

    if (!ok)
    {
        return false;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return exitCode == 0;
}

std::wstring GetFileNameOnly(const std::wstring& pathLike)
{
    if (pathLike.empty())
    {
        return L"";
    }
    fs::path p(pathLike);
    const std::wstring name = p.filename().wstring();
    return name.empty() ? pathLike : name;
}

std::wstring FindIdeExePath(const std::wstring& ideExe)
{
    wchar_t found[MAX_PATH]{};
    if (SearchPathW(nullptr, ideExe.c_str(), nullptr, MAX_PATH, found, nullptr) > 0)
    {
        return found;
    }

    wchar_t* localAppData = nullptr;
    size_t required = 0;
    if (_wdupenv_s(&localAppData, &required, L"LOCALAPPDATA") == 0 && localAppData != nullptr && *localAppData != L'\0')
    {
        fs::path candidate = fs::path(localAppData) / L"Programs" / L"Microsoft VS Code" / ideExe;
        free(localAppData);
        std::error_code ec;
        if (fs::exists(candidate, ec) && !ec)
        {
            return candidate.wstring();
        }
    }
    else
    {
        free(localAppData);
    }

    return L"";
}

bool StopProcessByName(const std::wstring& processName)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool found = false;
    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            if (_wcsicmp(entry.szExeFile, processName.c_str()) == 0)
            {
                found = true;
                HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
                if (process != nullptr)
                {
                    TerminateProcess(process, 0);
                    CloseHandle(process);
                }
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return found;
}

bool LaunchDetachedExe(const std::wstring& exePath)
{
    std::wstring cmd = L"\"" + exePath + L"\"";
    std::vector<wchar_t> buffer(cmd.begin(), cmd.end());
    buffer.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    const BOOL ok = CreateProcessW(
        exePath.c_str(),
        buffer.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS,
        nullptr,
        nullptr,
        &si,
        &pi);
    if (!ok)
    {
        return false;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

bool LaunchDetachedCommand(const std::wstring& commandLine)
{
    std::vector<wchar_t> buffer(commandLine.begin(), commandLine.end());
    buffer.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    const BOOL ok = CreateProcessW(
        nullptr,
        buffer.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS,
        nullptr,
        nullptr,
        &si,
        &pi);
    if (!ok)
    {
        return false;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

std::wstring GetIdeDisplayName(const std::wstring& ideExe)
{
    if (_wcsicmp(ideExe.c_str(), L"Trae.exe") == 0) return L"Trae";
    if (_wcsicmp(ideExe.c_str(), L"Kiro.exe") == 0) return L"Kiro";
    if (_wcsicmp(ideExe.c_str(), L"Antigravity.exe") == 0) return L"Antigravity";
    return L"VSCode";
}

std::wstring FindConfiguredIdePath(const std::wstring& ideExe)
{
    std::wstring path = FindIdeExePath(ideExe);
    if (!path.empty())
    {
        return path;
    }

    wchar_t* localAppData = nullptr;
    size_t required = 0;
    if (_wdupenv_s(&localAppData, &required, L"LOCALAPPDATA") == 0 && localAppData != nullptr && *localAppData != L'\0')
    {
        const fs::path root = fs::path(localAppData) / L"Programs";
        free(localAppData);
        std::error_code ec;
        if (fs::exists(root, ec) && !ec)
        {
            for (const auto& dir : fs::directory_iterator(root, ec))
            {
                if (ec || !dir.is_directory())
                {
                    continue;
                }
                const fs::path candidate = dir.path() / ideExe;
                if (fs::exists(candidate, ec) && !ec)
                {
                    return candidate.wstring();
                }
            }
        }
    }
    else
    {
        free(localAppData);
    }

    return L"";
}

bool RestartConfiguredIde(std::wstring& ideDisplay)
{
    AppConfig cfg;
    LoadConfig(cfg);
    const std::wstring ideExe = NormalizeIdeExe(cfg.ideExe);
    ideDisplay = GetIdeDisplayName(ideExe);
    const std::wstring exePath = FindConfiguredIdePath(ideExe);

    StopProcessByName(GetFileNameOnly(ideExe));
    Sleep(450);

    if (!exePath.empty() && LaunchDetachedExe(exePath))
    {
        return true;
    }

    // Fallback: let system resolve executable name from PATH / App Paths.
    return LaunchDetachedCommand(L"\"" + ideExe + L"\"");
}

bool PickOpenZipPath(HWND hwnd, std::wstring& outPath)
{
    std::vector<wchar_t> fileName(32768, L'\0');
    static constexpr wchar_t kZipFilter[] = L"Zip Files (*.zip)\0*.zip\0All Files (*.*)\0*.*\0\0";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = IsWindow(hwnd) ? hwnd : nullptr;
    ofn.lpstrFile = fileName.data();
    ofn.nMaxFile = static_cast<DWORD>(fileName.size());
    ofn.lpstrFilter = kZipFilter;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = L"zip";

    if (!GetOpenFileNameW(&ofn))
    {
        const DWORD dlgErr = CommDlgExtendedError();
        if (dlgErr != 0)
        {
            OutputDebugStringW((L"[PickOpenZipPath] CommDlgExtendedError=" + std::to_wstring(dlgErr) + L"\n").c_str());
        }
        return false;
    }

    outPath = fileName.data();
    return true;
}

bool PickSaveZipPath(HWND hwnd, std::wstring& outPath)
{
    wchar_t fileName[MAX_PATH] = L"codex_accounts.zip";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Zip Files (*.zip)\0*.zip\0All Files (*.*)\0*.*\0";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"zip";

    if (!GetSaveFileNameW(&ofn))
    {
        return false;
    }

    outPath = fileName;
    return true;
}

struct IndexEntry
{
    std::wstring name;
    std::wstring group;
    std::wstring path;
    std::wstring updatedAt;
    bool quotaUsageOk = false;
    std::wstring quotaPlanType;
    std::wstring quotaEmail;
    int quota5hRemainingPercent = -1;
    int quota7dRemainingPercent = -1;
    long long quota5hResetAfterSeconds = -1;
    long long quota7dResetAfterSeconds = -1;
    long long quota5hResetAt = -1;
    long long quota7dResetAt = -1;
};

struct IndexData
{
    std::vector<IndexEntry> accounts;
    std::wstring currentName;
    std::wstring currentGroup;
};

std::wstring MakeRelativeAuthPath(const std::wstring& group, const std::wstring& name)
{
    return L"backups/" + NormalizeGroup(group) + L"/" + name + L"/auth.json";
}

fs::path ResolveAuthPathFromIndex(const IndexEntry& item)
{
    std::wstring normalized = item.path;
    std::replace(normalized.begin(), normalized.end(), L'/', L'\\');
    fs::path relOrAbs(normalized);
    if (relOrAbs.is_absolute())
    {
        return relOrAbs;
    }

    fs::path primary = GetUserDataRoot() / relOrAbs;
    if (fs::exists(primary))
    {
        return primary;
    }

    return GetLegacyDataRoot() / relOrAbs;
}

bool SyncIndexEntryToGroup(IndexEntry& row, const std::wstring& targetGroup)
{
    const std::wstring normalizedTarget = NormalizeGroup(targetGroup);
    const std::wstring safeName = SanitizeAccountName(row.name);
    if (!IsPlanGroup(normalizedTarget) || safeName.empty())
    {
        return false;
    }

    const fs::path targetAuth = GetGroupDir(normalizedTarget) / safeName / L"auth.json";
    std::error_code ec;
    fs::create_directories(targetAuth.parent_path(), ec);
    if (ec)
    {
        return false;
    }

    if (!fs::exists(targetAuth))
    {
        const fs::path sourceAuth = ResolveAuthPathFromIndex(row);
        if (!fs::exists(sourceAuth))
        {
            return false;
        }
        ec.clear();
        fs::copy_file(sourceAuth, targetAuth, fs::copy_options::overwrite_existing, ec);
        if (ec)
        {
            return false;
        }
    }

    row.group = normalizedTarget;
    row.path = MakeRelativeAuthPath(normalizedTarget, safeName);
    return true;
}

bool LoadIndex(IndexData& out)
{
    out.accounts.clear();
    out.currentName.clear();
    out.currentGroup.clear();

    std::wstring json;
    if (!ReadUtf8File(GetIndexPath(), json))
    {
        if (!ReadUtf8File(GetLegacyIndexPath(), json))
        {
            return false;
        }
    }

    std::wstring accountsArray;
    if (ExtractJsonArrayField(json, L"accounts", accountsArray))
    {
        const auto objects = ExtractTopLevelObjectsFromArray(accountsArray);
        for (const auto& itemJson : objects)
        {
            IndexEntry row;
            row.name = UnescapeJsonString(ExtractJsonField(itemJson, L"name"));
            row.group = NormalizeGroup(UnescapeJsonString(ExtractJsonField(itemJson, L"group")));
            row.path = UnescapeJsonString(ExtractJsonField(itemJson, L"path"));
            row.updatedAt = UnescapeJsonString(ExtractJsonField(itemJson, L"updatedAt"));
            row.quotaUsageOk = ExtractJsonBoolField(itemJson, L"usageOk", false);
            row.quotaPlanType = UnescapeJsonString(ExtractJsonField(itemJson, L"planType"));
            row.quotaEmail = UnescapeJsonString(ExtractJsonField(itemJson, L"email"));
            row.quota5hRemainingPercent = ExtractJsonIntField(itemJson, L"quota5hRemainingPercent", -1);
            row.quota7dRemainingPercent = ExtractJsonIntField(itemJson, L"quota7dRemainingPercent", -1);
            row.quota5hResetAfterSeconds = ExtractJsonInt64Field(itemJson, L"quota5hResetAfterSeconds", -1);
            row.quota7dResetAfterSeconds = ExtractJsonInt64Field(itemJson, L"quota7dResetAfterSeconds", -1);
            row.quota5hResetAt = ExtractJsonInt64Field(itemJson, L"quota5hResetAt", -1);
            row.quota7dResetAt = ExtractJsonInt64Field(itemJson, L"quota7dResetAt", -1);
            if (!row.name.empty() && !row.path.empty())
            {
                out.accounts.push_back(row);
            }
        }
    }

    const std::wregex currentRe(LR"INDEX("current"\s*:\s*\{\s*"name"\s*:\s*"((?:\\.|[^"])*)"\s*,\s*"group"\s*:\s*"((?:\\.|[^"])*)"\s*\})INDEX");
    std::wsmatch m;
    if (std::regex_search(json, m, currentRe))
    {
        out.currentName = UnescapeJsonString(m[1].str());
        out.currentGroup = NormalizeGroup(UnescapeJsonString(m[2].str()));
    }

    return true;
}

bool SaveIndex(const IndexData& data)
{
    std::wstringstream ss;
    ss << L"{\n";
    ss << L"  \"current\": {\"name\":\"" << EscapeJsonString(data.currentName)
       << L"\",\"group\":\"" << EscapeJsonString(NormalizeGroup(data.currentGroup)) << L"\"},\n";
    ss << L"  \"accounts\": [\n";
    for (size_t i = 0; i < data.accounts.size(); ++i)
    {
        const auto& row = data.accounts[i];
        ss << L"    {\"name\":\"" << EscapeJsonString(row.name)
            << L"\",\"group\":\"" << EscapeJsonString(NormalizeGroup(row.group))
            << L"\",\"path\":\"" << EscapeJsonString(row.path)
            << L"\",\"updatedAt\":\"" << EscapeJsonString(row.updatedAt)
            << L"\",\"usageOk\":" << (row.quotaUsageOk ? L"true" : L"false")
            << L",\"planType\":\"" << EscapeJsonString(row.quotaPlanType)
            << L"\",\"email\":\"" << EscapeJsonString(row.quotaEmail)
            << L"\",\"quota5hRemainingPercent\":" << row.quota5hRemainingPercent
            << L",\"quota7dRemainingPercent\":" << row.quota7dRemainingPercent
            << L",\"quota5hResetAfterSeconds\":" << row.quota5hResetAfterSeconds
            << L",\"quota7dResetAfterSeconds\":" << row.quota7dResetAfterSeconds
            << L",\"quota5hResetAt\":" << row.quota5hResetAt
            << L",\"quota7dResetAt\":" << row.quota7dResetAt
            << L"}";
        if (i + 1 < data.accounts.size())
        {
            ss << L",";
        }
        ss << L"\n";
    }
    ss << L"  ]\n}\n";
    return WriteUtf8File(GetIndexPath(), ss.str());
}

std::wstring NowText()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tmLocal{};
    localtime_s(&tmLocal, &tt);
    wchar_t buf[64]{};
    wcsftime(buf, _countof(buf), L"%Y/%m/%d %H:%M", &tmLocal);
    return buf;
}

void EnsureIndexExists()
{
    IndexData idx;
    if (LoadIndex(idx))
    {
        return;
    }

    IndexData generated;
    const fs::path backupsDir = GetBackupsDir();
    const fs::path legacyBackupsDir = GetLegacyBackupsDir();
    std::error_code ec;
    fs::create_directories(backupsDir, ec);
    for (const std::wstring group : { L"personal", L"business", L"free", L"plus", L"team", L"pro" })
    {
        ec.clear();
        fs::create_directories(backupsDir / group, ec);
    }

    auto scanBackupsRoot = [&](const fs::path& root) {
        if (!fs::exists(root))
        {
            return;
        }

        for (const std::wstring group : { L"personal", L"business", L"free", L"plus", L"team", L"pro" })
        {
            const fs::path groupDir = root / group;
            if (!fs::exists(groupDir))
            {
                continue;
            }

            for (const auto& entry : fs::directory_iterator(groupDir, ec))
            {
                if (ec || !entry.is_directory())
                {
                    continue;
                }
                const fs::path auth = entry.path() / L"auth.json";
                if (!fs::exists(auth))
                {
                    continue;
                }

                const std::wstring name = entry.path().filename().wstring();
                const auto duplicated = std::find_if(generated.accounts.begin(), generated.accounts.end(), [&](const IndexEntry& row) {
                    return EqualsIgnoreCase(row.name, name) && NormalizeGroup(row.group) == group;
                });
                if (duplicated != generated.accounts.end())
                {
                    continue;
                }

                IndexEntry row;
                row.name = name;
                row.group = group;
                row.path = MakeRelativeAuthPath(group, row.name);
                row.updatedAt = FormatFileTime(auth);
                generated.accounts.push_back(row);
            }
        }

        // Legacy backups/<name>/auth.json
        for (const auto& entry : fs::directory_iterator(root, ec))
        {
            if (ec || !entry.is_directory())
            {
                continue;
            }

            const std::wstring folder = entry.path().filename().wstring();
            const std::wstring folderLower = ToLowerCopy(folder);
            if (folderLower == L"personal" || folderLower == L"business" ||
                folderLower == L"free" || folderLower == L"plus" ||
                folderLower == L"team" || folderLower == L"pro")
            {
                continue;
            }

            const fs::path auth = entry.path() / L"auth.json";
            if (!fs::exists(auth))
            {
                continue;
            }

            const auto duplicated = std::find_if(generated.accounts.begin(), generated.accounts.end(), [&](const IndexEntry& row) {
                return EqualsIgnoreCase(row.name, folder) && NormalizeGroup(row.group) == L"personal";
            });
            if (duplicated != generated.accounts.end())
            {
                continue;
            }

            IndexEntry row;
            row.name = folder;
            row.group = L"personal";
            row.path = MakeRelativeAuthPath(row.group, row.name);
            row.updatedAt = FormatFileTime(auth);
            generated.accounts.push_back(row);
        }
    };

    scanBackupsRoot(backupsDir);
    if (legacyBackupsDir != backupsDir)
    {
        scanBackupsRoot(legacyBackupsDir);
    }

    SaveIndex(generated);
}

bool BackupCurrentAccount(const std::wstring& name, std::wstring& status, std::wstring& code)
{
    EnsureIndexExists();

    const std::wstring safeName = SanitizeAccountName(name);
    if (safeName.empty())
    {
        status = L"保存失败：账号名无效";
        code = L"invalid_name";
        return false;
    }
    if (safeName.size() > 32)
    {
        status = L"保存失败：账号名最多 32 个字符";
        code = L"name_too_long";
        return false;
    }

    const std::wstring userAuth = GetUserAuthPath();
    if (userAuth.empty() || !fs::exists(userAuth))
    {
        status = L"保存失败：当前账号文件不存在";
        code = L"auth_missing";
        return false;
    }

    std::wstring detectedGroup = L"personal";
    UsageSnapshot currentUsage;
    if (QueryUsageFromAuthFile(userAuth, currentUsage) && !currentUsage.planType.empty())
    {
        detectedGroup = GroupFromPlanType(currentUsage.planType);
    }

    IndexData idx;
    LoadIndex(idx);
    const auto duplicated = std::find_if(idx.accounts.begin(), idx.accounts.end(), [&](const IndexEntry& row) {
        return EqualsIgnoreCase(row.name, safeName);
    });
    if (duplicated != idx.accounts.end())
    {
        status = L"名字重复，请修改后再保存";
        code = L"duplicate_name";
        return false;
    }

    const fs::path backupDir = GetGroupDir(detectedGroup) / safeName;
    std::error_code ec;
    fs::create_directories(backupDir, ec);
    if (ec)
    {
        status = L"保存失败：无法创建备份目录";
        code = L"create_dir_failed";
        return false;
    }

    fs::copy_file(userAuth, backupDir / L"auth.json", fs::copy_options::overwrite_existing, ec);
    if (ec)
    {
        status = L"保存失败：无法写入备份文件";
        code = L"write_failed";
        return false;
    }

    IndexEntry row;
    row.name = safeName;
    row.group = detectedGroup;
    row.path = MakeRelativeAuthPath(detectedGroup, safeName);
    row.updatedAt = NowText();
    if (currentUsage.ok)
    {
        row.quotaUsageOk = true;
        row.quotaPlanType = currentUsage.planType;
        row.quotaEmail = currentUsage.email;
        row.quota5hRemainingPercent = RemainingPercentFromUsed(currentUsage.primaryUsedPercent);
        row.quota7dRemainingPercent = RemainingPercentFromUsed(currentUsage.secondaryUsedPercent);
        row.quota5hResetAfterSeconds = currentUsage.primaryResetAfterSeconds;
        row.quota7dResetAfterSeconds = currentUsage.secondaryResetAfterSeconds;
        row.quota5hResetAt = currentUsage.primaryResetAt;
        row.quota7dResetAt = currentUsage.secondaryResetAt;
    }
    idx.accounts.push_back(row);
    idx.currentName = safeName;
    idx.currentGroup = detectedGroup;
    SaveIndex(idx);

    status = L"保存成功：[" + detectedGroup + L"] " + safeName;
    code = L"backup_saved";
    return true;
}

bool SwitchToAccount(const std::wstring& account, const std::wstring& group, std::wstring& status, std::wstring& code)
{
    EnsureIndexExists();
    const std::wstring safeGroup = NormalizeGroup(group);
    const std::wstring safeName = SanitizeAccountName(account);

    IndexData idx;
    LoadIndex(idx);
    auto it = std::find_if(idx.accounts.begin(), idx.accounts.end(), [&](const IndexEntry& row) {
        return NormalizeGroup(row.group) == safeGroup && EqualsIgnoreCase(row.name, safeName);
    });

    fs::path source;
    if (it != idx.accounts.end())
    {
        source = ResolveAuthPathFromIndex(*it);
    }
    else
    {
        source = GetBackupsDir() / safeName / L"auth.json";
    }
    if (!fs::exists(source))
    {
        status = L"切换失败：未找到备份账号";
        code = L"not_found";
        return false;
    }

    const std::wstring userAuth = GetUserAuthPath();
    if (userAuth.empty())
    {
        status = L"切换失败：找不到用户目录";
        code = L"userprofile_missing";
        return false;
    }

    std::error_code ec;
    fs::create_directories(fs::path(userAuth).parent_path(), ec);
    ec.clear();
    fs::copy_file(source, userAuth, fs::copy_options::overwrite_existing, ec);
    if (ec)
    {
        status = L"切换失败：无法写入当前账号文件";
        code = L"write_failed";
        return false;
    }

    idx.currentName = safeName;
    idx.currentGroup = safeGroup;
    if (it != idx.accounts.end())
    {
        it->updatedAt = NowText();
    }
    SaveIndex(idx);

    AppConfig cfg;
    LoadConfig(cfg);
    cfg.lastSwitchedAccount = safeName;
    cfg.lastSwitchedGroup = safeGroup;
    cfg.lastSwitchedAt = NowText();
    SaveConfig(cfg);

    std::wstring ideDisplay;
    if (!RestartConfiguredIde(ideDisplay))
    {
        status = L"切换成功，但重启 " + ideDisplay + L" 失败，请手动重启";
        code = L"restart_failed";
        return false;
    }

    status = L"切换成功，正在重启 " + ideDisplay;
    code = L"switch_success";
    return true;
}

bool PickOpenJsonPath(HWND hwnd, std::wstring& outPath)
{
    std::vector<wchar_t> fileName(32768, L'\0');
    static constexpr wchar_t kJsonFilter[] = L"JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0\0";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = IsWindow(hwnd) ? hwnd : nullptr;
    ofn.lpstrFile = fileName.data();
    ofn.nMaxFile = static_cast<DWORD>(fileName.size());
    ofn.lpstrFilter = kJsonFilter;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = L"json";

    if (!GetOpenFileNameW(&ofn))
    {
        const DWORD dlgErr = CommDlgExtendedError();
        if (dlgErr != 0)
        {
            OutputDebugStringW((L"[PickOpenJsonPath] CommDlgExtendedError=" + std::to_wstring(dlgErr) + L"\n").c_str());
        }
        return false;
    }

    outPath = fileName.data();
    return true;
}

bool JsonContainsField(const std::wstring& json, const std::wstring& key)
{
    if (key.empty())
    {
        return false;
    }
    return json.find(L"\"" + key + L"\"") != std::wstring::npos;
}

bool IsLikelyValidAuthJson(const std::wstring& json)
{
    if (!JsonContainsField(json, L"auth_mode") ||
        !JsonContainsField(json, L"OPENAI_API_KEY") ||
        !JsonContainsField(json, L"tokens") ||
        !JsonContainsField(json, L"id_token") ||
        !JsonContainsField(json, L"access_token") ||
        !JsonContainsField(json, L"refresh_token") ||
        !JsonContainsField(json, L"account_id") ||
        !JsonContainsField(json, L"last_refresh"))
    {
        return false;
    }

    const std::wstring authMode = ExtractJsonField(json, L"auth_mode");
    return authMode.empty() || _wcsicmp(authMode.c_str(), L"chatgpt") == 0;
}

bool DeleteAccountBackup(const std::wstring& account, const std::wstring& group, std::wstring& status, std::wstring& code)
{
    EnsureIndexExists();
    const std::wstring safeGroup = NormalizeGroup(group);
    const std::wstring safeName = SanitizeAccountName(account);

    IndexData idx;
    LoadIndex(idx);
    auto it = std::find_if(idx.accounts.begin(), idx.accounts.end(), [&](const IndexEntry& row) {
        return NormalizeGroup(row.group) == safeGroup && EqualsIgnoreCase(row.name, safeName);
    });

    fs::path dir = GetGroupDir(safeGroup) / safeName;
    if (it != idx.accounts.end())
    {
        const fs::path p = ResolveAuthPathFromIndex(*it).parent_path();
        if (!p.empty())
        {
            dir = p;
        }
    }

    std::error_code ec;
    auto count = fs::remove_all(dir, ec);
    if ((ec || count == 0) && fs::exists(GetBackupsDir() / safeName))
    {
        ec.clear();
        count = fs::remove_all(GetBackupsDir() / safeName, ec);
    }
    if (ec || count == 0)
    {
        status = L"删除失败：未找到账号备份";
        code = L"not_found";
        return false;
    }

    if (it != idx.accounts.end())
    {
        const bool wasCurrent = EqualsIgnoreCase(it->name, idx.currentName) && NormalizeGroup(it->group) == NormalizeGroup(idx.currentGroup);
        idx.accounts.erase(it);
        if (wasCurrent)
        {
            idx.currentName.clear();
            idx.currentGroup.clear();
        }
        SaveIndex(idx);
    }

    status = L"删除成功：[" + safeGroup + L"] " + safeName;
    code = L"delete_success";
    return true;
}

bool LoginNewAccount(std::wstring& status, std::wstring& code)
{
    const std::wstring authPath = GetUserAuthPath();
    if (authPath.empty())
    {
        status = L"登录新账号失败：找不到用户目录";
        code = L"userprofile_missing";
        return false;
    }

    std::error_code ec;
    fs::remove(authPath, ec);

    const fs::path autoPath = fs::path(authPath).parent_path() / L"auto.json";
    ec.clear();
    fs::remove(autoPath, ec);

    std::wstring ideDisplay;
    if (!RestartConfiguredIde(ideDisplay))
    {
        status = L"已清理登录文件，但重启 " + ideDisplay + L" 失败，请手动重启";
        code = L"restart_failed";
        return false;
    }

    status = L"已清理登录文件，正在重启 " + ideDisplay;
    code = L"login_new_success";
    return true;
}

bool ImportAuthJsonFile(const std::wstring& jsonPath, const std::wstring& preferredName, std::wstring& status, std::wstring& code)
{
    std::wstring json;
    if (!ReadUtf8File(jsonPath, json))
    {
        status = L"导入失败：无法读取 auth.json 文件";
        code = L"write_failed";
        return false;
    }
    if (!IsLikelyValidAuthJson(json))
    {
        status = L"该文件可能不是有效 auth.json，缺少必要字段";
        code = L"auth_json_invalid";
        return false;
    }

    EnsureIndexExists();
    IndexData idx;
    LoadIndex(idx);

    UsageSnapshot usage;
    QueryUsageFromAuthFile(jsonPath, usage);
    std::wstring detectedGroup = L"personal";
    if (usage.ok && !usage.planType.empty())
    {
        detectedGroup = GroupFromPlanType(usage.planType);
    }

    const std::wstring accountName = SanitizeAccountName(preferredName);
    if (accountName.empty())
    {
        status = L"导入失败：请先输入有效账号名";
        code = L"invalid_name";
        return false;
    }
    if (accountName.size() > 32)
    {
        status = L"导入失败：账号名最多 32 个字符";
        code = L"name_too_long";
        return false;
    }
    const auto duplicated = std::find_if(idx.accounts.begin(), idx.accounts.end(), [&](const IndexEntry& row) {
        return EqualsIgnoreCase(row.name, accountName);
    });
    if (duplicated != idx.accounts.end())
    {
        status = L"导入失败：名字重复，请修改后再导入";
        code = L"duplicate_name";
        return false;
    }

    const fs::path backupDir = GetGroupDir(detectedGroup) / accountName;
    std::error_code ec;
    fs::create_directories(backupDir, ec);
    if (ec)
    {
        status = L"导入失败：无法创建备份目录";
        code = L"create_dir_failed";
        return false;
    }

    fs::copy_file(fs::path(jsonPath), backupDir / L"auth.json", fs::copy_options::overwrite_existing, ec);
    if (ec)
    {
        status = L"导入失败：无法写入备份文件";
        code = L"write_failed";
        return false;
    }

    IndexEntry row;
    row.name = accountName;
    row.group = detectedGroup;
    row.path = MakeRelativeAuthPath(detectedGroup, accountName);
    row.updatedAt = NowText();
    if (usage.ok)
    {
        row.quotaUsageOk = true;
        row.quotaPlanType = usage.planType;
        row.quotaEmail = usage.email;
        row.quota5hRemainingPercent = RemainingPercentFromUsed(usage.primaryUsedPercent);
        row.quota7dRemainingPercent = RemainingPercentFromUsed(usage.secondaryUsedPercent);
        row.quota5hResetAfterSeconds = usage.primaryResetAfterSeconds;
        row.quota7dResetAfterSeconds = usage.secondaryResetAfterSeconds;
        row.quota5hResetAt = usage.primaryResetAt;
        row.quota7dResetAt = usage.secondaryResetAt;
    }
    idx.accounts.push_back(row);
    SaveIndex(idx);

    status = L"auth.json 导入成功：[" + detectedGroup + L"] " + accountName;
    code = L"auth_json_imported";
    return true;
}

bool ExportAccountsZip(HWND hwnd, std::wstring& status, std::wstring& code)
{
    std::wstring saveZipPath;
    if (!PickSaveZipPath(hwnd, saveZipPath))
    {
        status = L"导出已取消";
        code = L"export_cancelled";
        return false;
    }

    const fs::path backupsDir = GetBackupsDir();
    std::error_code ec;
    fs::create_directories(backupsDir, ec);

    const std::wstring sourcePattern = (backupsDir / L"*").wstring();
    const std::wstring command = L"Compress-Archive -Path " + QuotePowerShellLiteral(sourcePattern) +
        L" -DestinationPath " + QuotePowerShellLiteral(saveZipPath) + L" -Force";

    if (!RunPowerShellCommand(command))
    {
        status = L"导出失败：无法创建 ZIP 文件";
        code = L"write_failed";
        return false;
    }

    status = L"导出成功：" + saveZipPath;
    code = L"export_success";
    return true;
}

bool ImportAccountsZip(HWND hwnd, std::wstring& status, std::wstring& code)
{
    std::wstring zipPath;
    if (!PickOpenZipPath(hwnd, zipPath))
    {
        status = L"导入已取消";
        code = L"import_cancelled";
        return false;
    }

    const fs::path backupsDir = GetBackupsDir();
    std::error_code ec;
    fs::create_directories(backupsDir, ec);

    const std::wstring command = L"Expand-Archive -LiteralPath " + QuotePowerShellLiteral(zipPath) +
        L" -DestinationPath " + QuotePowerShellLiteral(backupsDir.wstring()) + L" -Force";

    if (!RunPowerShellCommand(command))
    {
        status = L"导入失败：无法解压 ZIP 文件";
        code = L"write_failed";
        return false;
    }

    status = L"导入成功：" + zipPath;
    code = L"import_success";
    return true;
}

std::wstring EscapeJsonString(const std::wstring& input)
{
    std::wstring out;
    out.reserve(input.size() + 8);
    for (const wchar_t ch : input)
    {
        switch (ch)
        {
        case L'\\': out += L"\\\\"; break;
        case L'"': out += L"\\\""; break;
        case L'\n': out += L"\\n"; break;
        case L'\r': out += L"\\r"; break;
        case L'\t': out += L"\\t"; break;
        default: out.push_back(ch); break;
        }
    }
    return out;
}

std::wstring UnescapeJsonString(const std::wstring& input)
{
    auto hexValue = [](const wchar_t ch) -> int
    {
        if (ch >= L'0' && ch <= L'9') return static_cast<int>(ch - L'0');
        if (ch >= L'a' && ch <= L'f') return 10 + static_cast<int>(ch - L'a');
        if (ch >= L'A' && ch <= L'F') return 10 + static_cast<int>(ch - L'A');
        return -1;
    };

    auto readHex4 = [&](size_t pos, unsigned int& outCode) -> bool
    {
        if (pos + 4 > input.size())
        {
            return false;
        }
        unsigned int value = 0;
        for (size_t j = 0; j < 4; ++j)
        {
            const int hv = hexValue(input[pos + j]);
            if (hv < 0)
            {
                return false;
            }
            value = (value << 4) | static_cast<unsigned int>(hv);
        }
        outCode = value;
        return true;
    };

    std::wstring out;
    out.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i)
    {
        const wchar_t ch = input[i];
        if (ch == L'\\' && i + 1 < input.size())
        {
            const wchar_t next = input[++i];
            switch (next)
            {
            case L'\\': out.push_back(L'\\'); break;
            case L'"': out.push_back(L'"'); break;
            case L'/': out.push_back(L'/'); break;
            case L'b': out.push_back(L'\b'); break;
            case L'f': out.push_back(L'\f'); break;
            case L'n': out.push_back(L'\n'); break;
            case L'r': out.push_back(L'\r'); break;
            case L't': out.push_back(L'\t'); break;
            case L'u':
            {
                unsigned int code = 0;
                if (!readHex4(i + 1, code))
                {
                    out.push_back(L'u');
                    break;
                }
                i += 4;

                // Decode surrogate pair if present.
                if (code >= 0xD800 && code <= 0xDBFF && (i + 6) < input.size() && input[i + 1] == L'\\' && input[i + 2] == L'u')
                {
                    unsigned int low = 0;
                    if (readHex4(i + 3, low) && low >= 0xDC00 && low <= 0xDFFF)
                    {
                        const unsigned int codePoint = 0x10000 + (((code - 0xD800) << 10) | (low - 0xDC00));
                        const wchar_t highSur = static_cast<wchar_t>(0xD800 + ((codePoint - 0x10000) >> 10));
                        const wchar_t lowSur = static_cast<wchar_t>(0xDC00 + ((codePoint - 0x10000) & 0x3FF));
                        out.push_back(highSur);
                        out.push_back(lowSur);
                        i += 6;
                        break;
                    }
                }

                out.push_back(static_cast<wchar_t>(code));
                break;
            }
            default: out.push_back(next); break;
            }
        }
        else
        {
            out.push_back(ch);
        }
    }

    return out;
}

std::wstring FormatFileTime(const fs::path& path)
{
    std::error_code ec;
    const auto writeTime = fs::last_write_time(path, ec);
    if (ec)
    {
        return L"-";
    }

    const auto systemNow = std::chrono::system_clock::now();
    const auto fileNow = fs::file_time_type::clock::now();
    const auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(writeTime - fileNow + systemNow);
    const std::time_t tt = std::chrono::system_clock::to_time_t(sctp);

    std::tm tmLocal{};
    localtime_s(&tmLocal, &tt);

    wchar_t buf[64]{};
    wcsftime(buf, _countof(buf), L"%Y/%m/%d %H:%M", &tmLocal);
    return buf;
}

bool FilesEqual(const fs::path& left, const fs::path& right)
{
    std::error_code ec;
    if (!fs::exists(left, ec) || !fs::exists(right, ec))
    {
        return false;
    }

    if (fs::file_size(left, ec) != fs::file_size(right, ec))
    {
        return false;
    }

    std::ifstream lhs(left, std::ios::binary);
    std::ifstream rhs(right, std::ios::binary);
    if (!lhs || !rhs)
    {
        return false;
    }

    constexpr size_t kBufferSize = 4096;
    char lb[kBufferSize]{};
    char rb[kBufferSize]{};

    while (lhs && rhs)
    {
        lhs.read(lb, kBufferSize);
        rhs.read(rb, kBufferSize);
        const std::streamsize ln = lhs.gcount();
        const std::streamsize rn = rhs.gcount();
        if (ln != rn)
        {
            return false;
        }
        if (memcmp(lb, rb, static_cast<size_t>(ln)) != 0)
        {
            return false;
        }
    }

    return true;
}

struct AccountEntry
{
    std::wstring name;
    std::wstring group;
    std::wstring updatedAt;
    bool isCurrent = false;
    bool usageOk = false;
    std::wstring usageError;
    std::wstring planType;
    std::wstring email;
    int quota5hRemainingPercent = -1;
    int quota7dRemainingPercent = -1;
    long long quota5hResetAfterSeconds = -1;
    long long quota7dResetAfterSeconds = -1;
    long long quota5hResetAt = -1;
    long long quota7dResetAt = -1;
};

std::vector<AccountEntry> CollectAccounts(const bool refreshUsage, const std::wstring& targetName, const std::wstring& targetGroup)
{
    EnsureIndexExists();

    std::vector<AccountEntry> result;
    IndexData idx;
    LoadIndex(idx);
    bool indexChanged = false;
    const std::wstring safeTargetName = SanitizeAccountName(targetName);
    const std::wstring safeTargetGroup = targetGroup.empty() ? L"" : NormalizeGroup(targetGroup);
    const bool hasTarget = !safeTargetName.empty();

    for (auto& row : idx.accounts)
    {
        const fs::path backupAuth = ResolveAuthPathFromIndex(row);
        if (!fs::exists(backupAuth))
        {
            continue;
        }

        AccountEntry item;
        item.name = row.name;
        item.group = NormalizeGroup(row.group);
        item.updatedAt = row.updatedAt.empty() ? FormatFileTime(backupAuth) : row.updatedAt;
        item.isCurrent = EqualsIgnoreCase(idx.currentName, row.name) &&
            NormalizeGroup(idx.currentGroup) == NormalizeGroup(row.group);
        item.usageOk = row.quotaUsageOk;
        item.planType = row.quotaPlanType;
        item.email = row.quotaEmail;
        item.quota5hRemainingPercent = row.quota5hRemainingPercent;
        item.quota7dRemainingPercent = row.quota7dRemainingPercent;
        item.quota5hResetAfterSeconds = row.quota5hResetAfterSeconds;
        item.quota7dResetAfterSeconds = row.quota7dResetAfterSeconds;
        item.quota5hResetAt = row.quota5hResetAt;
        item.quota7dResetAt = row.quota7dResetAt;

        const bool shouldRefresh = refreshUsage &&
            (!hasTarget || (EqualsIgnoreCase(row.name, safeTargetName) &&
                (safeTargetGroup.empty() || NormalizeGroup(row.group) == safeTargetGroup)));
        if (shouldRefresh)
        {
            UsageSnapshot usage;
            if (QueryUsageFromAuthFile(backupAuth, usage))
            {
                item.usageOk = true;
                item.planType = usage.planType;
                item.email = usage.email;
                item.quota5hRemainingPercent = RemainingPercentFromUsed(usage.primaryUsedPercent);
                item.quota7dRemainingPercent = RemainingPercentFromUsed(usage.secondaryUsedPercent);
                item.quota5hResetAfterSeconds = usage.primaryResetAfterSeconds;
                item.quota7dResetAfterSeconds = usage.secondaryResetAfterSeconds;
                item.quota5hResetAt = usage.primaryResetAt;
                item.quota7dResetAt = usage.secondaryResetAt;
                row.quotaUsageOk = true;
                row.quotaPlanType = item.planType;
                row.quotaEmail = item.email;
                row.quota5hRemainingPercent = item.quota5hRemainingPercent;
                row.quota7dRemainingPercent = item.quota7dRemainingPercent;
                row.quota5hResetAfterSeconds = item.quota5hResetAfterSeconds;
                row.quota7dResetAfterSeconds = item.quota7dResetAfterSeconds;
                row.quota5hResetAt = item.quota5hResetAt;
                row.quota7dResetAt = item.quota7dResetAt;
                indexChanged = true;

                const std::wstring refreshedGroup = GroupFromPlanType(usage.planType);
                const std::wstring expectedPath = MakeRelativeAuthPath(refreshedGroup, SanitizeAccountName(row.name));
                const bool needGroupSync =
                    IsPlanGroup(refreshedGroup) &&
                    (NormalizeGroup(row.group) != refreshedGroup ||
                        ToLowerCopy(row.path) != ToLowerCopy(expectedPath));
                if (needGroupSync)
                {
                    if (SyncIndexEntryToGroup(row, refreshedGroup))
                    {
                        indexChanged = true;
                    }
                }
                if (item.isCurrent && IsPlanGroup(row.group) && NormalizeGroup(idx.currentGroup) != NormalizeGroup(row.group))
                {
                    idx.currentGroup = NormalizeGroup(row.group);
                    indexChanged = true;
                }
            }
            else
            {
                item.usageError = usage.error;
            }
        }

        item.group = NormalizeGroup(row.group);
        result.push_back(item);
    }

    if (indexChanged)
    {
        SaveIndex(idx);
    }

    return result;
}

const AccountEntry* FindCurrentAccountEntry(const std::vector<AccountEntry>& accounts)
{
    for (const auto& item : accounts)
    {
        if (item.isCurrent)
        {
            return &item;
        }
    }
    return nullptr;
}

const AccountEntry* FindBestSwitchCandidate(const std::vector<AccountEntry>& accounts, const AccountEntry* current)
{
    const AccountEntry* best = nullptr;
    for (const auto& item : accounts)
    {
        if (!item.usageOk || item.quota5hRemainingPercent < 0)
        {
            continue;
        }
        if (current != nullptr &&
            EqualsIgnoreCase(item.name, current->name) &&
            NormalizeGroup(item.group) == NormalizeGroup(current->group))
        {
            continue;
        }
        if (best == nullptr || item.quota5hRemainingPercent > best->quota5hRemainingPercent)
        {
            best = &item;
        }
    }
    return best;
}

std::wstring BuildAccountsListJson(const bool refreshUsage, const std::wstring& targetName, const std::wstring& targetGroup)
{
    const std::vector<AccountEntry> accounts = CollectAccounts(refreshUsage, targetName, targetGroup);
    std::wstringstream ss;
    ss << L"{\"type\":\"accounts_list\",\"accounts\":[";
    for (size_t i = 0; i < accounts.size(); ++i)
    {
        const auto& item = accounts[i];
        if (i != 0)
        {
            ss << L",";
        }
        ss << L"{\"name\":\"" << EscapeJsonString(item.name)
            << L"\",\"group\":\"" << EscapeJsonString(item.group)
            << L"\",\"updatedAt\":\"" << EscapeJsonString(item.updatedAt)
            << L"\",\"isCurrent\":" << (item.isCurrent ? L"true" : L"false")
            << L",\"usageOk\":" << (item.usageOk ? L"true" : L"false")
            << L",\"usageError\":\"" << EscapeJsonString(item.usageError)
            << L"\",\"planType\":\"" << EscapeJsonString(item.planType)
            << L"\",\"email\":\"" << EscapeJsonString(item.email)
            << L"\",\"quota5hRemainingPercent\":" << item.quota5hRemainingPercent
            << L",\"quota7dRemainingPercent\":" << item.quota7dRemainingPercent
            << L",\"quota5hResetAfterSeconds\":" << item.quota5hResetAfterSeconds
            << L",\"quota7dResetAfterSeconds\":" << item.quota7dResetAfterSeconds
            << L",\"quota5hResetAt\":" << item.quota5hResetAt
            << L",\"quota7dResetAt\":" << item.quota7dResetAt
            << L"}";
    }
    ss << L"]}";
    return ss.str();
}

void PostAsyncWebJson(const HWND hwnd, const std::wstring& json)
{
    std::wstring* heapJson = new std::wstring(json);
    if (!PostMessageW(hwnd, WebViewHost::kMsgAsyncWebJson, 0, reinterpret_cast<LPARAM>(heapJson)))
    {
        delete heapJson;
    }
}

struct LowQuotaCandidatePayload
{
    std::wstring currentName;
    int currentQuota = -1;
    std::wstring bestName;
    std::wstring bestGroup;
    int bestQuota = -1;
    std::wstring accountKey;
};

void PostLowQuotaCandidate(const HWND hwnd, LowQuotaCandidatePayload&& payload)
{
    auto* heapPayload = new LowQuotaCandidatePayload(std::move(payload));
    if (!PostMessageW(hwnd, WebViewHost::kMsgLowQuotaCandidate, 0, reinterpret_cast<LPARAM>(heapPayload)))
    {
        delete heapPayload;
    }
}
}

void WebViewHost::ShowHr(HWND hwnd, const wchar_t* where, const HRESULT hr)
{
    wchar_t buf[256]{};
    swprintf_s(buf, L"%s failed. HRESULT=0x%08X", where, static_cast<unsigned>(hr));
    MessageBoxW(hwnd, buf, L"WebView2 Error", MB_ICONERROR);
}

void WebViewHost::RegisterWebMessageHandler(HWND hwnd)
{
    webview_->add_WebMessageReceived(
        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [this, hwnd](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT
            {
                std::wstring message;

                LPWSTR jsonMessage = nullptr;
                const HRESULT jsonHr = args->get_WebMessageAsJson(&jsonMessage);
                if (SUCCEEDED(jsonHr) && jsonMessage != nullptr)
                {
                    message.assign(jsonMessage);
                    CoTaskMemFree(jsonMessage);
                }
                else
                {
                    LPWSTR rawMessage = nullptr;
                    const HRESULT stringHr = args->TryGetWebMessageAsString(&rawMessage);
                    if (FAILED(stringHr) || rawMessage == nullptr)
                    {
                        return S_OK;
                    }

                    message.assign(rawMessage);
                    CoTaskMemFree(rawMessage);
                }

                const std::wstring action = ExtractJsonField(message, L"action");
                if (!action.empty())
                {
                    HandleWebAction(hwnd, action, message);
                }
                return S_OK;
            })
            .Get(),
        &webMessageToken_);
}

void WebViewHost::SendWebStatus(const std::wstring& text, const std::wstring& level, const std::wstring& code) const
{
    SendWebJson(
        L"{\"type\":\"status\",\"level\":\"" + EscapeJsonString(level) +
        L"\",\"code\":\"" + EscapeJsonString(code) +
        L"\",\"message\":\"" + EscapeJsonString(text) + L"\"}");
}

void WebViewHost::SendAccountsList(const bool refreshUsage, const std::wstring& targetName, const std::wstring& targetGroup) const
{
    SendWebJson(BuildAccountsListJson(refreshUsage, targetName, targetGroup));
}

void WebViewHost::SendAppInfo() const
{
    const std::wstring debugValue =
#ifdef _DEBUG
        L"true";
#else
        L"false";
#endif
    SendWebJson(
        L"{\"type\":\"app_info\",\"version\":\"" + EscapeJsonString(kAppVersion) +
        L"\",\"debug\":" + debugValue +
        L",\"repo\":\"https://github.com/isxlan0/Codex_AccountSwitch\"}");
}

void WebViewHost::SendUpdateInfo() const
{
    const HWND targetHwnd = hwnd_;
    std::thread([targetHwnd]() {
        const UpdateCheckResult check = CheckGitHubUpdate(kAppVersion);
        std::wstring json;
        if (!check.ok)
        {
            json =
                L"{\"type\":\"update_info\",\"ok\":false,\"current\":\"" + EscapeJsonString(kAppVersion) +
                L"\",\"latest\":\"\",\"hasUpdate\":false,\"url\":\"https://github.com/isxlan0/Codex_AccountSwitch/releases/latest\",\"downloadUrl\":\"https://github.com/isxlan0/Codex_AccountSwitch/releases/latest\",\"notes\":\"\",\"error\":\"" +
                EscapeJsonString(check.errorMessage) + L"\"}";
        }
        else
        {
            json =
                L"{\"type\":\"update_info\",\"ok\":true,\"current\":\"" + EscapeJsonString(check.currentVersion) +
                L"\",\"latest\":\"" + EscapeJsonString(check.latestVersion) +
                L"\",\"hasUpdate\":" + std::wstring(check.hasUpdate ? L"true" : L"false") +
                L",\"url\":\"" + EscapeJsonString(check.releaseUrl) +
                L"\",\"downloadUrl\":\"" + EscapeJsonString(check.downloadUrl) +
                L"\",\"notes\":\"" + EscapeJsonString(check.releaseNotes) + L"\"}";
        }

        std::wstring* heapJson = new std::wstring(std::move(json));
        if (!PostMessageW(targetHwnd, WebViewHost::kMsgAsyncWebJson, 0, reinterpret_cast<LPARAM>(heapJson)))
        {
            delete heapJson;
        }
    }).detach();
}

void WebViewHost::SendLanguageIndex() const
{
    const auto langs = LoadLanguageIndexList();
    std::wstringstream ss;
    ss << L"{\"type\":\"language_index\",\"languages\":[";
    for (size_t i = 0; i < langs.size(); ++i)
    {
        if (i != 0) ss << L",";
        ss << L"{\"code\":\"" << EscapeJsonString(langs[i].code)
           << L"\",\"name\":\"" << EscapeJsonString(langs[i].name)
           << L"\",\"file\":\"" << EscapeJsonString(langs[i].file) << L"\"}";
    }
    ss << L"]}";
    SendWebJson(ss.str());
}

void WebViewHost::SendLanguagePack(const std::wstring& languageCode) const
{
    std::vector<std::pair<std::wstring, std::wstring>> pairs;
    std::wstring resolvedCode;
    if (!LoadLanguageStrings(languageCode, pairs, resolvedCode))
    {
        SendWebJson(L"{\"type\":\"language_pack\",\"ok\":false}");
        return;
    }

    std::wstringstream ss;
    ss << L"{\"type\":\"language_pack\",\"ok\":true,\"code\":\"" << EscapeJsonString(resolvedCode) << L"\",\"strings\":{";
    for (size_t i = 0; i < pairs.size(); ++i)
    {
        if (i != 0) ss << L",";
        ss << L"\"" << EscapeJsonString(pairs[i].first) << L"\":\"" << EscapeJsonString(pairs[i].second) << L"\"";
    }
    ss << L"}}";
    SendWebJson(ss.str());
}

void WebViewHost::SendConfig(bool firstRun) const
{
    AppConfig cfg;
    LoadConfig(cfg);
    SendWebJson(
        L"{\"type\":\"config\",\"firstRun\":" + std::wstring(firstRun ? L"true" : L"false") +
        L",\"language\":\"" + EscapeJsonString(cfg.language) +
        L"\",\"languageIndex\":" + std::to_wstring(cfg.languageIndex) +
        L",\"ideExe\":\"" + EscapeJsonString(cfg.ideExe) +
        L"\",\"theme\":\"" + EscapeJsonString(cfg.theme) +
        L"\",\"autoUpdate\":" + std::wstring(cfg.autoUpdate ? L"true" : L"false") +
        L",\"autoRefreshAll\":true" +
        L",\"autoRefreshCurrent\":" + std::wstring(cfg.autoRefreshCurrent ? L"true" : L"false") +
        L",\"lowQuotaAutoPrompt\":" + std::wstring(cfg.lowQuotaAutoPrompt ? L"true" : L"false") +
        L",\"autoRefreshAllMinutes\":" + std::to_wstring(cfg.autoRefreshAllMinutes) +
        L",\"autoRefreshCurrentMinutes\":" + std::to_wstring(cfg.autoRefreshCurrentMinutes) +
        L",\"lastSwitchedAccount\":\"" + EscapeJsonString(cfg.lastSwitchedAccount) +
        L"\",\"lastSwitchedGroup\":\"" + EscapeJsonString(cfg.lastSwitchedGroup) +
        L"\",\"lastSwitchedAt\":\"" + EscapeJsonString(cfg.lastSwitchedAt) + L"\"}");
}

void WebViewHost::SendRefreshTimerState() const
{
    const int allRemain = allRefreshRemainingSec_ < 0 ? 0 : allRefreshRemainingSec_;
    const int currentRemain = currentRefreshRemainingSec_ < 0 ? 0 : currentRefreshRemainingSec_;
    SendWebJson(
        L"{\"type\":\"refresh_timers\",\"allIntervalSec\":" + std::to_wstring(allRefreshIntervalSec_) +
        L",\"currentIntervalSec\":" + std::to_wstring(currentRefreshIntervalSec_) +
        L",\"allEnabled\":true" +
        L",\"currentEnabled\":" + std::wstring(currentAutoRefreshEnabled_ ? L"true" : L"false") +
        L",\"allRemainingSec\":" + std::to_wstring(allRemain) +
        L",\"currentRemainingSec\":" + std::to_wstring(currentRemain) +
        L"}");
}

void WebViewHost::EnsureTrayIcon()
{
    if (hwnd_ == nullptr || trayIconAdded_)
    {
        return;
    }

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd_;
    nid.uID = 1;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = kMsgTrayNotify;
    nid.hIcon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR | LR_SHARED));
    if (nid.hIcon == nullptr)
    {
        nid.hIcon = LoadIconW(nullptr, IDI_INFORMATION);
    }
    wcscpy_s(nid.szTip, L"Codex Account Switch");
    if (Shell_NotifyIconW(NIM_ADD, &nid))
    {
        nid.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &nid);
        trayIconAdded_ = true;
    }
}

void WebViewHost::RemoveTrayIcon()
{
    if (hwnd_ == nullptr || !trayIconAdded_)
    {
        return;
    }
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd_;
    nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    trayIconAdded_ = false;
}

void WebViewHost::ShowLowQuotaBalloon(const std::wstring& currentName, const int currentQuota, const std::wstring& bestName, const std::wstring& bestGroup, const int bestQuota)
{
    EnsureTrayIcon();
    if (!trayIconAdded_ || hwnd_ == nullptr)
    {
        return;
    }

    pendingLowQuotaPrompt_ = true;
    pendingBestAccountName_ = bestName;
    pendingBestAccountGroup_ = NormalizeGroup(bestGroup);
    pendingCurrentAccountName_ = currentName;
    pendingCurrentQuota_ = currentQuota;
    pendingBestQuota_ = bestQuota;

    std::wstring title = L"Codex额度提醒";
    std::wstring message = L"当前账号 " + currentName + L" 的5小时额度已降至 " + std::to_wstring(currentQuota) + L"%";
    if (!bestName.empty() && bestQuota >= 0)
    {
        message += L"，可切换到 " + bestName + L"（" + std::to_wstring(bestQuota) + L"%）";
    }
    message += L"。点击此通知可选择是否切换并重启IDE。";

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd_;
    nid.uID = 1;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = NIIF_WARNING;
    wcsncpy_s(nid.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(nid.szInfo, message.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
    SendWebStatus(message, L"warning", L"low_quota_notification");
    // Always surface the in-app confirm prompt, tray click becomes optional.
    HandleLowQuotaPromptClick();
}

void WebViewHost::HandleLowQuotaPromptClick()
{
    if (!pendingLowQuotaPrompt_ || pendingBestAccountName_.empty())
    {
        pendingLowQuotaPrompt_ = false;
        return;
    }
    std::wstringstream ss;
    ss << L"{\"type\":\"low_quota_prompt\""
       << L",\"currentName\":\"" << EscapeJsonString(pendingCurrentAccountName_) << L"\""
       << L",\"currentQuota\":" << pendingCurrentQuota_
       << L",\"bestName\":\"" << EscapeJsonString(pendingBestAccountName_) << L"\""
       << L",\"bestGroup\":\"" << EscapeJsonString(pendingBestAccountGroup_) << L"\""
       << L",\"bestQuota\":" << pendingBestQuota_
       << L"}";
    SendWebJson(ss.str());
}

void WebViewHost::TriggerRefreshAll(const bool notifyStatus)
{
    if (allRefreshRunning_.exchange(true))
    {
        return;
    }
    allRefreshRemainingSec_ = allRefreshIntervalSec_;
    SendRefreshTimerState();

    const HWND targetHwnd = hwnd_;
    const bool checkLowQuota = lowQuotaPromptEnabled_;
    auto* runningFlag = &allRefreshRunning_;
    std::thread([targetHwnd, notifyStatus, checkLowQuota, runningFlag]() {
        const std::vector<AccountEntry> accounts = CollectAccounts(true, L"", L"");
        PostAsyncWebJson(targetHwnd, BuildAccountsListJson(false, L"", L""));
        if (notifyStatus)
        {
            PostAsyncWebJson(targetHwnd, L"{\"type\":\"status\",\"level\":\"success\",\"code\":\"quota_refreshed\",\"message\":\"额度已刷新\"}");
        }

        const AccountEntry* current = FindCurrentAccountEntry(accounts);
        if (checkLowQuota &&
            current != nullptr && current->usageOk && current->quota5hRemainingPercent >= 0 &&
            current->quota5hRemainingPercent <= kLowQuotaThresholdPercent)
        {
            const AccountEntry* best = FindBestSwitchCandidate(accounts, current);
            const std::wstring currentKey = NormalizeGroup(current->group) + L"::" + current->name;
            if (best != nullptr)
            {
                LowQuotaCandidatePayload payload;
                payload.currentName = current->name;
                payload.currentQuota = current->quota5hRemainingPercent;
                payload.bestName = best->name;
                payload.bestGroup = best->group;
                payload.bestQuota = best->quota5hRemainingPercent;
                payload.accountKey = currentKey;
                PostLowQuotaCandidate(targetHwnd, std::move(payload));
            }
        }

        runningFlag->store(false);
    }).detach();
}

void WebViewHost::TriggerRefreshCurrent()
{
    if (!currentAutoRefreshEnabled_)
    {
        currentRefreshRemainingSec_ = currentRefreshIntervalSec_;
        SendRefreshTimerState();
        return;
    }
    if (currentRefreshRunning_.exchange(true))
    {
        return;
    }
    currentRefreshRemainingSec_ = currentRefreshIntervalSec_;
    SendRefreshTimerState();

    IndexData idx;
    LoadIndex(idx);
    const std::wstring account = idx.currentName;
    const std::wstring group = idx.currentGroup;

    const HWND targetHwnd = hwnd_;
    const bool checkLowQuota = lowQuotaPromptEnabled_;
    auto* runningFlag = &currentRefreshRunning_;

    if (account.empty())
    {
        runningFlag->store(false);
        SendRefreshTimerState();
        return;
    }
    std::thread([targetHwnd, account, group, checkLowQuota, runningFlag]() {
        PostAsyncWebJson(targetHwnd, BuildAccountsListJson(true, account, group));
        const std::vector<AccountEntry> accounts = CollectAccounts(false, L"", L"");
        const AccountEntry* current = FindCurrentAccountEntry(accounts);
        if (checkLowQuota &&
            current != nullptr && current->usageOk && current->quota5hRemainingPercent >= 0 &&
            current->quota5hRemainingPercent <= kLowQuotaThresholdPercent)
        {
            const AccountEntry* best = FindBestSwitchCandidate(accounts, current);
            const std::wstring currentKey = NormalizeGroup(current->group) + L"::" + current->name;
            if (best != nullptr)
            {
                LowQuotaCandidatePayload payload;
                payload.currentName = current->name;
                payload.currentQuota = current->quota5hRemainingPercent;
                payload.bestName = best->name;
                payload.bestGroup = best->group;
                payload.bestQuota = best->quota5hRemainingPercent;
                payload.accountKey = currentKey;
                PostLowQuotaCandidate(targetHwnd, std::move(payload));
            }
        }
        runningFlag->store(false);
    }).detach();
}

void WebViewHost::HandleAutoRefreshTick()
{
    if (allRefreshRemainingSec_ > 0)
    {
        --allRefreshRemainingSec_;
    }
    if (currentAutoRefreshEnabled_ && currentRefreshRemainingSec_ > 0)
    {
        --currentRefreshRemainingSec_;
    }

    if (allRefreshRemainingSec_ <= 0)
    {
        TriggerRefreshAll();
    }
    if (currentAutoRefreshEnabled_ && currentRefreshRemainingSec_ <= 0)
    {
        TriggerRefreshCurrent();
    }
    SendRefreshTimerState();
}

void WebViewHost::SendWebJson(const std::wstring& json) const
{
    if (webview_ != nullptr)
    {
        webview_->PostWebMessageAsJson(json.c_str());
    }
}

void WebViewHost::HandleWebAction(HWND hwnd, const std::wstring& action, const std::wstring& rawMessage)
{
    if (action == L"window_minimize")
    {
        ShowWindow(hwnd, SW_MINIMIZE);
        return;
    }

    if (action == L"window_toggle_maximize")
    {
        ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
        return;
    }

    if (action == L"window_close")
    {
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
        return;
    }

    if (action == L"window_drag")
    {
        ReleaseCapture();
        SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return;
    }

    if (action == L"switch_account")
    {
        const std::wstring account = ExtractJsonField(rawMessage, L"account");
        const std::wstring group = ExtractJsonField(rawMessage, L"group");
        const std::wstring language = ExtractJsonField(rawMessage, L"language");
        const std::wstring ideExe = ExtractJsonField(rawMessage, L"ideExe");
        if (!language.empty() || !ideExe.empty())
        {
            AppConfig cfg;
            LoadConfig(cfg);
            if (!language.empty())
            {
                cfg.language = language;
            }
            if (!ideExe.empty())
            {
                cfg.ideExe = NormalizeIdeExe(ideExe);
            }
            SaveConfig(cfg);
        }
        std::wstring status;
        std::wstring code;
        const bool ok = SwitchToAccount(account, group, status, code);
        SendWebStatus(status, ok ? L"success" : L"error", code);
        if (ok)
        {
            SendAccountsList(false, L"", L"");
        }
        return;
    }

    if (action == L"backup_current")
    {
        const std::wstring name = ExtractJsonField(rawMessage, L"name");
        std::wstring status;
        std::wstring code;
        const bool ok = BackupCurrentAccount(name, status, code);
        const std::wstring level = (code == L"duplicate_name") ? L"warning" : (ok ? L"success" : L"error");
        SendWebStatus(status, level, code);
        if (ok)
        {
            const HWND targetHwnd = hwnd_;
            std::thread([targetHwnd, name]() {
                PostAsyncWebJson(targetHwnd, BuildAccountsListJson(true, name, L""));
                PostAsyncWebJson(targetHwnd, L"{\"type\":\"status\",\"level\":\"success\",\"code\":\"account_quota_refreshed\",\"message\":\"账号额度已刷新\"}");
            }).detach();
        }
        return;
    }

    if (action == L"delete_account")
    {
        const std::wstring account = ExtractJsonField(rawMessage, L"account");
        const std::wstring group = ExtractJsonField(rawMessage, L"group");
        std::wstring status;
        std::wstring code;
        const bool ok = DeleteAccountBackup(account, group, status, code);
        SendWebStatus(status, ok ? L"success" : L"error", code);
        if (ok)
        {
            SendAccountsList(false, L"", L"");
        }
        return;
    }

    if (action == L"list_accounts")
    {
        SendAccountsList(false, L"", L"");
        SendRefreshTimerState();
        return;
    }

    if (action == L"refresh_accounts")
    {
        TriggerRefreshAll(true);
        return;
    }

    if (action == L"refresh_account")
    {
        const std::wstring account = ExtractJsonField(rawMessage, L"account");
        const std::wstring group = ExtractJsonField(rawMessage, L"group");
        if (!account.empty())
        {
            const HWND targetHwnd = hwnd_;
            std::thread([targetHwnd, account, group]() {
                PostAsyncWebJson(targetHwnd, BuildAccountsListJson(true, account, group));
                PostAsyncWebJson(targetHwnd, L"{\"type\":\"status\",\"level\":\"success\",\"code\":\"account_quota_refreshed\",\"message\":\"账号额度已刷新\"}");
            }).detach();
        }
        return;
    }

    if (action == L"get_app_info")
    {
        SendAppInfo();
        return;
    }

    if (action == L"get_config")
    {
        bool created = false;
        EnsureConfigExists(created);
        AppConfig cfg;
        if (LoadConfig(cfg))
        {
            currentAutoRefreshEnabled_ = cfg.autoRefreshCurrent;
            lowQuotaPromptEnabled_ = cfg.lowQuotaAutoPrompt;
            allRefreshIntervalSec_ = ClampRefreshMinutes(cfg.autoRefreshAllMinutes, kDefaultAllRefreshMinutes) * 60;
            currentRefreshIntervalSec_ = ClampRefreshMinutes(cfg.autoRefreshCurrentMinutes, kDefaultCurrentRefreshMinutes) * 60;
            if (allRefreshRemainingSec_ <= 0 || allRefreshRemainingSec_ > allRefreshIntervalSec_)
            {
                allRefreshRemainingSec_ = allRefreshIntervalSec_;
            }
            if (currentRefreshRemainingSec_ <= 0 || currentRefreshRemainingSec_ > currentRefreshIntervalSec_)
            {
                currentRefreshRemainingSec_ = currentRefreshIntervalSec_;
            }
        }
        SendConfig(created);
        SendRefreshTimerState();
        return;
    }

    if (action == L"get_languages")
    {
        SendLanguageIndex();
        const std::wstring code = ExtractJsonField(rawMessage, L"code");
        AppConfig cfg;
        LoadConfig(cfg);
        SendLanguagePack(code.empty() ? cfg.language : code);
        return;
    }

    if (action == L"get_language_pack")
    {
        const std::wstring code = ExtractJsonField(rawMessage, L"code");
        SendLanguagePack(code.empty() ? L"zh-CN" : code);
        return;
    }

    if (action == L"set_config")
    {
        const std::wstring language = ExtractJsonField(rawMessage, L"language");
        const std::wstring ideExe = ExtractJsonField(rawMessage, L"ideExe");
        const std::wstring theme = ExtractJsonField(rawMessage, L"theme");
        const bool autoUpdate = ExtractJsonBoolField(rawMessage, L"autoUpdate", true);
        const bool autoRefreshCurrent = ExtractJsonBoolField(rawMessage, L"autoRefreshCurrent", true);
        const bool lowQuotaAutoPrompt = ExtractJsonBoolField(rawMessage, L"lowQuotaAutoPrompt", true);
        const int autoRefreshAllMinutes = ExtractJsonIntField(rawMessage, L"autoRefreshAllMinutes", kDefaultAllRefreshMinutes);
        const int autoRefreshCurrentMinutes = ExtractJsonIntField(rawMessage, L"autoRefreshCurrentMinutes", kDefaultCurrentRefreshMinutes);
        AppConfig cfg;
        LoadConfig(cfg);
        if (!language.empty())
        {
            cfg.language = language;
        }
        if (!ideExe.empty())
        {
            cfg.ideExe = NormalizeIdeExe(ideExe);
        }
        if (!theme.empty())
        {
            cfg.theme = NormalizeTheme(theme);
        }
        cfg.autoUpdate = autoUpdate;
        cfg.autoRefreshCurrent = autoRefreshCurrent;
        cfg.lowQuotaAutoPrompt = lowQuotaAutoPrompt;
        cfg.autoRefreshAllMinutes = ClampRefreshMinutes(autoRefreshAllMinutes, kDefaultAllRefreshMinutes);
        cfg.autoRefreshCurrentMinutes = ClampRefreshMinutes(autoRefreshCurrentMinutes, kDefaultCurrentRefreshMinutes);
        const bool saved = SaveConfig(cfg);
        if (saved)
        {
            ApplyWindowTitleTheme(hwnd_, cfg.theme);
            currentAutoRefreshEnabled_ = cfg.autoRefreshCurrent;
            lowQuotaPromptEnabled_ = cfg.lowQuotaAutoPrompt;
            if (!lowQuotaPromptEnabled_)
            {
                pendingLowQuotaPrompt_ = false;
            }
            allRefreshIntervalSec_ = cfg.autoRefreshAllMinutes * 60;
            currentRefreshIntervalSec_ = cfg.autoRefreshCurrentMinutes * 60;
            allRefreshRemainingSec_ = allRefreshIntervalSec_;
            currentRefreshRemainingSec_ = currentRefreshIntervalSec_;
        }
        SendWebStatus(saved ? L"设置已保存" : L"设置保存失败", saved ? L"success" : L"error", saved ? L"config_saved" : L"save_config_failed");
        SendConfig(false);
        SendRefreshTimerState();
        return;
    }

    if (action == L"check_update")
    {
        SendUpdateInfo();
        return;
    }

    if (action == L"open_external_url")
    {
        const std::wstring url = ExtractJsonField(rawMessage, L"url");
        if (!OpenExternalUrlByExplorer(url))
        {
            SendWebStatus(L"打开链接失败", L"error", L"open_url_failed");
            return;
        }
        SendWebStatus(L"已打开链接", L"success", L"open_url_success");
        return;
    }

    if (action == L"export_accounts")
    {
        std::wstring status;
        std::wstring code;
        const bool ok = ExportAccountsZip(hwnd, status, code);
        const std::wstring level = (code == L"export_cancelled") ? L"warning" : (ok ? L"success" : L"error");
        SendWebStatus(status, level, code);
        if (ok)
        {
            SendAccountsList(false, L"", L"");
        }
        return;
    }

    if (action == L"import_accounts")
    {
        std::wstring status;
        std::wstring code;
        const bool ok = ImportAccountsZip(hwnd, status, code);
        const std::wstring level = (code == L"import_cancelled") ? L"warning" : (ok ? L"success" : L"error");
        SendWebStatus(status, level, code);
        if (ok)
        {
            SendAccountsList(false, L"", L"");
        }
        return;
    }

    if (action == L"import_auth_json")
    {
        const std::wstring preferredName = ExtractJsonField(rawMessage, L"name");
        if (preferredName.empty())
        {
            std::wstring jsonPath;
            if (!PickOpenJsonPath(hwnd, jsonPath))
            {
                SendWebStatus(L"导入已取消", L"warning", L"import_cancelled");
                return;
            }

            std::wstring json;
            if (!ReadUtf8File(jsonPath, json))
            {
                SendWebStatus(L"导入失败：无法读取 auth.json 文件", L"error", L"write_failed");
                return;
            }
            if (!IsLikelyValidAuthJson(json))
            {
                SendWebStatus(L"该文件可能不是有效 auth.json，缺少必要字段", L"error", L"auth_json_invalid");
                return;
            }

            pendingImportAuthPath_ = jsonPath;
            SendWebJson(L"{\"type\":\"import_auth_need_name\"}");
            return;
        }

        if (pendingImportAuthPath_.empty())
        {
            SendWebStatus(L"请先选择要导入的 auth.json 文件", L"warning", L"import_cancelled");
            return;
        }

        std::wstring status;
        std::wstring code;
        const bool ok = ImportAuthJsonFile(pendingImportAuthPath_, preferredName, status, code);
        pendingImportAuthPath_.clear();
        const std::wstring level = (code == L"import_cancelled") ? L"warning" : (ok ? L"success" : L"error");
        SendWebStatus(status, level, code);
        if (ok)
        {
            SendAccountsList(false, L"", L"");
        }
        return;
    }

    if (action == L"login_new_account")
    {
        std::wstring status;
        std::wstring code;
        const bool ok = LoginNewAccount(status, code);
        SendWebStatus(status, ok ? L"success" : L"error", code);
        return;
    }

    if (action == L"debug_force_refresh_all")
    {
#ifdef _DEBUG
        TriggerRefreshAll();
        SendWebStatus(L"已触发调试：刷新全部额度", L"success", L"debug_action_ok");
#else
        SendWebStatus(L"仅 Debug 构建支持此操作", L"warning", L"debug_action_unsupported");
#endif
        return;
    }

    if (action == L"debug_force_refresh_current")
    {
#ifdef _DEBUG
        TriggerRefreshCurrent();
        SendWebStatus(L"已触发调试：刷新当前账号额度", L"success", L"debug_action_ok");
#else
        SendWebStatus(L"仅 Debug 构建支持此操作", L"warning", L"debug_action_unsupported");
#endif
        return;
    }

    if (action == L"debug_test_low_quota_notify")
    {
#ifdef _DEBUG
        ShowLowQuotaBalloon(L"当前账号", 18, L"测试账号", L"personal", 86);
        SendWebStatus(L"已触发调试：托盘低额度提醒", L"success", L"debug_action_ok");
#else
        SendWebStatus(L"仅 Debug 构建支持此操作", L"warning", L"debug_action_unsupported");
#endif
        return;
    }

    if (action == L"confirm_low_quota_switch")
    {
        if (!pendingLowQuotaPrompt_ || pendingBestAccountName_.empty())
        {
            SendWebStatus(L"低额度切换提示已过期", L"warning", L"not_found");
            return;
        }
        pendingLowQuotaPrompt_ = false;
        std::wstring status;
        std::wstring code;
        const std::wstring targetName = pendingBestAccountName_;
        const std::wstring targetGroup = pendingBestAccountGroup_;
        pendingBestAccountName_.clear();
        pendingBestAccountGroup_.clear();
        pendingCurrentAccountName_.clear();
        const bool ok = SwitchToAccount(targetName, targetGroup, status, code);
        SendWebStatus(status, ok ? L"success" : L"error", code);
        SendAccountsList(false, L"", L"");
        return;
    }

    if (action == L"cancel_low_quota_switch")
    {
        pendingLowQuotaPrompt_ = false;
        pendingBestAccountName_.clear();
        pendingBestAccountGroup_.clear();
        pendingCurrentAccountName_.clear();
        return;
    }

    SendWebStatus(L"未知操作: " + action, L"error", L"unknown_action");
}

void WebViewHost::Initialize(HWND hwnd)
{
    hwnd_ = hwnd;
    allRefreshIntervalSec_ = kDefaultAllRefreshMinutes * 60;
    currentRefreshIntervalSec_ = kDefaultCurrentRefreshMinutes * 60;
    allRefreshRemainingSec_ = allRefreshIntervalSec_;
    currentRefreshRemainingSec_ = currentRefreshIntervalSec_;
    if (!timerInitialized_)
    {
        SetTimer(hwnd_, kTimerAutoRefresh, 1000, nullptr);
        timerInitialized_ = true;
    }
    EnsureTrayIcon();

    AppConfig startCfg;
    if (LoadConfig(startCfg))
    {
        ApplyWindowTitleTheme(hwnd_, startCfg.theme);
        currentAutoRefreshEnabled_ = startCfg.autoRefreshCurrent;
        lowQuotaPromptEnabled_ = startCfg.lowQuotaAutoPrompt;
        allRefreshIntervalSec_ = ClampRefreshMinutes(startCfg.autoRefreshAllMinutes, kDefaultAllRefreshMinutes) * 60;
        currentRefreshIntervalSec_ = ClampRefreshMinutes(startCfg.autoRefreshCurrentMinutes, kDefaultCurrentRefreshMinutes) * 60;
        allRefreshRemainingSec_ = allRefreshIntervalSec_;
        currentRefreshRemainingSec_ = currentRefreshIntervalSec_;
    }
    userDataFolder_ = MakeTempUserDataFolder();
    if (userDataFolder_.empty())
    {
        ShowHr(hwnd, L"MakeTempUserDataFolder", E_FAIL);
        return;
    }

    const HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr,
        userDataFolder_.c_str(),
        nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this, hwnd](HRESULT result, ICoreWebView2Environment* env) -> HRESULT
            {
                if (FAILED(result) || env == nullptr)
                {
                    ShowHr(hwnd, L"CreateEnvironmentCompleted", FAILED(result) ? result : E_FAIL);
                    return S_OK;
                }

                env->CreateCoreWebView2Controller(
                    hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this, hwnd](HRESULT controllerResult, ICoreWebView2Controller* controller) -> HRESULT
                        {
                            if (FAILED(controllerResult) || controller == nullptr)
                            {
                                ShowHr(hwnd, L"CreateControllerCompleted", FAILED(controllerResult) ? controllerResult : E_FAIL);
                                return S_OK;
                            }

                            controller_ = controller;
                            controller_->put_IsVisible(TRUE);

                            HRESULT hr2 = controller_->get_CoreWebView2(&webview_);
                            if (FAILED(hr2) || !webview_)
                            {
                                ShowHr(hwnd, L"get_CoreWebView2", FAILED(hr2) ? hr2 : E_FAIL);
                                return S_OK;
                            }

                            ComPtr<ICoreWebView2Settings> settings;
                            hr2 = webview_->get_Settings(&settings);
                            if (SUCCEEDED(hr2) && settings)
                            {
                                settings->put_AreDefaultContextMenusEnabled(FALSE);
                                settings->put_AreDevToolsEnabled(FALSE);
                            }

                            Resize(hwnd);
                            RegisterWebMessageHandler(hwnd);
                            const std::wstring webUiPath = FindWebUiIndexPath();
                            if (webUiPath.empty())
                            {
                                const wchar_t* notFoundHtml =
                                    LR"(<html><body style='font-family:Segoe UI;padding:24px;'><h2>WebUI not found</h2><p>Please make sure <code>webui/index.html</code> exists.</p></body></html>)";
                                webview_->NavigateToString(notFoundHtml);
                                return S_OK;
                            }

                            std::wstring fileUrl = ToFileUrl(webUiPath);
#ifdef _DEBUG
                            fileUrl += L"?debug=1";
#endif
                            hr2 = webview_->Navigate(fileUrl.c_str());
                            if (FAILED(hr2))
                            {
                                ShowHr(hwnd, L"Navigate", hr2);
                            }

                            return S_OK;
                        })
                        .Get());

                return S_OK;
            })
            .Get());

    if (FAILED(hr))
    {
        ShowHr(hwnd, L"CreateCoreWebView2EnvironmentWithOptions", hr);
    }
}

void WebViewHost::Resize(HWND hwnd) const
{
    if (!controller_)
    {
        return;
    }

    RECT bounds{};
    GetClientRect(hwnd, &bounds);
    controller_->put_Bounds(bounds);
}

void WebViewHost::Cleanup()
{
    if (timerInitialized_ && hwnd_ != nullptr)
    {
        KillTimer(hwnd_, kTimerAutoRefresh);
        timerInitialized_ = false;
    }
    RemoveTrayIcon();

    if (webview_ != nullptr)
    {
        webview_->remove_WebMessageReceived(webMessageToken_);
    }

    webMessageToken_ = {};
    webview_.Reset();
    controller_.Reset();

    if (!userDataFolder_.empty())
    {
        DeleteDirectoryRecursive(userDataFolder_);
        userDataFolder_.clear();
    }
    pendingLowQuotaPrompt_ = false;
    pendingBestAccountName_.clear();
    pendingBestAccountGroup_.clear();
    pendingCurrentAccountName_.clear();
    pendingImportAuthPath_.clear();
    hwnd_ = nullptr;
}

bool WebViewHost::HandleWindowMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_TIMER)
    {
        if (wParam == kTimerAutoRefresh)
        {
            HandleAutoRefreshTick();
            return true;
        }
        return false;
    }

    if (msg == kMsgTrayNotify)
    {
        const UINT evt = static_cast<UINT>(lParam);
        const UINT evtLo = LOWORD(lParam);
        const UINT evtHi = HIWORD(lParam);
        if (evt == NIN_BALLOONUSERCLICK || evtLo == NIN_BALLOONUSERCLICK || evtHi == NIN_BALLOONUSERCLICK ||
            evt == WM_LBUTTONUP || evtLo == WM_LBUTTONUP || evtHi == WM_LBUTTONUP)
        {
            HandleLowQuotaPromptClick();
        }
        return true;
    }

    if (msg == kMsgLowQuotaCandidate)
    {
        auto* heapPayload = reinterpret_cast<LowQuotaCandidatePayload*>(lParam);
        if (heapPayload == nullptr)
        {
            return true;
        }

        const auto now = std::chrono::steady_clock::now();
        const bool cooledDown = lastLowQuotaPromptAt_.time_since_epoch().count() == 0 ||
            std::chrono::duration_cast<std::chrono::seconds>(now - lastLowQuotaPromptAt_).count() >= kLowQuotaPromptCooldownSeconds;
        if (lowQuotaPromptEnabled_ &&
            hwnd_ != nullptr &&
            !heapPayload->bestName.empty() &&
            (!EqualsIgnoreCase(lastLowQuotaPromptAccountKey_, heapPayload->accountKey) || cooledDown))
        {
            lastLowQuotaPromptAt_ = now;
            lastLowQuotaPromptAccountKey_ = heapPayload->accountKey;
            ShowLowQuotaBalloon(
                heapPayload->currentName,
                heapPayload->currentQuota,
                heapPayload->bestName,
                heapPayload->bestGroup,
                heapPayload->bestQuota);
        }

        delete heapPayload;
        return true;
    }

    if (msg != kMsgAsyncWebJson)
    {
        return false;
    }

    std::wstring* heapJson = reinterpret_cast<std::wstring*>(lParam);
    if (heapJson == nullptr)
    {
        return true;
    }

    SendWebJson(*heapJson);
    delete heapJson;
    return true;
}





