#define OBSDIR "{commonpf}\obs-studio"

[Setup]
AppName=TWICH Tip Alert
AppVersion=1.0.0
DefaultDirName={#OBSDIR}
DisableDirPage=yes
DisableProgramGroupPage=yes
CreateAppDir=no
Uninstallable=yes
OutputBaseFilename=TWICH_Tip_Alert_Setup
Compression=lzma
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
AppMutex=Global\obs-studio-instance,obs-studio-instance

[Files]
Source: "..\build\Release\twich_tip_alert.dll"; DestDir: "{#OBSDIR}\obs-plugins\64bit"; Flags: ignoreversion
Source: "..\build\Release\tdjson.dll";          DestDir: "{#OBSDIR}\obs-plugins\64bit"; Flags: ignoreversion
Source: "..\dll\libcrypto-3-x64.dll";          DestDir: "{#OBSDIR}\obs-plugins\64bit"; Flags: ignoreversion
Source: "..\dll\libssl-3-x64.dll";             DestDir: "{#OBSDIR}\obs-plugins\64bit"; Flags: ignoreversion
Source: "..\dll\zlib1.dll";                    DestDir: "{#OBSDIR}\obs-plugins\64bit"; Flags: ignoreversion

[Messages]
SetupAppRunningError=OBS Studio is currently running.\n\nPlease close OBS Studio completely and try again.
UninstallAppRunningError=OBS Studio is currently running.\n\nPlease close OBS Studio completely and try again.
