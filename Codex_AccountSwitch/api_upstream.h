#pragma once
// Include in webview_host.cpp's anonymous namespace after streaming helpers.

bool ForwardApiResponses(const ApiAccount &account, const std::string &method,
                         const std::string &path,
                         const std::map<std::string, std::string> &clientHeaders,
                         const std::string &body, const int timeoutSec,
                         SOCKET clientSock, bool *alreadyStreamed,
                         DWORD &status, std::wstring &contentType,
                         std::string &response, std::wstring &error)
{
  status = 0;
  contentType.clear();
  response.clear();
  error.clear();
  if (alreadyStreamed) *alreadyStreamed = false;

  const auto queryAt = path.find('?');
  const std::string route = path.substr(0, queryAt);
  const std::string query = queryAt == std::string::npos ? "" : path.substr(queryAt + 1);
  const bool compact = route == "/v1/responses/compact" || route == "/backend-api/codex/responses/compact";
  const bool models = route == "/v1/models";
  if (!models && !compact && route != "/v1/responses" && route != "/backend-api/codex/responses")
  {
    status = 404;
    contentType = L"application/json";
    response = "{\"error\":{\"message\":\"API accounts support Responses endpoints only\",\"type\":\"invalid_request_error\"}}";
    return true;
  }

  const auto fields = ApiJsonMembers(FromUtf8(body));
  std::string payload = body;
  if (fields.find(L"model") == fields.end())
  {
    // Only model insertion changes the original request; all values stay raw JSON.
    const auto first = body.find_first_not_of(" \t\r\n");
    if (first != std::string::npos && body[first] == '{')
    {
      const auto next = body.find_first_not_of(" \t\r\n", first + 1);
      payload.insert(first + 1, "\"model\":\"" + WideToUtf8(EscapeJsonString(account.model)) +
                                   "\"" + (next != std::string::npos && body[next] != '}' ? "," : ""));
    }
  }
  const auto streamField = fields.find(L"stream");
  const bool stream = streamField != fields.end() && streamField->second == L"true";

  const auto base = TrimWide(account.baseUrl);
  URL_COMPONENTS parts{};
  parts.dwStructSize = sizeof(parts);
  parts.dwSchemeLength = parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength = static_cast<DWORD>(-1);
  if (!WinHttpCrackUrl(base.c_str(), 0, 0, &parts))
  { error = L"api_upstream_url_invalid:" + std::to_wstring(GetLastError()); return false; }
  if (parts.nScheme != INTERNET_SCHEME_HTTP && parts.nScheme != INTERNET_SCHEME_HTTPS)
  { error = L"api_upstream_scheme_requires_http_or_https"; return false; }
  if (timeoutSec <= 0) { error = L"api_upstream_timeout_invalid"; return false; }
  const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
  std::wstring upstreamPath(parts.lpszUrlPath, parts.dwUrlPathLength);
  while (!upstreamPath.empty() && upstreamPath.back() == L'/') upstreamPath.pop_back();
  upstreamPath += models ? L"/models" : (compact ? L"/responses/compact" : L"/responses");
  std::wstring baseQuery;
  if (parts.dwExtraInfoLength)
  {
    baseQuery.assign(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    baseQuery = baseQuery.substr(0, baseQuery.find(L'#'));
    if (!baseQuery.empty() && baseQuery.front() == L'?') baseQuery.erase(0, 1);
  }
  if (!baseQuery.empty() || !query.empty())
  {
    upstreamPath += L"?" + baseQuery;
    if (!baseQuery.empty() && !query.empty()) upstreamPath += L"&";
    upstreamPath += FromUtf8(query);
  }

  WinHttpStreamContext ctx;
  ctx.hSession = WinHttpOpen(L"Codex-AccountSwitch", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!ctx.hSession) { error = L"api_upstream_session_failed:" + std::to_wstring(GetLastError()); return false; }
  if (!WinHttpSetTimeouts(ctx.hSession, timeoutSec * 1000, timeoutSec * 1000,
                         timeoutSec * 1000, timeoutSec * 1000))
  { error = L"api_upstream_timeout_failed:" + std::to_wstring(GetLastError()); return false; }
  ctx.hConnect = WinHttpConnect(ctx.hSession, host.c_str(), parts.nPort, 0);
  if (!ctx.hConnect) { error = L"api_upstream_connect_failed:" + std::to_wstring(GetLastError()); return false; }
  auto verb = FromUtf8(method);
  std::transform(verb.begin(), verb.end(), verb.begin(), [](wchar_t c) { return static_cast<wchar_t>(towupper(c)); });
  ctx.hRequest = WinHttpOpenRequest(ctx.hConnect, verb.c_str(), upstreamPath.c_str(), nullptr,
                                   WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                   parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
  if (!ctx.hRequest) { error = L"api_upstream_request_failed:" + std::to_wstring(GetLastError()); return false; }
  std::wstring headers = L"Authorization: Bearer " + account.apiKey + L"\r\nAccept-Encoding: identity\r\n";
  for (const auto &field : clientHeaders)
  {
    const auto name = ToLowerCopy(FromUtf8(field.first));
    if (name == L"content-type" || name == L"accept" || name == L"openai-beta")
      headers += FromUtf8(field.first) + L": " + FromUtf8(field.second) + L"\r\n";
  }
  if (!WinHttpSendRequest(ctx.hRequest, headers.c_str(), static_cast<DWORD>(-1),
                          payload.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char *>(payload.data()),
                          static_cast<DWORD>(payload.size()), static_cast<DWORD>(payload.size()), 0) ||
      !WinHttpReceiveResponse(ctx.hRequest, nullptr))
  { error = L"api_upstream_transport_failed:" + std::to_wstring(GetLastError()); return false; }
  DWORD statusSize = sizeof(status);
  if (!WinHttpQueryHeaders(ctx.hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                           WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX))
  { error = L"api_upstream_status_failed:" + std::to_wstring(GetLastError()); return false; }
  DWORD contentTypeSize = 0;
  WinHttpQueryHeaders(ctx.hRequest, WINHTTP_QUERY_CONTENT_TYPE, WINHTTP_HEADER_NAME_BY_INDEX,
                       nullptr, &contentTypeSize, WINHTTP_NO_HEADER_INDEX);
  if (contentTypeSize)
  {
    std::vector<wchar_t> buffer(contentTypeSize / sizeof(wchar_t) + 1, L'\0');
    if (WinHttpQueryHeaders(ctx.hRequest, WINHTTP_QUERY_CONTENT_TYPE, WINHTTP_HEADER_NAME_BY_INDEX,
                            buffer.data(), &contentTypeSize, WINHTTP_NO_HEADER_INDEX))
      contentType.assign(buffer.data());
  }

  // Do not emit client headers until status is accepted; failover owns all errors.
  if (status >= 200 && status < 300 && stream && clientSock != INVALID_SOCKET)
  {
    int input = -1, output = -1, total = -1;
    const bool ok = StreamProxyUpstreamToClient(ctx, clientSock, contentType, input, output, total, alreadyStreamed);
    if (!ok) error = L"api_upstream_client_stream_failed";
    return ok;
  }
  for (;;)
  {
    char buffer[8192];
    DWORD received = 0;
    if (!WinHttpReadData(ctx.hRequest, buffer, sizeof(buffer), &received))
    { error = L"api_upstream_read_failed:" + std::to_wstring(GetLastError()); return false; }
    if (!received) break;
    response.append(buffer, received);
  }
  if (status >= 400) error = L"http_status_" + std::to_wstring(status);
  return true;
}
