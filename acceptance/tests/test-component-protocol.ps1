#requires -Version 5.1
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $repo 'tools\progress.ps1')
function Assert($Condition,[string]$Message) { if (-not $Condition) { throw $Message } }
$work=Join-Path $repo ('.tmp\protocol-'+[guid]::NewGuid().ToString('N'))
$dir=Join-Path $work 'logs\setup'
$components=@([pscustomobject]@{id='node';tier='required';size_bytes=20971520},[pscustomobject]@{id='qq-login';tier='manual';size_bytes=0})
Progress-Begin -Directory $dir -Components $components -Selection @('node')
Progress-Item node downloading -Done 0 -Speed 1000000 -Label '下载'
$before=$script:Supply.pct
Progress-Item node downloading -Done 2097152 -Speed 1000000 -Label '下载'
Progress-Total -Force
Assert ($script:Supply.pct -gt $before) 'fractional download did not advance weighted progress'
[IO.File]::WriteAllText((Join-Path $dir 'pause'),'pause')
try { Progress-Guard;throw 'pause was ignored' } catch { Assert ($_.Exception.Message -eq 'KAIROS_SETUP_PAUSED') 'wrong pause result' }
$checkpoint=[IO.File]::ReadAllText((Join-Path $dir 'progress.json'))|ConvertFrom-Json
Assert ($checkpoint.speed -eq 0 -and $checkpoint.eta -eq 0) 'paused metrics not reset'
Assert ($checkpoint.items[0].done -eq 2097152) 'pause lost downloaded bytes'
Assert ($checkpoint.items[1].state -eq 'manual' -and $checkpoint.items[1].total -eq 0) 'manual item acquired download metrics'
Remove-Item -LiteralPath (Join-Path $dir 'pause')
Progress-Begin -Directory $dir -Components $components -Selection @('node')
Assert ($script:Supply.items[0].done -eq 2097152) 'resume checkpoint reset partial bytes'
# Extract only the trusted source mirror functions, never run bootstrap initialization/install.
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $repo 'tools\bootstrap.ps1'),[ref]$tokens,[ref]$errors)
Assert ($errors.Count -eq 0) 'bootstrap syntax errors'
foreach($name in @('Initialize-SetupMirror','Get-SetupMirrors')) {
    $fn=$ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq $name},$true)
    . ([scriptblock]::Create($fn.Extent.Text))
}
function Emit([string]$Text) { Write-Host $Text }
$script:Base=$work;$script:CheckMode=$false;$script:BundledIds=@{hermes=$true}
[IO.File]::WriteAllText((Join-Path $dir 'next-mirror'),'node')
Initialize-SetupMirror node
Assert (((Get-SetupMirrors @('a','b','c')) -join ',') -eq 'b,c,a') 'mirror did not change'
Initialize-SetupMirror node
Assert (((Get-SetupMirrors @('a','b','c')) -join ',') -eq 'b,c,a') 'mirror choice was not retained'
[IO.File]::WriteAllText((Join-Path $dir 'next-mirror'),'node')
Initialize-SetupMirror node
Assert (((Get-SetupMirrors @('a','b','c')) -join ',') -eq 'c,a,b') 'second mirror change failed'
Initialize-SetupMirror hermes
Assert ($script:SupplyMirrorOffset -eq 0) 'bundled hermes entered mirrors'
$result=[ordered]@{fractional_pct_before=$before;fractional_pct_after=$checkpoint.pct;paused_bytes=$checkpoint.items[0].done;paused_speed=$checkpoint.speed;paused_eta=$checkpoint.eta;resume_retains_bytes=$true;manual_no_bytes=$true;mirror_change_persisted=$true;second_mirror_change=$true}
$out=Join-Path $repo 'evidence\component-supply\protocol.json'
New-Item -ItemType Directory -Force (Split-Path -Parent $out) | Out-Null
[IO.File]::WriteAllText($out,($result|ConvertTo-Json),(New-Object Text.UTF8Encoding($false)))
Write-Host 'OK: component protocol / pause checkpoint / mirror preferences'
