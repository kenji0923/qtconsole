#!/usr/bin/env pwsh
# Generate windows/app.ico from resource/icon.svg.
#
# Renders the SVG to a matrix of PNG sizes with Inkscape, then bundles them
# into a single multi-resolution .ico with ImageMagick.
#
# Requirements (both found via PATH): inkscape, magick (ImageMagick).
# Usage: pwsh tools/generate_icon.ps1

$ErrorActionPreference = 'Stop'

# Resolve paths relative to the repo root (this script lives in tools/).
$repoRoot = Split-Path -Parent $PSScriptRoot
$svg      = Join-Path $repoRoot 'resource/icon.svg'
$outIco   = Join-Path $repoRoot 'windows/app.ico'
$sizes    = 16, 24, 32, 48, 64, 128, 256

# Tool checks.
foreach ($tool in 'inkscape', 'magick') {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "Required tool '$tool' not found on PATH."
    }
}
if (-not (Test-Path $svg)) {
    throw "Source SVG not found: $svg"
}

# Temp working dir for the PNG matrix.
$work = Join-Path ([System.IO.Path]::GetTempPath()) ("icon_" + [System.IO.Path]::GetRandomFileName())
New-Item -ItemType Directory -Path $work | Out-Null
try {
    $pngs = foreach ($s in $sizes) {
        $png = Join-Path $work "app-$s.png"
        Write-Host "Rendering ${s}x${s}..."
        & inkscape $svg --export-type=png --export-width=$s --export-height=$s `
            --export-background-opacity=0 --export-filename=$png | Out-Null
        if (-not (Test-Path $png)) { throw "Inkscape failed to produce $png" }
        $png
    }

    Write-Host "Bundling -> $outIco"
    & magick @pngs $outIco
    if ($LASTEXITCODE -ne 0) { throw "magick failed (exit $LASTEXITCODE)" }

    Write-Host "Done. Wrote $outIco"
    & magick identify $outIco
}
finally {
    Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
}
