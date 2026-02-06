@echo off
setlocal

set SCRIPT_DIR=%~dp0
set ISCC_EXE=C:\Program Files (x86)\Inno Setup 6\ISCC.exe

pushd "%SCRIPT_DIR%"

if exist ".\build_installer.ps1" (
  if exist "%ISCC_EXE%" (
    powershell -NoProfile -ExecutionPolicy Bypass -File ".\build_installer.ps1" -IsccPath "%ISCC_EXE%"
  ) else (
    powershell -NoProfile -ExecutionPolicy Bypass -File ".\build_installer.ps1"
  )
) else (
  if exist "%ISCC_EXE%" (
    "%ISCC_EXE%" ".\Codex_AccountSwitch.iss"
  ) else (
    iscc ".\Codex_AccountSwitch.iss"
  )
)

if errorlevel 1 (
  echo.
  echo Build installer failed.
  popd
  exit /b 1
)

echo.
echo Installer generated in dist\
popd
exit /b 0
