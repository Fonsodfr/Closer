; Closer - Windows installer (Inno Setup 6)
; GitHub Actions builds it with:
;   ISCC.exe /DAppVersion=0.4.0 /DSourceDir=build\Closer_artefacts\Release\VST3 packaging\windows\closer.iss
;
; It installs the plugin in the standard VST3 folder that every DAW scans:
;   C:\Program Files\Common Files\VST3\Closer.vst3

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\build\Closer_artefacts\Release\VST3"
#endif

[Setup]
; Never change this AppId: Windows uses it to recognise updates of Closer
AppId={{0B8E5F18-8EF3-4949-AD2E-7B43D3809CE8}
AppName=Closer
AppVersion={#AppVersion}
AppVerName=Closer {#AppVersion}
AppPublisher=Rave Factory
AppPublisherURL=https://github.com/Fonsodfr/Closer
AppSupportURL=https://github.com/Fonsodfr/Closer
VersionInfoVersion={#AppVersion}
VersionInfoCompany=Rave Factory
VersionInfoDescription=Closer installer

; The uninstaller and this readme go here; the plugin itself goes to the VST3 folder
DefaultDirName={autopf}\Rave Factory\Closer
DisableDirPage=yes
DisableProgramGroupPage=yes
UninstallDisplayName=Closer (Rave Factory)

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
MinVersion=10.0

OutputDir=..\..\installer-output
OutputBaseFilename=Closer-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Messages]
english.FinishedLabelNoIcons=Closer is installed in the VST3 folder.%n%nOpen your DAW and rescan plugins if needed. It appears as "Closer" by "Rave Factory".
spanish.FinishedLabelNoIcons=Closer está instalado en la carpeta VST3.%n%nAbre tu DAW y, si hace falta, vuelve a escanear los plugins. Aparece como "Closer" de "Rave Factory".

[Files]
Source: "{#SourceDir}\Closer.vst3\*"; DestDir: "{commoncf64}\VST3\Closer.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\INSTALL-Windows.txt"; DestDir: "{app}"; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Closer.vst3"
