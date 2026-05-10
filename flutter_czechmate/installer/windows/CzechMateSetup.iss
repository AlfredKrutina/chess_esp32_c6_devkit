; CzechMate — Inno Setup 6 (Windows x64).
; CI: viz .github/workflows/flutter-app-release.yml
; Lokálně po `flutter build windows --release`:
;   "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" ^
;     /DMyAppDisplayVersion=1.8.0 ^
;     /DMyAppOutputVersion=1.8.0-3 ^
;     /DMyAppSourceDir="C:\...\flutter_czechmate\build\windows\x64\runner\Release" ^
;     CzechMateSetup.iss

#define MyAppName "CzechMate"
#ifndef MyAppDisplayVersion
  #define MyAppDisplayVersion "0.0.0"
#endif
#ifndef MyAppOutputVersion
  #define MyAppOutputVersion "0.0.0"
#endif
#ifndef MyAppSourceDir
  #define MyAppSourceDir "..\..\build\windows\x64\runner\Release"
#endif
#define MyAppPublisher "CzechMate"
#define MyAppExeName "czechmate.exe"

[Setup]
AppId={{F8E7D6C5-B4A3-4918-9CDE-112233445566}
AppName={#MyAppName}
AppVersion={#MyAppDisplayVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
; Kořen repa (flutter_czechmate/installer/windows → ../../../)
OutputDir=..\..\..
OutputBaseFilename=czechmate-{#MyAppOutputVersion}-windows-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
PrivilegesRequired=admin
UninstallDisplayIcon={app}\{#MyAppExeName}
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#MyAppSourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
