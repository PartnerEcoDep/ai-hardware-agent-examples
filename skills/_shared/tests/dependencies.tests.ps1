param([Parameter(Mandatory)][string]$TestRoot)
. (Join-Path $PSScriptRoot '../scripts/dependencies.ps1')
$TestRoot = [IO.Path]::GetFullPath($TestRoot)
New-Item -ItemType Directory -Path $TestRoot -Force | Out-Null
$fixture = Join-Path $TestRoot ([guid]::NewGuid().ToString('N'))
$inputDir = Join-Path $fixture 'input'
$cache = Join-Path $fixture 'cache'
New-Item -ItemType Directory -Path $inputDir,$cache -Force | Out-Null
Set-Content -LiteralPath (Join-Path $inputDir 'payload.txt') -Value 'fixture' -Encoding ASCII
$zip = Join-Path $fixture 'fixture.zip'
Compress-Archive -Path (Join-Path $inputDir '*') -DestinationPath $zip
$spec = [pscustomobject]@{sha256=(Get-FileHash -LiteralPath $zip).Hash;local_path='fixture.zip';url=$null;relative_root='.'}
$root = Get-Dependency 'fixture' $spec $fixture $cache -Offline
if (!(Test-Path -LiteralPath (Join-Path $root 'payload.txt'))) { throw 'Local archive extraction failed' }
$cached = Get-Dependency 'fixture' $spec $fixture $cache -Offline
if ($cached -ne $root) { throw 'Cache not reused' }
function Expect-Failure([scriptblock]$Operation,[string]$Expected) {
    $failed = $false
    try { & $Operation | Out-Null } catch {
        $failed = $true
        if ($_.Exception.Message -notlike "*$Expected*") { throw }
    }
    if (!$failed) { throw "Expected failure: $Expected" }
}
$spec.sha256 = '0' * 64
Expect-Failure { Get-Dependency 'fixture' $spec $fixture $cache -Offline } 'checksum mismatch'
$spec.sha256 = (Get-FileHash -LiteralPath $zip).Hash
$spec.local_path = 'missing.zip'
Expect-Failure { Get-Dependency 'fixture' $spec $fixture $cache -Offline } 'Missing local dependency'
$spec.local_path = $null; $spec.url='https://example.invalid/no-network.zip'
Expect-Failure { Get-Dependency 'missing' $spec $fixture $cache -Offline } 'Offline dependency not cached'
$spec.local_path = 'fixture.zip'; $spec.relative_root='../..'
Expect-Failure { Get-Dependency 'fixture' $spec $fixture $cache -Offline } 'escapes extraction directory'
Write-Output 'Dependency tests passed: local ZIP, cache reuse, bad hash, missing local, offline, root containment.'
