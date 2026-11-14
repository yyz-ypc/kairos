#requires -Version 5.1
# Live, sanitized checkpoint protocol. No URLs, command lines or credentials are persisted.
$script:Supply = $null
$script:SupplyLastWrite = [datetime]::MinValue
function Progress-Safe([string]$Text) {
    # Labels are fixed by the caller; arbitrary installer errors never enter the checkpoint.
    $safe=[regex]::Replace($Text, '[\r\n|]', ' ')
    return $safe.Substring(0,[math]::Min(240,$safe.Length))
}
function Progress-Begin {
    param([string]$Directory, [object[]]$Components, [string[]]$Selection)
    $script:SupplyDir = $Directory
    New-Item -ItemType Directory -Force -Path $Directory | Out-Null
    $old = $null
    try { $old = [IO.File]::ReadAllText((Join-Path $Directory 'progress.json')) | ConvertFrom-Json } catch {}
    # Only cancellation clears a queue. A single-component repair must retain other
    # interrupted IDs, without implicitly running those paused optional components.
    $merged=@($Selection)
    foreach ($id in @($old.selection)) {
        $previous=@($old.items | Where-Object { $_.id -eq $id } | Select-Object -First 1)
        if ($previous.Count -and $previous[0].state -notin @('done','manual','skipped') -and
            $merged -notcontains $id -and @($Components | Where-Object { $_.id -eq $id -and $_.tier -ne 'manual' }).Count) { $merged+= [string]$id }
    }
    $Selection=$merged
    $items = @()
    foreach ($c in $Components) {
        $prev = @($old.items | Where-Object { $_.id -eq $c.id } | Select-Object -First 1)
        $state = 'pending'; $done = 0L
        if ($prev.Count -and $prev[0].state -in @('pending','checking','downloading','verifying','installing') -and [long]$prev[0].total -eq [long]$c.size_bytes) { $done=[long]$prev[0].done }
        if ($prev.Count -and $prev[0].state -eq 'done') { $state = 'done'; $done = [long]$prev[0].done }
        if ($c.tier -eq 'manual') { $state = 'manual'; $done = 0L }
        $size = [long]$c.size_bytes
        if ($c.tier -eq 'manual') { $size = 0L }
        $label=if ($state -eq 'done') { '已在位' } elseif ($state -eq 'manual') { '请到设置手动完成' } else { '等待' }
        $items += [ordered]@{ id=[string]$c.id; state=$state; done=$done; total=$size; speed=0; label=$label; message=''; mirror=''; tier=[string]$c.tier }
    }
    $script:Supply = [ordered]@{ schema=2; run_id=[guid]::NewGuid().ToString('N'); state='checking'; phase='检查'; pct=0; done=0L; total=0L; speed=0; eta=0; selection=@($Selection); items=@($items); updated_at=''; started_at=[datetime]::UtcNow.ToString('o') }
    Progress-Total -Force
}
function Progress-Total {
    param([string]$State='', [string]$Phase='', [switch]$Force)
    if (-not $script:Supply) { return }
    if ($State) { $script:Supply.state=$State }
    if ($Phase) { $script:Supply.phase=Progress-Safe $Phase }
    $active=@($script:Supply.items | Where-Object { $script:Supply.selection -contains $_.id -and $_.tier -ne 'manual' })
    $weights=@{ pending=0; checking=.03; downloading=.12; installing=.75; verifying=.92; done=1; skipped=1; failed=0; elevation=0 }
    $sum=0.0; $done=0L; $total=0L; $speed=0.0
    foreach ($item in $active) {
        $w=$weights[$item.state]; if ($null -eq $w) { $w=0 }
        if ($item.state -eq 'downloading' -and $item.total -gt 0) { $w=.12+.6*[math]::Min(1.0,$item.done/[double]$item.total) }
        if ($item.state -eq 'verifying' -and $item.label -eq '校验下载文件') { $w=.73 }
        $sum+=$w
        if ($item.total -gt 0) { $done += [math]::Min($item.total,$item.done); $total += $item.total; $speed += $item.speed }
    }
    $script:Supply.pct=[int][math]::Floor(100*$sum/[math]::Max(1,$active.Count))
    $script:Supply.done=$done; $script:Supply.total=$total; $script:Supply.speed=[long]$speed
    $script:Supply.eta=$(if ($speed -gt 0) { [long][math]::Ceiling(($total-$done)/$speed) } else { 0 })
    if (-not $Force -and ([datetime]::UtcNow-$script:SupplyLastWrite).TotalSeconds -lt 2) { return }
    $script:Supply.updated_at=[datetime]::UtcNow.ToString('o')
    $path=Join-Path $script:SupplyDir 'progress.json'; $tmp=$path+'.tmp'
    [IO.File]::WriteAllText($tmp,($script:Supply | ConvertTo-Json -Depth 8),(New-Object Text.UTF8Encoding($false)))
    $disk=[IO.File]::Open($tmp,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
    try { $disk.Flush($true) } finally { $disk.Dispose() }
    Move-Item -LiteralPath $tmp -Destination $path -Force
    $script:SupplyLastWrite=[datetime]::UtcNow
    Write-Host ('##TOTAL {0}|{1}|{2}|{3}|{4}|{5}' -f $script:Supply.pct,$done,$total,[long]$speed,$script:Supply.eta,$script:Supply.phase)
}
function Progress-Item {
    param([string]$Id,[string]$State,[long]$Done=-1,[long]$Total=-1,[long]$Speed=0,[string]$Label='', [string]$Mirror='')
    if (-not $script:Supply) { return }
    $item=$script:Supply.items | Where-Object { $_.id -eq $Id } | Select-Object -First 1
    if (-not $item) { return }
    $changed=$item.state -ne $State
    $item.state=$State; $item.speed=$Speed
    if ($Done -ge 0) { $item.done=$Done }
    # Only manifest-sized download packages expose byte metrics.
    if ($item.total -le 0) { $item.done=0L; $item.speed=0L }
    if ($State -eq 'done') { $item.done=$item.total }
    if ($Label) { $item.label=Progress-Safe $Label }
    if ($Mirror) { $item.mirror=([uri]('https://'+$Mirror)).Host }
    if ($changed) { Write-Host ('##EVENT {0}|{1}|{2}' -f $Id,$State,$item.label) }
    Write-Host ('##ITEM {0}|{1}|{2}|{3}|{4}|{5}' -f $Id,$State,$item.done,$item.total,$item.speed,$item.label)
    $runState=if ($State -in @('checking','downloading','installing','verifying')) { $State } else { 'installing' }
    Progress-Total -State $runState -Phase $item.label -Force:$changed
}
function Progress-Guard {
    param([switch]$NoHeartbeat)
    if (-not $script:Supply) { return }
    if (Test-Path -LiteralPath (Join-Path $script:SupplyDir 'pause')) {
        foreach ($item in $script:Supply.items) {
            if ($item.state -in @('checking','downloading','verifying','installing')) { $item.speed=0L; $item.label='已暂停，下次继续' }
        }
        Progress-Total -State paused -Phase '已暂停，下次继续' -Force
        throw 'KAIROS_SETUP_PAUSED'
    }
    if (([datetime]::UtcNow-[datetime]$script:Supply.started_at).TotalMinutes -ge 90) { throw '安装超过 90 分钟，请重试' }
    if ($script:SupplyItemStart -and ([datetime]::UtcNow-$script:SupplyItemStart).TotalMinutes -ge 30) { throw '当前组件超过 30 分钟，请重试' }
    if (-not $NoHeartbeat) { Progress-Total }
}
