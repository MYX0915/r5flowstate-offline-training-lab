$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$driver = Join-Path $root 'sources\blackbone-reference\src\BlackBoneDrv\bin\x64\Win10Release\BlackBoneDrv10.sys'
$signtool = 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe'
$folder = Join-Path $root 'artifacts\blackbone-driver-preflight'
$identity = [Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
$elevated = $identity.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
$signature = Get-AuthenticodeSignature -LiteralPath $driver
$previousPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
& $signtool verify /kp /v $driver *> (Join-Path $folder 'kernel-signature-check-host.txt')
$code = $LASTEXITCODE
$ErrorActionPreference = $previousPreference
[pscustomobject]@{
    VerifiedAtUtc = [DateTime]::UtcNow.ToString('o')
    Elevated = $elevated
    Driver = $driver
    SHA256 = (Get-FileHash -LiteralPath $driver -Algorithm SHA256).Hash
    SignToolExitCode = $code
    AuthenticodeStatus = $signature.Status.ToString()
    AuthenticodeMessage = $signature.StatusMessage
    Signer = $signature.SignerCertificate.Subject
    CertificateExpiryUtc = $signature.SignerCertificate.NotAfter.ToUniversalTime().ToString('o')
    DriverLoadAttempted = $false
}|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $folder 'signature-result-host.json') -Encoding UTF8
