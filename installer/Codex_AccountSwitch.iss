; Inno Setup script for Codex Account Switch
; Build command example:
;   iscc installer\Codex_AccountSwitch.iss

#define MyAppName "Codex Account Switch"
#define MyAppPublisher "Xiao Lan"
#define MyAppURL "https://github.com/isxlan0/Codex_AccountSwitch"
#define MyAppExeName "Codex_AccountSwitch.exe"
#define MyAppVersion "1.0.0"

[Setup]
AppId={{0ACAA3F5-9AE4-4E3E-9D0D-7B9A7A7D1C21}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={code:GetInstallDir}
DefaultGroupName={#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
OutputDir=..\dist
OutputBaseFilename=Codex_AccountSwitch_Setup_v{#MyAppVersion}
SetupIconFile=..\logo.ico
LicenseFile=..\LICENSE
InfoBeforeFile=INFO_BEFORE_INSTALL_EN.txt
InfoAfterFile=INFO_AFTER_INSTALL_EN.txt
Compression=lzma
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
; App binaries
Source: "..\x64\Release\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\x64\Release\WebView2Loader.dll"; DestDir: "{app}"; Flags: ignoreversion

; Web UI
Source: "..\webui\*"; DestDir: "{app}\webui"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
function GetInstallDir(Param: string): string;
begin
  if IsAdminInstallMode then
    Result := ExpandConstant('{autopf}\Codex Account Switch')
  else
    Result := ExpandConstant('{localappdata}\Programs\Codex Account Switch');
end;

procedure DeletePathIfExists(const Path: string);
begin
  if DirExists(Path) then
  begin
    DelTree(Path, True, True, True);
  end
  else if FileExists(Path) then
  begin
    DeleteFile(Path);
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  KeepData: Integer;
  ConfigPath: string;
  BackupsPath: string;
  ResultCode: Integer;
begin
  if CurUninstallStep <> usUninstall then
    exit;

  Exec(
    ExpandConstant('{cmd}'),
    '/C taskkill /F /IM Codex_AccountSwitch.exe /T >nul 2>nul',
    '',
    SW_HIDE,
    ewWaitUntilTerminated,
    ResultCode);

  ConfigPath := ExpandConstant('{localappdata}\Codex Account Switch\config.json');
  BackupsPath := ExpandConstant('{localappdata}\Codex Account Switch\backups');

  if (not FileExists(ConfigPath)) and (not DirExists(BackupsPath)) then
    exit;

  KeepData := MsgBox(
    'Do you want to keep user config and account backup data?'#13#10#13#10 +
    'Yes: Keep config.json and backups'#13#10 +
    'No: Remove them during uninstall',
    mbConfirmation,
    MB_YESNO);

  if KeepData = IDNO then
  begin
    DeletePathIfExists(ConfigPath);
    DeletePathIfExists(BackupsPath);
  end;
end;
