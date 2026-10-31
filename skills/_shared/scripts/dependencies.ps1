Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Invoke-BuildCommand {
    param([string]$Exe, [string[]]$Arguments, [string]$Log)
    & $Exe @Arguments *> $Log
    if ($LASTEXITCODE -ne 0) { throw "Command failed (exit $LASTEXITCODE). Inspect private log: $Log" }
}

function Get-Dependency {
    param([string]$Name, $Spec, [string]$ManifestDirectory, [string]$CacheRoot, [switch]$Offline)
    if (!$Spec.sha256 -or $Spec.sha256 -notmatch '^[0-9a-fA-F]{64}$') {
        throw "Dependency '$Name' needs a trusted SHA-256 and a local_path or URL in the manifest."
    }
    $identity = $Spec.sha256.ToLowerInvariant()
    $archive = $null
    if ($Spec.PSObject.Properties['local_path'] -and $Spec.local_path) {
        $archive = [IO.Path]::GetFullPath((Join-Path $ManifestDirectory $Spec.local_path))
        if (!(Test-Path -LiteralPath $archive -PathType Leaf)) { throw "Missing local dependency: $Name" }
    } else {
        if (!$Spec.url -or ([uri]$Spec.url).Scheme -ne 'https') { throw "Missing HTTPS download URL for $Name" }
        $filename = [IO.Path]::GetFileName(([uri]$Spec.url).AbsolutePath)
        $archive = Join-Path $CacheRoot "$identity-$filename"
        if (!(Test-Path -LiteralPath $archive)) {
            if ($Offline) { throw "Offline dependency not cached: $Name" }
            $partial = "$archive.partial"
            [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
            Invoke-WebRequest -UseBasicParsing -Uri $Spec.url -OutFile $partial
            if ((Get-FileHash -LiteralPath $partial -Algorithm SHA256).Hash -ne $Spec.sha256) {
                throw "Downloaded dependency checksum mismatch: $Name"
            }
            Move-Item -LiteralPath $partial -Destination $archive -Force
        }
    }
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $Spec.sha256) {
        throw "Local/cached dependency checksum mismatch: $Name"
    }
    $destination = Join-Path $CacheRoot "$Name-$identity"
    $stamp = Join-Path $destination '.complete'
    if (!(Test-Path -LiteralPath $stamp)) {
        # Fresh directory prevents partial extraction from becoming a valid cache.
        $stage = Join-Path $CacheRoot "$Name-$([guid]::NewGuid().ToString('N'))"
        New-Item -ItemType Directory -Path $stage -Force | Out-Null
        if ($archive.EndsWith('.zip',[StringComparison]::OrdinalIgnoreCase)) {
            Add-Type -AssemblyName System.IO.Compression.FileSystem
            [IO.Compression.ZipFile]::ExtractToDirectory($archive,$stage)
        } elseif ($archive -match '\.(tar|tar\.gz|tgz)$') {
            $tar = (Get-Command tar.exe -ErrorAction Stop).Source
            Invoke-BuildCommand $tar @('-xf',$archive,'-C',$stage) (Join-Path $stage 'extract.log')
        } else { throw "Unsupported archive for $Name; use .zip, .tar or .tar.gz" }
        Set-Content -LiteralPath (Join-Path $stage '.complete') -Value $identity -Encoding ASCII
        # A previous incomplete destination is left intact for diagnosis.
        if (Test-Path -LiteralPath $destination) { throw "Incomplete cache directory exists: $destination" }
        Move-Item -LiteralPath $stage -Destination $destination
    }
    $root = [IO.Path]::GetFullPath((Join-Path $destination $Spec.relative_root))
    if ($root -ne $destination -and !$root.StartsWith($destination + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
        throw "Dependency root escapes extraction directory: $Name"
    }
    if (!(Test-Path -LiteralPath $root -PathType Container)) { throw "Wrong relative_root for $Name" }
    return $root
}

function Resolve-BuildTool {
    param([string]$Override, [string]$Command, [string]$Dependency, [string]$RelativeExe)
    if ($Override) {
        if (!(Test-Path -LiteralPath $Override -PathType Leaf)) { throw "Tool not found: $Command" }
        return [IO.Path]::GetFullPath($Override)
    }
    $existing = Get-Command $Command -ErrorAction SilentlyContinue
    if ($existing) { return $existing.Source }
    $root = Get-Dependency $Dependency $manifest.$Dependency $manifestDirectory $CacheRoot -Offline:$Offline
    $exe = Join-Path $root $RelativeExe
    if (!(Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Dependency does not provide $Command" }
    return $exe
}

function Import-IdfEnvironment {
    param([string]$Root)
    # Do not concatenate caller-controlled paths into cmd.exe code.
    if ($Root -match '["%&|<>^!\r\n]') { throw 'IDF path contains unsupported CMD metacharacters' }
    $envDump = & $env:ComSpec /d /s /c "`"call `"$Root\export.bat`" >nul && set`""
    if ($LASTEXITCODE -ne 0) { throw 'ESP-IDF export failed' }
    foreach ($line in $envDump) {
        if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1],$matches[2],'Process') }
    }
    if (!$env:IDF_PYTHON_ENV_PATH -or !(Test-Path -LiteralPath (Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts/python.exe'))) {
        throw 'ESP-IDF activation did not produce a valid Python environment; inspect install/export logs'
    }
}
