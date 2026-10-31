[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('lrd','szpi','hoperun-app','hoperun-win')][string]$Target,
    [Parameter(Mandatory)][string]$RepoRoot,
    [string]$BuildRoot,
    [string]$CacheRoot,
    [string]$DependencyManifest,
    [string]$GoldieosToolsRoot,
    [string]$SdkRoot,
    [string]$MinGWRoot,
    [string]$IdfRoot,
    [string]$IdfToolsRoot,
    [string]$CMakePath,
    [string]$NinjaPath,
    [string]$GitPath,
    [string]$PythonPath,
    [string]$LocalConfig,
    [ValidateRange(1,32)][int]$Jobs=4,
    [switch]$Offline,
    [switch]$PrepareOnly
)
. (Join-Path $PSScriptRoot 'dependencies.ps1')
$buildToolsRoot = Split-Path $PSScriptRoot -Parent
if (!$DependencyManifest) { $DependencyManifest = Join-Path $buildToolsRoot 'manifests/dependencies.json' }
$DependencyManifest = (Resolve-Path -LiteralPath $DependencyManifest).Path
$manifestDirectory = Split-Path $DependencyManifest -Parent
$manifest = Get-Content -LiteralPath $DependencyManifest -Raw | ConvertFrom-Json
if ($manifest.schema_version -ne 1) { throw 'Unsupported dependency manifest schema' }
$RepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
if (!(Test-Path -LiteralPath (Join-Path $RepoRoot 'src/platforms/goldieos/targets/ws63/CMakeLists.txt'))) {
    throw 'Expected latest unified-app architecture; old demo source is unsupported'
}
if (!$CacheRoot) { $CacheRoot = Join-Path $buildToolsRoot 'manifests/cache' }
$CacheRoot = [IO.Path]::GetFullPath($CacheRoot)
if (!$BuildRoot) { $BuildRoot = Join-Path $RepoRoot "build/skills/$Target" }
$BuildRoot = [IO.Path]::GetFullPath($BuildRoot)
if ($BuildRoot -eq $RepoRoot) { throw 'Build directory cannot be the source root' }
New-Item -ItemType Directory -Path $CacheRoot,$BuildRoot -Force | Out-Null
$marker = Join-Path $BuildRoot 'skill-target.txt'
if (Test-Path -LiteralPath $marker) {
    if ((Get-Content -LiteralPath $marker -Raw).Trim() -ne $Target) { throw 'Use a separate build directory per target' }
} elseif (Test-Path -LiteralPath (Join-Path $BuildRoot 'CMakeCache.txt')) {
    throw 'Use a new skill build directory; do not adopt an unrelated CMake cache'
}
Set-Content -LiteralPath $marker -Value $Target -Encoding ASCII
$resultPath = Join-Path $BuildRoot 'build-result.json'
if (Test-Path -LiteralPath $resultPath) { Remove-Item -LiteralPath $resultPath }
$savedEnvironment = @{}
Get-ChildItem Env: | ForEach-Object { $savedEnvironment[$_.Name] = $_.Value }
try {
    $cmake = Resolve-BuildTool $CMakePath 'cmake.exe' 'cmake' 'bin/cmake.exe'
    $ninja = Resolve-BuildTool $NinjaPath 'ninja.exe' 'ninja' 'ninja.exe'
    $cmakeVersion = (& $cmake --version | Select-Object -First 1)
    if ($cmakeVersion -notmatch '(\d+\.\d+\.\d+)' -or [version]$matches[1] -lt [version]'3.21') { throw 'CMake >=3.21 required' }
    $env:PATH = (Split-Path $cmake) + ';' + (Split-Path $ninja) + ';' + $env:PATH
    if (!$SdkRoot) {
        $bundled = Join-Path $RepoRoot 'third_party/convai-sdk-26.8.0'
        if (Test-Path -LiteralPath (Join-Path $bundled 'include/convai/convai_api.h')) { $SdkRoot = $bundled }
        else { $SdkRoot = Get-Dependency 'sdk' $manifest.sdk $manifestDirectory $CacheRoot -Offline:$Offline }
    }
    $SdkRoot = (Resolve-Path -LiteralPath $SdkRoot).Path
    $sdkPlatform = if ($Target -eq 'hoperun-app') {'ws63'} elseif ($Target -eq 'hoperun-win') {'windows'} else {'esp32s3'}
    foreach ($file in @('include/convai/convai_api.h',"libs/$sdkPlatform/libconvai_sdk.a")) {
        if (!(Test-Path -LiteralPath (Join-Path $SdkRoot $file))) { throw "Incomplete matching SDK: $file" }
    }
    if ($Target.StartsWith('hoperun-')) {
        $runtimeArchive = Join-Path $RepoRoot 'third_party/goldieos/runtime.tar.gz'
        $runtimePin = Get-Content (Join-Path $RepoRoot 'third_party/goldieos/checksums.cmake') -Raw
        if ($runtimePin -notmatch 'GOLDIEOS_RUNTIME_SHA256\s+"([a-f0-9]{64})"') { throw 'Missing GoldieOS runtime checksum' }
        $runtimeHash = $matches[1]
        if (!(Test-Path -LiteralPath $runtimeArchive) -or
            (Get-FileHash -LiteralPath $runtimeArchive -Algorithm SHA256).Hash -ne $runtimeHash) {
            throw 'Missing or modified bundled GoldieOS runtime archive'
        }
        $backend = if ($Target -eq 'hoperun-app') {'ws63'} else {'windows'}
        $configure = @('-S',(Join-Path $RepoRoot "src/platforms/goldieos/targets/$backend"),'-B',$BuildRoot,'-G','Ninja',
            "-DCMAKE_MAKE_PROGRAM=$ninja",'-DCMAKE_BUILD_TYPE=Release','-DWS63_BOARD=hoperun',
            "-DCONVAI_SDK_ROOT=$SdkRoot")
        if ($LocalConfig) {
            $LocalConfig = (Resolve-Path -LiteralPath $LocalConfig).Path
            $configure += "-DWS63_LOCAL_CONFIG=$LocalConfig"
        }
        if ($backend -eq 'ws63') {
            if (!$GoldieosToolsRoot) {
                $GoldieosToolsRoot = Get-Dependency 'ws63_tools' $manifest.ws63_tools $manifestDirectory $CacheRoot -Offline:$Offline
            }
            $GoldieosToolsRoot = (Resolve-Path -LiteralPath $GoldieosToolsRoot).Path
            $chain = Join-Path $RepoRoot 'src/platforms/goldieos/cmake/toolchains/ws63-riscv-gcc.cmake'
            $compilerBin = Join-Path $GoldieosToolsRoot 'compiler/bin'
            foreach ($file in @((Join-Path $compilerBin 'riscv32-linux-musl-gcc.exe'),(Join-Path $GoldieosToolsRoot 'ws63_link_v4.exe'),(Join-Path $GoldieosToolsRoot 'ws63_sign_tool.exe'))) {
                if (!(Test-Path -LiteralPath $file)) { throw 'Incomplete WS63 tool bundle: compiler/bin, linker and signer required' }
            }
            $env:PATH = $compilerBin + ';' + $env:PATH
            $configure += @("-DCMAKE_TOOLCHAIN_FILE=$chain","-DGOLDIEOS_TOOLS_ROOT=$GoldieosToolsRoot")
            $artifact = Join-Path $BuildRoot 'out/unified_ai_hoperun.fwpkg'
        } else {
            if (!$MinGWRoot) { $MinGWRoot = Get-Dependency 'mingw' $manifest.mingw $manifestDirectory $CacheRoot -Offline:$Offline }
            $MinGWRoot = (Resolve-Path -LiteralPath $MinGWRoot).Path
            $gcc = Join-Path $MinGWRoot 'bin/gcc.exe'; $gxx = Join-Path $MinGWRoot 'bin/g++.exe'
            if (!(Test-Path -LiteralPath $gxx)) { throw 'MinGW g++ missing' }
            $machine = & $gcc -dumpmachine
            if ($LASTEXITCODE -ne 0 -or $machine -notmatch '^x86_64-w64-mingw32$') { throw 'Expected x64 Windows MinGW compiler' }
            $env:PATH = (Join-Path $MinGWRoot 'bin') + ';' + $env:PATH
            $configure += @("-DCMAKE_C_COMPILER=$gcc","-DCMAKE_CXX_COMPILER=$gxx")
            $artifact = Join-Path $BuildRoot 'unified_ai_simulator.exe'
        }
    } else {
        . (Join-Path $PSScriptRoot 'esp-idf.ps1')
        Initialize-Idf
        $artifact = Join-Path $BuildRoot "unified_ai_${Target}_merged.bin"
    }
    if ($PrepareOnly) { Write-Output "Dependencies ready for $Target; no image built."; return }
    # Remove only the exact expected delivery file. Build errors cannot reuse it.
    if (Test-Path -LiteralPath $artifact) { Remove-Item -LiteralPath $artifact }
    if ($Target.StartsWith('hoperun-')) {
        Invoke-BuildCommand $cmake $configure (Join-Path $BuildRoot 'configure.log')
        Invoke-BuildCommand $cmake @('--build',$BuildRoot,'--parallel',"$Jobs") (Join-Path $BuildRoot 'build.log')
        if ($Target -eq 'hoperun-win') {
            Invoke-BuildCommand (Join-Path $MinGWRoot 'bin/objdump.exe') @('-p',$artifact) (Join-Path $BuildRoot 'pe-dependencies.log')
            $imports = Select-String -LiteralPath (Join-Path $BuildRoot 'pe-dependencies.log') -Pattern 'DLL Name:\s+(\S+)'
            foreach ($import in $imports) {
                $dll = $import.Matches[0].Groups[1].Value
                $runtimeDll = Join-Path $MinGWRoot "bin/$dll"
                if (Test-Path -LiteralPath $runtimeDll) { Copy-Item -LiteralPath $runtimeDll -Destination $BuildRoot -Force }
            }
        }
    } else { Build-Idf }
    if (!(Test-Path -LiteralPath $artifact) -or (Get-Item -LiteralPath $artifact).Length -eq 0) { throw 'Missing or empty artifact' }
    $receipt = [ordered]@{ target=$Target; source=$RepoRoot; build=$BuildRoot; artifact=$artifact;
        sha256=(Get-FileHash -LiteralPath $artifact -Algorithm SHA256).Hash.ToLowerInvariant();
        bytes=(Get-Item -LiteralPath $artifact).Length; cmake=$cmakeVersion;
        sdk_version=$manifest.sdk.version; manifest_sha256=(Get-FileHash -LiteralPath $DependencyManifest).Hash;
        completed_utc=[DateTime]::UtcNow.ToString('o'); status='compiled'; functional_validation='not_run' }
    $receipt | ConvertTo-Json | Set-Content -LiteralPath $resultPath -Encoding UTF8
    Write-Output "Build succeeded: $artifact"
    Write-Output "Receipt: $resultPath"
} finally {
    # Environment changes are scoped to this invocation, including IDF export.
    foreach ($item in @(Get-ChildItem Env:)) {
        if (!$savedEnvironment.ContainsKey($item.Name)) { [Environment]::SetEnvironmentVariable($item.Name,$null,'Process') }
    }
    foreach ($key in $savedEnvironment.Keys) { [Environment]::SetEnvironmentVariable($key,$savedEnvironment[$key],'Process') }
}
