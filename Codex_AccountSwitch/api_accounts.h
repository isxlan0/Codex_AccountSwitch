#pragma once
// Implementation fragment: include inside webview_host.cpp's anonymous namespace.
// winsqlite3.h must be included at file scope.
#pragma comment(lib, "winsqlite3.lib")

struct ApiAccount
{
  std::wstring id, name, baseUrl, apiKey, model, sourceId, usageScript, balanceConfig, lastError, metadata;
  bool inPool = false;
};

std::mutex g_apiAccountsMutex;

// Keep object values as raw JSON, particularly CC Switch's usage_script object.
struct ApiJsonMemberRange
{
  size_t keyStart, valueStart, valueLength;
};

std::map<std::wstring, std::wstring> ApiJsonMembers(const std::wstring &json,
    std::map<std::wstring, ApiJsonMemberRange> *ranges = nullptr)
{
  std::map<std::wstring, std::wstring> fields;
  size_t p = json.find(L'{');
  if (p == std::wstring::npos) return fields;
  ++p;
  while (p < json.size())
  {
    while (p < json.size() && (iswspace(json[p]) || json[p] == L',')) ++p;
    if (p == json.size() || json[p] != L'"') break;
    const size_t keyStart = ++p;
    while (p < json.size() && json[p] != L'"')
    {
      if (json[p] == L'\\') ++p;
      ++p;
    }
    const std::wstring key = UnescapeJsonString(json.substr(keyStart, p - keyStart));
    ++p;
    while (p < json.size() && iswspace(json[p])) ++p;
    if (p == json.size() || json[p++] != L':') break;
    while (p < json.size() && iswspace(json[p])) ++p;
    const size_t start = p;
    int depth = 0;
    bool quoted = false;
    for (; p < json.size(); ++p)
    {
      const wchar_t c = json[p];
      if (quoted)
      {
        if (c == L'\\') ++p;
        else if (c == L'"') quoted = false;
      }
      else if (c == L'"') quoted = true;
      else if (c == L'{' || c == L'[') ++depth;
      else if (c == L'}' || c == L']')
      {
        if (depth == 0) break;
        --depth;
      }
      else if (c == L',' && depth == 0) break;
    }
    fields[key] = TrimWide(json.substr(start, p - start));
    if (ranges) (*ranges)[key] = {keyStart - 1, start, p - start};
  }
  return fields;
}

std::wstring PatchProfileApiKey(std::wstring json, const std::wstring &rawValue)
{
  std::map<std::wstring, ApiJsonMemberRange> ranges;
  const auto fields = ApiJsonMembers(json, &ranges);
  const auto it = ranges.find(L"OPENAI_API_KEY");
  if (it != ranges.end())
  {
    const auto &range = it->second;
    if (!rawValue.empty()) json.replace(range.valueStart, range.valueLength, rawValue);
    else
    {
      auto start = range.keyStart;
      auto end = range.valueStart + range.valueLength;
      if (end < json.size() && json[end] == L',') ++end;
      else
      {
        while (start > 0 && iswspace(json[start - 1])) --start;
        if (start > 0 && json[start - 1] == L',') --start;
      }
      json.erase(start, end - start);
    }
  }
  else if (!rawValue.empty())
  {
    if (TrimWide(json).empty()) json = L"{}";
    const auto end = json.rfind(L'}');
    json.insert(end, (fields.empty() ? L"" : L",") + std::wstring(L"\"OPENAI_API_KEY\":") + rawValue);
  }
  return json;
}

std::wstring ProfileApiKeyRaw(const std::wstring &json)
{
  const auto fields = ApiJsonMembers(json);
  const auto key = fields.find(L"OPENAI_API_KEY");
  return key == fields.end() ? L"" : key->second;
}

std::wstring ApiJsonString(const std::map<std::wstring, std::wstring> &fields,
                           const std::wstring &key)
{
  const auto it = fields.find(key);
  if (it == fields.end() || it->second.size() < 2 || it->second.front() != L'"') return L"";
  return UnescapeJsonString(it->second.substr(1, it->second.size() - 2));
}

