; 时序之子 v2 安装脚本（Inno Setup 6）
; 编译：installer\build-installer.bat  →  dist\Kairos-<版本>-setup.exe
; 版本号来自 win32\src\version.h（build-installer.bat 每次重新生成 version_auto.iss）
#include "version_auto.iss"
#include "brand_auto.iss"
#define AppExe BrandExe

#ifndef AppExeSource
  #define AppExeSource "..\bin\" + AppExe
#endif

; ── 可选：把 QQ 接入组件的官方原版包一起打进安装包 ──────────────────────────────
; 默认不内置（BundleLogin=0），装完由 bootstrap.ps1 联网下载，和以前一样。
; 想出内置版就 build-installer.bat -BundleLogin；那份片段由
; installer\prepare-login.ps1 生成（会先从 tools\components.json 抄校验值，
; 再确认本地那份 zip 的 SHA256，对不上就拒绝出包）。
; 为什么不做成默认：上游许可协议第 5.4 条把「并入第三方安装包」列为须事先取得
; 书面授权的行为，所以这条通道必须由出包的人逐个明确开启，并自负授权责任。
#if FileExists(AddBackslash(SourcePath) + "login_auto.iss")
  #include "login_auto.iss"
#endif
#ifndef BundleLogin
  #define BundleLogin 0
#endif
; 内置版的产物换个文件名：不然它和官方正式包同名，跑一次 -BundleLogin 就会
; 把 dist\ 里那份要发出去的 88 MB 正式包悄悄覆盖掉（只差几十 MB，很难看出来）。
#if BundleLogin
  #define OutSuffix "-login"
#else
  #define OutSuffix ""
#endif

#define AppTitle BrandShortcut
#define AppDir BrandInstallDir
#define AppUninstKey "Kairos_is1"
#define AppStateSub ""

