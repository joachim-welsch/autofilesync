<#
.SYNOPSIS
    Packt die Hilfebuecher AFS_Desktop\help\help.htb (Deutsch) und
    AFS_Desktop\help\<Sprache>\help.htb (jede andere Sprache) aus den Seiten neu.
.DESCRIPTION
    help.htb ist ein ZIP mit den Seiten aus dem [FILES]-Abschnitt von help.hhp
    sowie Inhaltsverzeichnis, Index, Projektdatei und Kontext-Zuordnung. Das
    Programm laedt nur das Buch, nicht die einzelnen Seiten - eine geaenderte
    Seite ist also erst nach dem Packen sichtbar. Der CMake-Build packt das
    Buch selbst (AFS_Desktop/CMakeLists.txt); build.cmd ruft dieses Skript vor
    dem MSBuild-Bau auf, es laesst sich auch einzeln starten.
#>
$ErrorActionPreference = 'Stop'
$helpRoot = Join-Path (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)) 'AFS_Desktop\help'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

# Ein Buch je Sprache: die deutschen Seiten in help, jede andere Sprache in help\<Sprache>.
function Pack-HelpBook([string]$helpDir) {
    $project = Get-Content (Join-Path $helpDir 'help.hhp')
    $inFiles = $false
    $files = @()
    foreach ($line in $project) {
        if ($line -match '^\[FILES\]') { $inFiles = $true; continue }
        if ($line -match '^\[') { $inFiles = $false; continue }
        if ($inFiles -and $line.Trim() -ne '') { $files += $line.Trim() }
    }
    $files += 'contents.hhc', 'index.hhk', 'help.hhp', 'wxhelp.map'
    $book = Join-Path $helpDir 'help.htb'
    $tmp = "$book.tmp"
    if (Test-Path $tmp) { Remove-Item $tmp }
    $zip = [System.IO.Compression.ZipFile]::Open($tmp, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($name in $files) {
            $path = Join-Path $helpDir $name
            if (-not (Test-Path $path)) { throw "Hilfeseite fehlt: $path" }
            [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $path, $name, [System.IO.Compression.CompressionLevel]::Optimal) | Out-Null
        }
    } finally {
        $zip.Dispose()
    }
    Move-Item -Force $tmp $book
    Write-Host "Hilfebuch gepackt: $book ($($files.Count) Dateien)"
}

Pack-HelpBook $helpRoot
foreach ($languageDir in Get-ChildItem $helpRoot -Directory) {
    if (Test-Path (Join-Path $languageDir.FullName 'help.hhp')) { Pack-HelpBook $languageDir.FullName }
}