ApiAccount ApiAccountFromJson(const std::wstring &json)
{
  const auto f = ApiJsonMembers(json);
  ApiAccount a;
  a.id = ApiJsonString(f, L"id");
  a.name = ApiJsonString(f, L"name");
  a.baseUrl = ApiJsonString(f, L"baseUrl");
  a.apiKey = ApiJsonString(f, L"apiKey");
  a.model = ApiJsonString(f, L"model");
  a.sourceId = ApiJsonString(f, L"sourceId");
  a.lastError = ApiJsonString(f, L"lastError");
  const auto pool = f.find(L"inPool");
  a.inPool = pool != f.end() && pool->second == L"true";
  const auto script = f.find(L"usageScript");
  if (script != f.end() && !script->second.empty() && script->second.front() == L'{') a.usageScript = script->second;
  const auto balance = f.find(L"balanceConfig");
  if (balance != f.end() && !balance->second.empty() && balance->second.front() == L'{') a.balanceConfig = balance->second;
  const auto metadata = f.find(L"metadata");
  if (metadata != f.end() && !metadata->second.empty() && metadata->second.front() == L'{') a.metadata = metadata->second;
  return a;
}

std::wstring ApiAccountsItemsJson(const std::vector<ApiAccount> &accounts)
{
  std::wostringstream out;
  out << L"[";
  bool first = true;
  for (const auto &a : accounts)
  {
    if (!first) out << L",";
    first = false;
    out << L"{\"id\":\"" << EscapeJsonString(a.id)
        << L"\",\"name\":\"" << EscapeJsonString(a.name)
        << L"\",\"baseUrl\":\"" << EscapeJsonString(a.baseUrl)
        << L"\",\"apiKey\":\"" << EscapeJsonString(a.apiKey)
        << L"\",\"model\":\"" << EscapeJsonString(a.model)
        << L"\",\"sourceId\":\"" << EscapeJsonString(a.sourceId)
        << L"\",\"lastError\":\"" << EscapeJsonString(a.lastError)
        << L"\",\"inPool\":" << (a.inPool ? L"true" : L"false")
        << L",\"usageScript\":" << (a.usageScript.empty() ? L"null" : a.usageScript)
        << L",\"metadata\":" << (a.metadata.empty() ? L"null" : a.metadata)
        << L",\"balanceConfig\":" << (a.balanceConfig.empty() ? L"null" : a.balanceConfig) << L"}";
  }
  out << L"]";
  return out.str();
}

std::vector<ApiAccount> LoadApiAccountsUnlocked()
{
  std::wstring json;
  std::vector<ApiAccount> result;
  if (!ReadUtf8File(GetUserDataRoot() / L"api_accounts.json", json)) return result;
  std::wstring items;
  if (!ExtractJsonArrayField(json, L"items", items)) return result;
  for (const auto &row : ExtractTopLevelObjectsFromArray(items)) result.push_back(ApiAccountFromJson(row));
  return result;
}

bool SaveApiAccountsUnlocked(const std::vector<ApiAccount> &accounts)
{
  return WriteUtf8File(GetUserDataRoot() / L"api_accounts.json", L"{\"items\":" + ApiAccountsItemsJson(accounts) + L"}");
}

std::vector<ApiAccount> LoadApiAccounts()
{
  std::lock_guard<std::mutex> lock(g_apiAccountsMutex);
  return LoadApiAccountsUnlocked();
}

bool SaveApiAccounts(const std::vector<ApiAccount> &accounts)
{
  std::lock_guard<std::mutex> lock(g_apiAccountsMutex);
  return SaveApiAccountsUnlocked(accounts);
}

std::wstring BuildApiAccountsJson()
{
  return L"{\"type\":\"api_accounts\",\"items\":" + ApiAccountsItemsJson(LoadApiAccounts()) + L"}";
}

bool FindApiAccount(const std::wstring &id, ApiAccount &out)
{
  for (const auto &a : LoadApiAccounts()) if (a.id == id) { out = a; return true; }
  return false;
}

std::wstring ApiKeyFromCodexConfig(const std::wstring &auth, const std::wstring &config)
{
  const auto provider = ApiTomlValue(config, L"", L"model_provider");
  const auto section = provider.empty() ? L"" : L"model_providers." + provider;
  const auto bearer = ApiTomlValue(config, section, L"experimental_bearer_token");
  if (!bearer.empty()) return bearer;
  return ApiJsonString(ApiJsonMembers(auth), L"OPENAI_API_KEY");
}

