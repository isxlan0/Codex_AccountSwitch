#pragma once

struct ProfileFieldReplacement
{
  std::wstring original, written;
};

struct ProfileSessionChanges
{
  std::map<std::pair<std::wstring, std::wstring>, ProfileFieldReplacement> toml;
  bool authChanged = false;
  ProfileFieldReplacement auth;
};

ProfileSessionChanges g_ApiProfileChanges, g_ProxyProfileChanges;
std::wstring g_LastWrittenCodexAuth, g_LastWrittenCodexToml;

std::wstring ProfileApiKeyRaw(const std::wstring &json);

void TrackProfileApiKey(ProfileSessionChanges &changes,
                        const std::wstring &before, const std::wstring &after)
{
  const auto oldValue = ProfileApiKeyRaw(before), newValue = ProfileApiKeyRaw(after);
  if (oldValue == newValue) return;
  if (!changes.authChanged || changes.auth.written != oldValue)
    changes.auth.original = oldValue;
  changes.auth.written = newValue;
  changes.authChanged = true;
}

void TrackProfileToml(ProfileSessionChanges &changes, const std::wstring &before,
                       const std::wstring &after, const std::wstring &provider,
                       bool model, bool authMethod)
{
  auto track = [&](const std::wstring &table, const std::wstring &key) {
    const auto oldValue = ApiTomlRawValue(before, table, key);
    const auto newValue = ApiTomlRawValue(after, table, key);
    if (oldValue == newValue) return;
    const auto id = std::make_pair(table, key);
    auto entry = changes.toml.find(id);
    if (entry == changes.toml.end())
      changes.toml.emplace(id, ProfileFieldReplacement{oldValue, newValue});
    else
    {
      // A later external edit becomes the baseline before another explicit switch.
      if (entry->second.written != oldValue) entry->second.original = oldValue;
      entry->second.written = newValue;
    }
  };
  track(L"", L"model_provider");
  if (model) track(L"", L"model");
  if (authMethod) track(L"", L"preferred_auth_method");
  for (const auto *key : {L"name", L"base_url", L"wire_api", L"requires_openai_auth", L"experimental_bearer_token"})
    track(L"model_providers." + provider, key);
}

