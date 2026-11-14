# 版本自检：确认 win32\src\version.h 里「数字」和「字符串」没走散。
#
#   数字：APP_VERSION_MAJOR / MINOR / PATCH / BUILD   → 资源 FILEVERSION、安装包
#   渠道：APP_VERSION_SUFFIX（正式版 ""，快照版 "-snapshot"）
#   开关：APP_SNAPSHOT（1 = 快照通道：独立状态目录/窗口类/卸载项，必须与后缀一致）
#   快照序号：APP_SNAPSHOT_N（V3-snapshot-2 里的 2，必须等于 APP_VERSION_BUILD）
#   字符串：APP_VERSION_PLAIN / APP_VERSION_TAG / APP_VERSION_STR / APP_VERSION / APP_VERSION_SHORT
#
# 命名规则：
#   正式版  V<major>.<minor>.<patch>                 例：V3.1.0
#   快照版  V<major>-snapshot-<n>                    例：V3-snapshot-2
#           第一个数字 = 现正式版最新大版本 + 1；n = 该快照通道里的第几版
#   安装包  Kairos-<APP_VERSION_SHORT>-setup.exe
#           正式版 3.1.0 / 快照版 3-snapshot-2
#
# 用法：powershell -NoProfile -ExecutionPolicy Bypass -File tools\check-version.ps1
# 通过：退出码 0，打印一行「[版本] OK V3-snapshot-2 (3.0.0.2)」。
# 走散：退出码 1，并列出哪一条对不上、应该改成什么。

#  文档与端口（E6）：
#   · docs/CHANGES.md 顶部最新版本标题、README.md、docs/项目总览.md 里的版本串，
#     必须包含 version.h 算出来的 APP_VERSION_TAG（快照版就是 V4-snapshot-1 这种）。
#   · tools/components.json 与 win32/src/her/her_status.cpp、her_actions.cpp 里
#     出现的关键端口，必须被 docs/deploy-qq.md 的端口表列全（子集断言，不做深入解析）。
#   · 改版本号时记得同步文档，否则构建会在自检这一步停下；
#     临时想跳过这两项可以加 -SkipDocs（构建脚本不传，默认严格）。

param(
    [string]$Header = "",
    [switch]$SkipDocs = $false
)

$ErrorActionPreference = 'Stop'
if (-not $Header) { $Header = Join-Path $PSScriptRoot '..\win32\src\version.h' }
if (-not (Test-Path -LiteralPath $Header)) {
    Write-Host "[版本] 找不到 version.h：$Header"
    exit 1
}

$text = Get-Content -LiteralPath $Header -Raw -Encoding UTF8

function Get-Num([string]$name) {
    $m = [regex]::Match($text, '(?m)^\s*#define\s+' + $name + '\s+(\d+)')
    if (-not $m.Success) { return $null }
    return $m.Groups[1].Value
}

function Get-Str([string]$name) {
    $m = [regex]::Match($text, '(?m)^\s*#define\s+' + $name + '\s+L?"([^"]*)"')
    if (-not $m.Success) { return $null }
    return $m.Groups[1].Value
}

$major = Get-Num 'APP_VERSION_MAJOR'
$minor = Get-Num 'APP_VERSION_MINOR'
$patch = Get-Num 'APP_VERSION_PATCH'
$build = Get-Num 'APP_VERSION_BUILD'
if ($null -eq $major -or $null -eq $minor -or $null -eq $patch -or $null -eq $build) {
    Write-Host "[版本] version.h 缺数字段（MAJOR/MINOR/PATCH/BUILD 四个都要有）"
    exit 1
}

$plain = "$major.$minor.$patch"

