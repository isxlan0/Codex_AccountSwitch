#pragma once
// Include at the end of webview_host.cpp's anonymous namespace.

std::wstring BuildApiManagementJson()
{
  AppConfig cfg;
  LoadConfig(cfg);
  IndexData index;
  {
    std::lock_guard<std::recursive_mutex> lock(g_IndexDataMutex);
    LoadIndex(index);
  }
  std::wostringstream out;
  out << L"{\"type\":\"api_accounts\",\"items\":" << ApiAccountsItemsJson(LoadApiAccounts())
      << L",\"activeApiAccount\":\"" << EscapeJsonString(cfg.activeApiAccount) << L"\""
      << L",\"proxyFixedApiAccount\":\"" << EscapeJsonString(cfg.proxyFixedApiAccount) << L"\""
      << L",\"proxyDispatchMode\":\"" << EscapeJsonString(cfg.proxyDispatchMode) << L"\""
      << L",\"timeoutSec\":" << cfg.apiTimeoutSec << L",\"officialPool\":[";
  bool first = true;
  for (const auto &a : index.accounts)
  {
    if (!first) out << L",";
    first = false;
    out << L"{\"name\":\"" << EscapeJsonString(a.name)
        << L"\",\"group\":\"" << EscapeJsonString(a.group)
        << L"\",\"inPool\":" << (a.inPool ? L"true" : L"false")
        << L",\"lastError\":\"" << EscapeJsonString(a.lastProxyError) << L"\"}";
  }
  out << L"]}";
  return out.str();
}

struct ApiScriptPending
{
  std::condition_variable ready;
  bool done = false;
  bool ok = false;
  std::wstring result, error;
};

std::mutex g_apiScriptMutex;
std::map<std::wstring, std::shared_ptr<ApiScriptPending>> g_apiScriptPending;

void ApiScriptReply(const std::wstring &raw)
{
  const auto fields = ApiJsonMembers(raw);
  const auto id = ApiJsonString(fields, L"requestId");
  std::lock_guard<std::mutex> lock(g_apiScriptMutex);
  const auto it = g_apiScriptPending.find(id);
  if (it == g_apiScriptPending.end()) return;
  auto &pending = *it->second;
  const auto ok = fields.find(L"ok");
  pending.ok = ok != fields.end() && ok->second == L"true";
  const auto result = fields.find(L"result");
  pending.result = result == fields.end() ? L"null" : result->second;
  pending.error = ApiJsonString(fields, L"error");
  pending.done = true;
  pending.ready.notify_one();
}

bool EvaluateApiScript(const HWND hwnd, const std::wstring &phase,
                       const std::wstring &code, const std::wstring &variables,
                       const std::wstring &response, int timeoutSec,
                       std::wstring &result, std::wstring &error)
{
  const auto id = GenerateRandomBase64Url(18);
  auto pending = std::make_shared<ApiScriptPending>();
  {
    std::lock_guard<std::mutex> lock(g_apiScriptMutex);
    g_apiScriptPending.emplace(id, pending);
  }
  PostAsyncWebJson(hwnd, L"{\"type\":\"api_script_eval\",\"requestId\":\"" + EscapeJsonString(id) +
                          L"\",\"phase\":\"" + phase + L"\",\"code\":\"" + EscapeJsonString(code) +
                          L"\",\"variables\":" + variables + L",\"responseText\":\"" + EscapeJsonString(response) + L"\"" +
                          L",\"timeoutSec\":" + std::to_wstring(timeoutSec) + L"}");
  std::unique_lock<std::mutex> lock(g_apiScriptMutex);
  const bool completed = pending->ready.wait_for(lock, std::chrono::seconds(timeoutSec), [&] { return pending->done; });
  g_apiScriptPending.erase(id);
  if (!completed) { error = L"余额脚本执行超时"; return false; }
  result = pending->result;
  error = pending->error;
  if (!pending->ok && error.empty()) error = L"余额脚本执行失败";
  return pending->ok;
}

void PostApiOperationResult(HWND hwnd, const std::wstring &type, const std::wstring &id,
                            bool ok, const std::wstring &result, const std::wstring &error)
{
  PostAsyncWebJson(hwnd, L"{\"type\":\"" + type + L"\",\"id\":\"" + EscapeJsonString(id) +
                          L"\",\"ok\":" + (ok ? L"true" : L"false") +
                          (ok ? L",\"result\":" + result : L",\"error\":\"" + EscapeJsonString(error) + L"\"") + L"}");
}

