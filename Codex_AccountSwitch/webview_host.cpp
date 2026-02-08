#include "webview_host.h"

#include "app_version.h"
#include "file_utils.h"
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
std::wstring FormatFileTime(const fs::path& path);
bool ReadUtf8File(const fs::path& file, std::wstring& out);
bool WriteUtf8File(const fs::path& file, const std::wstring& content);
bool OpenExternalUrlByExplorer(const std::wstring& url);

constexpr wchar_t kUsageHost[] = L"chatgpt.com";
constexpr wchar_t kUsagePath[] = L"/backend-api/wham/usage";
constexpr wchar_t kUsageDefaultCodexVersion[] = L"0.98.0";
constexpr wchar_t kUsageVsCodeVersion[] = L"0.4.71";

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
    if (group == L"business" || group == L"enterprise")
    {
        return L"business";
    }
    return L"personal";
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

std::wstring GroupFromPlanType(const std::wstring& planType)
{
    const std::wstring lower = ToLowerCopy(planType);
    if (lower == L"team" || lower == L"business" || lower == L"enterprise")
    {
        return L"business";
    }
    return L"personal";
}

bool ParseUsagePayload(const std::wstring& body, UsageSnapshot& out)
{
    out.planType = ExtractJsonField(body, L"plan_type");
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

    const std::wregex kvRe(LR"KV("((?:\\.|[^"])*)"\s*:\s*"((?:\\.|[^"])*)")KV");
    for (std::wsregex_iterator it(json.begin(), json.end(), kvRe), end; it != end; ++it)
    {
        outPairs.emplace_back(
            UnescapeJsonString((*it)[1].str()),
            UnescapeJsonString((*it)[2].str()));
    }
    return !outPairs.empty();
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
    wchar_t fileName[MAX_PATH]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Zip Files (*.zip)\0*.zip\0All Files (*.*)\0*.*\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"zip";

    if (!GetOpenFileNameW(&ofn))
    {
        return false;
    }

    outPath = fileName;
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

    auto scanBackupsRoot = [&](const fs::path& root) {
        if (!fs::exists(root))
        {
            return;
        }

        for (const std::wstring group : { L"personal", L"business" })
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
            if (folder == L"personal" || folder == L"business")
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
    code = L"";
    return true;
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
    code = L"";
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
    code = L"";
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
    code = L"";
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
    code = L"";
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
            case L'n': out.push_back(L'\n'); break;
            case L'r': out.push_back(L'\r'); break;
            case L't': out.push_back(L'\t'); break;
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
                if (NormalizeGroup(row.group) != refreshedGroup)
                {
                    row.group = refreshedGroup;
                    indexChanged = true;
                }
                if (item.isCurrent && NormalizeGroup(idx.currentGroup) != refreshedGroup)
                {
                    idx.currentGroup = refreshedGroup;
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
        L",\"lastSwitchedAccount\":\"" + EscapeJsonString(cfg.lastSwitchedAccount) +
        L"\",\"lastSwitchedGroup\":\"" + EscapeJsonString(cfg.lastSwitchedGroup) +
        L"\",\"lastSwitchedAt\":\"" + EscapeJsonString(cfg.lastSwitchedAt) + L"\"}");
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
        return;
    }

    if (action == L"refresh_accounts")
    {
        const HWND targetHwnd = hwnd_;
        std::thread([targetHwnd]() {
            PostAsyncWebJson(targetHwnd, BuildAccountsListJson(true, L"", L""));
            PostAsyncWebJson(targetHwnd, L"{\"type\":\"status\",\"level\":\"success\",\"code\":\"quota_refreshed\",\"message\":\"额度已刷新\"}");
        }).detach();
        return;
    }

    if (action == L"refresh_account")
    {
        const std::wstring account = ExtractJsonField(rawMessage, L"account");
        const std::wstring group = ExtractJsonField(rawMessage, L"group");
        const HWND targetHwnd = hwnd_;
        std::thread([targetHwnd, account, group]() {
            PostAsyncWebJson(targetHwnd, BuildAccountsListJson(true, account, group));
            PostAsyncWebJson(targetHwnd, L"{\"type\":\"status\",\"level\":\"success\",\"code\":\"account_quota_refreshed\",\"message\":\"账号额度已刷新\"}");
        }).detach();
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
        SendConfig(created);
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
        const bool saved = SaveConfig(cfg);
        if (saved)
        {
            ApplyWindowTitleTheme(hwnd_, cfg.theme);
        }
        SendWebStatus(saved ? L"设置已保存" : L"设置保存失败", saved ? L"success" : L"error", saved ? L"config_saved" : L"save_config_failed");
        SendConfig(false);
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

    if (action == L"login_new_account")
    {
        std::wstring status;
        std::wstring code;
        const bool ok = LoginNewAccount(status, code);
        SendWebStatus(status, ok ? L"success" : L"error", code);
        return;
    }

    SendWebStatus(L"未知操作: " + action, L"error", L"unknown_action");
}

void WebViewHost::Initialize(HWND hwnd)
{
    hwnd_ = hwnd;
    AppConfig startCfg;
    if (LoadConfig(startCfg))
    {
        ApplyWindowTitleTheme(hwnd_, startCfg.theme);
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
    hwnd_ = nullptr;
}

bool WebViewHost::HandleWindowMessage(UINT msg, WPARAM, LPARAM lParam)
{
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





