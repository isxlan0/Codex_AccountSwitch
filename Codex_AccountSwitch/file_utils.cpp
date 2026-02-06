#include "file_utils.h"

#include <objbase.h>
#include <windows.h>

std::wstring MakeTempUserDataFolder()
{
    wchar_t tempPath[MAX_PATH]{};
    const DWORD n = GetTempPathW(MAX_PATH, tempPath);
    if (n == 0 || n >= MAX_PATH)
    {
        return L"";
    }

    GUID guid{};
    if (FAILED(CoCreateGuid(&guid)))
    {
        return L"";
    }

    wchar_t guidStr[64]{};
    StringFromGUID2(guid, guidStr, 64);

    std::wstring dir = std::wstring(tempPath) + L"Codex_AccountSwitch_WebView2_" + guidStr;
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

void DeleteDirectoryRecursive(const std::wstring& dir)
{
    const std::wstring spec = dir + L"\\*";
    WIN32_FIND_DATAW fileData{};
    HANDLE hFind = FindFirstFileW(spec.c_str(), &fileData);

    if (hFind == INVALID_HANDLE_VALUE)
    {
        RemoveDirectoryW(dir.c_str());
        return;
    }

    do
    {
        const wchar_t* name = fileData.cFileName;
        if (wcscmp(name, L".") == 0 || wcscmp(name, L"..") == 0)
        {
            continue;
        }

        const std::wstring path = dir + L"\\" + name;
        if ((fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
            DeleteDirectoryRecursive(path);
            RemoveDirectoryW(path.c_str());
        }
        else
        {
            SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
            DeleteFileW(path.c_str());
        }
    } while (FindNextFileW(hFind, &fileData));

    FindClose(hFind);
    RemoveDirectoryW(dir.c_str());
}
