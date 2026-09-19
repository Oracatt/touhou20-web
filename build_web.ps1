param(
    [ValidateSet('Release','RelWithDebInfo','Debug')]
    [string]$Configuration = 'RelWithDebInfo',
    [string]$EmsdkRoot = 'D:\AIWorkspace\emsdk',
    [ValidateRange(1,128)]
    [int]$Parallel = 4,
    [ValidateRange(1,128)]
    [int]$BinaryenCores = 1,
    [string]$GameDirectory = 'E:\BaiduNetdiskDownload\PCGAME-Touhou.Kinjoukyou.Fossilized.Wonders-P2P\Touhou.Kinjoukyou.Fossilized.Wonders'
)

$ErrorActionPreference = 'Stop'
$emsdkEnvironment = Join-Path $EmsdkRoot 'emsdk_env.ps1'
if (-not (Test-Path -LiteralPath $emsdkEnvironment -PathType Leaf)) {
    throw "Emscripten environment was not found at $emsdkEnvironment"
}

$source = Join-Path $PSScriptRoot 'web'
$build = Join-Path $PSScriptRoot 'build_web'
$emsdkCmake = $null
foreach ($candidateName in @('emcmake.exe','emcmake.bat')) {
    $candidatePath = Join-Path $EmsdkRoot "upstream\emscripten\$candidateName"
    if (Test-Path -LiteralPath $candidatePath -PathType Leaf) {
        $emsdkCmake = $candidatePath
        break
    }
}
if (-not $emsdkCmake) {
    throw "Neither emcmake.exe nor emcmake.bat was found in $EmsdkRoot\upstream\emscripten"
}
$dataDirectory = Join-Path $build 'game-data'

# Load the SDK's own Node and compiler paths in this PowerShell process.
$emsdkQuietBefore = $env:EMSDK_QUIET
$env:EMSDK_QUIET = '1'
try {
    & $emsdkEnvironment
    $emsdkSetupResult = $LASTEXITCODE
} finally {
    $env:EMSDK_QUIET = $emsdkQuietBefore
}
if ($emsdkSetupResult) { throw 'Emscripten environment setup failed' }
if (-not $env:EMSDK_NODE -or -not (Test-Path -LiteralPath $env:EMSDK_NODE -PathType Leaf)) {
    throw 'The activated Emscripten SDK did not provide EMSDK_NODE'
}

# The large game link can exhaust memory when Binaryen creates one worker per
# CPU. Keep its worker limit separate from C++ compilation parallelism.
$binaryenCoresBefore = $env:BINARYEN_CORES
$env:BINARYEN_CORES = [string]$BinaryenCores
try {
    & $emsdkCmake cmake -S $source -B $build -G Ninja "-DCMAKE_BUILD_TYPE=$Configuration"
    if ($LASTEXITCODE) { throw 'Emscripten CMake configuration failed' }
    & cmake --build $build --target th20_web th20_web_game --parallel $Parallel
    if ($LASTEXITCODE) { throw 'WebAssembly compilation failed' }
} finally {
    $env:BINARYEN_CORES = $binaryenCoresBefore
}

New-Item -ItemType Directory -Path $dataDirectory -Force | Out-Null
$dataFiles = foreach ($name in @('th20.dat','thbgm.dat')) {
    $original = Join-Path $GameDirectory $name
    $destination = Join-Path $dataDirectory $name
    if (Test-Path -LiteralPath $original -PathType Leaf) {
        $originalSize = (Get-Item -LiteralPath $original).Length
        if (-not (Test-Path -LiteralPath $destination -PathType Leaf) -or
            (Get-Item -LiteralPath $destination).Length -ne $originalSize) {
            Copy-Item -LiteralPath $original -Destination $destination -Force
        }
    } elseif (-not (Test-Path -LiteralPath $destination -PathType Leaf)) {
        throw "Missing game resource: $original. Supply -GameDirectory or place $name in $dataDirectory"
    }
    [ordered]@{ name = "game-data/$name"; size = (Get-Item -LiteralPath $destination).Length }
}

$archive = Join-Path $dataDirectory 'th20.dat'
$smokeJson = & $env:EMSDK_NODE (Join-Path $build 'smoke_test.cjs') $build $archive
if ($LASTEXITCODE) { throw 'The compiled WASM runtime failed its real-archive smoke test' }
$smoke = ($smokeJson -join [Environment]::NewLine) | ConvertFrom-Json

$artifactNames = @('index.html','app.js','style.css','th20_web.js','th20_web.wasm',
                   'game.html','game.js','game.css','th20_game.js','th20_game.wasm')
$artifacts = foreach ($name in $artifactNames) {
    $path = Join-Path $build $name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing Web build artifact: $name" }
    $item = Get-Item -LiteralPath $path
    [ordered]@{
        name = $name
        size = $item.Length
        sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}
$verification = [ordered]@{
    status = 'wasm_game_and_resource_runtime_compiled'
    generated_at = [DateTimeOffset]::Now.ToString('o')
    configuration = $Configuration
    binaryen_cores = $BinaryenCores
    build_targets = @('th20_web','th20_web_game')
    recovered_game_runtime_linked = $true
    full_game_playable = $false
    original_game_parity_verified = $false
    original_executable_is_build_input = $false
    source_root = $PSScriptRoot
    smoke_test = $smoke
    artifacts = $artifacts
    resource_files = $dataFiles
    manual_browser_observation = [ordered]@{
        scope = 'Earlier browser checks during this port; not rerun by this build script.'
        observed = @('1280x960 canvas with correct 4:3 coverage','title background restored','difficulty selection','character selection','stone selection','manual first-stage loading completed','first stage continuing to update','shooting and movement input','pause menu')
        reported_size_and_title_background_issues_verified = $true
        visual_validation_report = 'WEB_VISUAL_VALIDATION.md'
    }
    note = 'Recovered C++ game code and the browser platform backend are linked as WebAssembly. The real THA1 archive smoke test passed. Full playthrough, visual accuracy, audio accuracy, and original-game frame-by-frame parity are not established by this build.'
}
$verification | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $build 'WEB_BUILD_VERIFICATION.json') -Encoding utf8

Write-Host "Game build: $build\game.html"
Write-Host 'Run .\serve_web.ps1, then open http://127.0.0.1:8123/game.html'
