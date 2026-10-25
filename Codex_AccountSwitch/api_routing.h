#pragma once
// Shared dispatch for explicitly selected official accounts and Responses API accounts.
std::wstring BuildApiManagementJson();

struct ProxyAccountTarget
{
  bool api = false;
  ApiAccount provider;
  IndexEntry official;
  std::wstring Key() const
  {
    return api ? L"api::" + provider.id : L"official::" + BuildIndexEntryKey(official.name, official.group);
  }
  std::wstring Name() const { return api ? provider.name : official.name; }
};

void AdoptProxyFixedFailover(const AppConfig &dispatched, const ProxyAccountTarget &target)
{
  if (dispatched.proxyDispatchMode != L"fixed") return;
  AppConfig latest;
  LoadConfig(latest);
  // Do not overwrite a selection the user changed while this request was in flight.
  if (latest.proxyDispatchMode != L"fixed" || latest.proxyFixedApiAccount != dispatched.proxyFixedApiAccount ||
      latest.proxyFixedAccount != dispatched.proxyFixedAccount || latest.proxyFixedGroup != dispatched.proxyFixedGroup) return;
  latest.proxyFixedApiAccount = target.api ? target.provider.id : L"";
  if (!target.api)
  {
    latest.proxyFixedAccount = target.official.name;
    latest.proxyFixedGroup = target.official.group;
  }
  if (latest.proxyStealthMode)
  {
    latest.activeApiAccount = target.api ? target.provider.id : L"";
    std::lock_guard<std::recursive_mutex> lock(g_IndexDataMutex);
    IndexData index;
    LoadIndex(index);
    index.currentName = target.api ? L"" : target.official.name;
    index.currentGroup = target.api ? L"" : target.official.group;
    SaveIndex(index);
  }
  if (SaveConfig(latest))
  {
    g_ProxyFixedAccount = latest.proxyFixedAccount;
    g_ProxyFixedGroup = latest.proxyFixedGroup;
    if (g_DebugWebHwnd != nullptr && IsWindow(g_DebugWebHwnd))
      PostAsyncWebJson(g_DebugWebHwnd, BuildApiManagementJson());
  }
}

void RecordProxyTargetResult(const ProxyAccountTarget &target, const std::wstring &error)
{
  bool changed = false;
  if (target.api)
  {
    std::lock_guard<std::mutex> lock(g_apiAccountsMutex);
    auto accounts = LoadApiAccountsUnlocked();
    for (auto &a : accounts)
      if (a.id == target.provider.id && a.lastError != error)
      {
        a.lastError = error;
        SaveApiAccountsUnlocked(accounts);
        changed = true;
        break;
      }
  }
  else
  {
    std::lock_guard<std::recursive_mutex> lock(g_IndexDataMutex);
    IndexData index;
    LoadIndex(index);
    for (auto &a : index.accounts)
      if (BuildIndexEntryKey(a.name, a.group) == BuildIndexEntryKey(target.official.name, target.official.group)
          && a.lastProxyError != error)
      {
        a.lastProxyError = error;
        SaveIndex(index);
        changed = true;
        break;
      }
  }
  if (changed && g_DebugWebHwnd != nullptr && IsWindow(g_DebugWebHwnd))
    PostAsyncWebJson(g_DebugWebHwnd, BuildApiManagementJson());
}

