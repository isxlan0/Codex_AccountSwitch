// Included inside WebViewHost::HandleWebAction; keep all account actions on the existing bridge.
if (action == L"get_api_accounts")
{
  SendWebJson(BuildApiManagementJson());
  return;
}
if (action == L"get_api_editor_config")
{
  SendWebJson(BuildApiEditorConfigJson(rawMessage));
  return;
}
if (action == L"api_script_result")
{
  ApiScriptReply(rawMessage);
  return;
}
if (action == L"fetch_api_models")
{
  const auto fields = ApiJsonMembers(rawMessage);
  FetchApiModels(hwnd, ApiJsonString(fields, L"requestId"),
                 ApiJsonString(fields, L"baseUrl"), ApiJsonString(fields, L"apiKey"));
  return;
}
if (action == L"preview_api_balance")
{
  const auto fields = ApiJsonMembers(rawMessage);
  const auto id = ApiJsonString(fields, L"id");
  const auto account = fields.find(L"account");
  if (account == fields.end())
    PostApiOperationResult(hwnd, L"api_balance_result", id, false, L"null", L"缺少用量脚本草稿账号");
  else
    QueryApiBalance(hwnd, id, ApiAccountFromJson(account->second), true);
  return;
}
if (action == L"check_api" || action == L"query_api_balance")
{
  const auto id = ApiJsonString(ApiJsonMembers(rawMessage), L"id");
  if (action == L"check_api") RunApiCheck(hwnd, id);
  else QueryApiBalance(hwnd, id);
  return;
}
if (action == L"save_api_account" || action == L"save_api_balance" || action == L"delete_api_account" ||
    action == L"import_current_api" || action == L"import_ccs_api" ||
    action == L"set_api_pool" || action == L"set_official_pool")
{
  std::lock_guard<std::recursive_mutex> profileLock(g_CodexProfileMutex);
  const auto fields = ApiJsonMembers(rawMessage);
  const auto id = ApiJsonString(fields, L"id");
  std::wstring message;
  bool ok = false;
  if (action == L"save_api_account")
  {
    ok = SaveApiAccountFromJson(rawMessage, message);
    if (ok && !id.empty())
    {
      AppConfig cfg;
      LoadConfig(cfg);
      if (cfg.activeApiAccount == id)
      {
        ApiAccount saved;
        FindApiAccount(id, saved);
        cfg.proxyDefaultModel = saved.model;
        ok = cfg.proxyStealthMode
            ? SyncSelectedCodexProfile(cfg, false, message)
            : ApplyApiAccountToCodexProfile(saved, message);
        if (!ok) message = L"账号已保存，但应用到当前 Codex 配置失败：" + message;
        else if (!SaveConfig(cfg))
        {
          ok = false;
          message = L"账号和本机配置已更新，但软件设置保存失败";
        }
        else if (!cfg.proxyStealthMode)
        {
          // Saving the current direct-API account is a persistent edit, not a
          // temporary switch. Otherwise Cleanup restores the old credentials
          // and startup moves the current marker away from this saved account.
          g_ApiProfileChanges = {};
        }
        SendConfig(false);
      }
    }
  }
  else if (action == L"save_api_balance")
  {
    std::lock_guard<std::mutex> accountsLock(g_apiAccountsMutex);
    auto accounts = LoadApiAccountsUnlocked();
    auto account = std::find_if(accounts.begin(), accounts.end(), [&](const ApiAccount &a) { return a.id == id; });
    const auto balance = fields.find(L"balanceConfig"), script = fields.find(L"usageScript");
    if (account == accounts.end()) message = L"API 账号不存在";
    else if (balance == fields.end() || script == fields.end()) message = L"缺少余额配置";
    else
    {
      account->balanceConfig = balance->second;
      account->usageScript = script->second;
      ok = SaveApiAccountsUnlocked(accounts);
      if (!ok) message = L"保存余额配置失败";
    }
  }
  else if (action == L"delete_api_account")
  {
    AppConfig cfg;
    LoadConfig(cfg);
    if (cfg.activeApiAccount == id || cfg.proxyFixedApiAccount == id) message = L"请先切换到其他账号，再删除当前或固定上游 API";
    else ok = DeleteApiAccount(id, message);
  }
  else if (action == L"import_current_api") ok = ImportCurrentApiAccount(message);
  else if (action == L"import_ccs_api")
  {
    auto path = ApiJsonString(fields, L"path");
    if (path.empty())
      path = (fs::path(GetUserAuthPath()).parent_path().parent_path() / L".cc-switch" / L"cc-switch.db").wstring();
    // SQLite import is local and read-only; it does not query upstream accounts.
    ok = ImportCcSwitchApiAccounts(path, message);
  }
  else if (action == L"set_api_pool")
  {
    std::lock_guard<std::mutex> lock(g_apiAccountsMutex);
    auto accounts = LoadApiAccountsUnlocked();
    for (auto &a : accounts)
      if (a.id == id)
      {
        a.inPool = ExtractJsonBoolField(rawMessage, L"enabled", false);
        ok = SaveApiAccountsUnlocked(accounts);
        break;
      }
    if (!ok) message = L"API 账号不存在或无法保存池成员设置";
  }
  else
  {
    std::lock_guard<std::recursive_mutex> lock(g_IndexDataMutex);
    IndexData index;
    LoadIndex(index);
    const auto account = ApiJsonString(fields, L"account");
    const auto group = ApiJsonString(fields, L"group");
    for (auto &a : index.accounts)
      if (BuildIndexEntryKey(a.name, a.group) == BuildIndexEntryKey(account, group))
      {
        a.inPool = ExtractJsonBoolField(rawMessage, L"enabled", false);
        ok = SaveIndex(index);
        break;
      }
    if (!ok) message = L"官方账号不存在或无法保存池成员设置";
  }
  SendWebStatus(message.empty() ? (ok ? L"API / 账号池配置已保存" : L"操作失败") : message,
                ok ? L"success" : L"error", ok ? L"api_config_saved" : L"api_operation_failed");
  SendWebJson(BuildApiManagementJson());
  if (action == L"save_api_account" || action == L"save_api_balance")
    SendWebJson(L"{\"type\":\"api_account_save_result\",\"requestId\":\"" +
                EscapeJsonString(ApiJsonString(fields, L"requestId")) + L"\",\"ok\":" +
                (ok ? L"true" : L"false") + L",\"error\":\"" + EscapeJsonString(message) + L"\"}");
  return;
}
if (action == L"switch_api_account")
{
  std::lock_guard<std::recursive_mutex> profileLock(g_CodexProfileMutex);
  const auto id = ApiJsonString(ApiJsonMembers(rawMessage), L"id");
  ApiAccount account;
  std::wstring error;
  if (!FindApiAccount(id, account))
  {
    SendWebStatus(L"API 账号不存在", L"error", L"api_not_found");
    return;
  }
  AppConfig cfg;
  LoadConfig(cfg);
  cfg.activeApiAccount = id;
  cfg.proxyFixedApiAccount = id;
  cfg.proxyDispatchMode = L"fixed";
  cfg.lastSwitchedAccount = account.name;
  cfg.lastSwitchedAt = NowText();
  cfg.proxyDefaultModel = account.model;
  if (cfg.proxyStealthMode)
  {
    if (cfg.proxyApiKey.empty()) cfg.proxyApiKey = GenerateProxyApiKey();
    if (!SyncSelectedCodexProfile(cfg, false, error))
    {
      SendWebStatus(error, L"error", L"api_profile_failed");
      return;
    }
    std::wstring status, code;
    if (!g_ProxyRunning.load() && !StartLocalProxyService(hwnd, cfg.proxyPort, cfg.proxyTimeoutSec,
                                                        cfg.proxyAllowLan, status, code))
    {
      SendWebStatus(status, L"error", code);
      return;
    }
  }
  else if (!ApplyApiAccountToCodexProfile(account, error))
  {
    SendWebStatus(error, L"error", L"api_profile_failed");
    return;
  }
  if (!SaveConfig(cfg))
  {
    SendWebStatus(L"无法保存 API 切换配置", L"error", L"api_config_failed");
    return;
  }
  g_ProxyDispatchMode = cfg.proxyDispatchMode;
  g_ProxyApiKey = cfg.proxyApiKey;
  {
    std::lock_guard<std::recursive_mutex> lock(g_IndexDataMutex);
    IndexData index;
    LoadIndex(index);
    index.currentName.clear();
    index.currentGroup.clear();
    SaveIndex(index);
  }
  SendConfig(false);
  SendWebJson(BuildApiManagementJson());
  SendAccountsList(false);
  if (cfg.proxyStealthMode)
    SendWebStatus(L"已切换到第三方 API 反代上游：" + account.name, L"success", L"api_switched");
  else
  {
    std::wstring client;
    const bool restarted = RestartConfiguredIde(client);
    SendWebStatus(restarted ? (client.empty() ? L"已切换 API，未重启客户端" : L"已切换 API 并重启 " + client) : L"API 已切换，但重启 " + client + L" 失败，请手动重启",
                  restarted ? L"success" : L"warning", restarted ? L"api_switched" : L"restart_failed");
  }
  return;
}
