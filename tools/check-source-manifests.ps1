#requires -Version 5.1
<#
  出包前自检：components.json 里所有 source.kind = "bundled" 的组件，
  其源码树必须与 tools\source-manifests\<id>.json 一致，且清单摘要必须
  等于 components.json 里登记的 sha256（安装时 bootstrap 就按这个值校验）。

  用法：
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\check-source-manifests.ps1
    powershell ... -File tools\check-source-manifests.ps1 -Only hermes

  退出码 0 = 全部通过；1 = 有组件对不上（出包脚本应中止）。
#>
param(
    [string[]]$Only = @()
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$manifestPath = Join-Path $repo 'tools\components.json'
if (-not (Test-Path -LiteralPath $manifestPath)) { throw "缺少 tools\components.json：$manifestPath" }
$doc = [IO.File]::ReadAllText($manifestPath, [Text.Encoding]::UTF8) | ConvertFrom-Json
$manifestTool = Join-Path $repo 'tools\source-manifest.ps1'
$bad = New-Object Collections.ArrayList
$checked = 0
foreach ($c in @($doc.components)) {
    if ($null -eq $c.source -or "$($c.source.kind)" -ne 'bundled') { continue }
    $id = "$($c.id)"
    if ($Only.Count -gt 0 -and $Only -notcontains $id) { continue }
    $root = Join-Path $repo ("$($c.source.url)".Replace('/', '\'))
    $mf = Join-Path $repo ("$($c.source.manifest)")
    if (-not (Test-Path -LiteralPath $root)) { $bad.Add("$id：源码目录不存在 $root"); continue }
    if (-not (Test-Path -LiteralPath $mf)) { $bad.Add("$id：清单不存在 $mf"); continue }
    $saved = [IO.File]::ReadAllText($mf, [Text.Encoding]::UTF8) | ConvertFrom-Json
    if ([string]$saved.digest -ne [string]$c.source.sha256) {
        $bad.Add("$id：清单摘要 $($saved.digest) 与 components.json 登记的 $($c.source.sha256) 不一致")
        continue
    }
    foreach($step in @($c.install.steps)){if($step.kind -in @('copy_tree','verify_tree') -and $step.manifest -and [string]$step.sha256 -ne [string]$c.source.sha256){$bad.Add("$id : install tree digest differs from source digest")}}
    $exclude = ''
    foreach ($st in @($c.install.steps)) {
        if ("$($st.kind)" -eq 'copy_tree' -and @($st.exclude).Count -gt 0) { $exclude = (@($st.exclude) -join ';'); break }
    }
    $argv = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $manifestTool, '-Root', $root, '-Verify', '-Manifest', $mf)
    if ($exclude) { $argv += @('-Exclude', $exclude) }
    $out = & powershell.exe @argv 2>&1
    if ($LASTEXITCODE -ne 0) { $bad.Add(("$id：源码树校验失败 — " + (($out | Where-Object { $_ -match '不符|缺失|新增' } | Select-Object -First 1)))); continue }
    $got = (@($out | ForEach-Object { "$_".Trim() } | Where-Object { $_ -match '^[0-9a-f]{64}$' }) | Select-Object -Last 1)
    if ($got -ne [string]$c.source.sha256) { $bad.Add("$id：实算摘要 $got 与登记值不一致"); continue }
    $checked++
    Write-Host ("[OK] {0}  {1}  {2}" -f $id, $got, $root)
}
if ($bad.Count -gt 0) {
    foreach ($b in $bad) { Write-Host "[FAIL] $b" }
    Write-Host "[FAIL] $($bad.Count) 个 bundled 组件校验不通过（检查后重跑 source-manifest.ps1 -Create）"
    exit 1
}
Write-Host "[OK] $checked 个 bundled 组件源码树与清单一致"
exit 0
