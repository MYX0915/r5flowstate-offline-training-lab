param([switch]$StopExisting,[string]$ShaderDumpDirectory='',[switch]$TraceTrainingInput)
$ErrorActionPreference='Stop'
$workspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$game='D:\R5Flowstate\Game'
$executable=Join-Path $game 'r5apex.exe'
$expectedExe='3F44BF9AEC8A18E7FA578B8A4927D8D27BBA5AB8980119BE9935137ADCD04CB5'
$exeHash=(Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash
if($exeHash-ne $expectedExe){throw 'Independent R5 executable identity changed; deployment stopped.'}
$running=@(Get-CimInstance Win32_Process -Filter "Name='r5apex.exe'" | Where-Object ExecutablePath -EQ $executable)
if($running.Count-gt 0 -and !$StopExisting){throw 'Offline client is already running. Use -StopExisting only for a deliberate test restart.'}
foreach($instance in $running){
    $process=Get-Process -Id $instance.ProcessId
    Stop-Process -Id $instance.ProcessId
    if(!$process.WaitForExit(15000)){throw 'Offline client did not finish exiting.'}
}
$source=Join-Path $workspace 'sources\r5flowstate-sdk\s21-r5sdk-s21-unify\game\client.dll'
$symbols=Join-Path $workspace 'sources\r5flowstate-sdk\s21-r5sdk-s21-unify\game\client.pdb'
$destination=Join-Path $game 'client.dll'
$artifact=Join-Path $workspace 'artifacts\r5flowstate-aim'
$sourceHash=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
$copied=$false
for($attempt=0;$attempt-lt 5;$attempt++){
    try{
        Copy-Item -LiteralPath $source -Destination $destination -ErrorAction Stop
        $copied=$true
        break
    }catch [IO.IOException]{
        if(($_.Exception.HResult-band 0xffff)-notin @(32,33)){throw}
        Start-Sleep -Milliseconds 200
    }
}
if(!$copied){throw 'Installed DLL is still locked; client was not started.'}
$installedHash=(Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
if($sourceHash-ne $installedHash){throw 'Installed DLL hash does not match this build.'}
Copy-Item -LiteralPath $source -Destination (Join-Path $artifact 'client-native-shader.dll')
Copy-Item -LiteralPath $symbols -Destination (Join-Path $artifact 'client-native-shader.pdb')
$arguments=@('-nodiscord','-allowmultiple','-devsdk','-dev','-offline','-fnf','-logfiles',
    '-windowed','-language','schinese','-width','1920','-height','1080',
    '+miles_language','english','+stream_memory','0','+net_usesocketsforloopback','1',
    '+cl_onlineAuthEnable','0','+timeout_during_load','600','+local_ammo_probe','1',
    '+local_entity_probe','1','+local_aim_enable','0','+local_model_highlight','0','+local_aim_circle','1')
if($ShaderDumpDirectory){
    $dumpPath=[IO.Path]::GetFullPath($ShaderDumpDirectory)
    if(!$dumpPath.StartsWith((Join-Path $workspace 'artifacts')+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){
        throw 'Shader dump directory must be inside this workspace artifacts directory.'
    }
    New-Item -ItemType Directory -Path $dumpPath -Force|Out-Null
    $arguments=@('-training-shader-dump',$dumpPath)+$arguments
}
if($TraceTrainingInput){$arguments+=@('+local_training_input_trace','1')}
$client=Start-Process -FilePath $executable -WorkingDirectory $game -ArgumentList $arguments -PassThru
$report=[ordered]@{
    timestampUtc=[DateTimeOffset]::UtcNow.ToString('o')
    pid=$client.Id
    executable=$executable
    executableSha256=$exeHash
    sourceDll=$source
    installedDll=$destination
    dllSha256=$installedHash
    symbols=$symbols
    arguments=$arguments
    connection='Wait for the main menu, then use the SDK command: connect 127.0.0.1:37015'
}
$json=$report|ConvertTo-Json -Depth 4
$json|Set-Content -LiteralPath (Join-Path $artifact 'deployment-current.json') -Encoding utf8
$json