[Setup]
AppId={#BrandAppId}
AppName={#AppTitle}
; 显示版本用完整串（V3-snapshot-2）：「应用和功能」那一列直接读它
AppVersion={#AppVerTag}
AppVerName={#AppTitle} {#AppVerTag}
; 文件版本必须是四段数字（资源管理器 → 属性 → 详细信息）
VersionInfoVersion={#AppVer}
VersionInfoProductVersion={#AppVer}
VersionInfoProductTextVersion={#AppVerTag}
VersionInfoCompany={#BrandName}
VersionInfoCopyright={#BrandName}
VersionInfoDescription={#AppTitle} 安装程序
AppPublisher={#BrandName}
DefaultDirName={autopf}\{#AppDir}
DefaultGroupName={#AppTitle}
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
OutputDir=..\dist
OutputBaseFilename={#BrandId}-{#AppVerShort}-setup{#OutSuffix}
SetupIconFile=..\win32\res\app.ico
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppTitle}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
LicenseFile=..\docs\THIRD-PARTY.md
ShowLanguageDialog=no
AllowNoIcons=yes
CreateUninstallRegKey=no

[Languages]
Name: "zh"; MessagesFile: "languages\ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "快捷方式："; Flags: unchecked

[Dirs]
; 密钥目录：和工作区同级（默认 <Base>\secrets），在 {app} 之外，卸载不删

[Files]
Source: "..\bin\WebView2Loader.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\win32\vendor\webview2\LICENSE.txt"; DestDir: "{app}\docs\webview2"; Flags: ignoreversion
Source: "..\win32\vendor\webview2\NOTICE.txt"; DestDir: "{app}\docs\webview2"; Flags: ignoreversion
Source: "{#AppExeSource}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\art\styles\*.webp"; DestDir: "{app}\art\styles"; Flags: ignoreversion
Source: "..\art\character\*.webp"; DestDir: "{app}\art\character"; Flags: ignoreversion
Source: "..\art\SOURCES.md"; DestDir: "{app}\art"; Flags: ignoreversion
; 只装面向用户的文档：开发期的计划稿、验收记录、代码审查留在仓库里，不随包发（里面写着
; 上游组件的名字与历史口径，装到用户机器上没有意义）。
Source: "..\docs\*"; DestDir: "{app}\docs"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "plan-*.md,ACCEPTANCE.md,code-review-*.md,heart\*"
Source: "..\plugin\astrbot_plugin_bili_bridge\*"; DestDir: "{app}\plugin\astrbot_plugin_bili_bridge"; Flags: ignoreversion recursesubdirs createallsubdirs
; 潮信 TideLetter 随 sidecar 整树内置（vendor\ws、vendor\silk-wasm、native\build），装完离线可启动；node_modules 只用于开发，不进包。
; 只排除开发期产生的运行时状态与自测目录，别把别人的 state/logs 装进新机器。
Source: "..\sidecar\*"; DestDir: "{app}\sidecar"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "hermes\node_modules\*,hermes\state\*,hermes\.selftest\*,hermes\logs\*,hermes\dist\*,hermes\*.log"
; 安装 Skill（冥零自己的那份）：装在 {app}\skill 下，工作台第一次启动时铺到
; %USERPROFILE%\.agents\skills\kairos-install，并写下 tools\skill-home.txt / skill-version.json。
; 没有它，Agent 就只会照大肥鱼那份说明书干活。
Source: "..\skill\kairos-install\*"; DestDir: "{app}\skill\kairos-install"; Flags: ignoreversion recursesubdirs createallsubdirs
; 人设注入用的清单和人设正文（导入冥零人设要读 plugin\mingling_skill\mingling_skill.json）
Source: "..\plugin\mingling_skill\*"; DestDir: "{app}\plugin\mingling_skill"; Flags: ignoreversion recursesubdirs createallsubdirs
; 工作台跑活要用的脚本：自设定（self-setup）、一键自动配置、体检、备份、掉线提醒、人设导入、桌宠
Source: "..\tools\*.ps1"; DestDir: "{app}\tools"; Flags: ignoreversion
Source: "..\tools\*.py"; DestDir: "{app}\tools"; Flags: ignoreversion
Source: "..\tools\*.cmd"; DestDir: "{app}\tools"; Flags: ignoreversion
; tools\*.txt 是运行时生成的（skill-home.txt 等），源码树里可能没有；没有就跳过，不能因此拒绝出包
Source: "..\tools\*.txt"; DestDir: "{app}\tools"; Flags: ignoreversion skipifsourcedoesntexist; Excludes: "skill-home.txt,pet-name-mine.txt,pet-persona-mine.txt"
; 组件清单：bootstrap.ps1 按 components.json 决定装什么、怎么验。上面只带 *.ps1/*.py/*.cmd/*.txt，
; 漏了这份 json，「一键自动配置」就不知道该装哪几样。
Source: "..\win32\src\brand.h"; DestDir: "{app}\win32\src"; Flags: ignoreversion
Source: "..\tools\bundled-assets.json"; DestDir: "{app}\tools"; Flags: ignoreversion
Source: "..\pack\components\*.zip"; DestDir: "{app}\pack\components"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\tools\components.json"; DestDir: "{app}\tools"; Flags: ignoreversion
Source: "..\tools\astrbot-requirements.txt"; DestDir: "{app}\tools"; Flags: ignoreversion
Source: "..\tools\edge-tts-requirements.txt"; DestDir: "{app}\tools"; Flags: ignoreversion
Source: "..\tools\source-manifests\*.json"; DestDir: "{app}\tools\source-manifests"; Flags: ignoreversion
; 掉线提醒用的那个 pythonw（工作台按 tools\python\pythonw.exe 找）；全量包不带这个目录时跳过
Source: "..\tools\python\*"; DestDir: "{app}\tools\python"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
; 配置包：自设定读 pack\mingling-pack\pack.json（tools\self-setup.ps1 按 <根>\pack\ 找）
Source: "..\pack\mingling-pack\*"; DestDir: "{app}\pack\mingling-pack"; Flags: ignoreversion recursesubdirs createallsubdirs
; 全依赖哈希锁里唯一没有 PyPI wheel 的 aiocqhttp，由固定 sdist（锁里有源码哈希）构建为 wheel，安装时只读这个本地经校验文件。
Source: "..\pack\mingling-vendor\*"; DestDir: "{app}\pack\mingling-vendor"; Flags: ignoreversion recursesubdirs createallsubdirs
; 密钥目录里放一份说明；已经有的不覆盖（用户的密钥在同一个文件夹里）

; ── 内置的 QQ 接入组件原版包（只在 -BundleLogin 出包时有） ────────────────────
; 落到 {app}\pack\login\ 之后，装完的 bootstrap.ps1 -Check / 一键自动配置会先看
; 这里有没有校验通过的那份，有就直接解压，不再联网下载（tools\bootstrap.ps1 的
; Find-BundledPackage，按目录 pack\<组件id> 找）。文件是上游发布的原版 zip，
; 一个字节都没改，只落了中性文件名。
#if BundleLogin
Source: "{#LoginZip}"; DestDir: "{app}\pack\login"; Flags: ignoreversion
#endif

[Icons]
Name: "{group}\{#AppTitle}"; Filename: "{app}\{#AppExe}"
Name: "{group}\使用说明"; Filename: "{app}\docs\使用说明.md"
Name: "{group}\B 站部署教程"; Filename: "{app}\docs\deploy-bili.md"
Name: "{group}\QQ 机器人部署教程"; Filename: "{app}\docs\deploy-qq.md"
Name: "{group}\卸载 {#AppTitle}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppTitle}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\tools\bootstrap.ps1"" -Base ""{code:SetupWorkspace}"" -Workspace ""{code:SetupWorkspace}"" -Check"; Flags: runhidden waituntilterminated
Filename: "{app}\{#AppExe}"; Description: "启动 {#AppTitle}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: dirifempty; Name: "{app}"

[Registry]
Root: HKCU; Subkey: "{#BrandInstallerKey}"; ValueType: string; ValueName: "InstallLocation"; ValueData: "{app}"; Flags: uninsdeletekey
Root: HKCU; Subkey: "{#BrandUninstallKey}"; ValueType: string; ValueName: "DisplayName"; ValueData: "{#AppTitle}"; Flags: uninsdeletekey
Root: HKCU; Subkey: "{#BrandUninstallKey}"; ValueType: string; ValueName: "DisplayVersion"; ValueData: "{#AppVerTag}"
Root: HKCU; Subkey: "{#BrandUninstallKey}"; ValueType: string; ValueName: "Publisher"; ValueData: "{#BrandName}"
Root: HKCU; Subkey: "{#BrandUninstallKey}"; ValueType: string; ValueName: "InstallLocation"; ValueData: "{app}"
Root: HKCU; Subkey: "{#BrandUninstallKey}"; ValueType: string; ValueName: "DisplayIcon"; ValueData: "{app}\{#AppExe}"
Root: HKCU; Subkey: "{#BrandUninstallKey}"; ValueType: string; ValueName: "UninstallString"; ValueData: "{uninstallexe}"

[Code]
var WsPage: TInputDirWizardPage;
procedure InitializeWizard;
begin
  WsPage := CreateInputDirPage(wpSelectDir, '选择工作区', '产物、组件和日志放在这里。',
    '默认使用文档中的时序之子；请选择独立的新目录。', False, '');
  WsPage.Add('');
  WsPage.Values[0] := ExpandConstant('{param:WORKSPACE|{userdocs}\{#BrandWorkspace}}');
end;
function SetupWorkspace(Param: String): String;
begin
  Result := WsPage.Values[0];
  if Result = '' then Result := ExpandConstant('{userdocs}\{#BrandWorkspace}');
end;
function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = WsPage.ID then
    Result := (WsPage.Values[0] <> '') and ForceDirectories(WsPage.Values[0]);
end;
procedure CurStepChanged(CurStep: TSetupStep);
var stateDir, cfg, ws, js: String;
begin
  if CurStep <> ssPostInstall then Exit;
  stateDir := GetEnv('KAIROS_STATE_DIR');
  if stateDir = '' then stateDir := ExpandConstant('{userappdata}\{#BrandState}');
  cfg := stateDir + '\settings.json';
  if FileExists(cfg) then Exit;
  ws := WsPage.Values[0];
  if ws = '' then ws := ExpandConstant('{userdocs}\{#BrandWorkspace}');
  ForceDirectories(stateDir);
  StringChangeEx(ws, '\', '/', True);
  StringChangeEx(ws, '"', '', True);
  js := '{"workspace":"' + ws + '","base":"' + ws + '"}';
  SaveStringToFile(cfg, js, False);
end;
