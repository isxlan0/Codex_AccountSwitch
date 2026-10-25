#pragma once
// Included after api_profile_mode.h inside webview_host.cpp's anonymous namespace.
std::wstring BuildApiManagementJson();
extern std::wstring g_ProxyApiKey;

std::wstring CodexSyncTomlString(const std::wstring &toml, const std::wstring &section,
                               const std::wstring &key)
{
  const auto raw = ApiTomlRawValue(toml, section, key);
  if (raw.size() < 2 || (raw.front() != L'"' && raw.front() != L'\'') || raw.back() != raw.front()) return L"";
  if (raw.size() >= 3 && raw[0] == raw[1] && raw[1] == raw[2] &&
      (raw.size() < 6 || raw.substr(raw.size() - 3) != raw.substr(0, 3))) return L"";
  return ApiTomlValue(toml, section, key);
}

std::wstring CodexSyncJsonValue(const std::wstring &json, const std::wstring &key,
                              const std::wstring &value)
{
  auto fields = ApiJsonMembers(json);
  fields[key] = value;
  std::wstring result = L"{";
  for (const auto &field : fields)
  {
    if (result.size() > 1) result += L",";
    result += L"\"" + EscapeJsonString(field.first) + L"\":" + field.second;
  }
  return result + L"}";
}

bool CodexSyncAuthComplete(const std::wstring &text)
{
  const auto json = TrimWide(text);
  if (json.size() < 2 || json.front() != L'{' || json.back() != L'}') return false;
  std::wstring brackets;
  bool quoted = false;
  for (size_t i = 0; i < json.size(); ++i)
  {
    const auto c = json[i];
    if (quoted)
    {
      if (c == L'\\') { if (++i >= json.size()) return false; }
      else if (c == L'"') quoted = false;
    }
    else if (c == L'"') quoted = true;
    else if (c == L'{' || c == L'[') brackets.push_back(c);
    else if (c == L'}' || c == L']')
    {
      if (brackets.empty() || brackets.back() != (c == L'}' ? L'{' : L'[')) return false;
      brackets.pop_back();
      if (brackets.empty() && i + 1 != json.size()) return false;
    }
  }
  return !quoted && brackets.empty();
}

bool CodexSyncSameStamp(const WIN32_FILE_ATTRIBUTE_DATA &a, const WIN32_FILE_ATTRIBUTE_DATA &b)
{
  return a.dwFileAttributes == b.dwFileAttributes &&
         CompareFileTime(&a.ftLastWriteTime, &b.ftLastWriteTime) == 0 &&
         a.nFileSizeHigh == b.nFileSizeHigh && a.nFileSizeLow == b.nFileSizeLow;
}

