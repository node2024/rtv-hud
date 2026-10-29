param([string]$Cs2Root, [switch]$LocalPreview, [switch]$Live)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-CompiledRelativePath([string]$SourcePath) {
    switch ([IO.Path]::GetExtension($SourcePath)) {
        '.css' { return [IO.Path]::ChangeExtension($SourcePath, '.vcss_c') }
        '.xml' { return [IO.Path]::ChangeExtension($SourcePath, '.vxml_c') }
        '.js' { return [IO.Path]::ChangeExtension($SourcePath, '.vjs_c') }
        default { throw "Unsupported source extension: $SourcePath" }
    }
}

try {
    if ([string]::IsNullOrWhiteSpace($Cs2Root)) {
        $Cs2Root = Read-Host 'CS2 installation folder (the folder containing game and content)'
    }
    $Cs2Root = $Cs2Root.Trim().Trim('"')
    $Cs2Root = (Resolve-Path -LiteralPath $Cs2Root).Path
    $compiler = Join-Path $Cs2Root 'game\bin\win64\resourcecompiler.exe'
    $gameRoot = Join-Path $Cs2Root 'game\csgo'
    $contentAddon = Join-Path $Cs2Root 'content\csgo_addons\rtv_hud'
    $gameAddon = Join-Path $Cs2Root 'game\csgo_addons\rtv_hud'
    if (-not (Test-Path -LiteralPath $compiler -PathType Leaf)) {
        throw 'resourcecompiler.exe not found. Install CS2 Workshop Tools and check the installation folder.'
    }
    foreach ($folder in @($contentAddon, $gameAddon)) {
        if (-not (Test-Path -LiteralPath $folder -PathType Container)) {
            throw 'Create an addon named rtv_hud in CS2 Workshop Tools first, then run this script again.'
        }
    }
    # These are generic display assets. This script never reads a maplist.
    $assets = @(
        'panorama\styles\custom_game\rtv_hud\vote.css',
        'panorama\layout\custom_game\rtv_hud\vote.xml',
        'panorama\styles\custom_game\rtv_hud\browser_scroll.css',
        'panorama\layout\custom_game\rtv_hud\browser_scroll.xml'
    )
    if ($LocalPreview) {
        $assets = @(
            'panorama\styles\custom_game\rtv_hud\vote.css',
            'panorama\layout\custom_game\rtv_hud\preview.xml'
        )
    }
    foreach ($relative in $assets) {
        if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot $relative) -PathType Leaf)) {
            throw "Source missing: $relative. Extract the entire ZIP before running."
        }
    }
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
    $runFolder = Join-Path $PSScriptRoot "build-output\$stamp"
    New-Item -ItemType Directory -Path $runFolder -Force | Out-Null
    $log = Join-Path $runFolder 'compile.log'
    "CS2: $Cs2Root`r`nAddon: $gameAddon" | Set-Content -LiteralPath $log -Encoding UTF8

    # Only the selected source files are installed. Preserve a changed previous copy.
    foreach ($relative in $assets) {
        $source = Join-Path $PSScriptRoot $relative
        $destination = Join-Path $contentAddon $relative
        if (Test-Path -LiteralPath $destination -PathType Leaf) {
            if ((Get-FileHash -LiteralPath $source).Hash -ne (Get-FileHash -LiteralPath $destination).Hash) {
                $backup = Join-Path $runFolder (Join-Path 'previous-source' $relative)
                New-Item -ItemType Directory -Path (Split-Path -Parent $backup) -Force | Out-Null
                Copy-Item -LiteralPath $destination -Destination $backup
            }
        }
        New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
        Copy-Item -LiteralPath $source -Destination $destination -Force
    }

    # Compile CSS first so the XML include can resolve it.
    Push-Location (Split-Path -Parent $compiler)
    try {
        foreach ($relative in $assets) {
            $inputFile = Join-Path $contentAddon $relative
            Write-Host "Compiling $relative"
            "`r`nINPUT: $inputFile" | Add-Content -LiteralPath $log -Encoding UTF8
            # PowerShell 5.1 treats native stderr as ErrorRecord, including compiler warnings.
            $ErrorActionPreference = 'Continue'
            & $compiler -i $inputFile -game $gameRoot -f 2>&1 | ForEach-Object {
                $line = $_.ToString()
                Write-Host $line
                Add-Content -LiteralPath $log -Value $line -Encoding UTF8
            }
            $compilerExit = $LASTEXITCODE
            $ErrorActionPreference = 'Stop'
            if ($compilerExit -ne 0) { throw "Compiler exit code: $compilerExit. See $log" }
            $compiled = Join-Path $gameAddon (Get-CompiledRelativePath $relative)
            if (-not (Test-Path -LiteralPath $compiled -PathType Leaf)) {
                throw "Compiled file missing: $compiled. See $log"
            }
            if ((Get-Item -LiteralPath $compiled).Length -eq 0) { throw "Empty compiled file: $compiled" }
        }
    } finally { Pop-Location }

    # This ZIP is a handoff for inspection, not a Workshop VPK or automatic upload.
    $stage = Join-Path $runFolder 'compiled-addon'
    foreach ($relative in $assets) {
        $compiledRelative = Get-CompiledRelativePath $relative
        $target = Join-Path $stage $compiledRelative
        New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $gameAddon $compiledRelative) -Destination $target
    }
    $addonInfo = Join-Path $gameAddon 'addoninfo.txt'
    if (Test-Path -LiteralPath $addonInfo -PathType Leaf) {
        Copy-Item -LiteralPath $addonInfo -Destination (Join-Path $stage 'addoninfo.txt')
    }
    Get-ChildItem -LiteralPath $stage -Recurse -File | Get-FileHash -Algorithm SHA256 |
        Format-Table -AutoSize | Out-String -Width 4096 |
        Set-Content -LiteralPath (Join-Path $runFolder 'SHA256.txt') -Encoding UTF8
    $archiveName = 'rtv-hud-compiled.zip'
    if ($LocalPreview) { $archiveName = 'rtv-hud-local-preview-compiled.zip' }
    if ($Live) { $archiveName = 'rtv-hud-live-compiled.zip' }
    $archive = Join-Path $runFolder $archiveName
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $archive
    Write-Host ''
    Write-Host 'BUILD OK. Compiled HUD files are in:' -ForegroundColor Green
    Write-Host $gameAddon
    Write-Host 'Compiled ZIP:'
    Write-Host $archive
    if ($LocalPreview) {
        Write-Host 'Next: open Workshop Tools with the rtv_hud addon, load map de_dust2, then run:'
        Write-Host 'sv_cheats 1'
        Write-Host 'ent_create custom_hud_layout { "targetname" "rtvhud_local_preview" "layout" "panorama/layout/custom_game/rtv_hud/preview.xml" }'
        Write-Host 'See LOCAL-PREVIEW.md. This is a static visual preview, not a live vote.'
    }
    Write-Host 'Nothing has been published or installed on the server.'
    exit 0
} catch {
    Write-Host ''
    Write-Host ('BUILD FAILED: ' + $_.Exception.Message) -ForegroundColor Red
    exit 1
}
