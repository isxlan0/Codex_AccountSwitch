param(
  [string[]]$Architectures = @("x64", "x86", "arm"),
  [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$rootDir = Resolve-Path (Join-Path $scriptDir "..")
$projectPath = Join-Path $rootDir "Codex_AccountSwitch\Codex_AccountSwitch.vcxproj"

if (-not (Test-Path $projectPath)) {
  throw "Project file not found: $projectPath"
}

function Resolve-MSBuildPath {
  $cmd = Get-Command msbuild -ErrorAction SilentlyContinue
  if ($cmd -and $cmd.Path) {
    return $cmd.Path
  }

  $vswherePath = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
  if (Test-Path $vswherePath) {
    $installPath = & $vswherePath -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
    if (-not [string]::IsNullOrWhiteSpace($installPath)) {
      $candidate = Join-Path $installPath "MSBuild\Current\Bin\MSBuild.exe"
      if (Test-Path $candidate) {
        return $candidate
      }
    }
  }

  $fallbackPaths = @(
    "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe",
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
  )
  foreach ($path in $fallbackPaths) {
    if (Test-Path $path) {
      return $path
    }
  }

  return $null
}

$msbuildPath = Resolve-MSBuildPath
if (-not $msbuildPath) {
  throw "MSBuild was not found. Install Visual Studio Build Tools (MSBuild), or run in Developer PowerShell."
}

$targets = @()
foreach ($arch in $Architectures) {
  $normalized = $arch.Trim().ToLowerInvariant()
  if ([string]::IsNullOrWhiteSpace($normalized)) {
    continue
  }

  switch ($normalized) {
    "x64" { $targets += @{ Label = "x64"; Platform = "x64" } }
    "x86" { $targets += @{ Label = "x86"; Platform = "Win32" } }
    "win32" { $targets += @{ Label = "x86"; Platform = "Win32" } }
    "arm" { $targets += @{ Label = "ARM"; Platform = "ARM64" } }
    "arm64" { $targets += @{ Label = "ARM"; Platform = "ARM64" } }
    default { throw "Unsupported architecture: $arch. Expected x64, x86, or arm." }
  }
}

if ($targets.Count -eq 0) {
  throw "No architectures selected."
}

Push-Location $rootDir
try {
  foreach ($target in $targets) {
    Write-Host "Building $Configuration | $($target.Label) ..."
    & $msbuildPath $projectPath "/p:Configuration=$Configuration" "/p:Platform=$($target.Platform)" "/m"
    if ($LASTEXITCODE -ne 0) {
      throw "Build failed for $Configuration | $($target.Label) with exit code $LASTEXITCODE"
    }
  }
}
finally {
  Pop-Location
}

Write-Host "Build completed. Outputs are under Release\\x64, Release\\x86, Release\\ARM."