# 渠道后缀：正式版是空串，快照版是 "-snapshot"。
$suffix = Get-Str 'APP_VERSION_SUFFIX'
if ($null -eq $suffix) {
    Write-Host "[版本] version.h 缺 APP_VERSION_SUFFIX（正式版写 `"`"，快照版写 `"-snapshot`"）"
    exit 1
}
$snap = Get-Num 'APP_SNAPSHOT'
if ($null -eq $snap) {
    Write-Host "[版本] version.h 缺 APP_SNAPSHOT（正式版 0，快照版 1）"
    exit 1
}
$snapN = Get-Num 'APP_SNAPSHOT_N'
if ($null -eq $snapN) { $snapN = '0' }
if (($suffix -eq "-snapshot" -and $snap -ne "1") -or ($suffix -ne "-snapshot" -and $snap -ne "0") -or $suffix -notin @("","-snapshot","-beta")) {
    Write-Host "[版本] APP_SNAPSHOT=$snap 与 APP_VERSION_SUFFIX=`"$suffix`" 不一致："
    Write-Host "        快照版要 APP_SNAPSHOT 1 + 后缀 -snapshot；正式版要 0 + 空串。"
    exit 1
}

if ($snap -eq "1") {
    # 快照版：V<major>-snapshot-<n>；文件版本第四段 = n；短串 = <major>-snapshot-<n>
    if ($suffix -ne "-snapshot") {
        Write-Host "[版本] 快照版的 APP_VERSION_SUFFIX 必须是 `"-snapshot`"，现在是 `"$suffix`"。"
        exit 1
    }
    if ([int]$snapN -lt 1) {
        Write-Host "[版本] 快照版要写 APP_SNAPSHOT_N（V3-snapshot-2 里的 2），现在是 $snapN。"
        exit 1
    }
    if ($build -ne $snapN) {
        Write-Host "[版本] APP_VERSION_BUILD=$build 与 APP_SNAPSHOT_N=$snapN 必须一样"
        Write-Host "        （文件版本第四段就是快照序号，3,0,0,2 ←→ V3-snapshot-2）。"
        exit 1
    }
    $tag   = "V$plain-snapshot-$snapN"
    $short = "$plain-snapshot-$snapN"
    $fullS = "$major.$minor.$patch.$snapN"
} elseif ($suffix -eq "-beta") {
    if ($snapN -ne "0" -or [int]$build -lt 1) { Write-Host '[version] beta requires SNAPSHOT_N=0 and BUILD>=1'; exit 1 }
    $tag = "V$plain-beta.$build"
    $short = "$plain-beta.$build"
    $fullS = "$plain.$build"
} else {
    # 正式版：V<major>.<minor>.<patch>；第四段仍是 BUILD（历史习惯，通常 0）
    if ($snapN -ne "0") {
        Write-Host "[版本] 正式版不该写 APP_SNAPSHOT_N（现在是 $snapN，正式版为 0）。"
        exit 1
    }
    $tag   = "V$plain"
    $short = $plain
    $fullS = "$plain.$build"
}

$bad = @()

function Want([string]$macro, [string]$expect) {
    $got = Get-Str $macro
    if ($null -eq $got) { $script:bad += "$macro 缺失，应为 `"$expect`""; return }
    if ($got -ne $expect) { $script:bad += "$macro = `"$got`"，应为 `"$expect`"" }
}

Want 'APP_VERSION_PLAIN'   $plain
Want 'APP_VERSION_TAG'     $tag
Want 'APP_VERSION_STR'     $fullS
Want 'APP_VERSION_SHORT'   $short
Want 'APP_VERSION'         $tag
Want 'APP_VERSION_PLAIN_W' $plain
Want 'APP_VERSION_FULL_W'  $fullS
Want 'APP_VERSION_SHORT_W' $short

if ($bad.Count -gt 0) {
    Write-Host "[版本] 数字是 $tag（$fullS），但下面的字符串对不上："
    foreach ($b in $bad) { Write-Host "        · $b" }
    Write-Host "       改 $Header，或者直接照上面「应为」抄。"
    exit 1
}

# ---------------------------------------------------------------- 文档与端口（E6）

$fail = @()
$hint = @()

