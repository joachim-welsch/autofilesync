<#
.SYNOPSIS
    Baut das Windows-Installationspaket AutoFileSync-<Version>.msi.
.DESCRIPTION
    Gegenstueck zu makeMacOSInstaller.sh: baut die Release-Version von Programm
    und Hintergrunddienst, stellt die Nutzlast in einem eigenen Verzeichnis
    zusammen, prueft sie und laesst WiX daraus ein MSI erzeugen. Geprueft wird
    vor dem Paketieren, weil ein MSI ohne Katalog oder ohne eine der
    Laufzeit-DLLs erst beim Anwender auffaellt.
.EXAMPLE
    .\makeWindowsInstaller.ps1
.EXAMPLE
    .\makeWindowsInstaller.ps1 -NoBuild -Output C:\Temp
.EXAMPLE
    .\makeWindowsInstaller.ps1 -DevFirewallRules
    Paket fuer den eigenen Rechner: oeffnet die Firewall zusaetzlich fuer die
    Programme im Build-Verzeichnis (Release und Debug), damit ein aus Visual
    Studio gestartetes Programm das Handy ohne Firewall-Abfrage erreicht.
.NOTES
    Signiert wird mit dem Zertifikat aus AFS_SIGN_THUMBPRINT (Speicher CurrentUser\My)
    oder AFS_SIGN_PFX + AFS_SIGN_PFX_PASSWORD. Ohne Zertifikat bricht das Skript ab;
    -NoSigning baut bewusst ein unsigniertes Paket (Entwicklerbuild).
#>
param(
    [ValidateSet('Release', 'Debug')][string]$Configuration = 'Release',
    [switch]$NoBuild,
    [switch]$NoSigning,
    [switch]$DevFirewallRules,
    [string]$Output
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$docDir = Join-Path $root 'doc'
$buildDir = Join-Path $root "AFS_Desktop\x64\$Configuration"
$serviceBuildDir = Join-Path $root "backgroundsync\x64\$Configuration"
$stagingDir = Join-Path $root 'build\windows_installer'
$payloadDir = Join-Path $stagingDir 'payload'
$serviceDir = Join-Path $stagingDir 'service'
$wxs = Join-Path $root 'AFS_Desktop\installer\autofilesync.wxs'
$icon = Join-Path $root 'AFS_Desktop\icons\afs.ico'
$modelSetup = 'setup_llama_windows.ps1', 'setup_llama_windows.cmd'
if (-not $Output) { $Output = Join-Path $root 'build' }

# Version.h ist die einzige Quelle der Versionsnummer - wie afs_version() unter macOS.
function Get-Version {
    $header = Join-Path $root 'AFS_Desktop\include\Version.h'
    $match = Select-String -Path $header -Pattern 'afsVERSION_TEXT\s+"([^"]+)"'
    if (-not $match) { throw "Version nicht aus $header lesbar." }
    return $match.Matches[0].Groups[1].Value
}

# Windows Installer vergleicht Versionsfelder als Zahlen und lehnt fuehrende
# Nullen ab; ein viertes Feld wertet er beim Upgrade-Vergleich ohnehin nicht
# aus. Fuer "major.minor.patch" ist das ein Durchreichen.
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
    foreach ($script in $modelSetup) { Copy-Item (Join-Path $PSScriptRoot $script) $payloadDir }
    Copy-Licenses
}

# Der Hintergrunddienst liegt neben der Nutzlast, nicht darin: die Nutzlast wird
# als Ganzes eingesammelt, die Dienstdatei aber braucht in der Produktdefinition
# ihre eigene Komponente mit der Dienstregistrierung. Er teilt sich die DLLs mit
# dem Programm, weil beide in dasselbe Verzeichnis installiert werden.
function Copy-Service {
    if (Test-Path $serviceDir) { Remove-Item -Recurse -Force $serviceDir }
    New-Item -ItemType Directory -Force $serviceDir | Out-Null
    Copy-Item (Join-Path $serviceBuildDir 'backgroundsync.exe') $serviceDir
}

# Die Lizenztexte gehoeren zu jedem Paket: LICENSE ist die GPL, LICENSING.md
# erklaert sie, dazu die Hinweise zu Komponenten Dritter samt ihrer Texte aus
# third-party\.
function Copy-Licenses {
    $target = Join-Path $payloadDir 'licenses'
    New-Item -ItemType Directory -Force $target | Out-Null
    Copy-Item (Join-Path $root 'LICENSE') $target
    foreach ($file in 'LICENSING.md', 'THIRD-PARTY-NOTICES.md', 'CHANGELOG.md') {
        Copy-Item (Join-Path $docDir $file) $target
    }
    Copy-Item -Recurse (Join-Path $root 'third-party') (Join-Path $target 'third-party')
}