bool ApiAccountFromCodexConfig(const std::wstring &auth, const std::wstring &config,
                               ApiAccount &account, std::wstring &error);

bool SaveApiAccountFromJson(const std::wstring &json, std::wstring &error)
{
  error.clear();
  ApiAccount a = ApiAccountFromJson(json);
  a.name = TrimWide(a.name);
  a.baseUrl = TrimWide(a.baseUrl);
  a.apiKey = TrimWide(a.apiKey);
  a.model = TrimWide(a.model);
  if (a.name.empty() || a.baseUrl.empty() || a.apiKey.empty() || a.model.empty())
  { error = L"名称、Base URL、API Key 和模型不能为空"; return false; }
  const auto metadata = ApiJsonMembers(a.metadata);
  const auto settings = metadata.find(L"settingsConfig");
  if (settings != metadata.end())
  {
    const auto fields = ApiJsonMembers(settings->second);
    const auto auth = fields.find(L"auth");
    const auto config = ApiJsonString(fields, L"config");
    ApiAccount fromConfig;
    if (auth == fields.end() || !ApiAccountFromCodexConfig(auth->second, config, fromConfig, error)) return false;
    if (fromConfig.apiKey != a.apiKey || fromConfig.baseUrl != a.baseUrl || fromConfig.model != a.model)
    { error = L"API 字段与 auth.json / config.toml 不一致，请同步配置后保存"; return false; }
  }
  std::lock_guard<std::mutex> lock(g_apiAccountsMutex);
  auto accounts = LoadApiAccountsUnlocked();
  if (a.id.empty())
  {
    a.id = GenerateRandomBase64Url(18);
    accounts.push_back(a);
  }
  else
  {
    auto it = std::find_if(accounts.begin(), accounts.end(), [&](const ApiAccount &row) { return row.id == a.id; });
    if (it == accounts.end()) { error = L"API 账号不存在"; return false; }
    // Editing never changes import identity; this is owned by the importer.
    a.sourceId = it->sourceId;
    a.inPool = it->inPool;
    *it = a;
  }
  if (!SaveApiAccountsUnlocked(accounts)) { error = L"写入 API 账号失败"; return false; }
  return true;
}

bool DeleteApiAccount(const std::wstring &id, std::wstring &error)
{
  error.clear();
  std::lock_guard<std::mutex> lock(g_apiAccountsMutex);
  auto accounts = LoadApiAccountsUnlocked();
  const auto it = std::find_if(accounts.begin(), accounts.end(), [&](const ApiAccount &a) { return a.id == id; });
  if (it == accounts.end()) { error = L"API 账号不存在"; return false; }
  accounts.erase(it);
  if (!SaveApiAccountsUnlocked(accounts)) { error = L"写入 API 账号失败"; return false; }
  return true;
}

std::wstring ApiTomlRawValue(const std::wstring &toml, const std::wstring &sectionName, const std::wstring &key)
{
  std::wstring section;
  for (size_t start = 0; start < toml.size();)
  {
    const auto end = toml.find(L'\n', start);
    auto next = end == std::wstring::npos ? toml.size() : end + 1;
    const auto line = TrimWide(toml.substr(start, next - start));
    if (!line.empty() && line.front() == L'[')
    {
      const auto close = ProfileTomlHeaderEnd(line);
      if (close != std::wstring::npos) section = ProfileTomlName(line.substr(1, close - 1));
    }
    else if (!line.empty() && line.front() != L'#')
    {
      const auto eq = toml.find(L'=', start);
      if (eq < next)
      {
        const auto valueStart = eq + 1;
        const auto valueEnd = ProfileTomlValueEnd(toml, valueStart);
        const auto lastLine = toml.find(L'\n', valueEnd);
        next = lastLine == std::wstring::npos ? toml.size() : lastLine + 1;
        if (section == ProfileTomlName(sectionName) &&
            ProfileTomlName(toml.substr(start, eq - start)) == ProfileTomlName(key))
        {
          return TrimWide(toml.substr(valueStart, valueEnd - valueStart));
        }
      }
    }
    start = next;
  }
  return L"";
}

