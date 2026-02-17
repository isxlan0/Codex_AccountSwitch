@echo off
setlocal

set SCRIPT_DIR=%~dp0
set ISCC_EXE=C:\Program Files (x86)\Inno Setup 6\ISCC.exe
set TARGET_PLATFORM=%~1
set TARGET_ARCH=%~2
if "%TARGET_PLATFORM%"=="" set TARGET_PLATFORM=windows
if "%TARGET_ARCH%"=="" set TARGET_ARCH=x64

pushd "%SCRIPT_DIR%"

if exist ".\build_installer.ps1" (
  if exist "%ISCC_EXE%" (
    powershell -NoProfile -ExecutionPolicy Bypass -File ".\build_installer.ps1" -IsccPath "%ISCC_EXE%" -TargetPlatform "%TARGET_PLATFORM%" -TargetArchitecture "%TARGET_ARCH%"
  ) else (
    powershell -NoProfile -ExecutionPolicy Bypass -File ".\build_installer.ps1" -TargetPlatform "%TARGET_PLATFORM%" -TargetArchitecture "%TARGET_ARCH%"
  )
) else (
  if exist "%ISCC_EXE%" (
    "%ISCC_EXE%" "/DPackagePlatform=%TARGET_PLATFORM%" "/DPackageArchitecture=%TARGET_ARCH%" ".\Codex_AccountSwitch.iss"
  ) else (
    iscc "/DPackagePlatform=%TARGET_PLATFORM%" "/DPackageArchitecture=%TARGET_ARCH%" ".\Codex_AccountSwitch.iss"
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
