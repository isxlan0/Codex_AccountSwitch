#pragma once

#include <string>

struct UpdateCheckResult
{
    bool ok = false;
    bool hasUpdate = false;
    std::wstring currentVersion;
    std::wstring latestVersion;
    std::wstring releaseUrl;
    std::wstring downloadUrl;
    std::wstring releaseNotes;
    std::wstring errorMessage;
};

UpdateCheckResult CheckGitHubUpdate(const std::wstring& currentVersion);
