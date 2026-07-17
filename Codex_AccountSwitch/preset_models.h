#pragma execution_character_set("utf-8")
#pragma once

#include <string>
#include <vector>

inline const std::vector<std::wstring> &GetManuallyRegisteredModelIds()
{
    static const std::vector<std::wstring> kManualModelIds = {
        L"latest",
        L"o3",
        L"gpt-5-3",
        L"gpt-5-3-instant",
        L"gpt-5-4-thinking",
        L"gpt-5-4-pro",
        L"gpt-5-5",
        L"gpt-5-5-instant",
        L"gpt-5-5-thinking",
        L"gpt-5-6-thinking",
        L"gpt-5.5-wm",
        L"gpt-5.5-cca-wm",
        L"gpt-5.6-sol-wm",
        L"gpt-5.6-terra-wm",
        L"gpt-5.6-luna-wm",
        L"gpt-5-5-pro",
        L"gpt-5-6-pro",
        L"gpt-5-mini",
        L"gpt-5-3-mini",
        L"gpt-5-5-mini",
        L"research",
    };
    return kManualModelIds;
}

inline const std::vector<std::wstring> &GetPresetModelIds()
{
    static const std::vector<std::wstring> kPresetModelIds = []()
    {
        std::vector<std::wstring> models = {
            L"gpt-5.5",
            L"gpt-5.2",
            L"gpt-5.4",
            L"gpt-5.4-mini",
            L"gpt-5.3-codex",
            L"gpt-5.3-codex-spark",
            L"gpt-5.2-codex",
            L"gpt-5.1-codex-max",
            L"gpt-5.1-codex-mini",
        };
        const auto &manualModels = GetManuallyRegisteredModelIds();
        models.insert(models.end(), manualModels.begin(), manualModels.end());
        return models;
    }();
    return kPresetModelIds;
}
