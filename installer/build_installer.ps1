param(
  [string]$IsccPath = "iscc"
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$rootDir = Resolve-Path (Join-Path $scriptDir "..")
$issPath = Join-Path $scriptDir "Codex_AccountSwitch.iss"
$versionHeaderPath = Join-Path $rootDir "Codex_AccountSwitch\\app_version.h"

if (-not (Test-Path $issPath)) {
  throw "Installer script not found: $issPath"
}
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

Push-Location $rootDir
try {
  & $IsccPath "/DMyAppVersion=$appVersion" $issPath
  if ($LASTEXITCODE -ne 0) {
    throw "ISCC failed with exit code $LASTEXITCODE"
  }
}
finally {
  Pop-Location
}