void QueryApiBalance(HWND hwnd, const std::wstring &id,
                     const ApiAccount &draft = ApiAccount{}, bool preview = false)
{
  std::thread([hwnd, id, draft, preview]() {
    std::wstring result, error;
    auto run = [&]() -> bool {
      ApiAccount account = draft;
      if (!preview && !FindApiAccount(id, account)) { error = L"API 账号不存在"; return false; }
      AppConfig cfg;
      LoadConfig(cfg);
      const auto script = ApiJsonMembers(account.usageScript);
      const auto balance = ApiJsonMembers(account.balanceConfig);
      const auto enabled = script.find(L"enabled");
      if (!preview && (enabled == script.end() || enabled->second != L"true") && ApiJsonString(balance, L"url").empty())
      { error = L"余额脚本未启用且未配置余额查询 URL"; return false; }
      const auto code = ApiJsonString(script, L"code");
      if (code.empty()) { error = L"不支持此余额脚本：缺少 JavaScript code，CC Switch 服务模板需转换为 request/extractor 脚本"; return false; }
      const int scriptTimeout = ExtractJsonIntField(account.usageScript, L"timeout", 0);
      const int timeout = scriptTimeout > 0 ? scriptTimeout : cfg.apiTimeoutSec;
      auto apiKey = TrimWide(ApiJsonString(script, L"apiKey"));
      auto baseUrl = TrimWide(ApiJsonString(script, L"baseUrl"));
      if (apiKey.empty()) apiKey = account.apiKey;
      const bool useAccountBaseUrl = baseUrl.empty();
      if (useAccountBaseUrl) baseUrl = TrimWide(account.baseUrl);
      while (!baseUrl.empty() && baseUrl.back() == L'/') baseUrl.pop_back();
      // Usage paths belong to the service, not the inference /v1 namespace.
      // An explicit usage-script URL remains authoritative.
      if (useAccountBaseUrl && baseUrl.size() >= 3 &&
          baseUrl.compare(baseUrl.size() - 3, 3, L"/v1") == 0)
        baseUrl.resize(baseUrl.size() - 3);
      const std::wstring variables = L"{\"apiKey\":\"" + EscapeJsonString(apiKey) + L"\",\"baseUrl\":\"" +
          EscapeJsonString(baseUrl) + L"\",\"accessToken\":\"" + EscapeJsonString(ApiJsonString(script, L"accessToken")) +
          L"\",\"userId\":\"" + EscapeJsonString(ApiJsonString(script, L"userId")) + L"\"}";
      std::wstring request;
      if (!EvaluateApiScript(hwnd, L"request", code, variables, L"null", timeout, request, error)) return false;
      const auto fields = ApiJsonMembers(request);
      const auto url = ApiJsonString(fields, L"url");
      const auto method = ApiJsonString(fields, L"method");
      if (url.empty() || method.empty()) { error = L"余额脚本 request 必须包含 url 和 method"; return false; }
      const auto headers = fields.find(L"headers");
      const auto body = fields.find(L"body");
      std::wstring requestBody;
      if (body != fields.end() && body->second != L"null")
        requestBody = !body->second.empty() && body->second.front() == L'"' ? ApiJsonString(fields, L"body") : body->second;
      DWORD status = 0;
      std::wstring response;
      if (!ExecuteApiHttp(url, method, headers == fields.end() ? L"{}" : headers->second,
                          requestBody, timeout, status, response, error)) return false;
      if (status < 200 || status >= 300)
      { error = L"余额查询 HTTP " + std::to_wstring(status) + L"：" + response; return false; }
      if (TrimWide(response).empty()) { error = L"余额查询返回空响应，无法作为 JSON 提取"; return false; }
      return EvaluateApiScript(hwnd, L"extract", code, variables, response, timeout, result, error);
    };
    const bool ok = run();
    PostApiOperationResult(hwnd, L"api_balance_result", id, ok, result, error);
  }).detach();
}

