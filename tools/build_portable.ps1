param(
  [string]$Configuration = "Release",
  [string]$Platform = "x64",
  [string]$TargetPlatform = "windows",
  [string]$TargetArchitecture = "x64"
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$rootDir = Resolve-Path (Join-Path $scriptDir "..")
$versionHeaderPath = Join-Path $rootDir "Codex_AccountSwitch\app_version.h"
$distDir = Join-Path $rootDir "dist"
$stageRoot = Join-Path $distDir "portable_stage"

if (-not (Test-Path $versionHeaderPath)) {
  throw "Version header not found: $versionHeaderPath"
}

$versionHeader = Get-Content -Raw $versionHeaderPath
$major = [regex]::Match($versionHeader, '#define\s+APP_VERSION_MAJOR\s+(?<v>\d+)')
$minor = [regex]::Match($versionHeader, '#define\s+APP_VERSION_MINOR\s+(?<v>\d+)')
$patch = [regex]::Match($versionHeader, '#define\s+APP_VERSION_PATCH\s+(?<v>\d+)')
if (-not ($major.Success -and $minor.Success -and $patch.Success)) {
  throw "Failed to parse APP_VERSION_MAJOR/MINOR/PATCH from $versionHeaderPath"
}

$appVersion = "$($major.Groups['v'].Value).$($minor.Groups['v'].Value).$($patch.Groups['v'].Value)"
$normalizedPlatform = $TargetPlatform.Trim().ToLowerInvariant()
$normalizedArch = $TargetArchitecture.Trim().ToLowerInvariant()
if ([string]::IsNullOrWhiteSpace($normalizedPlatform)) {
  $normalizedPlatform = "windows"
}
if ([string]::IsNullOrWhiteSpace($normalizedArch)) {
  $normalizedArch = $Platform.Trim().ToLowerInvariant()
}
if ([string]::IsNullOrWhiteSpace($normalizedArch)) {
  $normalizedArch = "x64"
}

switch ($normalizedArch) {
  "win32" { $normalizedArch = "x86" }
  "arm64" { $normalizedArch = "arm" }
  "amd64" { $normalizedArch = "x64" }
}

$binarySubDir = switch ($normalizedArch) {
  "x64" { "x64" }
  "x86" { "x86" }
  "arm" { "ARM" }
  default { $null }
}
if (-not $binarySubDir) {
  throw "Unsupported TargetArchitecture: $TargetArchitecture. Expected x64, x86, or arm."
}
$binDir = Join-Path $rootDir "Release\$binarySubDir"

$portableName = "Codex_AccountSwitch_Portable_${normalizedPlatform}_${normalizedArch}_v$appVersion"
$portableDir = Join-Path $stageRoot $portableName
$zipPath = Join-Path $distDir "$portableName.zip"

$requiredFiles = @(
  (Join-Path $binDir "Codex_AccountSwitch.exe"),
  (Join-Path $binDir "WebView2Loader.dll"),
  (Join-Path $rootDir "webui\index.html")
)
foreach ($file in $requiredFiles) {
  if (-not (Test-Path $file)) {
    throw "Required file not found: $file"
  }
}

if (Test-Path $portableDir) {
  Remove-Item -Recurse -Force $portableDir
}
if (Test-Path $zipPath) {
  Remove-Item -Force $zipPath
}

try {
  New-Item -ItemType Directory -Path $portableDir | Out-Null
  New-Item -ItemType Directory -Path (Join-Path $portableDir "data") | Out-Null

  Copy-Item -Path (Join-Path $binDir "Codex_AccountSwitch.exe") -Destination (Join-Path $portableDir "Codex_AccountSwitch.exe") -Force
  Copy-Item -Path (Join-Path $binDir "WebView2Loader.dll") -Destination (Join-Path $portableDir "WebView2Loader.dll") -Force
  Copy-Item -Path (Join-Path $rootDir "webui") -Destination (Join-Path $portableDir "webui") -Recurse -Force
  Set-Content -Path (Join-Path $portableDir "portable.mode") -Value "portable=1" -Encoding ASCII

  if (-not (Test-Path $distDir)) {
    New-Item -ItemType Directory -Path $distDir | Out-Null
  }
  Compress-Archive -Path (Join-Path $portableDir "*") -DestinationPath $zipPath -Force
  Write-Host "Portable package generated: $zipPath"
}
finally {
  if (Test-Path $stageRoot) {
    Remove-Item -Recurse -Force $stageRoot
  }
}