std::wstring ApiTomlValue(const std::wstring &toml, const std::wstring &sectionName, const std::wstring &key)
{
  auto value = ApiTomlRawValue(toml, sectionName, key);
  if (value.size() >= 6 && (value.substr(0, 3) == L"\"\"\"" || value.substr(0, 3) == L"'''"))
    value = value.substr(2, value.size() - 4);
  if (value.size() >= 2 && (value.front() == L'"' || value.front() == L'\''))
  {
    const auto text = value.substr(1, value.size() - 2);
    return value.front() == L'"' ? UnescapeJsonString(text) : text;
  }
  return value;
}

bool ApiAccountFromCodexConfig(const std::wstring &auth, const std::wstring &config,
                               ApiAccount &account, std::wstring &error)
{
  account.apiKey = ApiKeyFromCodexConfig(auth, config);
  if (account.apiKey.empty()) { error = L"缺少活动供应商 experimental_bearer_token 或 OPENAI_API_KEY（官方 OAuth 账号不导入）"; return false; }
  const auto provider = ApiTomlValue(config, L"", L"model_provider");
  if (provider.empty()) { error = L"配置缺少活动 model_provider"; return false; }
  const std::wstring section = L"model_providers." + provider;
  account.baseUrl = ApiTomlValue(config, section, L"base_url");
  account.model = ApiTomlValue(config, L"", L"model");
  const auto wireApi = ApiTomlValue(config, section, L"wire_api");
  if (!wireApi.empty() && wireApi != L"responses") { error = L"仅支持 wire_api = responses"; return false; }
  if (account.baseUrl.empty() || account.model.empty()) { error = L"配置缺少活动提供商 base_url 或 model"; return false; }
  account.metadata = L"{\"settingsConfig\":{\"auth\":" + auth + L",\"config\":\"" + EscapeJsonString(config) + L"\"}}";
  return true;
}

void UpsertImportedApiAccount(std::vector<ApiAccount> &accounts, ApiAccount account)
{
  auto it = std::find_if(accounts.begin(), accounts.end(), [&](const ApiAccount &a) { return a.sourceId == account.sourceId; });
  if (it == accounts.end())
  {
    account.id = GenerateRandomBase64Url(18);
    account.inPool = false;
    accounts.push_back(account);
  }
  else
  {
    account.id = it->id;
    account.inPool = it->inPool;
    account.balanceConfig = it->balanceConfig;
    if (!ApiJsonString(ApiJsonMembers(it->balanceConfig), L"url").empty() &&
        !ExtractJsonBoolField(it->usageScript, L"enabled", false))
      account.usageScript = it->usageScript;
    *it = account;
  }
}

bool ImportCurrentApiAccount(std::wstring &error)
{
  error.clear();
  std::wstring auth, config;
  const auto home = GetCodexHomeDir();
  std::error_code ec;
  const bool authExists = fs::exists(home / L"auth.json", ec);
  auth = L"{}";
  if (ec || (authExists && !ReadUtf8File(home / L"auth.json", auth)) || !ReadUtf8File(home / L"config.toml", config))
  { error = L"读取当前 Codex auth.json / config.toml 失败"; return false; }
  ApiAccount account;
  if (!ApiAccountFromCodexConfig(auth, config, account, error)) return false;
  account.name = ApiTomlValue(config, L"", L"model_provider");
  account.sourceId = L"codex-current:" + home.wstring();
  std::lock_guard<std::mutex> lock(g_apiAccountsMutex);
  auto accounts = LoadApiAccountsUnlocked();
  UpsertImportedApiAccount(accounts, account);
  if (!SaveApiAccountsUnlocked(accounts)) { error = L"写入 API 账号失败"; return false; }
  return true;
}

