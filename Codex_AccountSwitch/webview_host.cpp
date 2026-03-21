#include "webview_host.h"

#include "app_version.h"
#include "file_utils.h"
#include "main_window.h"
#include "resource.h"
#include "update_checker.h"

#include <algorithm>
#include <cctype>
#include <cderr.h>
#include <cstdint>
#include <chrono>
#include <commdlg.h>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <dwmapi.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <objbase.h>
#include <regex>
#include <set>
#include <shellapi.h>
#include <sstream>
#include <string>
#include <thread>
#include <tlhelp32.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <wincrypt.h>
#include <winhttp.h>
#include <wrl.h>

using namespace Microsoft::WRL;
namespace fs = std::filesystem;

#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Dwmapi.lib")
#pragma comment(lib, "Winhttp.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Advapi32.lib")

namespace
{
  struct AppConfig;
  std::wstring EscapeJsonString(const std::wstring &input);
  std::wstring UnescapeJsonString(const std::wstring &input);
  std::wstring ToLowerCopy(const std::wstring &value);
  std::wstring FormatFileTime(const fs::path &path);
  bool ReadUtf8File(const fs::path &file, std::wstring &out);
  bool WriteUtf8File(const fs::path &file, const std::wstring &content);
  bool OpenExternalUrlByExplorer(const std::wstring &url);
  void PostAsyncWebJson(const HWND hwnd, const std::wstring &json);
  void SendWebStatusThreadSafe(const HWND hwnd, const std::wstring &text,
                               const std::wstring &level,
                               const std::wstring &code);
  std::wstring BuildAccountsListJson(const bool refreshUsage,
                                     const std::wstring &targetName,
                                     const std::wstring &targetGroup);
  void DebugLogLine(const std::wstring &scope, const std::wstring &message);
  bool ApplyStealthProxyModeToCodexProfile(const AppConfig &cfg,
                                           std::wstring &error);
  bool RestoreCodexProfileFromStealthBackup(std::wstring &error);
  bool SyncStealthProxyEnvironment(const AppConfig &cfg, std::wstring &error);
  std::wstring ParseJwtPayload(const std::wstring &token);
  bool IsLikelyValidAuthJson(const std::wstring &json);
  std::wstring UrlEncode(const std::wstring &value);
  std::wstring MakeUniqueImportedName(const std::wstring &baseName,
                                      const std::vector<std::wstring> &reservedNames);
  bool ImportAuthJsonFile(const std::wstring &jsonPath,
                          const std::wstring &preferredName, std::wstring &status,
                          std::wstring &code, const bool queryUsage,
                          bool *outAbnormal);
  bool SendCodexApiRequestByAuthFile(const fs::path &authPath,
                                     const std::wstring &model,
                                     const std::wstring &inputText,
                                     std::wstring &outputText,
                                     std::wstring &rawResponse,
                                     std::wstring &error,
                                     int &statusCodeOut);
  bool StartLocalProxyService(HWND notifyHwnd, int port, int timeoutSec,
                              bool allowLan, std::wstring &status,
                              std::wstring &code);
  void StopLocalProxyService(std::wstring &status, std::wstring &code);
  std::wstring BuildProxyStatusJson();
  std::wstring GenerateProxyApiKey();
  bool LoadConfig(AppConfig &out);
  bool SaveProtectedWideText(const fs::path &file, const std::wstring &text,
                             std::wstring &error);
  bool LoadProtectedWideText(const fs::path &file, std::wstring &text,
                             std::wstring &error);
  bool DeleteProtectedFile(const fs::path &file, std::wstring &error);
  std::wstring Sha256Base64Url(const std::string &input);
  std::wstring DetectProxyAccountAbnormalReason(const std::wstring &responseBody,
                                                const DWORD statusCode);

  constexpr wchar_t kUsageHost[] = L"chatgpt.com";
  constexpr wchar_t kUsagePath[] = L"/backend-api/wham/usage";
  constexpr wchar_t kUsageDefaultCodexVersion[] = L"0.98.0";
  constexpr wchar_t kUsageVsCodeVersion[] = L"0.4.71";
  constexpr wchar_t kWebView2DownloadUrl[] =
      L"https://developer.microsoft.com/en-us/microsoft-edge/webview2/";
  constexpr UINT_PTR kTimerAutoRefresh = 8201;
  constexpr int kDefaultAllRefreshMinutes = 15;
  constexpr int kDefaultCurrentRefreshMinutes = 5;
  constexpr int kDefaultWebDavSyncMinutes = 15;
  constexpr int kDefaultCloudAccountSyncMinutes = 60;
  constexpr int kMinRefreshMinutes = 1;
  constexpr int kMaxRefreshMinutes = 240;
  constexpr int kMinWebDavSyncMinutes = 1;
  constexpr int kMaxWebDavSyncMinutes = 1440;
  constexpr int kDefaultProxyPort = 8045;
  constexpr int kDefaultProxyTimeoutSec = 120;
  constexpr int kLowQuotaThresholdPercent = 10;
  constexpr int kLowQuotaPromptCooldownSeconds = 30 * 60;
  constexpr int kProxyLowQuotaHintCooldownSeconds = 5 * 60;
  constexpr UINT kTrayCmdOpenWindow = 32001;
  constexpr UINT kTrayCmdMinimizeToTray = 32002;
  constexpr UINT kTrayCmdExitApp = 32003;
  constexpr UINT kTrayCmdSwitchBase = 32100;
  constexpr UINT kTrayCmdSwitchMax = 32299;
  constexpr DWORD kRefreshAllThrottleMs = 100;
  constexpr wchar_t kWebDavManifestFileName[] = L"manifest.json";
  constexpr wchar_t kWebDavSecretFileName[] = L"webdav_secret.bin";
  constexpr wchar_t kWebDavSyncStateFileName[] = L"webdav_sync_state.json";
  constexpr wchar_t kCloudAccountSecretFileName[] = L"cloud_account_secret.bin";
  constexpr wchar_t kCodexApiPathCompact[] = L"/backend-api/codex/responses";
  HWND g_DebugWebHwnd = nullptr;
  bool g_AutoDeleteAbnormalAccounts = false;
  constexpr size_t kTrafficLogMaxEntries = 5000;
  std::recursive_mutex g_IndexDataMutex;

  DWORD ComputeThrottleDelayMs(const DWORD baseMs)
  {
    if (baseMs == 0)
    {
      return 0;
    }
    if (baseMs < 80)
    {
      return baseMs;
    }

    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    const uint64_t tick = static_cast<uint64_t>(GetTickCount64());
    const uint64_t tid = static_cast<uint64_t>(GetCurrentThreadId());
    uint64_t seed =
        tick ^ (tid << 32) ^ static_cast<uint64_t>(counter.QuadPart);
    seed ^= seed >> 33;
    seed *= 0xff51afd7ed558ccdULL;
    seed ^= seed >> 33;
    seed *= 0xc4ceb9fe1a85ec53ULL;
    seed ^= seed >> 33;

    const DWORD jitterRange = std::min<DWORD>(80, baseMs / 2);
    const DWORD span = jitterRange * 2 + 1;
    const DWORD jitter =
        (span == 0) ? 0 : (static_cast<DWORD>(seed % static_cast<uint64_t>(span)));
    return baseMs - jitterRange + jitter;
  }

  std::wstring BuildDebugTimestamp()
  {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t buf[32]{};
    swprintf_s(buf, L"%02u:%02u:%02u.%03u", st.wHour, st.wMinute, st.wSecond,
               st.wMilliseconds);
    return buf;
  }

  void DebugLogLine(const std::wstring &scope, const std::wstring &message)
  {
#ifdef _DEBUG
    const std::wstring line =
        L"[CAS][" + scope + L"] " + BuildDebugTimestamp() + L" " + message;
    OutputDebugStringW((line + L"\n").c_str());
    if (g_DebugWebHwnd != nullptr)
    {
      const std::wstring json =
          L"{\"type\":\"debug_log\",\"scope\":\"" + EscapeJsonString(scope) +
          L"\",\"message\":\"" + EscapeJsonString(message) + L"\"}";
      PostAsyncWebJson(g_DebugWebHwnd, json);
    }
#else
    (void)scope;
    (void)message;
#endif
  }

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

  int ClampWebDavSyncMinutes(const int v, const int fallback)
  {
    int x = v;
    if (x < kMinWebDavSyncMinutes || x > kMaxWebDavSyncMinutes)
    {
      x = fallback;
    }
    if (x < kMinWebDavSyncMinutes)
    {
      x = kMinWebDavSyncMinutes;
    }
    if (x > kMaxWebDavSyncMinutes)
    {
      x = kMaxWebDavSyncMinutes;
    }
    return x;
  }

  std::wstring MaskSecret(const std::wstring &value, const size_t head = 8,
                          const size_t tail = 4)
  {
    if (value.empty())
    {
      return L"<empty>";
    }
    if (value.size() <= head + tail + 3)
    {
      return L"<len=" + std::to_wstring(value.size()) + L">";
    }
    return value.substr(0, head) + L"..." + value.substr(value.size() - tail) +
           L" (len=" + std::to_wstring(value.size()) + L")";
  }

  std::wstring TruncateForLog(const std::wstring &text, const size_t maxLen = 480)
  {
    if (text.size() <= maxLen)
    {
      return text;
    }
    return text.substr(0, maxLen) + L"...(truncated)";
  }

  std::wstring QueryWinHttpHeaderText(HINTERNET hRequest, DWORD query)
  {
    DWORD bytes = 0;
    if (!WinHttpQueryHeaders(hRequest, query, WINHTTP_HEADER_NAME_BY_INDEX,
                             WINHTTP_NO_OUTPUT_BUFFER, &bytes,
                             WINHTTP_NO_HEADER_INDEX))
    {
      if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || bytes == 0)
      {
        return L"";
      }
    }

    std::wstring out(bytes / sizeof(wchar_t), L'\0');
    if (!WinHttpQueryHeaders(hRequest, query, WINHTTP_HEADER_NAME_BY_INDEX,
                             out.data(), &bytes, WINHTTP_NO_HEADER_INDEX))
    {
      return L"";
    }
    while (!out.empty() && out.back() == L'\0')
    {
      out.pop_back();
    }
    return out;
  }

  std::wstring ExtractJsonField(const std::wstring &json,
                                const std::wstring &key)
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

    size_t endQuotePos = firstQuotePos + 1;
    bool escape = false;
    while (endQuotePos < json.size())
    {
      const wchar_t ch = json[endQuotePos];
      if (escape)
      {
        escape = false;
        ++endQuotePos;
        continue;
      }
      if (ch == L'\\')
      {
        escape = true;
        ++endQuotePos;
        continue;
      }
      if (ch == L'"')
      {
        return json.substr(firstQuotePos + 1, endQuotePos - firstQuotePos - 1);
      }
      ++endQuotePos;
    }

    return L"";
  }

  std::wstring ExtractJsonStringField(const std::wstring &json,
                                      const std::wstring &key)
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
    size_t p = colonPos + 1;
    while (p < json.size() && iswspace(json[p]))
    {
      ++p;
    }
    if (p >= json.size() || json[p] != L'"')
    {
      return L"";
    }
    size_t q = p + 1;
    bool escape = false;
    for (; q < json.size(); ++q)
    {
      const wchar_t ch = json[q];
      if (escape)
      {
        escape = false;
        continue;
      }
      if (ch == L'\\')
      {
        escape = true;
        continue;
      }
      if (ch == L'"')
      {
        return json.substr(p + 1, q - p - 1);
      }
    }
    return L"";
  }

  std::wstring ToFileUrl(const std::wstring &path)
  {
    std::wstring normalized = path;
    for (wchar_t &ch : normalized)
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

    for (const auto &candidate : candidates)
    {
      std::error_code ec;
      if (fs::exists(candidate, ec) && !ec)
      {
        return candidate.lexically_normal().wstring();
      }
    }

    return L"";
  }

  std::wstring SanitizeAccountName(const std::wstring &name)
  {
    if (name.empty())
    {
      return L"";
    }

    std::wstring out;
    out.reserve(name.size());
    for (const wchar_t ch : name)
    {
      if (ch == L'<' || ch == L'>' || ch == L':' || ch == L'"' || ch == L'/' ||
          ch == L'\\' || ch == L'|' || ch == L'?' || ch == L'*')
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

  std::wstring NormalizeGroup(const std::wstring &group)
  {
    const std::wstring lower = ToLowerCopy(group);
    if (lower == L"enterprise")
    {
      return L"business";
    }
    if (lower == L"personal" || lower == L"business" || lower == L"free" ||
        lower == L"plus" || lower == L"team" || lower == L"pro")
    {
      return lower;
    }
    return L"personal";
  }

  bool IsPlanGroup(const std::wstring &group)
  {
    const std::wstring lower = ToLowerCopy(group);
    return lower == L"free" || lower == L"plus" || lower == L"team" ||
           lower == L"pro";
  }

  bool EqualsIgnoreCase(const std::wstring &a, const std::wstring &b)
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

  fs::path GetExecutableDir()
  {
    wchar_t modulePath[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) == 0)
    {
      return fs::current_path();
    }
    return fs::path(modulePath).parent_path();
  }

  bool IsPortableModeEnabled()
  {
    const fs::path exeDir = GetExecutableDir();
    const fs::path markerPath = exeDir / L"portable.mode";
    std::error_code ec;
    if (fs::exists(markerPath, ec) && !ec)
    {
      return true;
    }

    wchar_t *envPortable = nullptr;
    size_t required = 0;
    const int envResult = _wdupenv_s(&envPortable, &required, L"CAS_PORTABLE");
    if (envResult == 0 && envPortable != nullptr && *envPortable != L'\0')
    {
      std::wstring text = ToLowerCopy(envPortable);
      free(envPortable);
      if (text == L"1" || text == L"true" || text == L"yes" || text == L"on")
      {
        return true;
      }
      return false;
    }
    free(envPortable);
    return false;
  }

  fs::path GetPortableDataRoot() { return GetExecutableDir() / L"data"; }

  fs::path GetLegacyDataRoot() { return GetWorkspaceRoot(); }

  fs::path GetUserDataRoot()
  {
    if (IsPortableModeEnabled())
    {
      return GetPortableDataRoot();
    }

    wchar_t *localAppData = nullptr;
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

  fs::path GetBackupsDir() { return GetUserDataRoot() / L"backups"; }

  fs::path GetGroupDir(const std::wstring &group)
  {
    return GetBackupsDir() / NormalizeGroup(group);
  }

  fs::path GetIndexPath() { return GetBackupsDir() / L"index.json"; }

  fs::path GetConfigPath() { return GetUserDataRoot() / L"config.json"; }

  fs::path GetWebDavSecretPath()
  {
    return GetUserDataRoot() / kWebDavSecretFileName;
  }

  fs::path GetWebDavSyncStatePath()
  {
    return GetUserDataRoot() / kWebDavSyncStateFileName;
  }

  fs::path GetCloudAccountSecretPath()
  {
    return GetUserDataRoot() / kCloudAccountSecretFileName;
  }

  fs::path GetLegacyBackupsDir() { return GetLegacyDataRoot() / L"backups"; }

  fs::path GetLegacyIndexPath() { return GetLegacyBackupsDir() / L"index.json"; }

  fs::path GetLegacyConfigPath() { return GetLegacyDataRoot() / L"config.json"; }

  struct TabVisibilityConfig
  {
    bool dashboard = true;
    bool accounts = true;
    bool api = true;
    bool traffic = true;
    bool token = true;
    bool cloud = true;
    bool about = true;
    bool settings = true;
  };

  struct AppConfig
  {
    std::wstring language = L"zh-CN";
    int languageIndex = 0;
    std::wstring ideExe = L"Code.exe";
    std::wstring theme = L"auto";
    TabVisibilityConfig tabVisibility{};
    bool autoUpdate = true;
    bool enableAutoRefreshQuota = true;
    bool autoMarkAbnormalAccounts = true;
    bool autoDeleteAbnormalAccounts = false;
    bool autoRefreshCurrent = true;
    bool lowQuotaAutoPrompt = true;
    std::wstring closeWindowBehavior = L"tray";
    int autoRefreshAllMinutes = kDefaultAllRefreshMinutes;
    int autoRefreshCurrentMinutes = kDefaultCurrentRefreshMinutes;
    int proxyPort = kDefaultProxyPort;
    int proxyTimeoutSec = kDefaultProxyTimeoutSec;
    bool proxyAllowLan = false;
    bool proxyAutoStart = false;
    bool proxyStealthMode = false;
    std::wstring proxyApiKey;
    std::wstring proxyDispatchMode = L"round_robin";
    std::wstring proxyFixedAccount;
    std::wstring proxyFixedGroup = L"personal";
    std::wstring lastSwitchedAccount;
    std::wstring lastSwitchedGroup;
    std::wstring lastSwitchedAt;
    std::wstring cloudAccountUrl;
    bool cloudAccountAutoDownload = false;
    int cloudAccountIntervalMinutes = kDefaultCloudAccountSyncMinutes;
    std::wstring cloudAccountLastDownloadAt;
    std::wstring cloudAccountLastDownloadStatus;
    bool cloudAccountPasswordConfigured = false;
    bool webdavEnabled = false;
    bool webdavAutoSync = true;
    int webdavSyncIntervalMinutes = kDefaultWebDavSyncMinutes;
    std::wstring webdavUrl;
    std::wstring webdavRemotePath = L"/CodexAccountSwitch";
    std::wstring webdavUsername;
    std::wstring webdavLastSyncAt;
    std::wstring webdavLastSyncStatus;
    bool webdavPasswordConfigured = false;
  };

  std::wstring NormalizeCloseWindowBehavior(const std::wstring &value)
  {
    const std::wstring lowered = ToLowerCopy(value);
    if (lowered == L"exit")
    {
      return L"exit";
    }
    return L"tray";
  }

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

    const std::wregex itemRe(
        LR"LANG(\{\s*"code"\s*:\s*"((?:\\.|[^"])*)"\s*,\s*"name"\s*:\s*"((?:\\.|[^"])*)"\s*,\s*"file"\s*:\s*"((?:\\.|[^"])*)"\s*\})LANG");
    for (std::wsregex_iterator it(json.begin(), json.end(), itemRe), end;
         it != end; ++it)
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

  int FindLanguageIndexByCode(const std::vector<LanguageMeta> &list,
                              const std::wstring &code)
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

  std::wstring FindLanguageCodeByIndex(const std::vector<LanguageMeta> &list,
                                       const int index)
  {
    if (index >= 0 && index < static_cast<int>(list.size()))
    {
      return list[static_cast<size_t>(index)].code;
    }
    return list.empty() ? L"zh-CN" : list.front().code;
  }

  int ExtractJsonIntField(const std::wstring &json, const std::wstring &key,
                          const int fallback)
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

  bool ExtractJsonBoolField(const std::wstring &json, const std::wstring &key,
                            const bool fallback)
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
    const std::wstring text =
        json.substr(firstQuotePos + 1, secondQuotePos - firstQuotePos - 1);
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

  void NormalizeTabVisibility(TabVisibilityConfig &cfg)
  {
    cfg.settings = true;
  }

  void ReadTabVisibilityFromJson(const std::wstring &json,
                                 TabVisibilityConfig &out)
  {
    out.dashboard = ExtractJsonBoolField(json, L"dashboard", out.dashboard);
    out.accounts = ExtractJsonBoolField(json, L"accounts", out.accounts);
    out.api = ExtractJsonBoolField(json, L"api", out.api);
    out.traffic = ExtractJsonBoolField(json, L"traffic", out.traffic);
    out.token = ExtractJsonBoolField(json, L"token", out.token);
    out.cloud = ExtractJsonBoolField(json, L"cloud", out.cloud);
    out.about = ExtractJsonBoolField(json, L"about", out.about);
    out.settings = ExtractJsonBoolField(json, L"settings", true);
    NormalizeTabVisibility(out);
  }

  std::wstring BuildTabVisibilityJson(const TabVisibilityConfig &input)
  {
    TabVisibilityConfig cfg = input;
    NormalizeTabVisibility(cfg);
    return L"{\"dashboard\":" +
           std::wstring(cfg.dashboard ? L"true" : L"false") +
           L",\"accounts\":" +
           std::wstring(cfg.accounts ? L"true" : L"false") +
           L",\"api\":" + std::wstring(cfg.api ? L"true" : L"false") +
           L",\"traffic\":" +
           std::wstring(cfg.traffic ? L"true" : L"false") +
           L",\"token\":" + std::wstring(cfg.token ? L"true" : L"false") +
           L",\"cloud\":" + std::wstring(cfg.cloud ? L"true" : L"false") +
           L",\"about\":" + std::wstring(cfg.about ? L"true" : L"false") +
           L",\"settings\":true}";
  }

  long long ExtractJsonInt64Field(const std::wstring &json,
                                  const std::wstring &key,
                                  const long long fallback)
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

  bool ExtractJsonObjectField(const std::wstring &json, const std::wstring &key,
                              std::wstring &out)
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

  bool ExtractJsonArrayField(const std::wstring &json, const std::wstring &key,
                             std::wstring &out)
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

  struct AuthJsonCompatFields
  {
    std::wstring authMode;
    std::wstring idToken;
    std::wstring accessToken;
    std::wstring refreshToken;
    std::wstring accountId;
  };

  std::wstring BuildJsonStringLiteral(const std::wstring &value)
  {
    return L"\"" + EscapeJsonString(value) + L"\"";
  }

  size_t FindTopLevelJsonObjectStart(const std::wstring &json)
  {
    for (size_t i = 0; i < json.size(); ++i)
    {
      if (!iswspace(json[i]))
      {
        return json[i] == L'{' ? i : std::wstring::npos;
      }
    }
    return std::wstring::npos;
  }

  size_t FindTopLevelJsonObjectClose(const std::wstring &json)
  {
    const size_t start = FindTopLevelJsonObjectStart(json);
    if (start == std::wstring::npos)
    {
      return std::wstring::npos;
    }

    int depth = 0;
    bool inString = false;
    bool escape = false;
    for (size_t i = start; i < json.size(); ++i)
    {
      const wchar_t ch = json[i];
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
          return i;
        }
      }
    }
    return std::wstring::npos;
  }

  bool JsonContainsTopLevelField(const std::wstring &json, const std::wstring &key)
  {
    const size_t start = FindTopLevelJsonObjectStart(json);
    if (start == std::wstring::npos)
    {
      return false;
    }

    int depth = 0;
    bool inString = false;
    bool escape = false;
    for (size_t i = start; i < json.size(); ++i)
    {
      const wchar_t ch = json[i];
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
        size_t end = i + 1;
        bool stringEscape = false;
        for (; end < json.size(); ++end)
        {
          const wchar_t current = json[end];
          if (stringEscape)
          {
            stringEscape = false;
            continue;
          }
          if (current == L'\\')
          {
            stringEscape = true;
            continue;
          }
          if (current == L'"')
          {
            break;
          }
        }
        if (end >= json.size())
        {
          return false;
        }

        const std::wstring candidate = json.substr(i + 1, end - i - 1);
        size_t cursor = end + 1;
        while (cursor < json.size() && iswspace(json[cursor]))
        {
          ++cursor;
        }
        if (depth == 1 && candidate == key && cursor < json.size() &&
            json[cursor] == L':')
        {
          return true;
        }
        i = end;
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
        if (depth <= 0)
        {
          break;
        }
      }
    }
    return false;
  }

  std::wstring ExtractCompatibleAuthField(const std::wstring &json,
                                          const std::wstring &key)
  {
    std::wstring tokensObject;
    if (ExtractJsonObjectField(json, L"tokens", tokensObject))
    {
      const std::wstring valueFromTokens = ExtractJsonField(tokensObject, key);
      if (!valueFromTokens.empty())
      {
        return valueFromTokens;
      }
    }
    return ExtractJsonField(json, key);
  }

  AuthJsonCompatFields ExtractAuthJsonCompatFields(const std::wstring &json)
  {
    AuthJsonCompatFields out;
    out.authMode = ExtractJsonField(json, L"auth_mode");
    out.idToken = ExtractCompatibleAuthField(json, L"id_token");
    out.accessToken = ExtractCompatibleAuthField(json, L"access_token");
    out.refreshToken = ExtractCompatibleAuthField(json, L"refresh_token");
    out.accountId = ExtractCompatibleAuthField(json, L"account_id");
    return out;
  }

  std::wstring BuildTokensObjectLiteral(const AuthJsonCompatFields &fields)
  {
    std::wstring out = L"{";
    bool first = true;
    auto appendStringField = [&](const std::wstring &key,
                                 const std::wstring &value)
    {
      if (value.empty())
      {
        return;
      }
      if (!first)
      {
        out += L", ";
      }
      out += L"\"" + key + L"\": " + BuildJsonStringLiteral(value);
      first = false;
    };
    appendStringField(L"id_token", fields.idToken);
    appendStringField(L"access_token", fields.accessToken);
    appendStringField(L"refresh_token", fields.refreshToken);
    appendStringField(L"account_id", fields.accountId);
    out += L"}";
    return first ? L"" : out;
  }

  bool AppendTopLevelJsonFieldIfMissing(std::wstring &json, const std::wstring &key,
                                        const std::wstring &rawValue)
  {
    if (rawValue.empty() || JsonContainsTopLevelField(json, key))
    {
      return false;
    }

    const size_t start = FindTopLevelJsonObjectStart(json);
    const size_t close = FindTopLevelJsonObjectClose(json);
    if (start == std::wstring::npos || close == std::wstring::npos || close <= start)
    {
      return false;
    }

    size_t insertPos = close;
    while (insertPos > start + 1 && iswspace(json[insertPos - 1]))
    {
      --insertPos;
    }

    bool hasMembers = false;
    for (size_t i = start + 1; i < close; ++i)
    {
      if (!iswspace(json[i]))
      {
        hasMembers = true;
        break;
      }
    }

    std::wstring fragment = hasMembers ? L",\n  " : L"\n  ";
    fragment += L"\"" + key + L"\": " + rawValue;
    json.insert(insertPos, fragment);
    return true;
  }

  bool NormalizeAuthJsonForCompatibility(const std::wstring &input,
                                         std::wstring &output)
  {
    output = input;
    const size_t start = FindTopLevelJsonObjectStart(output);
    const size_t close = FindTopLevelJsonObjectClose(output);
    if (start == std::wstring::npos || close == std::wstring::npos)
    {
      return false;
    }

    const AuthJsonCompatFields fields = ExtractAuthJsonCompatFields(output);
    bool changed = false;
    if (!JsonContainsTopLevelField(output, L"auth_mode"))
    {
      changed |= AppendTopLevelJsonFieldIfMissing(
          output, L"auth_mode",
          BuildJsonStringLiteral(fields.authMode.empty() ? L"chatgpt" : fields.authMode));
    }
    changed |= AppendTopLevelJsonFieldIfMissing(output, L"OPENAI_API_KEY", L"null");

    const std::wstring tokensObject = BuildTokensObjectLiteral(fields);
    if (!tokensObject.empty())
    {
      changed |= AppendTopLevelJsonFieldIfMissing(output, L"tokens", tokensObject);
    }

    if (!fields.idToken.empty())
    {
      changed |= AppendTopLevelJsonFieldIfMissing(
          output, L"id_token", BuildJsonStringLiteral(fields.idToken));
    }
    if (!fields.accessToken.empty())
    {
      changed |= AppendTopLevelJsonFieldIfMissing(
          output, L"access_token", BuildJsonStringLiteral(fields.accessToken));
    }
    if (!fields.refreshToken.empty())
    {
      changed |= AppendTopLevelJsonFieldIfMissing(
          output, L"refresh_token", BuildJsonStringLiteral(fields.refreshToken));
    }
    if (!fields.accountId.empty())
    {
      changed |= AppendTopLevelJsonFieldIfMissing(
          output, L"account_id", BuildJsonStringLiteral(fields.accountId));
    }
    return changed;
  }

  std::vector<std::wstring>
  ExtractTopLevelObjectsFromArray(const std::wstring &arrayText)
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

  std::wstring ToLowerCopy(const std::wstring &value)
  {
    std::wstring out = value;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](wchar_t ch)
                   { return static_cast<wchar_t>(towlower(ch)); });
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

    using RtlGetVersionFn = LONG(WINAPI *)(RtlOsVersionInfo *);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll != nullptr)
    {
      const auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(
          GetProcAddress(ntdll, "RtlGetVersion"));
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
    wchar_t *userProfile = nullptr;
    size_t required = 0;
    if (_wdupenv_s(&userProfile, &required, L"USERPROFILE") != 0 ||
        userProfile == nullptr || *userProfile == L'\0')
    {
      free(userProfile);
      return kUsageDefaultCodexVersion;
    }

    const fs::path versionPath =
        fs::path(userProfile) / L".codex" / L"version.json";
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
    return L"codex_vscode/" + ReadCodexLatestVersion() + L" (Windows " +
           DetectWindowsVersionTag() + L"; " + DetectCpuArchTag() +
           L") unknown (VS Code; " + std::wstring(kUsageVsCodeVersion) + L")";
  }

  std::wstring FromUtf8(const std::string &text)
  {
    if (text.empty())
    {
      return {};
    }
    const int size = MultiByteToWideChar(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0)
    {
      return {};
    }
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        out.data(), size);
    return out;
  }

  struct UsageSnapshot
  {
    bool ok = false;
    std::wstring error;
    std::wstring planType;
    std::wstring email;
    DWORD httpStatusCode = 0;
    std::wstring responseBody;
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

  std::wstring NormalizePlanType(const std::wstring &planType)
  {
    const std::wstring lower = ToLowerCopy(planType);
    if (lower == L"free" || lower == L"plus" || lower == L"team" ||
        lower == L"pro")
    {
      return lower;
    }
    return L"";
  }

  std::wstring GroupFromPlanType(const std::wstring &planType)
  {
    const std::wstring normalized = NormalizePlanType(planType);
    if (!normalized.empty())
    {
      return normalized;
    }
    return NormalizeGroup(planType);
  }

  bool ParseUsagePayload(const std::wstring &body, UsageSnapshot &out)
  {
    out.planType = ExtractJsonField(body, L"plan_type");
    const std::wstring normalizedPlanType = NormalizePlanType(out.planType);
    if (normalizedPlanType.empty())
    {
      out.error = L"unknown_plan_type:" + out.planType;
      DebugLogLine(L"usage.parse",
                   L"plan_type unsupported, raw=" + out.planType);
      return false;
    }
    out.planType = normalizedPlanType;
    out.email = ExtractJsonField(body, L"email");

    std::wstring rateLimitObj;
    if (!ExtractJsonObjectField(body, L"rate_limit", rateLimitObj))
    {
      out.error = L"rate_limit_missing";
      DebugLogLine(L"usage.parse", L"rate_limit object missing");
      return false;
    }

    std::wstring primaryObj;
    if (ExtractJsonObjectField(rateLimitObj, L"primary_window", primaryObj))
    {
      out.primaryUsedPercent =
          ExtractJsonIntField(primaryObj, L"used_percent", -1);
      out.primaryResetAfterSeconds =
          ExtractJsonInt64Field(primaryObj, L"reset_after_seconds", -1);
      out.primaryResetAt = ExtractJsonInt64Field(primaryObj, L"reset_at", -1);
    }

    std::wstring secondaryObj;
    if (ExtractJsonObjectField(rateLimitObj, L"secondary_window", secondaryObj))
    {
      out.secondaryUsedPercent =
          ExtractJsonIntField(secondaryObj, L"used_percent", -1);
      out.secondaryResetAfterSeconds =
          ExtractJsonInt64Field(secondaryObj, L"reset_after_seconds", -1);
      out.secondaryResetAt = ExtractJsonInt64Field(secondaryObj, L"reset_at", -1);
    }

    out.ok = true;
    DebugLogLine(L"usage.parse",
                 L"ok plan=" + out.planType + L", email=" + out.email);
    return true;
  }

  bool RequestUsageByToken(const std::wstring &accessToken,
                           const std::wstring &accountId,
                           std::wstring &responseBody, std::wstring &error,
                           DWORD *statusCodeOut = nullptr)
  {
    responseBody.clear();
    error.clear();
    if (statusCodeOut != nullptr)
    {
      *statusCodeOut = 0;
    }
    if (accessToken.empty() || accountId.empty())
    {
      error = L"missing_token_or_account_id";
      DebugLogLine(L"usage.http", L"missing accessToken/accountId");
      return false;
    }
    DebugLogLine(L"usage.http",
                 L"prepare request host=" + std::wstring(kUsageHost) +
                     L" path=" + std::wstring(kUsagePath) + L", accountId=" +
                     MaskSecret(accountId, 6, 4) + L", token=" +
                     MaskSecret(accessToken, 10, 6));

    const std::wstring userAgent = BuildUsageUserAgent();
    DebugLogLine(L"usage.http", L"user-agent=" + userAgent);

    HINTERNET hSession =
        WinHttpOpen(userAgent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (hSession == nullptr)
    {
      error = L"WinHttpOpen_failed";
      DebugLogLine(L"usage.http", L"WinHttpOpen failed");
      return false;
    }

    HINTERNET hConnect =
        WinHttpConnect(hSession, kUsageHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (hConnect == nullptr)
    {
      WinHttpCloseHandle(hSession);
      error = L"WinHttpConnect_failed";
      DebugLogLine(L"usage.http", L"WinHttpConnect failed");
      return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, L"GET", kUsagePath, nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (hRequest == nullptr)
    {
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      error = L"WinHttpOpenRequest_failed";
      DebugLogLine(L"usage.http", L"WinHttpOpenRequest failed");
      return false;
    }

    DWORD decompression =
        WINHTTP_DECOMPRESSION_FLAG_GZIP | WINHTTP_DECOMPRESSION_FLAG_DEFLATE;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_DECOMPRESSION, &decompression,
                     sizeof(decompression));

    const std::wstring headers = L"host: chatgpt.com\r\n"
                                 L"connection: keep-alive\r\n"
                                 L"Authorization: Bearer " +
                                 accessToken +
                                 L"\r\n"
                                 L"ChatGPT-Account-Id: " +
                                 accountId +
                                 L"\r\n"
                                 L"originator: codex_vscode\r\n"
                                 L"User-Agent: " +
                                 userAgent +
                                 L"\r\n"
                                 L"accept: */*\r\n"
                                 L"accept-language: *\r\n"
                                 L"sec-fetch-mode: cors\r\n"
                                 L"accept-encoding: gzip, deflate\r\n";

    bool ok =
        WinHttpSendRequest(hRequest, headers.c_str(), static_cast<DWORD>(-1L),
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0) == TRUE;
    if (ok)
    {
      ok = WinHttpReceiveResponse(hRequest, nullptr) == TRUE;
    }
    if (!ok)
    {
      const DWORD winErr = GetLastError();
      error = L"http_transport_failed";
      DebugLogLine(L"usage.http",
                   L"send/receive failed, winErr=" + std::to_wstring(winErr));
      WinHttpCloseHandle(hRequest);
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      return false;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (!WinHttpQueryHeaders(
            hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize,
            WINHTTP_NO_HEADER_INDEX))
    {
      const DWORD winErr = GetLastError();
      error = L"http_status_query_failed";
      DebugLogLine(L"usage.http",
                   L"query status code failed, winErr=" + std::to_wstring(winErr));
      WinHttpCloseHandle(hRequest);
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      return false;
    }

    const std::wstring statusText =
        QueryWinHttpHeaderText(hRequest, WINHTTP_QUERY_STATUS_TEXT);
    const std::wstring rawHeaders =
        QueryWinHttpHeaderText(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF);

    std::string payload;
    while (true)
    {
      DWORD size = 0;
      if (!WinHttpQueryDataAvailable(hRequest, &size))
      {
        const DWORD winErr = GetLastError();
        DebugLogLine(L"usage.http",
                     L"query data available failed, winErr=" +
                         std::to_wstring(winErr));
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
        const DWORD winErr = GetLastError();
        DebugLogLine(L"usage.http",
                     L"read data failed, winErr=" + std::to_wstring(winErr));
        break;
      }
      chunk.resize(read);
      payload += chunk;
    }

    const std::wstring bodyPreview =
        payload.empty() ? L"<empty>" : TruncateForLog(FromUtf8(payload));
    DebugLogLine(L"usage.http",
                 L"http status=" + std::to_wstring(statusCode) + L" (" +
                     statusText + L"), rawHeaders=" +
                     TruncateForLog(rawHeaders, 420));
    DebugLogLine(L"usage.http", L"response preview=" + bodyPreview);

    responseBody = FromUtf8(payload);
    if (statusCodeOut != nullptr)
    {
      *statusCodeOut = statusCode;
    }
    if (statusCode < 200 || statusCode >= 300)
    {
      error = L"http_status_not_ok";
      if (statusCode == 401)
      {
        error = L"http_status_401";
      }
      else if (statusCode == 403)
      {
        error = L"http_status_403";
      }
      DebugLogLine(L"usage.http",
                   L"http status not ok, status=" + std::to_wstring(statusCode) +
                       L", mappedError=" + error);
      WinHttpCloseHandle(hRequest);
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      return false;
    }

    if (payload.empty() || responseBody.empty())
    {
      error = L"response_utf8_decode_failed";
      DebugLogLine(L"usage.http", L"utf8 decode failed");
      WinHttpCloseHandle(hRequest);
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      return false;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    DebugLogLine(L"usage.http",
                 L"request success, bytes=" +
                     std::to_wstring(static_cast<unsigned long long>(
                         responseBody.size())));
    return true;
  }

  bool ReadAuthTokenInfo(const fs::path &authPath, std::wstring &accessToken,
                         std::wstring &accountId)
  {
    accessToken.clear();
    accountId.clear();
    std::wstring authJson;
    if (!ReadUtf8File(authPath, authJson))
    {
      return false;
    }
    const AuthJsonCompatFields fields = ExtractAuthJsonCompatFields(authJson);
    accessToken = fields.accessToken;
    accountId = fields.accountId;
    if (accessToken.empty())
    {
      DebugLogLine(L"usage.query", L"access_token missing in auth.json");
    }
    if (accountId.empty())
    {
      const std::wstring idToken = fields.idToken;
      const std::wstring payload = ParseJwtPayload(idToken);
      if (!payload.empty())
      {
        std::wstring authInfo;
        if (ExtractJsonObjectField(payload, L"https://api.openai.com/auth",
                                   authInfo))
        {
          accountId = ExtractJsonField(authInfo, L"chatgpt_account_id");
        }
        if (accountId.empty())
        {
          accountId = ExtractJsonField(payload, L"chatgpt_account_id");
        }
        if (!accountId.empty())
        {
          DebugLogLine(L"usage.query",
                       L"account_id recovered from id_token payload");
        }
        else
        {
          DebugLogLine(L"usage.query",
                       L"account_id missing in both auth.json and id_token");
        }
      }
    }
    return !accessToken.empty() && !accountId.empty();
  }

  bool QueryUsageFromAuthFile(const fs::path &authPath, UsageSnapshot &out)
  {
    out = UsageSnapshot{};
    DebugLogLine(L"usage.query", L"begin path=" + authPath.wstring());
    std::wstring accessToken;
    std::wstring accountId;
    if (!ReadAuthTokenInfo(authPath, accessToken, accountId))
    {
      out.error = L"auth_token_missing";
      DebugLogLine(L"usage.query", L"auth token/account id missing");
      return false;
    }
    DebugLogLine(L"usage.query",
                 L"token/account prepared, accountId=" +
                     MaskSecret(accountId, 6, 4) + L", token=" +
                     MaskSecret(accessToken, 10, 6));

    std::wstring body;
    std::wstring requestError;
    DWORD statusCode = 0;
    if (!RequestUsageByToken(accessToken, accountId, body, requestError,
                             &statusCode))
    {
      out.error = requestError;
      out.httpStatusCode = statusCode;
      out.responseBody = body;
      DebugLogLine(L"usage.query", L"http failed: " + requestError);
      return false;
    }

    out.httpStatusCode = statusCode;
    out.responseBody = body;

    if (!ParseUsagePayload(body, out))
    {
      if (out.error.empty())
      {
        out.error = L"usage_parse_failed";
      }
      DebugLogLine(L"usage.query", L"parse failed: " + out.error);
      return false;
    }

    out.ok = true;
    DebugLogLine(L"usage.query",
                 L"ok plan=" + out.planType + L", email=" + out.email);
    return true;
  }

  std::wstring DetectUsageRefreshAbnormalReason(const UsageSnapshot &usage)
  {
    const bool isUnauthorized401 =
        usage.httpStatusCode == 401 || ToLowerCopy(usage.error) == L"http_status_401";
    if (!isUnauthorized401)
    {
      return L"";
    }

    std::wstring reason =
        DetectProxyAccountAbnormalReason(usage.responseBody, usage.httpStatusCode);
    if (!reason.empty())
    {
      return reason;
    }

    return L"unauthorized";
  }

  bool ReadJsonStringToken(const std::wstring &text, size_t &i,
                           std::wstring &outRaw)
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

  void SkipJsonValue(const std::wstring &text, size_t &i)
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
    if (open == L'{')
      close = L'}';
    if (open == L'[')
      close = L']';
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

  bool ParseFlatJsonStringMap(
      const std::wstring &json,
      std::vector<std::pair<std::wstring, std::wstring>> &outPairs)
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
      outPairs.emplace_back(UnescapeJsonString(keyRaw),
                            UnescapeJsonString(valueRaw));
    }

    return !outPairs.empty();
  }

  bool LoadLanguageStrings(
      const std::wstring &code,
      std::vector<std::pair<std::wstring, std::wstring>> &outPairs,
      std::wstring &resolvedCode)
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
    const auto &lang = list[static_cast<size_t>(idx)];
    resolvedCode = lang.code;

    fs::path langFile = GetLangIndexPath().parent_path() / lang.file;
    std::wstring json;
    if (!ReadUtf8File(langFile, json))
    {
      return false;
    }

    return ParseFlatJsonStringMap(json, outPairs);
  }

  std::mutex g_RuntimeI18nMutex;
  std::unordered_map<std::wstring, std::wstring> g_RuntimeI18n;
  std::wstring g_RuntimeI18nCode;

  void ReloadRuntimeI18nLocked(const std::wstring &languageCode)
  {
    std::vector<std::pair<std::wstring, std::wstring>> pairs;
    std::wstring resolvedCode;
    if (!LoadLanguageStrings(languageCode, pairs, resolvedCode))
    {
      g_RuntimeI18n.clear();
      g_RuntimeI18nCode = languageCode;
      return;
    }
    std::unordered_map<std::wstring, std::wstring> loaded;
    loaded.reserve(pairs.size());
    for (const auto &row : pairs)
    {
      loaded[row.first] = row.second;
    }
    g_RuntimeI18n = std::move(loaded);
    g_RuntimeI18nCode = resolvedCode.empty() ? languageCode : resolvedCode;
  }

  void EnsureRuntimeI18nLoaded()
  {
    AppConfig cfg;
    const std::wstring desiredCode = LoadConfig(cfg) && !cfg.language.empty()
                                         ? cfg.language
                                         : L"zh-CN";
    std::lock_guard<std::mutex> lock(g_RuntimeI18nMutex);
    if (!g_RuntimeI18n.empty() &&
        _wcsicmp(g_RuntimeI18nCode.c_str(), desiredCode.c_str()) == 0)
    {
      return;
    }
    ReloadRuntimeI18nLocked(desiredCode);
  }

  std::wstring Tr(const std::wstring &key, const std::wstring &fallback = L"")
  {
    EnsureRuntimeI18nLoaded();
    std::lock_guard<std::mutex> lock(g_RuntimeI18nMutex);
    const auto it = g_RuntimeI18n.find(key);
    if (it != g_RuntimeI18n.end() && !it->second.empty())
    {
      return it->second;
    }
    return fallback;
  }

  void ReplaceAllInPlace(std::wstring &text, const std::wstring &from,
                         const std::wstring &to)
  {
    if (from.empty() || text.empty())
    {
      return;
    }
    size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::wstring::npos)
    {
      text.replace(pos, from.size(), to);
      pos += to.size();
    }
  }

  std::wstring TrFormat(
      const std::wstring &key, const std::wstring &fallback,
      const std::vector<std::pair<std::wstring, std::wstring>> &vars)
  {
    std::wstring text = Tr(key, fallback);
    for (const auto &kv : vars)
    {
      ReplaceAllInPlace(text, L"{" + kv.first + L"}", kv.second);
    }
    return text;
  }

  const std::vector<std::wstring> &GetSupportedIdeList()
  {
    static const std::vector<std::wstring> kList = {
        L"Code.exe", L"Trae.exe", L"Kiro.exe", L"Antigravity.exe"};
    return kList;
  }

  std::wstring NormalizeIdeExe(const std::wstring &ideExe)
  {
    if (ideExe.empty())
    {
      return L"Code.exe";
    }

    for (const auto &it : GetSupportedIdeList())
    {
      if (_wcsicmp(it.c_str(), ideExe.c_str()) == 0)
      {
        return it;
      }
    }
    return L"Code.exe";
  }

  std::wstring NormalizeTheme(const std::wstring &theme)
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

  void RefreshWindowFrameByFocusBounce(const HWND hwnd)
  {
    if (hwnd == nullptr || GetForegroundWindow() != hwnd)
    {
      return;
    }

    HINSTANCE instance =
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd, GWLP_HINSTANCE));
    if (instance == nullptr)
    {
      instance = GetModuleHandleW(nullptr);
    }

    constexpr wchar_t kFocusBounceClassName[] =
        L"CodexAccountSwitchFocusBounceWindow";
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = instance;
    wc.lpszClassName = kFocusBounceClassName;
    if (RegisterClassW(&wc) == 0)
    {
      const DWORD err = GetLastError();
      if (err != ERROR_CLASS_ALREADY_EXISTS)
      {
        return;
      }
    }

    const HWND bounce =
        CreateWindowExW(WS_EX_TOOLWINDOW, kFocusBounceClassName, L"", WS_POPUP,
                        -32000, -32000, 1, 1, hwnd, nullptr, instance, nullptr);
    if (bounce == nullptr)
    {
      return;
    }

    ShowWindow(bounce, SW_SHOWNORMAL);
    SetForegroundWindow(bounce);
    SetActiveWindow(bounce);
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    DestroyWindow(bounce);
  }

  bool IsWindowsAppsDarkModeEnabled()
  {
    // Returns true if Dark Mode is enabled (or presumed enabled)
    // Returns false ONLY if Light Mode is explicitly enabled
    auto checkKey = [](const wchar_t *key, bool &isDark) -> bool
    {
      DWORD value = 1;
      DWORD size = sizeof(value);
      if (RegGetValueW(HKEY_CURRENT_USER,
                       L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                       key, RRF_RT_REG_DWORD, nullptr, &value, &size) == ERROR_SUCCESS)
      {
        if (value == 0)
          isDark = true;
        return true; // Key found
      }
      return false; // Key not found
    };

    bool isDark = false;
    bool foundApps = checkKey(L"AppsUseLightTheme", isDark);
    if (isDark)
      return true; // Explicitly Dark

    bool foundSystem = checkKey(L"SystemUsesLightTheme", isDark);
    if (isDark)
      return true; // Explicitly Dark

    if (!foundApps && !foundSystem)
    {
      return true; // FALLBACK: Default to Dark if no registry keys found
    }

    return false; // Found keys and they were Light (1)
  }

  void ApplyWindowTitleTheme(const HWND hwnd, const std::wstring &themeMode)
  {
    if (hwnd == nullptr)
    {
      return;
    }

    const std::wstring mode = NormalizeTheme(themeMode);
    const BOOL useDark =
        (mode == L"dark" || (mode == L"auto" && IsWindowsAppsDarkModeEnabled()))
            ? TRUE
            : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark,
                          sizeof(useDark));
    RefreshWindowFrameByFocusBounce(hwnd);
  }

  std::wstring Utf8ToWide(const std::string &text)
  {
    if (text.empty())
    {
      return L"";
    }

    const int size = MultiByteToWideChar(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0)
    {
      return L"";
    }

    std::wstring out(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        out.data(), size);
    return out;
  }

  std::string WideToUtf8(const std::wstring &text)
  {
    if (text.empty())
    {
      return "";
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(),
                                         static_cast<int>(text.size()), nullptr,
                                         0, nullptr, nullptr);
    if (size <= 0)
    {
      return "";
    }

    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        out.data(), size, nullptr, nullptr);
    return out;
  }

  bool ReadUtf8File(const fs::path &file, std::wstring &out)
  {
    std::ifstream in(file, std::ios::binary);
    if (!in)
    {
      return false;
    }

    std::string bytes((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());
    out = Utf8ToWide(bytes);
    return true;
  }

  bool WriteUtf8File(const fs::path &file, const std::wstring &content)
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

  bool SaveBinaryFile(const fs::path &file, const std::vector<BYTE> &data)
  {
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out)
    {
      return false;
    }
    if (!data.empty())
    {
      out.write(reinterpret_cast<const char *>(data.data()),
                static_cast<std::streamsize>(data.size()));
    }
    return out.good();
  }

  bool ReadBinaryFile(const fs::path &file, std::vector<BYTE> &data)
  {
    data.clear();
    std::ifstream in(file, std::ios::binary);
    if (!in)
    {
      return false;
    }
    data.assign(std::istreambuf_iterator<char>(in),
                std::istreambuf_iterator<char>());
    return true;
  }

  bool SaveProtectedWideText(const fs::path &file, const std::wstring &text,
                             std::wstring &error)
  {
    error.clear();
    DATA_BLOB inBlob{};
    inBlob.pbData = reinterpret_cast<BYTE *>(const_cast<wchar_t *>(text.data()));
    inBlob.cbData = static_cast<DWORD>(text.size() * sizeof(wchar_t));
    DATA_BLOB outBlob{};
    if (!CryptProtectData(&inBlob, L"CodexAccountSwitchWebDavSecret", nullptr,
                          nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN,
                          &outBlob))
    {
      error = L"protect_failed_" + std::to_wstring(GetLastError());
      return false;
    }

    std::vector<BYTE> bytes(outBlob.pbData, outBlob.pbData + outBlob.cbData);
    LocalFree(outBlob.pbData);
    if (!SaveBinaryFile(file, bytes))
    {
      error = L"write_failed";
      return false;
    }
    return true;
  }

  bool LoadProtectedWideText(const fs::path &file, std::wstring &text,
                             std::wstring &error)
  {
    text.clear();
    error.clear();
    std::vector<BYTE> bytes;
    if (!ReadBinaryFile(file, bytes))
    {
      error = L"not_found";
      return false;
    }
    DATA_BLOB inBlob{};
    inBlob.pbData = bytes.data();
    inBlob.cbData = static_cast<DWORD>(bytes.size());
    DATA_BLOB outBlob{};
    if (!CryptUnprotectData(&inBlob, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &outBlob))
    {
      error = L"unprotect_failed_" + std::to_wstring(GetLastError());
      return false;
    }

    if (outBlob.cbData % sizeof(wchar_t) != 0)
    {
      LocalFree(outBlob.pbData);
      error = L"invalid_secret_blob";
      return false;
    }

    text.assign(reinterpret_cast<wchar_t *>(outBlob.pbData),
                outBlob.cbData / sizeof(wchar_t));
    LocalFree(outBlob.pbData);
    return true;
  }

  bool DeleteProtectedFile(const fs::path &file, std::wstring &error)
  {
    error.clear();
    std::error_code ec;
    fs::remove(file, ec);
    if (ec)
    {
      error = L"delete_failed";
      return false;
    }
    return true;
  }

  bool OpenExternalUrlByExplorer(const std::wstring &url)
  {
    if (url.empty())
    {
      return false;
    }
    const HINSTANCE result =
        ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
  }

  bool IsMissingWebView2RuntimeError(const HRESULT hr)
  {
    return hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }

  void RequestMainWindowExit(const HWND hwnd)
  {
    if (hwnd == nullptr)
    {
      return;
    }

    SetPropW(hwnd, kExitWindowPropName, reinterpret_cast<HANDLE>(1));
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
  }

  bool LoadConfig(AppConfig &out)
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
    std::wstring tabVisibilityJson;
    const bool hasTabVisibility =
        ExtractJsonObjectField(json, L"tabVisibility", tabVisibilityJson);
    const bool autoUpdate = ExtractJsonBoolField(json, L"autoUpdate", true);
    const bool hasEnableAutoRefreshQuota =
        json.find(L"\"enableAutoRefreshQuota\"") != std::wstring::npos;
    const bool enableAutoRefreshQuota = hasEnableAutoRefreshQuota
                                            ? ExtractJsonBoolField(
                                                  json, L"enableAutoRefreshQuota",
                                                  true)
                                            : !ExtractJsonBoolField(
                                                  json, L"disableAutoRefreshQuota",
                                                  false);
    const bool autoMarkAbnormalAccounts =
        ExtractJsonBoolField(json, L"autoMarkAbnormalAccounts", true);
    const bool autoDeleteAbnormalAccounts =
        ExtractJsonBoolField(json, L"autoDeleteAbnormalAccounts", false);
    const bool autoRefreshCurrent =
        ExtractJsonBoolField(json, L"autoRefreshCurrent", true);
    const bool lowQuotaAutoPrompt =
        ExtractJsonBoolField(json, L"lowQuotaAutoPrompt", true);
    const std::wstring closeWindowBehavior =
        NormalizeCloseWindowBehavior(
            ExtractJsonField(json, L"closeWindowBehavior"));
    const int autoRefreshAllMinutes = ExtractJsonIntField(
        json, L"autoRefreshAllMinutes", kDefaultAllRefreshMinutes);
    const int autoRefreshCurrentMinutes = ExtractJsonIntField(
        json, L"autoRefreshCurrentMinutes", kDefaultCurrentRefreshMinutes);
    const int proxyPort =
        ExtractJsonIntField(json, L"proxyPort", kDefaultProxyPort);
    const int proxyTimeoutSec =
        ExtractJsonIntField(json, L"proxyTimeoutSec", kDefaultProxyTimeoutSec);
    const bool proxyAllowLan =
        ExtractJsonBoolField(json, L"proxyAllowLan", false);
    const bool proxyAutoStart =
        ExtractJsonBoolField(json, L"proxyAutoStart", false);
    const bool proxyStealthMode =
        ExtractJsonBoolField(json, L"proxyStealthMode", false);
    const std::wstring proxyApiKey = ExtractJsonField(json, L"proxyApiKey");
    const std::wstring proxyDispatchMode =
        ExtractJsonField(json, L"proxyDispatchMode");
    const std::wstring proxyFixedAccount =
        ExtractJsonField(json, L"proxyFixedAccount");
    const std::wstring proxyFixedGroup =
        ExtractJsonField(json, L"proxyFixedGroup");
    const std::wstring lastAccount =
        ExtractJsonField(json, L"lastSwitchedAccount");
    const std::wstring lastGroup = ExtractJsonField(json, L"lastSwitchedGroup");
    const std::wstring lastAt = ExtractJsonField(json, L"lastSwitchedAt");
    const std::wstring cloudAccountUrl =
        ExtractJsonField(json, L"cloudAccountUrl");
    const bool cloudAccountAutoDownload =
        ExtractJsonBoolField(json, L"cloudAccountAutoDownload", false);
    const int cloudAccountIntervalMinutes =
        ExtractJsonIntField(json, L"cloudAccountIntervalMinutes",
                            kDefaultCloudAccountSyncMinutes);
    const std::wstring cloudAccountLastDownloadAt =
        ExtractJsonField(json, L"cloudAccountLastDownloadAt");
    const std::wstring cloudAccountLastDownloadStatus =
        ExtractJsonField(json, L"cloudAccountLastDownloadStatus");
    const bool hasCloudAccountPasswordConfigured =
        json.find(L"\"cloudAccountPasswordConfigured\"") != std::wstring::npos;
    const bool cloudAccountPasswordConfigured =
        hasCloudAccountPasswordConfigured
            ? ExtractJsonBoolField(json, L"cloudAccountPasswordConfigured", false)
            : fs::exists(GetCloudAccountSecretPath());
    const bool webdavEnabled =
        ExtractJsonBoolField(json, L"webdavEnabled", false);
    const bool webdavAutoSync =
        ExtractJsonBoolField(json, L"webdavAutoSync", true);
    const int webdavSyncIntervalMinutes =
        ExtractJsonIntField(json, L"webdavSyncIntervalMinutes",
                            kDefaultWebDavSyncMinutes);
    const std::wstring webdavUrl = ExtractJsonField(json, L"webdavUrl");
    const std::wstring webdavRemotePath =
        ExtractJsonField(json, L"webdavRemotePath");
    const std::wstring webdavUsername =
        ExtractJsonField(json, L"webdavUsername");
    const std::wstring webdavLastSyncAt =
        ExtractJsonField(json, L"webdavLastSyncAt");
    const std::wstring webdavLastSyncStatus =
        ExtractJsonField(json, L"webdavLastSyncStatus");
    const bool hasWebdavPasswordConfigured =
        json.find(L"\"webdavPasswordConfigured\"") != std::wstring::npos;
    const bool webdavPasswordConfigured = hasWebdavPasswordConfigured
                                             ? ExtractJsonBoolField(
                                                   json,
                                                   L"webdavPasswordConfigured",
                                                   false)
                                             : fs::exists(GetWebDavSecretPath());

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
    if (hasTabVisibility)
    {
      ReadTabVisibilityFromJson(tabVisibilityJson, out.tabVisibility);
    }
    NormalizeTabVisibility(out.tabVisibility);
    out.autoUpdate = autoUpdate;
    out.enableAutoRefreshQuota = enableAutoRefreshQuota;
    out.autoMarkAbnormalAccounts = autoMarkAbnormalAccounts;
    out.autoDeleteAbnormalAccounts = autoDeleteAbnormalAccounts;
    out.autoRefreshCurrent = autoRefreshCurrent;
    out.lowQuotaAutoPrompt = lowQuotaAutoPrompt;
    out.closeWindowBehavior = closeWindowBehavior;
    out.autoRefreshAllMinutes =
        ClampRefreshMinutes(autoRefreshAllMinutes, kDefaultAllRefreshMinutes);
    out.autoRefreshCurrentMinutes = ClampRefreshMinutes(
        autoRefreshCurrentMinutes, kDefaultCurrentRefreshMinutes);
    out.proxyPort = proxyPort < 1 || proxyPort > 65535 ? kDefaultProxyPort
                                                       : proxyPort;
    out.proxyTimeoutSec = proxyTimeoutSec < 30
                              ? 30
                              : (proxyTimeoutSec > 7200 ? 7200 : proxyTimeoutSec);
    out.proxyAllowLan = proxyAllowLan;
    out.proxyAutoStart = proxyAutoStart;
    out.proxyStealthMode = proxyStealthMode;
    out.proxyApiKey = proxyApiKey;
    {
      const std::wstring modeLower = ToLowerCopy(proxyDispatchMode);
      if (modeLower == L"round_robin" || modeLower == L"random" ||
          modeLower == L"fixed")
      {
        out.proxyDispatchMode = modeLower;
      }
    }
    out.proxyFixedAccount = proxyFixedAccount;
    out.proxyFixedGroup = NormalizeGroup(proxyFixedGroup);
    out.lastSwitchedAccount = lastAccount;
    out.lastSwitchedGroup = NormalizeGroup(lastGroup);
    out.lastSwitchedAt = lastAt;
    out.cloudAccountUrl = cloudAccountUrl;
    out.cloudAccountAutoDownload = cloudAccountAutoDownload;
    out.cloudAccountIntervalMinutes = ClampWebDavSyncMinutes(
        cloudAccountIntervalMinutes, kDefaultCloudAccountSyncMinutes);
    out.cloudAccountLastDownloadAt = cloudAccountLastDownloadAt;
    out.cloudAccountLastDownloadStatus = cloudAccountLastDownloadStatus;
    out.cloudAccountPasswordConfigured =
        cloudAccountPasswordConfigured && fs::exists(GetCloudAccountSecretPath());
    out.webdavEnabled = webdavEnabled;
    out.webdavAutoSync = webdavAutoSync;
    out.webdavSyncIntervalMinutes = ClampWebDavSyncMinutes(
        webdavSyncIntervalMinutes, kDefaultWebDavSyncMinutes);
    out.webdavUrl = webdavUrl;
    out.webdavRemotePath =
        webdavRemotePath.empty() ? L"/CodexAccountSwitch" : webdavRemotePath;
    out.webdavUsername = webdavUsername;
    out.webdavLastSyncAt = webdavLastSyncAt;
    out.webdavLastSyncStatus = webdavLastSyncStatus;
    out.webdavPasswordConfigured =
        webdavPasswordConfigured && fs::exists(GetWebDavSecretPath());
    return true;
  }

  bool SaveConfig(const AppConfig &in)
  {
    const AppConfig cfg = [&]()
    {
      AppConfig tmp = in;
      if (tmp.language.empty())
      {
        tmp.language = L"zh-CN";
      }
      const auto langs = LoadLanguageIndexList();
      tmp.languageIndex = FindLanguageIndexByCode(langs, tmp.language);
      tmp.ideExe = NormalizeIdeExe(tmp.ideExe);
      tmp.theme = NormalizeTheme(tmp.theme);
      tmp.closeWindowBehavior =
          NormalizeCloseWindowBehavior(tmp.closeWindowBehavior);
      NormalizeTabVisibility(tmp.tabVisibility);
      tmp.autoRefreshAllMinutes = ClampRefreshMinutes(tmp.autoRefreshAllMinutes,
                                                      kDefaultAllRefreshMinutes);
      tmp.autoRefreshCurrentMinutes = ClampRefreshMinutes(
          tmp.autoRefreshCurrentMinutes, kDefaultCurrentRefreshMinutes);
      if (tmp.proxyPort < 1 || tmp.proxyPort > 65535)
      {
        tmp.proxyPort = kDefaultProxyPort;
      }
      if (tmp.proxyTimeoutSec < 30)
      {
        tmp.proxyTimeoutSec = 30;
      }
      else if (tmp.proxyTimeoutSec > 7200)
      {
        tmp.proxyTimeoutSec = 7200;
      }
      if (tmp.proxyApiKey.empty())
      {
        tmp.proxyApiKey = GenerateProxyApiKey();
      }
      tmp.proxyDispatchMode = ToLowerCopy(tmp.proxyDispatchMode);
      if (tmp.proxyDispatchMode != L"round_robin" &&
          tmp.proxyDispatchMode != L"random" &&
          tmp.proxyDispatchMode != L"fixed")
      {
        tmp.proxyDispatchMode = L"round_robin";
      }
      tmp.proxyFixedGroup = NormalizeGroup(tmp.proxyFixedGroup);
      tmp.lastSwitchedGroup = NormalizeGroup(tmp.lastSwitchedGroup);
      tmp.cloudAccountIntervalMinutes = ClampWebDavSyncMinutes(
          tmp.cloudAccountIntervalMinutes, kDefaultCloudAccountSyncMinutes);
      tmp.cloudAccountPasswordConfigured =
          tmp.cloudAccountPasswordConfigured && fs::exists(GetCloudAccountSecretPath());
      tmp.webdavSyncIntervalMinutes = ClampWebDavSyncMinutes(
          tmp.webdavSyncIntervalMinutes, kDefaultWebDavSyncMinutes);
      if (tmp.webdavRemotePath.empty())
      {
        tmp.webdavRemotePath = L"/CodexAccountSwitch";
      }
      tmp.webdavPasswordConfigured =
          tmp.webdavPasswordConfigured && fs::exists(GetWebDavSecretPath());
      return tmp;
    }();

    std::wstringstream ss;
    ss << L"{\n";
    ss << L"  \"language\": \"" << EscapeJsonString(cfg.language) << L"\",\n";
    ss << L"  \"languageIndex\": " << cfg.languageIndex << L",\n";
    ss << L"  \"ideExe\": \"" << EscapeJsonString(cfg.ideExe) << L"\",\n";
    ss << L"  \"theme\": \"" << EscapeJsonString(cfg.theme) << L"\",\n";
    ss << L"  \"tabVisibility\": " << BuildTabVisibilityJson(cfg.tabVisibility)
       << L",\n";
    ss << L"  \"autoUpdate\": " << (cfg.autoUpdate ? L"true" : L"false")
       << L",\n";
    ss << L"  \"enableAutoRefreshQuota\": "
       << (cfg.enableAutoRefreshQuota ? L"true" : L"false") << L",\n";
    ss << L"  \"disableAutoRefreshQuota\": "
       << (cfg.enableAutoRefreshQuota ? L"false" : L"true") << L",\n";
    ss << L"  \"autoMarkAbnormalAccounts\": "
       << (cfg.autoMarkAbnormalAccounts ? L"true" : L"false") << L",\n";
    ss << L"  \"autoDeleteAbnormalAccounts\": "
       << (cfg.autoDeleteAbnormalAccounts ? L"true" : L"false") << L",\n";
    ss << L"  \"autoRefreshCurrent\": "
       << (cfg.autoRefreshCurrent ? L"true" : L"false") << L",\n";
    ss << L"  \"lowQuotaAutoPrompt\": "
       << (cfg.lowQuotaAutoPrompt ? L"true" : L"false") << L",\n";
    ss << L"  \"closeWindowBehavior\": \""
       << EscapeJsonString(cfg.closeWindowBehavior) << L"\",\n";
    ss << L"  \"autoRefreshAllMinutes\": " << cfg.autoRefreshAllMinutes << L",\n";
    ss << L"  \"autoRefreshCurrentMinutes\": " << cfg.autoRefreshCurrentMinutes
       << L",\n";
    ss << L"  \"proxyPort\": " << cfg.proxyPort << L",\n";
    ss << L"  \"proxyTimeoutSec\": " << cfg.proxyTimeoutSec << L",\n";
    ss << L"  \"proxyAllowLan\": " << (cfg.proxyAllowLan ? L"true" : L"false")
       << L",\n";
    ss << L"  \"proxyAutoStart\": " << (cfg.proxyAutoStart ? L"true" : L"false")
       << L",\n";
    ss << L"  \"proxyStealthMode\": "
       << (cfg.proxyStealthMode ? L"true" : L"false") << L",\n";
    ss << L"  \"proxyApiKey\": \"" << EscapeJsonString(cfg.proxyApiKey)
       << L"\",\n";
    ss << L"  \"proxyDispatchMode\": \""
       << EscapeJsonString(cfg.proxyDispatchMode) << L"\",\n";
    ss << L"  \"proxyFixedAccount\": \""
       << EscapeJsonString(cfg.proxyFixedAccount) << L"\",\n";
    ss << L"  \"proxyFixedGroup\": \"" << EscapeJsonString(cfg.proxyFixedGroup)
       << L"\",\n";
    ss << L"  \"lastSwitchedAccount\": \""
       << EscapeJsonString(cfg.lastSwitchedAccount) << L"\",\n";
    ss << L"  \"lastSwitchedGroup\": \""
       << EscapeJsonString(cfg.lastSwitchedGroup) << L"\",\n";
    ss << L"  \"lastSwitchedAt\": \"" << EscapeJsonString(cfg.lastSwitchedAt)
       << L"\",\n";
    ss << L"  \"cloudAccountUrl\": \"" << EscapeJsonString(cfg.cloudAccountUrl)
       << L"\",\n";
    ss << L"  \"cloudAccountAutoDownload\": "
       << (cfg.cloudAccountAutoDownload ? L"true" : L"false") << L",\n";
    ss << L"  \"cloudAccountIntervalMinutes\": "
       << cfg.cloudAccountIntervalMinutes << L",\n";
    ss << L"  \"cloudAccountLastDownloadAt\": \""
       << EscapeJsonString(cfg.cloudAccountLastDownloadAt) << L"\",\n";
    ss << L"  \"cloudAccountLastDownloadStatus\": \""
       << EscapeJsonString(cfg.cloudAccountLastDownloadStatus) << L"\",\n";
    ss << L"  \"cloudAccountPasswordConfigured\": "
       << (cfg.cloudAccountPasswordConfigured ? L"true" : L"false") << L",\n";
    ss << L"  \"webdavEnabled\": "
       << (cfg.webdavEnabled ? L"true" : L"false") << L",\n";
    ss << L"  \"webdavAutoSync\": "
       << (cfg.webdavAutoSync ? L"true" : L"false") << L",\n";
    ss << L"  \"webdavSyncIntervalMinutes\": "
       << cfg.webdavSyncIntervalMinutes << L",\n";
    ss << L"  \"webdavUrl\": \"" << EscapeJsonString(cfg.webdavUrl)
       << L"\",\n";
    ss << L"  \"webdavRemotePath\": \""
       << EscapeJsonString(cfg.webdavRemotePath) << L"\",\n";
    ss << L"  \"webdavUsername\": \""
       << EscapeJsonString(cfg.webdavUsername) << L"\",\n";
    ss << L"  \"webdavLastSyncAt\": \""
       << EscapeJsonString(cfg.webdavLastSyncAt) << L"\",\n";
    ss << L"  \"webdavLastSyncStatus\": \""
       << EscapeJsonString(cfg.webdavLastSyncStatus) << L"\",\n";
    ss << L"  \"webdavPasswordConfigured\": "
       << (cfg.webdavPasswordConfigured ? L"true" : L"false") << L"\n";
    ss << L"}\n";
    const bool saved = WriteUtf8File(GetConfigPath(), ss.str());
    if (saved)
    {
      std::lock_guard<std::mutex> lock(g_RuntimeI18nMutex);
      g_RuntimeI18n.clear();
      g_RuntimeI18nCode.clear();
    }
    return saved;
  }

  bool EnsureConfigExists(bool &created)
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
    wchar_t *userProfile = nullptr;
    size_t required = 0;
    if (_wdupenv_s(&userProfile, &required, L"USERPROFILE") != 0 ||
        userProfile == nullptr || *userProfile == L'\0')
    {
      free(userProfile);
      return L"";
    }

    fs::path path = fs::path(userProfile) / L".codex" / L"auth.json";
    free(userProfile);
    return path.wstring();
  }

  fs::path GetCodexHomeDir()
  {
    wchar_t *userProfile = nullptr;
    size_t required = 0;
    if (_wdupenv_s(&userProfile, &required, L"USERPROFILE") != 0 ||
        userProfile == nullptr || *userProfile == L'\0')
    {
      free(userProfile);
      return fs::path();
    }
    fs::path path = fs::path(userProfile) / L".codex";
    free(userProfile);
    return path;
  }

  std::wstring TrimWide(const std::wstring &value)
  {
    size_t begin = 0;
    while (begin < value.size() && iswspace(value[begin]))
    {
      ++begin;
    }
    size_t end = value.size();
    while (end > begin && iswspace(value[end - 1]))
    {
      --end;
    }
    return value.substr(begin, end - begin);
  }

  bool IsTopLevelManagedTomlKey(const std::wstring &line)
  {
    if (line.empty())
    {
      return false;
    }
    if (line.front() == L' ' || line.front() == L'\t')
    {
      return false;
    }
    const size_t eqPos = line.find(L'=');
    if (eqPos == std::wstring::npos)
    {
      return false;
    }
    const std::wstring key = ToLowerCopy(TrimWide(line.substr(0, eqPos)));
    return key == L"disable_response_storage" ||
           key == L"preferred_auth_method";
  }

  bool IsManagedProviderSection(const std::wstring &trimmedLine)
  {
    if (trimmedLine.size() < 3 || trimmedLine.front() != L'[' ||
        trimmedLine.back() != L']')
    {
      return false;
    }
    const std::wstring section =
        ToLowerCopy(trimmedLine.substr(1, trimmedLine.size() - 2));
    return section == L"model_providers.local_openai" ||
           section.rfind(L"model_providers.local_openai.", 0) == 0 ||
           section == L"model_providers.custom" ||
           section.rfind(L"model_providers.custom.", 0) == 0;
  }

  bool IsTomlSectionHeader(const std::wstring &trimmedLine)
  {
    return trimmedLine.size() >= 3 && trimmedLine.front() == L'[' &&
           trimmedLine.back() == L']';
  }

  bool IsProjectsSectionHeader(const std::wstring &trimmedLine)
  {
    if (trimmedLine.size() < 3 || trimmedLine.front() != L'[' ||
        trimmedLine.back() != L']')
    {
      return false;
    }
    const std::wstring section =
        ToLowerCopy(trimmedLine.substr(1, trimmedLine.size() - 2));
    return section.rfind(L"projects.", 0) == 0;
  }

  std::wstring ExtractTomlKeyName(const std::wstring &line)
  {
    if (line.empty())
    {
      return L"";
    }
    if (line.front() == L' ' || line.front() == L'\t')
    {
      return L"";
    }
    const size_t eqPos = line.find(L'=');
    if (eqPos == std::wstring::npos)
    {
      return L"";
    }
    return ToLowerCopy(TrimWide(line.substr(0, eqPos)));
  }

  std::wstring BuildStealthCodexToml(const std::wstring &origin,
                                     const std::wstring &providerBaseUrl,
                                     const std::wstring &projectBaseUrl)
  {
    std::wstring normalized = origin;
    size_t p = 0;
    while ((p = normalized.find(L"\r\n", p)) != std::wstring::npos)
    {
      normalized.replace(p, 2, L"\n");
    }
    p = 0;
    while ((p = normalized.find(L"\r", p)) != std::wstring::npos)
    {
      normalized.replace(p, 1, L"\n");
    }

    std::vector<std::wstring> lines;
    size_t start = 0;
    while (start <= normalized.size())
    {
      const size_t end = normalized.find(L'\n', start);
      if (end == std::wstring::npos)
      {
        lines.push_back(normalized.substr(start));
        break;
      }
      lines.push_back(normalized.substr(start, end - start));
      start = end + 1;
      if (start == normalized.size())
      {
        lines.push_back(L"");
        break;
      }
    }

    std::vector<std::wstring> out;
    out.reserve(lines.size() + 16);
    bool inManagedProviderSection = false;
    bool inManagedBlock = false;
    bool inProjectSection = false;
    bool projectBaseUrlInjected = false;
    for (const auto &rawLine : lines)
    {
      const std::wstring trimmed = TrimWide(rawLine);
      if (inProjectSection && IsTomlSectionHeader(trimmed))
      {
        if (!projectBaseUrlInjected)
        {
          out.push_back(L"base_url = \"" + projectBaseUrl + L"\"");
        }
        inProjectSection = false;
        projectBaseUrlInjected = false;
      }
      if (trimmed == L"# CAS_LOCAL_PROXY_BEGIN")
      {
        inManagedBlock = true;
        continue;
      }
      if (trimmed == L"# CAS_LOCAL_PROXY_END")
      {
        inManagedBlock = false;
        continue;
      }
      if (inManagedBlock)
      {
        continue;
      }

      if (inManagedProviderSection)
      {
        if (IsTomlSectionHeader(trimmed) && !IsManagedProviderSection(trimmed))
        {
          inManagedProviderSection = false;
        }
        else
        {
          continue;
        }
      }

      if (IsManagedProviderSection(trimmed))
      {
        inManagedProviderSection = true;
        continue;
      }
      if (IsProjectsSectionHeader(trimmed))
      {
        inProjectSection = true;
        projectBaseUrlInjected = false;
        out.push_back(rawLine);
        continue;
      }
      if (inProjectSection)
      {
        const std::wstring key = ExtractTomlKeyName(rawLine);
        if (key == L"base_url")
        {
          if (!projectBaseUrlInjected)
          {
            out.push_back(L"base_url = \"" + projectBaseUrl + L"\"");
            projectBaseUrlInjected = true;
          }
          continue;
        }
      }
      if (IsTopLevelManagedTomlKey(rawLine))
      {
        continue;
      }
      out.push_back(rawLine);
    }
    if (inProjectSection && !projectBaseUrlInjected)
    {
      out.push_back(L"base_url = \"" + projectBaseUrl + L"\"");
    }

    while (!out.empty() && TrimWide(out.back()).empty())
    {
      out.pop_back();
    }

    std::vector<std::wstring> managed;
    managed.push_back(L"# CAS_LOCAL_PROXY_BEGIN");
    managed.push_back(L"disable_response_storage = true");
    managed.push_back(L"preferred_auth_method = \"apikey\"");
    managed.push_back(L"");
    managed.push_back(L"[model_providers.custom]");
    managed.push_back(L"name = \"custom\"");
    managed.push_back(L"base_url = \"" + providerBaseUrl + L"\"");
    managed.push_back(L"wire_api = \"responses\"");
    managed.push_back(L"requires_openai_auth = true");
    managed.push_back(L"# CAS_LOCAL_PROXY_END");

    std::vector<std::wstring> finalLines;
    finalLines.reserve(managed.size() + out.size() + 2);
    finalLines.insert(finalLines.end(), managed.begin(), managed.end());
    if (!out.empty())
    {
      finalLines.push_back(L"");
      finalLines.insert(finalLines.end(), out.begin(), out.end());
    }

    std::wstring merged;
    for (size_t i = 0; i < finalLines.size(); ++i)
    {
      if (i != 0)
      {
        merged += L"\n";
      }
      merged += finalLines[i];
    }
    return merged;
  }

  std::wstring BuildStealthBackupStamp()
  {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t buf[64]{};
    swprintf_s(buf, L"%04u%02u%02u-%02u%02u%02u", st.wYear, st.wMonth, st.wDay,
               st.wHour, st.wMinute, st.wSecond);
    return buf;
  }

  bool SaveStealthOriginalSnapshot(const fs::path &codexHome,
                                   const fs::path &authPath,
                                   const fs::path &tomlPath,
                                   std::wstring &error)
  {
    error.clear();
    const fs::path originalDir = codexHome / L"bak" / L"cas_stealth_original";
    const fs::path authFlagPath = originalDir / L"auth.exists";
    const fs::path tomlFlagPath = originalDir / L"config.exists";
    const fs::path authBackupPath = originalDir / L"auth.json";
    const fs::path tomlBackupPath = originalDir / L"config.toml";

    std::error_code ec;
    fs::create_directories(originalDir, ec);
    if (ec)
    {
      error = L"create_original_backup_dir_failed";
      return false;
    }

    bool authExists = false;
    ec.clear();
    authExists = fs::exists(authPath, ec) && !ec;
    bool tomlExists = false;
    ec.clear();
    tomlExists = fs::exists(tomlPath, ec) && !ec;

    if (!fs::exists(authFlagPath))
    {
      if (!WriteUtf8File(authFlagPath, authExists ? L"1\n" : L"0\n"))
      {
        error = L"write_original_auth_flag_failed";
        return false;
      }
      if (authExists)
      {
        ec.clear();
        fs::copy_file(authPath, authBackupPath, fs::copy_options::overwrite_existing,
                      ec);
        if (ec)
        {
          error = L"save_original_auth_failed";
          return false;
        }
      }
    }

    if (!fs::exists(tomlFlagPath))
    {
      if (!WriteUtf8File(tomlFlagPath, tomlExists ? L"1\n" : L"0\n"))
      {
        error = L"write_original_toml_flag_failed";
        return false;
      }
      if (tomlExists)
      {
        ec.clear();
        fs::copy_file(tomlPath, tomlBackupPath, fs::copy_options::overwrite_existing,
                      ec);
        if (ec)
        {
          error = L"save_original_toml_failed";
          return false;
        }
      }
    }
    return true;
  }

  bool ReadExistsFlag(const fs::path &flagPath, bool &existsOut)
  {
    existsOut = false;
    std::wstring text;
    if (!ReadUtf8File(flagPath, text))
    {
      return false;
    }
    const std::wstring trimmed = TrimWide(text);
    existsOut = (trimmed == L"1" || ToLowerCopy(trimmed) == L"true");
    return true;
  }

  bool ApplyStealthProxyModeToCodexProfile(const AppConfig &cfg,
                                           std::wstring &error)
  {
    error.clear();
    if (cfg.proxyApiKey.empty())
    {
      error = L"proxy_api_key_empty";
      return false;
    }

    const fs::path codexHome = GetCodexHomeDir();
    if (codexHome.empty())
    {
      error = L"userprofile_missing";
      return false;
    }

    const fs::path authPath = codexHome / L"auth.json";
    const fs::path tomlPath = codexHome / L"config.toml";
    const fs::path backupDir = codexHome / L"bak" / BuildStealthBackupStamp();

    std::error_code ec;
    fs::create_directories(codexHome, ec);
    if (ec)
    {
      error = L"create_codex_dir_failed";
      return false;
    }
    ec.clear();
    fs::create_directories(backupDir, ec);
    if (ec)
    {
      error = L"create_backup_dir_failed";
      return false;
    }
    if (!SaveStealthOriginalSnapshot(codexHome, authPath, tomlPath, error))
    {
      return false;
    }

    ec.clear();
    if (fs::exists(authPath, ec) && !ec)
    {
      fs::copy_file(authPath, backupDir / L"auth.json",
                    fs::copy_options::overwrite_existing, ec);
      if (ec)
      {
        error = L"backup_auth_failed";
        return false;
      }
    }
    ec.clear();
    if (fs::exists(tomlPath, ec) && !ec)
    {
      fs::copy_file(tomlPath, backupDir / L"config.toml",
                    fs::copy_options::overwrite_existing, ec);
      if (ec)
      {
        error = L"backup_config_failed";
        return false;
      }
    }

    const std::wstring authJson =
        L"{\"OPENAI_API_KEY\":\"" + EscapeJsonString(cfg.proxyApiKey) + L"\"}\n";
    if (!WriteUtf8File(authPath, authJson))
    {
      error = L"write_auth_failed";
      return false;
    }

    std::wstring existingToml;
    ReadUtf8File(tomlPath, existingToml);
    int safePort = cfg.proxyPort;
    if (safePort < 1 || safePort > 65535)
    {
      safePort = kDefaultProxyPort;
    }
    const std::wstring projectBaseUrl =
        L"http://127.0.0.1:" + std::to_wstring(safePort);
    const std::wstring providerBaseUrl = projectBaseUrl + L"/v1";
    const std::wstring patchedToml =
        BuildStealthCodexToml(existingToml, providerBaseUrl, projectBaseUrl);
    if (!WriteUtf8File(tomlPath, patchedToml))
    {
      error = L"write_toml_failed";
      return false;
    }
    return true;
  }

  bool RestoreCodexProfileFromStealthBackup(std::wstring &error)
  {
    error.clear();
    const fs::path codexHome = GetCodexHomeDir();
    if (codexHome.empty())
    {
      error = L"userprofile_missing";
      return false;
    }
    const fs::path originalDir = codexHome / L"bak" / L"cas_stealth_original";
    const fs::path authFlagPath = originalDir / L"auth.exists";
    const fs::path tomlFlagPath = originalDir / L"config.exists";
    const fs::path authBackupPath = originalDir / L"auth.json";
    const fs::path tomlBackupPath = originalDir / L"config.toml";
    const fs::path authPath = codexHome / L"auth.json";
    const fs::path tomlPath = codexHome / L"config.toml";

    if (!fs::exists(authFlagPath) && !fs::exists(tomlFlagPath))
    {
      return true;
    }

    std::error_code ec;
    bool authExisted = false;
    if (fs::exists(authFlagPath) && !ReadExistsFlag(authFlagPath, authExisted))
    {
      error = L"read_original_auth_flag_failed";
      return false;
    }
    if (authExisted)
    {
      if (!fs::exists(authBackupPath))
      {
        error = L"original_auth_backup_missing";
        return false;
      }
      ec.clear();
      fs::copy_file(authBackupPath, authPath, fs::copy_options::overwrite_existing,
                    ec);
      if (ec)
      {
        error = L"restore_auth_failed";
        return false;
      }
    }
    else
    {
      ec.clear();
      fs::remove(authPath, ec);
    }

    bool tomlExisted = false;
    if (fs::exists(tomlFlagPath) && !ReadExistsFlag(tomlFlagPath, tomlExisted))
    {
      error = L"read_original_toml_flag_failed";
      return false;
    }
    if (tomlExisted)
    {
      if (!fs::exists(tomlBackupPath))
      {
        error = L"original_toml_backup_missing";
        return false;
      }
      ec.clear();
      fs::copy_file(tomlBackupPath, tomlPath, fs::copy_options::overwrite_existing,
                    ec);
      if (ec)
      {
        error = L"restore_toml_failed";
        return false;
      }
    }
    else
    {
      ec.clear();
      fs::remove(tomlPath, ec);
    }
    return true;
  }

  bool SetUserEnvironmentVariable(const std::wstring &name,
                                  const std::wstring *value,
                                  std::wstring &error)
  {
    error.clear();
    HKEY key = nullptr;
    const LONG openRet = RegCreateKeyExW(
        HKEY_CURRENT_USER, L"Environment", 0, nullptr, REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE, nullptr, &key, nullptr);
    if (openRet != ERROR_SUCCESS || key == nullptr)
    {
      error = L"open_env_registry_failed";
      return false;
    }

    bool ok = true;
    if (value != nullptr)
    {
      const std::wstring &v = *value;
      const DWORD bytes = static_cast<DWORD>((v.size() + 1) * sizeof(wchar_t));
      const LONG setRet =
          RegSetValueExW(key, name.c_str(), 0, REG_SZ,
                         reinterpret_cast<const BYTE *>(v.c_str()), bytes);
      if (setRet != ERROR_SUCCESS)
      {
        ok = false;
        error = L"set_env_registry_failed";
      }
    }
    else
    {
      const LONG delRet = RegDeleteValueW(key, name.c_str());
      if (delRet != ERROR_SUCCESS && delRet != ERROR_FILE_NOT_FOUND)
      {
        ok = false;
        error = L"delete_env_registry_failed";
      }
    }
    RegCloseKey(key);
    if (!ok)
    {
      return false;
    }

    // Keep current process env in sync for child processes launched right away.
    if (value != nullptr)
    {
      SetEnvironmentVariableW(name.c_str(), value->c_str());
    }
    else
    {
      SetEnvironmentVariableW(name.c_str(), nullptr);
    }

    DWORD_PTR msgResult = 0;
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                        reinterpret_cast<LPARAM>(L"Environment"),
                        SMTO_ABORTIFHUNG, 2000, &msgResult);
    return true;
  }

  bool SyncStealthProxyEnvironment(const AppConfig &cfg, std::wstring &error)
  {
    error.clear();
    if (cfg.proxyStealthMode)
    {
      // In stealth proxy mode we rely on ~/.codex/auth.json + config.toml only.
      // Do not inject OPENAI_* env vars to avoid override confusion.
      return true;
    }

    std::wstring delErr;
    if (!SetUserEnvironmentVariable(L"OPENAI_API_KEY", nullptr, delErr))
    {
      error = delErr;
      return false;
    }
    if (!SetUserEnvironmentVariable(L"OPENAI_BASE_URL", nullptr, delErr))
    {
      error = delErr;
      return false;
    }
    if (!SetUserEnvironmentVariable(L"OPENAI_API_BASE", nullptr, delErr))
    {
      error = delErr;
      return false;
    }
    return true;
  }

  std::wstring QuotePowerShellLiteral(const std::wstring &text)
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

  bool RunPowerShellCommand(const std::wstring &command)
  {
    std::wstring cmdLine =
        L"powershell -NoProfile -ExecutionPolicy Bypass -Command " + command;
    std::vector<wchar_t> buffer(cmdLine.begin(), cmdLine.end());
    buffer.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    const BOOL ok =
        CreateProcessW(nullptr, buffer.data(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

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

  std::wstring GetFileNameOnly(const std::wstring &pathLike)
  {
    if (pathLike.empty())
    {
      return L"";
    }
    fs::path p(pathLike);
    const std::wstring name = p.filename().wstring();
    return name.empty() ? pathLike : name;
  }

  std::wstring FindIdeExePath(const std::wstring &ideExe)
  {
    wchar_t found[MAX_PATH]{};
    if (SearchPathW(nullptr, ideExe.c_str(), nullptr, MAX_PATH, found, nullptr) >
        0)
    {
      return found;
    }

    wchar_t *localAppData = nullptr;
    size_t required = 0;
    if (_wdupenv_s(&localAppData, &required, L"LOCALAPPDATA") == 0 &&
        localAppData != nullptr && *localAppData != L'\0')
    {
      fs::path candidate =
          fs::path(localAppData) / L"Programs" / L"Microsoft VS Code" / ideExe;
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

  bool StopProcessByName(const std::wstring &processName)
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
          HANDLE process =
              OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
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

  bool LaunchDetachedExe(const std::wstring &exePath)
  {
    std::wstring cmd = L"\"" + exePath + L"\"";
    std::vector<wchar_t> buffer(cmd.begin(), cmd.end());
    buffer.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    const BOOL ok = CreateProcessW(
        exePath.c_str(), buffer.data(), nullptr, nullptr, FALSE,
        CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS, nullptr, nullptr, &si, &pi);
    if (!ok)
    {
      return false;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
  }

  bool LaunchDetachedCommand(const std::wstring &commandLine)
  {
    std::vector<wchar_t> buffer(commandLine.begin(), commandLine.end());
    buffer.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    const BOOL ok = CreateProcessW(
        nullptr, buffer.data(), nullptr, nullptr, FALSE,
        CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS, nullptr, nullptr, &si, &pi);
    if (!ok)
    {
      return false;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
  }

  std::wstring GetIdeDisplayName(const std::wstring &ideExe)
  {
    if (_wcsicmp(ideExe.c_str(), L"Trae.exe") == 0)
      return L"Trae";
    if (_wcsicmp(ideExe.c_str(), L"Kiro.exe") == 0)
      return L"Kiro";
    if (_wcsicmp(ideExe.c_str(), L"Antigravity.exe") == 0)
      return L"Antigravity";
    return L"VSCode";
  }

  std::wstring FindConfiguredIdePath(const std::wstring &ideExe)
  {
    std::wstring path = FindIdeExePath(ideExe);
    if (!path.empty())
    {
      return path;
    }

    wchar_t *localAppData = nullptr;
    size_t required = 0;
    if (_wdupenv_s(&localAppData, &required, L"LOCALAPPDATA") == 0 &&
        localAppData != nullptr && *localAppData != L'\0')
    {
      const fs::path root = fs::path(localAppData) / L"Programs";
      free(localAppData);
      std::error_code ec;
      if (fs::exists(root, ec) && !ec)
      {
        for (const auto &dir : fs::directory_iterator(root, ec))
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

  bool RestartConfiguredIde(std::wstring &ideDisplay)
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

  bool PickOpenZipPath(HWND hwnd, std::wstring &outPath)
  {
    std::vector<wchar_t> fileName(32768, L'\0');
    static constexpr wchar_t kZipFilter[] =
        L"Zip Files (*.zip)\0*.zip\0All Files (*.*)\0*.*\0\0";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = IsWindow(hwnd) ? hwnd : nullptr;
    ofn.lpstrFile = fileName.data();
    ofn.nMaxFile = static_cast<DWORD>(fileName.size());
    ofn.lpstrFilter = kZipFilter;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
                OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = L"zip";

    if (!GetOpenFileNameW(&ofn))
    {
      const DWORD dlgErr = CommDlgExtendedError();
      if (dlgErr != 0)
      {
        OutputDebugStringW((L"[PickOpenZipPath] CommDlgExtendedError=" +
                            std::to_wstring(dlgErr) + L"\n")
                               .c_str());
      }
      return false;
    }

    outPath = fileName.data();
    return true;
  }

  bool PickSaveZipPath(HWND hwnd, std::wstring &outPath)
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
    bool abnormal = false;
    std::wstring abnormalReason;
    std::wstring abnormalAt;
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

  std::wstring BuildAccountNameKey(const std::wstring &name)
  {
    return ToLowerCopy(SanitizeAccountName(name));
  }

  std::wstring BuildIndexEntryKey(const std::wstring &name,
                                  const std::wstring &group)
  {
    return NormalizeGroup(group) + L"::" + BuildAccountNameKey(name);
  }

  bool ShouldPreferDuplicateGroup(const std::wstring &candidateGroup,
                                  const std::wstring &currentGroup)
  {
    const bool candidateIsPlan = IsPlanGroup(candidateGroup);
    const bool currentIsPlan = IsPlanGroup(currentGroup);
    if (candidateIsPlan != currentIsPlan)
    {
      return candidateIsPlan;
    }
    return false;
  }

  bool ShouldPreferPreviousMetadataRow(const IndexEntry &candidate,
                                       const IndexEntry &current)
  {
    if (candidate.quotaUsageOk != current.quotaUsageOk)
    {
      return candidate.quotaUsageOk;
    }
    if (!candidate.quotaPlanType.empty() != !current.quotaPlanType.empty())
    {
      return !candidate.quotaPlanType.empty();
    }
    if (ShouldPreferDuplicateGroup(candidate.group, current.group))
    {
      return true;
    }
    if (candidate.abnormal != current.abnormal)
    {
      return candidate.abnormal;
    }
    if (!candidate.updatedAt.empty() != !current.updatedAt.empty())
    {
      return !candidate.updatedAt.empty();
    }
    return false;
  }

  void CopyIndexEntryRuntimeState(IndexEntry &target, const IndexEntry &source)
  {
    target.abnormal = source.abnormal;
    target.abnormalReason = source.abnormalReason;
    target.abnormalAt = source.abnormalAt;
    target.quotaUsageOk = source.quotaUsageOk;
    target.quotaPlanType = source.quotaPlanType;
    target.quotaEmail = source.quotaEmail;
    target.quota5hRemainingPercent = source.quota5hRemainingPercent;
    target.quota7dRemainingPercent = source.quota7dRemainingPercent;
    target.quota5hResetAfterSeconds = source.quota5hResetAfterSeconds;
    target.quota7dResetAfterSeconds = source.quota7dResetAfterSeconds;
    target.quota5hResetAt = source.quota5hResetAt;
    target.quota7dResetAt = source.quota7dResetAt;
  }

  std::wstring MakeRelativeAuthPath(const std::wstring &group,
                                    const std::wstring &name)
  {
    return L"backups/" + NormalizeGroup(group) + L"/" + name + L"/auth.json";
  }

  fs::path ResolveAuthPathFromIndex(const IndexEntry &item)
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

  bool SyncIndexEntryToGroup(IndexEntry &row, const std::wstring &targetGroup)
  {
    const std::wstring normalizedTarget = NormalizeGroup(targetGroup);
    const std::wstring safeName = SanitizeAccountName(row.name);
    if (!IsPlanGroup(normalizedTarget) || safeName.empty())
    {
      return false;
    }

    const fs::path targetAuth =
        GetGroupDir(normalizedTarget) / safeName / L"auth.json";
    const fs::path sourceAuth = ResolveAuthPathFromIndex(row);
    if (!fs::exists(sourceAuth))
    {
      return false;
    }
    std::error_code ec;
    fs::create_directories(targetAuth.parent_path(), ec);
    if (ec)
    {
      return false;
    }

    if (sourceAuth != targetAuth)
    {
      ec.clear();
      fs::copy_file(sourceAuth, targetAuth, fs::copy_options::overwrite_existing,
                    ec);
      if (ec)
      {
        return false;
      }

      auto normalizePathText = [](const fs::path &path)
      {
        std::wstring text = path.wstring();
        std::replace(text.begin(), text.end(), L'/', L'\\');
        std::transform(text.begin(), text.end(), text.begin(),
                       [](wchar_t ch) { return static_cast<wchar_t>(towlower(ch)); });
        return text;
      };
      auto isManagedBackupPath = [&](const fs::path &path)
      {
        const std::wstring value = normalizePathText(path);
        auto startsWithRoot = [&](const fs::path &root)
        {
          std::wstring rootText = normalizePathText(root);
          if (!rootText.empty() && rootText.back() != L'\\')
          {
            rootText.push_back(L'\\');
          }
          return !rootText.empty() && value.rfind(rootText, 0) == 0;
        };
        return startsWithRoot(GetBackupsDir()) || startsWithRoot(GetLegacyBackupsDir());
      };

      if (isManagedBackupPath(sourceAuth))
      {
        ec.clear();
        fs::remove(sourceAuth, ec);
        ec.clear();
        fs::remove(sourceAuth.parent_path(), ec);
      }
    }

    row.group = normalizedTarget;
    row.path = MakeRelativeAuthPath(normalizedTarget, safeName);
    return true;
  }

  bool LoadIndex(IndexData &out)
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
      for (const auto &itemJson : objects)
      {
        IndexEntry row;
        row.name = UnescapeJsonString(ExtractJsonField(itemJson, L"name"));
        row.group = NormalizeGroup(
            UnescapeJsonString(ExtractJsonField(itemJson, L"group")));
        row.path = UnescapeJsonString(ExtractJsonField(itemJson, L"path"));
        row.updatedAt =
            UnescapeJsonString(ExtractJsonField(itemJson, L"updatedAt"));
        row.abnormal = ExtractJsonBoolField(itemJson, L"abnormal", false);
        row.abnormalReason =
            UnescapeJsonString(ExtractJsonField(itemJson, L"abnormalReason"));
        row.abnormalAt =
            UnescapeJsonString(ExtractJsonField(itemJson, L"abnormalAt"));
        row.quotaUsageOk = ExtractJsonBoolField(itemJson, L"usageOk", false);
        row.quotaPlanType =
            UnescapeJsonString(ExtractJsonField(itemJson, L"planType"));
        row.quotaEmail = UnescapeJsonString(ExtractJsonField(itemJson, L"email"));
        row.quota5hRemainingPercent =
            ExtractJsonIntField(itemJson, L"quota5hRemainingPercent", -1);
        row.quota7dRemainingPercent =
            ExtractJsonIntField(itemJson, L"quota7dRemainingPercent", -1);
        row.quota5hResetAfterSeconds =
            ExtractJsonInt64Field(itemJson, L"quota5hResetAfterSeconds", -1);
        row.quota7dResetAfterSeconds =
            ExtractJsonInt64Field(itemJson, L"quota7dResetAfterSeconds", -1);
        row.quota5hResetAt =
            ExtractJsonInt64Field(itemJson, L"quota5hResetAt", -1);
        row.quota7dResetAt =
            ExtractJsonInt64Field(itemJson, L"quota7dResetAt", -1);
        if (!row.name.empty() && !row.path.empty())
        {
          out.accounts.push_back(row);
        }
      }
    }

    const std::wregex currentRe(
        LR"INDEX("current"\s*:\s*\{\s*"name"\s*:\s*"((?:\\.|[^"])*)"\s*,\s*"group"\s*:\s*"((?:\\.|[^"])*)"\s*\})INDEX");
    std::wsmatch m;
    if (std::regex_search(json, m, currentRe))
    {
      out.currentName = UnescapeJsonString(m[1].str());
      out.currentGroup = NormalizeGroup(UnescapeJsonString(m[2].str()));
    }

    return true;
  }

  bool SaveIndex(const IndexData &data)
  {
    std::wstringstream ss;
    ss << L"{\n";
    ss << L"  \"current\": {\"name\":\"" << EscapeJsonString(data.currentName)
       << L"\",\"group\":\""
       << EscapeJsonString(NormalizeGroup(data.currentGroup)) << L"\"},\n";
    ss << L"  \"accounts\": [\n";
    for (size_t i = 0; i < data.accounts.size(); ++i)
    {
      const auto &row = data.accounts[i];
      ss << L"    {\"name\":\"" << EscapeJsonString(row.name)
         << L"\",\"group\":\"" << EscapeJsonString(NormalizeGroup(row.group))
         << L"\",\"path\":\"" << EscapeJsonString(row.path)
         << L"\",\"updatedAt\":\"" << EscapeJsonString(row.updatedAt)
         << L"\",\"abnormal\":" << (row.abnormal ? L"true" : L"false")
         << L",\"abnormalReason\":\"" << EscapeJsonString(row.abnormalReason)
         << L"\",\"abnormalAt\":\"" << EscapeJsonString(row.abnormalAt)
         << L"\",\"usageOk\":" << (row.quotaUsageOk ? L"true" : L"false")
         << L",\"planType\":\"" << EscapeJsonString(row.quotaPlanType)
         << L"\",\"email\":\"" << EscapeJsonString(row.quotaEmail)
         << L"\",\"quota5hRemainingPercent\":" << row.quota5hRemainingPercent
         << L",\"quota7dRemainingPercent\":" << row.quota7dRemainingPercent
         << L",\"quota5hResetAfterSeconds\":" << row.quota5hResetAfterSeconds
         << L",\"quota7dResetAfterSeconds\":" << row.quota7dResetAfterSeconds
         << L",\"quota5hResetAt\":" << row.quota5hResetAt
         << L",\"quota7dResetAt\":" << row.quota7dResetAt << L"}";
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
    const std::lock_guard<std::recursive_mutex> lock(g_IndexDataMutex);
    IndexData idx;
    if (LoadIndex(idx))
    {
      IndexData scanned;
      scanned.currentName = idx.currentName;
      scanned.currentGroup = NormalizeGroup(idx.currentGroup);

      const fs::path backupsDir = GetBackupsDir();
      const fs::path legacyBackupsDir = GetLegacyBackupsDir();
      std::error_code ec;
      fs::create_directories(backupsDir, ec);
      for (const std::wstring group :
           {L"personal", L"business", L"free", L"plus", L"team", L"pro"})
      {
        ec.clear();
        fs::create_directories(backupsDir / group, ec);
      }

      auto scanBackupsRoot = [&](const fs::path &root)
      {
        if (!fs::exists(root))
        {
          return;
        }

        for (const std::wstring group :
             {L"personal", L"business", L"free", L"plus", L"team", L"pro"})
        {
          const fs::path groupDir = root / group;
          if (!fs::exists(groupDir))
          {
            continue;
          }

          for (const auto &entry : fs::directory_iterator(groupDir, ec))
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
            const auto duplicated =
                std::find_if(scanned.accounts.begin(), scanned.accounts.end(),
                             [&](const IndexEntry &row)
                             {
                               return EqualsIgnoreCase(row.name, name);
                             });
            if (duplicated != scanned.accounts.end())
            {
              if (ShouldPreferDuplicateGroup(group, duplicated->group))
              {
                duplicated->group = group;
                duplicated->path = MakeRelativeAuthPath(group, name);
                duplicated->updatedAt = FormatFileTime(auth);
              }
              continue;
            }

            IndexEntry row;
            row.name = name;
            row.group = group;
            row.path = MakeRelativeAuthPath(group, row.name);
            row.updatedAt = FormatFileTime(auth);
            scanned.accounts.push_back(row);
          }
        }

        for (const auto &entry : fs::directory_iterator(root, ec))
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

          const auto duplicated =
              std::find_if(scanned.accounts.begin(), scanned.accounts.end(),
                           [&](const IndexEntry &row)
                           {
                             return EqualsIgnoreCase(row.name, folder);
                           });
          if (duplicated != scanned.accounts.end())
          {
            continue;
          }

          IndexEntry row;
          row.name = folder;
          row.group = L"personal";
          row.path = MakeRelativeAuthPath(row.group, row.name);
          row.updatedAt = FormatFileTime(auth);
          scanned.accounts.push_back(row);
        }
      };

      scanBackupsRoot(backupsDir);
      if (legacyBackupsDir != backupsDir)
      {
        scanBackupsRoot(legacyBackupsDir);
      }

      std::unordered_map<std::wstring, IndexEntry> previousByKey;
      std::unordered_map<std::wstring, IndexEntry> previousByName;
      for (const auto &row : idx.accounts)
      {
        previousByKey[BuildIndexEntryKey(row.name, row.group)] = row;
        const std::wstring nameKey = BuildAccountNameKey(row.name);
        auto existing = previousByName.find(nameKey);
        if (existing == previousByName.end() ||
            ShouldPreferPreviousMetadataRow(row, existing->second))
        {
          previousByName[nameKey] = row;
        }
      }

      for (auto &row : scanned.accounts)
      {
        const auto it = previousByKey.find(BuildIndexEntryKey(row.name, row.group));
        if (it != previousByKey.end())
        {
          CopyIndexEntryRuntimeState(row, it->second);
          continue;
        }

        const auto fallback = previousByName.find(BuildAccountNameKey(row.name));
        if (fallback != previousByName.end())
        {
          CopyIndexEntryRuntimeState(row, fallback->second);
        }
      }

      bool currentExists = false;
      if (!scanned.currentName.empty())
      {
        const std::wstring currentKey =
            BuildIndexEntryKey(scanned.currentName, scanned.currentGroup);
        currentExists = std::any_of(scanned.accounts.begin(), scanned.accounts.end(),
                                    [&](const IndexEntry &row)
                                    {
                                      return BuildIndexEntryKey(row.name, row.group) ==
                                             currentKey;
                                    });
        if (!currentExists)
        {
          const std::wstring currentNameKey =
              BuildAccountNameKey(scanned.currentName);
          const auto sameName =
              std::find_if(scanned.accounts.begin(), scanned.accounts.end(),
                           [&](const IndexEntry &row)
                           {
                             return BuildAccountNameKey(row.name) == currentNameKey;
                           });
          if (sameName != scanned.accounts.end())
          {
            scanned.currentGroup = NormalizeGroup(sameName->group);
            currentExists = true;
          }
        }
      }
      if (!currentExists)
      {
        scanned.currentName.clear();
        scanned.currentGroup = L"personal";
      }

      SaveIndex(scanned);
      return;
    }

    IndexData generated;
    const fs::path backupsDir = GetBackupsDir();
    const fs::path legacyBackupsDir = GetLegacyBackupsDir();
    std::error_code ec;
    fs::create_directories(backupsDir, ec);
    for (const std::wstring group :
         {L"personal", L"business", L"free", L"plus", L"team", L"pro"})
    {
      ec.clear();
      fs::create_directories(backupsDir / group, ec);
    }

    auto scanBackupsRoot = [&](const fs::path &root)
    {
      if (!fs::exists(root))
      {
        return;
      }

      for (const std::wstring group :
           {L"personal", L"business", L"free", L"plus", L"team", L"pro"})
      {
        const fs::path groupDir = root / group;
        if (!fs::exists(groupDir))
        {
          continue;
        }

        for (const auto &entry : fs::directory_iterator(groupDir, ec))
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
          const auto duplicated =
              std::find_if(generated.accounts.begin(), generated.accounts.end(),
                           [&](const IndexEntry &row)
                           {
                             return EqualsIgnoreCase(row.name, name);
                           });
          if (duplicated != generated.accounts.end())
          {
            if (ShouldPreferDuplicateGroup(group, duplicated->group))
            {
              duplicated->group = group;
              duplicated->path = MakeRelativeAuthPath(group, name);
              duplicated->updatedAt = FormatFileTime(auth);
            }
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
      for (const auto &entry : fs::directory_iterator(root, ec))
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

        const auto duplicated =
            std::find_if(generated.accounts.begin(), generated.accounts.end(),
                         [&](const IndexEntry &row)
                         {
                           return EqualsIgnoreCase(row.name, folder);
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

  struct WebDavFileEntry
  {
    std::wstring account;
    std::wstring group = L"personal";
    std::wstring relativePath;
    std::wstring updatedAt;
    std::wstring sha256;
    long long size = 0;
  };

  struct WebDavManifest
  {
    std::wstring generatedAt;
    std::vector<WebDavFileEntry> files;
  };

  struct WebDavRequestConfig
  {
    bool secure = true;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
    std::wstring host;
    std::wstring rootPath;
    std::wstring basePath;
    std::wstring authHeader;
  };

  struct WebDavConflictEntry
  {
    WebDavFileEntry localFile;
    WebDavFileEntry remoteFile;
  };

  struct WebDavPendingConflictContext
  {
    bool active = false;
    std::wstring mode;
    WebDavManifest localManifest;
    WebDavManifest remoteManifest;
    WebDavManifest baseManifest;
    std::vector<WebDavConflictEntry> conflicts;
  };

  std::mutex g_WebDavConflictMutex;
  WebDavPendingConflictContext g_WebDavPendingConflict;

  std::wstring BuildWebDavAccountKey(const std::wstring &account)
  {
    return ToLowerCopy(SanitizeAccountName(account));
  }

  std::wstring BuildWebDavRelativePath(const std::wstring &group,
                                       const std::wstring &account)
  {
    return NormalizeGroup(group) + L"/" + SanitizeAccountName(account) +
           L"/auth.json";
  }

  bool IsSameWebDavState(const WebDavFileEntry &a, const WebDavFileEntry &b)
  {
    return EqualsIgnoreCase(a.account, b.account) &&
           NormalizeGroup(a.group) == NormalizeGroup(b.group) &&
           a.sha256 == b.sha256;
  }

  std::wstring NormalizeWebDavRemotePath(const std::wstring &input)
  {
    std::wstring path = input;
    std::replace(path.begin(), path.end(), L'\\', L'/');
    while (!path.empty() && iswspace(path.front()))
    {
      path.erase(path.begin());
    }
    while (!path.empty() && iswspace(path.back()))
    {
      path.pop_back();
    }
    if (path.empty())
    {
      path = L"/CodexAccountSwitch";
    }
    if (path.front() != L'/')
    {
      path.insert(path.begin(), L'/');
    }
    while (path.size() > 1 && path.back() == L'/')
    {
      path.pop_back();
    }
    return path;
  }

  std::wstring JoinUrlPath(const std::wstring &left, const std::wstring &right)
  {
    if (left.empty())
    {
      return right;
    }
    if (right.empty())
    {
      return left;
    }
    const bool leftSlash = left.back() == L'/';
    const bool rightSlash = right.front() == L'/';
    if (leftSlash && rightSlash)
    {
      return left + right.substr(1);
    }
    if (!leftSlash && !rightSlash)
    {
      return left + L"/" + right;
    }
    return left + right;
  }

  std::wstring WebDavManifestRemotePath(const WebDavRequestConfig &cfg)
  {
    return JoinUrlPath(cfg.basePath, kWebDavManifestFileName);
  }

  std::wstring WebDavRemoteFilePath(const WebDavRequestConfig &cfg,
                                    const WebDavFileEntry &entry)
  {
    return JoinUrlPath(JoinUrlPath(cfg.basePath, L"accounts"),
                       entry.relativePath);
  }

  std::wstring Base64EncodeWide(const std::string &input)
  {
    if (input.empty())
    {
      return L"";
    }
    DWORD size = 0;
    if (!CryptBinaryToStringA(
            reinterpret_cast<const BYTE *>(input.data()),
            static_cast<DWORD>(input.size()),
            CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &size) ||
        size == 0)
    {
      return L"";
    }
    std::string out(size, '\0');
    if (!CryptBinaryToStringA(
            reinterpret_cast<const BYTE *>(input.data()),
            static_cast<DWORD>(input.size()),
            CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, out.data(), &size))
    {
      return L"";
    }
    if (!out.empty() && out.back() == '\0')
    {
      out.pop_back();
    }
    return Utf8ToWide(out);
  }

  bool BuildWebDavRequestConfig(const AppConfig &cfg, WebDavRequestConfig &out,
                                std::wstring &error)
  {
    error.clear();
    out = WebDavRequestConfig{};
    if (cfg.webdavUrl.empty())
    {
      error = L"missing_url";
      return false;
    }
    if (cfg.webdavUsername.empty())
    {
      error = L"missing_username";
      return false;
    }
    std::wstring password;
    if (!LoadProtectedWideText(GetWebDavSecretPath(), password, error) ||
        password.empty())
    {
      if (error.empty())
      {
        error = L"missing_password";
      }
      return false;
    }

    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwSchemeLength = static_cast<DWORD>(-1);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(cfg.webdavUrl.c_str(), 0, 0, &parts))
    {
      error = L"invalid_url";
      return false;
    }
    out.secure = parts.nScheme != INTERNET_SCHEME_HTTP;
    out.port = parts.nPort;
    out.host.assign(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring urlPath;
    if (parts.dwUrlPathLength > 0)
    {
      urlPath.append(parts.lpszUrlPath, parts.dwUrlPathLength);
    }
    if (parts.dwExtraInfoLength > 0)
    {
      urlPath.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    }
    if (urlPath.empty())
    {
      urlPath = L"/";
    }
    while (urlPath.size() > 1 && urlPath.back() == L'/')
    {
      urlPath.pop_back();
    }
    out.rootPath = urlPath;
    out.basePath = JoinUrlPath(urlPath, NormalizeWebDavRemotePath(cfg.webdavRemotePath));
    out.authHeader =
        L"Authorization: Basic " +
        Base64EncodeWide(WideToUtf8(cfg.webdavUsername + L":" + password)) +
        L"\r\n";
    return !out.host.empty();
  }

  bool SendWebDavRequest(const WebDavRequestConfig &cfg,
                         const std::wstring &method,
                         const std::wstring &path,
                         const std::string &body,
                         const std::wstring &contentType,
                         DWORD &statusCode,
                         std::string &responseBody,
                         std::wstring &error)
  {
    statusCode = 0;
    responseBody.clear();
    error.clear();

    HINTERNET hSession = WinHttpOpen(L"Codex Account Switch/1.0",
                                     WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession)
    {
      error = L"WinHttpOpen_failed";
      return false;
    }
    HINTERNET hConnect =
        WinHttpConnect(hSession, cfg.host.c_str(), cfg.port, 0);
    if (!hConnect)
    {
      WinHttpCloseHandle(hSession);
      error = L"WinHttpConnect_failed";
      return false;
    }
    HINTERNET hRequest =
        WinHttpOpenRequest(hConnect, method.c_str(), path.c_str(), nullptr,
                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                           cfg.secure ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest)
    {
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      error = L"WinHttpOpenRequest_failed";
      return false;
    }

    DWORD decompression =
        WINHTTP_DECOMPRESSION_FLAG_GZIP | WINHTTP_DECOMPRESSION_FLAG_DEFLATE;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_DECOMPRESSION, &decompression,
                     sizeof(decompression));
    WinHttpSetTimeouts(hRequest, 10000, 10000, 30000, 30000);

    std::wstring headers = cfg.authHeader + L"Accept: */*\r\n";
    if (!contentType.empty())
    {
      headers += L"Content-Type: " + contentType + L"\r\n";
    }
    BOOL ok = WinHttpSendRequest(
        hRequest, headers.c_str(), static_cast<DWORD>(-1L),
        body.empty() ? WINHTTP_NO_REQUEST_DATA
                     : const_cast<char *>(body.data()),
        static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0);
    if (ok)
    {
      ok = WinHttpReceiveResponse(hRequest, nullptr);
    }
    if (!ok)
    {
      error = L"http_transport_failed";
      WinHttpCloseHandle(hRequest);
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      return false;
    }

    DWORD statusSize = sizeof(statusCode);
    if (!WinHttpQueryHeaders(hRequest,
                             WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &statusCode,
                             &statusSize, WINHTTP_NO_HEADER_INDEX))
    {
      error = L"http_status_query_failed";
      WinHttpCloseHandle(hRequest);
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      return false;
    }

    for (;;)
    {
      DWORD bytesAvailable = 0;
      if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable))
      {
        error = L"http_query_data_failed";
        break;
      }
      if (bytesAvailable == 0)
      {
        break;
      }
      std::string chunk(static_cast<size_t>(bytesAvailable), '\0');
      DWORD bytesRead = 0;
      if (!WinHttpReadData(hRequest, chunk.data(), bytesAvailable, &bytesRead))
      {
        error = L"http_read_failed";
        break;
      }
      responseBody.append(chunk.data(), chunk.data() + bytesRead);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return error.empty();
  }

  std::vector<std::wstring> SplitWebDavPathSegments(const std::wstring &path)
  {
    std::vector<std::wstring> out;
    std::wstring cleaned = path;
    std::replace(cleaned.begin(), cleaned.end(), L'\\', L'/');
    std::wstring current;
    for (wchar_t ch : cleaned)
    {
      if (ch == L'/')
      {
        if (!current.empty())
        {
          out.push_back(current);
          current.clear();
        }
        continue;
      }
      current.push_back(ch);
    }
    if (!current.empty())
    {
      out.push_back(current);
    }
    return out;
  }

  bool EnsureWebDavDirectoryTree(const WebDavRequestConfig &cfg,
                                 const std::wstring &path,
                                 std::wstring &error)
  {
    error.clear();
    std::wstring normalizedPath = path;
    std::replace(normalizedPath.begin(), normalizedPath.end(), L'\\', L'/');
    if (normalizedPath.empty())
    {
      normalizedPath = L"/";
    }

    std::wstring root = cfg.rootPath.empty() ? L"/" : cfg.rootPath;
    std::replace(root.begin(), root.end(), L'\\', L'/');
    if (root.empty())
    {
      root = L"/";
    }
    while (root.size() > 1 && root.back() == L'/')
    {
      root.pop_back();
    }

    std::wstring relativePath = normalizedPath;
    if (relativePath == root)
    {
      return true;
    }
    if (root != L"/" && relativePath.rfind(root, 0) == 0)
    {
      relativePath.erase(0, root.size());
    }
    while (!relativePath.empty() && relativePath.front() == L'/')
    {
      relativePath.erase(relativePath.begin());
    }

    std::wstring current = root;
    const auto segments = SplitWebDavPathSegments(relativePath);
    for (const auto &segment : segments)
    {
      current = JoinUrlPath(current, segment);
      DWORD statusCode = 0;
      std::string body;
      if (!SendWebDavRequest(cfg, L"MKCOL", current, std::string{}, L"",
                             statusCode, body, error))
      {
        return false;
      }
      if (!(statusCode == 201 || statusCode == 405 || statusCode == 301 ||
            statusCode == 302))
      {
        error = L"mkcol_http_" + std::to_wstring(statusCode);
        return false;
      }
    }
    return true;
  }

  bool ParseWebDavManifest(const std::wstring &json, WebDavManifest &out)
  {
    out = WebDavManifest{};
    out.generatedAt = UnescapeJsonString(ExtractJsonField(json, L"generatedAt"));
    std::wstring filesArray;
    if (!ExtractJsonArrayField(json, L"files", filesArray))
    {
      return true;
    }
    const auto objects = ExtractTopLevelObjectsFromArray(filesArray);
    for (const auto &itemJson : objects)
    {
      WebDavFileEntry row;
      row.account = UnescapeJsonString(ExtractJsonField(itemJson, L"account"));
      row.group = NormalizeGroup(
          UnescapeJsonString(ExtractJsonField(itemJson, L"group")));
      row.relativePath =
          UnescapeJsonString(ExtractJsonField(itemJson, L"relativePath"));
      row.updatedAt =
          UnescapeJsonString(ExtractJsonField(itemJson, L"updatedAt"));
      row.sha256 = UnescapeJsonString(ExtractJsonField(itemJson, L"sha256"));
      row.size = ExtractJsonInt64Field(itemJson, L"size", 0);
      if (row.account.empty() || row.relativePath.empty())
      {
        continue;
      }
      out.files.push_back(row);
    }
    return true;
  }

  std::wstring SerializeWebDavManifest(const WebDavManifest &manifest)
  {
    std::wstringstream ss;
    ss << L"{\n";
    ss << L"  \"generatedAt\": \"" << EscapeJsonString(manifest.generatedAt)
       << L"\",\n";
    ss << L"  \"files\": [\n";
    for (size_t i = 0; i < manifest.files.size(); ++i)
    {
      const auto &row = manifest.files[i];
      ss << L"    {\"account\":\"" << EscapeJsonString(row.account)
         << L"\",\"group\":\"" << EscapeJsonString(NormalizeGroup(row.group))
         << L"\",\"relativePath\":\""
         << EscapeJsonString(row.relativePath) << L"\",\"updatedAt\":\""
         << EscapeJsonString(row.updatedAt) << L"\",\"sha256\":\""
         << EscapeJsonString(row.sha256) << L"\",\"size\":" << row.size
         << L"}";
      if (i + 1 < manifest.files.size())
      {
        ss << L",";
      }
      ss << L"\n";
    }
    ss << L"  ]\n";
    ss << L"}\n";
    return ss.str();
  }

  bool LoadWebDavBaseline(WebDavManifest &out)
  {
    out = WebDavManifest{};
    std::wstring json;
    if (!ReadUtf8File(GetWebDavSyncStatePath(), json))
    {
      return false;
    }
    return ParseWebDavManifest(json, out);
  }

  bool SaveWebDavBaseline(const WebDavManifest &manifest)
  {
    return WriteUtf8File(GetWebDavSyncStatePath(), SerializeWebDavManifest(manifest));
  }

  bool ReadLocalAuthJsonFile(const fs::path &path, std::wstring &content,
                             WebDavFileEntry &entry, std::wstring &error)
  {
    error.clear();
    if (!ReadUtf8File(path, content))
    {
      error = L"read_auth_failed";
      return false;
    }
    std::wstring normalized;
    if (NormalizeAuthJsonForCompatibility(content, normalized))
    {
      content = normalized;
    }
    entry.updatedAt = FormatFileTime(path);
    entry.size = static_cast<long long>(WideToUtf8(content).size());
    entry.sha256 = Sha256Base64Url(WideToUtf8(content));
    return true;
  }

  bool CollectLocalWebDavManifest(WebDavManifest &out, std::wstring &error)
  {
    error.clear();
    out = WebDavManifest{};
    out.generatedAt = NowText();
    std::map<std::wstring, WebDavFileEntry> dedup;
    auto collectEntry = [&](const std::wstring &group,
                            const std::wstring &account,
                            const fs::path &authPath) -> bool
    {
      const std::wstring safeName = SanitizeAccountName(account);
      if (safeName.empty() || !fs::exists(authPath))
      {
        return true;
      }
      WebDavFileEntry entry;
      entry.account = safeName;
      entry.group = NormalizeGroup(group);
      entry.relativePath = BuildWebDavRelativePath(entry.group, entry.account);
      std::wstring content;
      if (!ReadLocalAuthJsonFile(authPath, content, entry, error))
      {
        return false;
      }
      const std::wstring key = BuildWebDavAccountKey(entry.account);
      const auto it = dedup.find(key);
      if (it == dedup.end() || entry.updatedAt >= it->second.updatedAt)
      {
        dedup[key] = entry;
      }
      return true;
    };

    auto scanBackupsRoot = [&](const fs::path &root) -> bool
    {
      std::error_code ec;
      if (!fs::exists(root))
      {
        return true;
      }
      for (const auto &entry : fs::directory_iterator(root, ec))
      {
        if (ec || !entry.is_directory())
        {
          continue;
        }
        const std::wstring folder = entry.path().filename().wstring();
        const std::wstring lower = ToLowerCopy(folder);
        if (lower == L"personal" || lower == L"business" || lower == L"free" ||
            lower == L"plus" || lower == L"team" || lower == L"pro" ||
            lower == L"enterprise")
        {
          for (const auto &accountDir : fs::directory_iterator(entry.path(), ec))
          {
            if (ec || !accountDir.is_directory())
            {
              continue;
            }
            if (!collectEntry(folder, accountDir.path().filename().wstring(),
                              accountDir.path() / L"auth.json"))
            {
              return false;
            }
          }
        }
        else
        {
          if (!collectEntry(L"personal", folder, entry.path() / L"auth.json"))
          {
            return false;
          }
        }
      }
      return true;
    };

    if (!scanBackupsRoot(GetBackupsDir()))
    {
      return false;
    }
    if (GetLegacyBackupsDir() != GetBackupsDir() &&
        !scanBackupsRoot(GetLegacyBackupsDir()))
    {
      return false;
    }
    for (const auto &pair : dedup)
    {
      out.files.push_back(pair.second);
    }
    std::sort(out.files.begin(), out.files.end(), [](const WebDavFileEntry &a,
                                                     const WebDavFileEntry &b)
              { return ToLowerCopy(a.account) < ToLowerCopy(b.account); });
    return true;
  }

  std::map<std::wstring, WebDavFileEntry>
  BuildWebDavFileMap(const WebDavManifest &manifest)
  {
    std::map<std::wstring, WebDavFileEntry> out;
    for (const auto &entry : manifest.files)
    {
      out[BuildWebDavAccountKey(entry.account)] = entry;
    }
    return out;
  }

  bool WebDavGetManifest(const WebDavRequestConfig &cfg, WebDavManifest &manifest,
                         bool &exists, std::wstring &error)
  {
    exists = false;
    manifest = WebDavManifest{};
    DWORD statusCode = 0;
    std::string body;
    if (!SendWebDavRequest(cfg, L"GET", WebDavManifestRemotePath(cfg), std::string{},
                           L"", statusCode, body, error))
    {
      return false;
    }
    if (statusCode == 404)
    {
      return true;
    }
    if (statusCode != 200)
    {
      error = L"manifest_http_" + std::to_wstring(statusCode);
      return false;
    }
    exists = true;
    return ParseWebDavManifest(Utf8ToWide(body), manifest);
  }

  bool WebDavPutTextFile(const WebDavRequestConfig &cfg, const std::wstring &path,
                         const std::wstring &content, std::wstring &error)
  {
    const fs::path parent = fs::path(path).parent_path();
    if (!EnsureWebDavDirectoryTree(cfg, parent.wstring(), error))
    {
      return false;
    }
    DWORD statusCode = 0;
    std::string body;
    if (!SendWebDavRequest(cfg, L"PUT", path, WideToUtf8(content),
                           L"application/json; charset=utf-8", statusCode, body,
                           error))
    {
      return false;
    }
    if (!(statusCode == 200 || statusCode == 201 || statusCode == 204))
    {
      error = L"put_http_" + std::to_wstring(statusCode);
      return false;
    }
    return true;
  }

  bool WebDavDeletePath(const WebDavRequestConfig &cfg,
                        const std::wstring &path,
                        std::wstring &error)
  {
    DWORD statusCode = 0;
    std::string body;
    if (!SendWebDavRequest(cfg, L"DELETE", path, std::string{}, L"", statusCode,
                           body, error))
    {
      return false;
    }
    if (!(statusCode == 200 || statusCode == 204 || statusCode == 404))
    {
      error = L"delete_http_" + std::to_wstring(statusCode);
      return false;
    }
    return true;
  }

  bool WebDavDownloadTextFile(const WebDavRequestConfig &cfg,
                              const std::wstring &path,
                              std::wstring &content,
                              std::wstring &error)
  {
    content.clear();
    DWORD statusCode = 0;
    std::string body;
    if (!SendWebDavRequest(cfg, L"GET", path, std::string{}, L"", statusCode,
                           body, error))
    {
      return false;
    }
    if (statusCode != 200)
    {
      error = L"get_http_" + std::to_wstring(statusCode);
      return false;
    }
    content = Utf8ToWide(body);
    return true;
  }

  bool UpsertLocalWebDavAccount(const WebDavFileEntry &entry,
                                const std::wstring &content,
                                std::wstring &error)
  {
    const std::lock_guard<std::recursive_mutex> lock(g_IndexDataMutex);
    error.clear();
    const std::wstring safeName = SanitizeAccountName(entry.account);
    const std::wstring safeGroup = NormalizeGroup(entry.group);
    const fs::path targetDir = GetGroupDir(safeGroup) / safeName;
    const fs::path targetAuth = targetDir / L"auth.json";
    std::error_code ec;
    fs::create_directories(targetDir, ec);
    if (ec)
    {
      error = L"create_dir_failed";
      return false;
    }
    std::wstring normalizedContent = content;
    NormalizeAuthJsonForCompatibility(content, normalizedContent);
    if (!WriteUtf8File(targetAuth, normalizedContent))
    {
      error = L"write_failed";
      return false;
    }

    EnsureIndexExists();
    IndexData idx;
    LoadIndex(idx);
    auto it = std::find_if(idx.accounts.begin(), idx.accounts.end(),
                           [&](const IndexEntry &row)
                           {
                             return EqualsIgnoreCase(row.name, safeName);
                           });
    const std::wstring nextPath = MakeRelativeAuthPath(safeGroup, safeName);
    if (it == idx.accounts.end())
    {
      IndexEntry row;
      row.name = safeName;
      row.group = safeGroup;
      row.path = nextPath;
      row.updatedAt = entry.updatedAt.empty() ? NowText() : entry.updatedAt;
      idx.accounts.push_back(row);
    }
    else
    {
      const fs::path oldAuth = ResolveAuthPathFromIndex(*it);
      it->name = safeName;
      it->group = safeGroup;
      it->path = nextPath;
      it->updatedAt = entry.updatedAt.empty() ? NowText() : entry.updatedAt;
      if (EqualsIgnoreCase(idx.currentName, safeName))
      {
        idx.currentGroup = safeGroup;
      }
      if (!oldAuth.empty() && oldAuth != targetAuth)
      {
        std::error_code removeEc;
        fs::remove(oldAuth, removeEc);
      }
    }
    SaveIndex(idx);
    return true;
  }

  void StoreWebDavPendingConflictContext(const WebDavPendingConflictContext &ctx)
  {
    std::lock_guard<std::mutex> lock(g_WebDavConflictMutex);
    g_WebDavPendingConflict = ctx;
  }

  bool LoadWebDavPendingConflictContext(WebDavPendingConflictContext &ctx)
  {
    std::lock_guard<std::mutex> lock(g_WebDavConflictMutex);
    if (!g_WebDavPendingConflict.active)
    {
      return false;
    }
    ctx = g_WebDavPendingConflict;
    return true;
  }

  void ClearWebDavPendingConflictContext()
  {
    std::lock_guard<std::mutex> lock(g_WebDavConflictMutex);
    g_WebDavPendingConflict = WebDavPendingConflictContext{};
  }

  std::wstring BuildWebDavConflictsJson(
      const std::vector<WebDavConflictEntry> &conflicts)
  {
    std::wstringstream ss;
    ss << L"{\"type\":\"webdav_sync_conflicts\",\"conflicts\":[";
    for (size_t i = 0; i < conflicts.size(); ++i)
    {
      const auto &item = conflicts[i];
      ss << L"{\"account\":\"" << EscapeJsonString(item.localFile.account)
         << L"\",\"localGroup\":\""
         << EscapeJsonString(NormalizeGroup(item.localFile.group))
         << L"\",\"localUpdatedAt\":\""
         << EscapeJsonString(item.localFile.updatedAt)
         << L"\",\"remoteGroup\":\""
         << EscapeJsonString(NormalizeGroup(item.remoteFile.group))
         << L"\",\"remoteUpdatedAt\":\""
         << EscapeJsonString(item.remoteFile.updatedAt) << L"\"}";
      if (i + 1 < conflicts.size())
      {
        ss << L",";
      }
    }
    ss << L"]}";
    return ss.str();
  }

  std::wstring BuildWebDavSyncStateJson(const AppConfig &cfg, const bool running,
                                        const int remainingSec)
  {
    return L"{\"type\":\"webdav_sync_status\",\"enabled\":" +
           std::wstring(cfg.webdavEnabled ? L"true" : L"false") +
           L",\"autoSync\":" +
           std::wstring(cfg.webdavAutoSync ? L"true" : L"false") +
           L",\"intervalMinutes\":" +
           std::to_wstring(cfg.webdavSyncIntervalMinutes) +
           L",\"remainingSec\":" + std::to_wstring(remainingSec < 0 ? 0 : remainingSec) +
           L",\"running\":" + std::wstring(running ? L"true" : L"false") +
           L",\"lastSyncAt\":\"" + EscapeJsonString(cfg.webdavLastSyncAt) +
           L"\",\"lastSyncStatus\":\"" + EscapeJsonString(cfg.webdavLastSyncStatus) +
           L"\",\"passwordConfigured\":" +
           std::wstring(cfg.webdavPasswordConfigured ? L"true" : L"false") + L"}";
  }

  void UpdateWebDavStatusInConfig(const std::wstring &status,
                                  const bool touchTime,
                                  AppConfig *cfgOut = nullptr)
  {
    AppConfig cfg;
    LoadConfig(cfg);
    cfg.webdavLastSyncStatus = status;
    if (touchTime)
    {
      cfg.webdavLastSyncAt = NowText();
    }
    cfg.webdavPasswordConfigured = fs::exists(GetWebDavSecretPath());
    SaveConfig(cfg);
    if (cfgOut != nullptr)
    {
      *cfgOut = cfg;
    }
  }

  struct CloudAccountRequestConfig
  {
    bool secure = false;
    INTERNET_PORT port = 0;
    std::wstring host;
    std::wstring basePath;
    std::wstring accessKey;
  };

  struct CloudProviderItemMeta
  {
    std::wstring name;
    std::wstring mtime;
    std::wstring path;
  };

  struct CloudAccountIdentity
  {
    std::wstring accountId;
    std::wstring email;
    std::wstring normalizedContent;
  };

  struct CloudAccountDownloadResult
  {
    bool ok = false;
    bool accountsChanged = false;
    std::wstring level = L"error";
    std::wstring statusText;
    std::wstring statusCode;
    int successCount = 0;
    int failedCount = 0;
    int skippedCount = 0;
    int totalCount = 0;
    std::wstring lastError;
    AppConfig cfg;
  };

  std::wstring BuildProgressStatusJson(const std::wstring &code,
                                       const int current,
                                       const int total,
                                       const std::wstring &scope = L"")
  {
    std::wstringstream ss;
    ss << L"{\"type\":\"status\",\"level\":\"info\",\"code\":\""
       << EscapeJsonString(code) << L"\",\"message\":\"\""
       << L",\"current\":" << std::to_wstring(current < 0 ? 0 : current)
       << L",\"total\":" << std::to_wstring(total < 0 ? 0 : total);
    if (!scope.empty())
    {
      ss << L",\"scope\":\"" << EscapeJsonString(scope) << L"\"";
    }
    ss << L"}";
    return ss.str();
  }

  std::wstring BuildCloudAccountResultStatusJson(
      const CloudAccountDownloadResult &result)
  {
    std::wstringstream ss;
    ss << L"{\"type\":\"status\",\"level\":\""
       << EscapeJsonString(result.level.empty() ? L"error" : result.level)
       << L"\",\"code\":\""
       << EscapeJsonString(result.statusCode.empty() ? L"cloud_account_batch_failed"
                                                     : result.statusCode)
       << L"\",\"message\":\""
       << EscapeJsonString(result.statusText.empty() ? L"云账号下载失败"
                                                     : result.statusText)
       << L"\",\"success\":" << std::to_wstring(result.successCount)
       << L",\"failed\":" << std::to_wstring(result.failedCount)
       << L",\"skipped\":" << std::to_wstring(result.skippedCount)
       << L",\"total\":" << std::to_wstring(result.totalCount)
       << L",\"lastError\":\"" << EscapeJsonString(result.lastError) << L"\"}";
    return ss.str();
  }

  std::wstring BuildCloudAccountStateJson(const AppConfig &cfg,
                                          const bool running,
                                          const int remainingSec)
  {
    return L"{\"type\":\"cloud_account_status\",\"autoDownload\":" +
           std::wstring(cfg.cloudAccountAutoDownload ? L"true" : L"false") +
           L",\"intervalMinutes\":" +
           std::to_wstring(cfg.cloudAccountIntervalMinutes) +
           L",\"remainingSec\":" +
           std::to_wstring(remainingSec < 0 ? 0 : remainingSec) +
           L",\"running\":" + std::wstring(running ? L"true" : L"false") +
           L",\"lastDownloadAt\":\"" +
           EscapeJsonString(cfg.cloudAccountLastDownloadAt) +
           L"\",\"lastDownloadStatus\":\"" +
           EscapeJsonString(cfg.cloudAccountLastDownloadStatus) +
           L"\",\"passwordConfigured\":" +
           std::wstring(cfg.cloudAccountPasswordConfigured ? L"true" : L"false") +
           L"}";
  }

  void UpdateCloudAccountStatusInConfig(const std::wstring &status,
                                        const bool touchTime,
                                        AppConfig *cfgOut = nullptr)
  {
    AppConfig cfg;
    LoadConfig(cfg);
    cfg.cloudAccountLastDownloadStatus = status;
    if (touchTime)
    {
      cfg.cloudAccountLastDownloadAt = NowText();
    }
    cfg.cloudAccountPasswordConfigured = fs::exists(GetCloudAccountSecretPath());
    SaveConfig(cfg);
    if (cfgOut != nullptr)
    {
      *cfgOut = cfg;
    }
  }

  std::wstring NormalizeCloudAccountBasePath(std::wstring path)
  {
    while (path.size() > 1 && path.back() == L'/')
    {
      path.pop_back();
    }
    const std::wstring lower = ToLowerCopy(path);
    if (lower.size() >= 5 && lower.substr(lower.size() - 5) == L"/json")
    {
      path.resize(path.size() - 5);
    }
    while (path.size() > 1 && path.back() == L'/')
    {
      path.pop_back();
    }
    return path == L"/" ? L"" : path;
  }

  std::wstring BuildCloudAccountRequestPath(const CloudAccountRequestConfig &cfg,
                                            const std::wstring &suffix)
  {
    if (cfg.basePath.empty())
    {
      return suffix.empty() ? L"/" : suffix;
    }
    if (suffix.empty())
    {
      return cfg.basePath;
    }
    if (!suffix.empty() && suffix.front() == L'/')
    {
      return cfg.basePath + suffix;
    }
    return cfg.basePath + L"/" + suffix;
  }

  bool BuildCloudAccountRequestConfig(const AppConfig &cfg,
                                      CloudAccountRequestConfig &out,
                                      std::wstring &error)
  {
    error.clear();
    out = CloudAccountRequestConfig{};
    if (cfg.cloudAccountUrl.empty())
    {
      error = L"missing_url";
      return false;
    }
    std::wstring accessKey;
    if (!LoadProtectedWideText(GetCloudAccountSecretPath(), accessKey, error) ||
        accessKey.empty())
    {
      if (error.empty())
      {
        error = L"missing_password";
      }
      return false;
    }

    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwSchemeLength = static_cast<DWORD>(-1);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(cfg.cloudAccountUrl.c_str(), 0, 0, &parts))
    {
      error = L"invalid_url";
      return false;
    }

    out.secure = parts.nScheme != INTERNET_SCHEME_HTTP;
    out.port = parts.nPort;
    out.host.assign(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring urlPath;
    if (parts.dwUrlPathLength > 0)
    {
      urlPath.append(parts.lpszUrlPath, parts.dwUrlPathLength);
    }
    if (parts.dwExtraInfoLength > 0)
    {
      urlPath.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    }
    if (urlPath.empty())
    {
      urlPath = L"/";
    }
    out.basePath = NormalizeCloudAccountBasePath(urlPath);
    out.accessKey = accessKey;
    return !out.host.empty();
  }

  bool SendCloudAccountRequest(const CloudAccountRequestConfig &cfg,
                               const std::wstring &path,
                               DWORD &statusCode,
                               std::wstring &responseBody,
                               std::wstring &error)
  {
    statusCode = 0;
    responseBody.clear();
    error.clear();

    HINTERNET hSession = WinHttpOpen(L"Codex Account Switch/1.0",
                                     WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession)
    {
      error = L"WinHttpOpen_failed";
      return false;
    }
    HINTERNET hConnect =
        WinHttpConnect(hSession, cfg.host.c_str(), cfg.port, 0);
    if (!hConnect)
    {
      WinHttpCloseHandle(hSession);
      error = L"WinHttpConnect_failed";
      return false;
    }
    HINTERNET hRequest =
        WinHttpOpenRequest(hConnect, L"GET", path.c_str(), nullptr,
                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                           cfg.secure ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest)
    {
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      error = L"WinHttpOpenRequest_failed";
      return false;
    }

    DWORD decompression =
        WINHTTP_DECOMPRESSION_FLAG_GZIP | WINHTTP_DECOMPRESSION_FLAG_DEFLATE;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_DECOMPRESSION, &decompression,
                     sizeof(decompression));
    WinHttpSetTimeouts(hRequest, 10000, 10000, 30000, 30000);

    const std::wstring headers =
        L"Accept: application/json\r\nX-Access-Key: " + cfg.accessKey + L"\r\n";
    BOOL ok = WinHttpSendRequest(hRequest, headers.c_str(),
                                 static_cast<DWORD>(-1L),
                                 WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok)
    {
      ok = WinHttpReceiveResponse(hRequest, nullptr);
    }
    if (!ok)
    {
      error = L"http_transport_failed";
      WinHttpCloseHandle(hRequest);
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      return false;
    }

    DWORD statusSize = sizeof(statusCode);
    if (!WinHttpQueryHeaders(hRequest,
                             WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &statusCode,
                             &statusSize, WINHTTP_NO_HEADER_INDEX))
    {
      error = L"http_status_query_failed";
      WinHttpCloseHandle(hRequest);
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      return false;
    }

    std::string body;
    for (;;)
    {
      DWORD bytesAvailable = 0;
      if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable))
      {
        error = L"http_query_data_failed";
        break;
      }
      if (bytesAvailable == 0)
      {
        break;
      }
      std::string chunk(static_cast<size_t>(bytesAvailable), '\0');
      DWORD bytesRead = 0;
      if (!WinHttpReadData(hRequest, chunk.data(), bytesAvailable, &bytesRead))
      {
        error = L"http_read_failed";
        break;
      }
      body.append(chunk.data(), chunk.data() + bytesRead);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    if (!error.empty())
    {
      return false;
    }
    responseBody = Utf8ToWide(body);
    return true;
  }

  std::wstring TrimWideCopy(const std::wstring &value)
  {
    size_t start = 0;
    while (start < value.size() && iswspace(value[start]))
    {
      ++start;
    }
    size_t end = value.size();
    while (end > start && iswspace(value[end - 1]))
    {
      --end;
    }
    return value.substr(start, end - start);
  }

  bool LooksLikeJsonText(const std::wstring &value)
  {
    const std::wstring trimmed = TrimWideCopy(value);
    if (trimmed.empty())
    {
      return false;
    }
    return trimmed.front() == L'{' || trimmed.front() == L'[';
  }

  std::wstring ExtractCloudAccountContentValue(const std::wstring &detailJson)
  {
    std::wstring objectText;
    if (ExtractJsonObjectField(detailJson, L"content", objectText))
    {
      return objectText;
    }
    std::wstring arrayText;
    if (ExtractJsonArrayField(detailJson, L"content", arrayText))
    {
      return arrayText;
    }
    return UnescapeJsonString(ExtractJsonStringField(detailJson, L"content"));
  }

  bool TryDownloadCloudAccountLinkedContent(const CloudAccountRequestConfig &cfg,
                                            const std::wstring &candidate,
                                            std::wstring &downloadedContent,
                                            std::wstring &error)
  {
    downloadedContent.clear();
    error.clear();
    const std::wstring trimmed = TrimWideCopy(candidate);
    if (trimmed.empty() || LooksLikeJsonText(trimmed))
    {
      return false;
    }

    std::wstring resolvedPath = trimmed;
    const std::wstring lower = ToLowerCopy(trimmed);
    if (lower.rfind(L"http://", 0) == 0 || lower.rfind(L"https://", 0) == 0)
    {
      URL_COMPONENTS parts{};
      parts.dwStructSize = sizeof(parts);
      parts.dwSchemeLength = static_cast<DWORD>(-1);
      parts.dwHostNameLength = static_cast<DWORD>(-1);
      parts.dwUrlPathLength = static_cast<DWORD>(-1);
      parts.dwExtraInfoLength = static_cast<DWORD>(-1);
      if (!WinHttpCrackUrl(trimmed.c_str(), 0, 0, &parts))
      {
        error = L"invalid_link_url";
        return false;
      }
      const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
      const bool secure = parts.nScheme != INTERNET_SCHEME_HTTP;
      if (!EqualsIgnoreCase(host, cfg.host) || parts.nPort != cfg.port ||
          secure != cfg.secure)
      {
        error = L"unsupported_external_link";
        return false;
      }
      resolvedPath.clear();
      if (parts.dwUrlPathLength > 0)
      {
        resolvedPath.append(parts.lpszUrlPath, parts.dwUrlPathLength);
      }
      if (parts.dwExtraInfoLength > 0)
      {
        resolvedPath.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
      }
      if (resolvedPath.empty())
      {
        resolvedPath = L"/";
      }
    }

    DWORD statusCode = 0;
    std::wstring body;
    if (!SendCloudAccountRequest(cfg, BuildCloudAccountRequestPath(cfg, resolvedPath),
                                 statusCode, body, error) ||
        statusCode != 200)
    {
      if (error.empty())
      {
        error = L"linked_download_http_" + std::to_wstring(statusCode);
      }
      return false;
    }

    downloadedContent = body;
    return !downloadedContent.empty();
  }

  std::wstring ExtractCloudAccountIdFromAuthJson(const std::wstring &json)
  {
    const AuthJsonCompatFields fields = ExtractAuthJsonCompatFields(json);
    if (!fields.accountId.empty())
    {
      return fields.accountId;
    }
    if (fields.idToken.empty())
    {
      return L"";
    }
    const std::wstring payload = ParseJwtPayload(fields.idToken);
    if (payload.empty())
    {
      return L"";
    }
    std::wstring authInfo;
    if (!ExtractJsonObjectField(payload, L"https://api.openai.com/auth", authInfo))
    {
      authInfo = payload;
    }
    std::wstring accountId = ExtractJsonField(authInfo, L"chatgpt_account_id");
    if (accountId.empty())
    {
      accountId = ExtractJsonField(payload, L"chatgpt_account_id");
    }
    return accountId;
  }

  std::wstring ExtractCloudAccountEmailFromAuthJson(const std::wstring &json)
  {
    std::wstring email = ExtractJsonField(json, L"email");
    if (!email.empty())
    {
      return ToLowerCopy(email);
    }
    const AuthJsonCompatFields fields = ExtractAuthJsonCompatFields(json);
    if (fields.idToken.empty())
    {
      return L"";
    }
    const std::wstring payload = ParseJwtPayload(fields.idToken);
    if (payload.empty())
    {
      return L"";
    }
    email = ExtractJsonField(payload, L"email");
    return email.empty() ? L"" : ToLowerCopy(email);
  }

  CloudAccountIdentity BuildCloudAccountIdentity(const std::wstring &content)
  {
    CloudAccountIdentity out;
    out.normalizedContent = content;
    NormalizeAuthJsonForCompatibility(content, out.normalizedContent);
    out.accountId = ExtractCloudAccountIdFromAuthJson(out.normalizedContent);
    out.email = ExtractCloudAccountEmailFromAuthJson(out.normalizedContent);
    return out;
  }

  bool IsDuplicateCloudAccount(const CloudAccountIdentity &target,
                               const CloudAccountIdentity &local)
  {
    if (!target.accountId.empty())
    {
      return !local.accountId.empty() &&
             EqualsIgnoreCase(local.accountId, target.accountId);
    }
    if (!target.email.empty())
    {
      return !local.email.empty() && EqualsIgnoreCase(local.email, target.email);
    }
    return !target.normalizedContent.empty() &&
           target.normalizedContent == local.normalizedContent;
  }

  bool FindDuplicateLocalCloudAccount(const std::wstring &downloadedContent,
                                      std::wstring &matchedName,
                                      std::wstring &matchedGroup)
  {
    matchedName.clear();
    matchedGroup.clear();
    const CloudAccountIdentity target = BuildCloudAccountIdentity(downloadedContent);
    const std::lock_guard<std::recursive_mutex> lock(g_IndexDataMutex);
    EnsureIndexExists();
    IndexData idx;
    if (!LoadIndex(idx))
    {
      return false;
    }
    for (const auto &row : idx.accounts)
    {
      const fs::path authPath = ResolveAuthPathFromIndex(row);
      if (!fs::exists(authPath))
      {
        continue;
      }
      std::wstring localContent;
      if (!ReadUtf8File(authPath, localContent))
      {
        continue;
      }
      CloudAccountIdentity local = BuildCloudAccountIdentity(localContent);
      if (local.email.empty() && !row.quotaEmail.empty())
      {
        local.email = ToLowerCopy(row.quotaEmail);
      }
      if (IsDuplicateCloudAccount(target, local))
      {
        matchedName = row.name;
        matchedGroup = row.group;
        return true;
      }
    }
    return false;
  }

  std::wstring BuildPreferredCloudAccountName(
      const std::wstring &providerItemName,
      const std::wstring &downloadedContent)
  {
    const CloudAccountIdentity identity = BuildCloudAccountIdentity(downloadedContent);
    if (!identity.email.empty())
    {
      return identity.email;
    }
    const std::wstring stem = fs::path(providerItemName).stem().wstring();
    if (!stem.empty())
    {
      return stem;
    }
    return L"cloud_auth";
  }

  bool ImportCloudAccountContent(const std::wstring &content,
                                 const std::wstring &preferredBaseName,
                                 const bool queryUsage,
                                 std::wstring &savedName,
                                 std::wstring &status,
                                 std::wstring &code)
  {
    savedName.clear();
    std::wstring normalizedContent = content;
    NormalizeAuthJsonForCompatibility(content, normalizedContent);
    if (!IsLikelyValidAuthJson(normalizedContent))
    {
      status = L"下载内容不是有效 auth.json";
      code = L"cloud_account_invalid_payload";
      return false;
    }

    std::vector<std::wstring> reservedNames;
    {
      const std::lock_guard<std::recursive_mutex> lock(g_IndexDataMutex);
      EnsureIndexExists();
      IndexData idx;
      LoadIndex(idx);
      reservedNames.reserve(idx.accounts.size());
      for (const auto &row : idx.accounts)
      {
        reservedNames.push_back(row.name);
      }
    }

    const std::wstring uniqueName =
        MakeUniqueImportedName(preferredBaseName, reservedNames);
    const fs::path tempPath =
        fs::temp_directory_path() /
        (L"codex_cloud_import_" + std::to_wstring(GetCurrentProcessId()) + L"_" +
         std::to_wstring(GetTickCount64()) + L".json");
    if (!WriteUtf8File(tempPath, normalizedContent))
    {
      status = L"无法写入临时文件";
      code = L"write_failed";
      return false;
    }

    const bool ok =
        ImportAuthJsonFile(tempPath.wstring(), uniqueName, status, code, queryUsage,
                           nullptr);
    std::error_code ec;
    fs::remove(tempPath, ec);
    if (ok)
    {
      savedName = uniqueName;
    }
    return ok;
  }

  CloudAccountDownloadResult ExecuteCloudAccountDownload(const HWND progressHwnd)
  {
    CloudAccountDownloadResult result;
    AppConfig cfg;
    LoadConfig(cfg);
    cfg.cloudAccountPasswordConfigured = fs::exists(GetCloudAccountSecretPath());
    const bool queryUsage = cfg.enableAutoRefreshQuota;

    CloudAccountRequestConfig requestCfg;
    std::wstring error;
    if (!BuildCloudAccountRequestConfig(cfg, requestCfg, error))
    {
      result.level = L"warning";
      result.statusCode = L"cloud_account_invalid_config";
      result.statusText = L"云账号配置不完整，请先填写下载地址和密码";
      UpdateCloudAccountStatusInConfig(result.statusText, false, &result.cfg);
      return result;
    }

    DWORD statusCode = 0;
    std::wstring indexJson;
    if (!SendCloudAccountRequest(
            requestCfg, BuildCloudAccountRequestPath(requestCfg, L"/json"),
            statusCode, indexJson, error) ||
        statusCode != 200)
    {
      result.statusCode = L"cloud_account_provider_request_failed";
      result.statusText = L"请求云账号 Provider 失败";
      UpdateCloudAccountStatusInConfig(result.statusText, true, &result.cfg);
      return result;
    }

    std::wstring itemsArray;
    if (!ExtractJsonArrayField(indexJson, L"items", itemsArray))
    {
      result.statusCode = L"cloud_account_provider_invalid_index";
      result.statusText = L"Provider 索引返回格式无效";
      UpdateCloudAccountStatusInConfig(result.statusText, true, &result.cfg);
      return result;
    }

    std::vector<CloudProviderItemMeta> providerItems;
    for (const auto &obj : ExtractTopLevelObjectsFromArray(itemsArray))
    {
      CloudProviderItemMeta item;
      item.name = UnescapeJsonString(ExtractJsonStringField(obj, L"name"));
      item.mtime = UnescapeJsonString(ExtractJsonStringField(obj, L"mtime"));
      item.path = UnescapeJsonString(ExtractJsonStringField(obj, L"path"));
      if (item.name.empty())
      {
        continue;
      }
      providerItems.push_back(item);
    }
    if (providerItems.empty())
    {
      result.level = L"warning";
      result.statusCode = L"cloud_account_provider_empty";
      result.statusText = L"Provider 当前没有可下载账号";
      UpdateCloudAccountStatusInConfig(result.statusText, true, &result.cfg);
      return result;
    }
    std::sort(providerItems.begin(), providerItems.end(),
              [](const CloudProviderItemMeta &a, const CloudProviderItemMeta &b)
              {
                if (a.mtime != b.mtime)
                {
                  return a.mtime > b.mtime;
                }
                return ToLowerCopy(a.name) < ToLowerCopy(b.name);
              });

    result.totalCount = static_cast<int>(providerItems.size());
    for (size_t i = 0; i < providerItems.size(); ++i)
    {
      const auto &providerItem = providerItems[i];
      if (progressHwnd != nullptr && IsWindow(progressHwnd))
      {
        PostAsyncWebJson(progressHwnd,
                         BuildProgressStatusJson(
                             L"cloud_account_progress",
                             static_cast<int>(i + 1), result.totalCount));
        if (queryUsage)
        {
          PostAsyncWebJson(progressHwnd,
                           BuildProgressStatusJson(
                               L"quota_refresh_progress",
                               static_cast<int>(i + 1), result.totalCount,
                               L"cloud"));
        }
      }
      std::wstring detailJson;
      const std::wstring detailPath =
          BuildCloudAccountRequestPath(requestCfg,
                                       L"/json/item?name=" +
                                           UrlEncode(providerItem.name));
      if (!SendCloudAccountRequest(requestCfg, detailPath, statusCode, detailJson,
                                   error) ||
          statusCode != 200)
      {
        ++result.failedCount;
        result.lastError = L"获取云账号失败：" + providerItem.name;
        continue;
      }

      std::wstring downloadedContent = ExtractCloudAccountContentValue(detailJson);
      std::wstring detailItemJson;
      std::wstring detailItemPath;
      if (ExtractJsonObjectField(detailJson, L"item", detailItemJson))
      {
        detailItemPath =
            UnescapeJsonString(ExtractJsonStringField(detailItemJson, L"path"));
      }
      std::wstring normalizedContent = downloadedContent;
      NormalizeAuthJsonForCompatibility(downloadedContent, normalizedContent);
      if (downloadedContent.empty() || !IsLikelyValidAuthJson(normalizedContent))
      {
        std::wstring linkedContent;
        std::wstring linkedError;
        bool linkedDownloaded = false;
        for (const std::wstring &candidate :
             {downloadedContent, detailItemPath, providerItem.path})
        {
          linkedContent.clear();
          linkedError.clear();
          if (!TryDownloadCloudAccountLinkedContent(requestCfg, candidate,
                                                    linkedContent, linkedError))
          {
            continue;
          }
          std::wstring linkedNormalized = linkedContent;
          NormalizeAuthJsonForCompatibility(linkedContent, linkedNormalized);
          if (!linkedContent.empty() && IsLikelyValidAuthJson(linkedNormalized))
          {
            downloadedContent = linkedContent;
            normalizedContent = linkedNormalized;
            linkedDownloaded = true;
            break;
          }
        }
        if (!linkedDownloaded)
        {
          ++result.failedCount;
          result.lastError = L"云端数据无效：" + providerItem.name;
          continue;
        }
      }

      std::wstring duplicateName;
      std::wstring duplicateGroup;
      if (FindDuplicateLocalCloudAccount(normalizedContent, duplicateName,
                                         duplicateGroup))
      {
        ++result.skippedCount;
        continue;
      }

      std::wstring savedName;
      std::wstring importStatus;
      std::wstring importCode;
      const std::wstring preferredName =
          BuildPreferredCloudAccountName(providerItem.name, normalizedContent);
      if (!ImportCloudAccountContent(normalizedContent, preferredName, queryUsage,
                                     savedName, importStatus, importCode))
      {
        ++result.failedCount;
        result.lastError =
            importStatus.empty() ? (L"导入失败：" + providerItem.name) : importStatus;
        continue;
      }

      ++result.successCount;
      result.accountsChanged = true;
    }

    if (result.successCount > 0)
    {
      result.ok = true;
      result.level = result.failedCount > 0 ? L"warning" : L"success";
      result.statusCode = result.failedCount > 0 ? L"cloud_account_batch_partial"
                                                 : L"cloud_account_batch_done";
    }
    else if (result.failedCount == 0 && result.skippedCount > 0)
    {
      result.ok = true;
      result.level = L"warning";
      result.statusCode = L"cloud_account_batch_done";
    }
    else
    {
      result.level = L"error";
      result.statusCode = L"cloud_account_batch_failed";
    }
    result.statusText = L"云账号导入完成：成功 " +
                        std::to_wstring(result.successCount) + L"，失败 " +
                        std::to_wstring(result.failedCount) + L"，跳过 " +
                        std::to_wstring(result.skippedCount);
    UpdateCloudAccountStatusInConfig(result.statusText, true, &result.cfg);
    return result;
  }

  fs::path ResolveLocalWebDavAuthPath(const WebDavFileEntry &entry)
  {
    std::wstring rel = entry.relativePath;
    std::replace(rel.begin(), rel.end(), L'/', L'\\');
    const fs::path primary = GetBackupsDir() / rel;
    if (fs::exists(primary))
    {
      return primary;
    }
    return GetLegacyBackupsDir() / rel;
  }

  bool UploadLocalWebDavEntry(const WebDavRequestConfig &cfg,
                              const WebDavFileEntry &entry,
                              std::wstring &error)
  {
    std::wstring content;
    WebDavFileEntry refreshed = entry;
    if (!ReadLocalAuthJsonFile(ResolveLocalWebDavAuthPath(entry), content,
                               refreshed, error))
    {
      return false;
    }
    return WebDavPutTextFile(cfg, WebDavRemoteFilePath(cfg, entry), content, error);
  }

  bool DownloadRemoteWebDavEntry(const WebDavRequestConfig &cfg,
                                 const WebDavFileEntry &entry,
                                 std::wstring &error)
  {
    std::wstring content;
    if (!WebDavDownloadTextFile(cfg, WebDavRemoteFilePath(cfg, entry), content,
                                error))
    {
      return false;
    }
    return UpsertLocalWebDavAccount(entry, content, error);
  }

  WebDavManifest BuildMergedWebDavManifest(
      const std::map<std::wstring, WebDavFileEntry> &mergedMap)
  {
    WebDavManifest merged;
    merged.generatedAt = NowText();
    for (const auto &pair : mergedMap)
    {
      merged.files.push_back(pair.second);
    }
    return merged;
  }

  bool ExecuteWebDavSyncMode(
      const std::wstring &mode,
      const std::map<std::wstring, std::wstring> &decisions,
      bool &needsConflictResolution,
      std::vector<WebDavConflictEntry> &conflicts,
      std::wstring &statusText,
      AppConfig &cfgOut,
      std::wstring &error)
  {
    needsConflictResolution = false;
    conflicts.clear();
    statusText.clear();
    error.clear();
    cfgOut = AppConfig{};

    AppConfig cfg;
    LoadConfig(cfg);
    cfg.webdavPasswordConfigured = fs::exists(GetWebDavSecretPath());
    WebDavRequestConfig requestCfg;
    if (!BuildWebDavRequestConfig(cfg, requestCfg, error))
    {
      statusText = L"WebDAV 配置不完整，请先填写地址、用户名和密码";
      return false;
    }
    if (!EnsureWebDavDirectoryTree(requestCfg, requestCfg.basePath, error))
    {
      statusText = L"无法创建或访问 WebDAV 远端目录";
      return false;
    }

    WebDavManifest localManifest;
    if (!CollectLocalWebDavManifest(localManifest, error))
    {
      statusText = L"读取本地账号备份失败";
      return false;
    }

    WebDavManifest remoteManifest;
    bool remoteManifestExists = false;
    if (!WebDavGetManifest(requestCfg, remoteManifest, remoteManifestExists,
                           error))
    {
      statusText = L"读取 WebDAV 云端清单失败";
      return false;
    }
    if (!remoteManifestExists)
    {
      remoteManifest = WebDavManifest{};
      remoteManifest.generatedAt = NowText();
    }

    if (mode == L"upload")
    {
      int uploaded = 0;
      for (const auto &entry : localManifest.files)
      {
        if (!UploadLocalWebDavEntry(requestCfg, entry, error))
        {
          statusText = L"上传账号到 WebDAV 失败";
          return false;
        }
        ++uploaded;
      }
      if (!WebDavPutTextFile(requestCfg, WebDavManifestRemotePath(requestCfg),
                             SerializeWebDavManifest(localManifest), error))
      {
        statusText = L"写入 WebDAV 清单失败";
        return false;
      }
      SaveWebDavBaseline(localManifest);
      UpdateWebDavStatusInConfig(L"手动上传完成", true, &cfgOut);
      statusText = L"WebDAV 上传完成，共同步 " + std::to_wstring(uploaded) +
                   L" 个账号";
      return true;
    }

    if (mode == L"reset_upload")
    {
      if (remoteManifestExists && !remoteManifest.files.empty())
      {
        for (const auto &entry : remoteManifest.files)
        {
          if (!WebDavDeletePath(requestCfg, WebDavRemoteFilePath(requestCfg, entry),
                                error))
          {
            statusText = L"删除 WebDAV 云端账号失败";
            return false;
          }
        }
      }
      if (!WebDavDeletePath(requestCfg, WebDavManifestRemotePath(requestCfg),
                            error))
      {
        statusText = L"删除 WebDAV 云端清单失败";
        return false;
      }
      int uploaded = 0;
      for (const auto &entry : localManifest.files)
      {
        if (!UploadLocalWebDavEntry(requestCfg, entry, error))
        {
          statusText = L"上传账号到 WebDAV 失败";
          return false;
        }
        ++uploaded;
      }
      if (!WebDavPutTextFile(requestCfg, WebDavManifestRemotePath(requestCfg),
                             SerializeWebDavManifest(localManifest), error))
      {
        statusText = L"写入 WebDAV 清单失败";
        return false;
      }
      SaveWebDavBaseline(localManifest);
      UpdateWebDavStatusInConfig(L"已清理云端并上传本地", true, &cfgOut);
      statusText =
          L"WebDAV 已清理云端并上传本地，共同步 " + std::to_wstring(uploaded) +
          L" 个账号";
      return true;
    }

    if (mode == L"download")
    {
      if (remoteManifest.files.empty())
      {
        statusText = L"WebDAV 云端暂无可下载的账号";
        UpdateWebDavStatusInConfig(statusText, true, &cfgOut);
        return true;
      }
      int downloaded = 0;
      for (const auto &entry : remoteManifest.files)
      {
        if (!DownloadRemoteWebDavEntry(requestCfg, entry, error))
        {
          statusText = L"从 WebDAV 下载账号失败";
          return false;
        }
        ++downloaded;
      }
      SaveWebDavBaseline(remoteManifest);
      UpdateWebDavStatusInConfig(L"手动下载完成", true, &cfgOut);
      statusText = L"WebDAV 下载完成，共同步 " + std::to_wstring(downloaded) +
                   L" 个账号";
      return true;
    }

    WebDavManifest baseManifest;
    LoadWebDavBaseline(baseManifest);
    const auto localMap = BuildWebDavFileMap(localManifest);
    const auto remoteMap = BuildWebDavFileMap(remoteManifest);
    const auto baseMap = BuildWebDavFileMap(baseManifest);

    std::set<std::wstring> keys;
    for (const auto &pair : localMap)
      keys.insert(pair.first);
    for (const auto &pair : remoteMap)
      keys.insert(pair.first);
    for (const auto &pair : baseMap)
      keys.insert(pair.first);

    std::map<std::wstring, WebDavFileEntry> mergedMap;
    std::vector<WebDavFileEntry> uploadPlan;
    std::vector<WebDavFileEntry> downloadPlan;

    for (const auto &key : keys)
    {
      const auto localIt = localMap.find(key);
      const auto remoteIt = remoteMap.find(key);
      const auto baseIt = baseMap.find(key);
      const bool hasLocal = localIt != localMap.end();
      const bool hasRemote = remoteIt != remoteMap.end();
      const bool hasBase = baseIt != baseMap.end();

      if (hasLocal && hasRemote)
      {
        const auto &localEntry = localIt->second;
        const auto &remoteEntry = remoteIt->second;
        if (IsSameWebDavState(localEntry, remoteEntry))
        {
          mergedMap[key] = localEntry;
          continue;
        }

        const auto decisionIt = decisions.find(key);
        if (decisionIt != decisions.end())
        {
          if (decisionIt->second == L"remote")
          {
            mergedMap[key] = remoteEntry;
            downloadPlan.push_back(remoteEntry);
          }
          else
          {
            mergedMap[key] = localEntry;
            uploadPlan.push_back(localEntry);
          }
          continue;
        }

        if (!hasBase)
        {
          conflicts.push_back({localEntry, remoteEntry});
          continue;
        }
        const bool localChanged = !IsSameWebDavState(localEntry, baseIt->second);
        const bool remoteChanged = !IsSameWebDavState(remoteEntry, baseIt->second);
        if (localChanged && !remoteChanged)
        {
          mergedMap[key] = localEntry;
          uploadPlan.push_back(localEntry);
        }
        else if (!localChanged && remoteChanged)
        {
          mergedMap[key] = remoteEntry;
          downloadPlan.push_back(remoteEntry);
        }
        else
        {
          conflicts.push_back({localEntry, remoteEntry});
        }
      }
      else if (hasLocal)
      {
        mergedMap[key] = localIt->second;
        uploadPlan.push_back(localIt->second);
      }
      else if (hasRemote)
      {
        mergedMap[key] = remoteIt->second;
        downloadPlan.push_back(remoteIt->second);
      }
    }

    if (!conflicts.empty())
    {
      needsConflictResolution = true;
      return true;
    }

    for (const auto &entry : downloadPlan)
    {
      if (!DownloadRemoteWebDavEntry(requestCfg, entry, error))
      {
        statusText = L"从 WebDAV 应用远端变更失败";
        return false;
      }
    }
    for (const auto &entry : uploadPlan)
    {
      if (!UploadLocalWebDavEntry(requestCfg, entry, error))
      {
        statusText = L"将本地变更上传到 WebDAV 失败";
        return false;
      }
    }

    const WebDavManifest mergedManifest = BuildMergedWebDavManifest(mergedMap);
    if (!WebDavPutTextFile(requestCfg, WebDavManifestRemotePath(requestCfg),
                           SerializeWebDavManifest(mergedManifest), error))
    {
      statusText = L"写入 WebDAV 清单失败";
      return false;
    }
    SaveWebDavBaseline(mergedManifest);
    UpdateWebDavStatusInConfig(L"双向同步完成", true, &cfgOut);
    statusText = L"WebDAV 双向同步完成：上传 " +
                 std::to_wstring(uploadPlan.size()) + L" 个，下载 " +
                 std::to_wstring(downloadPlan.size()) + L" 个";
    return true;
  }

  bool BackupCurrentAccount(const std::wstring &name, std::wstring &status,
                            std::wstring &code, const bool queryUsage)
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
    if (queryUsage && QueryUsageFromAuthFile(userAuth, currentUsage) &&
        !currentUsage.planType.empty())
    {
      detectedGroup = GroupFromPlanType(currentUsage.planType);
    }

    IndexData idx;
    LoadIndex(idx);
    const auto duplicated = std::find_if(
        idx.accounts.begin(), idx.accounts.end(), [&](const IndexEntry &row)
        { return EqualsIgnoreCase(row.name, safeName); });
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

    std::wstring authJson;
    if (!ReadUtf8File(userAuth, authJson))
    {
      status = L"保存失败：无法读取当前账号文件";
      code = L"write_failed";
      return false;
    }
    std::wstring normalizedAuthJson = authJson;
    NormalizeAuthJsonForCompatibility(authJson, normalizedAuthJson);
    if (!WriteUtf8File(backupDir / L"auth.json", normalizedAuthJson))
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
      row.quota5hRemainingPercent =
          RemainingPercentFromUsed(currentUsage.primaryUsedPercent);
      row.quota7dRemainingPercent =
          RemainingPercentFromUsed(currentUsage.secondaryUsedPercent);
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

  bool SwitchToAccount(const std::wstring &account, const std::wstring &group,
                       std::wstring &status, std::wstring &code)
  {
    EnsureIndexExists();
    std::wstring resolvedGroup = NormalizeGroup(group);
    const std::wstring safeName = SanitizeAccountName(account);

    IndexData idx;
    LoadIndex(idx);
    auto it = std::find_if(idx.accounts.begin(), idx.accounts.end(),
                           [&](const IndexEntry &row)
                           {
                             return NormalizeGroup(row.group) == resolvedGroup &&
                                    EqualsIgnoreCase(row.name, safeName);
                           });
    if (it == idx.accounts.end())
    {
      // Group may have changed after a quota refresh. Fall back to name-only lookup.
      it = std::find_if(idx.accounts.begin(), idx.accounts.end(),
                        [&](const IndexEntry &row)
                        {
                          return EqualsIgnoreCase(row.name, safeName);
                        });
      if (it != idx.accounts.end())
      {
        resolvedGroup = NormalizeGroup(it->group);
      }
    }

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
    std::wstring sourceAuthJson;
    if (!ReadUtf8File(source, sourceAuthJson))
    {
      status = L"切换失败：无法读取备份账号文件";
      code = L"write_failed";
      return false;
    }
    std::wstring normalizedSourceAuthJson = sourceAuthJson;
    NormalizeAuthJsonForCompatibility(sourceAuthJson, normalizedSourceAuthJson);
    if (!WriteUtf8File(userAuth, normalizedSourceAuthJson))
    {
      status = L"切换失败：无法写入当前账号文件";
      code = L"write_failed";
      return false;
    }

    idx.currentName = safeName;
    idx.currentGroup = resolvedGroup;
    if (it != idx.accounts.end())
    {
      it->updatedAt = NowText();
    }
    SaveIndex(idx);

    AppConfig cfg;
    LoadConfig(cfg);
    cfg.lastSwitchedAccount = safeName;
    cfg.lastSwitchedGroup = resolvedGroup;
    cfg.lastSwitchedAt = NowText();
    if (cfg.proxyApiKey.empty())
    {
      cfg.proxyApiKey = GenerateProxyApiKey();
    }
    SaveConfig(cfg);
    std::wstring stealthError;
    if (cfg.proxyStealthMode)
    {
      if (!ApplyStealthProxyModeToCodexProfile(cfg, stealthError))
      {
        DebugLogLine(L"proxy.stealth", L"apply on switch failed: " + stealthError);
      }
      std::wstring envError;
      if (!SyncStealthProxyEnvironment(cfg, envError))
      {
        DebugLogLine(L"proxy.stealth", L"sync env on switch failed: " + envError);
        if (stealthError.empty())
        {
          stealthError = L"env_sync_failed:" + envError;
        }
        else
        {
          stealthError += L"; env_sync_failed:" + envError;
        }
      }
    }

    if (cfg.proxyStealthMode)
    {
      status = L"切换成功（反代模式）。请重启 IDE 或 CLI 以生效。";
      if (!stealthError.empty())
      {
        status += L"；无痕换号配置失败: " + stealthError;
      }
      code = L"switch_success_proxy_mode";
      return true;
    }

    std::wstring ideDisplay;
    if (!RestartConfiguredIde(ideDisplay))
    {
      status = L"切换成功，但重启 " + ideDisplay + L" 失败，请手动重启";
      if (!stealthError.empty())
      {
        status += L"；无痕换号配置失败: " + stealthError;
      }
      code = L"restart_failed";
      return false;
    }

    status = L"切换成功，正在重启 " + ideDisplay;
    if (!stealthError.empty())
    {
      status += L"；无痕换号配置失败: " + stealthError;
    }
    code = L"switch_success";
    return true;
  }

  bool PickOpenJsonPaths(HWND hwnd, std::vector<std::wstring> &outPaths)
  {
    outPaths.clear();
    static constexpr size_t kInitialFileBufferChars = 1024 * 1024;
    static constexpr size_t kMaxFileBufferChars = 1024 * 1024;
    std::vector<wchar_t> fileName(kInitialFileBufferChars, L'\0');
    static constexpr wchar_t kJsonFilter[] =
        L"JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0\0";
    auto parseSelection = [&](const std::vector<wchar_t> &buffer)
    {
      const std::wstring first = buffer.data();
      if (first.empty())
      {
        return false;
      }

      const wchar_t *next = buffer.data() + first.size() + 1;
      if (*next == L'\0')
      {
        outPaths.push_back(first);
        return true;
      }

      const fs::path baseDir = fs::path(first);
      while (*next != L'\0')
      {
        const std::wstring file = next;
        if (!file.empty())
        {
          outPaths.push_back((baseDir / file).wstring());
        }
        next += file.size() + 1;
      }
      return !outPaths.empty();
    };

    while (true)
    {
      std::fill(fileName.begin(), fileName.end(), L'\0');

      OPENFILENAMEW ofn{};
      ofn.lStructSize = sizeof(ofn);
      ofn.hwndOwner = IsWindow(hwnd) ? hwnd : nullptr;
      ofn.lpstrFile = fileName.data();
      ofn.nMaxFile = static_cast<DWORD>(fileName.size());
      ofn.lpstrFilter = kJsonFilter;
      ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
                  OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_ALLOWMULTISELECT;
      ofn.lpstrDefExt = L"json";

      if (GetOpenFileNameW(&ofn))
      {
        return parseSelection(fileName);
      }

      const DWORD dlgErr = CommDlgExtendedError();
      if (dlgErr == FNERR_BUFFERTOOSMALL && fileName.size() < kMaxFileBufferChars)
      {
        size_t nextSize = fileName.size() * 2;
        const DWORD requiredChars = static_cast<DWORD>(fileName[0]);
        if (requiredChars > 0)
        {
          const size_t requiredSize = static_cast<size_t>(requiredChars) + 2;
          if (nextSize < requiredSize)
          {
            nextSize = requiredSize;
          }
        }
        if (nextSize > kMaxFileBufferChars)
        {
          nextSize = kMaxFileBufferChars;
        }
        fileName.assign(nextSize, L'\0');
        continue;
      }

      if (dlgErr != 0)
      {
        OutputDebugStringW((L"[PickOpenJsonPaths] CommDlgExtendedError=" +
                            std::to_wstring(dlgErr) + L"\n")
                               .c_str());
      }
      return false;
    }
  }

  bool PickOpenJsonPath(HWND hwnd, std::wstring &outPath)
  {
    std::vector<std::wstring> paths;
    if (!PickOpenJsonPaths(hwnd, paths))
    {
      return false;
    }
    outPath = paths.front();
    return true;
  }

  bool JsonContainsField(const std::wstring &json, const std::wstring &key)
  {
    if (key.empty())
    {
      return false;
    }
    return json.find(L"\"" + key + L"\"") != std::wstring::npos;
  }

  bool IsLikelyValidAuthJson(const std::wstring &json)
  {
    const AuthJsonCompatFields fields = ExtractAuthJsonCompatFields(json);
    if (fields.idToken.empty() || fields.accessToken.empty())
    {
      return false;
    }

    const std::wstring authMode = fields.authMode;
    return authMode.empty() || _wcsicmp(authMode.c_str(), L"chatgpt") == 0;
  }

  bool DeleteAccountBackup(const std::wstring &account, const std::wstring &group,
                           std::wstring &status, std::wstring &code)
  {
    EnsureIndexExists();
    const std::wstring safeGroup = NormalizeGroup(group);
    const std::wstring safeName = SanitizeAccountName(account);

    IndexData idx;
    LoadIndex(idx);
    auto it = std::find_if(idx.accounts.begin(), idx.accounts.end(),
                           [&](const IndexEntry &row)
                           {
                             return NormalizeGroup(row.group) == safeGroup &&
                                    EqualsIgnoreCase(row.name, safeName);
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
      const bool wasCurrent =
          EqualsIgnoreCase(it->name, idx.currentName) &&
          NormalizeGroup(it->group) == NormalizeGroup(idx.currentGroup);
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

  bool DeleteAccountBackupFiles(const IndexEntry &row, std::wstring &error)
  {
    error.clear();
    const std::wstring safeGroup = NormalizeGroup(row.group);
    const std::wstring safeName = SanitizeAccountName(row.name);
    fs::path dir = ResolveAuthPathFromIndex(row).parent_path();
    if (dir.empty())
    {
      dir = GetGroupDir(safeGroup) / safeName;
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
      error = L"delete_failed";
      return false;
    }
    return true;
  }

  bool LoginNewAccount(std::wstring &status, std::wstring &code)
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

  std::wstring MakeImportBaseNameFromPath(const std::wstring &jsonPath,
                                          const UsageSnapshot &usage)
  {
    std::wstring baseName;
    if (usage.ok && !usage.email.empty())
    {
      baseName = usage.email;
    }
    if (baseName.empty())
    {
      baseName = fs::path(jsonPath).stem().wstring();
    }
    if (baseName.empty())
    {
      baseName = L"imported_auth";
    }
    baseName = SanitizeAccountName(baseName);
    if (baseName.empty())
    {
      baseName = L"imported_auth";
    }
    if (baseName.size() > 32)
    {
      baseName.resize(32);
    }
    return baseName;
  }

  bool ContainsAccountNameIgnoreCase(const std::vector<std::wstring> &names,
                                     const std::wstring &candidate)
  {
    return std::any_of(names.begin(), names.end(), [&](const std::wstring &item)
                       { return EqualsIgnoreCase(item, candidate); });
  }

  std::wstring
  MakeUniqueImportedName(const std::wstring &baseName,
                         const std::vector<std::wstring> &reservedNames)
  {
    std::wstring safeBase = SanitizeAccountName(baseName);
    if (safeBase.empty())
    {
      safeBase = L"imported_auth";
    }
    if (safeBase.size() > 32)
    {
      safeBase.resize(32);
    }

    if (!ContainsAccountNameIgnoreCase(reservedNames, safeBase))
    {
      return safeBase;
    }

    for (int suffix = 2; suffix < 10000; ++suffix)
    {
      const std::wstring tail = L"#" + std::to_wstring(suffix);
      const size_t maxBaseLen = (tail.size() >= 32) ? 0 : (32 - tail.size());
      std::wstring candidate = safeBase.substr(0, maxBaseLen) + tail;
      if (!ContainsAccountNameIgnoreCase(reservedNames, candidate))
      {
        return candidate;
      }
    }

    return L"imported_auth";
  }

  bool ImportAuthJsonFile(const std::wstring &jsonPath,
                          const std::wstring &preferredName, std::wstring &status,
                          std::wstring &code, const bool queryUsage,
                          bool *outAbnormal)
  {
    if (outAbnormal != nullptr)
    {
      *outAbnormal = false;
    }
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
    std::wstring normalizedJson = json;
    NormalizeAuthJsonForCompatibility(json, normalizedJson);

    EnsureIndexExists();
    IndexData idx;
    LoadIndex(idx);

    UsageSnapshot usage;
    bool usageOk = false;
    if (queryUsage)
    {
      usageOk = QueryUsageFromAuthFile(jsonPath, usage);
    }
    std::wstring abnormalReason;
    if (queryUsage && !usageOk)
    {
      abnormalReason = DetectUsageRefreshAbnormalReason(usage);
    }
    const bool isAbnormal = !abnormalReason.empty();
    if (outAbnormal != nullptr)
    {
      *outAbnormal = isAbnormal;
    }
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
    const auto duplicated = std::find_if(
        idx.accounts.begin(), idx.accounts.end(), [&](const IndexEntry &row)
        { return EqualsIgnoreCase(row.name, accountName); });
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

    if (!WriteUtf8File(backupDir / L"auth.json", normalizedJson))
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
      row.quota5hRemainingPercent =
          RemainingPercentFromUsed(usage.primaryUsedPercent);
      row.quota7dRemainingPercent =
          RemainingPercentFromUsed(usage.secondaryUsedPercent);
      row.quota5hResetAfterSeconds = usage.primaryResetAfterSeconds;
      row.quota7dResetAfterSeconds = usage.secondaryResetAfterSeconds;
      row.quota5hResetAt = usage.primaryResetAt;
      row.quota7dResetAt = usage.secondaryResetAt;
    }
    if (!abnormalReason.empty())
    {
      row.abnormal = true;
      row.abnormalReason = abnormalReason;
      row.abnormalAt = NowText();
      row.updatedAt = row.abnormalAt;
    }
    idx.accounts.push_back(row);
    SaveIndex(idx);

    status = L"auth.json 导入成功：[" + detectedGroup + L"] " + accountName;
    code = L"auth_json_imported";
    return true;
  }

  bool BackupCurrentAccountAuto(std::wstring &savedName, std::wstring &status,
                                std::wstring &code, const bool queryUsage)
  {
    savedName.clear();
    EnsureIndexExists();

    const std::wstring userAuth = GetUserAuthPath();
    if (userAuth.empty() || !fs::exists(userAuth))
    {
      status = L"保存失败：当前账号文件不存在";
      code = L"auth_missing";
      return false;
    }

    UsageSnapshot usage;
    if (queryUsage)
    {
      QueryUsageFromAuthFile(userAuth, usage);
    }

    std::wstring baseName = usage.email;
    if (baseName.empty())
    {
      baseName = L"current_account";
    }

    IndexData idx;
    LoadIndex(idx);
    std::vector<std::wstring> reservedNames;
    reservedNames.reserve(idx.accounts.size());
    for (const auto &row : idx.accounts)
    {
      reservedNames.push_back(row.name);
    }

    const std::wstring uniqueName =
        MakeUniqueImportedName(baseName, reservedNames);
    if (!BackupCurrentAccount(uniqueName, status, code, queryUsage))
    {
      return false;
    }

    savedName = uniqueName;
    return true;
  }

  std::vector<std::wstring> ExtractTopLevelJsonObjects(const std::wstring &text)
  {
    std::vector<std::wstring> objects;
    size_t i = 0;
    while (i < text.size())
    {
      while (i < text.size() && std::iswspace(text[i]))
      {
        ++i;
      }
      if (i >= text.size())
      {
        break;
      }
      if (text[i] == L'{')
      {
        size_t start = i;
        int depth = 0;
        bool inString = false;
        bool escape = false;
        while (i < text.size())
        {
          const wchar_t ch = text[i];
          if (escape)
          {
            escape = false;
            ++i;
            continue;
          }
          if (ch == L'\\')
          {
            escape = true;
            ++i;
            continue;
          }
          if (ch == L'"')
          {
            inString = !inString;
            ++i;
            continue;
          }
          if (!inString)
          {
            if (ch == L'{')
            {
              ++depth;
            }
            else if (ch == L'}')
            {
              --depth;
              if (depth == 0)
              {
                ++i;
                objects.push_back(text.substr(start, i - start));
                break;
              }
            }
          }
          ++i;
        }
      }
      else
      {
        ++i;
      }
    }
    return objects;
  }

  bool ImportSingleTokenJson(const std::wstring &jsonContent,
                             const std::vector<std::wstring> &reservedNames,
                             std::wstring &savedName, std::wstring &status,
                             std::wstring &code, const bool queryUsage)
  {
    savedName.clear();
    std::wstring normalizedJsonContent = jsonContent;
    NormalizeAuthJsonForCompatibility(jsonContent, normalizedJsonContent);
    if (!IsLikelyValidAuthJson(normalizedJsonContent))
    {
      status = L"令牌数据无效：缺少必要字段 (id_token, access_token)";
      code = L"invalid_token_data";
      return false;
    }

    const std::wstring tempPath =
        fs::temp_directory_path() / L"codex_temp_import.json";
    if (!WriteUtf8File(tempPath, normalizedJsonContent))
    {
      status = L"无法写入临时文件";
      code = L"write_failed";
      return false;
    }

    UsageSnapshot usage;
    if (queryUsage)
    {
      QueryUsageFromAuthFile(tempPath, usage);
    }

    std::wstring baseName;
    if (usage.ok && !usage.email.empty())
    {
      baseName = usage.email;
    }
    if (baseName.empty())
    {
      const std::wstring idToken =
          ExtractCompatibleAuthField(normalizedJsonContent, L"id_token");
      if (!idToken.empty())
      {
        baseName = L"imported_oauth";
      }
      else
      {
        baseName = L"imported_auth";
      }
    }

    const std::wstring uniqueName =
        MakeUniqueImportedName(baseName, reservedNames);

    const bool ok =
        ImportAuthJsonFile(tempPath, uniqueName, status, code, queryUsage, nullptr);
    std::error_code ec;
    fs::remove(tempPath, ec);
    if (ok)
    {
      savedName = uniqueName;
    }
    return ok;
  }

  bool ImportManualTokenData(const std::wstring &tokenData,
                             const std::vector<std::wstring> &reservedNames,
                             std::wstring &savedName, std::wstring &status,
                             std::wstring &code, const bool queryUsage)
  {
    savedName.clear();
    const auto objects = ExtractTopLevelJsonObjects(tokenData);
    if (objects.empty())
    {
      status = L"未找到有效的 JSON 对象";
      code = L"no_json_found";
      return false;
    }

    if (objects.size() == 1)
    {
      std::vector<std::wstring> currentReserved = reservedNames;
      return ImportSingleTokenJson(objects[0], currentReserved, savedName, status,
                                   code, queryUsage);
    }

    std::vector<std::wstring> currentReserved = reservedNames;
    int successCount = 0;
    std::wstring lastError;
    std::wstring lastSavedName;

    for (const auto &obj : objects)
    {
      std::wstring singleStatus;
      std::wstring singleCode;
      std::wstring singleSavedName;
      if (ImportSingleTokenJson(obj, currentReserved, singleSavedName,
                                singleStatus, singleCode, queryUsage))
      {
        ++successCount;
        if (!singleSavedName.empty())
        {
          currentReserved.push_back(singleSavedName);
          lastSavedName = singleSavedName;
        }
      }
      else
      {
        lastError = singleStatus;
      }
    }

    if (successCount > 0)
    {
      savedName = lastSavedName;
      status = L"成功导入 " + std::to_wstring(successCount) + L" 个账号";
      code = L"batch_import_success";
      return true;
    }

    status = lastError.empty() ? L"批量导入失败" : lastError;
    code = L"batch_import_failed";
    return false;
  }

  constexpr wchar_t kOAuthClientId[] = L"app_EMoamEEZ73f0CkXaXp7hrann";
  constexpr wchar_t kOAuthAuthUrl[] = L"https://auth.openai.com/oauth/authorize";
  constexpr wchar_t kOAuthTokenUrl[] = L"https://auth.openai.com/oauth/token";
  constexpr int kOAuthRedirectPort = 1455;

  std::wstring Base64UrlEncode(const std::vector<unsigned char> &data)
  {
    static const wchar_t kTable[] =
        L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::wstring result;
    result.reserve(((data.size() + 2) / 3) * 4);
    size_t i = 0;
    while (i < data.size())
    {
      unsigned int n = (static_cast<unsigned int>(data[i]) << 16);
      if (i + 1 < data.size())
      {
        n |= (static_cast<unsigned int>(data[i + 1]) << 8);
      }
      if (i + 2 < data.size())
      {
        n |= static_cast<unsigned int>(data[i + 2]);
      }
      result.push_back(kTable[(n >> 18) & 0x3F]);
      result.push_back(kTable[(n >> 12) & 0x3F]);
      if (i + 1 < data.size())
      {
        result.push_back(kTable[(n >> 6) & 0x3F]);
      }
      if (i + 2 < data.size())
      {
        result.push_back(kTable[n & 0x3F]);
      }
      i += 3;
    }
    return result;
  }

  std::wstring Base64UrlEncodeNoPadding(const std::vector<unsigned char> &data)
  {
    std::wstring result = Base64UrlEncode(data);
    while (!result.empty() && result.back() == L'=')
    {
      result.pop_back();
    }
    return result;
  }

  std::vector<unsigned char> Base64UrlDecode(const std::wstring &input)
  {
    static const wchar_t kTable[] =
        L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::vector<unsigned char> result;

    std::wstring padded = input;
    while (padded.size() % 4 != 0)
    {
      padded.push_back(L'=');
    }

    for (size_t i = 0; i < padded.size(); i += 4)
    {
      unsigned int n = 0;
      for (int j = 0; j < 4; ++j)
      {
        n <<= 6;
        if (padded[i + j] == L'=')
        {
          // padding, leave as 0
        }
        else
        {
          const wchar_t *pos = std::wcschr(kTable, padded[i + j]);
          if (pos)
          {
            n |= static_cast<unsigned int>(pos - kTable);
          }
        }
      }
      result.push_back(static_cast<unsigned char>((n >> 16) & 0xFF));
      if (padded[i + 2] != L'=')
      {
        result.push_back(static_cast<unsigned char>((n >> 8) & 0xFF));
      }
      if (padded[i + 3] != L'=')
      {
        result.push_back(static_cast<unsigned char>(n & 0xFF));
      }
    }
    return result;
  }

  std::wstring Sha256Base64Url(const std::string &input)
  {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    std::vector<unsigned char> hashData(32, 0);
    DWORD hashLen = 32;

    if (!CryptAcquireContextW(&hProv, nullptr, nullptr, PROV_RSA_AES,
                              CRYPT_VERIFYCONTEXT))
    {
      return L"";
    }

    BOOL ok = CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash);
    if (ok)
    {
      ok = CryptHashData(hHash, reinterpret_cast<const BYTE *>(input.data()),
                         static_cast<DWORD>(input.size()), 0);
      if (ok)
      {
        ok = CryptGetHashParam(hHash, HP_HASHVAL, hashData.data(), &hashLen, 0);
      }
      CryptDestroyHash(hHash);
    }
    CryptReleaseContext(hProv, 0);

    if (!ok)
    {
      return L"";
    }

    return Base64UrlEncodeNoPadding(hashData);
  }

  std::wstring GenerateRandomBase64Url(size_t byteCount)
  {
    HCRYPTPROV hProv = 0;
    if (!CryptAcquireContextW(&hProv, nullptr, nullptr, PROV_RSA_AES,
                              CRYPT_VERIFYCONTEXT))
    {
      return L"";
    }

    std::vector<unsigned char> randomBytes(byteCount, 0);
    BOOL ok =
        CryptGenRandom(hProv, static_cast<DWORD>(byteCount), randomBytes.data());
    CryptReleaseContext(hProv, 0);

    if (!ok)
    {
      return L"";
    }

    return Base64UrlEncodeNoPadding(randomBytes);
  }

  std::wstring GenerateProxyApiKey()
  {
    std::wstring token = GenerateRandomBase64Url(24);
    if (token.empty())
    {
      token = GenerateRandomBase64Url(16);
    }
    if (token.empty())
    {
      token = L"local-proxy-key";
    }
    return L"sk-" + token;
  }

  std::wstring UrlEncode(const std::wstring &value)
  {
    std::wstring result;
    for (wchar_t ch : value)
    {
      if ((ch >= L'0' && ch <= L'9') || (ch >= L'A' && ch <= L'Z') ||
          (ch >= L'a' && ch <= L'z') || ch == L'-' || ch == L'_' || ch == L'.' ||
          ch == L'~')
      {
        result.push_back(ch);
      }
      else
      {
        char buf[8];
        int len = snprintf(buf, sizeof(buf), "%%%02X",
                           static_cast<unsigned char>(ch & 0xFF));
        for (int i = 0; i < len; ++i)
        {
          result.push_back(static_cast<wchar_t>(buf[i]));
        }
      }
    }
    return result;
  }

  struct OAuthCallbackResult
  {
    std::wstring authCode;
    std::wstring state;
    std::wstring error;
    bool received = false;
  };

  SOCKET g_OAuthListenSocket = INVALID_SOCKET;
  OAuthCallbackResult *g_OAuthCallbackResult = nullptr;
  HANDLE g_OAuthStopEvent = nullptr;
  std::mutex g_OAuthCallbackMutex;

  std::wstring UrlDecodeWide(const std::wstring &value)
  {
    std::string in = WideToUtf8(value);
    std::string out;
    out.reserve(in.size());
    auto hexVal = [](const char c) -> int
    {
      if (c >= '0' && c <= '9')
        return c - '0';
      if (c >= 'a' && c <= 'f')
        return 10 + (c - 'a');
      if (c >= 'A' && c <= 'F')
        return 10 + (c - 'A');
      return -1;
    };

    for (size_t i = 0; i < in.size(); ++i)
    {
      const char ch = in[i];
      if (ch == '%' && i + 2 < in.size())
      {
        const int hi = hexVal(in[i + 1]);
        const int lo = hexVal(in[i + 2]);
        if (hi >= 0 && lo >= 0)
        {
          out.push_back(static_cast<char>((hi << 4) | lo));
          i += 2;
          continue;
        }
      }
      if (ch == '+')
      {
        out.push_back(' ');
      }
      else
      {
        out.push_back(ch);
      }
    }
    return Utf8ToWide(out);
  }

  std::map<std::wstring, std::wstring>
  ParseUrlQueryPairs(const std::wstring &inputUrlOrQuery)
  {
    std::map<std::wstring, std::wstring> params;
    if (inputUrlOrQuery.empty())
    {
      return params;
    }

    std::wstring query = inputUrlOrQuery;
    const size_t qPos = query.find(L'?');
    if (qPos != std::wstring::npos)
    {
      query = query.substr(qPos + 1);
    }
    const size_t hashPos = query.find(L'#');
    if (hashPos != std::wstring::npos)
    {
      query = query.substr(0, hashPos);
    }

    size_t pos = 0;
    while (pos < query.size())
    {
      const size_t ampPos = query.find(L'&', pos);
      const std::wstring pair =
          (ampPos == std::wstring::npos) ? query.substr(pos)
                                         : query.substr(pos, ampPos - pos);
      const size_t eqPos = pair.find(L'=');
      if (eqPos != std::wstring::npos && eqPos > 0)
      {
        const std::wstring key = UrlDecodeWide(pair.substr(0, eqPos));
        const std::wstring value = UrlDecodeWide(pair.substr(eqPos + 1));
        if (!key.empty())
        {
          params[key] = value;
        }
      }
      if (ampPos == std::wstring::npos)
      {
        break;
      }
      pos = ampPos + 1;
    }

    return params;
  }

  bool ParseOAuthCallbackUrl(const std::wstring &callbackUrl,
                             std::wstring &authCode, std::wstring &state,
                             std::wstring &error)
  {
    authCode.clear();
    state.clear();
    error.clear();
    const auto params = ParseUrlQueryPairs(callbackUrl);
    if (params.empty())
    {
      return false;
    }
    const auto errIt = params.find(L"error");
    if (errIt != params.end())
    {
      error = errIt->second;
    }
    const auto codeIt = params.find(L"code");
    if (codeIt != params.end())
    {
      authCode = codeIt->second;
    }
    const auto stateIt = params.find(L"state");
    if (stateIt != params.end())
    {
      state = stateIt->second;
    }
    return !authCode.empty() || !error.empty();
  }

  void PostOAuthFlowEvent(const HWND hwnd, const std::wstring &stage,
                          const std::wstring &authUrl = L"",
                          const std::wstring &message = L"",
                          const std::wstring &code = L"")
  {
    if (hwnd == nullptr)
    {
      return;
    }
    std::wstring json = L"{\"type\":\"oauth_flow\",\"stage\":\"" +
                        EscapeJsonString(stage) + L"\",\"authUrl\":\"" +
                        EscapeJsonString(authUrl) + L"\",\"message\":\"" +
                        EscapeJsonString(message) + L"\",\"code\":\"" +
                        EscapeJsonString(code) + L"\",\"port\":" +
                        std::to_wstring(kOAuthRedirectPort) + L"}";
    PostAsyncWebJson(hwnd, json);
  }

  DWORD WINAPI OAuthCallbackThread(LPVOID param)
  {
    const int port = kOAuthRedirectPort;
    OAuthCallbackResult *result = static_cast<OAuthCallbackResult *>(param);

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET)
    {
      result->error = L"socket_create_failed";
      result->received = true;
      return 1;
    }

    int reuse = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char *>(&reuse), sizeof(reuse));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<u_short>(port));

    if (bind(listenSock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) ==
        SOCKET_ERROR)
    {
      closesocket(listenSock);
      result->error = L"bind_failed";
      result->received = true;
      return 1;
    }

    if (listen(listenSock, 1) == SOCKET_ERROR)
    {
      closesocket(listenSock);
      result->error = L"listen_failed";
      result->received = true;
      return 1;
    }

    g_OAuthListenSocket = listenSock;

    fd_set readSet;
    timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    while (!result->received && g_OAuthStopEvent &&
           WaitForSingleObject(g_OAuthStopEvent, 0) != WAIT_OBJECT_0)
    {
      FD_ZERO(&readSet);
      FD_SET(listenSock, &readSet);
      int selectResult = select(0, &readSet, nullptr, nullptr, &timeout);
      if (selectResult > 0 && FD_ISSET(listenSock, &readSet))
      {
        sockaddr_in clientAddr = {};
        int clientAddrLen = sizeof(clientAddr);
        SOCKET clientSock =
            accept(listenSock, reinterpret_cast<sockaddr *>(&clientAddr),
                   &clientAddrLen);
        if (clientSock != INVALID_SOCKET)
        {
          char requestBuf[4096] = {};
          int recvLen = recv(clientSock, requestBuf, sizeof(requestBuf) - 1, 0);
          if (recvLen > 0)
          {
            std::string request(requestBuf);
            size_t pathStart = request.find("GET ");
            if (pathStart != std::string::npos)
            {
              size_t pathEnd = request.find(" HTTP/", pathStart);
              if (pathEnd != std::string::npos)
              {
                std::string fullPath =
                    request.substr(pathStart + 4, pathEnd - pathStart - 4);
                size_t queryStart = fullPath.find('?');
                if (queryStart != std::string::npos)
                {
                  std::string query = fullPath.substr(queryStart + 1);
                  std::map<std::string, std::string> params;
                  size_t pos = 0;
                  while (pos < query.size())
                  {
                    size_t ampPos = query.find('&', pos);
                    std::string pair = (ampPos == std::string::npos)
                                           ? query.substr(pos)
                                           : query.substr(pos, ampPos - pos);
                    size_t eqPos = pair.find('=');
                    if (eqPos != std::string::npos)
                    {
                      std::string key = pair.substr(0, eqPos);
                      std::string value = pair.substr(eqPos + 1);
                      params[key] = value;
                    }
                    if (ampPos == std::string::npos)
                    {
                      break;
                    }
                    pos = ampPos + 1;
                  }

                  std::string response;
                  if (params.count("error"))
                  {
                    result->error = Utf8ToWide(params["error"]);
                    response =
                        "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html; "
                        "charset=utf-8\r\n\r\n<h1>Authorization Failed</h1>";
                  }
                  else if (params.count("code"))
                  {
                    result->authCode = Utf8ToWide(params["code"]);
                    if (params.count("state"))
                    {
                      result->state = Utf8ToWide(params["state"]);
                    }
                    result->received = true;
                    response =
                        "HTTP/1.1 200 OK\r\nContent-Type: text/html; "
                        "charset=utf-8\r\n\r\n<h1>Authorization "
                        "Successful!</h1><p>You can close this window now.</p>";
                  }
                  else
                  {
                    response =
                        "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html; "
                        "charset=utf-8\r\n\r\n<h1>Invalid Request</h1>";
                  }
                  send(clientSock, response.c_str(),
                       static_cast<int>(response.size()), 0);
                }
              }
            }
          }
          closesocket(clientSock);
        }
      }
    }

    closesocket(listenSock);
    g_OAuthListenSocket = INVALID_SOCKET;
    return 0;
  }

  bool HttpRequestPost(const std::wstring &host, const std::wstring &path,
                       const std::string &body, const std::wstring &contentType,
                       std::string &responseBody, std::wstring &error)
  {
    responseBody.clear();
    error.clear();

    HINTERNET hSession = WinHttpOpen(
        L"CodexAccountSwitch/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession)
    {
      error = L"WinHttpOpen_failed";
      return false;
    }

    HINTERNET hConnect =
        WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect)
    {
      WinHttpCloseHandle(hSession);
      error = L"WinHttpConnect_failed";
      return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, L"POST", path.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest)
    {
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      error = L"WinHttpOpenRequest_failed";
      return false;
    }

    DWORD decompression =
        WINHTTP_DECOMPRESSION_FLAG_GZIP | WINHTTP_DECOMPRESSION_FLAG_DEFLATE;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_DECOMPRESSION, &decompression,
                     sizeof(decompression));

    std::wstring headers =
        L"Content-Type: " + contentType + L"\r\nAccept: application/json\r\n";

    BOOL ok = WinHttpSendRequest(
        hRequest, headers.c_str(), static_cast<DWORD>(-1L),
        const_cast<char *>(body.data()), static_cast<DWORD>(body.size()),
        static_cast<DWORD>(body.size()), 0);
    if (ok)
    {
      ok = WinHttpReceiveResponse(hRequest, nullptr);
    }

    if (ok)
    {
      DWORD statusCode = 0;
      DWORD statusCodeSize = sizeof(statusCode);
      WinHttpQueryHeaders(hRequest,
                          WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                          WINHTTP_HEADER_NAME_BY_INDEX, &statusCode,
                          &statusCodeSize, WINHTTP_NO_HEADER_INDEX);

      DWORD bytesAvailable = 0;
      DWORD bytesRead = 0;
      do
      {
        bytesAvailable = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable))
        {
          break;
        }
        if (bytesAvailable == 0)
        {
          break;
        }
        std::vector<char> buffer(bytesAvailable + 1);
        bytesRead = 0;
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable,
                            &bytesRead))
        {
          responseBody.append(buffer.data(), bytesRead);
        }
      } while (bytesAvailable > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (!ok)
    {
      error = L"http_request_failed";
      return false;
    }

    return true;
  }

  std::wstring ParseJwtPayload(const std::wstring &token)
  {
    size_t firstDot = token.find(L'.');
    size_t secondDot = token.find(L'.', firstDot + 1);
    if (firstDot == std::wstring::npos || secondDot == std::wstring::npos)
    {
      return L"";
    }

    std::wstring payload = token.substr(firstDot + 1, secondDot - firstDot - 1);
    std::vector<unsigned char> decoded = Base64UrlDecode(payload);
    if (decoded.empty())
    {
      return L"";
    }

    return Utf8ToWide(std::string(decoded.begin(), decoded.end()));
  }

  bool StartOAuthLogin(const HWND notifyHwnd, std::wstring &savedName,
                       std::wstring &status,
                       std::wstring &code)
  {
    savedName.clear();
    status.clear();
    code.clear();

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
      status = L"初始化网络失败";
      code = L"wsa_startup_failed";
      return false;
    }

    std::wstring codeVerifier = GenerateRandomBase64Url(96);
    if (codeVerifier.empty())
    {
      WSACleanup();
      status = L"生成随机码失败";
      code = L"random_gen_failed";
      return false;
    }

    std::string verifierUtf8 = WideToUtf8(codeVerifier);
    std::wstring codeChallenge = Sha256Base64Url(verifierUtf8);
    if (codeChallenge.empty())
    {
      WSACleanup();
      status = L"生成挑战码失败";
      code = L"challenge_gen_failed";
      return false;
    }

    std::wstring state = GenerateRandomBase64Url(32);
    if (state.empty())
    {
      WSACleanup();
      status = L"生成状态码失败";
      code = L"state_gen_failed";
      return false;
    }

    std::wstring redirectUri = L"http://localhost:" +
                               std::to_wstring(kOAuthRedirectPort) +
                               L"/auth/callback";

    std::wstring authUrl =
        std::wstring(kOAuthAuthUrl) + L"?client_id=" + UrlEncode(kOAuthClientId) +
        L"&response_type=code" + L"&redirect_uri=" + UrlEncode(redirectUri) +
        L"&scope=" + UrlEncode(L"openid email profile offline_access") +
        L"&state=" + UrlEncode(state) + L"&code_challenge=" +
        UrlEncode(codeChallenge) + L"&code_challenge_method=S256" +
        L"&prompt=login" + L"&id_token_add_organizations=true" +
        L"&codex_cli_simplified_flow=true";

    PostOAuthFlowEvent(notifyHwnd, L"listening", authUrl, L"");

    OAuthCallbackResult callbackResult;
    {
      std::lock_guard<std::mutex> lock(g_OAuthCallbackMutex);
      g_OAuthCallbackResult = &callbackResult;
      g_OAuthStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    }

    DWORD threadId = 0;
    HANDLE hThread = CreateThread(nullptr, 0, OAuthCallbackThread,
                                  &callbackResult, 0, &threadId);
    if (!hThread)
    {
      {
        std::lock_guard<std::mutex> lock(g_OAuthCallbackMutex);
        CloseHandle(g_OAuthStopEvent);
        g_OAuthStopEvent = nullptr;
        g_OAuthCallbackResult = nullptr;
      }
      WSACleanup();
      status = L"创建监听线程失败";
      code = L"thread_create_failed";
      PostOAuthFlowEvent(notifyHwnd, L"error", authUrl, status, code);
      return false;
    }

    if (!OpenExternalUrlByExplorer(authUrl))
    {
      SetEvent(g_OAuthStopEvent);
      WaitForSingleObject(hThread, 3000);
      CloseHandle(hThread);
      {
        std::lock_guard<std::mutex> lock(g_OAuthCallbackMutex);
        CloseHandle(g_OAuthStopEvent);
        g_OAuthStopEvent = nullptr;
        g_OAuthCallbackResult = nullptr;
      }
      WSACleanup();
      status = L"打开浏览器失败";
      code = L"browser_open_failed";
      PostOAuthFlowEvent(notifyHwnd, L"browser_open_failed", authUrl, status,
                         code);
      return false;
    }

    PostOAuthFlowEvent(notifyHwnd, L"browser_opened", authUrl, L"");

    DWORD waitStart = GetTickCount();
    const DWORD timeoutMs = 120000;
    while (!callbackResult.received && (GetTickCount() - waitStart) < timeoutMs)
    {
      Sleep(100);
    }

    SetEvent(g_OAuthStopEvent);
    WaitForSingleObject(hThread, 3000);
    CloseHandle(hThread);
    {
      std::lock_guard<std::mutex> lock(g_OAuthCallbackMutex);
      CloseHandle(g_OAuthStopEvent);
      g_OAuthStopEvent = nullptr;
      g_OAuthCallbackResult = nullptr;
    }

    if (!callbackResult.received)
    {
      WSACleanup();
      status = L"OAuth 授权超时";
      code = L"oauth_timeout";
      PostOAuthFlowEvent(notifyHwnd, L"timeout", authUrl, status, code);
      return false;
    }

    if (!callbackResult.error.empty())
    {
      WSACleanup();
      status = L"OAuth 授权失败: " + callbackResult.error;
      code = L"oauth_error";
      PostOAuthFlowEvent(notifyHwnd, L"error", authUrl, status, code);
      return false;
    }

    if (callbackResult.authCode.empty())
    {
      WSACleanup();
      status = L"未收到授权码";
      code = L"no_auth_code";
      PostOAuthFlowEvent(notifyHwnd, L"error", authUrl, status, code);
      return false;
    }

    PostOAuthFlowEvent(notifyHwnd, L"callback_received", authUrl, L"");

    std::string postBody = "grant_type=authorization_code"
                           "&client_id=" +
                           WideToUtf8(kOAuthClientId) +
                           "&code=" + WideToUtf8(callbackResult.authCode) +
                           "&redirect_uri=" + WideToUtf8(redirectUri) +
                           "&code_verifier=" + verifierUtf8;

    std::string tokenResponseBody;
    std::wstring tokenError;
    if (!HttpRequestPost(L"auth.openai.com", L"/oauth/token", postBody,
                         L"application/x-www-form-urlencoded", tokenResponseBody,
                         tokenError))
    {
      WSACleanup();
      status = L"Token 请求失败: " + tokenError;
      code = L"token_request_failed";
      PostOAuthFlowEvent(notifyHwnd, L"error", authUrl, status, code);
      return false;
    }

    std::wstring tokenJson = Utf8ToWide(tokenResponseBody);
    std::wstring accessToken = ExtractJsonField(tokenJson, L"access_token");
    std::wstring refreshToken = ExtractJsonField(tokenJson, L"refresh_token");
    std::wstring idToken = ExtractJsonField(tokenJson, L"id_token");
    int expiresIn = 3600;
    {
      std::wstring expiresInStr = ExtractJsonField(tokenJson, L"expires_in");
      if (!expiresInStr.empty())
      {
        try
        {
          expiresIn = std::stoi(expiresInStr);
        }
        catch (...)
        {
          expiresIn = 3600;
        }
      }
    }

    if (accessToken.empty() || idToken.empty())
    {
      WSACleanup();
      status = L"Token 响应无效";
      code = L"invalid_token_response";
      PostOAuthFlowEvent(notifyHwnd, L"error", authUrl, status, code);
      return false;
    }

    std::wstring idTokenPayload = ParseJwtPayload(idToken);
    std::wstring email;
    std::wstring accountId;
    std::wstring planType;

    if (!idTokenPayload.empty())
    {
      email = ExtractJsonField(idTokenPayload, L"email");
      std::wstring authInfo;
      if (!ExtractJsonObjectField(idTokenPayload, L"https://api.openai.com/auth",
                                  authInfo))
      {
        authInfo = idTokenPayload;
      }
      accountId = ExtractJsonField(authInfo, L"chatgpt_account_id");
      planType = ExtractJsonField(authInfo, L"chatgpt_plan_type");
      if (planType.empty())
      {
        planType = ExtractJsonField(idTokenPayload, L"chatgpt_plan_type");
      }
    }

    if (email.empty())
    {
      email = L"unknown";
    }

    std::wstring baseName = email;
    std::wstring planSuffix;
    if (!planType.empty())
    {
      std::wstring planLower = ToLowerCopy(planType);
      if (planLower.find(L"plus") != std::wstring::npos)
      {
        planSuffix = L"-plus";
      }
      else if (planLower.find(L"team") != std::wstring::npos)
      {
        planSuffix = L"-team";
      }
      else if (planLower.find(L"pro") != std::wstring::npos)
      {
        planSuffix = L"-pro";
      }
    }

    EnsureIndexExists();
    IndexData idx;
    LoadIndex(idx);
    std::vector<std::wstring> reservedNames;
    reservedNames.reserve(idx.accounts.size() + 1);
    for (const auto &row : idx.accounts)
    {
      reservedNames.push_back(row.name);
    }

    std::wstring uniqueName =
        MakeUniqueImportedName(baseName + planSuffix, reservedNames);

    time_t now = time(nullptr);
    struct tm tmNow;
    gmtime_s(&tmNow, &now);
    wchar_t timeBuf[64];
    wcsftime(timeBuf, sizeof(timeBuf) / sizeof(wchar_t), L"%Y-%m-%dT%H:%M:%SZ",
             &tmNow);

    time_t expiredTime = now + expiresIn;
    struct tm tmExpired;
    gmtime_s(&tmExpired, &expiredTime);
    wchar_t expiredBuf[64];
    wcsftime(expiredBuf, sizeof(expiredBuf) / sizeof(wchar_t),
             L"%Y-%m-%dT%H:%M:%SZ", &tmExpired);

    std::wstring authJson = L"{\n";
    authJson += L"  \"auth_mode\": \"chatgpt\",\n";
    authJson += L"  \"OPENAI_API_KEY\": null,\n";
    authJson += L"  \"tokens\": {\n";
    authJson += L"    \"id_token\": \"" + EscapeJsonString(idToken) + L"\",\n";
    authJson +=
        L"    \"access_token\": \"" + EscapeJsonString(accessToken) + L"\",\n";
    authJson +=
        L"    \"refresh_token\": \"" + EscapeJsonString(refreshToken) + L"\",\n";
    authJson +=
        L"    \"account_id\": \"" + EscapeJsonString(accountId) + L"\"\n";
    authJson += L"  },\n";
    authJson += L"  \"id_token\": \"" + EscapeJsonString(idToken) + L"\",\n";
    authJson +=
        L"  \"access_token\": \"" + EscapeJsonString(accessToken) + L"\",\n";
    authJson +=
        L"  \"refresh_token\": \"" + EscapeJsonString(refreshToken) + L"\",\n";
    authJson += L"  \"account_id\": \"" + EscapeJsonString(accountId) + L"\",\n";
    authJson += L"  \"last_refresh\": \"" + std::wstring(timeBuf) + L"\",\n";
    authJson += L"  \"email\": \"" + EscapeJsonString(email) + L"\",\n";
    authJson += L"  \"type\": \"codex\",\n";
    authJson += L"  \"expired\": \"" + std::wstring(expiredBuf) + L"\"\n";
    authJson += L"}";

    std::wstring detectedGroup = L"personal";
    if (!planType.empty())
    {
      detectedGroup = GroupFromPlanType(planType);
    }

    const fs::path backupDir = GetGroupDir(detectedGroup) / uniqueName;
    std::error_code ec;
    fs::create_directories(backupDir, ec);
    if (ec)
    {
      WSACleanup();
      status = L"创建备份目录失败";
      code = L"create_dir_failed";
      PostOAuthFlowEvent(notifyHwnd, L"error", authUrl, status, code);
      return false;
    }

    if (!WriteUtf8File(backupDir / L"auth.json", authJson))
    {
      WSACleanup();
      status = L"写入认证文件失败";
      code = L"write_failed";
      PostOAuthFlowEvent(notifyHwnd, L"error", authUrl, status, code);
      return false;
    }

    IndexEntry row;
    row.name = uniqueName;
    row.group = detectedGroup;
    row.path = MakeRelativeAuthPath(detectedGroup, uniqueName);
    row.updatedAt = NowText();
    row.quotaUsageOk = false;
    row.quotaPlanType = planType;
    row.quotaEmail = email;
    idx.accounts.push_back(row);
    SaveIndex(idx);

    WSACleanup();

    savedName = uniqueName;
    status = L"OAuth 授权成功: " + uniqueName;
    code = L"oauth_success";
    PostOAuthFlowEvent(notifyHwnd, L"completed", authUrl, status, code);
    return true;
  }

  bool RenameAccount(const std::wstring &account, const std::wstring &group,
                     const std::wstring &newName, std::wstring &status,
                     std::wstring &code)
  {
    EnsureIndexExists();
    const std::wstring safeGroup = NormalizeGroup(group);
    const std::wstring safeName = SanitizeAccountName(account);
    const std::wstring safeNewName = SanitizeAccountName(newName);

    if (safeNewName.empty())
    {
      status = L"重命名失败：账号名无效";
      code = L"invalid_name";
      return false;
    }
    if (safeNewName.size() > 32)
    {
      status = L"重命名失败：账号名最多 32 个字符";
      code = L"name_too_long";
      return false;
    }

    IndexData idx;
    LoadIndex(idx);

    auto it = std::find_if(idx.accounts.begin(), idx.accounts.end(),
                           [&](const IndexEntry &row)
                           {
                             return NormalizeGroup(row.group) == safeGroup &&
                                    EqualsIgnoreCase(row.name, safeName);
                           });
    if (it == idx.accounts.end())
    {
      status = L"重命名失败：未找到目标账号";
      code = L"not_found";
      return false;
    }

    const auto duplicated = std::find_if(
        idx.accounts.begin(), idx.accounts.end(), [&](const IndexEntry &row)
        { return &row != &(*it) && EqualsIgnoreCase(row.name, safeNewName); });
    if (duplicated != idx.accounts.end())
    {
      status = L"重命名失败：名字重复，请修改后再试";
      code = L"duplicate_name";
      return false;
    }

    const std::wstring normalizedGroup = NormalizeGroup(it->group);
    const fs::path targetDir = GetGroupDir(normalizedGroup) / safeNewName;
    std::error_code ec;

    fs::path sourceDir = ResolveAuthPathFromIndex(*it).parent_path();
    if (sourceDir.empty() || !fs::exists(sourceDir))
    {
      sourceDir = GetGroupDir(normalizedGroup) / safeName;
    }

    if (!EqualsIgnoreCase(safeName, safeNewName))
    {
      if (fs::exists(targetDir))
      {
        status = L"重命名失败：名字重复，请修改后再试";
        code = L"duplicate_name";
        return false;
      }

      if (!sourceDir.empty() && fs::exists(sourceDir))
      {
        fs::create_directories(targetDir.parent_path(), ec);
        if (ec)
        {
          status = L"重命名失败：无法创建目标目录";
          code = L"create_dir_failed";
          return false;
        }

        ec.clear();
        fs::rename(sourceDir, targetDir, ec);
        if (ec)
        {
          status = L"重命名失败：无法更新账号目录";
          code = L"write_failed";
          return false;
        }
      }
    }

    const bool wasCurrent = EqualsIgnoreCase(idx.currentName, it->name) &&
                            NormalizeGroup(idx.currentGroup) == normalizedGroup;
    it->name = safeNewName;
    it->path = MakeRelativeAuthPath(normalizedGroup, safeNewName);
    it->updatedAt = NowText();
    if (wasCurrent)
    {
      idx.currentName = safeNewName;
      idx.currentGroup = normalizedGroup;
    }
    SaveIndex(idx);

    status = L"重命名成功：[" + normalizedGroup + L"] " + safeName + L" -> " +
             safeNewName;
    code = L"account_renamed";
    return true;
  }

  bool ExportAccountsZip(HWND hwnd, std::wstring &status, std::wstring &code)
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
    const std::wstring command =
        L"Compress-Archive -Path " + QuotePowerShellLiteral(sourcePattern) +
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

  bool ImportAccountsZip(HWND hwnd, std::wstring &status, std::wstring &code)
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

    const std::wstring command =
        L"Expand-Archive -LiteralPath " + QuotePowerShellLiteral(zipPath) +
        L" -DestinationPath " + QuotePowerShellLiteral(backupsDir.wstring()) +
        L" -Force";

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

  std::wstring EscapeJsonString(const std::wstring &input)
  {
    std::wstring out;
    out.reserve(input.size() + 8);
    for (const wchar_t ch : input)
    {
      switch (ch)
      {
      case L'\\':
        out += L"\\\\";
        break;
      case L'"':
        out += L"\\\"";
        break;
      case L'\n':
        out += L"\\n";
        break;
      case L'\r':
        out += L"\\r";
        break;
      case L'\t':
        out += L"\\t";
        break;
      default:
        out.push_back(ch);
        break;
      }
    }
    return out;
  }

  std::wstring UnescapeJsonString(const std::wstring &input)
  {
    auto hexValue = [](const wchar_t ch) -> int
    {
      if (ch >= L'0' && ch <= L'9')
        return static_cast<int>(ch - L'0');
      if (ch >= L'a' && ch <= L'f')
        return 10 + static_cast<int>(ch - L'a');
      if (ch >= L'A' && ch <= L'F')
        return 10 + static_cast<int>(ch - L'A');
      return -1;
    };

    auto readHex4 = [&](size_t pos, unsigned int &outCode) -> bool
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
        case L'\\':
          out.push_back(L'\\');
          break;
        case L'"':
          out.push_back(L'"');
          break;
        case L'/':
          out.push_back(L'/');
          break;
        case L'b':
          out.push_back(L'\b');
          break;
        case L'f':
          out.push_back(L'\f');
          break;
        case L'n':
          out.push_back(L'\n');
          break;
        case L'r':
          out.push_back(L'\r');
          break;
        case L't':
          out.push_back(L'\t');
          break;
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
          if (code >= 0xD800 && code <= 0xDBFF && (i + 6) < input.size() &&
              input[i + 1] == L'\\' && input[i + 2] == L'u')
          {
            unsigned int low = 0;
            if (readHex4(i + 3, low) && low >= 0xDC00 && low <= 0xDFFF)
            {
              const unsigned int codePoint =
                  0x10000 + (((code - 0xD800) << 10) | (low - 0xDC00));
              const wchar_t highSur =
                  static_cast<wchar_t>(0xD800 + ((codePoint - 0x10000) >> 10));
              const wchar_t lowSur =
                  static_cast<wchar_t>(0xDC00 + ((codePoint - 0x10000) & 0x3FF));
              out.push_back(highSur);
              out.push_back(lowSur);
              i += 6;
              break;
            }
          }

          out.push_back(static_cast<wchar_t>(code));
          break;
        }
        default:
          out.push_back(next);
          break;
        }
      }
      else
      {
        out.push_back(ch);
      }
    }

    return out;
  }

  std::wstring FormatFileTime(const fs::path &path)
  {
    std::error_code ec;
    const auto writeTime = fs::last_write_time(path, ec);
    if (ec)
    {
      return L"-";
    }

    const auto systemNow = std::chrono::system_clock::now();
    const auto fileNow = fs::file_time_type::clock::now();
    const auto sctp =
        std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            writeTime - fileNow + systemNow);
    const std::time_t tt = std::chrono::system_clock::to_time_t(sctp);

    std::tm tmLocal{};
    localtime_s(&tmLocal, &tt);

    wchar_t buf[64]{};
    wcsftime(buf, _countof(buf), L"%Y/%m/%d %H:%M", &tmLocal);
    return buf;
  }

  bool FilesEqual(const fs::path &left, const fs::path &right)
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
    bool abnormal = false;
    std::wstring abnormalReason;
    std::wstring abnormalAt;
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

  void ClearIndexEntryQuotaState(IndexEntry &row)
  {
    row.quotaUsageOk = false;
    row.quotaPlanType.clear();
    row.quotaEmail.clear();
    row.quota5hRemainingPercent = -1;
    row.quota7dRemainingPercent = -1;
    row.quota5hResetAfterSeconds = -1;
    row.quota7dResetAfterSeconds = -1;
    row.quota5hResetAt = -1;
    row.quota7dResetAt = -1;
  }

  void ClearAccountEntryQuotaState(AccountEntry &item)
  {
    item.usageOk = false;
    item.planType.clear();
    item.email.clear();
    item.quota5hRemainingPercent = -1;
    item.quota7dRemainingPercent = -1;
    item.quota5hResetAfterSeconds = -1;
    item.quota7dResetAfterSeconds = -1;
    item.quota5hResetAt = -1;
    item.quota7dResetAt = -1;
  }

  bool ApplyUsageRefreshFailureAsAbnormal(const UsageSnapshot &usage,
                                          IndexEntry &row,
                                          AccountEntry &item)
  {
    const std::wstring abnormalReason = DetectUsageRefreshAbnormalReason(usage);
    if (abnormalReason.empty())
    {
      return false;
    }

    row.abnormal = true;
    row.abnormalReason = abnormalReason;
    row.abnormalAt = NowText();
    row.updatedAt = row.abnormalAt;
    item.abnormal = true;
    item.abnormalReason = abnormalReason;
    item.abnormalAt = row.abnormalAt;
    item.updatedAt = row.updatedAt;
    return true;
  }

  std::vector<AccountEntry> CollectAccounts(const bool refreshUsage,
                                            const std::wstring &targetName,
                                            const std::wstring &targetGroup)
  {
    const std::lock_guard<std::recursive_mutex> lock(g_IndexDataMutex);
    EnsureIndexExists();

    std::vector<AccountEntry> result;
    IndexData idx;
    LoadIndex(idx);
    bool indexChanged = false;
    const std::wstring safeTargetName = SanitizeAccountName(targetName);
    const std::wstring safeTargetGroup =
        targetGroup.empty() ? L"" : NormalizeGroup(targetGroup);
    const bool hasTarget = !safeTargetName.empty();
    const bool autoDeleteAbnormal = g_AutoDeleteAbnormalAccounts;

    for (size_t i = 0; i < idx.accounts.size();)
    {
      auto &row = idx.accounts[i];
      const fs::path backupAuth = ResolveAuthPathFromIndex(row);
      if (!fs::exists(backupAuth))
      {
        ++i;
        continue;
      }

      AccountEntry item;
      item.name = row.name;
      item.group = NormalizeGroup(row.group);
      item.updatedAt =
          row.updatedAt.empty() ? FormatFileTime(backupAuth) : row.updatedAt;
      item.isCurrent =
          EqualsIgnoreCase(idx.currentName, row.name) &&
          NormalizeGroup(idx.currentGroup) == NormalizeGroup(row.group);
      item.abnormal = row.abnormal;
      item.abnormalReason = row.abnormalReason;
      item.abnormalAt = row.abnormalAt;
      item.usageOk = row.quotaUsageOk;
      item.planType = row.quotaPlanType;
      item.email = row.quotaEmail;
      item.quota5hRemainingPercent = row.quota5hRemainingPercent;
      item.quota7dRemainingPercent = row.quota7dRemainingPercent;
      item.quota5hResetAfterSeconds = row.quota5hResetAfterSeconds;
      item.quota7dResetAfterSeconds = row.quota7dResetAfterSeconds;
      item.quota5hResetAt = row.quota5hResetAt;
      item.quota7dResetAt = row.quota7dResetAt;

      const bool shouldRefresh =
          refreshUsage &&
          (!hasTarget || (EqualsIgnoreCase(row.name, safeTargetName) &&
                          (safeTargetGroup.empty() ||
                           NormalizeGroup(row.group) == safeTargetGroup)));
      if (shouldRefresh)
      {
        UsageSnapshot usage;
        if (QueryUsageFromAuthFile(backupAuth, usage))
        {
          item.usageOk = true;
          item.planType = usage.planType;
          item.email = usage.email;
          item.quota5hRemainingPercent =
              RemainingPercentFromUsed(usage.primaryUsedPercent);
          item.quota7dRemainingPercent =
              RemainingPercentFromUsed(usage.secondaryUsedPercent);
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
          if (row.abnormal)
          {
            row.abnormal = false;
            row.abnormalReason.clear();
            row.abnormalAt.clear();
            item.abnormal = false;
            item.abnormalReason.clear();
            item.abnormalAt.clear();
          }
          indexChanged = true;

          const std::wstring refreshedGroup = GroupFromPlanType(usage.planType);
          const std::wstring expectedPath =
              MakeRelativeAuthPath(refreshedGroup, SanitizeAccountName(row.name));
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
          if (item.isCurrent && IsPlanGroup(row.group) &&
              NormalizeGroup(idx.currentGroup) != NormalizeGroup(row.group))
          {
            idx.currentGroup = NormalizeGroup(row.group);
            indexChanged = true;
          }
        }
        else
        {
          item.usageError = usage.error;
          if (ApplyUsageRefreshFailureAsAbnormal(usage, row, item))
          {
            indexChanged = true;
          }
        }
      }

      if (autoDeleteAbnormal && row.abnormal)
      {
        std::wstring deleteError;
        if (DeleteAccountBackupFiles(row, deleteError))
        {
          const bool wasCurrent = item.isCurrent;
          idx.accounts.erase(
              idx.accounts.begin() +
              static_cast<std::vector<IndexEntry>::difference_type>(i));
          if (wasCurrent)
          {
            idx.currentName.clear();
            idx.currentGroup.clear();
          }
          indexChanged = true;
          if (g_DebugWebHwnd != nullptr && IsWindow(g_DebugWebHwnd))
          {
            SendWebStatusThreadSafe(
                g_DebugWebHwnd,
                L"",
                L"warning",
                L"account_abnormal_auto_deleted");
          }
          continue;
        }
      }

      item.group = NormalizeGroup(row.group);
      result.push_back(item);
      ++i;
    }

    if (indexChanged)
    {
      SaveIndex(idx);
    }

    return result;
  }

  std::vector<AccountEntry> CollectAccountsWithThrottle(const DWORD throttleMs)
  {
    const std::lock_guard<std::recursive_mutex> lock(g_IndexDataMutex);
    EnsureIndexExists();

    std::vector<AccountEntry> result;
    IndexData idx;
    LoadIndex(idx);
    bool indexChanged = false;

    for (size_t i = 0; i < idx.accounts.size(); ++i)
    {
      auto &row = idx.accounts[i];
      const fs::path backupAuth = ResolveAuthPathFromIndex(row);
      if (!fs::exists(backupAuth))
      {
        continue;
      }

      AccountEntry item;
      item.name = row.name;
      item.group = NormalizeGroup(row.group);
      item.updatedAt =
          row.updatedAt.empty() ? FormatFileTime(backupAuth) : row.updatedAt;
      item.isCurrent =
          EqualsIgnoreCase(idx.currentName, row.name) &&
          NormalizeGroup(idx.currentGroup) == NormalizeGroup(row.group);
      item.abnormal = row.abnormal;
      item.abnormalReason = row.abnormalReason;
      item.abnormalAt = row.abnormalAt;
      item.usageOk = row.quotaUsageOk;
      item.planType = row.quotaPlanType;
      item.email = row.quotaEmail;
      item.quota5hRemainingPercent = row.quota5hRemainingPercent;
      item.quota7dRemainingPercent = row.quota7dRemainingPercent;
      item.quota5hResetAfterSeconds = row.quota5hResetAfterSeconds;
      item.quota7dResetAfterSeconds = row.quota7dResetAfterSeconds;
      item.quota5hResetAt = row.quota5hResetAt;
      item.quota7dResetAt = row.quota7dResetAt;

      UsageSnapshot usage;
      if (QueryUsageFromAuthFile(backupAuth, usage))
      {
        item.usageOk = true;
        item.usageError.clear();
        item.planType = usage.planType;
        item.email = usage.email;
        item.quota5hRemainingPercent =
            RemainingPercentFromUsed(usage.primaryUsedPercent);
        item.quota7dRemainingPercent =
            RemainingPercentFromUsed(usage.secondaryUsedPercent);
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
        if (row.abnormal)
        {
          row.abnormal = false;
          row.abnormalReason.clear();
          row.abnormalAt.clear();
          item.abnormal = false;
          item.abnormalReason.clear();
          item.abnormalAt.clear();
        }
        indexChanged = true;

        const std::wstring refreshedGroup = GroupFromPlanType(usage.planType);
        const std::wstring expectedPath =
            MakeRelativeAuthPath(refreshedGroup, SanitizeAccountName(row.name));
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
        if (item.isCurrent && IsPlanGroup(row.group) &&
            NormalizeGroup(idx.currentGroup) != NormalizeGroup(row.group))
        {
          idx.currentGroup = NormalizeGroup(row.group);
          indexChanged = true;
        }
      }
      else
      {
        item.usageError = usage.error;
        if (ApplyUsageRefreshFailureAsAbnormal(usage, row, item))
        {
          indexChanged = true;
        }
      }

      item.group = NormalizeGroup(row.group);
      result.push_back(item);

      if (throttleMs > 0 && i + 1 < idx.accounts.size())
      {
        Sleep(ComputeThrottleDelayMs(throttleMs));
      }
    }

    if (indexChanged)
    {
      SaveIndex(idx);
    }

    return result;
  }

  const AccountEntry *
  FindCurrentAccountEntry(const std::vector<AccountEntry> &accounts)
  {
    for (const auto &item : accounts)
    {
      if (item.isCurrent)
      {
        return &item;
      }
    }
    return nullptr;
  }

  const AccountEntry *
  FindBestSwitchCandidate(const std::vector<AccountEntry> &accounts,
                          const AccountEntry *current)
  {
    const AccountEntry *best = nullptr;
    for (const auto &item : accounts)
    {
      if (!item.usageOk || item.quota5hRemainingPercent < 0)
      {
        continue;
      }
      if (current != nullptr && EqualsIgnoreCase(item.name, current->name) &&
          NormalizeGroup(item.group) == NormalizeGroup(current->group))
      {
        continue;
      }
      if (best == nullptr ||
          item.quota5hRemainingPercent > best->quota5hRemainingPercent)
      {
        best = &item;
      }
    }
    return best;
  }

  int GetLowQuotaMetricValue(const AccountEntry &item, bool &usesWeeklyWindow)
  {
    usesWeeklyWindow = false;
    const std::wstring plan = NormalizePlanType(item.planType);
    if (plan == L"free")
    {
      usesWeeklyWindow = true;
      return item.quota7dRemainingPercent;
    }
    return item.quota5hRemainingPercent;
  }

  const AccountEntry *FindBestSwitchCandidateByMetric(
      const std::vector<AccountEntry> &accounts, const AccountEntry *current,
      const bool useWeeklyWindow, int &outBestQuota)
  {
    outBestQuota = -1;
    const AccountEntry *best = nullptr;
    for (const auto &item : accounts)
    {
      if (!item.usageOk)
      {
        continue;
      }
      if (current != nullptr && EqualsIgnoreCase(item.name, current->name) &&
          NormalizeGroup(item.group) == NormalizeGroup(current->group))
      {
        continue;
      }

      bool candidateUsesWeekly = false;
      int candidateMetric = GetLowQuotaMetricValue(item, candidateUsesWeekly);
      if (useWeeklyWindow)
      {
        if (!candidateUsesWeekly || candidateMetric < 0)
        {
          continue;
        }
      }
      else
      {
        candidateMetric = item.quota5hRemainingPercent;
        if (candidateMetric < 0)
        {
          continue;
        }
      }

      if (best == nullptr || candidateMetric > outBestQuota)
      {
        best = &item;
        outBestQuota = candidateMetric;
      }
    }
    return best;
  }

  std::wstring BuildCodexApiUserAgent()
  {
    return L"codex_cli_rs/" + ReadCodexLatestVersion() +
           L" (Windows " + DetectWindowsVersionTag() + L"; " +
           DetectCpuArchTag() + L")";
  }

  std::wstring NewSessionId()
  {
    GUID guid{};
    if (CoCreateGuid(&guid) != S_OK)
    {
      return GenerateRandomBase64Url(16);
    }
    wchar_t buf[64]{};
    StringFromGUID2(guid, buf, static_cast<int>(_countof(buf)));
    std::wstring out = buf;
    if (!out.empty() && out.front() == L'{')
    {
      out.erase(out.begin());
    }
    if (!out.empty() && out.back() == L'}')
    {
      out.pop_back();
    }
    return out;
  }

  std::wstring ExtractResponseOutputText(const std::wstring &json)
  {
    std::wstring outputArray;
    if (!ExtractJsonArrayField(json, L"output", outputArray))
    {
      return L"";
    }

    const std::vector<std::wstring> outputItems =
        ExtractTopLevelObjectsFromArray(outputArray);
    for (const auto &item : outputItems)
    {
      if (ToLowerCopy(ExtractJsonField(item, L"type")) != L"message")
      {
        continue;
      }

      std::wstring contentArray;
      if (!ExtractJsonArrayField(item, L"content", contentArray))
      {
        continue;
      }

      const std::vector<std::wstring> contentItems =
          ExtractTopLevelObjectsFromArray(contentArray);
      for (const auto &part : contentItems)
      {
        const std::wstring partType = ToLowerCopy(ExtractJsonField(part, L"type"));
        if (partType == L"output_text" || partType == L"text" ||
            partType == L"input_text")
        {
          std::wstring text = UnescapeJsonString(ExtractJsonField(part, L"text"));
          if (!text.empty())
          {
            return text;
          }
        }
      }
    }

    return L"";
  }

  std::wstring ExtractSseOutputText(const std::wstring &ssePayload)
  {
    std::wstring output;
    std::wstring doneText;
    size_t pos = 0;
    while (pos < ssePayload.size())
    {
      size_t end = ssePayload.find(L'\n', pos);
      if (end == std::wstring::npos)
      {
        end = ssePayload.size();
      }
      std::wstring line = ssePayload.substr(pos, end - pos);
      if (!line.empty() && line.back() == L'\r')
      {
        line.pop_back();
      }

      const std::wstring prefix = L"data:";
      if (line.size() >= prefix.size() &&
          line.compare(0, prefix.size(), prefix) == 0)
      {
        size_t dataPos = prefix.size();
        while (dataPos < line.size() && iswspace(line[dataPos]))
        {
          ++dataPos;
        }
        const std::wstring data = line.substr(dataPos);
        if (!data.empty() && data != L"[DONE]")
        {
          const std::wstring eventType = ToLowerCopy(ExtractJsonField(data, L"type"));
          if (eventType == L"response.output_text.delta")
          {
            const std::wstring delta =
                UnescapeJsonString(ExtractJsonField(data, L"delta"));
            if (!delta.empty())
            {
              output += delta;
            }
          }
          else if (eventType == L"response.output_text.done")
          {
            doneText = UnescapeJsonString(ExtractJsonField(data, L"text"));
          }
        }
      }

      if (end == ssePayload.size())
      {
        break;
      }
      pos = end + 1;
    }
    if (output.empty() && !doneText.empty())
    {
      output = doneText;
    }
    return output;
  }

  std::mutex g_ProxyMutex;
  std::thread g_ProxyThread;
  SOCKET g_ProxyListenSocket = INVALID_SOCKET;
  std::atomic<bool> g_ProxyRunning{false};
  int g_ProxyPort = kDefaultProxyPort;
  int g_ProxyTimeoutSec = kDefaultProxyTimeoutSec;
  bool g_ProxyAllowLan = false;
  std::wstring g_ProxyApiKey;
  std::wstring g_ProxyDispatchMode = L"round_robin";
  std::wstring g_ProxyFixedAccount;
  std::wstring g_ProxyFixedGroup = L"personal";
  bool g_ProxyAutoMarkAbnormalAccounts = true;
  size_t g_ProxyRoundRobinCursor = 0;
  std::mutex g_ProxyLowQuotaHintMutex;
  std::chrono::steady_clock::time_point g_ProxyLowQuotaHintAt{};
  std::wstring g_ProxyLowQuotaHintAccountKey;

  std::mutex g_ModelCacheMutex;
  std::vector<std::wstring> g_ModelIdCache;
  std::chrono::steady_clock::time_point g_ModelCacheAt{};
  constexpr int kModelCacheTtlSec = 300;

  struct TrafficLogEntry
  {
    long long calledAt = 0;
    int statusCode = 0;
    std::wstring method;
    std::wstring model;
    std::wstring protocol;
    std::wstring account;
    std::wstring path;
    int inputTokens = -1;
    int outputTokens = -1;
    int totalTokens = -1;
    long long elapsedMs = -1;
  };

  std::mutex g_TrafficLogMutex;
  std::vector<TrafficLogEntry> g_TrafficLogs;
  std::atomic<unsigned long long> g_TrafficLogVersion{0};

  struct TokenStatsCacheItem
  {
    unsigned long long sourceVersion = 0;
    long long updatedAt = 0;
    std::wstring payloadJson;
  };

  std::mutex g_TokenStatsCacheMutex;
  std::map<std::wstring, TokenStatsCacheItem> g_TokenStatsCache;
  constexpr size_t kTokenStatsCacheMaxEntries = 4096;

  fs::path GetTrafficLogPath() { return GetUserDataRoot() / L"traffic_logs.jsonl"; }
  fs::path GetTokenStatsCachePath()
  {
    return GetUserDataRoot() / L"token_stats_cache.json";
  }

  void LoadTrafficLogsFromDisk()
  {
    std::ifstream in(GetTrafficLogPath(), std::ios::binary);
    if (!in)
    {
      return;
    }

    std::vector<TrafficLogEntry> loaded;
    loaded.reserve(512);
    std::string line;
    while (std::getline(in, line))
    {
      if (line.empty())
      {
        continue;
      }
      const std::wstring json = FromUtf8(line);
      TrafficLogEntry e;
      e.calledAt = ExtractJsonInt64Field(json, L"calledAt", 0);
      e.statusCode = ExtractJsonIntField(json, L"status", 0);
      e.method = UnescapeJsonString(ExtractJsonField(json, L"method"));
      e.model = UnescapeJsonString(ExtractJsonField(json, L"model"));
      e.protocol = UnescapeJsonString(ExtractJsonField(json, L"protocol"));
      e.account = UnescapeJsonString(ExtractJsonField(json, L"account"));
      e.path = UnescapeJsonString(ExtractJsonField(json, L"path"));
      e.inputTokens = ExtractJsonIntField(json, L"inputTokens", -1);
      e.outputTokens = ExtractJsonIntField(json, L"outputTokens", -1);
      e.totalTokens = ExtractJsonIntField(json, L"totalTokens", -1);
      e.elapsedMs = ExtractJsonInt64Field(json, L"elapsedMs", -1);
      loaded.push_back(std::move(e));
    }

    if (loaded.size() > kTrafficLogMaxEntries)
    {
      const size_t keep = kTrafficLogMaxEntries;
      loaded.erase(loaded.begin(), loaded.end() - keep);
    }

    {
      std::lock_guard<std::mutex> lock(g_TrafficLogMutex);
      g_TrafficLogs = std::move(loaded);
    }
    g_TrafficLogVersion.store(
        static_cast<unsigned long long>(g_TrafficLogs.size()),
        std::memory_order_relaxed);
  }

  void LoadTokenStatsCacheFromDisk()
  {
    std::wstring json;
    if (!ReadUtf8File(GetTokenStatsCachePath(), json))
    {
      return;
    }

    std::wstring entriesArray;
    if (!ExtractJsonArrayField(json, L"entries", entriesArray))
    {
      return;
    }

    std::map<std::wstring, TokenStatsCacheItem> loaded;
    unsigned long long maxSourceVersion = 0;
    const auto objects = ExtractTopLevelObjectsFromArray(entriesArray);
    for (const auto &obj : objects)
    {
      const std::wstring key = UnescapeJsonString(ExtractJsonField(obj, L"key"));
      if (key.empty())
      {
        continue;
      }

      TokenStatsCacheItem item;
      const long long src = ExtractJsonInt64Field(obj, L"sourceVersion", 0);
      item.sourceVersion = src > 0 ? static_cast<unsigned long long>(src) : 0;
      item.updatedAt = ExtractJsonInt64Field(obj, L"updatedAt", 0);
      item.payloadJson =
          UnescapeJsonString(ExtractJsonField(obj, L"payloadText"));
      if (item.payloadJson.empty())
      {
        std::wstring payloadObj;
        if (ExtractJsonObjectField(obj, L"payload", payloadObj))
        {
          item.payloadJson = payloadObj;
        }
      }
      if (item.payloadJson.empty())
      {
        continue;
      }
      if (item.sourceVersion > maxSourceVersion)
      {
        maxSourceVersion = item.sourceVersion;
      }
      loaded[key] = std::move(item);
      if (loaded.size() >= kTokenStatsCacheMaxEntries)
      {
        break;
      }
    }

    {
      std::lock_guard<std::mutex> lock(g_TokenStatsCacheMutex);
      g_TokenStatsCache = std::move(loaded);
    }
    if (maxSourceVersion > 0)
    {
      const unsigned long long cur =
          g_TrafficLogVersion.load(std::memory_order_relaxed);
      if (maxSourceVersion > cur)
      {
        g_TrafficLogVersion.store(maxSourceVersion, std::memory_order_relaxed);
      }
    }
  }

  void InitializeTrafficPersistenceOnBoot()
  {
    static std::once_flag once;
    std::call_once(once, []()
                   {
    LoadTrafficLogsFromDisk();
    LoadTokenStatsCacheFromDisk(); });
  }

  void PersistTokenStatsCache()
  {
    std::map<std::wstring, TokenStatsCacheItem> snapshot;
    {
      std::lock_guard<std::mutex> lock(g_TokenStatsCacheMutex);
      snapshot = g_TokenStatsCache;
    }

    std::error_code ec;
    fs::create_directories(GetTokenStatsCachePath().parent_path(), ec);
    std::wstringstream ss;
    const long long nowSec =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    ss << L"{\"updatedAt\":" << nowSec << L",\"entries\":[";
    size_t i = 0;
    for (const auto &kv : snapshot)
    {
      if (i++ > 0)
      {
        ss << L",";
      }
      ss << L"{\"key\":\"" << EscapeJsonString(kv.first) << L"\""
         << L",\"sourceVersion\":" << kv.second.sourceVersion
         << L",\"updatedAt\":" << kv.second.updatedAt
         << L",\"payloadText\":\"" << EscapeJsonString(kv.second.payloadJson)
         << L"\"}";
    }
    ss << L"]}";
    WriteUtf8File(GetTokenStatsCachePath(), ss.str());
  }

  std::wstring FormatTrafficCallTime(const long long epochSec)
  {
    if (epochSec <= 0)
    {
      return L"-";
    }
    std::time_t tt = static_cast<std::time_t>(epochSec);
    std::tm tmLocal{};
    localtime_s(&tmLocal, &tt);
    wchar_t buf[64]{};
    wcsftime(buf, _countof(buf), L"%Y-%m-%d %H:%M:%S", &tmLocal);
    return buf;
  }

  void AppendTrafficLog(const TrafficLogEntry &entry)
  {
    {
      std::lock_guard<std::mutex> lock(g_TrafficLogMutex);
      g_TrafficLogs.push_back(entry);
      if (g_TrafficLogs.size() > kTrafficLogMaxEntries)
      {
        const size_t trim = g_TrafficLogs.size() - kTrafficLogMaxEntries;
        g_TrafficLogs.erase(g_TrafficLogs.begin(), g_TrafficLogs.begin() + trim);
      }
    }
    g_TrafficLogVersion.fetch_add(1, std::memory_order_relaxed);

    std::wstringstream line;
    line << L"{\"calledAt\":" << entry.calledAt << L",\"status\":" << entry.statusCode
         << L",\"method\":\"" << EscapeJsonString(entry.method) << L"\""
         << L",\"model\":\"" << EscapeJsonString(entry.model) << L"\""
         << L",\"protocol\":\"" << EscapeJsonString(entry.protocol) << L"\""
         << L",\"account\":\"" << EscapeJsonString(entry.account) << L"\""
         << L",\"path\":\"" << EscapeJsonString(entry.path) << L"\""
         << L",\"inputTokens\":" << entry.inputTokens
         << L",\"outputTokens\":" << entry.outputTokens
         << L",\"totalTokens\":" << entry.totalTokens
         << L",\"elapsedMs\":" << entry.elapsedMs << L"}\n";
    std::error_code ec;
    fs::create_directories(GetTrafficLogPath().parent_path(), ec);
    std::ofstream out(GetTrafficLogPath(), std::ios::binary | std::ios::app);
    if (!out)
    {
      return;
    }
    const std::string bytes = WideToUtf8(line.str());
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  }

  int ParseTokenField(const std::wstring &json, const std::wstring &primaryKey,
                      const std::wstring &fallbackKey = L"")
  {
    int v = ExtractJsonIntField(json, primaryKey, -1);
    if (v >= 0)
    {
      return v;
    }
    if (!fallbackKey.empty())
    {
      v = ExtractJsonIntField(json, fallbackKey, -1);
      if (v >= 0)
      {
        return v;
      }
    }
    return -1;
  }

  void ParseTokenUsage(const std::wstring &responseBody, int &inputTokens,
                       int &outputTokens, int &totalTokens)
  {
    inputTokens = ParseTokenField(responseBody, L"input_tokens", L"prompt_tokens");
    outputTokens =
        ParseTokenField(responseBody, L"output_tokens", L"completion_tokens");
    totalTokens = ParseTokenField(responseBody, L"total_tokens");
    if (totalTokens < 0 && inputTokens >= 0 && outputTokens >= 0)
    {
      totalTokens = inputTokens + outputTokens;
    }
  }

  std::wstring BuildTrafficLogsJson(const std::wstring &accountFilter, int limit)
  {
    const std::wstring normalizedFilter =
        ToLowerCopy(SanitizeAccountName(accountFilter));
    if (limit <= 0 || limit > 1000)
    {
      limit = 200;
    }

    std::vector<TrafficLogEntry> snapshot;
    {
      std::lock_guard<std::mutex> lock(g_TrafficLogMutex);
      snapshot = g_TrafficLogs;
    }

    std::wstringstream ss;
    ss << L"{\"type\":\"traffic_logs\",\"items\":[";
    int emitted = 0;
    for (auto it = snapshot.rbegin(); it != snapshot.rend() && emitted < limit;
         ++it)
    {
      if (!normalizedFilter.empty() && normalizedFilter != L"all" &&
          ToLowerCopy(it->account) != normalizedFilter)
      {
        continue;
      }
      if (emitted > 0)
      {
        ss << L",";
      }
      ss << L"{\"status\":" << it->statusCode << L",\"method\":\""
         << EscapeJsonString(it->method) << L"\",\"model\":\""
         << EscapeJsonString(it->model) << L"\",\"protocol\":\""
         << EscapeJsonString(it->protocol) << L"\",\"account\":\""
         << EscapeJsonString(it->account) << L"\",\"path\":\""
         << EscapeJsonString(it->path) << L"\",\"inputTokens\":"
         << it->inputTokens << L",\"outputTokens\":" << it->outputTokens
         << L",\"totalTokens\":" << it->totalTokens << L",\"elapsedMs\":"
         << it->elapsedMs << L",\"calledAt\":" << it->calledAt << L",\"calledAtText\":\""
         << EscapeJsonString(FormatTrafficCallTime(it->calledAt)) << L"\"}";
      ++emitted;
    }
    ss << L"]}";
    return ss.str();
  }

  std::wstring BuildTokenStatsJson(const std::wstring &accountFilter,
                                   const std::wstring &period)
  {
    const std::wstring normalizedFilter =
        ToLowerCopy(SanitizeAccountName(accountFilter));
    const std::wstring periodLower = ToLowerCopy(period);
    const std::wstring cacheKey =
        (normalizedFilter.empty() ? L"all" : normalizedFilter) + L"::" +
        (periodLower.empty() ? L"day" : periodLower);
    const unsigned long long currentLogVersion =
        g_TrafficLogVersion.load(std::memory_order_relaxed);
    {
      std::lock_guard<std::mutex> lock(g_TokenStatsCacheMutex);
      const auto it = g_TokenStatsCache.find(cacheKey);
      if (it != g_TokenStatsCache.end() &&
          it->second.sourceVersion == currentLogVersion &&
          !it->second.payloadJson.empty())
      {
        return it->second.payloadJson;
      }
    }
    long long windowSec = 24 * 3600;
    if (periodLower == L"hour")
    {
      windowSec = 3600;
    }
    else if (periodLower == L"week")
    {
      windowSec = 7 * 24 * 3600;
    }
    const long long nowSec =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    const long long minSec = nowSec - windowSec;

    std::vector<TrafficLogEntry> snapshot;
    {
      std::lock_guard<std::mutex> lock(g_TrafficLogMutex);
      snapshot = g_TrafficLogs;
    }

    long long sumInput = 0;
    long long sumOutput = 0;
    long long sumTotal = 0;
    std::map<std::wstring, long long> accountUseCount;
    std::map<std::wstring, long long> accountOutputTokens;
    std::map<std::wstring, long long> accountTotalTokens;
    std::map<std::wstring, long long> modelOutputTokens;
    std::map<std::wstring, long long> modelTotalTokens;
    std::vector<const TrafficLogEntry *> windowEntries;

    for (const auto &it : snapshot)
    {
      if (it.calledAt < minSec)
      {
        continue;
      }
      if (!normalizedFilter.empty() && normalizedFilter != L"all" &&
          ToLowerCopy(it.account) != normalizedFilter)
      {
        continue;
      }
      if (!it.account.empty())
      {
        accountUseCount[it.account]++;
      }
      windowEntries.push_back(&it);
      if (it.inputTokens >= 0)
      {
        sumInput += it.inputTokens;
      }
      if (it.outputTokens >= 0)
      {
        sumOutput += it.outputTokens;
        if (!it.account.empty())
        {
          accountOutputTokens[it.account] += it.outputTokens;
        }
        if (!it.model.empty())
        {
          modelOutputTokens[it.model] += it.outputTokens;
        }
      }
      if (it.totalTokens >= 0)
      {
        sumTotal += it.totalTokens;
        if (!it.account.empty())
        {
          accountTotalTokens[it.account] += it.totalTokens;
        }
        if (!it.model.empty())
        {
          modelTotalTokens[it.model] += it.totalTokens;
        }
      }
    }
    if (sumTotal <= 0 && sumInput >= 0 && sumOutput >= 0)
    {
      sumTotal = sumInput + sumOutput;
    }

    std::wstring activeAccount;
    long long activeCount = 0;
    for (const auto &p : accountUseCount)
    {
      if (p.second > activeCount)
      {
        activeCount = p.second;
        activeAccount = p.first;
      }
    }

    std::vector<std::pair<std::wstring, long long>> models;
    models.reserve(modelTotalTokens.size());
    for (const auto &p : modelTotalTokens)
    {
      models.push_back(p);
    }
    std::sort(models.begin(), models.end(),
              [](const auto &a, const auto &b)
              { return a.second > b.second; });

    std::vector<std::pair<std::wstring, long long>> accountTotals;
    accountTotals.reserve(accountTotalTokens.size());
    for (const auto &p : accountTotalTokens)
    {
      accountTotals.push_back(p);
    }
    std::sort(accountTotals.begin(), accountTotals.end(),
              [](const auto &a, const auto &b)
              { return a.second > b.second; });

    int bucketCount = 24;
    long long bucketSec = 3600;
    if (periodLower == L"hour")
    {
      bucketCount = 12;
      bucketSec = 300;
    }
    else if (periodLower == L"week")
    {
      bucketCount = 7;
      bucketSec = 24 * 3600;
    }
    std::vector<long long> totalTrend(static_cast<size_t>(bucketCount), 0);
    std::map<std::wstring, std::vector<long long>> modelTrend;
    std::map<std::wstring, std::vector<long long>> accountTrend;
    for (const auto *pit : windowEntries)
    {
      if (pit == nullptr)
      {
        continue;
      }
      const long long tokenValue =
          pit->totalTokens >= 0
              ? pit->totalTokens
              : ((pit->inputTokens >= 0 && pit->outputTokens >= 0)
                     ? (pit->inputTokens + pit->outputTokens)
                     : -1);
      if (tokenValue < 0)
      {
        continue;
      }
      const long long delta = pit->calledAt - minSec;
      const long long rawIndex = delta >= 0 ? (delta / bucketSec) : 0;
      const int bucketIndex = static_cast<int>(
          (std::min)(static_cast<long long>(bucketCount - 1),
                     (std::max)(0LL, rawIndex)));
      totalTrend[static_cast<size_t>(bucketIndex)] += tokenValue;
      if (!pit->model.empty())
      {
        auto &v = modelTrend[pit->model];
        if (v.empty())
        {
          v.assign(static_cast<size_t>(bucketCount), 0);
        }
        v[static_cast<size_t>(bucketIndex)] += tokenValue;
      }
      if (!pit->account.empty())
      {
        auto &v = accountTrend[pit->account];
        if (v.empty())
        {
          v.assign(static_cast<size_t>(bucketCount), 0);
        }
        v[static_cast<size_t>(bucketIndex)] += tokenValue;
      }
    }

    auto formatBucketLabel = [&](const long long sec) -> std::wstring
    {
      std::time_t tt = static_cast<std::time_t>(sec);
      std::tm tmLocal{};
      localtime_s(&tmLocal, &tt);
      wchar_t buf[32]{};
      if (periodLower == L"week")
      {
        wcsftime(buf, _countof(buf), L"%m-%d", &tmLocal);
      }
      else if (periodLower == L"hour")
      {
        wcsftime(buf, _countof(buf), L"%H:%M", &tmLocal);
      }
      else
      {
        wcsftime(buf, _countof(buf), L"%H:00", &tmLocal);
      }
      return buf;
    };

    auto emitTrendSeries = [&](std::wstringstream &out, const std::wstring &name,
                               const std::vector<long long> &vals)
    {
      long long localTotal = 0;
      for (long long v : vals)
      {
        localTotal += v;
      }
      out << L"{\"name\":\"" << EscapeJsonString(name) << L"\",\"totalTokens\":"
          << localTotal << L",\"values\":[";
      for (size_t i = 0; i < vals.size(); ++i)
      {
        if (i != 0)
        {
          out << L",";
        }
        out << vals[i];
      }
      out << L"]}";
    };

    std::wstringstream ss;
    ss << L"{\"type\":\"token_stats\",\"period\":\"" << EscapeJsonString(periodLower)
       << L"\",\"inputTokens\":" << sumInput << L",\"outputTokens\":" << sumOutput
       << L",\"totalTokens\":" << sumTotal << L",\"activeAccount\":\""
       << EscapeJsonString(activeAccount) << L"\",\"models\":[";
    for (size_t i = 0; i < models.size(); ++i)
    {
      if (i != 0)
      {
        ss << L",";
      }
      const auto &name = models[i].first;
      const long long total = models[i].second;
      const long long out = modelOutputTokens[name];
      ss << L"{\"name\":\"" << EscapeJsonString(name) << L"\",\"outputTokens\":"
         << out << L",\"totalTokens\":" << total << L"}";
    }
    ss << L"],\"accounts\":[";
    const size_t accountTopN = (std::min)(accountTotals.size(), static_cast<size_t>(20));
    for (size_t i = 0; i < accountTopN; ++i)
    {
      if (i != 0)
      {
        ss << L",";
      }
      const auto &name = accountTotals[i].first;
      ss << L"{\"name\":\"" << EscapeJsonString(name) << L"\",\"calls\":"
         << accountUseCount[name] << L",\"outputTokens\":"
         << accountOutputTokens[name] << L",\"totalTokens\":"
         << accountTotals[i].second << L"}";
    }

    const long long rangeStartSec = minSec;
    const long long rangeEndSec =
        minSec + static_cast<long long>((std::max)(0, bucketCount - 1)) * bucketSec;
    ss << L"],\"trend\":{\"bucketSec\":" << bucketSec
       << L",\"rangeStartSec\":" << rangeStartSec << L",\"rangeEndSec\":"
       << rangeEndSec << L",\"labels\":[";
    for (int i = 0; i < bucketCount; ++i)
    {
      if (i != 0)
      {
        ss << L",";
      }
      const long long bucketStart = minSec + static_cast<long long>(i) * bucketSec;
      ss << L"\"" << EscapeJsonString(formatBucketLabel(bucketStart)) << L"\"";
    }
    ss << L"],\"total\":[";
    for (int i = 0; i < bucketCount; ++i)
    {
      if (i != 0)
      {
        ss << L",";
      }
      ss << totalTrend[static_cast<size_t>(i)];
    }
    ss << L"],\"byModel\":[";
    const size_t modelTrendN = (std::min)(models.size(), static_cast<size_t>(8));
    for (size_t i = 0; i < modelTrendN; ++i)
    {
      if (i != 0)
      {
        ss << L",";
      }
      const auto itTrend = modelTrend.find(models[i].first);
      std::vector<long long> values(static_cast<size_t>(bucketCount), 0);
      if (itTrend != modelTrend.end())
      {
        values = itTrend->second;
      }
      emitTrendSeries(ss, models[i].first, values);
    }
    ss << L"],\"byAccount\":[";
    const size_t accountTrendN = (std::min)(accountTotals.size(), static_cast<size_t>(8));
    for (size_t i = 0; i < accountTrendN; ++i)
    {
      if (i != 0)
      {
        ss << L",";
      }
      const auto itTrend = accountTrend.find(accountTotals[i].first);
      std::vector<long long> values(static_cast<size_t>(bucketCount), 0);
      if (itTrend != accountTrend.end())
      {
        values = itTrend->second;
      }
      emitTrendSeries(ss, accountTotals[i].first, values);
    }
    ss << L"]}}";
    const std::wstring payload = ss.str();
    {
      std::lock_guard<std::mutex> lock(g_TokenStatsCacheMutex);
      if (g_TokenStatsCache.size() >= kTokenStatsCacheMaxEntries)
      {
        auto oldest = g_TokenStatsCache.begin();
        for (auto it = g_TokenStatsCache.begin(); it != g_TokenStatsCache.end();
             ++it)
        {
          if (it->second.updatedAt < oldest->second.updatedAt)
          {
            oldest = it;
          }
        }
        g_TokenStatsCache.erase(oldest);
      }
      TokenStatsCacheItem item;
      item.sourceVersion = currentLogVersion;
      item.updatedAt = nowSec;
      item.payloadJson = payload;
      g_TokenStatsCache[cacheKey] = std::move(item);
    }
    PersistTokenStatsCache();
    return payload;
  }

  std::string ToLowerAscii(std::string value)
  {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c)
                   {
                     return static_cast<char>(std::tolower(c));
                   });
    return value;
  }

  std::string TrimAscii(std::string value)
  {
    while (!value.empty() &&
           (value.front() == ' ' || value.front() == '\t' || value.front() == '\r' ||
            value.front() == '\n'))
    {
      value.erase(value.begin());
    }
    while (!value.empty() &&
           (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' ||
            value.back() == '\n'))
    {
      value.pop_back();
    }
    return value;
  }

  std::wstring ToLowerWide(std::wstring value)
  {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](wchar_t c)
                   { return static_cast<wchar_t>(towlower(c)); });
    return value;
  }

  bool IsLikelyModelId(const std::wstring &value)
  {
    if (value.size() < 2 || value.size() > 96)
    {
      return false;
    }
    bool hasAlpha = false;
    for (const wchar_t ch : value)
    {
      if ((ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z'))
      {
        hasAlpha = true;
      }
      const bool ok = (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') ||
                      (ch >= L'0' && ch <= L'9') || ch == L'-' || ch == L'_' ||
                      ch == L'.';
      if (!ok)
      {
        return false;
      }
    }
    return hasAlpha;
  }

  std::vector<std::wstring> BuildFallbackModelIds()
  {
    // Keep this list aligned with codexpost.py static model definitions.
    return {
        L"gpt-5.3-codex",
        L"gpt-5.2-codex",
        L"gpt-5.2",
        L"gpt-5.1-codex-max",
        L"gpt-5.1-codex-mini",
        L"gpt-5.1-codex",
        L"gpt-5.1",
        L"gpt-5-codex-mini",
        L"gpt-5-codex",
        L"gpt-5",
    };
  }

  bool FetchModelIdsFromUpstream(const fs::path &authPath,
                                 std::vector<std::wstring> &models,
                                 std::wstring &error)
  {
    models.clear();
    error.clear();

    std::wstring accessToken;
    std::wstring accountId;
    if (!ReadAuthTokenInfo(authPath, accessToken, accountId))
    {
      error = L"auth_token_missing";
      return false;
    }

    const std::vector<std::wstring> paths = {L"/backend-api/models",
                                             L"/backend-api/codex/models"};
    std::unordered_set<std::wstring> dedup;
    for (const auto &path : paths)
    {
      HINTERNET hSession = WinHttpOpen(
          BuildCodexApiUserAgent().c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
      if (hSession == nullptr)
      {
        continue;
      }
      HINTERNET hConnect =
          WinHttpConnect(hSession, kUsageHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
      if (hConnect == nullptr)
      {
        WinHttpCloseHandle(hSession);
        continue;
      }
      HINTERNET hRequest = WinHttpOpenRequest(
          hConnect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
          WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
      if (hRequest == nullptr)
      {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        continue;
      }

      const std::wstring headers =
          L"Accept: application/json\r\nAuthorization: Bearer " + accessToken +
          L"\r\nChatGPT-Account-Id: " + accountId +
          L"\r\nVersion: " + ReadCodexLatestVersion() +
          L"\r\nOpenai-Beta: responses=experimental\r\nSession_id: " +
          NewSessionId() + L"\r\nOriginator: codex_cli_rs\r\nUser-Agent: " +
          BuildCodexApiUserAgent() + L"\r\nConnection: close\r\n";

      BOOL ok = WinHttpSendRequest(hRequest, headers.c_str(),
                                   static_cast<DWORD>(-1L),
                                   WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
      if (ok)
      {
        ok = WinHttpReceiveResponse(hRequest, nullptr);
      }
      if (!ok)
      {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        continue;
      }
      DWORD statusCode = 0;
      DWORD statusSize = sizeof(statusCode);
      WinHttpQueryHeaders(hRequest,
                          WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                          WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize,
                          WINHTTP_NO_HEADER_INDEX);

      std::string payload;
      while (true)
      {
        char buf[2048]{};
        DWORD read = 0;
        if (!WinHttpReadData(hRequest, buf, sizeof(buf), &read) || read == 0)
        {
          break;
        }
        payload.append(buf, buf + read);
      }

      WinHttpCloseHandle(hRequest);
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);

      if (statusCode < 200 || statusCode >= 300 || payload.empty())
      {
        continue;
      }

      const std::wstring json = FromUtf8(payload);
      const std::wregex reId(LR"RE("id"\s*:\s*"([^"]+)")RE");
      for (std::wsregex_iterator it(json.begin(), json.end(), reId), end;
           it != end; ++it)
      {
        const std::wstring id = UnescapeJsonString((*it)[1].str());
        if (IsLikelyModelId(id) && dedup.insert(ToLowerWide(id)).second)
        {
          models.push_back(id);
        }
      }
      const std::wregex reSlug(LR"RE("slug"\s*:\s*"([^"]+)")RE");
      for (std::wsregex_iterator it(json.begin(), json.end(), reSlug), end;
           it != end; ++it)
      {
        const std::wstring id = UnescapeJsonString((*it)[1].str());
        if (IsLikelyModelId(id) && dedup.insert(ToLowerWide(id)).second)
        {
          models.push_back(id);
        }
      }
      if (!models.empty())
      {
        return true;
      }
    }

    error = L"models_fetch_failed";
    return false;
  }

  bool GetCachedModelIds(const fs::path &authPath, std::vector<std::wstring> &models,
                         std::wstring &error)
  {
    {
      std::lock_guard<std::mutex> lock(g_ModelCacheMutex);
      const auto now = std::chrono::steady_clock::now();
      if (!g_ModelIdCache.empty() &&
          std::chrono::duration_cast<std::chrono::seconds>(now - g_ModelCacheAt)
                  .count() <= kModelCacheTtlSec)
      {
        models = g_ModelIdCache;
        return true;
      }
    }

    std::vector<std::wstring> fetched;
    if (!FetchModelIdsFromUpstream(authPath, fetched, error))
    {
      models = BuildFallbackModelIds();
      {
        std::lock_guard<std::mutex> lock(g_ModelCacheMutex);
        g_ModelIdCache = models;
        g_ModelCacheAt = std::chrono::steady_clock::now();
      }
      DebugLogLine(L"api.models",
                   L"upstream fetch failed, using fallback list: " + error);
      error.clear();
      return true;
    }
    {
      std::lock_guard<std::mutex> lock(g_ModelCacheMutex);
      g_ModelIdCache = fetched;
      g_ModelCacheAt = std::chrono::steady_clock::now();
      models = g_ModelIdCache;
    }
    return true;
  }

  std::wstring BuildOpenAiModelsJson(const std::vector<std::wstring> &models)
  {
    const long long created = static_cast<long long>(time(nullptr));
    std::wstringstream ss;
    ss << L"{\"object\":\"list\",\"data\":[";
    for (size_t i = 0; i < models.size(); ++i)
    {
      if (i != 0)
      {
        ss << L",";
      }
      ss << L"{\"id\":\"" << EscapeJsonString(models[i])
         << L"\",\"object\":\"model\",\"created\":" << created
         << L",\"owned_by\":\"openai\"}";
    }
    ss << L"]}";
    return ss.str();
  }

  std::wstring ExtractOpenAiUserPrompt(const std::wstring &jsonBody)
  {
    std::wstring messagesArray;
    if (!ExtractJsonArrayField(jsonBody, L"messages", messagesArray))
    {
      return UnescapeJsonString(ExtractJsonStringField(jsonBody, L"input"));
    }
    std::wstring best;
    const auto items = ExtractTopLevelObjectsFromArray(messagesArray);
    for (const auto &msg : items)
    {
      const std::wstring role = ToLowerCopy(ExtractJsonField(msg, L"role"));
      std::wstring content =
          UnescapeJsonString(ExtractJsonStringField(msg, L"content"));
      std::wstring contentArray;
      if (ExtractJsonArrayField(msg, L"content", contentArray))
      {
        content.clear();
        const auto parts = ExtractTopLevelObjectsFromArray(contentArray);
        for (const auto &part : parts)
        {
          const std::wstring partType =
              ToLowerCopy(ExtractJsonField(part, L"type"));
          if (partType == L"text" || partType == L"input_text" ||
              partType.empty())
          {
            const std::wstring partText =
                UnescapeJsonString(ExtractJsonStringField(part, L"text"));
            if (!partText.empty())
            {
              if (!content.empty())
              {
                content += L"\n";
              }
              content += partText;
            }
          }
        }
      }
      if (role == L"user" && !content.empty())
      {
        best = content;
      }
    }
    return best;
  }

  std::wstring ExtractOpenAiLegacyPrompt(const std::wstring &jsonBody)
  {
    std::wstring prompt =
        UnescapeJsonString(ExtractJsonStringField(jsonBody, L"prompt"));
    if (!prompt.empty())
    {
      return prompt;
    }
    std::wstring input =
        UnescapeJsonString(ExtractJsonStringField(jsonBody, L"input"));
    if (!input.empty())
    {
      return input;
    }
    return ExtractOpenAiUserPrompt(jsonBody);
  }

  std::wstring EscapeJsonForSseDataLine(const std::wstring &input)
  {
    std::wstring escaped;
    escaped.reserve(input.size() + 16);
    for (wchar_t ch : input)
    {
      if (ch == L'\\')
      {
        escaped += L"\\\\";
      }
      else if (ch == L'"')
      {
        escaped += L"\\\"";
      }
      else if (ch == L'\r')
      {
        continue;
      }
      else if (ch == L'\n')
      {
        escaped += L"\\n";
      }
      else
      {
        escaped.push_back(ch);
      }
    }
    return escaped;
  }

  bool SendSocketAll(SOCKET sock, const char *data, const size_t len)
  {
    size_t sentTotal = 0;
    while (sentTotal < len)
    {
      const int sent =
          send(sock, data + sentTotal, static_cast<int>(len - sentTotal), 0);
      if (sent == SOCKET_ERROR || sent <= 0)
      {
        return false;
      }
      sentTotal += static_cast<size_t>(sent);
    }
    return true;
  }

  bool SendHttpTextResponse(SOCKET clientSock, const int statusCode,
                            const std::string &reason,
                            const std::string &body,
                            const std::string &contentType = "application/json")
  {
    std::ostringstream ss;
    ss << "HTTP/1.1 " << statusCode << " " << reason << "\r\n";
    ss << "Content-Type: " << contentType << "\r\n";
    ss << "Content-Length: " << body.size() << "\r\n";
    ss << "Connection: close\r\n\r\n";
    const std::string header = ss.str();
    return SendSocketAll(clientSock, header.data(), header.size()) &&
           SendSocketAll(clientSock, body.data(), body.size());
  }

  bool ResolveCurrentAuthPath(fs::path &authPath, std::wstring &accountName,
                              std::wstring &groupName, std::wstring &error)
  {
    authPath.clear();
    accountName.clear();
    groupName.clear();
    error.clear();

    EnsureIndexExists();
    IndexData idx;
    if (!LoadIndex(idx))
    {
      error = L"index_load_failed";
      return false;
    }
    if (idx.currentName.empty())
    {
      error = L"no_selected_account";
      return false;
    }

    const std::wstring currentGroup = NormalizeGroup(idx.currentGroup);
    for (const auto &row : idx.accounts)
    {
      if (EqualsIgnoreCase(row.name, idx.currentName) &&
          NormalizeGroup(row.group) == currentGroup)
      {
        authPath = ResolveAuthPathFromIndex(row);
        accountName = row.name;
        groupName = NormalizeGroup(row.group);
        break;
      }
    }

    if (authPath.empty() || !fs::exists(authPath))
    {
      error = L"auth_missing";
      return false;
    }
    return true;
  }

  bool AutoSelectProxyFixedAccountIfNeeded(std::wstring &selectedName,
                                           std::wstring &selectedGroup)
  {
    selectedName.clear();
    selectedGroup.clear();

    if (!g_ProxyRunning.load())
    {
      return false;
    }
    if (ToLowerCopy(g_ProxyDispatchMode) != L"fixed")
    {
      return false;
    }

    const std::lock_guard<std::recursive_mutex> lock(g_IndexDataMutex);
    EnsureIndexExists();
    IndexData idx;
    if (!LoadIndex(idx))
    {
      return false;
    }
    if (!idx.currentName.empty())
    {
      return false;
    }

    for (const auto &row : idx.accounts)
    {
      if (row.abnormal)
      {
        continue;
      }
      const fs::path candidate = ResolveAuthPathFromIndex(row);
      if (candidate.empty() || !fs::exists(candidate))
      {
        continue;
      }
      idx.currentName = row.name;
      idx.currentGroup = NormalizeGroup(row.group);
      SaveIndex(idx);
      selectedName = idx.currentName;
      selectedGroup = idx.currentGroup;
      return true;
    }
    return false;
  }

  bool HasUsableProxyQuota(const IndexEntry &row)
  {
    if (g_ProxyAutoMarkAbnormalAccounts && row.abnormal)
    {
      return false;
    }
    if (!row.quotaUsageOk)
    {
      // Unknown quota should still be considered usable and validated by real requests.
      return true;
    }
    const std::wstring plan = ToLowerCopy(row.quotaPlanType);
    const bool isFreePlan =
        plan == L"free" || NormalizeGroup(row.group) == L"free";
    if (isFreePlan)
    {
      const int freeQuota =
          row.quota7dRemainingPercent >= 0 ? row.quota7dRemainingPercent
                                           : row.quota5hRemainingPercent;
      return freeQuota > 0;
    }
    if (row.quota5hRemainingPercent >= 0)
    {
      return row.quota5hRemainingPercent > 0;
    }
    if (row.quota7dRemainingPercent >= 0)
    {
      return row.quota7dRemainingPercent > 0;
    }
    return false;
  }

  int GetProxyQuotaMetricValue(const IndexEntry &row, bool &usesWeeklyWindow)
  {
    usesWeeklyWindow = false;
    const std::wstring plan = ToLowerCopy(row.quotaPlanType);
    const bool isFreePlan =
        plan == L"free" || NormalizeGroup(row.group) == L"free";
    if (isFreePlan)
    {
      usesWeeklyWindow = true;
      return row.quota7dRemainingPercent >= 0 ? row.quota7dRemainingPercent
                                              : row.quota5hRemainingPercent;
    }
    return row.quota5hRemainingPercent;
  }

  bool HasPositiveProxyQuota(const IndexEntry &row)
  {
    if (!HasUsableProxyQuota(row))
    {
      return false;
    }
    bool usesWeeklyWindow = false;
    const int quota = GetProxyQuotaMetricValue(row, usesWeeklyWindow);
    return quota > 0;
  }

  int GetProxyComparableQuotaMetric(const IndexEntry &row,
                                    const bool useWeeklyWindow)
  {
    bool candidateWeekly = false;
    int metric = GetProxyQuotaMetricValue(row, candidateWeekly);
    if (useWeeklyWindow && !candidateWeekly)
    {
      return -1;
    }
    if (!useWeeklyWindow)
    {
      metric = row.quota5hRemainingPercent;
    }
    return metric;
  }

  void MaybeSendProxyLowQuotaHint(const IndexEntry &current,
                                  const std::vector<IndexEntry> &candidates)
  {
    bool useWeeklyWindow = false;
    const int currentQuota = GetProxyQuotaMetricValue(current, useWeeklyWindow);
    if (currentQuota < 0 || currentQuota > kLowQuotaThresholdPercent)
    {
      return;
    }
    if (g_DebugWebHwnd == nullptr || !IsWindow(g_DebugWebHwnd))
    {
      return;
    }

    int bestQuota = -1;
    std::wstring bestName;
    for (const auto &row : candidates)
    {
      if (EqualsIgnoreCase(row.name, current.name) &&
          NormalizeGroup(row.group) == NormalizeGroup(current.group))
      {
        continue;
      }
      const int candidateQuota =
          GetProxyComparableQuotaMetric(row, useWeeklyWindow);
      if (candidateQuota < 0)
      {
        continue;
      }
      if (bestName.empty() || candidateQuota > bestQuota)
      {
        bestName = row.name;
        bestQuota = candidateQuota;
      }
    }

    const std::wstring accountKey =
        NormalizeGroup(current.group) + L"::" + current.name;
    const auto now = std::chrono::steady_clock::now();
    {
      std::lock_guard<std::mutex> lock(g_ProxyLowQuotaHintMutex);
      const bool cooledDown =
          g_ProxyLowQuotaHintAt.time_since_epoch().count() == 0 ||
          std::chrono::duration_cast<std::chrono::seconds>(now -
                                                           g_ProxyLowQuotaHintAt)
                  .count() >= kProxyLowQuotaHintCooldownSeconds;
      if (!cooledDown && accountKey == g_ProxyLowQuotaHintAccountKey)
      {
        return;
      }
      g_ProxyLowQuotaHintAt = now;
      g_ProxyLowQuotaHintAccountKey = accountKey;
    }

    std::wstring msg = TrFormat(
        L"status_text.proxy_low_quota_current",
        L"反代当前账号 {account} 可用额度仅剩 {quota}%（{window}窗口）",
        {{L"account", current.name},
         {L"quota", std::to_wstring(currentQuota)},
         {L"window", useWeeklyWindow ? Tr(L"low_quota.window_7d", L"7天")
                                     : Tr(L"low_quota.window_5h", L"5小时")}});
    if (!bestName.empty() && bestQuota > currentQuota)
    {
      msg += TrFormat(L"status_text.proxy_low_quota_suggest",
                      L"，建议切换到 {account}（{quota}%）",
                      {{L"account", bestName},
                       {L"quota", std::to_wstring(bestQuota)}});
    }
    SendWebStatusThreadSafe(g_DebugWebHwnd, msg, L"warning", L"proxy_low_quota");
  }

  void ShowProxyTrayBalloon(const std::wstring &title, const std::wstring &message)
  {
    if (g_DebugWebHwnd == nullptr || !IsWindow(g_DebugWebHwnd))
    {
      return;
    }
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_DebugWebHwnd;
    nid.uID = 1;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = NIIF_WARNING;
    wcsncpy_s(nid.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(nid.szInfo, message.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
  }

  std::wstring BuildProxyAccountKey(const std::wstring &accountName,
                                    const std::wstring &groupName)
  {
    return ToLowerCopy(NormalizeGroup(groupName)) + L"::" +
           ToLowerCopy(accountName);
  }

  std::wstring DetectProxyAccountAbnormalReason(const std::wstring &responseBody,
                                                const DWORD statusCode)
  {
    if (statusCode != 401)
    {
      return L"";
    }

    const std::wstring lower = ToLowerCopy(responseBody);
    auto contains = [&](const wchar_t *token) -> bool
    {
      if (token == nullptr || *token == L'\0')
      {
        return false;
      }
      return lower.find(token) != std::wstring::npos;
    };
    auto extractLowerField = [&](const std::wstring &key) -> std::wstring
    {
      return ToLowerCopy(
          UnescapeJsonString(ExtractJsonField(responseBody, key)));
    };
    auto normalizeReason = [&](const std::wstring &value) -> std::wstring
    {
      std::wstring text = ToLowerCopy(value);
      text.erase(std::remove_if(text.begin(), text.end(), [](wchar_t ch)
                                { return ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n'; }),
                 text.end());
      return text;
    };

    const std::wstring code = normalizeReason(extractLowerField(L"code"));
    const std::wstring type = normalizeReason(extractLowerField(L"type"));
    const std::wstring err = normalizeReason(extractLowerField(L"error"));
    const std::wstring message = normalizeReason(extractLowerField(L"message"));

    const std::vector<std::wstring> candidates = {
        L"token_invalidated", L"tokeninvalidated", L"invalid_token",
        L"token_invalid", L"invalid_api_key", L"apikeyinvalid",
        L"auth_token_missing", L"account_not_found", L"account_deactivated",
        L"not_authorized", L"unauthorized", L"forbidden"};
    for (const auto &token : candidates)
    {
      if ((!code.empty() && code.find(token) != std::wstring::npos) ||
          (!type.empty() && type.find(token) != std::wstring::npos) ||
          (!err.empty() && err.find(token) != std::wstring::npos) ||
          (!message.empty() && message.find(token) != std::wstring::npos) ||
          contains(token.c_str()))
      {
        return token;
      }
    }
    if (contains(L"auth") || contains(L"token") || contains(L"unauthorized") ||
        contains(L"forbidden"))
    {
      return L"unauthorized";
    }
    return L"unauthorized";
  }

  bool IsProxyAccountAbnormalPayload(const std::wstring &responseBody,
                                     const DWORD statusCode,
                                     std::wstring &reasonOut)
  {
    reasonOut = DetectProxyAccountAbnormalReason(responseBody, statusCode);
    return !reasonOut.empty();
  }

  bool IsUsageLimitReachedPayload(const std::wstring &responseBody)
  {
    if (responseBody.empty())
    {
      return false;
    }
    if (ToLowerCopy(responseBody).find(L"usage_limit_reached") !=
        std::wstring::npos)
    {
      return true;
    }
    const std::wstring typeValue =
        ToLowerCopy(UnescapeJsonString(ExtractJsonField(responseBody, L"type")));
    return typeValue == L"usage_limit_reached";
  }

  bool MarkProxyAccountQuotaExhausted(const std::wstring &accountName,
                                      const std::wstring &groupName,
                                      const std::wstring &responseBody,
                                      std::wstring &error)
  {
    error.clear();
    EnsureIndexExists();
    IndexData idx;
    if (!LoadIndex(idx))
    {
      error = L"index_load_failed";
      return false;
    }

    const std::wstring normalizedGroup = NormalizeGroup(groupName);
    auto it = std::find_if(
        idx.accounts.begin(), idx.accounts.end(), [&](const IndexEntry &row)
        { return EqualsIgnoreCase(row.name, accountName) &&
                 NormalizeGroup(row.group) == normalizedGroup; });
    if (it == idx.accounts.end())
    {
      it = std::find_if(idx.accounts.begin(), idx.accounts.end(),
                        [&](const IndexEntry &row)
                        {
                          return EqualsIgnoreCase(row.name, accountName);
                        });
    }
    if (it == idx.accounts.end())
    {
      error = L"account_not_found_in_index";
      return false;
    }

    const std::wstring planInBody =
        ToLowerCopy(UnescapeJsonString(ExtractJsonField(responseBody, L"plan_type")));
    if (!planInBody.empty())
    {
      it->quotaPlanType = planInBody;
    }
    const std::wstring currentPlan = ToLowerCopy(it->quotaPlanType);
    const bool isFreePlan =
        planInBody == L"free" || currentPlan == L"free" ||
        NormalizeGroup(it->group) == L"free";
    const long long resetsAt = ExtractJsonInt64Field(responseBody, L"resets_at", -1);
    const long long resetsInSeconds =
        ExtractJsonInt64Field(responseBody, L"resets_in_seconds", -1);

    it->quotaUsageOk = true;
    if (isFreePlan)
    {
      it->quota7dRemainingPercent = 0;
      if (it->quota5hRemainingPercent < 0)
      {
        it->quota5hRemainingPercent = 0;
      }
      if (resetsInSeconds >= 0)
      {
        it->quota7dResetAfterSeconds = resetsInSeconds;
      }
      if (resetsAt >= 0)
      {
        it->quota7dResetAt = resetsAt;
      }
    }
    else
    {
      it->quota5hRemainingPercent = 0;
      if (it->quota7dRemainingPercent < 0)
      {
        it->quota7dRemainingPercent = 0;
      }
      if (resetsInSeconds >= 0)
      {
        it->quota5hResetAfterSeconds = resetsInSeconds;
      }
      if (resetsAt >= 0)
      {
        it->quota5hResetAt = resetsAt;
      }
    }
    it->updatedAt = NowText();

    if (!SaveIndex(idx))
    {
      error = L"save_index_failed";
      return false;
    }
    return true;
  }

  bool MarkProxyAccountAbnormal(const std::wstring &accountName,
                                const std::wstring &groupName,
                                const std::wstring &reason,
                                const std::wstring &responseBody,
                                std::wstring &error)
  {
    error.clear();
    EnsureIndexExists();
    IndexData idx;
    if (!LoadIndex(idx))
    {
      error = L"index_load_failed";
      return false;
    }

    const std::wstring normalizedGroup = NormalizeGroup(groupName);
    auto it = std::find_if(
        idx.accounts.begin(), idx.accounts.end(), [&](const IndexEntry &row)
        { return EqualsIgnoreCase(row.name, accountName) &&
                 NormalizeGroup(row.group) == normalizedGroup; });
    if (it == idx.accounts.end())
    {
      it = std::find_if(idx.accounts.begin(), idx.accounts.end(),
                        [&](const IndexEntry &row)
                        {
                          return EqualsIgnoreCase(row.name, accountName);
                        });
    }
    if (it == idx.accounts.end())
    {
      error = L"account_not_found_in_index";
      return false;
    }

    std::wstring abnormalReason = ToLowerCopy(reason);
    if (abnormalReason.empty())
    {
      abnormalReason = DetectProxyAccountAbnormalReason(responseBody, 0);
    }
    if (abnormalReason.empty())
    {
      abnormalReason = L"proxy_account_unavailable";
    }
    it->abnormal = true;
    it->abnormalReason = abnormalReason;
    it->abnormalAt = NowText();
    it->updatedAt = it->abnormalAt;

    if (!SaveIndex(idx))
    {
      error = L"save_index_failed";
      return false;
    }

    if (g_AutoDeleteAbnormalAccounts)
    {
      std::wstring status;
      std::wstring code;
      const bool deleted =
          DeleteAccountBackup(it->name, NormalizeGroup(it->group), status, code);
      if (deleted && g_DebugWebHwnd != nullptr && IsWindow(g_DebugWebHwnd))
      {
        SendWebStatusThreadSafe(g_DebugWebHwnd, L"", L"warning",
                                L"account_abnormal_auto_deleted");
      }
    }
    return true;
  }

  bool ResolveBestUsableProxyCandidateExcluding(
      const std::set<std::wstring> &excludedKeys, fs::path &authPath,
      std::wstring &accountName, std::wstring &groupName, std::wstring &error)
  {
    authPath.clear();
    accountName.clear();
    groupName.clear();
    error.clear();

    EnsureIndexExists();
    IndexData idx;
    if (!LoadIndex(idx))
    {
      error = L"index_load_failed";
      return false;
    }

    const IndexEntry *best = nullptr;
    const IndexEntry *unknownQuotaCandidate = nullptr;
    int bestQuota = -1;
    for (const auto &row : idx.accounts)
    {
      if (excludedKeys.find(BuildProxyAccountKey(row.name, row.group)) !=
          excludedKeys.end())
      {
        continue;
      }
      const fs::path path = ResolveAuthPathFromIndex(row);
      if (!fs::exists(path) || !HasUsableProxyQuota(row))
      {
        continue;
      }
      bool usesWeeklyWindow = false;
      const int quota = GetProxyQuotaMetricValue(row, usesWeeklyWindow);
      if (quota < 0)
      {
        if (unknownQuotaCandidate == nullptr)
        {
          unknownQuotaCandidate = &row;
        }
        continue;
      }
      if (best == nullptr || quota > bestQuota)
      {
        best = &row;
        bestQuota = quota;
      }
    }
    if (best == nullptr)
    {
      best = unknownQuotaCandidate;
    }
    if (best == nullptr)
    {
      error = L"no_quota_available_accounts";
      return false;
    }

    authPath = ResolveAuthPathFromIndex(*best);
    accountName = best->name;
    groupName = NormalizeGroup(best->group);
    return true;
  }

  bool ResolveBestUsableProxyCandidate(const std::wstring &excludeName,
                                       const std::wstring &excludeGroup,
                                       fs::path &authPath,
                                       std::wstring &accountName,
                                       std::wstring &groupName,
                                       std::wstring &error)
  {
    std::set<std::wstring> excluded;
    excluded.insert(BuildProxyAccountKey(excludeName, excludeGroup));
    return ResolveBestUsableProxyCandidateExcluding(excluded, authPath, accountName,
                                                    groupName, error);
  }

  bool ResolveProxyDispatchAuthPath(fs::path &authPath, std::wstring &accountName,
                                    std::wstring &groupName,
                                    std::wstring &error)
  {
    authPath.clear();
    accountName.clear();
    groupName.clear();
    error.clear();

    EnsureIndexExists();
    IndexData idx;
    if (!LoadIndex(idx))
    {
      error = L"index_load_failed";
      return false;
    }

    std::vector<IndexEntry> candidates;
    candidates.reserve(idx.accounts.size());
    for (const auto &row : idx.accounts)
    {
      const fs::path p = ResolveAuthPathFromIndex(row);
      if (fs::exists(p))
      {
        candidates.push_back(row);
      }
    }
    if (candidates.empty())
    {
      error = L"no_available_accounts";
      return false;
    }
    std::vector<IndexEntry> usableCandidates;
    usableCandidates.reserve(candidates.size());
    for (const auto &row : candidates)
    {
      if (HasUsableProxyQuota(row))
      {
        usableCandidates.push_back(row);
      }
    }

    const std::wstring mode = ToLowerCopy(g_ProxyDispatchMode);
    if (mode == L"fixed")
    {
      if (idx.currentName.empty())
      {
        // Auto-select the first normal account when fixed mode is enabled
        // but no current account is chosen.
        for (const auto &row : idx.accounts)
        {
          if (row.abnormal)
          {
            continue;
          }
          const fs::path candidate = ResolveAuthPathFromIndex(row);
          if (candidate.empty() || !fs::exists(candidate))
          {
            continue;
          }
          idx.currentName = row.name;
          idx.currentGroup = NormalizeGroup(row.group);
          SaveIndex(idx);
          break;
        }
      }

      const std::wstring targetName = idx.currentName;
      const std::wstring targetGroup = NormalizeGroup(idx.currentGroup);
      if (targetName.empty())
      {
        error = L"no_selected_account";
        return false;
      }
      for (const auto &row : candidates)
      {
        if (EqualsIgnoreCase(row.name, targetName) &&
            NormalizeGroup(row.group) == targetGroup)
        {
          if (HasPositiveProxyQuota(row))
          {
            MaybeSendProxyLowQuotaHint(row, candidates);
            authPath = ResolveAuthPathFromIndex(row);
            accountName = row.name;
            groupName = NormalizeGroup(row.group);
            return true;
          }
          fs::path fallbackAuthPath;
          std::wstring fallbackName;
          std::wstring fallbackGroup;
          std::wstring fallbackError;
          if (!ResolveBestUsableProxyCandidate(targetName, targetGroup,
                                               fallbackAuthPath, fallbackName,
                                               fallbackGroup, fallbackError))
          {
            if (HasUsableProxyQuota(row))
            {
              // Keep fixed-account behavior as fallback when quota is unknown.
              authPath = ResolveAuthPathFromIndex(row);
              accountName = row.name;
              groupName = NormalizeGroup(row.group);
              return true;
            }
            error = fallbackError.empty() ? L"no_quota_available_accounts"
                                          : fallbackError;
            return false;
          }
          std::wstring switchStatus;
          std::wstring switchCode;
          const bool switched = SwitchToAccount(fallbackName, fallbackGroup,
                                                switchStatus, switchCode);
          if (switched)
          {
            const std::wstring trayTitle =
                Tr(L"tray.notify_title", L"Codex额度提醒");
            const std::wstring trayMessage = TrFormat(
                L"tray.proxy_fixed_auto_switched",
                L"固定账号额度不可用，已自动切换到 {account}",
                {{L"account", fallbackName}});
            ShowProxyTrayBalloon(trayTitle, trayMessage);
            if (g_DebugWebHwnd != nullptr && IsWindow(g_DebugWebHwnd))
            {
              SendWebStatusThreadSafe(g_DebugWebHwnd,
                                      TrFormat(
                                          L"status_text.proxy_fixed_auto_switched",
                                          L"固定账号额度不可用，已自动切换到 {account} 并继续请求",
                                          {{L"account", fallbackName}}),
                                      L"warning", L"proxy_fixed_auto_switched");
              PostAsyncWebJson(g_DebugWebHwnd,
                               BuildAccountsListJson(false, L"", L""));
            }
            authPath = fallbackAuthPath;
            accountName = fallbackName;
            groupName = NormalizeGroup(fallbackGroup);
            return true;
          }
          error = switchStatus.empty() ? L"proxy_fixed_switch_failed"
                                       : L"proxy_fixed_switch_failed:" +
                                             switchStatus;
          return false;
        }
      }
      error = L"fixed_account_not_found";
      return false;
    }

    if (usableCandidates.empty())
    {
      error = L"no_quota_available_accounts";
      return false;
    }
    // For round_robin/random, include both known-quota and unknown-quota usable accounts.
    const std::vector<IndexEntry> &activeCandidates = usableCandidates;

    size_t pick = 0;
    if (mode == L"random")
    {
      const std::wstring rnd = GenerateRandomBase64Url(4);
      size_t seed = 0;
      for (wchar_t ch : rnd)
      {
        seed = seed * 131 + static_cast<size_t>(ch);
      }
      pick = activeCandidates.empty() ? 0 : (seed % activeCandidates.size());
    }
    else
    {
      // round_robin by default
      pick = activeCandidates.empty()
                 ? 0
                 : (g_ProxyRoundRobinCursor % activeCandidates.size());
      ++g_ProxyRoundRobinCursor;
    }

    const auto &row = activeCandidates[pick];
    MaybeSendProxyLowQuotaHint(row, activeCandidates);
    authPath = ResolveAuthPathFromIndex(row);
    accountName = row.name;
    groupName = NormalizeGroup(row.group);
    return true;
  }

  bool SendProxyUpstreamRequestByAuth(
      const fs::path &authPath, const std::string &methodLower,
      const std::wstring &upstreamPath,
      const std::map<std::string, std::string> &headers, const std::string &body,
      const int timeoutSec, DWORD &statusCode, std::wstring &contentType,
      std::string &responseBody, std::wstring &error)
  {
    statusCode = 0;
    contentType.clear();
    responseBody.clear();
    error.clear();

    std::wstring accessToken;
    std::wstring accountId;
    if (!ReadAuthTokenInfo(authPath, accessToken, accountId))
    {
      error = L"auth_token_missing";
      return false;
    }

    HINTERNET hSession = WinHttpOpen(
        BuildCodexApiUserAgent().c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (hSession == nullptr)
    {
      error = L"WinHttpOpen_failed";
      return false;
    }
    HINTERNET hConnect =
        WinHttpConnect(hSession, kUsageHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (hConnect == nullptr)
    {
      WinHttpCloseHandle(hSession);
      error = L"WinHttpConnect_failed";
      return false;
    }
    std::wstring methodW = L"POST";
    if (methodLower == "get")
    {
      methodW = L"GET";
    }
    else if (methodLower == "post")
    {
      methodW = L"POST";
    }
    else if (methodLower == "put")
    {
      methodW = L"PUT";
    }
    else if (methodLower == "delete")
    {
      methodW = L"DELETE";
    }
    else if (!methodLower.empty())
    {
      methodW = FromUtf8(methodLower);
    }
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, methodW.c_str(), upstreamPath.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (hRequest == nullptr)
    {
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      error = L"WinHttpOpenRequest_failed";
      return false;
    }

    const int timeoutMs = timeoutSec * 1000;
    WinHttpSetTimeouts(hRequest, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    std::wstring contentTypeHeader = L"application/json";
    auto itContentType = headers.find("content-type");
    if (itContentType != headers.end() && !itContentType->second.empty())
    {
      contentTypeHeader = FromUtf8(itContentType->second);
    }
    std::wstring acceptHeader = L"application/json";
    auto itAccept = headers.find("accept");
    if (itAccept != headers.end() && !itAccept->second.empty())
    {
      acceptHeader = FromUtf8(itAccept->second);
    }

    const std::wstring reqHeaders =
        L"Content-Type: " + contentTypeHeader + L"\r\nAccept: " + acceptHeader +
        L"\r\nAuthorization: Bearer " + accessToken +
        L"\r\nChatGPT-Account-Id: " + accountId +
        L"\r\nVersion: " + ReadCodexLatestVersion() +
        L"\r\nOpenai-Beta: responses=experimental\r\nSession_id: " +
        NewSessionId() + L"\r\nOriginator: codex_cli_rs\r\nUser-Agent: " +
        BuildCodexApiUserAgent() + L"\r\nConnection: close\r\n";

    BOOL ok = WinHttpSendRequest(
        hRequest, reqHeaders.c_str(), static_cast<DWORD>(-1L),
        body.empty() ? WINHTTP_NO_REQUEST_DATA
                     : const_cast<char *>(body.data()),
        static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0);
    if (ok)
    {
      ok = WinHttpReceiveResponse(hRequest, nullptr);
    }
    if (!ok)
    {
      error = L"http_transport_failed";
      WinHttpCloseHandle(hRequest);
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      return false;
    }

    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest,
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize,
                        WINHTTP_NO_HEADER_INDEX);
    contentType = QueryWinHttpHeaderText(hRequest, WINHTTP_QUERY_CONTENT_TYPE);

    while (true)
    {
      char chunk[4096]{};
      DWORD read = 0;
      if (!WinHttpReadData(hRequest, chunk, sizeof(chunk), &read) || read == 0)
      {
        break;
      }
      responseBody.append(chunk, chunk + read);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
  }

  bool ParseHttpRequestFromSocket(SOCKET clientSock, std::string &method,
                                  std::string &path,
                                  std::map<std::string, std::string> &headers,
                                  std::string &body)
  {
    method.clear();
    path.clear();
    headers.clear();
    body.clear();

    std::string buffer;
    buffer.reserve(8192);
    size_t headerEnd = std::string::npos;
    while (headerEnd == std::string::npos && buffer.size() < 1024 * 1024)
    {
      char chunk[4096]{};
      const int n = recv(clientSock, chunk, sizeof(chunk), 0);
      if (n <= 0)
      {
        return false;
      }
      buffer.append(chunk, chunk + n);
      headerEnd = buffer.find("\r\n\r\n");
    }
    if (headerEnd == std::string::npos)
    {
      return false;
    }

    const std::string head = buffer.substr(0, headerEnd);
    std::istringstream hs(head);
    std::string requestLine;
    if (!std::getline(hs, requestLine))
    {
      return false;
    }
    if (!requestLine.empty() && requestLine.back() == '\r')
    {
      requestLine.pop_back();
    }
    std::istringstream rl(requestLine);
    std::string httpVersion;
    if (!(rl >> method >> path >> httpVersion))
    {
      return false;
    }
    method = ToLowerAscii(method);

    std::string line;
    while (std::getline(hs, line))
    {
      if (!line.empty() && line.back() == '\r')
      {
        line.pop_back();
      }
      const size_t sep = line.find(':');
      if (sep == std::string::npos)
      {
        continue;
      }
      std::string key = ToLowerAscii(line.substr(0, sep));
      std::string value = line.substr(sep + 1);
      while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
      {
        value.erase(value.begin());
      }
      headers[key] = value;
    }

    size_t contentLen = 0;
    const auto itLen = headers.find("content-length");
    if (itLen != headers.end())
    {
      contentLen =
          static_cast<size_t>((std::max)(0, atoi(itLen->second.c_str())));
    }

    const size_t bodyStart = headerEnd + 4;
    if (buffer.size() > bodyStart)
    {
      body = buffer.substr(bodyStart);
    }
    while (body.size() < contentLen)
    {
      char chunk[4096]{};
      const int n = recv(clientSock, chunk, sizeof(chunk), 0);
      if (n <= 0)
      {
        return false;
      }
      body.append(chunk, chunk + n);
    }
    if (body.size() > contentLen)
    {
      body.resize(contentLen);
    }
    return true;
  }

  bool IsProxyRequestAuthorized(const std::map<std::string, std::string> &headers)
  {
    const std::wstring expected = g_ProxyApiKey;
    if (expected.empty())
    {
      return true;
    }

    auto normalizeToken = [](std::string token) -> std::wstring
    {
      token = TrimAscii(token);
      if (token.size() >= 2 &&
          ((token.front() == '"' && token.back() == '"') ||
           (token.front() == '\'' && token.back() == '\'')))
      {
        token = token.substr(1, token.size() - 2);
      }
      token = TrimAscii(token);
      return Utf8ToWide(token);
    };

    auto itAuth = headers.find("authorization");
    if (itAuth != headers.end())
    {
      std::string value = TrimAscii(itAuth->second);
      std::string lower = ToLowerAscii(value);
      const std::string prefix = "bearer ";
      if (lower.rfind(prefix, 0) == 0)
      {
        const std::wstring token = normalizeToken(value.substr(prefix.size()));
        if (!token.empty() && token == expected)
        {
          return true;
        }
      }
      else
      {
        const std::wstring token = normalizeToken(value);
        if (!token.empty() && token == expected)
        {
          return true;
        }
      }
    }

    auto itApiKey = headers.find("x-api-key");
    if (itApiKey != headers.end())
    {
      const std::wstring token = normalizeToken(itApiKey->second);
      if (!token.empty() && token == expected)
      {
        return true;
      }
    }

    auto itAnthropicKey = headers.find("anthropic-api-key");
    if (itAnthropicKey != headers.end())
    {
      const std::wstring token = normalizeToken(itAnthropicKey->second);
      if (!token.empty() && token == expected)
      {
        return true;
      }
    }

    return false;
  }

  bool ForwardProxyToUpstream(const std::string &method, const std::string &path,
                              const std::map<std::string, std::string> &headers,
                              const std::string &body, const int timeoutSec,
                              DWORD &statusCode, std::wstring &contentType,
                              std::string &responseBody, std::wstring &error,
                              TrafficLogEntry *trafficMeta = nullptr)
  {
    statusCode = 0;
    contentType.clear();
    responseBody.clear();
    error.clear();

    fs::path authPath;
    std::wstring accountName;
    std::wstring groupName;
    if (!ResolveProxyDispatchAuthPath(authPath, accountName, groupName, error))
    {
      return false;
    }

    const std::string methodLower = ToLowerAscii(method);
    const std::string lowerPath = ToLowerAscii(path);
    const auto startedAt = std::chrono::steady_clock::now();
    int inputTokens = -1;
    int outputTokens = -1;
    int totalTokens = -1;
    std::wstring reqModel;
    std::wstring accountUsed;
    std::wstring protocol = L"codex";
    if (lowerPath.rfind("/v1/", 0) == 0)
    {
      protocol = L"openai";
    }
    if (!body.empty())
    {
      reqModel = ExtractJsonField(FromUtf8(body), L"model");
    }
    if (trafficMeta != nullptr)
    {
      trafficMeta->calledAt =
          std::chrono::duration_cast<std::chrono::seconds>(
              std::chrono::system_clock::now().time_since_epoch())
              .count();
      trafficMeta->statusCode = 0;
      trafficMeta->method = ToLowerWide(FromUtf8(method));
      trafficMeta->model = reqModel;
      trafficMeta->protocol = protocol;
      trafficMeta->account = accountName;
      trafficMeta->path = FromUtf8(path);
      trafficMeta->inputTokens = -1;
      trafficMeta->outputTokens = -1;
      trafficMeta->totalTokens = -1;
      trafficMeta->elapsedMs = -1;
    }
    accountUsed = accountName;
    auto finalizeReturn = [&](const bool result) -> bool
    {
      if (trafficMeta != nullptr)
      {
        trafficMeta->statusCode = static_cast<int>(statusCode);
        trafficMeta->method = ToLowerWide(FromUtf8(method));
        trafficMeta->model = reqModel;
        trafficMeta->protocol = protocol;
        trafficMeta->account = accountUsed;
        trafficMeta->inputTokens = inputTokens;
        trafficMeta->outputTokens = outputTokens;
        trafficMeta->totalTokens = totalTokens;
        trafficMeta->elapsedMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startedAt)
                .count();
      }
      return result;
    };
    const bool silentAutoSwitchInRoundRobin =
        ToLowerCopy(g_ProxyDispatchMode) == L"round_robin";
    std::set<std::wstring> triedAccountKeys;
    triedAccountKeys.insert(BuildProxyAccountKey(accountName, groupName));
    auto appendFailoverFailureLog = [&](const std::wstring &failedAccount,
                                        const std::wstring &failedBody,
                                        const DWORD failedStatus,
                                        const int inTokens,
                                        const int outTokens,
                                        const int totalUsed)
    {
      TrafficLogEntry retryFailedEntry;
      retryFailedEntry.calledAt =
          std::chrono::duration_cast<std::chrono::seconds>(
              std::chrono::system_clock::now().time_since_epoch())
              .count();
      retryFailedEntry.statusCode = static_cast<int>(failedStatus);
      retryFailedEntry.method = ToLowerWide(FromUtf8(method));
      retryFailedEntry.model = reqModel;
      retryFailedEntry.protocol = protocol;
      retryFailedEntry.account = failedAccount;
      retryFailedEntry.path = FromUtf8(path);
      retryFailedEntry.inputTokens = inTokens;
      retryFailedEntry.outputTokens = outTokens;
      retryFailedEntry.totalTokens = totalUsed;
      retryFailedEntry.elapsedMs =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - startedAt)
              .count();
      AppendTrafficLog(retryFailedEntry);
    };
    auto handleRetryableProxyFailure =
        [&](const DWORD failedStatus, const std::wstring &failedBody,
            const int inTokens, const int outTokens,
            const int totalUsed) -> bool
    {
      const bool usageLimitReached =
          failedStatus >= 400 && IsUsageLimitReachedPayload(failedBody);
      std::wstring abnormalReason;
      const bool abnormalDetected =
          !usageLimitReached && g_ProxyAutoMarkAbnormalAccounts &&
          failedStatus >= 400 &&
          IsProxyAccountAbnormalPayload(failedBody, failedStatus, abnormalReason);
      if (!usageLimitReached && !abnormalDetected)
      {
        return false;
      }

      appendFailoverFailureLog(accountName, failedBody, failedStatus, inTokens,
                               outTokens, totalUsed);

      if (usageLimitReached)
      {
        std::wstring markError;
        MarkProxyAccountQuotaExhausted(accountName, groupName, failedBody,
                                       markError);
        const std::wstring baseMsg = TrFormat(
            L"status_text.proxy_usage_limit_reached",
            L"账号 {account} 已用尽额度（usage_limit_reached）",
            {{L"account", accountName}});
        if (!silentAutoSwitchInRoundRobin)
        {
          if (g_DebugWebHwnd != nullptr && IsWindow(g_DebugWebHwnd))
          {
            SendWebStatusThreadSafe(
                g_DebugWebHwnd,
                baseMsg + Tr(L"status_text.proxy_usage_limit_switching_retry",
                             L"，正在切换并重试请求"),
                L"warning", L"proxy_usage_limit_reached");
          }
          ShowProxyTrayBalloon(Tr(L"tray.notify_title", L"Codex额度提醒"),
                               baseMsg + Tr(L"tray.proxy_auto_switching_suffix",
                                            L"，正在自动切换账号"));
        }
      }
      else
      {
        std::wstring markError;
        if (!MarkProxyAccountAbnormal(accountName, groupName, abnormalReason,
                                      failedBody, markError) &&
            abnormalReason.empty())
        {
          abnormalReason = L"proxy_account_unavailable";
        }
        if (!silentAutoSwitchInRoundRobin)
        {
          const std::wstring baseMsg = TrFormat(
              L"status_text.proxy_account_abnormal_marked",
              L"账号 {account} 请求失败（{reason}），已标记为异常",
              {{L"account", accountName},
               {L"reason",
                abnormalReason.empty() ? L"proxy_account_unavailable"
                                       : abnormalReason}});
          if (g_DebugWebHwnd != nullptr && IsWindow(g_DebugWebHwnd))
          {
            SendWebStatusThreadSafe(
                g_DebugWebHwnd,
                baseMsg + Tr(L"status_text.proxy_abnormal_switching_retry",
                             L"，正在切换并重试请求"),
                L"warning", L"proxy_account_abnormal");
          }
          ShowProxyTrayBalloon(Tr(L"tray.notify_title", L"Codex额度提醒"),
                               baseMsg + Tr(L"tray.proxy_auto_switching_suffix",
                                            L"，正在自动切换账号"));
        }
      }

      fs::path retryAuthPath;
      std::wstring retryName;
      std::wstring retryGroup;
      std::wstring retryError;
      if (!ResolveBestUsableProxyCandidateExcluding(triedAccountKeys, retryAuthPath,
                                                    retryName, retryGroup,
                                                    retryError))
      {
        return false;
      }
      if (EqualsIgnoreCase(retryName, accountName) &&
          NormalizeGroup(retryGroup) == NormalizeGroup(groupName))
      {
        return false;
      }

      std::wstring switchStatus;
      std::wstring switchCode;
      const bool switched =
          SwitchToAccount(retryName, retryGroup, switchStatus, switchCode);
      if (switched)
      {
        if (g_DebugWebHwnd != nullptr && IsWindow(g_DebugWebHwnd))
        {
          PostAsyncWebJson(g_DebugWebHwnd, BuildAccountsListJson(false, L"", L""));
        }
        if (!silentAutoSwitchInRoundRobin)
        {
          const std::wstring trayKey =
              abnormalDetected ? L"tray.proxy_abnormal_auto_switched_retry"
                               : L"tray.proxy_auto_switched_retry";
          const std::wstring trayFallback =
              abnormalDetected ? L"已跳过异常账号并切换到 {account} 重试"
                               : L"已自动切换到账号 {account}，并继续重试当前请求";
          ShowProxyTrayBalloon(
              Tr(L"tray.notify_title", L"Codex额度提醒"),
              TrFormat(trayKey, trayFallback,
                       {{L"account", retryName}}));
          if (g_DebugWebHwnd != nullptr && IsWindow(g_DebugWebHwnd))
          {
            const std::wstring statusKey =
                abnormalDetected ? L"status_text.proxy_abnormal_auto_switched_retry"
                                 : L"status_text.proxy_auto_switched_retry";
            const std::wstring statusFallback =
                abnormalDetected ? L"已自动切换到 {account}，因异常账号正在重试请求"
                                 : L"已自动切换到 {account}，正在重试请求";
            const std::wstring statusCode =
                abnormalDetected ? L"proxy_abnormal_auto_switched"
                                 : L"proxy_auto_switched";
            SendWebStatusThreadSafe(
                g_DebugWebHwnd,
                TrFormat(statusKey, statusFallback, {{L"account", retryName}}),
                L"warning", statusCode);
          }
        }
      }
      else if (!silentAutoSwitchInRoundRobin &&
               g_DebugWebHwnd != nullptr && IsWindow(g_DebugWebHwnd))
      {
        SendWebStatusThreadSafe(
            g_DebugWebHwnd,
            TrFormat(L"status_text.proxy_auto_switch_failed",
                     L"自动切换账号失败: {error}", {{L"error", switchStatus}}),
            L"error", switchCode);
      }

      authPath = retryAuthPath;
      accountName = retryName;
      groupName = NormalizeGroup(retryGroup);
      accountUsed = retryName;
      triedAccountKeys.insert(BuildProxyAccountKey(accountName, groupName));
      return true;
    };
    if (lowerPath.rfind("/v1/models", 0) == 0 && methodLower == "get")
    {
      std::vector<std::wstring> models;
      if (!GetCachedModelIds(authPath, models, error))
      {
        return false;
      }
      statusCode = 200;
      contentType = L"application/json";
      responseBody = WideToUtf8(BuildOpenAiModelsJson(models));
      return finalizeReturn(true);
    }

    if (lowerPath.rfind("/v1/chat/completions", 0) == 0 &&
        methodLower == "post")
    {
      const std::wstring jsonBody = FromUtf8(body);
      const std::wstring requestModel = ExtractJsonField(jsonBody, L"model");
      const bool requestStream = ExtractJsonBoolField(jsonBody, L"stream", false);
      const std::wstring prompt = ExtractOpenAiUserPrompt(jsonBody);
      std::wstring modelToUse = requestModel;
      if (modelToUse.empty())
      {
        std::vector<std::wstring> models;
        std::wstring modelErr;
        if (GetCachedModelIds(authPath, models, modelErr) && !models.empty())
        {
          modelToUse = models.front();
        }
      }
      if (modelToUse.empty())
      {
        statusCode = 400;
        contentType = L"application/json";
        responseBody =
            R"({"error":{"message":"model is required","type":"invalid_request_error","code":"model_required"}})";
        return finalizeReturn(true);
      }
      const std::wstring inputToUse =
          prompt.empty() ? UnescapeJsonString(ExtractJsonField(jsonBody, L"input"))
                         : prompt;

      std::wstring outputText;
      std::wstring rawResponse;
      int innerStatus = 0;
      if (!SendCodexApiRequestByAuthFile(authPath, modelToUse, inputToUse, outputText,
                                         rawResponse, error, innerStatus))
      {
        ParseTokenUsage(rawResponse, inputTokens, outputTokens, totalTokens);
        while (innerStatus > 0 &&
               handleRetryableProxyFailure(static_cast<DWORD>(innerStatus),
                                           rawResponse, inputTokens, outputTokens,
                                           totalTokens))
        {
          if (SendCodexApiRequestByAuthFile(authPath, modelToUse, inputToUse,
                                            outputText, rawResponse, error,
                                            innerStatus))
          {
            accountUsed = accountName;
            innerStatus = 0;
            break;
          }
          ParseTokenUsage(rawResponse, inputTokens, outputTokens, totalTokens);
        }
        if (innerStatus > 0)
        {
          statusCode = static_cast<DWORD>(innerStatus);
          contentType = L"application/json";
          responseBody = rawResponse.empty()
                             ? "{\"error\":\"" + WideToUtf8(error) + "\"}"
                             : WideToUtf8(rawResponse);
          ParseTokenUsage(rawResponse, inputTokens, outputTokens, totalTokens);
          reqModel = modelToUse;
          return finalizeReturn(true);
        }
        if (outputText.empty())
        {
          return false;
        }
      }

      const long long created = static_cast<long long>(time(nullptr));
      const std::wstring cid = L"chatcmpl_local_" + NewSessionId();
      const std::wstring modelRet = modelToUse;
      std::wstring contentEsc = EscapeJsonString(outputText);
      if (requestStream)
      {
        std::wstringstream sse;
        sse << L"data: {\"id\":\"" << EscapeJsonString(cid)
            << L"\",\"object\":\"chat.completion.chunk\",\"created\":" << created
            << L",\"model\":\"" << EscapeJsonString(modelRet)
            << L"\",\"choices\":[{\"index\":0,\"delta\":{\"role\":\"assistant\"},\"finish_reason\":null}]}\n\n";
        sse << L"data: {\"id\":\"" << EscapeJsonString(cid)
            << L"\",\"object\":\"chat.completion.chunk\",\"created\":" << created
            << L",\"model\":\"" << EscapeJsonString(modelRet)
            << L"\",\"choices\":[{\"index\":0,\"delta\":{\"content\":\""
            << contentEsc << L"\"},\"finish_reason\":null}]}\n\n";
        sse << L"data: {\"id\":\"" << EscapeJsonString(cid)
            << L"\",\"object\":\"chat.completion.chunk\",\"created\":" << created
            << L",\"model\":\"" << EscapeJsonString(modelRet)
            << L"\",\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"stop\"}]}\n\n";
        sse << L"data: [DONE]\n\n";
        statusCode = 200;
        contentType = L"text/event-stream; charset=utf-8";
        responseBody = WideToUtf8(sse.str());
        return finalizeReturn(true);
      }

      std::wstringstream ss;
      ss << L"{\"id\":\"" << EscapeJsonString(cid)
         << L"\",\"object\":\"chat.completion\",\"created\":" << created
         << L",\"model\":\"" << EscapeJsonString(modelRet)
         << L"\",\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\""
         << contentEsc
         << L"\"},\"finish_reason\":\"stop\"}],\"usage\":null}";
      statusCode = 200;
      contentType = L"application/json";
      responseBody = WideToUtf8(ss.str());
      ParseTokenUsage(rawResponse, inputTokens, outputTokens, totalTokens);
      reqModel = modelToUse;
      return finalizeReturn(true);
    }

    if (lowerPath.rfind("/v1/completions", 0) == 0 && methodLower == "post")
    {
      const std::wstring jsonBody = FromUtf8(body);
      const std::wstring requestModel = ExtractJsonField(jsonBody, L"model");
      const bool requestStream = ExtractJsonBoolField(jsonBody, L"stream", false);
      const std::wstring prompt = ExtractOpenAiLegacyPrompt(jsonBody);
      std::wstring modelToUse = requestModel;
      if (modelToUse.empty())
      {
        std::vector<std::wstring> models;
        std::wstring modelErr;
        if (GetCachedModelIds(authPath, models, modelErr) && !models.empty())
        {
          modelToUse = models.front();
        }
      }
      if (modelToUse.empty())
      {
        statusCode = 400;
        contentType = L"application/json";
        responseBody =
            R"({"error":{"message":"model is required","type":"invalid_request_error","code":"model_required"}})";
        return finalizeReturn(true);
      }

      std::wstring outputText;
      std::wstring rawResponse;
      int innerStatus = 0;
      if (!SendCodexApiRequestByAuthFile(authPath, modelToUse, prompt, outputText,
                                         rawResponse, error, innerStatus))
      {
        ParseTokenUsage(rawResponse, inputTokens, outputTokens, totalTokens);
        while (innerStatus > 0 &&
               handleRetryableProxyFailure(static_cast<DWORD>(innerStatus),
                                           rawResponse, inputTokens, outputTokens,
                                           totalTokens))
        {
          if (SendCodexApiRequestByAuthFile(authPath, modelToUse, prompt, outputText,
                                            rawResponse, error, innerStatus))
          {
            accountUsed = accountName;
            innerStatus = 0;
            break;
          }
          ParseTokenUsage(rawResponse, inputTokens, outputTokens, totalTokens);
        }
        if (innerStatus > 0)
        {
          statusCode = static_cast<DWORD>(innerStatus);
          contentType = L"application/json";
          responseBody = rawResponse.empty()
                             ? "{\"error\":\"" + WideToUtf8(error) + "\"}"
                             : WideToUtf8(rawResponse);
          ParseTokenUsage(rawResponse, inputTokens, outputTokens, totalTokens);
          reqModel = modelToUse;
          return finalizeReturn(true);
        }
        if (outputText.empty())
        {
          return false;
        }
      }

      const long long created = static_cast<long long>(time(nullptr));
      const std::wstring cid = L"cmpl_local_" + NewSessionId();
      if (requestStream)
      {
        std::wstringstream sse;
        sse << L"data: {\"id\":\"" << EscapeJsonString(cid)
            << L"\",\"object\":\"text_completion\",\"created\":" << created
            << L",\"model\":\"" << EscapeJsonString(modelToUse)
            << L"\",\"choices\":[{\"text\":\""
            << EscapeJsonForSseDataLine(outputText)
            << L"\",\"index\":0,\"finish_reason\":\"stop\"}]}\n\n";
        sse << L"data: [DONE]\n\n";
        statusCode = 200;
        contentType = L"text/event-stream; charset=utf-8";
        responseBody = WideToUtf8(sse.str());
        return finalizeReturn(true);
      }

      std::wstringstream out;
      out << L"{\"id\":\"" << EscapeJsonString(cid)
          << L"\",\"object\":\"text_completion\",\"created\":" << created
          << L",\"model\":\"" << EscapeJsonString(modelToUse)
          << L"\",\"choices\":[{\"text\":\"" << EscapeJsonString(outputText)
          << L"\",\"index\":0,\"finish_reason\":\"stop\"}],\"usage\":null}";
      statusCode = 200;
      contentType = L"application/json";
      responseBody = WideToUtf8(out.str());
      ParseTokenUsage(rawResponse, inputTokens, outputTokens, totalTokens);
      reqModel = modelToUse;
      return finalizeReturn(true);
    }

    if (lowerPath.rfind("/v1/messages", 0) == 0 && methodLower == "post")
    {
      const std::wstring jsonBody = FromUtf8(body);
      const std::wstring requestModel = ExtractJsonField(jsonBody, L"model");
      const bool requestStream = ExtractJsonBoolField(jsonBody, L"stream", false);
      const std::wstring prompt = ExtractOpenAiUserPrompt(jsonBody);
      std::wstring modelToUse = requestModel;
      if (modelToUse.empty())
      {
        std::vector<std::wstring> models;
        std::wstring modelErr;
        if (GetCachedModelIds(authPath, models, modelErr) && !models.empty())
        {
          modelToUse = models.front();
        }
      }
      if (modelToUse.empty())
      {
        statusCode = 400;
        contentType = L"application/json";
        responseBody =
            R"({"error":{"message":"model is required","type":"invalid_request_error","code":"model_required"}})";
        return finalizeReturn(true);
      }

      std::wstring outputText;
      std::wstring rawResponse;
      int innerStatus = 0;
      if (!SendCodexApiRequestByAuthFile(authPath, modelToUse, prompt, outputText,
                                         rawResponse, error, innerStatus))
      {
        ParseTokenUsage(rawResponse, inputTokens, outputTokens, totalTokens);
        while (innerStatus > 0 &&
               handleRetryableProxyFailure(static_cast<DWORD>(innerStatus),
                                           rawResponse, inputTokens, outputTokens,
                                           totalTokens))
        {
          if (SendCodexApiRequestByAuthFile(authPath, modelToUse, prompt, outputText,
                                            rawResponse, error, innerStatus))
          {
            accountUsed = accountName;
            innerStatus = 0;
            break;
          }
          ParseTokenUsage(rawResponse, inputTokens, outputTokens, totalTokens);
        }
        if (innerStatus > 0)
        {
          statusCode = static_cast<DWORD>(innerStatus);
          contentType = L"application/json";
          responseBody = rawResponse.empty()
                             ? "{\"error\":\"" + WideToUtf8(error) + "\"}"
                             : WideToUtf8(rawResponse);
          ParseTokenUsage(rawResponse, inputTokens, outputTokens, totalTokens);
          reqModel = modelToUse;
          return finalizeReturn(true);
        }
        if (outputText.empty())
        {
          return false;
        }
      }

      const std::wstring msgId = L"msg_local_" + NewSessionId();
      if (requestStream)
      {
        std::wstringstream sse;
        sse << L"event: message_start\n";
        sse << L"data: {\"type\":\"message_start\",\"message\":{\"id\":\""
            << EscapeJsonString(msgId)
            << L"\",\"type\":\"message\",\"role\":\"assistant\",\"model\":\""
            << EscapeJsonString(modelToUse)
            << L"\",\"content\":[],\"stop_reason\":null,\"stop_sequence\":null,\"usage\":{\"input_tokens\":0,\"output_tokens\":0}}}\n\n";
        sse << L"event: content_block_start\n";
        sse << L"data: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"\"}}\n\n";
        sse << L"event: content_block_delta\n";
        sse << L"data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\""
            << EscapeJsonForSseDataLine(outputText) << L"\"}}\n\n";
        sse << L"event: content_block_stop\n";
        sse << L"data: {\"type\":\"content_block_stop\",\"index\":0}\n\n";
        sse << L"event: message_delta\n";
        sse << L"data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\",\"stop_sequence\":null},\"usage\":{\"output_tokens\":0}}\n\n";
        sse << L"event: message_stop\n";
        sse << L"data: {\"type\":\"message_stop\"}\n\n";
        statusCode = 200;
        contentType = L"text/event-stream; charset=utf-8";
        responseBody = WideToUtf8(sse.str());
        return finalizeReturn(true);
      }

      std::wstringstream out;
      out << L"{\"id\":\"" << EscapeJsonString(msgId)
          << L"\",\"type\":\"message\",\"role\":\"assistant\",\"model\":\""
          << EscapeJsonString(modelToUse)
          << L"\",\"content\":[{\"type\":\"text\",\"text\":\""
          << EscapeJsonString(outputText)
          << L"\"}],\"stop_reason\":\"end_turn\",\"stop_sequence\":null,"
          << L"\"usage\":{\"input_tokens\":0,\"output_tokens\":0}}";
      statusCode = 200;
      contentType = L"application/json";
      responseBody = WideToUtf8(out.str());
      ParseTokenUsage(rawResponse, inputTokens, outputTokens, totalTokens);
      reqModel = modelToUse;
      return finalizeReturn(true);
    }

    std::wstring upstreamPath;
    if (lowerPath.rfind("/v1/responses/compact", 0) == 0)
    {
      const std::string suffix =
          path.substr(std::string("/v1/responses/compact").size());
      upstreamPath = L"/backend-api/codex/responses/compact" + FromUtf8(suffix);
    }
    else if (lowerPath.rfind("/v1/responses", 0) == 0)
    {
      const std::string suffix = path.substr(std::string("/v1/responses").size());
      upstreamPath = L"/backend-api/codex/responses" + FromUtf8(suffix);
    }
    else if (lowerPath.rfind("/backend-api/", 0) == 0)
    {
      upstreamPath = FromUtf8(path);
    }
    else
    {
      statusCode = 404;
      contentType = L"application/json";
      responseBody = R"({"error":"unsupported_path"})";
      return finalizeReturn(true);
    }

    while (true)
    {
      if (!SendProxyUpstreamRequestByAuth(authPath, methodLower, upstreamPath,
                                          headers, body, timeoutSec, statusCode,
                                          contentType, responseBody, error))
      {
        return false;
      }

      const std::wstring responseWide = FromUtf8(responseBody);
      ParseTokenUsage(responseWide, inputTokens, outputTokens, totalTokens);
      if (!handleRetryableProxyFailure(statusCode, responseWide, inputTokens,
                                       outputTokens, totalTokens))
      {
        break;
      }
    }
    if (reqModel.empty())
    {
      reqModel = ExtractJsonField(FromUtf8(responseBody), L"model");
    }
    return finalizeReturn(true);
  }

  void HandleProxyClient(SOCKET clientSock, const int timeoutSec)
  {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    std::string body;
    if (!ParseHttpRequestFromSocket(clientSock, method, path, headers, body))
    {
      SendHttpTextResponse(clientSock, 400, "Bad Request",
                           R"({"error":"bad_request"})");
      return;
    }

    const std::string methodLower = ToLowerAscii(method);
    const std::string lowerPath = ToLowerAscii(path);
    const size_t queryPos = lowerPath.find('?');
    const std::string pathOnly =
        queryPos == std::string::npos ? lowerPath : lowerPath.substr(0, queryPos);
    if ((methodLower == "get" || methodLower == "head") &&
        (pathOnly == "/healthz" || pathOnly == "/health" ||
         pathOnly == "/v1/healthz" || pathOnly == "/v1/health" ||
         pathOnly == "/ready" || pathOnly == "/status"))
    {
      const std::wstring bodyW =
          L"{\"ok\":true,\"status\":\"ok\",\"running\":" +
          std::wstring(g_ProxyRunning.load() ? L"true" : L"false") +
          L",\"port\":" + std::to_wstring(g_ProxyPort) + L"}";
      SendHttpTextResponse(clientSock, 200, "OK", WideToUtf8(bodyW));
      return;
    }

    if (!IsProxyRequestAuthorized(headers))
    {
      SendHttpTextResponse(
          clientSock, 401, "Unauthorized",
          R"({"error":{"message":"invalid_api_key","type":"invalid_request_error","code":"invalid_api_key"}})");
      return;
    }

    DWORD statusCode = 0;
    std::wstring contentType;
    std::string upstreamBody;
    std::wstring error;
    TrafficLogEntry trafficMeta;
    if (!ForwardProxyToUpstream(method, path, headers, body, timeoutSec,
                                statusCode, contentType, upstreamBody, error,
                                &trafficMeta))
    {
      TrafficLogEntry failedMeta;
      failedMeta.calledAt =
          std::chrono::duration_cast<std::chrono::seconds>(
              std::chrono::system_clock::now().time_since_epoch())
              .count();
      failedMeta.statusCode = 502;
      failedMeta.method = ToLowerWide(FromUtf8(method));
      failedMeta.model = ExtractJsonField(FromUtf8(body), L"model");
      failedMeta.protocol = lowerPath.rfind("/v1/", 0) == 0 ? L"openai" : L"codex";
      failedMeta.account = L"-";
      failedMeta.path = FromUtf8(path);
      failedMeta.inputTokens = -1;
      failedMeta.outputTokens = -1;
      failedMeta.totalTokens = -1;
      failedMeta.elapsedMs = -1;
      AppendTrafficLog(failedMeta);
      SendHttpTextResponse(clientSock, 502, "Bad Gateway",
                           "{\"error\":\"" + WideToUtf8(error) + "\"}");
      return;
    }
    trafficMeta.statusCode = static_cast<int>(statusCode);
    if (trafficMeta.calledAt <= 0)
    {
      trafficMeta.calledAt =
          std::chrono::duration_cast<std::chrono::seconds>(
              std::chrono::system_clock::now().time_since_epoch())
              .count();
    }
    if (trafficMeta.path.empty())
    {
      trafficMeta.path = FromUtf8(path);
    }
    AppendTrafficLog(trafficMeta);

    std::string reason = "OK";
    if (statusCode >= 400 && statusCode < 500)
    {
      reason = "Client Error";
    }
    else if (statusCode >= 500)
    {
      reason = "Server Error";
    }
    SendHttpTextResponse(clientSock, static_cast<int>(statusCode), reason,
                         upstreamBody,
                         contentType.empty() ? "application/json"
                                             : WideToUtf8(contentType));
  }

  void ProxyServiceLoop(const int timeoutSec)
  {
    while (g_ProxyRunning.load())
    {
      fd_set readfds;
      FD_ZERO(&readfds);
      FD_SET(g_ProxyListenSocket, &readfds);
      timeval tv{};
      tv.tv_sec = 0;
      tv.tv_usec = 200000;
      const int sel = select(0, &readfds, nullptr, nullptr, &tv);
      if (sel <= 0)
      {
        continue;
      }

      sockaddr_in clientAddr{};
      int clientLen = sizeof(clientAddr);
      SOCKET clientSock = accept(g_ProxyListenSocket,
                                 reinterpret_cast<sockaddr *>(&clientAddr),
                                 &clientLen);
      if (clientSock == INVALID_SOCKET)
      {
        continue;
      }
      HandleProxyClient(clientSock, timeoutSec);
      closesocket(clientSock);
    }
  }

  std::wstring BuildProxyStatusJson()
  {
    std::wstringstream ss;
    ss << L"{\"type\":\"proxy_status\",\"running\":"
       << (g_ProxyRunning.load() ? L"true" : L"false") << L",\"port\":"
       << g_ProxyPort << L",\"timeoutSec\":" << g_ProxyTimeoutSec
       << L",\"allowLan\":" << (g_ProxyAllowLan ? L"true" : L"false")
       << L",\"apiKey\":\"" << EscapeJsonString(g_ProxyApiKey) << L"\""
       << L",\"dispatchMode\":\"" << EscapeJsonString(g_ProxyDispatchMode)
       << L"\"" << L",\"fixedAccount\":\""
       << EscapeJsonString(g_ProxyFixedAccount) << L"\"" << L",\"fixedGroup\":\""
       << EscapeJsonString(g_ProxyFixedGroup) << L"\"}";
    return ss.str();
  }

  bool StartLocalProxyService(HWND notifyHwnd, int port, int timeoutSec,
                              bool allowLan, std::wstring &status,
                              std::wstring &code)
  {
    std::lock_guard<std::mutex> lock(g_ProxyMutex);
    if (g_ProxyRunning.load())
    {
      status = L"代理服务已在运行";
      code = L"proxy_already_running";
      return true;
    }

    if (port < 1 || port > 65535)
    {
      status = L"端口无效";
      code = L"proxy_invalid_port";
      return false;
    }

    const int clampedTimeout =
        timeoutSec < 30 ? 30 : (timeoutSec > 7200 ? 7200 : timeoutSec);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
      status = L"启动网络失败";
      code = L"proxy_wsa_startup_failed";
      return false;
    }

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET)
    {
      WSACleanup();
      status = L"创建监听套接字失败";
      code = L"proxy_socket_failed";
      return false;
    }

    int opt = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char *>(&opt),
               sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(allowLan ? INADDR_ANY : INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<u_short>(port));
    if (bind(listenSock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) ==
        SOCKET_ERROR)
    {
      closesocket(listenSock);
      WSACleanup();
      status = L"绑定端口失败，端口可能被占用";
      code = L"proxy_bind_failed";
      return false;
    }
    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR)
    {
      closesocket(listenSock);
      WSACleanup();
      status = L"启动监听失败";
      code = L"proxy_listen_failed";
      return false;
    }

    g_ProxyListenSocket = listenSock;
    g_ProxyPort = port;
    g_ProxyTimeoutSec = clampedTimeout;
    g_ProxyAllowLan = allowLan;
    g_ProxyRunning.store(true);
    g_ProxyThread = std::thread([clampedTimeout]()
                                { ProxyServiceLoop(clampedTimeout); });

    if (notifyHwnd != nullptr)
    {
      PostAsyncWebJson(notifyHwnd, BuildProxyStatusJson());
    }
    status = L"代理服务已启动";
    code = L"proxy_started";
    return true;
  }

  void StopLocalProxyService(std::wstring &status, std::wstring &code)
  {
    std::thread threadToJoin;
    {
      std::lock_guard<std::mutex> lock(g_ProxyMutex);
      if (!g_ProxyRunning.load())
      {
        status = L"代理服务未运行";
        code = L"proxy_not_running";
        return;
      }
      g_ProxyRunning.store(false);
      if (g_ProxyListenSocket != INVALID_SOCKET)
      {
        closesocket(g_ProxyListenSocket);
        g_ProxyListenSocket = INVALID_SOCKET;
      }
      threadToJoin = std::move(g_ProxyThread);
    }
    if (threadToJoin.joinable())
    {
      threadToJoin.join();
    }
    WSACleanup();
    status = L"代理服务已停止";
    code = L"proxy_stopped";
  }

  bool SendCodexApiRequestByAuthFile(const fs::path &authPath,
                                     const std::wstring &model,
                                     const std::wstring &inputText,
                                     std::wstring &outputText,
                                     std::wstring &rawResponse,
                                     std::wstring &error,
                                     int &statusCodeOut)
  {
    outputText.clear();
    rawResponse.clear();
    error.clear();
    statusCodeOut = 0;

    std::wstring accessToken;
    std::wstring accountId;
    if (!ReadAuthTokenInfo(authPath, accessToken, accountId))
    {
      error = L"auth_token_missing";
      return false;
    }

    if (model.empty())
    {
      error = L"model_required";
      statusCodeOut = 400;
      rawResponse =
          L"{\"error\":{\"message\":\"model is required\",\"type\":\"invalid_request_error\",\"code\":\"model_required\"}}";
      return false;
    }
    const std::wstring useModel = model;
    const std::wstring body =
        L"{\"model\":\"" + EscapeJsonString(useModel) +
        L"\",\"stream\":true,\"store\":false,\"instructions\":\"You are Codex.\",\"input\":[{\"type\":"
        L"\"message\",\"role\":\"user\",\"content\":[{\"type\":\"input_text\","
        L"\"text\":\"" +
        EscapeJsonString(inputText) +
        L"\"}]}]}";
    const std::string bodyUtf8 = WideToUtf8(body);

    HINTERNET hSession = WinHttpOpen(
        BuildCodexApiUserAgent().c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (hSession == nullptr)
    {
      error = L"WinHttpOpen_failed";
      return false;
    }

    HINTERNET hConnect =
        WinHttpConnect(hSession, kUsageHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (hConnect == nullptr)
    {
      WinHttpCloseHandle(hSession);
      error = L"WinHttpConnect_failed";
      return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, L"POST", kCodexApiPathCompact, nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (hRequest == nullptr)
    {
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      error = L"WinHttpOpenRequest_failed";
      return false;
    }

    const std::wstring headers =
        L"Content-Type: application/json\r\nAccept: text/event-stream\r\n"
        L"Accept-Encoding: identity\r\nCache-Control: no-cache\r\n"
        L"Authorization: Bearer " +
        accessToken + L"\r\nChatGPT-Account-Id: " + accountId +
        L"\r\nVersion: " + ReadCodexLatestVersion() +
        L"\r\nOpenai-Beta: responses=experimental\r\nSession_id: " +
        NewSessionId() + L"\r\nOriginator: codex_cli_rs\r\nUser-Agent: " +
        BuildCodexApiUserAgent() + L"\r\nConnection: Keep-Alive\r\n";

    BOOL ok = WinHttpSendRequest(
        hRequest, headers.c_str(), static_cast<DWORD>(-1L),
        const_cast<char *>(bodyUtf8.data()), static_cast<DWORD>(bodyUtf8.size()),
        static_cast<DWORD>(bodyUtf8.size()), 0);
    if (ok)
    {
      ok = WinHttpReceiveResponse(hRequest, nullptr);
    }
    if (!ok)
    {
      error = L"http_transport_failed";
      WinHttpCloseHandle(hRequest);
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      return false;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest,
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize,
                        WINHTTP_NO_HEADER_INDEX);
    statusCodeOut = static_cast<int>(statusCode);

    std::string payload;
    while (true)
    {
      char readBuf[2048]{};
      DWORD read = 0;
      if (!WinHttpReadData(hRequest, readBuf, sizeof(readBuf), &read))
      {
        break;
      }
      if (read == 0)
      {
        break;
      }
      std::string chunk(readBuf, readBuf + read);
      payload += chunk;
    }

    rawResponse = FromUtf8(payload);
    if (statusCode < 200 || statusCode >= 300)
    {
      error = L"http_status_" + std::to_wstring(statusCode);
      WinHttpCloseHandle(hRequest);
      WinHttpCloseHandle(hConnect);
      WinHttpCloseHandle(hSession);
      return false;
    }

    std::wstring contentType =
        QueryWinHttpHeaderText(hRequest, WINHTTP_QUERY_CONTENT_TYPE);
    const std::wstring contentTypeLower = ToLowerCopy(contentType);
    const bool looksLikeSse =
        rawResponse.find(L"event: ") != std::wstring::npos &&
        rawResponse.find(L"data: ") != std::wstring::npos;

    if (contentTypeLower.find(L"text/event-stream") != std::wstring::npos ||
        looksLikeSse)
    {
      outputText = ExtractSseOutputText(rawResponse);
    }
    if (outputText.empty())
    {
      outputText = ExtractResponseOutputText(rawResponse);
    }
    if (outputText.empty())
    {
      outputText = rawResponse;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
  }

  std::wstring BuildAccountsListJson(const bool refreshUsage,
                                     const std::wstring &targetName,
                                     const std::wstring &targetGroup)
  {
    const std::vector<AccountEntry> accounts =
        CollectAccounts(refreshUsage, targetName, targetGroup);
    std::wstringstream ss;
    ss << L"{\"type\":\"accounts_list\",\"accounts\":[";
    for (size_t i = 0; i < accounts.size(); ++i)
    {
      const auto &item = accounts[i];
      if (i != 0)
      {
        ss << L",";
      }
      ss << L"{\"name\":\"" << EscapeJsonString(item.name) << L"\",\"group\":\""
         << EscapeJsonString(item.group) << L"\",\"updatedAt\":\""
         << EscapeJsonString(item.updatedAt) << L"\",\"isCurrent\":"
         << (item.isCurrent ? L"true" : L"false") << L",\"abnormal\":"
         << (item.abnormal ? L"true" : L"false") << L",\"abnormalReason\":\""
         << EscapeJsonString(item.abnormalReason) << L"\",\"abnormalAt\":\""
         << EscapeJsonString(item.abnormalAt) << L"\",\"usageOk\":"
         << (item.usageOk ? L"true" : L"false") << L",\"usageError\":\""
         << EscapeJsonString(item.usageError) << L"\",\"planType\":\""
         << EscapeJsonString(item.planType) << L"\",\"email\":\""
         << EscapeJsonString(item.email) << L"\",\"quota5hRemainingPercent\":"
         << item.quota5hRemainingPercent << L",\"quota7dRemainingPercent\":"
         << item.quota7dRemainingPercent << L",\"quota5hResetAfterSeconds\":"
         << item.quota5hResetAfterSeconds << L",\"quota7dResetAfterSeconds\":"
         << item.quota7dResetAfterSeconds << L",\"quota5hResetAt\":"
         << item.quota5hResetAt << L",\"quota7dResetAt\":" << item.quota7dResetAt
         << L"}";
    }
    ss << L"]}";
    return ss.str();
  }

  void PostAsyncWebJson(const HWND hwnd, const std::wstring &json)
  {
    std::wstring *heapJson = new std::wstring(json);
    if (!PostMessageW(hwnd, WebViewHost::kMsgAsyncWebJson, 0,
                      reinterpret_cast<LPARAM>(heapJson)))
    {
      delete heapJson;
    }
  }

  void SendWebStatusThreadSafe(const HWND hwnd, const std::wstring &text,
                               const std::wstring &level,
                               const std::wstring &code)
  {
    std::wstring json = L"{\"type\":\"status\",\"level\":\"" +
                        EscapeJsonString(level) + L"\",\"code\":\"" +
                        EscapeJsonString(code) + L"\",\"message\":\"" +
                        EscapeJsonString(text) + L"\"}";
    PostAsyncWebJson(hwnd, json);
  }

  struct LowQuotaCandidatePayload
  {
    std::wstring currentName;
    int currentQuota = -1;
    std::wstring currentWindow;
    std::wstring bestName;
    std::wstring bestGroup;
    int bestQuota = -1;
    std::wstring accountKey;
  };

  void PostLowQuotaCandidate(const HWND hwnd,
                             LowQuotaCandidatePayload &&payload)
  {
    auto *heapPayload = new LowQuotaCandidatePayload(std::move(payload));
    if (!PostMessageW(hwnd, WebViewHost::kMsgLowQuotaCandidate, 0,
                      reinterpret_cast<LPARAM>(heapPayload)))
    {
      delete heapPayload;
    }
  }
} // namespace

void WebViewHost::ShowHr(HWND hwnd, const wchar_t *where, const HRESULT hr)
{
  const wchar_t *errorPoint =
      (where != nullptr && where[0] != L'\0') ? where : L"WebView2 initialization";

  wchar_t detail[256]{};
  swprintf_s(detail, L"%s failed. HRESULT=0x%08X", errorPoint,
             static_cast<unsigned>(hr));

  if (IsMissingWebView2RuntimeError(hr))
  {
    std::wstring message =
        L"Microsoft Edge WebView2 Runtime is required but was not found.\n\n"
        L"Please install WebView2 Runtime and then restart the application.\n\n"
        L"Open the official Microsoft download page now?\n\n";
    message += detail;

    const int result =
        MessageBoxW(hwnd, message.c_str(), L"Missing WebView2 Runtime",
                    MB_ICONERROR | MB_YESNO | MB_SETFOREGROUND);
    if (result == IDYES && !OpenExternalUrlByExplorer(kWebView2DownloadUrl))
    {
      const std::wstring fallback =
          L"Unable to open the download page automatically.\nPlease visit:\n" +
          std::wstring(kWebView2DownloadUrl);
      MessageBoxW(hwnd, fallback.c_str(), L"Open Download Page Failed",
                  MB_ICONERROR | MB_SETFOREGROUND);
    }

    RequestMainWindowExit(hwnd);
    return;
  }

  MessageBoxW(hwnd, detail, L"WebView2 Error", MB_ICONERROR | MB_SETFOREGROUND);
  RequestMainWindowExit(hwnd);
}

void WebViewHost::RegisterWebMessageHandler(HWND hwnd)
{
  webview_->add_WebMessageReceived(
      Callback<ICoreWebView2WebMessageReceivedEventHandler>(
          [this,
           hwnd](ICoreWebView2 *,
                 ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT
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
              const HRESULT stringHr =
                  args->TryGetWebMessageAsString(&rawMessage);
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

void WebViewHost::SendWebStatus(const std::wstring &text,
                                const std::wstring &level,
                                const std::wstring &code) const
{
  SendWebJson(L"{\"type\":\"status\",\"level\":\"" + EscapeJsonString(level) +
              L"\",\"code\":\"" + EscapeJsonString(code) +
              L"\",\"message\":\"" + EscapeJsonString(text) + L"\"}");
}

void WebViewHost::SendAccountsList(const bool refreshUsage,
                                   const std::wstring &targetName,
                                   const std::wstring &targetGroup) const
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
  SendWebJson(L"{\"type\":\"app_info\",\"version\":\"" +
              EscapeJsonString(kAppVersion) + L"\",\"debug\":" + debugValue +
              L",\"repo\":\"https://github.com/isxlan0/Codex_AccountSwitch\"}");
}

void WebViewHost::SendUpdateInfo() const
{
  const HWND targetHwnd = hwnd_;
  std::thread([targetHwnd]()
              {
    const UpdateCheckResult check = CheckGitHubUpdate(kAppVersion);
    std::wstring json;
    if (!check.ok) {
      json =
          L"{\"type\":\"update_info\",\"ok\":false,\"current\":\"" +
          EscapeJsonString(kAppVersion) +
          L"\",\"latest\":\"\",\"hasUpdate\":false,\"url\":\"https://"
          L"github.com/isxlan0/Codex_AccountSwitch/releases/"
          L"latest\",\"downloadUrl\":\"https://github.com/isxlan0/"
          L"Codex_AccountSwitch/releases/latest\",\"notes\":\"\",\"error\":\"" +
          EscapeJsonString(check.errorMessage) + L"\"}";
    } else {
      json = L"{\"type\":\"update_info\",\"ok\":true,\"current\":\"" +
             EscapeJsonString(check.currentVersion) + L"\",\"latest\":\"" +
             EscapeJsonString(check.latestVersion) + L"\",\"hasUpdate\":" +
             std::wstring(check.hasUpdate ? L"true" : L"false") +
             L",\"url\":\"" + EscapeJsonString(check.releaseUrl) +
             L"\",\"downloadUrl\":\"" + EscapeJsonString(check.downloadUrl) +
             L"\",\"notes\":\"" + EscapeJsonString(check.releaseNotes) + L"\"}";
    }

    std::wstring *heapJson = new std::wstring(std::move(json));
    if (!PostMessageW(targetHwnd, WebViewHost::kMsgAsyncWebJson, 0,
                      reinterpret_cast<LPARAM>(heapJson))) {
      delete heapJson;
    } })
      .detach();
}

void WebViewHost::SendLanguageIndex() const
{
  const auto langs = LoadLanguageIndexList();
  std::wstringstream ss;
  ss << L"{\"type\":\"language_index\",\"languages\":[";
  for (size_t i = 0; i < langs.size(); ++i)
  {
    if (i != 0)
      ss << L",";
    ss << L"{\"code\":\"" << EscapeJsonString(langs[i].code)
       << L"\",\"name\":\"" << EscapeJsonString(langs[i].name)
       << L"\",\"file\":\"" << EscapeJsonString(langs[i].file) << L"\"}";
  }
  ss << L"]}";
  SendWebJson(ss.str());
}

void WebViewHost::SendLanguagePack(const std::wstring &languageCode) const
{
  std::vector<std::pair<std::wstring, std::wstring>> pairs;
  std::wstring resolvedCode;
  if (!LoadLanguageStrings(languageCode, pairs, resolvedCode))
  {
    SendWebJson(L"{\"type\":\"language_pack\",\"ok\":false}");
    return;
  }

  std::wstringstream ss;
  ss << L"{\"type\":\"language_pack\",\"ok\":true,\"code\":\""
     << EscapeJsonString(resolvedCode) << L"\",\"strings\":{";
  for (size_t i = 0; i < pairs.size(); ++i)
  {
    if (i != 0)
      ss << L",";
    ss << L"\"" << EscapeJsonString(pairs[i].first) << L"\":\""
       << EscapeJsonString(pairs[i].second) << L"\"";
  }
  ss << L"}}";
  SendWebJson(ss.str());
}

void WebViewHost::SendConfig(bool firstRun) const
{
  AppConfig cfg;
  LoadConfig(cfg);
  SendWebJson(
      L"{\"type\":\"config\",\"firstRun\":" +
      std::wstring(firstRun ? L"true" : L"false") + L",\"language\":\"" +
      EscapeJsonString(cfg.language) + L"\",\"languageIndex\":" +
      std::to_wstring(cfg.languageIndex) + L",\"ideExe\":\"" +
      EscapeJsonString(cfg.ideExe) + L"\",\"theme\":\"" +
      EscapeJsonString(cfg.theme) + L"\",\"tabVisibility\":" +
      BuildTabVisibilityJson(cfg.tabVisibility) + L",\"autoUpdate\":" +
      std::wstring(cfg.autoUpdate ? L"true" : L"false") +
      L",\"enableAutoRefreshQuota\":" +
      std::wstring(cfg.enableAutoRefreshQuota ? L"true" : L"false") +
      L",\"disableAutoRefreshQuota\":" +
      std::wstring(cfg.enableAutoRefreshQuota ? L"false" : L"true") +
      L",\"autoMarkAbnormalAccounts\":" +
      std::wstring(cfg.autoMarkAbnormalAccounts ? L"true" : L"false") +
      L",\"autoDeleteAbnormalAccounts\":" +
      std::wstring(cfg.autoDeleteAbnormalAccounts ? L"true" : L"false") +
      L",\"autoRefreshAll\":true" + L",\"autoRefreshCurrent\":" +
      std::wstring(cfg.autoRefreshCurrent ? L"true" : L"false") +
      L",\"lowQuotaAutoPrompt\":" +
      std::wstring(cfg.lowQuotaAutoPrompt ? L"true" : L"false") +
      L",\"closeWindowBehavior\":\"" +
      EscapeJsonString(cfg.closeWindowBehavior) + L"\"" +
      L",\"autoRefreshAllMinutes\":" +
      std::to_wstring(cfg.autoRefreshAllMinutes) +
      L",\"autoRefreshCurrentMinutes\":" +
      std::to_wstring(cfg.autoRefreshCurrentMinutes) + L",\"proxyPort\":" +
      std::to_wstring(cfg.proxyPort) + L",\"proxyTimeoutSec\":" +
      std::to_wstring(cfg.proxyTimeoutSec) + L",\"proxyAllowLan\":" +
      std::wstring(cfg.proxyAllowLan ? L"true" : L"false") +
      L",\"proxyAutoStart\":" +
      std::wstring(cfg.proxyAutoStart ? L"true" : L"false") +
      L",\"proxyStealthMode\":" +
      std::wstring(cfg.proxyStealthMode ? L"true" : L"false") +
      L",\"proxyApiKey\":\"" + EscapeJsonString(cfg.proxyApiKey) + L"\"" +
      L",\"proxyDispatchMode\":\"" +
      EscapeJsonString(cfg.proxyDispatchMode) + L"\"" +
      L",\"proxyFixedAccount\":\"" + EscapeJsonString(cfg.proxyFixedAccount) +
      L"\"" + L",\"proxyFixedGroup\":\"" +
      EscapeJsonString(cfg.proxyFixedGroup) + L"\"" +
      L",\"lastSwitchedAccount\":\"" +
      EscapeJsonString(cfg.lastSwitchedAccount) +
      L"\",\"lastSwitchedGroup\":\"" + EscapeJsonString(cfg.lastSwitchedGroup) +
      L"\",\"lastSwitchedAt\":\"" + EscapeJsonString(cfg.lastSwitchedAt) +
      L"\",\"cloudAccountUrl\":\"" +
      EscapeJsonString(cfg.cloudAccountUrl) +
      L"\",\"cloudAccountAutoDownload\":" +
      std::wstring(cfg.cloudAccountAutoDownload ? L"true" : L"false") +
      L",\"cloudAccountIntervalMinutes\":" +
      std::to_wstring(cfg.cloudAccountIntervalMinutes) +
      L",\"cloudAccountLastDownloadAt\":\"" +
      EscapeJsonString(cfg.cloudAccountLastDownloadAt) +
      L"\",\"cloudAccountLastDownloadStatus\":\"" +
      EscapeJsonString(cfg.cloudAccountLastDownloadStatus) +
      L"\",\"cloudAccountPasswordConfigured\":" +
      std::wstring(cfg.cloudAccountPasswordConfigured ? L"true" : L"false") +
      L",\"webdavEnabled\":" +
      std::wstring(cfg.webdavEnabled ? L"true" : L"false") +
      L",\"webdavAutoSync\":" +
      std::wstring(cfg.webdavAutoSync ? L"true" : L"false") +
      L",\"webdavSyncIntervalMinutes\":" +
      std::to_wstring(cfg.webdavSyncIntervalMinutes) +
      L",\"webdavUrl\":\"" + EscapeJsonString(cfg.webdavUrl) +
      L"\",\"webdavRemotePath\":\"" +
      EscapeJsonString(cfg.webdavRemotePath) +
      L"\",\"webdavUsername\":\"" + EscapeJsonString(cfg.webdavUsername) +
      L"\",\"webdavLastSyncAt\":\"" +
      EscapeJsonString(cfg.webdavLastSyncAt) +
      L"\",\"webdavLastSyncStatus\":\"" +
      EscapeJsonString(cfg.webdavLastSyncStatus) +
      L"\",\"webdavPasswordConfigured\":" +
      std::wstring(cfg.webdavPasswordConfigured ? L"true" : L"false") +
      L"}");
}

void WebViewHost::SendRefreshTimerState() const
{
  const int allRemain =
      allRefreshRemainingSec_ < 0 ? 0 : allRefreshRemainingSec_;
  const int currentRemain =
      currentRefreshRemainingSec_ < 0 ? 0 : currentRefreshRemainingSec_;
  SendWebJson(
      L"{\"type\":\"refresh_timers\",\"allIntervalSec\":" +
      std::to_wstring(allRefreshIntervalSec_) + L",\"currentIntervalSec\":" +
      std::to_wstring(currentRefreshIntervalSec_) + L",\"enableAutoRefreshQuota\":" +
      std::wstring(autoRefreshQuotaDisabled_ ? L"false" : L"true") +
      L",\"disableAutoRefreshQuota\":" +
      std::wstring(autoRefreshQuotaDisabled_ ? L"true" : L"false") +
      L",\"allEnabled\":" +
      std::wstring(autoRefreshQuotaDisabled_ ? L"false" : L"true") +
      L",\"autoRefreshQuotaDisabled\":" +
      std::wstring(autoRefreshQuotaDisabled_ ? L"true" : L"false") +
      L",\"currentEnabled\":" +
      std::wstring(currentAutoRefreshEnabled_ ? L"true" : L"false") +
      L",\"allRemainingSec\":" + std::to_wstring(allRemain) +
      L",\"currentRemainingSec\":" + std::to_wstring(currentRemain) + L"}");
}

void WebViewHost::SendWebDavSyncState() const
{
  AppConfig cfg;
  LoadConfig(cfg);
  SendWebJson(BuildWebDavSyncStateJson(cfg, webDavSyncRunning_.load(),
                                       webDavSyncRemainingSec_));
}

void WebViewHost::SendCloudAccountState() const
{
  AppConfig cfg;
  LoadConfig(cfg);
  SendWebJson(BuildCloudAccountStateJson(cfg, cloudAccountDownloadRunning_.load(),
                                         cloudAccountRemainingSec_));
}

std::wstring BuildTrayQuotaText(const IndexEntry &row)
{
  const std::wstring na = Tr(L"tray.quota_na", L"--");
  if (!row.quotaUsageOk)
  {
    return TrFormat(L"tray.quota_format", L"5H {q5} | 7D {q7}",
                    {{L"q5", na}, {L"q7", na}});
  }
  const std::wstring q5 = row.quota5hRemainingPercent >= 0
                              ? (std::to_wstring(row.quota5hRemainingPercent) + L"%")
                              : na;
  const std::wstring q7 = row.quota7dRemainingPercent >= 0
                              ? (std::to_wstring(row.quota7dRemainingPercent) + L"%")
                              : na;
  return TrFormat(L"tray.quota_format", L"5H {q5} | 7D {q7}",
                  {{L"q5", q5}, {L"q7", q7}});
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
  const int iconW = GetSystemMetrics(SM_CXSMICON);
  const int iconH = GetSystemMetrics(SM_CYSMICON);
  nid.hIcon = static_cast<HICON>(
      LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON),
                 IMAGE_ICON, iconW, iconH, LR_DEFAULTCOLOR | LR_SHARED));
  if (nid.hIcon == nullptr)
  {
    nid.hIcon = LoadIconW(nullptr, IDI_INFORMATION);
  }
  const std::wstring trayTip = Tr(L"app.brand", L"Codex Account Switch");
  wcsncpy_s(nid.szTip, trayTip.c_str(), _TRUNCATE);
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
  NOTIFYICONDATAW clearNid{};
  clearNid.cbSize = sizeof(clearNid);
  clearNid.hWnd = hwnd_;
  clearNid.uID = 1;
  clearNid.uFlags = NIF_INFO;
  clearNid.dwInfoFlags = NIIF_NONE;
  clearNid.szInfoTitle[0] = L'\0';
  clearNid.szInfo[0] = L'\0';
  Shell_NotifyIconW(NIM_MODIFY, &clearNid);

  NOTIFYICONDATAW nid{};
  nid.cbSize = sizeof(nid);
  nid.hWnd = hwnd_;
  nid.uID = 1;
  Shell_NotifyIconW(NIM_DELETE, &nid);
  trayIconAdded_ = false;
}

void WebViewHost::ShowTrayContextMenu()
{
  if (hwnd_ == nullptr)
  {
    return;
  }
  EnsureTrayIcon();

  HMENU menu = CreatePopupMenu();
  HMENU switchMenu = CreatePopupMenu();
  if (menu == nullptr || switchMenu == nullptr)
  {
    if (switchMenu != nullptr)
    {
      DestroyMenu(switchMenu);
    }
    if (menu != nullptr)
    {
      DestroyMenu(menu);
    }
    return;
  }

  traySwitchTargets_.clear();
  const std::wstring trayOpen =
      Tr(L"tray.menu.open_window", L"打开页面");
  const std::wstring trayMinimize =
      Tr(L"tray.menu.minimize_to_tray", L"最小化到托盘");
  const std::wstring trayExit = Tr(L"tray.menu.exit", L"退出程序");
  const std::wstring traySwitch =
      Tr(L"tray.menu.quick_switch", L"快速切换账号");
  const std::wstring trayNoSwitch =
      Tr(L"tray.menu.no_switchable", L"暂无可切换账号");
  AppendMenuW(menu, MF_STRING, kTrayCmdOpenWindow, trayOpen.c_str());
  AppendMenuW(menu, MF_STRING, kTrayCmdMinimizeToTray, trayMinimize.c_str());
  AppendMenuW(menu, MF_STRING, kTrayCmdExitApp, trayExit.c_str());
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

  IndexData idx;
  LoadIndex(idx);
  std::vector<IndexEntry> menuRows;
  menuRows.reserve(idx.accounts.size());
  for (const auto &row : idx.accounts)
  {
    const fs::path authPath = ResolveAuthPathFromIndex(row);
    if (fs::exists(authPath))
    {
      menuRows.push_back(row);
    }
  }
  std::sort(menuRows.begin(), menuRows.end(),
            [&](const IndexEntry &a, const IndexEntry &b)
            {
              const bool aCurrent =
                  EqualsIgnoreCase(a.name, idx.currentName) &&
                  NormalizeGroup(a.group) == NormalizeGroup(idx.currentGroup);
              const bool bCurrent =
                  EqualsIgnoreCase(b.name, idx.currentName) &&
                  NormalizeGroup(b.group) == NormalizeGroup(idx.currentGroup);
              if (aCurrent != bCurrent)
              {
                return aCurrent;
              }
              return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
            });

  UINT cmdId = kTrayCmdSwitchBase;
  for (const auto &row : menuRows)
  {
    if (cmdId > kTrayCmdSwitchMax)
    {
      break;
    }
    const bool isCurrent =
        EqualsIgnoreCase(row.name, idx.currentName) &&
        NormalizeGroup(row.group) == NormalizeGroup(idx.currentGroup);
    const std::wstring label =
        (isCurrent ? L"* " : L"") + row.name + L" [" + BuildTrayQuotaText(row) +
        L"]";
    UINT flags = MF_STRING;
    if (isCurrent)
    {
      flags |= MF_CHECKED;
    }
    AppendMenuW(switchMenu, flags, cmdId, label.c_str());
    traySwitchTargets_.push_back({row.name, NormalizeGroup(row.group)});
    ++cmdId;
  }
  if (traySwitchTargets_.empty())
  {
    AppendMenuW(switchMenu, MF_STRING | MF_GRAYED, 0, trayNoSwitch.c_str());
  }

  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(switchMenu),
              traySwitch.c_str());

  POINT pt{};
  GetCursorPos(&pt);
  SetForegroundWindow(hwnd_);
  const UINT picked = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD,
                                     pt.x, pt.y, 0, hwnd_, nullptr);
  if (picked != 0)
  {
    HandleTrayCommand(picked);
  }
  PostMessageW(hwnd_, WM_NULL, 0, 0);
  DestroyMenu(menu);
}

bool WebViewHost::HandleTrayCommand(const UINT commandId)
{
  if (hwnd_ == nullptr)
  {
    return false;
  }
  if (commandId == kTrayCmdOpenWindow)
  {
    ShowWindow(hwnd_, IsIconic(hwnd_) ? SW_RESTORE : SW_SHOW);
    SetForegroundWindow(hwnd_);
    return true;
  }
  if (commandId == kTrayCmdMinimizeToTray)
  {
    ShowWindow(hwnd_, SW_HIDE);
    return true;
  }
  if (commandId == kTrayCmdExitApp)
  {
    SetPropW(hwnd_, kExitWindowPropName, reinterpret_cast<HANDLE>(1));
    PostMessageW(hwnd_, WM_CLOSE, 0, 0);
    return true;
  }
  if (commandId >= kTrayCmdSwitchBase && commandId <= kTrayCmdSwitchMax)
  {
    const size_t idx = static_cast<size_t>(commandId - kTrayCmdSwitchBase);
    if (idx >= traySwitchTargets_.size())
    {
      return true;
    }
    const std::wstring targetName = traySwitchTargets_[idx].first;
    const std::wstring targetGroup = traySwitchTargets_[idx].second;
    const HWND targetHwnd = hwnd_;
    std::thread([targetHwnd, targetName, targetGroup]()
                {
      std::wstring status;
      std::wstring code;
      const bool ok = SwitchToAccount(targetName, targetGroup, status, code);
      if (ok) {
        // Tray quick-switch should not use generic switch_success mapped text.
        SendWebStatusThreadSafe(
            targetHwnd,
            TrFormat(L"status_text.tray_switched_account",
                     L"托盘已切换到账号 {account}", {{L"account", targetName}}),
            L"success", L"");
        PostAsyncWebJson(targetHwnd, BuildAccountsListJson(false, L"", L""));
      } else {
        SendWebStatusThreadSafe(targetHwnd, status, L"error", code);
      } })
        .detach();
    return true;
  }
  return false;
}

void WebViewHost::ShowLowQuotaBalloon(const std::wstring &currentName,
                                      const int currentQuota,
                                      const std::wstring &currentWindow,
                                      const std::wstring &bestName,
                                      const std::wstring &bestGroup,
                                      const int bestQuota)
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
  pendingCurrentQuotaWindow_ = currentWindow;
  pendingCurrentQuota_ = currentQuota;
  pendingBestQuota_ = bestQuota;

  std::wstring title = Tr(L"tray.notify_title", L"Codex额度提醒");
  std::wstring windowText =
      currentWindow.empty()
          ? Tr(L"tray.low_quota_window_default", L"额度")
          : TrFormat(L"tray.low_quota_window_suffix", L"{window}额度",
                     {{L"window", currentWindow}});
  std::wstring message = TrFormat(
      L"tray.low_quota_current",
      L"当前账号 {account} 的{window}已降至 {quota}%",
      {{L"account", currentName},
       {L"window", windowText},
       {L"quota", std::to_wstring(currentQuota)}});
  if (!bestName.empty() && bestQuota >= 0)
  {
    message += TrFormat(L"tray.low_quota_switch_suggest",
                        L"，可切换到 {account}（{quota}%）",
                        {{L"account", bestName},
                         {L"quota", std::to_wstring(bestQuota)}});
  }
  message += Tr(L"tray.low_quota_click_hint",
                L"。点击此通知可选择是否切换并重启IDE。");

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
  ss << L"{\"type\":\"low_quota_prompt\"" << L",\"currentName\":\""
     << EscapeJsonString(pendingCurrentAccountName_) << L"\""
     << L",\"currentQuota\":" << pendingCurrentQuota_
     << L",\"currentWindow\":\"" << EscapeJsonString(pendingCurrentQuotaWindow_)
     << L"\",\"bestName\":\""
     << EscapeJsonString(pendingBestAccountName_) << L"\""
     << L",\"bestGroup\":\"" << EscapeJsonString(pendingBestAccountGroup_)
     << L"\"" << L",\"bestQuota\":" << pendingBestQuota_ << L"}";
  SendWebJson(ss.str());
}

void WebViewHost::TriggerRefreshAll(const bool notifyStatus)
{
  if (allRefreshRunning_.exchange(true))
  {
    if (notifyStatus && hwnd_ != nullptr && IsWindow(hwnd_))
    {
      PostAsyncWebJson(
          hwnd_,
          L"{\"type\":\"status\",\"level\":\"warning\",\"code\":"
          L"\"quota_refresh_running\",\"message\":\"额度刷新正在进行中，请稍候…\"}");
    }
    return;
  }
  allRefreshRemainingSec_ = allRefreshIntervalSec_;
  SendRefreshTimerState();

  const HWND targetHwnd = hwnd_;
  const bool checkLowQuota = lowQuotaPromptEnabled_;
  auto *runningFlag = &allRefreshRunning_;
  std::thread([targetHwnd, notifyStatus, checkLowQuota, runningFlag]()
              {
    std::wstring refreshError;
    try {
      EnsureIndexExists();
      IndexData idx;
      LoadIndex(idx);

      std::vector<std::pair<std::wstring, std::wstring>> targets;
      targets.reserve(idx.accounts.size());
      for (const auto &row : idx.accounts) {
        const fs::path authPath = ResolveAuthPathFromIndex(row);
        if (!fs::exists(authPath)) {
          continue;
        }
        targets.emplace_back(row.name, NormalizeGroup(row.group));
      }

      std::wstring latestJson;
      if (targets.empty()) {
        latestJson = BuildAccountsListJson(false, L"", L"");
        if (!latestJson.empty()) {
          PostAsyncWebJson(targetHwnd, latestJson);
        }
      } else {
        for (size_t i = 0; i < targets.size(); ++i) {
          const auto &item = targets[i];
          if (targetHwnd != nullptr && IsWindow(targetHwnd)) {
            PostAsyncWebJson(
                targetHwnd,
                BuildProgressStatusJson(L"quota_refresh_progress",
                                        static_cast<int>(i + 1),
                                        static_cast<int>(targets.size()),
                                        L"all"));
          }
          latestJson = BuildAccountsListJson(true, item.first, item.second);
          if (!latestJson.empty()) {
            // Push incremental result immediately so UI updates account rows progressively.
            PostAsyncWebJson(targetHwnd, latestJson);
          }
          if (i + 1 < targets.size()) {
            Sleep(ComputeThrottleDelayMs(kRefreshAllThrottleMs));
          }
        }
        if (latestJson.empty()) {
          latestJson = BuildAccountsListJson(false, L"", L"");
          if (!latestJson.empty()) {
            PostAsyncWebJson(targetHwnd, latestJson);
          }
        }
      }
      PostAsyncWebJson(
          targetHwnd,
          L"{\"type\":\"status\",\"level\":\"success\",\"code\":"
          L"\"quota_refreshed\",\"message\":\"\",\"silent\":" +
              std::wstring(notifyStatus ? L"false" : L"true") + L"}");

      const std::vector<AccountEntry> accounts = CollectAccounts(false, L"", L"");
      const AccountEntry *current = FindCurrentAccountEntry(accounts);
      bool useWeeklyWindow = false;
      const int currentQuota =
          (current != nullptr) ? GetLowQuotaMetricValue(*current, useWeeklyWindow)
                               : -1;
      if (checkLowQuota && current != nullptr && current->usageOk &&
          currentQuota >= 0 && currentQuota <= kLowQuotaThresholdPercent) {
        int bestQuota = -1;
        const AccountEntry *best = FindBestSwitchCandidateByMetric(
            accounts, current, useWeeklyWindow, bestQuota);
        const std::wstring currentKey =
            NormalizeGroup(current->group) + L"::" + current->name;
        if (best != nullptr) {
          LowQuotaCandidatePayload payload;
          payload.currentName = current->name;
          payload.currentQuota = currentQuota;
          payload.currentWindow = useWeeklyWindow ? L"7天" : L"5小时";
          payload.bestName = best->name;
          payload.bestGroup = best->group;
          payload.bestQuota = bestQuota;
          payload.accountKey = currentKey;
          PostLowQuotaCandidate(targetHwnd, std::move(payload));
        }
      }
    } catch (...) {
      refreshError = L"刷新全部额度失败，请稍后重试";
    }

    runningFlag->store(false);
    if (!refreshError.empty()) {
      PostAsyncWebJson(targetHwnd,
                       L"{\"type\":\"status\",\"level\":\"error\",\"code\":"
                       L"\"quota_refresh_failed\",\"message\":\"" +
                           EscapeJsonString(refreshError) +
                           L"\",\"silent\":" +
                           std::wstring(notifyStatus ? L"false" : L"true") +
                           L"}");
    } })
      .detach();
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
  auto *runningFlag = &currentRefreshRunning_;

  if (account.empty())
  {
    runningFlag->store(false);
    SendRefreshTimerState();
    return;
  }
  std::thread([targetHwnd, account, group, checkLowQuota, runningFlag]()
              {
    PostAsyncWebJson(targetHwnd, BuildAccountsListJson(true, account, group));
    const std::vector<AccountEntry> accounts = CollectAccounts(false, L"", L"");
    const AccountEntry *current = FindCurrentAccountEntry(accounts);
    bool useWeeklyWindow = false;
    const int currentQuota =
        (current != nullptr) ? GetLowQuotaMetricValue(*current, useWeeklyWindow)
                             : -1;
    if (checkLowQuota && current != nullptr && current->usageOk &&
        currentQuota >= 0 && currentQuota <= kLowQuotaThresholdPercent) {
      int bestQuota = -1;
      const AccountEntry *best = FindBestSwitchCandidateByMetric(
          accounts, current, useWeeklyWindow, bestQuota);
      const std::wstring currentKey =
          NormalizeGroup(current->group) + L"::" + current->name;
      if (best != nullptr) {
        LowQuotaCandidatePayload payload;
        payload.currentName = current->name;
        payload.currentQuota = currentQuota;
        payload.currentWindow = useWeeklyWindow ? L"7天" : L"5小时";
        payload.bestName = best->name;
        payload.bestGroup = best->group;
        payload.bestQuota = bestQuota;
        payload.accountKey = currentKey;
        PostLowQuotaCandidate(targetHwnd, std::move(payload));
      }
    }
    runningFlag->store(false); })
      .detach();
}

void WebViewHost::TriggerWebDavSync(const std::wstring &mode,
                                    const bool notifyStatus)
{
  if (webDavSyncRunning_.exchange(true))
  {
    if (notifyStatus)
    {
      SendWebStatus(L"WebDAV 同步正在进行中，请稍候", L"warning",
                    L"webdav_sync_running");
    }
    return;
  }
  webDavSyncRemainingSec_ = webDavSyncIntervalSec_;
  SendWebDavSyncState();

  const HWND targetHwnd = hwnd_;
  auto *runningFlag = &webDavSyncRunning_;
  const int nextRemainingSec = webDavSyncIntervalSec_;
  std::thread([targetHwnd, mode, notifyStatus, runningFlag, nextRemainingSec]()
              {
    bool needsConflictResolution = false;
    std::vector<WebDavConflictEntry> conflicts;
    std::wstring statusText;
    std::wstring error;
    AppConfig cfgAfter;
    const bool ok = ExecuteWebDavSyncMode(mode, {}, needsConflictResolution,
                                          conflicts, statusText, cfgAfter,
                                          error);
    runningFlag->store(false);

    if (needsConflictResolution)
    {
      WebDavPendingConflictContext ctx;
      ctx.active = true;
      ctx.mode = mode;
      ctx.conflicts = conflicts;
      StoreWebDavPendingConflictContext(ctx);
      UpdateWebDavStatusInConfig(L"等待处理 WebDAV 冲突", false, &cfgAfter);
      PostAsyncWebJson(targetHwnd, BuildWebDavConflictsJson(conflicts));
      PostAsyncWebJson(targetHwnd,
                       BuildWebDavSyncStateJson(cfgAfter, false,
                                                nextRemainingSec));
      if (notifyStatus)
      {
        SendWebStatusThreadSafe(targetHwnd,
                                L"检测到 WebDAV 冲突，请选择保留本地还是云端版本",
                                L"warning", L"webdav_conflicts_detected");
      }
      return;
    }

    ClearWebDavPendingConflictContext();
    if (ok)
    {
      PostAsyncWebJson(targetHwnd,
                       BuildWebDavSyncStateJson(cfgAfter, false,
                                                nextRemainingSec));
      PostAsyncWebJson(targetHwnd, BuildAccountsListJson(false, L"", L""));
      if (notifyStatus)
      {
        SendWebStatusThreadSafe(targetHwnd, statusText, L"success",
                                L"webdav_sync_success");
      }
      return;
    }

    UpdateWebDavStatusInConfig(statusText.empty() ? error : statusText, true,
                               &cfgAfter);
    PostAsyncWebJson(targetHwnd,
                     BuildWebDavSyncStateJson(cfgAfter, false,
                                              nextRemainingSec));
    if (notifyStatus)
    {
      SendWebStatusThreadSafe(targetHwnd,
                              statusText.empty() ? L"WebDAV 同步失败"
                                                 : statusText,
                              L"error", L"webdav_sync_failed");
    }
  })
      .detach();
}

void WebViewHost::TriggerCloudAccountDownload(const bool notifyStatus)
{
  if (cloudAccountDownloadRunning_.exchange(true))
  {
    if (notifyStatus)
    {
      SendWebStatus(L"云账号下载任务已在进行中", L"warning",
                    L"cloud_account_download_running");
    }
    return;
  }
  cloudAccountRemainingSec_ = cloudAccountIntervalSec_;
  SendCloudAccountState();

  const HWND targetHwnd = hwnd_;
  auto *runningFlag = &cloudAccountDownloadRunning_;
  const int nextRemainingSec = cloudAccountIntervalSec_;
  std::thread([targetHwnd, notifyStatus, runningFlag, nextRemainingSec]()
              {
    const CloudAccountDownloadResult result =
        ExecuteCloudAccountDownload(targetHwnd);
    runningFlag->store(false);
    PostAsyncWebJson(targetHwnd,
                     BuildCloudAccountStateJson(result.cfg, false,
                                                nextRemainingSec));
    if (result.accountsChanged)
    {
      PostAsyncWebJson(targetHwnd, BuildAccountsListJson(false, L"", L""));
    }
    if (notifyStatus)
    {
      PostAsyncWebJson(targetHwnd, BuildCloudAccountResultStatusJson(result));
    } })
      .detach();
}

void WebViewHost::HandleAutoRefreshTick()
{
  if (autoRefreshQuotaDisabled_)
  {
    allRefreshRemainingSec_ = allRefreshIntervalSec_;
    currentRefreshRemainingSec_ = currentRefreshIntervalSec_;
  }
  else if (allRefreshRemainingSec_ > 0)
  {
    --allRefreshRemainingSec_;
  }
  if (!autoRefreshQuotaDisabled_ && currentAutoRefreshEnabled_ &&
      currentRefreshRemainingSec_ > 0)
  {
    --currentRefreshRemainingSec_;
  }
  if (webDavAutoSyncEnabled_ && webDavEnabled_ && webDavSyncRemainingSec_ > 0)
  {
    --webDavSyncRemainingSec_;
  }
  if (cloudAccountAutoDownloadEnabled_ && cloudAccountRemainingSec_ > 0)
  {
    --cloudAccountRemainingSec_;
  }

  if (!autoRefreshQuotaDisabled_ && allRefreshRemainingSec_ <= 0)
  {
    TriggerRefreshAll();
  }
  if (!autoRefreshQuotaDisabled_ && currentAutoRefreshEnabled_ &&
      currentRefreshRemainingSec_ <= 0)
  {
    TriggerRefreshCurrent();
  }
  if (webDavAutoSyncEnabled_ && webDavEnabled_ && webDavSyncRemainingSec_ <= 0)
  {
    TriggerWebDavSync(L"bidirectional", false);
  }
  if (cloudAccountAutoDownloadEnabled_ && cloudAccountRemainingSec_ <= 0)
  {
    TriggerCloudAccountDownload(false);
  }
  SendRefreshTimerState();
  SendWebDavSyncState();
  SendCloudAccountState();
}

void WebViewHost::SendWebJson(const std::wstring &json) const
{
  if (webview_ != nullptr)
  {
    webview_->PostWebMessageAsJson(json.c_str());
  }
}

void WebViewHost::HandleWebAction(HWND hwnd, const std::wstring &action,
                                  const std::wstring &rawMessage)
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
    const HWND targetHwnd = hwnd_;
    std::thread([targetHwnd, account, group]()
                {
      std::wstring status;
      std::wstring code;
      const bool ok = SwitchToAccount(account, group, status, code);
      SendWebStatusThreadSafe(targetHwnd, status, ok ? L"success" : L"error",
                              code);
      if (ok) {
        PostAsyncWebJson(targetHwnd, BuildAccountsListJson(false, L"", L""));
      } })
        .detach();
    return;
  }

  if (action == L"backup_current")
  {
    const std::wstring name = ExtractJsonField(rawMessage, L"name");
    AppConfig cfg;
    LoadConfig(cfg);
    const bool queryUsage = cfg.enableAutoRefreshQuota;
    std::wstring status;
    std::wstring code;
    const bool ok = BackupCurrentAccount(name, status, code, queryUsage);
    const std::wstring level =
        (code == L"duplicate_name") ? L"warning" : (ok ? L"success" : L"error");
    SendWebStatus(status, level, code);
    if (ok)
    {
      const HWND targetHwnd = hwnd_;
      std::thread([targetHwnd, name, queryUsage]()
                  {
        PostAsyncWebJson(targetHwnd,
                         BuildAccountsListJson(queryUsage, name, L""));
        PostAsyncWebJson(targetHwnd,
                         L"{\"type\":\"status\",\"level\":\"success\",\"code\":"
                         L"\"account_quota_refreshed\",\"message\":\"\"}"); })
          .detach();
    }
    return;
  }

  if (action == L"backup_current_auto")
  {
    AppConfig cfg;
    LoadConfig(cfg);
    const bool queryUsage = cfg.enableAutoRefreshQuota;
    std::wstring savedName;
    std::wstring status;
    std::wstring code;
    const bool ok = BackupCurrentAccountAuto(savedName, status, code, queryUsage);
    const std::wstring level =
        (code == L"duplicate_name") ? L"warning" : (ok ? L"success" : L"error");
    SendWebStatus(status, level, code);
    if (ok)
    {
      const HWND targetHwnd = hwnd_;
      const std::wstring targetName = savedName;
      std::thread([targetHwnd, targetName, queryUsage]()
                  {
        PostAsyncWebJson(targetHwnd,
                         BuildAccountsListJson(queryUsage, targetName, L""));
        PostAsyncWebJson(targetHwnd,
                         L"{\"type\":\"status\",\"level\":\"success\",\"code\":"
                         L"\"account_quota_refreshed\",\"message\":\"\"}"); })
          .detach();
    }
    return;
  }

  if (action == L"rename_account")
  {
    const std::wstring account = ExtractJsonField(rawMessage, L"account");
    const std::wstring group = ExtractJsonField(rawMessage, L"group");
    const std::wstring newName = ExtractJsonField(rawMessage, L"newName");
    std::wstring status;
    std::wstring code;
    const bool ok = RenameAccount(account, group, newName, status, code);
    const std::wstring level =
        (code == L"duplicate_name") ? L"warning" : (ok ? L"success" : L"error");
    SendWebStatus(status, level, code);
    if (ok)
    {
      std::wstring autoName;
      std::wstring autoGroup;
      AutoSelectProxyFixedAccountIfNeeded(autoName, autoGroup);
      SendAccountsList(false, L"", L"");
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

  if (action == L"delete_accounts_batch")
  {
    std::wstring itemsArray;
    if (!ExtractJsonArrayField(rawMessage, L"items", itemsArray))
    {
      SendWebStatus(L"请至少选择一个账号", L"warning", L"batch_empty");
      return;
    }

    const auto objects = ExtractTopLevelObjectsFromArray(itemsArray);
    std::vector<std::pair<std::wstring, std::wstring>> targets;
    targets.reserve(objects.size());
    std::unordered_set<std::wstring> dedup;
    for (const auto &obj : objects)
    {
      const std::wstring account = ExtractJsonField(obj, L"account");
      if (account.empty())
      {
        continue;
      }
      const std::wstring group =
          NormalizeGroup(ExtractJsonField(obj, L"group"));
      const std::wstring key = ToLowerCopy(group) + L"::" + account;
      if (!dedup.insert(key).second)
      {
        continue;
      }
      targets.emplace_back(account, group);
    }

    if (targets.empty())
    {
      SendWebStatus(L"请至少选择一个账号", L"warning", L"batch_empty");
      return;
    }

    const HWND targetHwnd = hwnd_;
    std::thread([targetHwnd, targets = std::move(targets)]() mutable
                {
      int successCount = 0;
      std::wstring lastError;
      for (const auto &item : targets) {
        std::wstring status;
        std::wstring code;
        const bool ok =
            DeleteAccountBackup(item.first, item.second, status, code);
        if (ok) {
          ++successCount;
        } else if (!status.empty()) {
          lastError = status;
        }
      }

      const int totalCount = static_cast<int>(targets.size());
      std::wstring level = L"success";
      std::wstring code = L"batch_delete_done";
      if (successCount == 0) {
        level = L"error";
        code = L"batch_delete_failed";
      } else if (successCount < totalCount) {
        level = L"warning";
        code = L"batch_delete_partial";
      }

      std::wstring statusJson =
          L"{\"type\":\"status\",\"level\":\"" + EscapeJsonString(level) +
          L"\",\"code\":\"" + EscapeJsonString(code) +
          L"\",\"message\":\"\",\"success\":" + std::to_wstring(successCount) +
          L",\"total\":" + std::to_wstring(totalCount) + L",\"lastError\":\"" +
          EscapeJsonString(lastError) + L"\"}";

      PostAsyncWebJson(targetHwnd, statusJson);
      PostAsyncWebJson(targetHwnd, BuildAccountsListJson(false, L"", L"")); })
        .detach();
    return;
  }

  if (action == L"list_accounts")
  {
    SendAccountsList(false, L"", L"");
    SendRefreshTimerState();
    return;
  }

  if (action == L"refresh_accounts_batch")
  {
    std::wstring itemsArray;
    if (!ExtractJsonArrayField(rawMessage, L"items", itemsArray))
    {
      SendWebStatus(L"请至少选择一个账号", L"warning", L"batch_empty");
      return;
    }

    const auto objects = ExtractTopLevelObjectsFromArray(itemsArray);
    std::vector<std::pair<std::wstring, std::wstring>> targets;
    targets.reserve(objects.size());
    std::unordered_set<std::wstring> dedup;
    for (const auto &obj : objects)
    {
      const std::wstring account = ExtractJsonField(obj, L"account");
      if (account.empty())
      {
        continue;
      }
      const std::wstring group =
          NormalizeGroup(ExtractJsonField(obj, L"group"));
      const std::wstring key = ToLowerCopy(group) + L"::" + account;
      if (!dedup.insert(key).second)
      {
        continue;
      }
      targets.emplace_back(account, group);
    }

    if (targets.empty())
    {
      SendWebStatus(L"请至少选择一个账号", L"warning", L"batch_empty");
      return;
    }

    const HWND targetHwnd = hwnd_;
    std::thread([targetHwnd, targets = std::move(targets)]() mutable
                {
      std::wstring latestJson = BuildAccountsListJson(false, L"", L"");
      for (size_t i = 0; i < targets.size(); ++i) {
        const auto &item = targets[i];
        if (targetHwnd != nullptr && IsWindow(targetHwnd)) {
          PostAsyncWebJson(
              targetHwnd,
              BuildProgressStatusJson(L"quota_refresh_progress",
                                      static_cast<int>(i + 1),
                                      static_cast<int>(targets.size()),
                                      L"batch"));
        }
        latestJson = BuildAccountsListJson(true, item.first, item.second);
        if (!latestJson.empty()) {
          // Push incremental result immediately so UI updates account rows progressively.
          PostAsyncWebJson(targetHwnd, latestJson);
        }
        if (i + 1 < targets.size()) {
          Sleep(ComputeThrottleDelayMs(kRefreshAllThrottleMs));
        }
      }
      if (latestJson.empty()) {
        latestJson = BuildAccountsListJson(false, L"", L"");
      }
      if (!latestJson.empty()) {
        PostAsyncWebJson(targetHwnd, latestJson);
      }
      PostAsyncWebJson(targetHwnd,
                       L"{\"type\":\"status\",\"level\":\"success\",\"code\":"
                       L"\"batch_refresh_done\",\"message\":\"\"}"); })
        .detach();
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
      std::thread([targetHwnd, account, group]()
                  {
        const std::wstring normalizedGroup = NormalizeGroup(group);
        // Refresh by name to tolerate stale group values from UI.
        PostAsyncWebJson(targetHwnd, BuildAccountsListJson(true, account, L""));

        UsageSnapshot usage;
        bool refreshedOk = false;
        std::wstring err;
        std::wstring abnormalReason;
        EnsureIndexExists();
        IndexData idx;
        if (LoadIndex(idx)) {
          auto it = std::find_if(
              idx.accounts.begin(), idx.accounts.end(),
              [&](const IndexEntry &row) {
                const bool nameMatch =
                    EqualsIgnoreCase(row.name, SanitizeAccountName(account));
                if (!nameMatch) {
                  return false;
                }
                if (normalizedGroup.empty()) {
                  return true;
                }
                return NormalizeGroup(row.group) == normalizedGroup;
              });
          if (it == idx.accounts.end() && !normalizedGroup.empty()) {
            it = std::find_if(idx.accounts.begin(), idx.accounts.end(),
                              [&](const IndexEntry &row) {
                                return EqualsIgnoreCase(
                                    row.name, SanitizeAccountName(account));
                              });
          }
          if (it != idx.accounts.end()) {
            const fs::path authPath = ResolveAuthPathFromIndex(*it);
            refreshedOk = QueryUsageFromAuthFile(authPath, usage);
            if (!refreshedOk) {
              err = usage.error;
              abnormalReason = DetectUsageRefreshAbnormalReason(usage);
            }
          } else {
            err = L"account_not_found_in_index";
          }
        } else {
          err = L"index_load_failed";
        }

        if (refreshedOk) {
          PostAsyncWebJson(
              targetHwnd,
              L"{\"type\":\"status\",\"level\":\"success\",\"code\":\"account_"
              L"quota_refreshed\",\"message\":\"账号额度已刷新\"}");
        } else {
          const bool notFound = (err == L"account_not_found_in_index");
          const std::wstring msg =
              notFound ? L"账号索引已变化，已跳过本次刷新（账号仍可直接使用）"
                       : (L"账号额度刷新失败" +
                         (err.empty() ? L"" : (L": " + err)));
          PostAsyncWebJson(
              targetHwnd,
              L"{\"type\":\"status\",\"level\":\"" +
                  std::wstring(notFound ? L"warning" : L"error") +
                  L"\",\"code\":\"" +
                  std::wstring(notFound
                                   ? L"account_quota_refresh_skipped"
                                   : L"account_quota_refresh_failed") +
                  L"\",\"message\":\"" + EscapeJsonString(msg) + L"\"}");
        } })
          .detach();
    }
    return;
  }

  if (action == L"send_api_request")
  {
    const std::wstring model = ExtractJsonField(rawMessage, L"model");
    std::wstring content = ExtractJsonField(rawMessage, L"content");
    content = UnescapeJsonString(content);

    if (content.empty())
    {
      SendWebStatus(L"API请求参数不完整", L"warning", L"api_invalid_input");
      return;
    }

    const HWND targetHwnd = hwnd_;
    std::thread([targetHwnd, model, content]()
                {
      EnsureIndexExists();
      IndexData idx;
      if (!LoadIndex(idx)) {
        SendWebStatusThreadSafe(targetHwnd, L"读取账号索引失败", L"error",
                                L"api_no_selected_account");
        std::wstring failJson =
            L"{\"type\":\"api_result\",\"ok\":false,\"error\":\"" +
            EscapeJsonString(L"no_selected_account") + L"\"}";
        PostAsyncWebJson(targetHwnd, failJson);
        return;
      }

      fs::path authPath;
      std::wstring account;
      std::wstring group;
      const IndexEntry *currentPositiveRow = nullptr;
      const IndexEntry *bestPositiveRow = nullptr;
      const IndexEntry *currentUnknownUsableRow = nullptr;
      const IndexEntry *fallbackUnknownUsableRow = nullptr;
      int bestPositiveQuota = -1;
      bool hasAnyAuthFile = false;
      const std::wstring currentGroup = NormalizeGroup(idx.currentGroup);
      for (const auto &row : idx.accounts) {
        const fs::path candidate = ResolveAuthPathFromIndex(row);
        if (candidate.empty() || !fs::exists(candidate)) {
          continue;
        }
        hasAnyAuthFile = true;
        const bool isCurrent = !idx.currentName.empty() &&
                               EqualsIgnoreCase(row.name, idx.currentName) &&
                               NormalizeGroup(row.group) == currentGroup;
        if (HasUsableProxyQuota(row)) {
          bool usesWeeklyWindow = false;
          const int quotaMetric = GetProxyQuotaMetricValue(row, usesWeeklyWindow);
          if (quotaMetric > 0) {
            if (isCurrent) {
              currentPositiveRow = &row;
            }
            if (bestPositiveRow == nullptr || quotaMetric > bestPositiveQuota) {
              bestPositiveRow = &row;
              bestPositiveQuota = quotaMetric;
            }
          } else {
            if (isCurrent) {
              currentUnknownUsableRow = &row;
            }
            if (fallbackUnknownUsableRow == nullptr) {
              fallbackUnknownUsableRow = &row;
            }
          }
        }
      }

      auto bindRow = [&](const IndexEntry *row) -> bool {
        if (row == nullptr) {
          return false;
        }
        const fs::path candidate = ResolveAuthPathFromIndex(*row);
        if (candidate.empty() || !fs::exists(candidate)) {
          return false;
        }
        authPath = candidate;
        account = row->name;
        group = NormalizeGroup(row->group);
        return true;
      };

      if (currentPositiveRow != nullptr) {
        bindRow(currentPositiveRow);
      }
      if (authPath.empty()) {
        bindRow(bestPositiveRow);
      }
      if (authPath.empty()) {
        bindRow(currentUnknownUsableRow);
      }
      if (authPath.empty()) {
        bindRow(fallbackUnknownUsableRow);
      }

      if (authPath.empty() || !fs::exists(authPath)) {
        const std::wstring failCode =
            idx.accounts.empty()
                ? L"api_no_selected_account"
                : (hasAnyAuthFile ? L"api_no_quota_available"
                                  : L"api_account_auth_missing");
        const std::wstring failError =
            idx.accounts.empty()
                ? L"no_selected_account"
                : (hasAnyAuthFile ? L"no_quota_available_accounts"
                                  : L"auth_missing");
        const std::wstring failMessage =
            idx.accounts.empty()
                ? L"账号管理中没有可用账号"
                : (hasAnyAuthFile ? L"没有可用额度账号（已自动跳过 0 额度账号）"
                                  : L"未找到可用账号的auth文件");
        SendWebStatusThreadSafe(targetHwnd, failMessage, L"error", failCode);
        std::wstring failJson =
            L"{\"type\":\"api_result\",\"ok\":false,\"error\":\"" +
            EscapeJsonString(failError) + L"\"}";
        PostAsyncWebJson(targetHwnd, failJson);
        return;
      }

      std::wstring outputText;
      std::wstring rawResponse;
      std::wstring error;
      int statusCode = 0;
      const bool ok = SendCodexApiRequestByAuthFile(
          authPath, model, content, outputText, rawResponse, error, statusCode);

      if (ok) {
        SendWebStatusThreadSafe(targetHwnd, L"API请求成功", L"success",
                                L"api_request_success");
        std::wstring resultJson =
            L"{\"type\":\"api_result\",\"ok\":true,\"account\":\"" +
            EscapeJsonString(account) + L"\",\"group\":\"" +
            EscapeJsonString(group) + L"\",\"model\":\"" +
            EscapeJsonString(model) +
            L"\",\"content\":\"" + EscapeJsonString(outputText) + L"\",\"raw\":\"" +
            EscapeJsonString(rawResponse) + L"\"}";
        PostAsyncWebJson(targetHwnd, resultJson);
      } else {
        bool quotaMarkedAsExhausted = false;
        if (IsUsageLimitReachedPayload(rawResponse)) {
          std::wstring markErr;
          if (MarkProxyAccountQuotaExhausted(account, group, rawResponse, markErr)) {
            quotaMarkedAsExhausted = true;
            PostAsyncWebJson(targetHwnd, BuildAccountsListJson(false, L"", L""));
          } else {
            DebugLogLine(L"api.request",
                         L"mark quota exhausted failed: " + markErr);
          }
        }
        std::wstring errMsg = L"API请求失败";
        if (!error.empty()) {
          errMsg += L": " + error;
        }
        if (statusCode > 0) {
          errMsg += L" (HTTP " + std::to_wstring(statusCode) + L")";
        }
        if (quotaMarkedAsExhausted) {
          errMsg += L"；已将该账号标记为 0 额度，后续自动请求将跳过";
        }
        SendWebStatusThreadSafe(targetHwnd, errMsg, L"error",
                                L"api_request_failed");
        std::wstring resultJson =
            L"{\"type\":\"api_result\",\"ok\":false,\"account\":\"" +
            EscapeJsonString(account) + L"\",\"group\":\"" +
            EscapeJsonString(group) + L"\",\"model\":\"" +
            EscapeJsonString(model) +
            L"\",\"error\":\"" + EscapeJsonString(error) + L"\",\"status\":" +
            std::to_wstring(statusCode) + L",\"raw\":\"" +
            EscapeJsonString(rawResponse) + L"\"}";
        PostAsyncWebJson(targetHwnd, resultJson);
      } })
        .detach();
    return;
  }

  if (action == L"get_proxy_status")
  {
    SendWebJson(BuildProxyStatusJson());
    return;
  }

  if (action == L"get_traffic_logs")
  {
    const std::wstring account = ExtractJsonField(rawMessage, L"account");
    int limit = ExtractJsonIntField(rawMessage, L"limit", 200);
    SendWebJson(BuildTrafficLogsJson(account, limit));
    return;
  }

  if (action == L"get_token_stats")
  {
    const std::wstring account = ExtractJsonField(rawMessage, L"account");
    const std::wstring period = ExtractJsonField(rawMessage, L"period");
    SendWebJson(BuildTokenStatsJson(account, period.empty() ? L"day" : period));
    return;
  }

  if (action == L"get_api_models")
  {
    const HWND targetHwnd = hwnd_;
    std::thread([targetHwnd]()
                {
      auto postModels = [targetHwnd](const std::vector<std::wstring> &models) {
        std::wstringstream ss;
        ss << L"{\"type\":\"api_models\",\"models\":[";
        for (size_t i = 0; i < models.size(); ++i) {
          if (i != 0) {
            ss << L",";
          }
          ss << L"\"" << EscapeJsonString(models[i]) << L"\"";
        }
        ss << L"]}";
        PostAsyncWebJson(targetHwnd, ss.str());
      };

      fs::path authPath;
      std::wstring accountName;
      std::wstring groupName;
      std::wstring err;
      if (!ResolveCurrentAuthPath(authPath, accountName, groupName, err)) {
        DebugLogLine(L"api.models",
                     L"no current account, using fallback model list: " + err);
        postModels(BuildFallbackModelIds());
        return;
      }
      std::vector<std::wstring> models;
      if (!GetCachedModelIds(authPath, models, err)) {
        DebugLogLine(L"api.models",
                     L"cache get failed, using fallback model list: " + err);
        models = BuildFallbackModelIds();
      }
      postModels(models); })
        .detach();
    return;
  }

  if (action == L"start_proxy_service")
  {
    int port = ExtractJsonIntField(rawMessage, L"port", kDefaultProxyPort);
    int timeoutSec =
        ExtractJsonIntField(rawMessage, L"timeoutSec", kDefaultProxyTimeoutSec);
    const bool allowLan =
        ExtractJsonBoolField(rawMessage, L"allowLan", false);
    std::wstring dispatchMode = ToLowerCopy(
        UnescapeJsonString(ExtractJsonStringField(rawMessage, L"dispatchMode")));
    if (dispatchMode != L"round_robin" && dispatchMode != L"random" &&
        dispatchMode != L"fixed")
    {
      dispatchMode = g_ProxyDispatchMode;
    }
    const std::wstring fixedAccount = UnescapeJsonString(
        ExtractJsonStringField(rawMessage, L"fixedAccount"));
    const std::wstring fixedGroup = NormalizeGroup(UnescapeJsonString(
        ExtractJsonStringField(rawMessage, L"fixedGroup")));

    std::wstring status;
    std::wstring code;
    const bool ok =
        StartLocalProxyService(hwnd_, port, timeoutSec, allowLan, status, code);
    if (ok)
    {
      AppConfig cfg;
      if (LoadConfig(cfg))
      {
        if (cfg.proxyApiKey.empty())
        {
          cfg.proxyApiKey = GenerateProxyApiKey();
        }
        cfg.proxyPort = port;
        cfg.proxyTimeoutSec = timeoutSec;
        cfg.proxyAllowLan = allowLan;
        cfg.proxyDispatchMode = dispatchMode;
        cfg.proxyFixedAccount = fixedAccount;
        cfg.proxyFixedGroup = fixedGroup;
        SaveConfig(cfg);
        g_ProxyApiKey = cfg.proxyApiKey;
        g_ProxyDispatchMode = cfg.proxyDispatchMode;
        g_ProxyFixedAccount = cfg.proxyFixedAccount;
        g_ProxyFixedGroup = cfg.proxyFixedGroup;
        g_ProxyAutoMarkAbnormalAccounts = cfg.autoMarkAbnormalAccounts;
        g_AutoDeleteAbnormalAccounts = cfg.autoDeleteAbnormalAccounts;
        if (cfg.proxyStealthMode)
        {
          std::wstring stealthError;
          if (!ApplyStealthProxyModeToCodexProfile(cfg, stealthError))
          {
            DebugLogLine(L"proxy.stealth",
                         L"apply after start failed: " + stealthError);
          }
          std::wstring envError;
          if (!SyncStealthProxyEnvironment(cfg, envError))
          {
            DebugLogLine(L"proxy.stealth",
                         L"sync env after start failed: " + envError);
          }
        }
      }
    }
    SendWebStatus(status, ok ? L"success" : L"error", code);
    SendWebJson(BuildProxyStatusJson());
    return;
  }

  if (action == L"stop_proxy_service")
  {
    bool configUpdated = false;
    AppConfig cfg;
    if (LoadConfig(cfg) && cfg.proxyAutoStart)
    {
      cfg.proxyAutoStart = false;
      configUpdated = SaveConfig(cfg);
    }
    std::wstring status;
    std::wstring code;
    StopLocalProxyService(status, code);
    SendWebStatus(status, code == L"proxy_not_running" ? L"warning" : L"success",
                  code);
    SendWebJson(BuildProxyStatusJson());
    if (configUpdated)
    {
      SendConfig(false);
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
      autoRefreshQuotaDisabled_ = !cfg.enableAutoRefreshQuota;
      g_ProxyAutoMarkAbnormalAccounts = cfg.autoMarkAbnormalAccounts;
      g_AutoDeleteAbnormalAccounts = cfg.autoDeleteAbnormalAccounts;
      currentAutoRefreshEnabled_ = cfg.autoRefreshCurrent;
      lowQuotaPromptEnabled_ = cfg.lowQuotaAutoPrompt;
      proxyStealthModeEnabled_ = cfg.proxyStealthMode;
      cloudAccountAutoDownloadEnabled_ = cfg.cloudAccountAutoDownload;
      cloudAccountIntervalSec_ =
          ClampWebDavSyncMinutes(cfg.cloudAccountIntervalMinutes,
                                 kDefaultCloudAccountSyncMinutes) *
          60;
      cloudAccountLastDownloadAt_ = cfg.cloudAccountLastDownloadAt;
      cloudAccountLastDownloadStatus_ = cfg.cloudAccountLastDownloadStatus;
      webDavEnabled_ = cfg.webdavEnabled;
      webDavAutoSyncEnabled_ = cfg.webdavEnabled && cfg.webdavAutoSync;
      webDavSyncIntervalSec_ =
          ClampWebDavSyncMinutes(cfg.webdavSyncIntervalMinutes,
                                 kDefaultWebDavSyncMinutes) *
          60;
      webDavLastSyncAt_ = cfg.webdavLastSyncAt;
      webDavLastSyncStatus_ = cfg.webdavLastSyncStatus;
      allRefreshIntervalSec_ = ClampRefreshMinutes(cfg.autoRefreshAllMinutes,
                                                   kDefaultAllRefreshMinutes) *
                              60;
      currentRefreshIntervalSec_ =
          ClampRefreshMinutes(cfg.autoRefreshCurrentMinutes,
                              kDefaultCurrentRefreshMinutes) *
          60;
      if (allRefreshRemainingSec_ <= 0 ||
          allRefreshRemainingSec_ > allRefreshIntervalSec_)
      {
        allRefreshRemainingSec_ = allRefreshIntervalSec_;
      }
      if (currentRefreshRemainingSec_ <= 0 ||
          currentRefreshRemainingSec_ > currentRefreshIntervalSec_)
      {
        currentRefreshRemainingSec_ = currentRefreshIntervalSec_;
      }
      if (autoRefreshQuotaDisabled_)
      {
        allRefreshRemainingSec_ = allRefreshIntervalSec_;
        currentRefreshRemainingSec_ = currentRefreshIntervalSec_;
      }
      if (webDavSyncRemainingSec_ <= 0 ||
          webDavSyncRemainingSec_ > webDavSyncIntervalSec_)
      {
        webDavSyncRemainingSec_ = webDavSyncIntervalSec_;
      }
      if (cloudAccountRemainingSec_ <= 0 ||
          cloudAccountRemainingSec_ > cloudAccountIntervalSec_)
      {
        cloudAccountRemainingSec_ = cloudAccountIntervalSec_;
      }
    }
    SendConfig(created);
    SendRefreshTimerState();
    SendWebDavSyncState();
    SendCloudAccountState();
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
    std::wstring tabVisibilityJson;
    const bool hasTabVisibility =
        ExtractJsonObjectField(rawMessage, L"tabVisibility", tabVisibilityJson);
    const bool autoUpdate =
        ExtractJsonBoolField(rawMessage, L"autoUpdate", true);
    const bool hasEnableAutoRefreshQuota =
        rawMessage.find(L"\"enableAutoRefreshQuota\"") != std::wstring::npos;
    const bool enableAutoRefreshQuota = hasEnableAutoRefreshQuota
                                            ? ExtractJsonBoolField(
                                                  rawMessage,
                                                  L"enableAutoRefreshQuota",
                                                  true)
                                            : !ExtractJsonBoolField(
                                                  rawMessage,
                                                  L"disableAutoRefreshQuota",
                                                  false);
    const bool autoMarkAbnormalAccounts =
        ExtractJsonBoolField(rawMessage, L"autoMarkAbnormalAccounts", true);
    const bool autoDeleteAbnormalAccounts =
        ExtractJsonBoolField(rawMessage, L"autoDeleteAbnormalAccounts", false);
    const bool autoRefreshCurrent =
        ExtractJsonBoolField(rawMessage, L"autoRefreshCurrent", true);
    const bool lowQuotaAutoPrompt =
        ExtractJsonBoolField(rawMessage, L"lowQuotaAutoPrompt", true);
    const std::wstring closeWindowBehavior =
        NormalizeCloseWindowBehavior(
            UnescapeJsonString(
                ExtractJsonStringField(rawMessage, L"closeWindowBehavior")));
    const int autoRefreshAllMinutes = ExtractJsonIntField(
        rawMessage, L"autoRefreshAllMinutes", kDefaultAllRefreshMinutes);
    const int autoRefreshCurrentMinutes =
        ExtractJsonIntField(rawMessage, L"autoRefreshCurrentMinutes",
                            kDefaultCurrentRefreshMinutes);
    const bool hasProxyPort = rawMessage.find(L"\"proxyPort\"") != std::wstring::npos;
    const bool hasProxyTimeoutSec =
        rawMessage.find(L"\"proxyTimeoutSec\"") != std::wstring::npos;
    const bool hasProxyAllowLan =
        rawMessage.find(L"\"proxyAllowLan\"") != std::wstring::npos;
    const bool hasProxyAutoStart =
        rawMessage.find(L"\"proxyAutoStart\"") != std::wstring::npos;
    const bool hasProxyStealthMode =
        rawMessage.find(L"\"proxyStealthMode\"") != std::wstring::npos;
    const bool hasProxyApiKey =
        rawMessage.find(L"\"proxyApiKey\"") != std::wstring::npos;
    const bool hasProxyDispatchMode =
        rawMessage.find(L"\"proxyDispatchMode\"") != std::wstring::npos;
    const bool hasProxyFixedAccount =
        rawMessage.find(L"\"proxyFixedAccount\"") != std::wstring::npos;
    const bool hasProxyFixedGroup =
        rawMessage.find(L"\"proxyFixedGroup\"") != std::wstring::npos;
    const bool hasAutoMarkAbnormalAccounts =
        rawMessage.find(L"\"autoMarkAbnormalAccounts\"") != std::wstring::npos;
    const bool hasAutoDeleteAbnormalAccounts =
        rawMessage.find(L"\"autoDeleteAbnormalAccounts\"") != std::wstring::npos;
    const bool hasCloudAccountUrl =
        rawMessage.find(L"\"cloudAccountUrl\"") != std::wstring::npos;
    const bool hasCloudAccountPassword =
        rawMessage.find(L"\"cloudAccountPassword\"") != std::wstring::npos;
    const bool hasCloudAccountPasswordClear =
        rawMessage.find(L"\"cloudAccountPasswordClear\"") != std::wstring::npos;
    const bool hasCloudAccountAutoDownload =
        rawMessage.find(L"\"cloudAccountAutoDownload\"") != std::wstring::npos;
    const bool hasCloudAccountIntervalMinutes =
        rawMessage.find(L"\"cloudAccountIntervalMinutes\"") != std::wstring::npos;
    const bool hasWebdavEnabled =
        rawMessage.find(L"\"webdavEnabled\"") != std::wstring::npos;
    const bool hasWebdavAutoSync =
        rawMessage.find(L"\"webdavAutoSync\"") != std::wstring::npos;
    const bool hasWebdavSyncIntervalMinutes =
        rawMessage.find(L"\"webdavSyncIntervalMinutes\"") != std::wstring::npos;
    const bool hasWebdavUrl =
        rawMessage.find(L"\"webdavUrl\"") != std::wstring::npos;
    const bool hasWebdavRemotePath =
        rawMessage.find(L"\"webdavRemotePath\"") != std::wstring::npos;
    const bool hasWebdavUsername =
        rawMessage.find(L"\"webdavUsername\"") != std::wstring::npos;
    const bool hasWebdavPassword =
        rawMessage.find(L"\"webdavPassword\"") != std::wstring::npos;
    const bool hasWebdavPasswordClear =
        rawMessage.find(L"\"webdavPasswordClear\"") != std::wstring::npos;
    const int proxyPort = ExtractJsonIntField(rawMessage, L"proxyPort", -1);
    const int proxyTimeoutSec =
        ExtractJsonIntField(rawMessage, L"proxyTimeoutSec", -1);
    const bool webdavEnabled =
        ExtractJsonBoolField(rawMessage, L"webdavEnabled", false);
    const bool webdavAutoSync =
        ExtractJsonBoolField(rawMessage, L"webdavAutoSync", true);
    const int webdavSyncIntervalMinutes =
        ExtractJsonIntField(rawMessage, L"webdavSyncIntervalMinutes",
                            kDefaultWebDavSyncMinutes);
    const std::wstring webdavUrl =
        UnescapeJsonString(ExtractJsonStringField(rawMessage, L"webdavUrl"));
    const std::wstring webdavRemotePath =
        UnescapeJsonString(ExtractJsonStringField(rawMessage, L"webdavRemotePath"));
    const std::wstring webdavUsername =
        UnescapeJsonString(ExtractJsonStringField(rawMessage, L"webdavUsername"));
    const std::wstring webdavPassword =
        UnescapeJsonString(ExtractJsonStringField(rawMessage, L"webdavPassword"));
    const bool webdavPasswordClear =
        ExtractJsonBoolField(rawMessage, L"webdavPasswordClear", false);
    const bool proxyAllowLan =
        ExtractJsonBoolField(rawMessage, L"proxyAllowLan", false);
    const bool proxyAutoStart =
        ExtractJsonBoolField(rawMessage, L"proxyAutoStart", false);
    const bool proxyStealthMode =
        ExtractJsonBoolField(rawMessage, L"proxyStealthMode", false);
    const std::wstring cloudAccountUrl =
        UnescapeJsonString(ExtractJsonStringField(rawMessage, L"cloudAccountUrl"));
    const std::wstring cloudAccountPassword =
        UnescapeJsonString(
            ExtractJsonStringField(rawMessage, L"cloudAccountPassword"));
    const bool cloudAccountPasswordClear =
        ExtractJsonBoolField(rawMessage, L"cloudAccountPasswordClear", false);
    const bool cloudAccountAutoDownload =
        ExtractJsonBoolField(rawMessage, L"cloudAccountAutoDownload", false);
    const int cloudAccountIntervalMinutes =
        ExtractJsonIntField(rawMessage, L"cloudAccountIntervalMinutes",
                            kDefaultCloudAccountSyncMinutes);
    const std::wstring proxyApiKey =
        UnescapeJsonString(ExtractJsonStringField(rawMessage, L"proxyApiKey"));
    const std::wstring proxyDispatchMode =
        ToLowerCopy(UnescapeJsonString(
            ExtractJsonStringField(rawMessage, L"proxyDispatchMode")));
    const std::wstring proxyFixedAccount =
        UnescapeJsonString(ExtractJsonStringField(rawMessage, L"proxyFixedAccount"));
    const std::wstring proxyFixedGroup =
        NormalizeGroup(UnescapeJsonString(
            ExtractJsonStringField(rawMessage, L"proxyFixedGroup")));
    AppConfig cfg;
    LoadConfig(cfg);
    const bool wasProxyStealthMode = cfg.proxyStealthMode;
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
    if (hasTabVisibility)
    {
      ReadTabVisibilityFromJson(tabVisibilityJson, cfg.tabVisibility);
    }
    NormalizeTabVisibility(cfg.tabVisibility);
    cfg.autoUpdate = autoUpdate;
    cfg.enableAutoRefreshQuota = enableAutoRefreshQuota;
    if (hasAutoMarkAbnormalAccounts)
    {
      cfg.autoMarkAbnormalAccounts = autoMarkAbnormalAccounts;
    }
    if (hasAutoDeleteAbnormalAccounts)
    {
      cfg.autoDeleteAbnormalAccounts = autoDeleteAbnormalAccounts;
    }
    cfg.autoRefreshCurrent = autoRefreshCurrent;
    cfg.lowQuotaAutoPrompt = lowQuotaAutoPrompt;
    cfg.closeWindowBehavior = closeWindowBehavior;
    cfg.autoRefreshAllMinutes =
        ClampRefreshMinutes(autoRefreshAllMinutes, kDefaultAllRefreshMinutes);
    cfg.autoRefreshCurrentMinutes = ClampRefreshMinutes(
        autoRefreshCurrentMinutes, kDefaultCurrentRefreshMinutes);
    if (hasProxyPort)
    {
      cfg.proxyPort =
          (proxyPort >= 1 && proxyPort <= 65535) ? proxyPort : kDefaultProxyPort;
    }
    if (hasProxyTimeoutSec)
    {
      cfg.proxyTimeoutSec = proxyTimeoutSec < 30
                                ? 30
                                : (proxyTimeoutSec > 7200 ? 7200 : proxyTimeoutSec);
    }
    if (hasProxyAllowLan)
    {
      cfg.proxyAllowLan = proxyAllowLan;
    }
    if (hasProxyAutoStart)
    {
      cfg.proxyAutoStart = proxyAutoStart;
    }
    if (hasProxyStealthMode)
    {
      cfg.proxyStealthMode = proxyStealthMode;
    }
    if (hasProxyApiKey)
    {
      cfg.proxyApiKey = proxyApiKey;
    }
    if (hasProxyDispatchMode)
    {
      if (proxyDispatchMode == L"round_robin" || proxyDispatchMode == L"random" ||
          proxyDispatchMode == L"fixed")
      {
        cfg.proxyDispatchMode = proxyDispatchMode;
      }
    }
    if (hasProxyFixedAccount)
    {
      cfg.proxyFixedAccount = proxyFixedAccount;
    }
    if (hasProxyFixedGroup)
    {
      cfg.proxyFixedGroup = proxyFixedGroup;
    }
    if (hasCloudAccountUrl)
    {
      cfg.cloudAccountUrl = cloudAccountUrl;
    }
    if (hasCloudAccountAutoDownload)
    {
      cfg.cloudAccountAutoDownload = cloudAccountAutoDownload;
    }
    if (hasCloudAccountIntervalMinutes)
    {
      cfg.cloudAccountIntervalMinutes = ClampWebDavSyncMinutes(
          cloudAccountIntervalMinutes, kDefaultCloudAccountSyncMinutes);
    }

    std::wstring cloudAccountSecretError;
    if (hasCloudAccountPasswordClear && cloudAccountPasswordClear)
    {
      DeleteProtectedFile(GetCloudAccountSecretPath(), cloudAccountSecretError);
      cfg.cloudAccountPasswordConfigured = false;
    }
    if (hasCloudAccountPassword && !cloudAccountPassword.empty())
    {
      if (SaveProtectedWideText(GetCloudAccountSecretPath(), cloudAccountPassword,
                                cloudAccountSecretError))
      {
        cfg.cloudAccountPasswordConfigured = true;
      }
    }
    else if (fs::exists(GetCloudAccountSecretPath()))
    {
      cfg.cloudAccountPasswordConfigured = true;
    }
    if (hasWebdavEnabled)
    {
      cfg.webdavEnabled = webdavEnabled;
    }
    if (hasWebdavAutoSync)
    {
      cfg.webdavAutoSync = webdavAutoSync;
    }
    if (hasWebdavSyncIntervalMinutes)
    {
      cfg.webdavSyncIntervalMinutes = ClampWebDavSyncMinutes(
          webdavSyncIntervalMinutes, kDefaultWebDavSyncMinutes);
    }
    if (hasWebdavUrl)
    {
      cfg.webdavUrl = webdavUrl;
    }
    if (hasWebdavRemotePath)
    {
      cfg.webdavRemotePath =
          webdavRemotePath.empty() ? L"/CodexAccountSwitch" : webdavRemotePath;
    }
    if (hasWebdavUsername)
    {
      cfg.webdavUsername = webdavUsername;
    }

    std::wstring webdavSecretError;
    if (hasWebdavPasswordClear && webdavPasswordClear)
    {
      DeleteProtectedFile(GetWebDavSecretPath(), webdavSecretError);
      cfg.webdavPasswordConfigured = false;
    }
    if (hasWebdavPassword && !webdavPassword.empty())
    {
      if (SaveProtectedWideText(GetWebDavSecretPath(), webdavPassword,
                                webdavSecretError))
      {
        cfg.webdavPasswordConfigured = true;
      }
    }
    else if (fs::exists(GetWebDavSecretPath()))
    {
      cfg.webdavPasswordConfigured = true;
    }
    const bool saved = SaveConfig(cfg);
    if (saved)
    {
      ApplyWindowTitleTheme(hwnd_, cfg.theme);
      autoRefreshQuotaDisabled_ = !cfg.enableAutoRefreshQuota;
      g_ProxyAutoMarkAbnormalAccounts = cfg.autoMarkAbnormalAccounts;
      g_AutoDeleteAbnormalAccounts = cfg.autoDeleteAbnormalAccounts;
      currentAutoRefreshEnabled_ = cfg.autoRefreshCurrent;
      lowQuotaPromptEnabled_ = cfg.lowQuotaAutoPrompt;
      closeWindowToTray_ = cfg.closeWindowBehavior != L"exit";
      proxyStealthModeEnabled_ = cfg.proxyStealthMode;
      cloudAccountAutoDownloadEnabled_ = cfg.cloudAccountAutoDownload;
      cloudAccountIntervalSec_ =
          cfg.cloudAccountIntervalMinutes * 60;
      cloudAccountLastDownloadAt_ = cfg.cloudAccountLastDownloadAt;
      cloudAccountLastDownloadStatus_ = cfg.cloudAccountLastDownloadStatus;
      webDavEnabled_ = cfg.webdavEnabled;
      webDavAutoSyncEnabled_ = cfg.webdavEnabled && cfg.webdavAutoSync;
      webDavSyncIntervalSec_ = cfg.webdavSyncIntervalMinutes * 60;
      webDavLastSyncAt_ = cfg.webdavLastSyncAt;
      webDavLastSyncStatus_ = cfg.webdavLastSyncStatus;
      g_ProxyApiKey = cfg.proxyApiKey;
      g_ProxyDispatchMode = cfg.proxyDispatchMode;
      g_ProxyFixedAccount = cfg.proxyFixedAccount;
      g_ProxyFixedGroup = cfg.proxyFixedGroup;
      if (!lowQuotaPromptEnabled_)
      {
        pendingLowQuotaPrompt_ = false;
      }
      allRefreshIntervalSec_ = cfg.autoRefreshAllMinutes * 60;
      currentRefreshIntervalSec_ = cfg.autoRefreshCurrentMinutes * 60;
      allRefreshRemainingSec_ = allRefreshIntervalSec_;
      currentRefreshRemainingSec_ = currentRefreshIntervalSec_;
      if (autoRefreshQuotaDisabled_)
      {
        allRefreshRemainingSec_ = allRefreshIntervalSec_;
        currentRefreshRemainingSec_ = currentRefreshIntervalSec_;
      }
      cloudAccountRemainingSec_ = cloudAccountIntervalSec_;
      webDavSyncRemainingSec_ = webDavSyncIntervalSec_;

      if (cfg.proxyStealthMode || wasProxyStealthMode)
      {
        const HWND targetHwnd = hwnd_;
        const AppConfig cfgCopy = cfg;
        std::thread([targetHwnd, cfgCopy, wasProxyStealthMode]()
                    {
          std::wstring stealthError;
          std::wstring restoreError;
          std::wstring envError;
          if (cfgCopy.proxyStealthMode) {
            if (!ApplyStealthProxyModeToCodexProfile(cfgCopy, stealthError)) {
              DebugLogLine(L"proxy.stealth", L"apply failed: " + stealthError);
            }
          } else if (wasProxyStealthMode) {
            if (!RestoreCodexProfileFromStealthBackup(restoreError)) {
              DebugLogLine(L"proxy.stealth",
                           L"restore failed: " + restoreError);
            }
          }
          if (!SyncStealthProxyEnvironment(cfgCopy, envError)) {
            DebugLogLine(L"proxy.stealth", L"sync env failed: " + envError);
          }
          std::wstring warnText;
          if (!stealthError.empty()) {
            warnText += L"无痕换号配置失败: " + stealthError;
          }
          if (!restoreError.empty()) {
            if (!warnText.empty()) {
              warnText += L"；";
            }
            warnText += L"无痕换号恢复失败: " + restoreError;
          }
          if (!envError.empty()) {
            if (!warnText.empty()) {
              warnText += L"；";
            }
            warnText += L"环境变量同步失败: " + envError;
          }
          if (!warnText.empty()) {
            SendWebStatusThreadSafe(targetHwnd, warnText, L"warning",
                                    L"config_saved");
          } })
            .detach();
      }
    }
    std::wstring statusText = saved ? L"设置已保存" : L"设置保存失败";
    SendWebStatus(statusText, saved ? L"success" : L"error",
                  saved ? L"config_saved" : L"save_config_failed");
    SendConfig(false);
    SendRefreshTimerState();
    SendWebDavSyncState();
    SendCloudAccountState();
    if (!cloudAccountSecretError.empty())
    {
      SendWebStatus(L"云账号密码处理失败：" + cloudAccountSecretError, L"warning",
                    L"config_saved");
    }
    if (!webdavSecretError.empty())
    {
      SendWebStatus(L"WebDAV 密码处理失败：" + webdavSecretError, L"warning",
                    L"config_saved");
    }
    return;
  }

  if (action == L"test_webdav_connection")
  {
    AppConfig cfg;
    LoadConfig(cfg);
    cfg.webdavPasswordConfigured = fs::exists(GetWebDavSecretPath());
    WebDavRequestConfig requestCfg;
    std::wstring error;
    if (!BuildWebDavRequestConfig(cfg, requestCfg, error))
    {
      SendWebStatus(L"WebDAV 配置不完整，请检查地址、用户名和密码", L"warning",
                    L"webdav_invalid_config");
      SendWebDavSyncState();
      return;
    }
    if (!EnsureWebDavDirectoryTree(requestCfg, requestCfg.basePath, error))
    {
      SendWebStatus(L"WebDAV 连接测试失败：无法访问远端目录", L"error",
                    L"webdav_test_failed");
      SendWebDavSyncState();
      return;
    }
    WebDavManifest manifest;
    bool exists = false;
    if (!WebDavGetManifest(requestCfg, manifest, exists, error))
    {
      SendWebStatus(L"WebDAV 连接测试失败：读取清单失败", L"error",
                    L"webdav_test_failed");
      SendWebDavSyncState();
      return;
    }
    SendWebStatus(exists ? L"WebDAV 连接测试通过，已读取到云端清单"
                         : L"WebDAV 连接测试通过，已就绪但云端暂无清单",
                  L"success", L"webdav_test_success");
    SendWebDavSyncState();
    return;
  }

  if (action == L"run_webdav_sync")
  {
    std::wstring mode = ToLowerCopy(UnescapeJsonString(
        ExtractJsonStringField(rawMessage, L"mode")));
    if (mode != L"upload" && mode != L"download" && mode != L"bidirectional" &&
        mode != L"reset_upload")
    {
      mode = L"bidirectional";
    }
    TriggerWebDavSync(mode, true);
    return;
  }

  if (action == L"download_latest_cloud_account")
  {
    TriggerCloudAccountDownload(true);
    return;
  }

  if (action == L"resolve_webdav_conflicts")
  {
    const bool cancel =
        ExtractJsonBoolField(rawMessage, L"cancel", false);
    if (cancel)
    {
      ClearWebDavPendingConflictContext();
      AppConfig cfg;
      UpdateWebDavStatusInConfig(L"已取消 WebDAV 冲突处理", false, &cfg);
      SendWebStatus(L"已取消 WebDAV 冲突处理", L"warning",
                    L"webdav_conflicts_cancelled");
      SendWebDavSyncState();
      return;
    }

    std::wstring decisionsArray;
    if (!ExtractJsonArrayField(rawMessage, L"decisions", decisionsArray))
    {
      SendWebStatus(L"请先为每个冲突选择保留本地或云端版本", L"warning",
                    L"webdav_conflicts_missing");
      return;
    }
    std::map<std::wstring, std::wstring> decisions;
    for (const auto &obj : ExtractTopLevelObjectsFromArray(decisionsArray))
    {
      const std::wstring account =
          UnescapeJsonString(ExtractJsonField(obj, L"account"));
      std::wstring winner = ToLowerCopy(
          UnescapeJsonString(ExtractJsonField(obj, L"winner")));
      if (winner != L"local" && winner != L"remote")
      {
        winner = L"remote";
      }
      if (!account.empty())
      {
        decisions[BuildWebDavAccountKey(account)] = winner;
      }
    }
    if (decisions.empty())
    {
      SendWebStatus(L"请先为每个冲突选择保留本地或云端版本", L"warning",
                    L"webdav_conflicts_missing");
      return;
    }
    if (webDavSyncRunning_.exchange(true))
    {
      SendWebStatus(L"WebDAV 同步正在进行中，请稍候", L"warning",
                    L"webdav_sync_running");
      return;
    }
    webDavSyncRemainingSec_ = webDavSyncIntervalSec_;
    SendWebDavSyncState();
    const HWND targetHwnd = hwnd_;
    auto *runningFlag = &webDavSyncRunning_;
    const int nextRemainingSec = webDavSyncIntervalSec_;
    std::thread([targetHwnd, decisions = std::move(decisions), runningFlag,
                 nextRemainingSec]() mutable
                {
      bool needsConflictResolution = false;
      std::vector<WebDavConflictEntry> conflicts;
      std::wstring statusText;
      std::wstring error;
      AppConfig cfgAfter;
      const bool ok = ExecuteWebDavSyncMode(L"bidirectional", decisions,
                                            needsConflictResolution, conflicts,
                                            statusText, cfgAfter, error);
      runningFlag->store(false);
      if (needsConflictResolution)
      {
        WebDavPendingConflictContext ctx;
        ctx.active = true;
        ctx.mode = L"bidirectional";
        ctx.conflicts = conflicts;
        StoreWebDavPendingConflictContext(ctx);
        UpdateWebDavStatusInConfig(L"仍存在待处理的 WebDAV 冲突", false,
                                   &cfgAfter);
        PostAsyncWebJson(targetHwnd, BuildWebDavConflictsJson(conflicts));
        PostAsyncWebJson(targetHwnd,
                         BuildWebDavSyncStateJson(cfgAfter, false,
                                                  nextRemainingSec));
        SendWebStatusThreadSafe(targetHwnd,
                                L"部分冲突仍未解决，请继续选择保留本地或云端版本",
                                L"warning", L"webdav_conflicts_detected");
        return;
      }
      ClearWebDavPendingConflictContext();
      if (ok)
      {
        PostAsyncWebJson(targetHwnd,
                         BuildWebDavSyncStateJson(cfgAfter, false,
                                                  nextRemainingSec));
        PostAsyncWebJson(targetHwnd, BuildAccountsListJson(false, L"", L""));
        SendWebStatusThreadSafe(targetHwnd, statusText, L"success",
                                L"webdav_sync_success");
        return;
      }
      UpdateWebDavStatusInConfig(statusText.empty() ? error : statusText, true,
                                 &cfgAfter);
      PostAsyncWebJson(targetHwnd,
                       BuildWebDavSyncStateJson(cfgAfter, false,
                                                nextRemainingSec));
      SendWebStatusThreadSafe(targetHwnd,
                              statusText.empty() ? L"WebDAV 同步失败"
                                                 : statusText,
                              L"error", L"webdav_sync_failed");
    })
        .detach();
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
    const std::wstring level = (code == L"export_cancelled")
                                   ? L"warning"
                                   : (ok ? L"success" : L"error");
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
    const std::wstring level = (code == L"import_cancelled")
                                   ? L"warning"
                                   : (ok ? L"success" : L"error");
    SendWebStatus(status, level, code);
    if (ok)
    {
      SendAccountsList(false, L"", L"");
    }
    return;
  }

  if (action == L"import_auth_json")
  {
    std::vector<std::wstring> jsonPaths;
    if (!PickOpenJsonPaths(hwnd, jsonPaths))
    {
      SendWebStatus(L"导入已取消", L"warning", L"import_cancelled");
      return;
    }

    AppConfig cfg;
    LoadConfig(cfg);
    const bool queryUsage = cfg.enableAutoRefreshQuota;
    const bool autoDeleteAbnormal = cfg.autoDeleteAbnormalAccounts;
    const HWND targetHwnd = hwnd_;
    std::thread([targetHwnd, jsonPaths = std::move(jsonPaths),
                 queryUsage, autoDeleteAbnormal]() mutable
                {
      EnsureIndexExists();
      IndexData idx;
      LoadIndex(idx);
      std::vector<std::wstring> reservedNames;
      reservedNames.reserve(idx.accounts.size() + jsonPaths.size());
      for (const auto &row : idx.accounts) {
        reservedNames.push_back(row.name);
      }

      const int totalCount = static_cast<int>(jsonPaths.size());
      int successCount = 0;
      int abnormalCount = 0;
      std::wstring lastError;

      for (size_t i = 0; i < jsonPaths.size(); ++i) {
        const auto &jsonPath = jsonPaths[i];
        if (targetHwnd != nullptr && IsWindow(targetHwnd)) {
          PostAsyncWebJson(
              targetHwnd,
              BuildProgressStatusJson(L"import_auth_batch_progress",
                                      static_cast<int>(i + 1), totalCount));
        }
        UsageSnapshot usage;
        if (queryUsage) {
          QueryUsageFromAuthFile(jsonPath, usage);
        }
        const std::wstring baseName =
            MakeImportBaseNameFromPath(jsonPath, usage);
        const std::wstring uniqueName =
            MakeUniqueImportedName(baseName, reservedNames);

        std::wstring status;
        std::wstring code;
        bool abnormal = false;
        const bool ok = ImportAuthJsonFile(jsonPath, uniqueName, status, code,
                                           queryUsage, &abnormal);
        if (ok) {
          if (abnormal) {
            ++abnormalCount;
            if (!autoDeleteAbnormal) {
              ++successCount;
            }
          } else {
            ++successCount;
          }
          reservedNames.push_back(uniqueName);
        } else {
          lastError = status;
        }
      }

      const int failedCount = totalCount - successCount;
      std::wstring level = L"success";
      std::wstring code = L"import_auth_batch_done";
      if (successCount == totalCount) {
        level = L"success";
        code = L"import_auth_batch_done";
      } else if (successCount > 0) {
        level = L"warning";
        code = L"import_auth_batch_partial";
      } else {
        level = L"error";
        code = L"import_auth_batch_failed";
      }

      std::wstring statusJson =
          L"{\"type\":\"status\",\"level\":\"" + EscapeJsonString(level) +
          L"\",\"code\":\"" + EscapeJsonString(code) +
          L"\",\"message\":\"\",\"success\":" + std::to_wstring(successCount) +
          L",\"failed\":" + std::to_wstring(failedCount) +
          L",\"total\":" + std::to_wstring(totalCount) +
          L",\"abnormal\":" + std::to_wstring(abnormalCount) +
          L",\"lastError\":\"" + EscapeJsonString(lastError) + L"\"}";

      std::wstring autoName;
      std::wstring autoGroup;
      AutoSelectProxyFixedAccountIfNeeded(autoName, autoGroup);
      PostAsyncWebJson(targetHwnd, statusJson);
      PostAsyncWebJson(targetHwnd, BuildAccountsListJson(false, L"", L"")); })
        .detach();
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

  if (action == L"start_oauth_login")
  {
    {
      std::lock_guard<std::mutex> lock(g_OAuthCallbackMutex);
      if (g_OAuthCallbackResult != nullptr)
      {
        SendWebStatus(L"已有进行中的 OAuth 授权，请先完成当前流程", L"warning",
                      L"oauth_session_busy");
        return;
      }
    }
    AppConfig cfg;
    LoadConfig(cfg);
    const bool queryUsage = cfg.enableAutoRefreshQuota;
    const HWND targetHwnd = hwnd_;
    std::thread([targetHwnd, queryUsage]()
                {
      std::wstring status;
      std::wstring code;
      std::wstring savedName;
      const bool ok = StartOAuthLogin(targetHwnd, savedName, status, code);
      SendWebStatusThreadSafe(targetHwnd, status, ok ? L"success" : L"error",
                              code);
      if (ok) {
        PostAsyncWebJson(targetHwnd,
                         BuildAccountsListJson(queryUsage, savedName, L""));
        PostAsyncWebJson(targetHwnd,
                         L"{\"type\":\"status\",\"level\":\"success\",\"code\":"
                         L"\"account_quota_refreshed\",\"message\":\"\"}");
      } })
        .detach();
    return;
  }

  if (action == L"submit_oauth_callback")
  {
    std::wstring callbackUrl = ExtractJsonField(rawMessage, L"callbackUrl");
    callbackUrl = UnescapeJsonString(callbackUrl);
    if (callbackUrl.empty())
    {
      SendWebStatus(L"请粘贴回调链接", L"warning", L"invalid_callback_url");
      return;
    }

    std::wstring authCode;
    std::wstring callbackState;
    std::wstring callbackError;
    if (!ParseOAuthCallbackUrl(callbackUrl, authCode, callbackState,
                               callbackError))
    {
      SendWebStatus(L"回调链接格式无效", L"warning", L"invalid_callback_url");
      return;
    }

    std::lock_guard<std::mutex> lock(g_OAuthCallbackMutex);
    if (g_OAuthCallbackResult == nullptr)
    {
      SendWebStatus(L"当前没有进行中的 OAuth 授权", L"warning",
                    L"oauth_session_not_active");
      return;
    }

    if (!callbackError.empty())
    {
      g_OAuthCallbackResult->error = callbackError;
      g_OAuthCallbackResult->received = true;
    }
    else
    {
      g_OAuthCallbackResult->authCode = authCode;
      g_OAuthCallbackResult->state = callbackState;
      g_OAuthCallbackResult->received = true;
      PostOAuthFlowEvent(hwnd_, L"callback_received", L"", L"");
    }
    SendWebStatus(L"已提交回调链接，正在继续授权流程", L"success",
                  L"oauth_callback_submitted");
    return;
  }

  if (action == L"import_manual_token")
  {
    std::wstring tokenData = ExtractJsonField(rawMessage, L"tokenData");
    tokenData = UnescapeJsonString(tokenData);
    if (tokenData.empty())
    {
      SendWebStatus(L"请粘贴令牌数据", L"warning", L"invalid_name");
      return;
    }

    AppConfig cfg;
    LoadConfig(cfg);
    const bool queryUsage = cfg.enableAutoRefreshQuota;
    const HWND targetHwnd = hwnd_;
    std::thread([targetHwnd, tokenData = std::move(tokenData),
                 queryUsage]() mutable
                {
      EnsureIndexExists();
      IndexData idx;
      LoadIndex(idx);
      std::vector<std::wstring> reservedNames;
      reservedNames.reserve(idx.accounts.size() + 1);
      for (const auto &row : idx.accounts) {
        reservedNames.push_back(row.name);
      }

      std::wstring status;
      std::wstring code;
      std::wstring savedName;
      const bool ok = ImportManualTokenData(tokenData, reservedNames, savedName,
                                            status, code, queryUsage);

      std::wstring level = ok ? L"success" : L"error";
      SendWebStatusThreadSafe(targetHwnd, status, level, code);
      if (ok) {
        PostAsyncWebJson(targetHwnd,
                         BuildAccountsListJson(queryUsage, savedName, L""));
      } })
        .detach();
    return;
  }

  if (action == L"debug_force_refresh_all")
  {
#ifdef _DEBUG
    TriggerRefreshAll();
    SendWebStatus(L"已触发调试：刷新全部额度", L"success", L"debug_action_ok");
#else
    SendWebStatus(L"仅 Debug 构建支持此操作", L"warning",
                  L"debug_action_unsupported");
#endif
    return;
  }

  if (action == L"debug_force_refresh_current")
  {
#ifdef _DEBUG
    TriggerRefreshCurrent();
    SendWebStatus(L"已触发调试：刷新当前账号额度", L"success",
                  L"debug_action_ok");
#else
    SendWebStatus(L"仅 Debug 构建支持此操作", L"warning",
                  L"debug_action_unsupported");
#endif
    return;
  }

  if (action == L"debug_test_low_quota_notify")
  {
#ifdef _DEBUG
    ShowLowQuotaBalloon(L"当前账号", 18, L"5小时", L"测试账号", L"personal", 86);
    SendWebStatus(L"已触发调试：托盘低额度提醒", L"success",
                  L"debug_action_ok");
#else
    SendWebStatus(L"仅 Debug 构建支持此操作", L"warning",
                  L"debug_action_unsupported");
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
    pendingCurrentQuotaWindow_.clear();
    const HWND targetHwnd = hwnd_;
    std::thread([targetHwnd, targetName, targetGroup]()
                {
      std::wstring status;
      std::wstring code;
      const bool ok = SwitchToAccount(targetName, targetGroup, status, code);
      SendWebStatusThreadSafe(targetHwnd, status, ok ? L"success" : L"error",
                              code);
      PostAsyncWebJson(targetHwnd, BuildAccountsListJson(false, L"", L"")); })
        .detach();
    return;
  }

  if (action == L"cancel_low_quota_switch")
  {
    pendingLowQuotaPrompt_ = false;
    pendingBestAccountName_.clear();
    pendingBestAccountGroup_.clear();
    pendingCurrentAccountName_.clear();
    pendingCurrentQuotaWindow_.clear();
    return;
  }

  SendWebStatus(L"未知操作: " + action, L"error", L"unknown_action");
}

void WebViewHost::Initialize(HWND hwnd)
{
  hwnd_ = hwnd;
  g_DebugWebHwnd = hwnd_;
  InitializeTrafficPersistenceOnBoot();
  allRefreshIntervalSec_ = kDefaultAllRefreshMinutes * 60;
  currentRefreshIntervalSec_ = kDefaultCurrentRefreshMinutes * 60;
  webDavSyncIntervalSec_ = kDefaultWebDavSyncMinutes * 60;
  allRefreshRemainingSec_ = allRefreshIntervalSec_;
  currentRefreshRemainingSec_ = currentRefreshIntervalSec_;
  webDavSyncRemainingSec_ = webDavSyncIntervalSec_;
  if (!timerInitialized_)
  {
    SetTimer(hwnd_, kTimerAutoRefresh, 1000, nullptr);
    timerInitialized_ = true;
  }
  EnsureTrayIcon();

  AppConfig startCfg;
  if (!LoadConfig(startCfg))
  {
    AppConfig cfg;
    if (SaveConfig(cfg))
    {
      LoadConfig(startCfg);
    }
  }
  if (startCfg.proxyApiKey.empty())
  {
    startCfg.proxyApiKey = GenerateProxyApiKey();
    const AppConfig saveCfg = startCfg;
    std::thread([saveCfg]()
                { SaveConfig(saveCfg); })
        .detach();
  }
  ApplyWindowTitleTheme(hwnd_, startCfg.theme);
  autoRefreshQuotaDisabled_ = !startCfg.enableAutoRefreshQuota;
  g_ProxyAutoMarkAbnormalAccounts = startCfg.autoMarkAbnormalAccounts;
  g_AutoDeleteAbnormalAccounts = startCfg.autoDeleteAbnormalAccounts;
  currentAutoRefreshEnabled_ = startCfg.autoRefreshCurrent;
  lowQuotaPromptEnabled_ = startCfg.lowQuotaAutoPrompt;
  closeWindowToTray_ = startCfg.closeWindowBehavior != L"exit";
  proxyStealthModeEnabled_ = startCfg.proxyStealthMode;
  g_ProxyPort = startCfg.proxyPort;
  g_ProxyTimeoutSec = startCfg.proxyTimeoutSec;
  g_ProxyAllowLan = startCfg.proxyAllowLan;
  g_ProxyApiKey = startCfg.proxyApiKey;
  g_ProxyDispatchMode = startCfg.proxyDispatchMode;
  g_ProxyFixedAccount = startCfg.proxyFixedAccount;
  g_ProxyFixedGroup = startCfg.proxyFixedGroup;
  cloudAccountAutoDownloadEnabled_ = startCfg.cloudAccountAutoDownload;
  webDavEnabled_ = startCfg.webdavEnabled;
  webDavAutoSyncEnabled_ = startCfg.webdavEnabled && startCfg.webdavAutoSync;
  allRefreshIntervalSec_ = ClampRefreshMinutes(startCfg.autoRefreshAllMinutes,
                                               kDefaultAllRefreshMinutes) *
                          60;
  currentRefreshIntervalSec_ =
      ClampRefreshMinutes(startCfg.autoRefreshCurrentMinutes,
                          kDefaultCurrentRefreshMinutes) *
      60;
  webDavSyncIntervalSec_ = ClampWebDavSyncMinutes(
                              startCfg.webdavSyncIntervalMinutes,
                              kDefaultWebDavSyncMinutes) *
                          60;
  cloudAccountIntervalSec_ = ClampWebDavSyncMinutes(
                                 startCfg.cloudAccountIntervalMinutes,
                                 kDefaultCloudAccountSyncMinutes) *
                             60;
  allRefreshRemainingSec_ = allRefreshIntervalSec_;
  currentRefreshRemainingSec_ = currentRefreshIntervalSec_;
  webDavSyncRemainingSec_ = webDavSyncIntervalSec_;
  cloudAccountRemainingSec_ = cloudAccountIntervalSec_;
  cloudAccountLastDownloadAt_ = startCfg.cloudAccountLastDownloadAt;
  cloudAccountLastDownloadStatus_ = startCfg.cloudAccountLastDownloadStatus;
  if (autoRefreshQuotaDisabled_)
  {
    allRefreshRemainingSec_ = allRefreshIntervalSec_;
    currentRefreshRemainingSec_ = currentRefreshIntervalSec_;
  }

  LPWSTR browserVersion = nullptr;
  const HRESULT browserVersionHr =
      GetAvailableCoreWebView2BrowserVersionString(nullptr, &browserVersion);
  if (browserVersion != nullptr)
  {
    CoTaskMemFree(browserVersion);
  }
  if (IsMissingWebView2RuntimeError(browserVersionHr))
  {
    ShowHr(hwnd, L"GetAvailableCoreWebView2BrowserVersionString",
           browserVersionHr);
    return;
  }

  userDataFolder_ = MakeTempUserDataFolder();
  if (userDataFolder_.empty())
  {
    ShowHr(hwnd, L"MakeTempUserDataFolder", E_FAIL);
    return;
  }

  const HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
      nullptr, userDataFolder_.c_str(), nullptr,
      Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
          [this, hwnd, startCfg](HRESULT result,
                                 ICoreWebView2Environment *env) -> HRESULT
          {
            if (FAILED(result) || env == nullptr)
            {
              ShowHr(hwnd, L"CreateEnvironmentCompleted",
                     FAILED(result) ? result : E_FAIL);
              return S_OK;
            }

            env->CreateCoreWebView2Controller(
                hwnd,
                Callback<
                    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    [this,
                     hwnd, startCfg](HRESULT controllerResult,
                                     ICoreWebView2Controller *controller) -> HRESULT
                    {
                      if (FAILED(controllerResult) || controller == nullptr)
                      {
                        ShowHr(hwnd, L"CreateControllerCompleted",
                               FAILED(controllerResult) ? controllerResult
                                                        : E_FAIL);
                        return S_OK;
                      }

                      controller_ = controller;

                      {
                        const std::wstring cfgTheme = NormalizeTheme(startCfg.theme);
                        const bool isDark =
                            (cfgTheme == L"dark" ||
                             (cfgTheme == L"auto" && IsWindowsAppsDarkModeEnabled()));
                        ComPtr<ICoreWebView2Controller2> controller2;
                        if (SUCCEEDED(controller_->QueryInterface(IID_PPV_ARGS(&controller2))) && controller2)
                        {
                          COREWEBVIEW2_COLOR bg;
                          bg.A = 255;
                          if (isDark)
                          {
                            bg.R = 0x0a;
                            bg.G = 0x0e;
                            bg.B = 0x17;
                          }
                          else
                          {
                            bg.R = 0xfa;
                            bg.G = 0xf7;
                            bg.B = 0xf2;
                          }
                          controller2->put_DefaultBackgroundColor(bg);
                        }
                      }

                      controller_->put_IsVisible(TRUE);

                      HRESULT hr2 = controller_->get_CoreWebView2(&webview_);
                      if (FAILED(hr2) || !webview_)
                      {
                        ShowHr(hwnd, L"get_CoreWebView2",
                               FAILED(hr2) ? hr2 : E_FAIL);
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
                        const wchar_t *notFoundHtml =
                            LR"(<html><body style='font-family:Segoe UI;padding:24px;'><h2>WebUI not found</h2><p>Please make sure <code>webui/index.html</code> exists.</p></body></html>)";
                        webview_->NavigateToString(notFoundHtml);
                        return S_OK;
                      }

                      std::wstring fileUrl = ToFileUrl(webUiPath);
#ifdef _DEBUG
                      fileUrl += L"?debug=1";
#endif

                      {
                        const std::wstring themeScript =
                            L"try { localStorage.setItem('cas_theme', '" +
                            NormalizeTheme(startCfg.theme) +
                            L"'); } catch(e) {}";
                        webview_->AddScriptToExecuteOnDocumentCreated(themeScript.c_str(), nullptr);
                      }
                      hr2 = webview_->Navigate(fileUrl.c_str());
                      if (FAILED(hr2))
                      {
                        ShowHr(hwnd, L"Navigate", hr2);
                      }

                      const HWND targetHwnd = hwnd_;
                      std::thread([startCfg, targetHwnd]()
                                  {
                        if (startCfg.proxyStealthMode) {
                          std::wstring stealthError;
                          if (!ApplyStealthProxyModeToCodexProfile(startCfg,
                                                                   stealthError)) {
                            DebugLogLine(L"proxy.stealth",
                                         L"apply on startup failed: " +
                                             stealthError);
                          }
                        }
                        std::wstring envError;
                        if (!SyncStealthProxyEnvironment(startCfg, envError)) {
                          DebugLogLine(L"proxy.stealth",
                                       L"sync env on startup failed: " +
                                           envError);
                        }
                        if (startCfg.proxyAutoStart) {
                          std::wstring proxyStatus;
                          std::wstring proxyCode;
                          StartLocalProxyService(
                              targetHwnd, startCfg.proxyPort,
                              startCfg.proxyTimeoutSec, startCfg.proxyAllowLan,
                              proxyStatus, proxyCode);
                        } })
                          .detach();

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
  if (g_ProxyRunning.load())
  {
    std::wstring proxyStatus;
    std::wstring proxyCode;
    StopLocalProxyService(proxyStatus, proxyCode);
  }
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
  pendingCurrentQuotaWindow_.clear();
  hwnd_ = nullptr;
  g_DebugWebHwnd = nullptr;
}

bool WebViewHost::HandleWindowMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
  if (msg == WM_COMMAND)
  {
    if (HandleTrayCommand(static_cast<UINT>(LOWORD(wParam))))
    {
      return true;
    }
  }

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
    if (evt == WM_RBUTTONUP || evtLo == WM_RBUTTONUP || evtHi == WM_RBUTTONUP ||
        evt == WM_CONTEXTMENU || evtLo == WM_CONTEXTMENU ||
        evtHi == WM_CONTEXTMENU)
    {
      ShowTrayContextMenu();
      return true;
    }
    if (evt == NIN_BALLOONUSERCLICK || evtLo == NIN_BALLOONUSERCLICK ||
        evtHi == NIN_BALLOONUSERCLICK || evt == WM_LBUTTONUP ||
        evtLo == WM_LBUTTONUP || evtHi == WM_LBUTTONUP)
    {
      HandleLowQuotaPromptClick();
    }
    return true;
  }

  if (msg == kMsgLowQuotaCandidate)
  {
    auto *heapPayload = reinterpret_cast<LowQuotaCandidatePayload *>(lParam);
    if (heapPayload == nullptr)
    {
      return true;
    }
    const LowQuotaCandidatePayload payload = *heapPayload;
    delete heapPayload;

    const auto now = std::chrono::steady_clock::now();
    const bool cooledDown =
        lastLowQuotaPromptAt_.time_since_epoch().count() == 0 ||
        std::chrono::duration_cast<std::chrono::seconds>(now -
                                                         lastLowQuotaPromptAt_)
                .count() >= kLowQuotaPromptCooldownSeconds;
    if (lowQuotaPromptEnabled_ && hwnd_ != nullptr &&
        !payload.bestName.empty() &&
        (!EqualsIgnoreCase(lastLowQuotaPromptAccountKey_, payload.accountKey) ||
         cooledDown))
    {
      lastLowQuotaPromptAt_ = now;
      lastLowQuotaPromptAccountKey_ = payload.accountKey;
      if (proxyStealthModeEnabled_)
      {
        const HWND targetHwnd = hwnd_;
        const std::wstring bestName = payload.bestName;
        const std::wstring bestGroup = payload.bestGroup;
        std::thread([targetHwnd, bestName, bestGroup]()
                    {
          std::wstring status;
          std::wstring code;
          const bool ok = SwitchToAccount(bestName, bestGroup, status, code);
          if (ok) {
            SendWebStatusThreadSafe(
                targetHwnd,
                Tr(L"status_text.low_quota_auto_switched",
                   L"当前账号额度过低，已自动切换到额度最高的账号"),
                L"warning", L"");
            PostAsyncWebJson(targetHwnd, BuildAccountsListJson(false, L"", L""));
          } else {
            SendWebStatusThreadSafe(targetHwnd, status, L"error", code);
          } })
            .detach();
      }
      else
      {
        ShowLowQuotaBalloon(payload.currentName, payload.currentQuota,
                            payload.currentWindow, payload.bestName,
                            payload.bestGroup, payload.bestQuota);
      }
    }
    return true;
  }

  if (msg != kMsgAsyncWebJson)
  {
    return false;
  }

  std::wstring *heapJson = reinterpret_cast<std::wstring *>(lParam);
  if (heapJson == nullptr)
  {
    return true;
  }

  SendWebJson(*heapJson);
  delete heapJson;
  return true;
}
