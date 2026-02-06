param(
  [string]$IsccPath = "iscc"
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$rootDir = Resolve-Path (Join-Path $scriptDir "..")
$issPath = Join-Path $scriptDir "Codex_AccountSwitch.iss"

if (-not (Test-Path $issPath)) {
  throw "Installer script not found: $issPath"
}

Push-Location $rootDir
try {
  & $IsccPath $issPath
  if ($LASTEXITCODE -ne 0) {
    throw "ISCC failed with exit code $LASTEXITCODE"
  }
}
finally {
  Pop-Location
}
