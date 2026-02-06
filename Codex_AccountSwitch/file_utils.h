#pragma once

#include <string>

std::wstring MakeTempUserDataFolder();
void DeleteDirectoryRecursive(const std::wstring& dir);
