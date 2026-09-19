param(
    [ValidateRange(1,65535)]
    [int]$Port = 8123,
    [string]$Python = 'python',
    [switch]$OpenBrowser
)

$ErrorActionPreference = 'Stop'
$webBuildDirectory = Join-Path $PSScriptRoot 'build_web'
if (-not (Test-Path -LiteralPath (Join-Path $webBuildDirectory 'game.html') -PathType Leaf)) {
    throw 'The WASM game is not built yet. Run .\build_web.ps1 first.'
}

$serverArguments = @((Join-Path $PSScriptRoot 'web\serve.py'),
                     '--directory', $webBuildDirectory, '--port', $Port)
if ($OpenBrowser) { $serverArguments += '--open-browser' }
& $Python @serverArguments
if ($LASTEXITCODE) { throw "The local HTTP server exited with code $LASTEXITCODE" }
