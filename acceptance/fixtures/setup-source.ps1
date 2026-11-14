# Source-checkout-only test adapter. installer [Files] never includes acceptance/.
function New-SetupTestManifest([string]$Origin,[string]$Destination) {
    $json=Invoke-RestMethod -Uri ($Origin+'/components.json') -UseBasicParsing
    $dir=Join-Path $Destination 'logs\setup';New-Item -ItemType Directory -Force -Path $dir | Out-Null
    $file=Join-Path $dir 'test-components.json'
    [IO.File]::WriteAllText($file,($json | ConvertTo-Json -Depth 32),(New-Object Text.UTF8Encoding($false)))
    return $file
}
