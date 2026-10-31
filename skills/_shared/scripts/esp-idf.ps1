function Get-HostTool {
    param([string]$Override,[string]$Command,[string]$WingetId)
    if ($Override) {
        if (!(Test-Path -LiteralPath $Override)) { throw "Missing explicit $Command" }
        return [IO.Path]::GetFullPath($Override)
    }
    $tool = Get-Command $Command -ErrorAction SilentlyContinue
    if ($tool) { return $tool.Source }
    if ($Offline) { throw "Offline host tool missing: $Command; provide explicit path" }
    $winget = Get-Command winget.exe -ErrorAction SilentlyContinue
    if (!$winget) { throw "winget unavailable: install $WingetId or provide explicit $Command path" }
    $args = @('install','--id',$WingetId,'--exact','--scope','user','--silent',
        '--accept-package-agreements','--accept-source-agreements','--disable-interactivity')
    Invoke-BuildCommand $winget.Source $args (Join-Path $BuildRoot "$Command-install.log")
    # Refresh this process only; installer may have registered user PATH entries.
    $env:PATH = [Environment]::GetEnvironmentVariable('PATH','Machine') + ';' +
        [Environment]::GetEnvironmentVariable('PATH','User') + ';' + $env:PATH
    $tool = Get-Command $Command -ErrorAction SilentlyContinue
    if (!$tool) { throw "Installed tool not discoverable: provide explicit $Command path" }
    return $tool.Source
}

function Initialize-Idf {
    $script:idfPython = Get-HostTool $PythonPath 'python.exe' 'Python.Python.3.12'
    $pythonEnvironment = Split-Path (Split-Path $script:idfPython -Parent) -Parent
    if (Test-Path -LiteralPath (Join-Path $pythonEnvironment 'pyvenv.cfg')) {
        $env:IDF_PYTHON_ENV_PATH = $pythonEnvironment
    }
    $git = Get-HostTool $GitPath 'git.exe' 'Git.Git'
    $env:PATH = (Split-Path $script:idfPython) + ';' + (Split-Path $git) + ';' + $env:PATH
    $env:IDF_TOOLS_PATH = Join-Path $CacheRoot 'idf-tools'
    if (!$IdfRoot) {
        $script:IdfRoot = Join-Path $CacheRoot "esp-idf-$($manifest.esp_idf.version)"
        if (!(Test-Path -LiteralPath $script:IdfRoot)) {
            if ($Offline) { throw 'ESP-IDF source not cached; provide IdfRoot' }
            Invoke-BuildCommand $git @('-c','core.longpaths=true','clone','--recursive','--depth','1',
                '--branch',$manifest.esp_idf.version,$manifest.esp_idf.repository,$script:IdfRoot) (Join-Path $BuildRoot 'idf-clone.log')
        }
    }
    $script:IdfRoot = (Resolve-Path -LiteralPath $IdfRoot).Path
    $versionFile = Join-Path $IdfRoot 'tools/cmake/version.cmake'
    if (!(Test-Path -LiteralPath $versionFile)) { throw 'Incomplete ESP-IDF checkout' }
    $version = Get-Content -LiteralPath $versionFile -Raw
    if ($version -notmatch 'IDF_VERSION_MAJOR\s+6\)' -or $version -notmatch 'IDF_VERSION_MINOR\s+1\)') {
        throw 'This dependency manifest requires ESP-IDF 6.1'
    }
    # Local installations may use their existing IDF_TOOLS_PATH explicitly.
    if ($savedEnvironment.ContainsKey('IDF_TOOLS_PATH')) { $env:IDF_TOOLS_PATH = $savedEnvironment['IDF_TOOLS_PATH'] }
    if ($IdfToolsRoot) {
        if (!$PythonPath -or !(Test-Path -LiteralPath (Join-Path $pythonEnvironment 'pyvenv.cfg'))) {
            throw 'Existing EIM tools require PythonPath pointing to its prepared virtual environment'
        }
        $IdfToolsRoot = (Resolve-Path -LiteralPath $IdfToolsRoot).Path
        $env:IDF_TOOLS_PATH = $IdfToolsRoot
        if (!(Test-Path -LiteralPath (Join-Path $IdfToolsRoot 'espidf.constraints.v6.1.txt'))) {
            throw 'Existing EIM tools missing espidf.constraints.v6.1.txt; provide the complete installation'
        }
        $toolList = (Get-Content -LiteralPath (Join-Path $IdfRoot 'tools/tools.json') -Raw | ConvertFrom-Json).tools
        foreach ($tool in $toolList) {
            $recommended = @($tool.versions | Where-Object status -eq 'recommended')
            if (!$recommended.Count) { continue }
            $toolBase = Join-Path $IdfToolsRoot ($tool.name + '/' + $recommended[0].name)
            if (!(Test-Path -LiteralPath $toolBase)) { continue }
            foreach ($parts in $tool.export_paths) {
                $relative = $parts -join '/'
                $bin = Join-Path $toolBase $relative
                # EIM can omit the redundant top-level tool directory in archives.
                if (!(Test-Path -LiteralPath $bin) -and $parts.Count -gt 1 -and $parts[0] -eq $tool.name) {
                    $bin = Join-Path $toolBase ($parts[1..($parts.Count-1)] -join '/')
                }
                if (Test-Path -LiteralPath $bin) { $env:PATH = $bin + ';' + $env:PATH }
            }
            foreach ($property in $tool.export_vars.PSObject.Properties) {
                [Environment]::SetEnvironmentVariable($property.Name,$property.Value.Replace('${TOOL_PATH}',$toolBase),'Process')
            }
        }
        $env:IDF_PATH = $IdfRoot
    } elseif (!$Offline) {
        if ($IdfRoot -match '["%&|<>^!\r\n]') { throw 'IDF path contains CMD metacharacters' }
        Invoke-BuildCommand $env:ComSpec @('/d','/s','/c',"`"call `"$IdfRoot\install.bat`" esp32s3`"") (Join-Path $BuildRoot 'idf-install.log')
    }
    if (!$IdfToolsRoot) { Import-IdfEnvironment $IdfRoot }
    $env:ESP_IDF_VERSION = '6.1.0'
    $env:PYTHONUTF8 = '1'
    if (!(Get-Command xtensa-esp-elf-gcc.exe -ErrorAction SilentlyContinue)) {
        throw 'ESP-IDF activation did not expose the matching Xtensa compiler; check IDF_TOOLS_PATH layout'
    }
    $script:idfPython = (Get-Command python.exe -ErrorAction Stop).Source
}

function Build-Idf {
    $idf = Join-Path $IdfRoot 'tools/idf.py'
    # Absolute project directory avoids dependence on caller working directory.
    Invoke-BuildCommand $script:idfPython @($idf,'-C',$RepoRoot,'-B',$BuildRoot,
        "-DUNIFIED_BOARD=$Target","-DCONVAI_SDK_ROOT=$SdkRoot",'reconfigure') (Join-Path $BuildRoot 'configure.log')
    Invoke-BuildCommand $cmake @('--build',$BuildRoot,'--parallel',"$Jobs") (Join-Path $BuildRoot 'build.log')
    Push-Location $BuildRoot
    try {
        Invoke-BuildCommand $script:idfPython @('-m','esptool','--chip','esp32s3','merge-bin',
            '-o',$artifact,'@flash_args') (Join-Path $BuildRoot 'merge.log')
    } finally { Pop-Location }
}
