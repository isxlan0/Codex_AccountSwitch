@echo off
setlocal

set SCRIPT_DIR=%~dp0
set ISCC_EXE=C:\Program Files (x86)\Inno Setup 6\ISCC.exe
set TARGET_PLATFORM=%~1
set TARGET_ARCH=%~2
if "%TARGET_PLATFORM%"=="" set TARGET_PLATFORM=windows
set TARGET_ARCH_SPECIFIED=1
if "%TARGET_ARCH%"=="" set TARGET_ARCH_SPECIFIED=0
if "%TARGET_ARCH%"=="" set TARGET_ARCH=x64
set BINARY_SUBDIR=x64
set INSTALLER_ALLOWED=x64compatible
set INSTALLER_MODE=x64compatible

if /I "%TARGET_ARCH%"=="x86" (
  set BINARY_SUBDIR=x86
  set INSTALLER_ALLOWED=x86compatible
  set INSTALLER_MODE=x86
)
if /I "%TARGET_ARCH%"=="win32" (
  set TARGET_ARCH=x86
  set BINARY_SUBDIR=x86
  set INSTALLER_ALLOWED=x86compatible
  set INSTALLER_MODE=x86
)
if /I "%TARGET_ARCH%"=="arm" (
  set BINARY_SUBDIR=ARM
  set INSTALLER_ALLOWED=arm64
  set INSTALLER_MODE=arm64
)
if /I "%TARGET_ARCH%"=="arm64" (
  set TARGET_ARCH=arm
  set BINARY_SUBDIR=ARM
  set INSTALLER_ALLOWED=arm64
  set INSTALLER_MODE=arm64
)

pushd "%SCRIPT_DIR%"

if exist ".\build_installer.ps1" (
  if exist "%ISCC_EXE%" (
    if "%TARGET_ARCH_SPECIFIED%"=="1" (
      powershell -NoProfile -ExecutionPolicy Bypass -File ".\build_installer.ps1" -IsccPath "%ISCC_EXE%" -TargetPlatform "%TARGET_PLATFORM%" -TargetArchitecture "%TARGET_ARCH%"
    ) else (
      powershell -NoProfile -ExecutionPolicy Bypass -File ".\build_installer.ps1" -IsccPath "%ISCC_EXE%" -TargetPlatform "%TARGET_PLATFORM%"
    )
  ) else (
    if "%TARGET_ARCH_SPECIFIED%"=="1" (
      powershell -NoProfile -ExecutionPolicy Bypass -File ".\build_installer.ps1" -TargetPlatform "%TARGET_PLATFORM%" -TargetArchitecture "%TARGET_ARCH%"
    ) else (
      powershell -NoProfile -ExecutionPolicy Bypass -File ".\build_installer.ps1" -TargetPlatform "%TARGET_PLATFORM%"
    )
  )
) else (
  if exist "%ISCC_EXE%" (
    "%ISCC_EXE%" "/DPackagePlatform=%TARGET_PLATFORM%" "/DPackageArchitecture=%TARGET_ARCH%" "/DBinarySubDir=%BINARY_SUBDIR%" "/DInstallerArchitecturesAllowed=%INSTALLER_ALLOWED%" "/DInstallerArchitecturesInstallIn64BitMode=%INSTALLER_MODE%" ".\Codex_AccountSwitch.iss"
  ) else (
    iscc "/DPackagePlatform=%TARGET_PLATFORM%" "/DPackageArchitecture=%TARGET_ARCH%" "/DBinarySubDir=%BINARY_SUBDIR%" "/DInstallerArchitecturesAllowed=%INSTALLER_ALLOWED%" "/DInstallerArchitecturesInstallIn64BitMode=%INSTALLER_MODE%" ".\Codex_AccountSwitch.iss"
  )
)

if errorlevel 1 (
  echo.
  echo Build installer failed.
  popd
  exit /b 1
)

echo.
echo Installer and portable package generated in dist\ (%TARGET_PLATFORM% %TARGET_ARCH%)
popd
exit /b 0
