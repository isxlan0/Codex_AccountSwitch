#pragma once
// Include after api_accounts.h, before SwitchToAccount.

bool SyncSelectedCodexProfile(const AppConfig &cfg, bool leavingProxy,
                               std::wstring &error)
{
  (void)leavingProxy;
  std::lock_guard<std::recursive_mutex> profileLock(g_CodexProfileMutex);
  error.clear();
  if (g_CodexProfileClosing) { error = L"软件正在退出，已停止更新 Codex 配置"; return false; }
  if (cfg.proxyStealthMode)
  {
    return ApplyStealthProxyModeToCodexProfile(cfg, error);
  }

  if (!cfg.activeApiAccount.empty())
  {
    ApiAccount account;
    if (!FindApiAccount(cfg.activeApiAccount, account)) { error = L"当前选中的 API 账号不存在"; return false; }
    return ApplyApiAccountToCodexProfile(account, error);
  }
  if (leavingProxy)
  {
    IndexData index;
    fs::path source;
    {
      std::lock_guard<std::recursive_mutex> indexLock(g_IndexDataMutex);
      LoadIndex(index);
      const auto selectedName = cfg.proxyDispatchMode == L"fixed" && !cfg.proxyFixedAccount.empty()
          ? cfg.proxyFixedAccount : index.currentName;
      const auto selectedGroup = cfg.proxyDispatchMode == L"fixed" && !cfg.proxyFixedAccount.empty()
          ? cfg.proxyFixedGroup : index.currentGroup;
      for (const auto &account : index.accounts)
      {
        if (EqualsIgnoreCase(account.name, selectedName) &&
            NormalizeGroup(account.group) == NormalizeGroup(selectedGroup))
        {
          source = ResolveAuthPathFromIndex(account);
          break;
        }
      }
    }
    // No selected official account: keep the current Codex profile unchanged.
    if (source.empty()) return true;
    std::wstring auth, normalized;
    if (!ReadUtf8File(source, auth)) { error = L"读取当前官方账号认证备份失败"; return false; }
    NormalizeAuthJsonForCompatibility(auth, normalized);
    if (!WriteUtf8File(GetUserAuthPath(), normalized)) { error = L"恢复当前官方账号认证失败"; return false; }
  }
  return true;
}