bool ImportCcSwitchApiAccounts(const std::wstring &dbPath, std::wstring &summary)
{
  summary.clear();
  sqlite3 *db = nullptr;
  const auto path = WideToUtf8(dbPath);
  if (sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK)
  {
    summary = L"打开 CC Switch 数据库失败：" + (db ? Utf8ToWide(sqlite3_errmsg(db)) : L"SQLite open failed");
    if (db) sqlite3_close(db);
    return false;
  }
  sqlite3_stmt *stmt = nullptr;
  const char *sql = "SELECT id, name, settings_config, meta FROM providers WHERE app_type = 'codex'";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
  {
    summary = L"读取 CC Switch providers 失败：" + Utf8ToWide(sqlite3_errmsg(db));
    sqlite3_close(db);
    return false;
  }
  std::vector<ApiAccount> imported;
  std::wostringstream skipped;
  int skippedCount = 0;
  int rc;
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
  {
    auto column = [&](int n) -> std::wstring {
      const auto p = sqlite3_column_text(stmt, n);
      return p ? Utf8ToWide(reinterpret_cast<const char *>(p)) : L"";
    };
    ApiAccount account;
    account.sourceId = L"cc-switch:" + column(0);
    account.name = column(1);
    const auto settings = ApiJsonMembers(column(2));
    const auto auth = settings.find(L"auth");
    std::wstring error;
    if (!ApiAccountFromCodexConfig(auth == settings.end() ? L"{}" : auth->second, ApiJsonString(settings, L"config"), account, error))
    {
      ++skippedCount;
      skipped << L"\n" << account.name << L"：" << (error.empty() ? L"缺少 auth 配置" : error);
      continue;
    }
    const auto meta = ApiJsonMembers(column(3));
    auto script = meta.find(L"usage_script");
    if (script == meta.end()) script = meta.find(L"usageScript");
    if (script != meta.end() && !script->second.empty() && script->second.front() == L'{') account.usageScript = script->second;
    imported.push_back(account);
  }
  if (rc != SQLITE_DONE) summary = L"读取 CC Switch 数据库失败：" + Utf8ToWide(sqlite3_errmsg(db));
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  if (rc != SQLITE_DONE) return false;
  if (!imported.empty())
  {
    std::lock_guard<std::mutex> lock(g_apiAccountsMutex);
    auto accounts = LoadApiAccountsUnlocked();
    for (const auto &a : imported) UpsertImportedApiAccount(accounts, a);
    if (!SaveApiAccountsUnlocked(accounts)) { summary = L"写入 API 账号失败"; return false; }
  }
  summary = L"已导入/更新 " + std::to_wstring(imported.size()) + L" 个 API 账号，跳过 " + std::to_wstring(skippedCount) + L" 个" + skipped.str();
  return true;
}

std::mutex g_apiProfileMutex;

std::wstring BuildApiCodexToml(const std::wstring &original, const ApiAccount &account)
{
  // Only connection fields belong to the account switcher. A stored editor
  // snapshot must never replace unrelated local keys or whole tables.
  const std::wstring provider = L"cas_api";
  auto result = PatchProfileTomlField(original, L"", L"model", L"\"" + EscapeJsonString(account.model) + L"\"");
  result = PatchProfileTomlField(result, L"", L"model_provider", L"\"" + EscapeJsonString(provider) + L"\"");
  const auto section = L"model_providers." + provider;
  if (ApiTomlRawValue(original, section, L"name").empty())
    result = PatchProfileTomlField(result, section, L"name", L"\"Codex AccountSwitch API\"");
  result = PatchProfileTomlField(result, section, L"base_url", L"\"" + EscapeJsonString(account.baseUrl) + L"\"");
  result = PatchProfileTomlField(result, section, L"wire_api", L"\"responses\"");
  result = PatchProfileTomlField(result, section, L"requires_openai_auth", L"true");
  if (!ApiTomlRawValue(original, section, L"experimental_bearer_token").empty())
    result = PatchProfileTomlField(result, section, L"experimental_bearer_token", L"\"" + EscapeJsonString(account.apiKey) + L"\"");
  return result;
}

std::wstring BuildApiEditorConfigJson(const std::wstring &request)
{
  std::lock_guard<std::recursive_mutex> profileLock(g_CodexProfileMutex);
  const auto fields = ApiJsonMembers(request);
  const auto requestId = ApiJsonString(fields, L"requestId");
  const auto home = GetCodexHomeDir();
  std::wstring auth, config, error;
  std::error_code ec;
  const bool authExists = fs::exists(home / L"auth.json", ec);
  if (ec || (authExists && !ReadUtf8File(home / L"auth.json", auth)))
    error = L"读取本机完整 auth.json 失败";
  const bool configExists = fs::exists(home / L"config.toml", ec);
  if (ec || (configExists && !ReadUtf8File(home / L"config.toml", config)))
    error = L"读取本机完整 config.toml 失败";
  const auto prefix = L"{\"type\":\"api_editor_config_result\",\"requestId\":\"" + EscapeJsonString(requestId);
  if (!error.empty()) return prefix + L"\",\"ok\":false,\"error\":\"" + EscapeJsonString(error) + L"\"}";
  const auto draft = ApiAccountFromJson(request);
  auth = PatchProfileApiKey(auth, L"\"" + EscapeJsonString(draft.apiKey) + L"\"");
  config = BuildApiCodexToml(config, draft);
  return prefix + L"\",\"ok\":true,\"auth\":" + auth + L",\"config\":\"" + EscapeJsonString(config) + L"\"}";
}

