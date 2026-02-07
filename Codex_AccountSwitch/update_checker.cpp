#include "update_checker.h"

#include <windows.h>
#include <winhttp.h>

#include <cwctype>
#include <string>
#include <vector>

#pragma comment(lib, "Winhttp.lib")

namespace
{
constexpr wchar_t kHost[] = L"api.github.com";
constexpr wchar_t kReleasePath[] = L"/repos/isxlan0/Codex_AccountSwitch/releases/latest";
constexpr wchar_t kTagsPath[] = L"/repos/isxlan0/Codex_AccountSwitch/tags?per_page=1";
constexpr wchar_t kReleaseUrl[] = L"https://github.com/isxlan0/Codex_AccountSwitch/releases/latest";
std::wstring JsonUnescape(const std::wstring& text);

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

std::wstring ExtractJsonStringField(const std::wstring& text, const std::wstring& key, size_t startPos = 0)
{
    const std::wstring pattern = L"\"" + key + L"\"";
    const size_t keyPos = text.find(pattern, startPos);
    if (keyPos == std::wstring::npos)
    {
        return L"";
    }

    const size_t colonPos = text.find(L':', keyPos + pattern.size());
    if (colonPos == std::wstring::npos)
    {
        return L"";
    }

    size_t p = colonPos + 1;
    while (p < text.size() && iswspace(text[p]))
    {
        ++p;
    }
    if (p >= text.size() || text[p] != L'"')
    {
        return L"";
    }
    ++p;

    std::wstring escaped;
    escaped.reserve(64);
    bool escape = false;
    for (; p < text.size(); ++p)
    {
        const wchar_t ch = text[p];
        if (escape)
        {
            escaped.push_back(L'\\');
            escaped.push_back(ch);
            escape = false;
            continue;
        }
        if (ch == L'\\')
        {
            escape = true;
            continue;
        }
        if (ch == L'"')
        {
            return JsonUnescape(escaped);
        }
        escaped.push_back(ch);
    }

    return L"";
}

bool EndsWithIgnoreCase(const std::wstring& text, const std::wstring& suffix)
{
    if (text.size() < suffix.size())
    {
        return false;
    }

    const size_t offset = text.size() - suffix.size();
    for (size_t i = 0; i < suffix.size(); ++i)
    {
        if (towlower(text[offset + i]) != towlower(suffix[i]))
        {
            return false;
        }
    }
    return true;
}

std::wstring ExtractPreferredDownloadUrl(const std::wstring& text)
{
    const std::wstring key = L"browser_download_url";
    size_t searchPos = 0;
    while (searchPos < text.size())
    {
        const size_t keyPos = text.find(L"\"" + key + L"\"", searchPos);
        if (keyPos == std::wstring::npos)
        {
            return L"";
        }

        const std::wstring url = ExtractJsonStringField(text, key, keyPos);
        if (!url.empty() &&
            (EndsWithIgnoreCase(url, L".exe") || EndsWithIgnoreCase(url, L".msi") || EndsWithIgnoreCase(url, L".zip")))
        {
            return url;
        }

        searchPos = keyPos + key.size() + 2;
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

std::vector<int> ParseVersionNumbers(const std::wstring& version)
{
    const std::wstring normalized = NormalizeVersion(version);
    std::vector<int> numbers;
    int value = 0;
    bool inNumber = false;

    for (wchar_t ch : normalized)
    {
        if (ch >= L'0' && ch <= L'9')
        {
            inNumber = true;
            value = value * 10 + static_cast<int>(ch - L'0');
        }
        else if (inNumber)
        {
            numbers.push_back(value);
            value = 0;
            inNumber = false;
        }
    }
    if (inNumber)
    {
        numbers.push_back(value);
    }
    return numbers;
}

int CompareVersion(const std::wstring& left, const std::wstring& right)
{
    const std::vector<int> lv = ParseVersionNumbers(left);
    const std::vector<int> rv = ParseVersionNumbers(right);
    const size_t n = lv.size() > rv.size() ? lv.size() : rv.size();
    for (size_t i = 0; i < n; ++i)
    {
        const int l = i < lv.size() ? lv[i] : 0;
        const int r = i < rv.size() ? rv[i] : 0;
        if (l < r) return -1;
        if (l > r) return 1;
    }

    const std::wstring ln = NormalizeVersion(left);
    const std::wstring rn = NormalizeVersion(right);
    if (ln == rn) return 0;
    return ln < rn ? -1 : 1;
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
        latest = ExtractJsonStringField(body, L"tag_name");
        result.releaseNotes = ExtractJsonStringField(body, L"body");

        // Prefer installer/binary asset URL from latest release.
        std::wstring asset = ExtractPreferredDownloadUrl(body);
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
            latest = ExtractJsonStringField(body, L"name");
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
    result.hasUpdate = CompareVersion(result.currentVersion, result.latestVersion) < 0;
    if (result.downloadUrl.empty())
    {
        result.downloadUrl = result.releaseUrl;
    }
    return result;
}