# Der Lizenzdialog von WiX zeigt RTF. Aus LICENSING.md und der GPL wird es hier
# erzeugt, damit der Text im Installationsprogramm derselbe ist wie im Repo;
# Zeichen ausserhalb von ASCII gehen als \u-Escapes hinein, sonst zerlegt der
# RTF-Leser die Umlaute.
# LICENSING.md is Markdown; the dialog shows plain text, so the markers a reader
# would stumble over go: heading marks, code ticks, emphasis stars and the angle
# brackets around links.
function ConvertFrom-MarkdownLine([string]$line) {
    $line = $line -replace '^#+\s*', ''
    $line = $line -replace '`', ''
    $line = $line -replace '\*\*', ''
    $line = $line -replace '^\*(.*)\*$', '$1'
    return $line -replace '<(https?://[^>]+)>', '$1'
}

function ConvertTo-Rtf([string[]]$paragraphs) {
    $builder = New-Object System.Text.StringBuilder
    [void]$builder.Append('{\rtf1\ansi\deff0{\fonttbl{\f0\fnil Segoe UI;}}\f0\fs18 ')
    foreach ($raw in $paragraphs) {
        $line = ConvertFrom-MarkdownLine $raw
        foreach ($char in $line.ToCharArray()) {
            $code = [int]$char
            if ($char -eq '\' -or $char -eq '{' -or $char -eq '}') { [void]$builder.Append('\' + $char) }
            elseif ($code -gt 127) { [void]$builder.Append('\u' + $code + '?') }
            else { [void]$builder.Append($char) }
        }
        [void]$builder.Append('\par ')
    }
    [void]$builder.Append('}')
    return $builder.ToString()
}

function Write-LicenseRtf {
    $text = @(Get-Content (Join-Path $docDir 'LICENSING.md') -Encoding UTF8) + @('', '') + @(Get-Content (Join-Path $root 'LICENSE') -Encoding UTF8)
    $rtf = Join-Path $payloadDir 'license.rtf'
    [System.IO.File]::WriteAllText($rtf, (ConvertTo-Rtf $text), [System.Text.Encoding]::ASCII)
    return $rtf
}

# Die Laufzeit-DLLs stehen hier namentlich, damit eine fehlende auffaellt statt
# mitzufehlen: intl-8 und iconv-2 haben genau so einmal gefehlt, und die Exe
# startet ohne sie gar nicht erst. Ohne die Setup-Skripte zeigt der
# Startmenue-Eintrag fuer das Chat-Modell ins Leere.
function Test-Payload {
    $required = @(
        'autofilesync.exe',
        'intl-8.dll', 'iconv-2.dll', 'libssh2.dll', 'libcurl.dll', 'zlib1.dll',
        'libssl-3-x64.dll', 'libcrypto-3-x64.dll',
        'llama.dll', 'ggml.dll', 'ggml-base.dll', 'ggml-cpu.dll',
        'help\help.htb', 'licenses\LICENSE', 'licenses\LICENSING.md', 'licenses\THIRD-PARTY-NOTICES.md'
    ) + $modelSetup
    foreach ($languageDir in (Get-ChildItem (Join-Path $root 'AFS_Desktop\help') -Directory)) {
        if (Test-Path (Join-Path $languageDir.FullName 'help.hhp')) { $required += "help\$($languageDir.Name)\help.htb" }
    }
    foreach ($language in (Get-ChildItem (Join-Path $root 'po') -Filter '*.po')) {
        $required += "locale\$($language.BaseName)\LC_MESSAGES\autofilesync.mo"
    }
    $missing = $required | Where-Object { -not (Test-Path (Join-Path $payloadDir $_)) }
    if (-not (Test-Path (Join-Path $serviceDir 'backgroundsync.exe'))) { $missing += 'backgroundsync.exe' }
    if ($missing) { throw "Nutzlast unvollstaendig: $($missing -join ', ')" }
    Write-Host "Nutzlast vollstaendig ($($required.Count + 1) geforderte Eintraege)." -ForegroundColor Green
}

# Signiert werden die Programmdatei in der Nutzlast und das fertige MSI, mit
# Zeitstempel, damit die Signatur das Zertifikat ueberlebt. Das Zertifikat kommt
# aus dem Zertifikatspeicher (AFS_SIGN_THUMBPRINT) oder einer PFX-Datei
# (AFS_SIGN_PFX, AFS_SIGN_PFX_PASSWORD). Ohne Zertifikat wird abgebrochen, bevor
# gebaut wird: eine Warnung im Protokoll geht unter, und ein unsigniertes Paket
# soll nur entstehen, wenn es mit -NoSigning verlangt ist (Entwicklerbuild).
$timestampServer = 'http://timestamp.digicert.com'

function Find-SignTool {
    $candidates = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x64\signtool.exe" -ErrorAction SilentlyContinue |
        Sort-Object FullName
    if ($candidates) { return $candidates[-1].FullName }
    $onPath = Get-Command signtool -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }
    throw 'signtool.exe nicht gefunden - das Windows SDK wird gebraucht.'
}

