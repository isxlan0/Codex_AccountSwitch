param(
  [Parameter(Mandatory = $true)]
  [string]$Version,

  [string]$OutputPath,

  [string]$RepositoryRoot
)

$ErrorActionPreference = "Stop"
$utf8NoBom = [System.Text.UTF8Encoding]::new($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
  $RepositoryRoot = Resolve-Path (Join-Path $scriptDir "..")
} else {
  $RepositoryRoot = Resolve-Path $RepositoryRoot
}

$normalizedVersion = $Version.Trim()
if ([string]::IsNullOrWhiteSpace($normalizedVersion)) {
  throw "Version cannot be empty."
}

if ($normalizedVersion.StartsWith("v", [System.StringComparison]::OrdinalIgnoreCase)) {
  $normalizedVersion = $normalizedVersion.Substring(1)
}

$tagName = "v$normalizedVersion"

function Get-ChangelogPath {
  param([string]$Root)

  $candidates = @(
    "CHANGELOG.md",
    "CHANGELOG",
    "changelog.md"
  )

  foreach ($candidate in $candidates) {
    $path = Join-Path $Root $candidate
    if (Test-Path $path) {
      return $path
    }
  }

  return $null
}

function Get-ChangelogSection {
  param(
    [string]$Path,
    [string]$Version
  )

  if (-not (Test-Path $Path)) {
    return $null
  }

  $lines = Get-Content -Encoding UTF8 $Path
  $escapedVersion = [regex]::Escape($Version)
  $headingPattern = "^\s*(#{1,6})\s*\[?v?$escapedVersion\]?\b.*$"

  $startIndex = -1
  $headingLevel = 0
  for ($index = 0; $index -lt $lines.Count; $index++) {
    $match = [regex]::Match($lines[$index], $headingPattern)
    if ($match.Success) {
      $startIndex = $index
      $headingLevel = $match.Groups[1].Value.Length
      break
    }
  }

  if ($startIndex -lt 0) {
    return $null
  }

  $endIndex = $lines.Count
  for ($index = $startIndex + 1; $index -lt $lines.Count; $index++) {
    $match = [regex]::Match($lines[$index], "^\s*(#{1,6})\s+.+$")
    if ($match.Success -and $match.Groups[1].Value.Length -le $headingLevel) {
      $endIndex = $index
      break
    }
  }

  $sectionLines = @()
  if ($endIndex -gt ($startIndex + 1)) {
    $sectionLines = $lines[($startIndex + 1)..($endIndex - 1)]
  }

  $content = ($sectionLines -join [Environment]::NewLine).Trim()
  if ([string]::IsNullOrWhiteSpace($content)) {
    return $null
  }

  return $content
}

function Get-PreviousTag {
  param(
    [string]$Root,
    [string]$CurrentTag
  )

  Push-Location $Root
  try {
    $output = & git -c i18n.logOutputEncoding=utf-8 describe --tags --abbrev=0 2>$null
    if ($LASTEXITCODE -ne 0) {
      return $null
    }

    $tag = ($output | Select-Object -First 1).Trim()
    if ([string]::IsNullOrWhiteSpace($tag)) {
      return $null
    }

    if ($tag -eq $CurrentTag) {
      $headParent = & git rev-parse --verify HEAD^ 2>$null
      if ($LASTEXITCODE -ne 0) {
        return $null
      }

      $previousOutput = & git -c i18n.logOutputEncoding=utf-8 describe --tags --abbrev=0 HEAD^ 2>$null
      if ($LASTEXITCODE -ne 0) {
        return $null
      }

      $previousTag = ($previousOutput | Select-Object -First 1).Trim()
      if ([string]::IsNullOrWhiteSpace($previousTag)) {
        return $null
      }

      return $previousTag
    }

    return $tag
  }
  finally {
    Pop-Location
  }
}

function Get-CommitLines {
  param(
    [string]$Root,
    [string]$PreviousTag
  )

  Push-Location $Root
  try {
    $arguments = @("-c", "i18n.logOutputEncoding=utf-8", "log", "--pretty=format:- %s")
    if (-not [string]::IsNullOrWhiteSpace($PreviousTag)) {
      $arguments += "$PreviousTag..HEAD"
    }

    $output = & git @arguments
    if ($LASTEXITCODE -ne 0) {
      throw "Failed to generate git log."
    }

    $lines = @($output | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    if ($lines.Count -eq 0) {
      return @("- Maintenance release.")
    }

    return $lines
  }
  finally {
    Pop-Location
  }
}

$releaseNotes = $null
$changelogPath = Get-ChangelogPath -Root $RepositoryRoot
if ($changelogPath) {
  $releaseNotes = Get-ChangelogSection -Path $changelogPath -Version $normalizedVersion
}

if ([string]::IsNullOrWhiteSpace($releaseNotes)) {
  $previousTag = Get-PreviousTag -Root $RepositoryRoot -CurrentTag $tagName
  $commitLines = Get-CommitLines -Root $RepositoryRoot -PreviousTag $previousTag
  $releaseNotes = @(
    "Release $tagName",
    "",
    "Changes:",
    $commitLines
  ) -join [Environment]::NewLine
}

if (-not [string]::IsNullOrWhiteSpace($OutputPath)) {
  $outputDir = Split-Path -Parent $OutputPath
  if (-not [string]::IsNullOrWhiteSpace($outputDir) -and -not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir | Out-Null
  }

  [System.IO.File]::WriteAllText($OutputPath, $releaseNotes, [System.Text.UTF8Encoding]::new($false))
}

$releaseNotes
