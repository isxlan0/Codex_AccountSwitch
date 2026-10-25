// Included inside WebViewHost::HandleWebAction, alongside api_actions.h.
if (action == L"get_codex_cli_info" || action == L"install_codex_cli")
{
  static std::atomic<bool> cliOperationRunning{false};
  static std::atomic<bool> cliInstalling{false};
  const bool install = action == L"install_codex_cli";
  const auto buildCliInfo = [](const std::wstring &version,
                               const std::wstring &error, bool installing)
  {
    return L"{\"type\":\"codex_cli_info\",\"version\":\"" +
           EscapeJsonString(version) + L"\",\"error\":\"" +
           EscapeJsonString(error) + L"\",\"installing\":" +
           (installing ? L"true}" : L"false}");
  };
  if (cliOperationRunning.exchange(true))
  {
    if (cliInstalling.load())
      SendWebJson(buildCliInfo(L"", L"", true));
    else if (install)
      SendWebJson(buildCliInfo(L"", L"Codex CLI version check is in progress. Please try again after it completes.", false));
    return;
  }
  cliInstalling.store(install);
  if (install)
    SendWebJson(buildCliInfo(L"", L"", true));

  const HWND targetHwnd = hwnd_;
  std::thread([targetHwnd, install, buildCliInfo]()
  {
    std::wstring error;
    if (install)
    {
      if (SearchPathW(nullptr, L"npm.cmd", nullptr, 0, nullptr, nullptr) == 0)
      {
        error = L"npm.cmd was not found. Install Node.js and npm, then restart this application before installing Codex CLI.";
      }
      else
      {
        std::wstring settings;
        if (!ReadUtf8File(GetWorkspaceRoot() / L"codex-cli-settings.json", settings))
          error = L"Unable to read codex-cli-settings.json.";
        else
        {
          const auto command = ApiJsonString(ApiJsonMembers(settings), L"installCommand");
          if (command.empty())
            error = L"installCommand is missing from codex-cli-settings.json.";
          else if (!RunPowerShellCommand(command + L"; exit $LASTEXITCODE"))
            error = L"Codex CLI installation failed. Check the npm registry connection and global installation permissions.";
        }
      }
    }

    const std::wstring version = ReadCodexVersionFromCli();
    if (install && error.empty() && version.empty())
      error = L"The installation command completed, but Codex CLI could not be detected. Check the npm global executable directory in PATH and restart this application.";
    cliInstalling.store(false);
    cliOperationRunning.store(false);
    PostAsyncWebJson(targetHwnd, buildCliInfo(version, error, false));
  }).detach();
  return;
}