function Read-Repo([string]$rel) {
    $p = Join-Path $PSScriptRoot ('..\' + $rel)
    if (-not (Test-Path -LiteralPath $p)) {
        $script:fail += "缺文件 $rel（版本串/端口表本该写在这里）"
        return $null
    }
    return (Get-Content -LiteralPath $p -Raw -Encoding UTF8)
}

if (-not $SkipDocs) {
    # (a) 文档里的版本串必须是 version.h 算出来的 APP_VERSION_TAG
    $changes = Read-Repo 'CHANGELOG.md'
    if ($null -ne $changes) {
        $mh = [regex]::Match($changes, '(?m)^##[ \t]+([^\r\n]+)')
        if (-not $mh.Success) {
            $script:fail += "docs\CHANGES.md 顶部找不到「## 版本」标题（期望最新标题里含 $tag）"
        } elseif ($mh.Groups[1].Value.IndexOf($tag, [StringComparison]::OrdinalIgnoreCase) -lt 0) {
            $script:fail += "docs\CHANGES.md 顶部最新版本标题是「## $($mh.Groups[1].Value.Trim())」，里面没有 $tag（version.h 是 $tag）"
        }
    }

    foreach ($rel in @('README.md')) {
        $c = Read-Repo $rel
        if ($null -ne $c -and $c.IndexOf($tag, [StringComparison]::OrdinalIgnoreCase) -lt 0) {
            $script:fail += "$rel 里找不到版本串 $tag（version.h 是 $tag；改版本号时这份文档也要跟上）"
        }
    }

    # (b) 端口子集断言：代码/组件里出现的关键端口 ⊆ docs\deploy-qq.md 端口表
    $doc = Read-Repo 'docs\deploy-qq.md'
    if ($null -ne $doc) {
        $docPorts = @{}
        foreach ($m in [regex]::Matches($doc, '(?m)^\|[ \t]*`(\d{4,5})`')) {
            $docPorts[$m.Groups[1].Value] = $true
        }
        if ($docPorts.Count -eq 0) {
            $script:fail += 'docs\deploy-qq.md「## 3. 端口」表里一个端口都没读到（表行形如 | `6199` | AstrBot | …）'
        } else {
            # 旧版回退端口：代码里还在用，文档的端口表暂时没登记。
            # docs\ 不在本次改动范围，所以先放行并打提示；文档补上 6099/5099 那两行后就能删掉这张白名单。
            $legacyOk = @{ '5099' = 'SnowLuma 旧版回退'; '6099' = 'NapCat WebUI' }
            $hitLegacy = @{}
            foreach ($rel in @('tools\components.json', 'win32\src\her\her_status.cpp', 'win32\src\her\her_actions.cpp')) {
                $c = Read-Repo $rel
                if ($null -eq $c) { continue }
                $found = @{}
                # 只认「端口上下文」里的四/五位数：URL 的 :port、xxx_port=、--port、
                # Listening(…)、"port":、端口 25109 这几种写法，别把 4000 毫秒、chcp 65001 当端口
                $portPatterns = @(
                    '(?:127\.0\.0\.1|localhost):(\d{4,5})',
                    '(?i)port\s*=\s*(\d{4,5})',
                    '(?i)"port"\s*:\s*(\d{4,5})',
                    '(?i)--port\s+(\d{4,5})',
                    '(?i)Listening\(\s*(\d{4,5})\s*\)',
                    '(?i)端口\s*[:：]?\s*`(\d{4,5})`'
                )
                foreach ($pat in $portPatterns) {
                    foreach ($m in [regex]::Matches($c, $pat)) { $found[$m.Groups[1].Value] = $true }
                }
                foreach ($p in $found.Keys) {
                    if ($docPorts.ContainsKey($p)) { continue }
                    if ($legacyOk.ContainsKey($p)) { $hitLegacy[$p] = $true; continue }
                    $script:fail += "$rel 里用了端口 $p，但 docs\deploy-qq.md 的端口表里没有（期望：文档补这一行，或代码改用表里已有的端口）"
                }
            }
            if ($hitLegacy.Count -gt 0) {
                $script:hint += ("代码里还有旧版回退端口 " + (($hitLegacy.Keys | Sort-Object) -join '/') + "，文档端口表未登记，暂按白名单放行")
            }
        }
    }
}

foreach ($h in $hint) { Write-Host "[版本] 提示：$h" }

if ($fail.Count -gt 0) {
    Write-Host "[版本] 版本号是 $tag，但下面这些没跟上："
    foreach ($f in $fail) { Write-Host "        · $f" }
    Write-Host "       改完再跑一次；只是临时想跳过文档/端口两项，可以加 -SkipDocs。"
    exit 1
}

Write-Host "[版本] OK $tag ($fullS)"
exit 0
