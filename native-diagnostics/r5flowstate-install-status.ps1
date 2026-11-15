param([string]$GameRoot = 'D:\R5Flowstate\Game')

$ErrorActionPreference = 'Stop'
$manifestPath = Join-Path $GameRoot '.r5f\manifests\client_1.0.23.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$entries = @{}
foreach ($entry in $manifest.files) { $entries[$entry.p] = $entry }
$seen = [Collections.Generic.HashSet[string]]::new()
$verifiedBytes = [long]0
$journalPath = Join-Path $GameRoot '.r5f\journal.tsv'
$journalAvailable = Test-Path -LiteralPath $journalPath
$journalLines = if ($journalAvailable) { Get-Content -LiteralPath $journalPath } else { @() }
foreach ($line in $journalLines) {
    $parts = $line -split "`t", 3
    if ($parts.Length -ne 3) { continue }
    $entry = $entries[$parts[2]]
    if ($null -eq $entry) { continue }
    $index = [int]$parts[1]
    $key = "$($parts[2])`t$index"
    if (-not $seen.Add($key)) { continue }
    if ($index -lt 0) {
        if ($parts[0] -eq $entry.h) { $verifiedBytes += [long]$entry.s }
        continue
    }
    if ($index -ge @($entry.ch).Count -or $parts[0] -ne $entry.ch[$index]) { continue }
    $chunkSize = [long]$entry.cs
    $verifiedBytes += [Math]::Min($chunkSize, [long]$entry.s - $index * $chunkSize)
}
$installState = Get-Content -LiteralPath (Join-Path $GameRoot 'INSTALL_STATE.json') -Raw | ConvertFrom-Json
[pscustomobject]@{
    observed_at = Get-Date -Format o
    game_root = $GameRoot
    client_catalog = $manifest.catalog_version
    journal_available = $journalAvailable
    journal_verified_client_bytes = if ($journalAvailable) { $verifiedBytes } else { $null }
    client_total_bytes = [long]$manifest.payload_bytes
    journal_client_percent = if ($journalAvailable) { [Math]::Round(100 * $verifiedBytes / $manifest.payload_bytes, 2) } else { $null }
    journal_objects = $seen.Count
    client_ready = $installState.client_ready
    server_ready = $installState.server_ready
    platform_ready = $installState.platform_ready
    incomplete = $installState.incomplete
    hd_enabled = $installState.hd_enabled
    hd_ready = $installState.hd_ready
    last_error = $installState.last_error
} | ConvertTo-Json
