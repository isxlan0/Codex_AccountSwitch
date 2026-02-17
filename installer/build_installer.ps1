param(
  [string]$IsccPath = "iscc",
  [switch]$SkipPortable,
  [string]$TargetPlatform = "windows",
  [string]$TargetArchitecture = "x64",
  [string[]]$TargetArchitectures
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$rootDir = Resolve-Path (Join-Path $scriptDir "..")
$issPath = Join-Path $scriptDir "Codex_AccountSwitch.iss"
$versionHeaderPath = Join-Path $rootDir "Codex_AccountSwitch\\app_version.h"
$portableBuildScriptPath = Join-Path $rootDir "tools\\build_portable.ps1"

function Resolve-IsccExecutable([string]$candidate) {
  if (-not [string]::IsNullOrWhiteSpace($candidate)) {
    if (Test-Path $candidate) {
      return (Resolve-Path $candidate).Path
    }
    $cmd = Get-Command $candidate -ErrorAction SilentlyContinue
    if ($cmd -and $cmd.Path) {
      return $cmd.Path
    }
  }

  $knownPaths = @(
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
  )
  foreach ($path in $knownPaths) {
    if (-not [string]::IsNullOrWhiteSpace($path) -and (Test-Path $path)) {
      return $path
    }
  }

  throw "ISCC not found. Install Inno Setup 6, or pass -IsccPath `"C:\Path\To\ISCC.exe`"."
}

if (-not (Test-Path $issPath)) {
  throw "Installer script not found: $issPath"
}
if (-not (Test-Path $versionHeaderPath)) {
  throw "Version header not found: $versionHeaderPath"
}
if (-not $SkipPortable -and -not (Test-Path $portableBuildScriptPath)) {
  throw "Portable build script not found: $portableBuildScriptPath"
}

$versionHeader = Get-Content -Raw $versionHeaderPath
$major = [regex]::Match($versionHeader, '#define\s+APP_VERSION_MAJOR\s+(?<v>\d+)')
$minor = [regex]::Match($versionHeader, '#define\s+APP_VERSION_MINOR\s+(?<v>\d+)')
$patch = [regex]::Match($versionHeader, '#define\s+APP_VERSION_PATCH\s+(?<v>\d+)')
if (-not ($major.Success -and $minor.Success -and $patch.Success)) {
  throw "Failed to parse APP_VERSION_MAJOR/MINOR/PATCH from $versionHeaderPath"
}
$appVersion = "$($major.Groups['v'].Value).$($minor.Groups['v'].Value).$($patch.Groups['v'].Value)"
$resolvedIsccPath = Resolve-IsccExecutable $IsccPath
$normalizedPlatform = $TargetPlatform.Trim().ToLowerInvariant()
if ([string]::IsNullOrWhiteSpace($normalizedPlatform)) {
  $normalizedPlatform = "windows"
}

if ($null -eq $TargetArchitectures -or $TargetArchitectures.Count -eq 0) {
  if ($PSBoundParameters.ContainsKey("TargetArchitecture")) {
    $TargetArchitectures = @($TargetArchitecture)
  } else {
    $TargetArchitectures = @("x64", "x86", "arm")
  }
}

function Resolve-NormalizedArchitecture([string]$arch) {
  $value = $arch.Trim().ToLowerInvariant()
  if ([string]::IsNullOrWhiteSpace($value)) {
    return $null
  }

  switch ($value) {
    "win32" { return "x86" }
    "arm64" { return "arm" }
    "amd64" { return "x64" }
    "x64" { return "x64" }
    "x86" { return "x86" }
    "arm" { return "arm" }
    default { return $null }
  }
}

$normalizedArchitectures = @()
foreach ($arch in $TargetArchitectures) {
  $normalizedArch = Resolve-NormalizedArchitecture $arch
  if (-not $normalizedArch) {
    throw "Unsupported architecture: $arch. Expected x64, x86, or arm."
  }

  if ($normalizedArchitectures -notcontains $normalizedArch) {
    $normalizedArchitectures += $normalizedArch
  }
}

if ($normalizedArchitectures.Count -eq 0) {
  throw "No target architectures resolved. Expected x64, x86, or arm."
}

Push-Location $rootDir
try {
  foreach ($normalizedArch in $normalizedArchitectures) {
    $binarySubDir = switch ($normalizedArch) {
      "x64" { "x64" }
      "x86" { "x86" }
      "arm" { "ARM" }
    }
    $installerAllowed = switch ($normalizedArch) {
      "x64" { "x64compatible" }
      "x86" { "x86compatible" }
      "arm" { "arm64" }
    }
    $installerMode = switch ($normalizedArch) {
      "x64" { "x64compatible" }
      "x86" { "x86" }
      "arm" { "arm64" }
    }

    & $resolvedIsccPath "/DMyAppVersion=$appVersion" "/DPackagePlatform=$normalizedPlatform" "/DPackageArchitecture=$normalizedArch" "/DBinarySubDir=$binarySubDir" "/DInstallerArchitecturesAllowed=$installerAllowed" "/DInstallerArchitecturesInstallIn64BitMode=$installerMode" $issPath
    if ($LASTEXITCODE -ne 0) {
      throw "ISCC failed for $normalizedArch with exit code $LASTEXITCODE"
    }

    if (-not $SkipPortable) {
      & $portableBuildScriptPath -Configuration "Release" -Platform $normalizedArch -TargetPlatform $normalizedPlatform -TargetArchitecture $normalizedArch
      if ($LASTEXITCODE -ne 0) {
        throw "Portable build failed for $normalizedArch with exit code $LASTEXITCODE"
      }
    }
  }
}
finally {
  Pop-Location
}
