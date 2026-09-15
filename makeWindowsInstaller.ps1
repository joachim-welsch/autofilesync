<#
.SYNOPSIS
    Baut das Windows-Installationspaket AutoFileSync-<Version>.msi.
.DESCRIPTION
    Gegenstueck zu makeMacOSInstaller.sh: baut die Release-Version, stellt die
    Nutzlast in einem eigenen Verzeichnis zusammen, prueft sie und laesst WiX
    daraus ein MSI erzeugen. Geprueft wird vor dem Paketieren, weil ein MSI ohne
    Katalog oder ohne eine der Laufzeit-DLLs erst beim Anwender auffaellt.
.EXAMPLE
    .\makeWindowsInstaller.ps1
.EXAMPLE
    .\makeWindowsInstaller.ps1 -NoBuild -Output C:\Temp
#>
param(
    [ValidateSet('Release', 'Debug')][string]$Configuration = 'Release',
    [switch]$NoBuild,
    [string]$Output
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $root "AFS_Desktop\x64\$Configuration"
$stagingDir = Join-Path $root 'build\windows_installer'
$payloadDir = Join-Path $stagingDir 'payload'
$wxs = Join-Path $root 'AFS_Desktop\installer\autofilesync.wxs'
$icon = Join-Path $root 'AFS_Desktop\icons\afs.ico'
$modelSetup = 'setup_llama_windows.ps1', 'setup_llama_windows.cmd'
if (-not $Output) { $Output = Join-Path $root 'build' }

# Version.h ist die einzige Quelle der Versionsnummer - wie afs_version() unter macOS.
function Get-Version {
    $header = Join-Path $root 'AFS_Desktop\include\Version.h'
    $match = Select-String -Path $header -Pattern 'afsVERSION_STRING\[\]\s*=\s*"([^"]+)"'
    if (-not $match) { throw "Version nicht aus $header lesbar." }
    return $match.Matches[0].Groups[1].Value
}

# Windows Installer vergleicht Versionsfelder als Zahlen; "0.01.0001" waere als
# Zeichenkette gueltig, aber fuehrende Nullen lehnt WiX ab. Das vierte Feld
# wertet der Installer beim Upgrade-Vergleich ohnehin nicht aus.
function ConvertTo-MsiVersion([string]$version) {
    $fields = $version.Split('.') | Select-Object -First 3 | ForEach-Object { [int]$_ }
    while ($fields.Count -lt 3) { $fields += 0 }
    return ($fields -join '.')
}

function Copy-Payload {
    if (Test-Path $payloadDir) { Remove-Item -Recurse -Force $payloadDir }
    New-Item -ItemType Directory -Force $payloadDir | Out-Null
    Copy-Item (Join-Path $buildDir 'autofilesync.exe') $payloadDir
    Get-ChildItem (Join-Path $buildDir '*.dll') | Copy-Item -Destination $payloadDir
    foreach ($directory in 'help', 'locale') {
        $source = Join-Path $buildDir $directory
        if (-not (Test-Path $source)) { throw "Im Build fehlt $directory - erwartet in $buildDir." }
        Copy-Item -Recurse $source (Join-Path $payloadDir $directory)
    }
    foreach ($script in $modelSetup) { Copy-Item (Join-Path $root $script) $payloadDir }
}

# Die Laufzeit-DLLs stehen hier namentlich, damit eine fehlende auffaellt statt
# mitzufehlen: intl-8 und iconv-2 haben genau so einmal gefehlt, und die Exe
# startet ohne sie gar nicht erst. Ohne die Setup-Skripte zeigt der
# Startmenue-Eintrag fuer das Chat-Modell ins Leere.
function Test-Payload {
    $required = @(
        'autofilesync.exe',
        'intl-8.dll', 'iconv-2.dll', 'libssh2.dll', 'zlib1.dll',
        'libssl-3-x64.dll', 'libcrypto-3-x64.dll',
        'llama.dll', 'ggml.dll', 'ggml-base.dll', 'ggml-cpu.dll',
        'help\help.htb'
    ) + $modelSetup
    foreach ($language in (Get-ChildItem (Join-Path $root 'po') -Filter '*.po')) {
        $required += "locale\$($language.BaseName)\LC_MESSAGES\autofilesync.mo"
    }
    $missing = $required | Where-Object { -not (Test-Path (Join-Path $payloadDir $_)) }
    if ($missing) { throw "Nutzlast unvollstaendig: $($missing -join ', ')" }
    Write-Host "Nutzlast vollstaendig ($($required.Count) geforderte Eintraege)." -ForegroundColor Green
}

# Die UI-Erweiterung wird global abgelegt, nicht als .wix-Verzeichnis im Repo.
# Ihre Version muss die von wix selbst sein: ohne Angabe laedt "extension add"
# die neueste (derzeit 7.x), und wix 5 kann die nicht laden.
function Install-WixUiExtension {
    if (-not (Get-Command wix -ErrorAction SilentlyContinue)) {
        throw "wix nicht gefunden - installieren mit: dotnet tool install --global wix"
    }
    $wixVersion = (& wix --version).Split('+')[0]
    $installed = (& wix extension list -g) -join "`n"
    if ($installed -match "WixToolset\.UI\.wixext\s+$([regex]::Escape($wixVersion))") { return }
    Write-Host "WiX-UI-Erweiterung $wixVersion wird nachgeladen..."
    & wix extension add -g "WixToolset.UI.wixext/$wixVersion"
    if ($LASTEXITCODE -ne 0) { throw "WiX-UI-Erweiterung $wixVersion konnte nicht geladen werden." }
}

if (-not $NoBuild) {
    Write-Host "=== $Configuration bauen ===" -ForegroundColor Green
    & cmd /c "`"$(Join-Path $root 'build.cmd')`" -$(if ($Configuration -eq 'Debug') {'d'} else {'r'})"
    if ($LASTEXITCODE -ne 0) { throw "Build fehlgeschlagen." }
}
if (-not (Test-Path (Join-Path $buildDir 'autofilesync.exe'))) {
    throw "Kein Build in $buildDir - zuerst .\build.cmd ausfuehren oder -NoBuild weglassen."
}

Write-Host '=== Nutzlast zusammenstellen ===' -ForegroundColor Green
Copy-Payload
Test-Payload

Write-Host '=== Paket erzeugen ===' -ForegroundColor Green
Install-WixUiExtension
$version = Get-Version
New-Item -ItemType Directory -Force $Output | Out-Null
$msi = Join-Path $Output "AutoFileSync-$version.msi"
& wix build -arch x64 -culture de-DE -ext WixToolset.UI.wixext `
    -d "PayloadDir=$payloadDir" -d "Version=$(ConvertTo-MsiVersion $version)" -d "IconFile=$icon" `
    -o $msi $wxs
if ($LASTEXITCODE -ne 0) { throw "wix build fehlgeschlagen." }
Remove-Item -Recurse -Force $stagingDir

Write-Host '=== Fertig ===' -ForegroundColor Green
Write-Host "Installationsprogramm: $msi ($([math]::Round((Get-Item $msi).Length / 1MB, 1)) MB)"