bool ApplyApiAccountToCodexProfile(const ApiAccount &account, std::wstring &error)
{
  error.clear();
  std::lock_guard<std::recursive_mutex> profileLock(g_CodexProfileMutex);
  std::lock_guard<std::mutex> lock(g_apiProfileMutex);
  if (g_CodexProfileClosing) { error = L"软件正在退出，已停止更新 Codex 配置"; return false; }
  const auto home = GetCodexHomeDir();
  std::error_code ec;
  fs::create_directories(home, ec);
  if (ec) { error = L"创建 Codex 配置目录失败"; return false; }
  std::wstring original;
  const bool configExists = fs::exists(home / L"config.toml", ec);
  if (ec) { error = L"读取 Codex 配置文件状态失败"; return false; }
  if (configExists && !ReadUtf8File(home / L"config.toml", original))
  { error = L"读取 Codex config.toml 失败"; return false; }
  std::wstring existingAuth;
  const bool authExists = fs::exists(home / L"auth.json", ec);
  if (ec || (authExists && !ReadUtf8File(home / L"auth.json", existingAuth)))
  { error = L"读取 Codex auth.json 失败"; return false; }
  const auto auth = PatchProfileApiKey(existingAuth, L"\"" + EscapeJsonString(account.apiKey) + L"\"");
  const auto config = BuildApiCodexToml(original, account);
  TrackProfileApiKey(g_ApiProfileChanges, existingAuth, auth);
  if (!WriteUtf8File(home / L"auth.json", auth)) { error = L"写入 API auth.json 失败"; return false; }
  g_LastWrittenCodexAuth = auth;
  g_LastWrittenCodexToml = original;
  TrackProfileToml(g_ApiProfileChanges, original, config, L"cas_api", true, false);
  if (!WriteUtf8File(home / L"config.toml", config))
  { error = L"写入 API config.toml 失败"; return false; }
  g_LastWrittenCodexToml = config;
  return true;
}

std::wstring FormatApiHttpError(DWORD code)
{
  switch (code)
  {
  case ERROR_WINHTTP_TIMEOUT: return L"请求超时，未在配置的超时时间内完成";
  case ERROR_WINHTTP_NAME_NOT_RESOLVED: return L"DNS 解析失败，无法解析服务器或代理服务器名称";
  case ERROR_WINHTTP_CANNOT_CONNECT: return L"无法连接服务器或代理服务器";
  case ERROR_WINHTTP_CONNECTION_ERROR: return L"连接已中断，可能被关闭、重置或收到无效的 SSL 协议数据";
  case ERROR_CONNECTION_ABORTED: return L"连接已被本机软件中止";
  case ERROR_NETNAME_DELETED: return L"网络连接已被断开";
  case ERROR_WINHTTP_SECURE_FAILURE: return L"TLS 安全连接失败，服务器证书验证未通过";
  case ERROR_WINHTTP_SECURE_CHANNEL_ERROR: return L"TLS 安全通道建立失败";
  case ERROR_WINHTTP_SECURE_CERT_DATE_INVALID: return L"服务器证书已过期或尚未生效";
  case ERROR_WINHTTP_SECURE_CERT_CN_INVALID: return L"服务器证书名称与访问的主机名不匹配";
  case ERROR_WINHTTP_SECURE_INVALID_CA: return L"服务器证书的颁发机构不受信任";
  case ERROR_WINHTTP_SECURE_CERT_REVOKED: return L"服务器证书已被吊销";
  case ERROR_WINHTTP_SECURE_CERT_REV_FAILED: return L"无法完成服务器证书吊销状态检查";
  case ERROR_WINHTTP_CLIENT_AUTH_CERT_NEEDED: return L"服务器要求提供客户端证书";
  case ERROR_WINHTTP_INVALID_SERVER_RESPONSE: return L"服务器返回了无效的 HTTP 响应";
  case ERROR_WINHTTP_OPERATION_CANCELLED: return L"HTTP 请求已取消";
  case ERROR_WINHTTP_INVALID_URL: return L"URL 格式无效";
  case ERROR_WINHTTP_UNRECOGNIZED_SCHEME: return L"URL 使用了不支持的协议";
  }
  LPWSTR buffer = nullptr;
  const auto module = GetModuleHandleW(L"winhttp.dll");
  DWORD length = 0;
  if (module)
    length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS,
                            module, code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
  if (!length)
    length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                            nullptr, code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
  const auto message = length ? TrimWide(std::wstring(buffer, length)) : L"系统未提供此错误的具体说明";
  if (buffer) LocalFree(buffer);
  return message;
}

