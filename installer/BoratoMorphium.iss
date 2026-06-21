; Inno Setup script for Borato Morphium (VST3 + Standalone, Windows x64).
;
; Build the plugin first (Release), then compile this script with Inno Setup:
;   "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\BoratoMorphium.iss
; The installer is written to installer\Output\.

#define MyAppName "Borato Morphium"
; CI overrides the version with: ISCC.exe /DMyAppVersion=x.y.z (read from the
; CMake project version). The default below is the local fallback.
#ifndef MyAppVersion
  #define MyAppVersion "0.4.0"
#endif
#define MyAppPublisher "Borato Company"
#define MyAppURL "https://borato.audio"
#define MyAppExeName "Borato Morphium.exe"

; Built artefacts, relative to this script (installer/).
#define BuildDir "..\build\BoratoMorphium_artefacts\Release"

[Setup]
; A stable, unique id for this product (keep constant across versions).
AppId={{8B6F1E2A-1C9D-4E3B-9A77-BORATOMORPHIUM}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
DefaultDirName={commonpf64}\{#MyAppPublisher}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=Output
OutputBaseFilename=BoratoMorphium-{#MyAppVersion}-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; 64-bit only.
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Writing to Program Files / Common Files needs elevation.
PrivilegesRequired=admin

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[Types]
Name: "full";   Description: "Full installation (VST3 + Standalone)"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3";       Description: "VST3 plugin";            Types: full custom
Name: "standalone"; Description: "Standalone application"; Types: full custom

[Files]
; VST3 is a bundle (folder). Install it to the standard system VST3 location.
Source: "{#BuildDir}\VST3\Borato Morphium.vst3\*"; \
    DestDir: "{commoncf64}\VST3\Borato Morphium.vst3"; \
    Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3

; Standalone executable.
Source: "{#BuildDir}\Standalone\Borato Morphium.exe"; \
    DestDir: "{app}"; Flags: ignoreversion; Components: standalone

[Icons]
Name: "{group}\{#MyAppName}";            Filename: "{app}\{#MyAppExeName}"; Components: standalone
Name: "{group}\Uninstall {#MyAppName}";  Filename: "{uninstallexe}"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; \
    Flags: nowait postinstall skipifsilent; Components: standalone
