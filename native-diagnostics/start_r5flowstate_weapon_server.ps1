param([switch]$StopExisting)
$ErrorActionPreference='Stop'
$workspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$game='D:\R5Flowstate\Game'
$exe=Join-Path $game 'r5apex_ds.exe'
$source=Join-Path $workspace 'sources\r5flowstate-sdk\s21-r5sdk-s21-unify\game\server.dll'
$destination=Join-Path $game 'server.dll'
$artifact=Join-Path $workspace 'artifacts\r5flowstate-aim\weapon-training'
if(!(Test-Path -LiteralPath $exe)-or !(Test-Path -LiteralPath $source)){throw 'Offline server or completed build missing'}
$sourceHash=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
$running=@(Get-CimInstance Win32_Process -Filter "Name='r5apex_ds.exe'"|Where-Object ExecutablePath -EQ $exe)
if($running.Count-gt 0 -and !$StopExisting){throw 'Offline server is running; deliberate restart requires -StopExisting'}
$backup=Join-Path $artifact 'server-before-weapon-training.dll'
if(!(Test-Path -LiteralPath $backup)){Copy-Item -LiteralPath $destination -Destination $backup}
foreach($instance in $running){
    $process=Get-Process -Id $instance.ProcessId
    Stop-Process -Id $instance.ProcessId
    if(!$process.WaitForExit(15000)){throw 'Offline server exit timed out'}
}
Copy-Item -LiteralPath $source -Destination $destination
$installedHash=(Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
if($sourceHash-ne $installedHash){throw 'Installed server DLL does not match build'}
Copy-Item -LiteralPath $source -Destination (Join-Path $artifact 'server-weapon-training.dll')
Copy-Item -LiteralPath (Join-Path $workspace 'sources\r5flowstate-sdk\s21-r5sdk-s21-unify\game\server.pdb') -Destination (Join-Path $artifact 'server-weapon-training.pdb')
$arguments=@('-dedicated','-port','37015','-devsdk','-dev','-logfiles','+net_usesocketsforloopback','1','-offline',
    '+sv_onlineAuthEnable','0','+spire_host_visibility','0','+sv_cheats','1','+launchplaylist','survival_firingrange',
    '+map','mp_rr_canyonlands_staging_mu1','+sv_modPolicy','0')
$server=Start-Process -FilePath $exe -WorkingDirectory $game -ArgumentList $arguments -WindowStyle Hidden -PassThru
$report=[ordered]@{timestampUtc=[DateTimeOffset]::UtcNow.ToString('o');pid=$server.Id;executable=$exe;dllSha256=$installedHash;arguments=$arguments}
$report|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $artifact 'server-deployment.json') -Encoding utf8
$report|ConvertTo-Json -Depth 4
