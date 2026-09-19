param(
    [int]$Port = 8120
)

$ErrorActionPreference = 'Stop'
$site = Join-Path $PSScriptRoot 'build_web'
if (-not (Test-Path -LiteralPath (Join-Path $site 'index.html'))) {
    throw 'Build the WebAssembly target first with .\build_web.ps1'
}

$url = "http://127.0.0.1:$Port/"
Start-Process $url
python -m http.server $Port --bind 127.0.0.1 --directory $site