void FetchApiModels(HWND hwnd, const std::wstring &requestId,
                    const std::wstring &baseUrl, const std::wstring &apiKey)
{
  std::thread([hwnd, requestId, baseUrl, apiKey]() {
    std::wstring error;
    std::set<std::wstring> models;
    auto run = [&]() -> bool {
      auto url = TrimWide(baseUrl);
      const auto key = TrimWide(apiKey);
      if (url.empty() || key.empty()) { error = L"Base URL 和 API Key 不能为空"; return false; }
      const auto queryAt = url.find(L'?');
      const auto query = queryAt == std::wstring::npos ? L"" : url.substr(queryAt);
      url = url.substr(0, queryAt);
      while (!url.empty() && url.back() == L'/') url.pop_back();
      const auto segmentAt = url.find_last_of(L'/');
      const auto segment = url.substr(segmentAt == std::wstring::npos ? 0 : segmentAt + 1);
      const bool versioned = segment.size() > 1 && segment.front() == L'v' &&
          std::all_of(segment.begin() + 1, segment.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; });
      url += (versioned ? L"/models" : L"/v1/models") + query;
      AppConfig cfg;
      LoadConfig(cfg);
      DWORD status = 0;
      std::wstring response;
      const auto headers = L"{\"Authorization\":\"Bearer " + EscapeJsonString(key) + L"\"}";
      if (!ExecuteApiHttp(url, L"GET", headers, L"", cfg.apiTimeoutSec, status, response, error)) return false;
      if (status < 200 || status >= 300)
      { error = L"获取模型列表 HTTP " + std::to_wstring(status) + L"：" + response; return false; }
      const auto fields = ApiJsonMembers(response);
      bool hasArray = false;
      for (const auto *arrayName : { L"data", L"models" })
      {
        const auto array = fields.find(arrayName);
        if (array == fields.end() || array->second.empty() || array->second.front() != L'[') continue;
        hasArray = true;
        for (const auto &row : ExtractTopLevelObjectsFromArray(array->second))
        {
          const auto entry = ApiJsonMembers(row);
          auto id = ApiJsonString(entry, L"id");
          if (std::wstring(arrayName) == L"models")
          {
            const auto slug = ApiJsonString(entry, L"slug");
            if (!slug.empty()) id = slug;
            if (id.empty()) id = ApiJsonString(entry, L"name");
          }
          if (id.compare(0, 7, L"models/") == 0) id.erase(0, 7);
          if (!id.empty()) models.insert(id);
        }
      }
      if (!hasArray) { error = L"模型列表响应缺少 data 或 models 数组"; return false; }
      return true;
    };
    const bool ok = run();
    std::wostringstream out;
    out << L"{\"type\":\"api_models_result\",\"requestId\":\"" << EscapeJsonString(requestId)
        << L"\",\"ok\":" << (ok ? L"true" : L"false");
    if (ok)
    {
      out << L",\"models\":[";
      bool first = true;
      for (const auto &model : models)
      {
        if (!first) out << L",";
        first = false;
        out << L"\"" << EscapeJsonString(model) << L"\"";
      }
      out << L"]";
    }
    else out << L",\"error\":\"" << EscapeJsonString(error) << L"\"";
    out << L"}";
    PostAsyncWebJson(hwnd, out.str());
  }).detach();
}

void RunApiCheck(HWND hwnd, const std::wstring &id)
{
  std::thread([hwnd, id]() {
    std::wstring error;
    DWORD status = 0;
    long long latencyMs = 0;
    auto run = [&]() -> bool {
      ApiAccount account;
      if (!FindApiAccount(id, account)) { error = L"API 账号不存在"; return false; }
      AppConfig cfg;
      LoadConfig(cfg);
      std::wstring response;
      const auto started = std::chrono::steady_clock::now();
      const bool reachable = ExecuteApiHttp(TrimWide(account.baseUrl), L"GET", L"{}", L"",
                                            cfg.apiTimeoutSec, status, response, error, false);
      latencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - started).count();
      return reachable;
    };
    const bool ok = run();
    {
      std::lock_guard<std::mutex> lock(g_apiAccountsMutex);
      auto accounts = LoadApiAccountsUnlocked();
      auto it = std::find_if(accounts.begin(), accounts.end(), [&](const ApiAccount &a) { return a.id == id; });
      if (it != accounts.end())
      {
        it->lastError = error;
        if (!SaveApiAccountsUnlocked(accounts)) error = L"保存 API 检查结果失败";
      }
    }
    const auto result = L"{\"httpStatus\":" + std::to_wstring(status) +
                        L",\"latencyMs\":" + std::to_wstring(latencyMs) + L"}";
    PostApiOperationResult(hwnd, L"api_check_result", id, ok && error.empty(), result, error);
    PostAsyncWebJson(hwnd, BuildApiManagementJson());
  }).detach();
}