bool SyncCurrentCodexConfigToApp(HWND hwnd, bool force)
{
  std::lock_guard<std::recursive_mutex> profileLock(g_CodexProfileMutex);
  if (g_CodexProfileClosing) return false;
  static bool observed = false;
  static WIN32_FILE_ATTRIBUTE_DATA lastAuthStamp{}, lastTomlStamp{};
  static std::wstring lastAuth, lastToml;
  static std::wstring lastFailure;
  const auto home = GetCodexHomeDir();
  const auto authPath = home / L"auth.json", tomlPath = home / L"config.toml";
  WIN32_FILE_ATTRIBUTE_DATA authStamp{}, tomlStamp{}, authAfter{}, tomlAfter{};
  auto readStamps = [&](WIN32_FILE_ATTRIBUTE_DATA &auth, WIN32_FILE_ATTRIBUTE_DATA &toml) {
    auth = {};
    if (!GetFileAttributesExW(authPath.c_str(), GetFileExInfoStandard, &auth))
    {
      const auto error = GetLastError();
      if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) return false;
    }
    return GetFileAttributesExW(tomlPath.c_str(), GetFileExInfoStandard, &toml) != FALSE;
  };
  auto fail = [&](const wchar_t *message) {
    if (lastFailure != message) DebugLogLine(L"codex.config.sync", message);
    lastFailure = message;
    return false;
  };
  if (!readStamps(authStamp, tomlStamp)) return fail(L"Cannot inspect local Codex configuration files");
  if (!force && observed && CodexSyncSameStamp(authStamp, lastAuthStamp) &&
      CodexSyncSameStamp(tomlStamp, lastTomlStamp)) return true;
  std::wstring auth = L"{}", toml;
  if ((authStamp.dwFileAttributes != 0 && !ReadUtf8File(authPath, auth)) || !ReadUtf8File(tomlPath, toml))
    return fail(L"Cannot read local Codex configuration files");
  if (!readStamps(authAfter, tomlAfter) || !CodexSyncSameStamp(authStamp, authAfter) ||
      !CodexSyncSameStamp(tomlStamp, tomlAfter))
    return fail(L"Local Codex configuration changed while being read; retry next tick");
  auto accept = [&]() {
    lastFailure.clear();
    observed = true; lastAuthStamp = authAfter; lastTomlStamp = tomlAfter;
    lastAuth = auth; lastToml = toml;
    return true;
  };
  if ((auth == g_LastWrittenCodexAuth && toml == g_LastWrittenCodexToml) ||
      (observed && auth == lastAuth && toml == lastToml)) return accept();
  if (!CodexSyncAuthComplete(auth)) return fail(L"Local auth.json is incomplete; retry next tick");
  const auto model = CodexSyncTomlString(toml, L"", L"model");
  if (model.empty()) return fail(L"Local configuration has no complete model value; retry next tick");
  const auto provider = CodexSyncTomlString(toml, L"", L"model_provider");
  const auto apiKey = ApiKeyFromCodexConfig(auth, toml);
  AppConfig cfg;
  if (!LoadConfig(cfg)) return fail(L"Cannot read application configuration");
  const auto baseUrl = CodexSyncTomlString(toml, L"model_providers." + provider, L"base_url");
  const auto ownProxyUrl = L"http://127.0.0.1:" + std::to_wstring(cfg.proxyPort) + L"/v1";
  const bool localProxy = provider == L"cas_local_proxy" && baseUrl == ownProxyUrl;
  bool configChanged = cfg.proxyDefaultModel != model;
  cfg.proxyDefaultModel = model;
  if (cfg.proxyStealthMode != localProxy)
  {
    cfg.proxyStealthMode = localProxy;
    configChanged = true;
  }
  if (localProxy && !apiKey.empty() && cfg.proxyApiKey != apiKey)
  {
    cfg.proxyApiKey = apiKey;
    configChanged = true;
  }
  if (!localProxy && apiKey.empty() && !cfg.activeApiAccount.empty())
  {
    cfg.activeApiAccount.clear();
    configChanged = true;
  }
  bool accountsChanged = false;
  bool incompleteApi = false;
  if (localProxy || !apiKey.empty())
  {
    const auto wireApi = CodexSyncTomlString(toml, L"model_providers." + provider, L"wire_api");
    incompleteApi = !localProxy && (provider.empty() || baseUrl.empty() || (!wireApi.empty() && wireApi != L"responses"));
    if (!incompleteApi)
    {
      std::unique_lock<std::mutex> accountsLock(g_apiAccountsMutex);
      auto accounts = LoadApiAccountsUnlocked();
      auto before = ApiAccountsItemsJson(accounts);
      const auto matchesCurrent = [&](const ApiAccount &a) {
        return a.baseUrl == baseUrl && a.apiKey == apiKey && a.model == model;
      };
      // activeApiAccount is a selection, not proof that disk credentials belong
      // to that saved record. Disk can contain a different account after exit
      // restoration or an external switch; never overwrite an edited record.
      auto selected = std::find_if(accounts.begin(), accounts.end(), [&](const ApiAccount &a) {
        return a.id == cfg.activeApiAccount && (localProxy || matchesCurrent(a));
      });
      if (!localProxy && selected == accounts.end())
      {
        selected = std::find_if(accounts.begin(), accounts.end(), matchesCurrent);
        if (selected == accounts.end())
        {
          const auto source = L"codex-current:" + home.wstring();
          const bool sourceConflict = std::any_of(accounts.begin(), accounts.end(), [&](const ApiAccount &a) { return a.sourceId == source; });
          ApiAccount imported;
          std::wstring error;
          if (!ApiAccountFromCodexConfig(auth, toml, imported, error)) return fail(L"Cannot parse current API configuration");
          imported.id = GenerateRandomBase64Url(18); imported.name = provider;
          imported.sourceId = sourceConflict ? source + L":" + imported.id : source;
          accounts.push_back(imported); selected = accounts.end() - 1;
        }
        cfg.activeApiAccount = selected->id; configChanged = true;
      }
      if (selected != accounts.end())
      {
        if (!localProxy)
        {
          const auto metadata = ApiJsonMembers(selected->metadata);
          const auto existing = metadata.find(L"settingsConfig");
          const auto settingsJson = existing == metadata.end() ? L"{}" : existing->second;
          const auto settingsFields = ApiJsonMembers(settingsJson);
          const auto existingAuth = settingsFields.find(L"auth");
          if (existingAuth == settingsFields.end() || existingAuth->second != TrimWide(auth) ||
              ApiJsonString(settingsFields, L"config") != toml)
          {
            auto settings = CodexSyncJsonValue(settingsJson, L"auth", auth);
            settings = CodexSyncJsonValue(settings, L"config", L"\"" + EscapeJsonString(toml) + L"\"");
            selected->metadata = CodexSyncJsonValue(selected->metadata, L"settingsConfig", settings);
          }
        }
      }
      if (ApiAccountsItemsJson(accounts) != before)
      {
        if (!SaveApiAccountsUnlocked(accounts)) return fail(L"Cannot persist synchronized API account");
        accountsChanged = true;
      }
    }
  }
  if (configChanged && !SaveConfig(cfg)) return fail(L"Cannot persist synchronized application configuration");
  if (localProxy) g_ProxyApiKey = cfg.proxyApiKey;
  if (configChanged || accountsChanged)
  {
    PostAsyncWebJson(hwnd, BuildApiManagementJson());
    PostAsyncWebJson(hwnd, L"{\"type\":\"codex_config_synced\",\"accountId\":\"" +
                          EscapeJsonString(cfg.activeApiAccount) + L"\"}");
  }
  if (incompleteApi) return fail(L"Local API connection fields are incomplete; retry next tick");
  return accept();
}