bool ExecuteApiHttp(const std::wstring &url, const std::wstring &method,
                    const std::wstring &headersJson, const std::wstring &body,
                    int timeoutSec, DWORD &status, std::wstring &response,
                    std::wstring &error, bool readResponseBody = true)
{
  status = 0;
  response.clear();
  error.clear();
  URL_COMPONENTS parts{};
  parts.dwStructSize = sizeof(parts);
  parts.dwSchemeLength = parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength = static_cast<DWORD>(-1);
  if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts))
  { const auto code = GetLastError(); error = L"URL 解析失败：" + FormatApiHttpError(code); return false; }
  if (parts.nScheme != INTERNET_SCHEME_HTTP && parts.nScheme != INTERNET_SCHEME_HTTPS) { error = L"仅支持 HTTP/HTTPS URL"; return false; }
  if (timeoutSec <= 0) { error = L"HTTP 超时必须大于 0"; return false; }
  const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
  std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
  if (path.empty()) path = L"/";
  if (parts.dwExtraInfoLength) path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
  HINTERNET session = WinHttpOpen(L"Codex-AccountSwitch", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                 WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session)
  { const auto code = GetLastError(); error = L"创建 HTTP 会话失败：" + FormatApiHttpError(code); return false; }
  HINTERNET connection = nullptr, request = nullptr;
  auto verb = method;
  std::transform(verb.begin(), verb.end(), verb.begin(), [](wchar_t c) { return static_cast<wchar_t>(towupper(c)); });
  bool ok = false;
  DWORD failure = ERROR_SUCCESS;
  do
  {
    if (!WinHttpSetTimeouts(session, timeoutSec * 1000, timeoutSec * 1000, timeoutSec * 1000, timeoutSec * 1000))
    { failure = GetLastError(); break; }
    connection = WinHttpConnect(session, host.c_str(), parts.nPort, 0);
    if (!connection) { failure = GetLastError(); break; }
    request = WinHttpOpenRequest(connection, verb.c_str(), path.c_str(), nullptr,
                                WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    if (!request) { failure = GetLastError(); break; }
    std::wstring headers;
    for (const auto &field : ApiJsonMembers(headersJson))
    {
      const auto value = ApiJsonString({field}, field.first);
      headers += field.first + L": " + value + L"\r\n";
    }
    const auto payload = WideToUtf8(body);
    if (!WinHttpSendRequest(request, headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
                            headers.empty() ? 0 : static_cast<DWORD>(-1),
                            payload.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char *>(payload.data()),
                            static_cast<DWORD>(payload.size()), static_cast<DWORD>(payload.size()), 0))
    { failure = GetLastError(); break; }
    if (!WinHttpReceiveResponse(request, nullptr)) { failure = GetLastError(); break; }
    DWORD statusSize = sizeof(status);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX))
    { failure = GetLastError(); break; }
    if (!readResponseBody) { ok = true; break; }
    std::string bytes;
    char buffer[8192];
    DWORD received = 0;
    bool readOk = true;
    for (;;)
    {
      if (!WinHttpReadData(request, buffer, sizeof(buffer), &received))
      { failure = GetLastError(); readOk = false; break; }
      if (!received) break;
      bytes.append(buffer, received);
    }
    response = Utf8ToWide(bytes);
    ok = readOk;
  } while (false);
  if (!ok) error = L"HTTP 请求失败：" + FormatApiHttpError(failure);
  if (request) WinHttpCloseHandle(request);
  if (connection) WinHttpCloseHandle(connection);
  WinHttpCloseHandle(session);
  return ok;
}
