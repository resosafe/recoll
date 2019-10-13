; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

#define MyAppName "Recoll"
#define MyAppVersion "1.26.0-pre4-20191011-2491388e"
#define MyAppPublisher "Recoll.org"
#define MyAppURL "http://www.recoll.org"
#define MyAppExeName "recoll.exe"

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{E9BC39EC-0E3D-4DDA-8DA0-FDB8ED16DC8D}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
;AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultGroupName={#MyAppName}
LicenseFile=C:\install\recoll\COPYING.txt
OutputDir=C:\Temp
OutputBaseFilename=recoll-setup-{#MyAppVersion}
SetupIconFile=C:\recoll\src\desktop\recoll.ico
Compression=lzma
SolidCompression=yes
;; Use either prv=adm and defaultdir={pf} or prv=lowest and defaultdir=userpf
;PrivilegesRequired=lowest
;DefaultDirName={userpf}\{#MyAppName}
PrivilegesRequired=admin
DefaultDirName={pf}\{#MyAppName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "C:\install\recoll\recoll.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\install\recoll\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

