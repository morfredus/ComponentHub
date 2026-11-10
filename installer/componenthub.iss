; ComponentHub installer (Inno Setup 6). Run by scripts\windows\package-win.ps1, which defines:
;   /DAppVersion=1.18.1  /DAppVersionNum=1.18.1.0  /DStageDir=<folder to install>
; ASCII only and no BOM, like every other text file of the repository.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef AppVersionNum
  #define AppVersionNum "0.0.0.0"
#endif
#ifndef StageDir
  #error StageDir must be defined (see package-win.ps1)
#endif

[Setup]
; Never change this identifier: it is how a new version finds and replaces the installed one.
AppId={{219DCF2F-9968-4517-864C-B7EEC7944233}
AppName=ComponentHub
AppVersion={#AppVersion}
AppVerName=ComponentHub {#AppVersion}
AppPublisher=morfredus
AppPublisherURL=https://morfredus.fr
VersionInfoVersion={#AppVersionNum}
DefaultDirName={autopf}\ComponentHub
DefaultGroupName=ComponentHub
AllowNoIcons=yes
UninstallDisplayIcon={app}\ComponentHub.exe
OutputBaseFilename=ComponentHub-{#AppVersion}-win64-setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Per-user install by default (no administrator rights); the dialog still offers all users.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
; Close a running ComponentHub instead of failing on a locked file.
CloseApplications=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\ComponentHub"; Filename: "{app}\ComponentHub.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\ComponentHub"; Filename: "{app}\ComponentHub.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Run]
Filename: "{app}\ComponentHub.exe"; Description: "{cm:LaunchProgram,ComponentHub}"; Flags: nowait postinstall skipifsilent

[Code]
// The user's data (database under %APPDATA%\morfredus\ComponentHub, settings under
// HKCU\Software\morfredus\ComponentHub) is kept by default; the question defaults to "No".
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if (CurUninstallStep = usPostUninstall) and not UninstallSilent then
    if MsgBox(ExpandConstant('{cm:KeepData}'), mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES then
    begin
      DelTree(ExpandConstant('{userappdata}\morfredus\ComponentHub'), True, True, True);
      RegDeleteKeyIncludingSubkeys(HKCU, 'Software\morfredus\ComponentHub');
    end;
end;

[CustomMessages]
english.KeepData=Also delete your ComponentHub data (database and settings)?%n%nChoose No to keep them for a later reinstall.
french.KeepData=Supprimer aussi vos donnees ComponentHub (base de donnees et reglages) ?%n%nChoisir Non pour les conserver en vue d'une reinstallation.
