#include "update_checker.h"

#include <windows.h>
#include <winhttp.h>

#include <cwctype>
#include <regex>
#include <string>

#pragma comment(lib, "Winhttp.lib")

namespace
{
constexpr wchar_t kHost[] = L"api.github.com";
constexpr wchar_t kReleasePath[] = L"/repos/isxlan0/Codex_AccountSwitch/releases/latest";
constexpr wchar_t kTagsPath[] = L"/repos/isxlan0/Codex_AccountSwitch/tags?per_page=1";
constexpr wchar_t kReleaseUrl[] = L"https://github.com/isxlan0/Codex_AccountSwitch/releases/latest";

bool HttpGet(const std::wstring& path, std::wstring& body)
{
    body.clear();

    HINTERNET hSession = WinHttpOpen(
        L"CodexAccountSwitch/1.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (hSession == nullptr)
    {
        return false;
    }

    HINTERNET hConnect = WinHttpConnect(hSession, kHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (hConnect == nullptr)
    {
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (hRequest == nullptr)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    const wchar_t* headers = L"Accept: application/vnd.github+json\r\n";
    bool ok = WinHttpSendRequest(
        hRequest,
        headers,
        static_cast<DWORD>(-1L),
        WINHTTP_NO_REQUEST_DATA,
        0,
        0,
        0) == TRUE;
    if (ok)
    {
        ok = WinHttpReceiveResponse(hRequest, nullptr) == TRUE;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (ok)
    {
        ok = WinHttpQueryHeaders(
                 hRequest,
                 WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                 WINHTTP_HEADER_NAME_BY_INDEX,
                 &statusCode,
                 &statusSize,
                 WINHTTP_NO_HEADER_INDEX) == TRUE &&
             statusCode >= 200 && statusCode < 300;
    }

    std::string payload;
    while (ok)
    {
        DWORD size = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &size))
        {
            ok = false;
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
            ok = false;
            break;
        }
        chunk.resize(read);
        payload += chunk;
    }

    if (ok && !payload.empty())
    {
        const int wideSize = MultiByteToWideChar(CP_UTF8, 0, payload.data(), static_cast<int>(payload.size()), nullptr, 0);
        if (wideSize > 0)
        {
            body.resize(wideSize);
            MultiByteToWideChar(CP_UTF8, 0, payload.data(), static_cast<int>(payload.size()), body.data(), wideSize);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return ok && !body.empty();
}

std::wstring ExtractByRegex(const std::wstring& text, const std::wregex& re)
{
    std::wsmatch m;
    if (std::regex_search(text, m, re) && m.size() > 1)
    {
        return m[1].str();
    }
    return L"";
}

std::wstring JsonUnescape(const std::wstring& text)
{
    std::wstring out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i)
    {
        const wchar_t ch = text[i];
        if (ch == L'\\' && i + 1 < text.size())
        {
            const wchar_t n = text[++i];
            switch (n)
            {
            case L'\\': out.push_back(L'\\'); break;
            case L'"': out.push_back(L'"'); break;
            case L'n': out.push_back(L'\n'); break;
            case L'r': out.push_back(L'\r'); break;
            case L't': out.push_back(L'\t'); break;
            default: out.push_back(n); break;
            }
        }
        else
        {
            out.push_back(ch);
        }
    }
    return out;
}

std::wstring NormalizeVersion(const std::wstring& version)
{
    std::wstring out;
    out.reserve(version.size());
    for (wchar_t ch : version)
    {
        if (ch == L'v' || ch == L'V')
        {
            if (out.empty())
            {
                continue;
            }
        }
        if (ch == L' ')
        {
            continue;
        }
        out.push_back(static_cast<wchar_t>(towlower(ch)));
    }
    return out;
}
}

UpdateCheckResult CheckGitHubUpdate(const std::wstring& currentVersion)
{
    UpdateCheckResult result;
    result.currentVersion = currentVersion;
    result.releaseUrl = kReleaseUrl;

    std::wstring body;
    std::wstring latest;

    if (HttpGet(kReleasePath, body))
    {
        latest = ExtractByRegex(body, std::wregex(L"\\\"tag_name\\\"\\s*:\\s*\\\"([^\\\"]+)\\\""));
        const std::wstring notesRaw = ExtractByRegex(body, std::wregex(LR"REGEX(\"body\"\s*:\s*\"((?:\\.|[^"])*)\")REGEX"));
        result.releaseNotes = JsonUnescape(notesRaw);

        // Prefer installer/binary asset URL from latest release.
        const std::wregex exeAssetRe(LR"REGEX(\"browser_download_url\"\s*:\s*\"([^\"]+\.(?:exe|msi|zip))\")REGEX", std::regex::icase);
        std::wstring asset = ExtractByRegex(body, exeAssetRe);
        if (!asset.empty())
        {
            result.downloadUrl = asset;
        }
    }

    if (latest.empty())
    {
        body.clear();
        if (HttpGet(kTagsPath, body))
        {
            latest = ExtractByRegex(body, std::wregex(L"\\\"name\\\"\\s*:\\s*\\\"([^\\\"]+)\\\""));
        }
    }

    if (latest.empty())
    {
        result.ok = false;
        result.errorMessage = L"Failed to fetch version info from GitHub";
        return result;
    }

    result.latestVersion = latest;
    result.ok = true;
    result.hasUpdate = NormalizeVersion(result.currentVersion) != NormalizeVersion(result.latestVersion);
    if (result.downloadUrl.empty())
    {
        result.downloadUrl = result.releaseUrl;
    }
    return result;
}
