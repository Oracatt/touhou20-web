param(
    [string]$EmsdkRoot = 'D:\AIWorkspace\emsdk',
    [string]$BuildDirectory = '',
    [string]$BgmArchive = ''
)

$ErrorActionPreference = 'Stop'
$sourceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if (-not $BuildDirectory) { $BuildDirectory = Join-Path $sourceRoot 'build_web' }
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
if (-not $BgmArchive) { $BgmArchive = Join-Path $BuildDirectory 'game-data\thbgm.dat' }
$BgmArchive = [IO.Path]::GetFullPath($BgmArchive)
$emsdkEnvironment = Join-Path $EmsdkRoot 'emsdk_env.ps1'
if (-not (Test-Path -LiteralPath $emsdkEnvironment -PathType Leaf)) {
    throw "Emscripten environment not found: $emsdkEnvironment"
}
if (-not (Test-Path -LiteralPath $BgmArchive -PathType Leaf)) {
    throw "BGM test input not found: $BgmArchive. Supply -BgmArchive or build the game resource package first."
}

$compiler = $null
foreach ($name in @('em++.exe', 'em++.bat')) {
    $candidate = Join-Path $EmsdkRoot "upstream\emscripten\$name"
    if (Test-Path -LiteralPath $candidate -PathType Leaf) { $compiler = $candidate; break }
}
if (-not $compiler) { throw "No em++.exe or em++.bat found in $EmsdkRoot\upstream\emscripten" }

$quietBefore = $env:EMSDK_QUIET
$binaryenBefore = $env:BINARYEN_CORES
try {
    $env:EMSDK_QUIET = '1'
    & $emsdkEnvironment
    if ($LASTEXITCODE) { throw 'Emscripten environment setup failed' }
    if (-not $env:EMSDK_NODE -or -not (Test-Path -LiteralPath $env:EMSDK_NODE -PathType Leaf)) {
        throw 'The activated SDK did not provide a usable EMSDK_NODE'
    }
    $env:BINARYEN_CORES = '1'
    New-Item -ItemType Directory -Path $BuildDirectory -Force | Out-Null

    Write-Host 'Compiling and running the HUD score regression...'
    $hudOutput = Join-Path $BuildDirectory 'hud_score_test.js'
    $hudArguments = @(
        '-std=c++20', '-O2', '-DTH20_WEB=1', '-Wno-invalid-offsetof',
        '-msse', '-msse2', '-msimd128', '-fexceptions',
        "-I$sourceRoot\web\compat", "-I$sourceRoot\include",
        "-I$sourceRoot\native_recovered", "-I$sourceRoot\source_reconstruction\core_scheduler",
        (Join-Path $PSScriptRoot 'hud_score.cpp'),
        (Join-Path $sourceRoot 'source_reconstruction\text_renderer\format.cpp'),
        '-o', $hudOutput, '-sENVIRONMENT=node', '-sEXIT_RUNTIME=1'
    )
    & $compiler @hudArguments
    if ($LASTEXITCODE) { throw 'HUD score regression compilation failed' }
    & $env:EMSDK_NODE $hudOutput
    if ($LASTEXITCODE) { throw 'HUD score regression failed' }

    Write-Host 'Compiling the browser pixel regression against the production renderer...'
    $renderArguments = @(
        '-std=c++20', '-O2', '-g1', '-DTH20_WEB=1', "-I$sourceRoot\web\compat",
        (Join-Path $PSScriptRoot 'render_probe.cpp'),
        (Join-Path $sourceRoot 'web\src\web_d3d9.cpp'),
        '-o', (Join-Path $BuildDirectory 'render_probe.js'),
        '-sUSE_WEBGL2=1', '-sFULL_ES3=1', '-sASYNCIFY=1',
        '-sENVIRONMENT=web', '-sNO_EXIT_RUNTIME=1', '-sASSERTIONS=2'
    )
    & $compiler @renderArguments
    if ($LASTEXITCODE) { throw 'Browser pixel regression compilation failed' }
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'render_probe.html') -Destination (Join-Path $BuildDirectory 'render_probe.html') -Force
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'gameplay_probe.html') -Destination (Join-Path $BuildDirectory 'gameplay_probe.html') -Force

    Write-Host 'Running the production audio scheduler regression with original PCM...'
    & $env:EMSDK_NODE (Join-Path $sourceRoot 'web\audio_stream_test.cjs') $BgmArchive
    if ($LASTEXITCODE) { throw 'Audio stream regression failed' }
} finally {
    $env:EMSDK_QUIET = $quietBefore
    $env:BINARYEN_CORES = $binaryenBefore
}

Write-Host 'HUD and audio checks passed. The pixel probe is compiled; browser execution is still required.'
Write-Host "Serve $BuildDirectory and open render_probe.html. Expected result: 27/27 pixel checks passed."
Write-Host 'For the default build directory: .\serve_web.ps1, then http://127.0.0.1:8123/render_probe.html'
