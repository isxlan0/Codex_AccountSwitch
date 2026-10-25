#pragma once

std::wstring ProfileTomlName(const std::wstring &name)
{
  std::wstring result;
  wchar_t quote = 0;
  for (size_t i = 0; i < name.size(); ++i)
  {
    const auto c = name[i];
    if (quote)
    {
      if (quote == L'"' && c == L'\\' && i + 1 < name.size()) result += name[++i];
      else if (c == quote) quote = 0;
      else result += c;
    }
    else if (c == L'"' || c == L'\'') quote = c;
    else if (c == L'.') result += L'\x1f';
    else if (!iswspace(c)) result += c;
  }
  return result;
}

size_t ProfileTomlHeaderEnd(const std::wstring &line)
{
  wchar_t quote = 0;
  for (size_t i = 1; i < line.size(); ++i)
  {
    const auto c = line[i];
    if (quote)
    {
      if (quote == L'"' && c == L'\\') ++i;
      else if (c == quote) quote = 0;
    }
    else if (c == L'"' || c == L'\'') quote = c;
    else if (c == L']') return i;
  }
  return std::wstring::npos;
}

// Consume a complete TOML value, including multiline strings and arrays.
// Untouched values are never parsed and serialized again.
size_t ProfileTomlValueEnd(const std::wstring &text, size_t start)
{
  wchar_t quote = 0;
  bool triple = false;
  int depth = 0;
  for (size_t i = start; i < text.size(); ++i)
  {
    const auto c = text[i];
    if (quote)
    {
      if (quote == L'"' && c == L'\\') { ++i; continue; }
      if (c != quote) continue;
      if (!triple) quote = 0;
      else if (text.compare(i, 3, std::wstring(3, quote)) == 0)
      {
        i += 2;
        while (i + 1 < text.size() && text[i + 1] == quote) ++i;
        quote = 0; triple = false;
      }
    }
    else if (c == L'"' || c == L'\'')
    {
      quote = c;
      triple = text.compare(i, 3, std::wstring(3, c)) == 0;
      if (triple) i += 2;
    }
    else if (c == L'[' || c == L'{') ++depth;
    else if (c == L']' || c == L'}') --depth;
    else if (c == L'#')
    {
      if (depth == 0) return i;
      const auto end = text.find(L'\n', i);
      if (end == std::wstring::npos) return text.size();
      i = end;
    }
    else if ((c == L'\r' || c == L'\n') && depth == 0) return i;
  }
  return text.size();
}

// Patch one named value in the existing text, not a whole file or table.
std::wstring PatchProfileTomlField(std::wstring text, const std::wstring &table,
                                   const std::wstring &key, const std::wstring &rawValue)
{
  const auto target = ProfileTomlName(table);
  const std::wstring newline = text.find(L"\r\n") == std::wstring::npos ? L"\n" : L"\r\n";
  std::wstring section;
  size_t insert = text.size();
  bool foundTable = table.empty();
  for (size_t start = 0; start < text.size();)
  {
    const auto end = text.find(L'\n', start);
    auto next = end == std::wstring::npos ? text.size() : end + 1;
    const auto line = text.substr(start, next - start);
    const auto trimmed = TrimWide(line);
    if (!trimmed.empty() && trimmed.front() == L'[')
    {
      const auto close = ProfileTomlHeaderEnd(trimmed);
      if (close != std::wstring::npos)
      {
        if (section == target && foundTable) insert = start;
        section = ProfileTomlName(trimmed.substr(1, close - 1));
        if (section == target) { foundTable = true; insert = next; }
      }
    }
    else if (!trimmed.empty() && trimmed.front() != L'#')
    {
      const auto eq = line.find(L'=');
      if (eq != std::wstring::npos)
      {
        size_t valueStart = start + eq + 1;
        while (valueStart < text.size() && (text[valueStart] == L' ' || text[valueStart] == L'\t')) ++valueStart;
        auto valueEnd = ProfileTomlValueEnd(text, valueStart);
        const auto lastLine = text.find(L'\n', valueEnd);
        next = lastLine == std::wstring::npos ? text.size() : lastLine + 1;
        if (section == target && ProfileTomlName(line.substr(0, eq)) == ProfileTomlName(key))
        {
          if (rawValue.empty()) { text.erase(start, next - start); return text; }
          while (valueEnd > valueStart && iswspace(text[valueEnd - 1])) --valueEnd;
          text.replace(valueStart, valueEnd - valueStart, rawValue);
          return text;
        }
      }
      if (section == target && foundTable) insert = next;
    }
    start = next;
  }
  if (rawValue.empty()) return text;
  std::wstring addition;
  if (!foundTable)
  {
    insert = text.size();
    addition = L"[" + table + L"]" + newline;
  }
  if (insert > 0 && text[insert - 1] != L'\n') addition = newline + addition;
  addition += key + L" = " + rawValue + newline;
  text.insert(insert, addition);
  return text;
}