function Get-SigningArguments {
    if ($env:AFS_SIGN_THUMBPRINT) { return @('/sha1', $env:AFS_SIGN_THUMBPRINT) }
    if ($env:AFS_SIGN_PFX) {
        $arguments = @('/f', $env:AFS_SIGN_PFX)
        if ($env:AFS_SIGN_PFX_PASSWORD) { $arguments += @('/p', $env:AFS_SIGN_PFX_PASSWORD) }
        return $arguments
    }
    return $null
}

if (-not $NoSigning -and -not (Get-SigningArguments)) {
    throw 'Kein Signaturzertifikat: AFS_SIGN_THUMBPRINT oder AFS_SIGN_PFX setzen, oder mit -NoSigning bewusst ein unsigniertes Paket bauen.'
}

function Invoke-Sign([string]$file) {
    $certificate = Get-SigningArguments
    if ($NoSigning) {
        Write-Warning "Nicht signiert (-NoSigning): $(Split-Path -Leaf $file)"
        return
    }
    $signtool = Find-SignTool
    & $signtool sign /fd sha256 /td sha256 /tr $timestampServer /d 'AutoFileSync' /du 'https://welsch-software.com' @certificate $file
    if ($LASTEXITCODE -ne 0) { throw "Signieren fehlgeschlagen: $file" }
    Write-Host "Signiert: $(Split-Path -Leaf $file)" -ForegroundColor Green
}

# Die Erweiterungen werden global abgelegt, nicht als .wix-Verzeichnis im Repo:
# UI fuer die Dialoge, Util fuer das Anmelderecht des Dienstkontos. Ihre Version
# muss die von wix selbst sein: ohne Angabe laedt "extension add" die neueste
# (derzeit 7.x), und wix 5 kann die nicht laden.
$wixExtensions = 'WixToolset.UI.wixext', 'WixToolset.Util.wixext', 'WixToolset.Firewall.wixext'

function Install-WixExtensions {
    if (-not (Get-Command wix -ErrorAction SilentlyContinue)) {
        throw "wix nicht gefunden - installieren mit: dotnet tool install --global wix"
    }
    $wixVersion = (& wix --version).Split('+')[0]
    $installed = (& wix extension list -g) -join "`n"
    foreach ($extension in $wixExtensions) {
        if ($installed -match "$([regex]::Escape($extension))\s+$([regex]::Escape($wixVersion))") { continue }
        Write-Host "WiX-Erweiterung $extension $wixVersion wird nachgeladen..."
        & wix extension add -g "$extension/$wixVersion"
        if ($LASTEXITCODE -ne 0) { throw "WiX-Erweiterung $extension $wixVersion konnte nicht geladen werden." }
    }
}

if (-not $NoBuild) {
    Write-Host "=== $Configuration bauen ===" -ForegroundColor Green
    & cmd /c "`"$(Join-Path $PSScriptRoot 'build.cmd')`" -$(if ($Configuration -eq 'Debug') {'d'} else {'r'})"
    if ($LASTEXITCODE -ne 0) { throw "Build fehlgeschlagen." }
}
foreach ($built in (Join-Path $buildDir 'autofilesync.exe'), (Join-Path $serviceBuildDir 'backgroundsync.exe')) {
    if (-not (Test-Path $built)) { throw "Kein Build: $built fehlt - zuerst scripts\build.cmd ausfuehren oder -NoBuild weglassen." }
}

Write-Host '=== Nutzlast zusammenstellen ===' -ForegroundColor Green
Copy-Payload
Copy-Service
Test-Payload
$licenseRtf = Write-LicenseRtf
Invoke-Sign (Join-Path $payloadDir 'autofilesync.exe')
Invoke-Sign (Join-Path $serviceDir 'backgroundsync.exe')

Write-Host '=== Paket erzeugen ===' -ForegroundColor Green
Install-WixExtensions
$version = Get-Version
New-Item -ItemType Directory -Force $Output | Out-Null
$msi = Join-Path $Output "AutoFileSync-$version.msi"
$extensionArguments = $wixExtensions | ForEach-Object { '-ext', $_ }
$defineArguments = @()
if ($DevFirewallRules) { $defineArguments = '-d', "DevBuildRoot=$root" }
& wix build -arch x64 -culture de-DE @extensionArguments @defineArguments `
    -d "PayloadDir=$payloadDir" -d "ServiceDir=$serviceDir" -d "Version=$(ConvertTo-MsiVersion $version)" -d "IconFile=$icon" -d "LicenseRtf=$licenseRtf" `
    -o $msi $wxs
if ($LASTEXITCODE -ne 0) { throw "wix build fehlgeschlagen." }
Invoke-Sign $msi
Remove-Item -Recurse -Force $stagingDir

Write-Host '=== Fertig ===' -ForegroundColor Green
Write-Host "Installationsprogramm: $msi ($([math]::Round((Get-Item $msi).Length / 1MB, 1)) MB)"
