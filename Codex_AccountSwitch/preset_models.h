#pragma once

#include <string>
#include <vector>

inline const std::vector<std::wstring> &GetPresetModelIds()
{
    static const std::vector<std::wstring> kPresetModelIds = {
        L"gpt-5.2",
        L"gpt-5.4",
        L"gpt-5.4-mini",
        L"gpt-5.3-codex",
        L"gpt-5.3-codex-spark",
        L"gpt-5.2-codex",
        L"gpt-5.1-codex-max",
        L"gpt-5.1-codex-mini",
    };
    return kPresetModelIds;
}