bool ForwardProxyToUpstream(const std::string &method, const std::string &path,
                            const std::map<std::string, std::string> &headers,
                            const std::string &body, const int timeoutSec,
                            DWORD &statusCode, std::wstring &contentType,
                            std::string &responseBody, std::wstring &error,
                            TrafficLogEntry *trafficMeta = nullptr,
                            SOCKET clientSock = INVALID_SOCKET,
                            bool *alreadyStreamed = nullptr)
{
  AppConfig cfg;
  LoadConfig(cfg);
  IndexData index;
  {
    std::lock_guard<std::recursive_mutex> lock(g_IndexDataMutex);
    LoadIndex(index);
  }
  const auto apiAccounts = LoadApiAccounts();
  const auto route = path.substr(0, path.find('?'));
  const bool apiCompatible = route == "/v1/models" || route == "/v1/responses" ||
      route == "/v1/responses/compact" || route == "/backend-api/codex/responses" ||
      route == "/backend-api/codex/responses/compact";
  std::vector<ProxyAccountTarget> pool, ordered;
  for (const auto &a : index.accounts)
    if (a.inPool && HasUsableProxyQuota(a) && fs::exists(ResolveAuthPathFromIndex(a)))
      pool.push_back({false, {}, a});
  for (const auto &a : apiAccounts)
    if (a.inPool && apiCompatible) pool.push_back({true, a, {}});

  if (cfg.proxyDispatchMode == L"fixed")
  {
    if (!cfg.proxyFixedApiAccount.empty())
    {
      for (const auto &a : apiAccounts)
        if (a.id == cfg.proxyFixedApiAccount) ordered.push_back({true, a, {}});
    }
    else
    {
      const auto fixedName = cfg.proxyFixedAccount.empty() ? index.currentName : cfg.proxyFixedAccount;
      const auto fixedGroup = cfg.proxyFixedAccount.empty() ? index.currentGroup : cfg.proxyFixedGroup;
      for (const auto &a : index.accounts)
        if (EqualsIgnoreCase(a.name, fixedName) && NormalizeGroup(a.group) == NormalizeGroup(fixedGroup))
          ordered.push_back({false, {}, a});
    }
    if (ordered.empty())
    {
      error = L"no_selected_account: 请先手动指定固定上游";
      return false;
    }
  }
  if (!pool.empty())
  {
    size_t start = 0;
    {
      std::lock_guard<std::mutex> lock(g_ProxyFailoverMutex);
      if (cfg.proxyDispatchMode == L"random")
      {
        for (wchar_t c : GenerateRandomBase64Url(4)) start = start * 131 + c;
        start %= pool.size();
      }
      else if (cfg.proxyDispatchMode != L"fixed") start = g_ProxyRoundRobinCursor++ % pool.size();
    }
    for (size_t i = 0; i < pool.size(); ++i)
      ordered.push_back(pool[(start + i) % pool.size()]);
  }
  if (ordered.empty())
  {
    error = L"account_pool_empty: 请在软件中手动选择加入轮换池的账号";
    return false;
  }
  std::set<std::wstring> tried;
  bool transported = false;
  bool streamed = false;
  const auto started = std::chrono::steady_clock::now();
  for (const auto &target : ordered)
  {
    if (!tried.insert(target.Key()).second) continue;
    statusCode = 0;
    contentType.clear();
    responseBody.clear();
    error.clear();
    streamed = false;
    if (target.api)
    {
      transported = ForwardApiResponses(target.provider, method, path, headers, body, timeoutSec,
                                        clientSock, &streamed, statusCode, contentType, responseBody, error);
      if (trafficMeta != nullptr)
      {
        trafficMeta->calledAt = static_cast<long long>(time(nullptr));
        trafficMeta->account = target.Name();
        trafficMeta->method = FromUtf8(method);
        trafficMeta->path = FromUtf8(path);
        trafficMeta->model = ExtractJsonField(FromUtf8(body), L"model");
        if (trafficMeta->model.empty()) trafficMeta->model = target.provider.model;
        trafficMeta->protocol = L"openai";
        trafficMeta->statusCode = static_cast<int>(statusCode);
        trafficMeta->elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count();
        if (!streamed) ParseTokenUsageSmart(FromUtf8(responseBody), trafficMeta->inputTokens,
                                            trafficMeta->outputTokens, trafficMeta->totalTokens);
      }
    }
    else
    {
      transported = ForwardOfficialProxyToUpstream(ResolveAuthPathFromIndex(target.official),
          target.official.name, target.official.group, method, path, headers, body, timeoutSec,
          statusCode, contentType, responseBody, error, trafficMeta, clientSock, &streamed);
    }
    if (alreadyStreamed != nullptr) *alreadyStreamed = streamed;
    const bool retryable = !transported || statusCode == 401 || statusCode == 403 ||
                           statusCode == 429 || statusCode >= 500;
    const bool succeeded = transported && statusCode >= 200 && statusCode < 300;
    std::wstring failure;
    if (!succeeded)
      failure = !error.empty() ? error : L"HTTP " + std::to_wstring(statusCode) + L": " + FromUtf8(responseBody);
    RecordProxyTargetResult(target, failure);
    if (succeeded)
    {
      if (tried.size() > 1) AdoptProxyFixedFailover(cfg, target);
      return true;
    }
    if (streamed)
    {
      if (g_DebugWebHwnd != nullptr && IsWindow(g_DebugWebHwnd))
        SendWebStatusThreadSafe(g_DebugWebHwnd, target.Name() + L" 串流中断，已标记；已输出的请求不会重放：" + failure,
                                L"warning", L"proxy_stream_interrupted");
      return transported;
    }
    if (!retryable) return transported;
    if (!target.api && IsUsageLimitReachedPayload(FromUtf8(responseBody)))
    {
      std::wstring markError;
      MarkProxyAccountQuotaExhausted(target.official.name, target.official.group, FromUtf8(responseBody), markError);
    }
    else if (!target.api && cfg.autoMarkAbnormalAccounts)
    {
      std::wstring reason, markError;
      if (IsProxyAccountAbnormalPayload(FromUtf8(responseBody), statusCode, reason))
        MarkProxyAccountAbnormal(target.official.name, target.official.group, reason, FromUtf8(responseBody), markError);
    }
    const bool hasNext = std::any_of(ordered.begin(), ordered.end(), [&](const ProxyAccountTarget &a) {
      return tried.find(a.Key()) == tried.end();
    });
    if (!hasNext)
    {
      if (g_DebugWebHwnd != nullptr && IsWindow(g_DebugWebHwnd))
        SendWebStatusThreadSafe(g_DebugWebHwnd, L"所有已选上游均不可用，失败账号已标记：" + target.Name() + L"：" + failure,
                                L"error", L"proxy_pool_exhausted");
      break;
    }
    if (g_DebugWebHwnd != nullptr && IsWindow(g_DebugWebHwnd))
      SendWebStatusThreadSafe(g_DebugWebHwnd, target.Name() + L" 请求失败，已标记并尝试下一个已入池上游：" + failure,
                              L"warning", L"proxy_upstream_failover");
    if (trafficMeta != nullptr) AppendTrafficLog(*trafficMeta);
  }
  if (error.empty()) error = L"所有已选上游均不可用";
  return transported;
}
