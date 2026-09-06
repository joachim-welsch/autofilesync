<#
.SYNOPSIS
    Baut und startet die GUI-Tests unter Windows - Gegenstueck zu run_GUI_Tests.sh.
.DESCRIPTION
    Uebersetzt die Quellen aus GuiTests.vcxproj direkt mit cl.exe, statt msbuild
    auf das Projekt loszulassen: dessen Google Test kommt aus einem NuGet-Paket,
    das erst von Visual Studio wiederhergestellt werden muss. Die Dateiliste
    stammt trotzdem aus dem Projekt, damit es nur eine Quelle dafuer gibt.
.EXAMPLE
    .\run_GUI_Tests.ps1
.EXAMPLE
    .\run_GUI_Tests.ps1 -Filter *Papierkorb* -NoBuild
#>
param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [string]$Filter = '',
    [switch]$NoBuild,
    [string]$WxDir = 'C:\Entwicklung\extlib\wxWidgets-3.3.1',
    [string]$GTestDir = 'C:\Entwicklung\extlib\vcpkg\packages\gtest_x64-windows',
    [string]$VcpkgDir = 'C:\Entwicklung\extlib\vcpkg\installed\x64-windows',
    [string]$BoostDir = 'C:\Entwicklung\extlib\boost\boost_1_90_0'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$guiDir = Join-Path $root 'AFS_Desktop\test\GUI-Tests'
$buildDir = Join-Path $guiDir "build\windows\$Configuration"
$exe = Join-Path $buildDir 'GuiTests.exe'

$debug = $Configuration -eq 'Debug'
if ($debug) {
    $wxSuffix = 'ud'; $wxSetupDir = 'mswud'
    $gtestLibDir = Join-Path $GTestDir 'debug\lib'
    $vcpkgLibDir = Join-Path $VcpkgDir 'debug\lib'
    $vcpkgBinDir = Join-Path $VcpkgDir 'debug\bin'
    $gtestBinDir = Join-Path $GTestDir 'debug\bin'
} else {
    $wxSuffix = 'u'; $wxSetupDir = 'mswu'
    $gtestLibDir = Join-Path $GTestDir 'lib'
    $vcpkgLibDir = Join-Path $VcpkgDir 'lib'
    $vcpkgBinDir = Join-Path $VcpkgDir 'bin'
    $gtestBinDir = Join-Path $GTestDir 'bin'
}
$wxLibDir = Join-Path $WxDir 'lib\vc_x64_lib'
# llama-cpp/ggml sind hier nicht in den gemergten vcpkg-Installed-Baum
# uebernommen (nur die Produktiv-vcxproj findet sie ueber die globale
# vcpkg-Integration), deshalb direkt aus dem Package-Ordner verlinken.
$vcpkgRoot = Split-Path (Split-Path $VcpkgDir -Parent) -Parent
$llamaPkgDir = Join-Path $vcpkgRoot 'packages\llama-cpp_x64-windows'
$ggmlPkgDir = Join-Path $vcpkgRoot 'packages\ggml_x64-windows'

function Find-VcVars {
    $candidates = @()
    foreach ($base in @("${env:ProgramFiles}\Microsoft Visual Studio", "${env:ProgramFiles(x86)}\Microsoft Visual Studio")) {
        if (Test-Path $base) {
            $candidates += Get-ChildItem $base -Recurse -Filter 'vcvars64.bat' -ErrorAction SilentlyContinue |
                Select-Object -ExpandProperty FullName
        }
    }
    if ($candidates.Count -eq 0) { throw "vcvars64.bat nicht gefunden - Visual Studio mit C++-Werkzeugen noetig." }
    return $candidates[0]
}

function Invoke-InVcEnv([string]$command) {
    $vcvars = Find-VcVars
    # Die Umgebung von vcvars64 gilt nur innerhalb dieser cmd-Sitzung, deshalb
    # laeuft der Befehl gleich mit dahinter statt in einem eigenen Aufruf. Die
    # Meldungen von vcvars gehen ins Leere: es warnt in manchen Installationen
    # ueber ein fehlendes vswhere.exe, arbeitet aber trotzdem korrekt.
    & cmd /c "`"$vcvars`" >nul 2>nul & $command"
    if ($LASTEXITCODE -ne 0) { throw "Fehlgeschlagen (Exit $LASTEXITCODE): $command" }
}

function Get-ProjectSources {
    $projectFile = Join-Path $guiDir 'GuiTests.vcxproj'
    $xml = [xml](Get-Content $projectFile)
    $sources = @()
    foreach ($item in $xml.Project.ItemGroup.ClCompile) {
        if ($item.Include) { $sources += $item.Include }
    }
    if ($sources.Count -eq 0) { throw "Keine Quellen in $projectFile gefunden." }
    return $sources
}

if (-not $NoBuild) {
    # Die Release-Bibliotheken von wxWidgets sind in manchen Baeumen als x86
    # abgelegt; ohne diese Pruefung endet das in ueber tausend offenen Symbolen.
    $wxCheck = Join-Path $wxLibDir "wxbase33$wxSuffix.lib"
    if (-not (Test-Path $wxCheck)) { throw "wxWidgets-Bibliothek fehlt: $wxCheck" }
    if (-not (Test-Path $gtestLibDir)) { throw "Google Test fehlt: $gtestLibDir" }

    New-Item -ItemType Directory -Force $buildDir | Out-Null
    $defines = '_GUITEST', 'wxUSE_HELP', '_WXWIDGETS', 'WIN32', 'X64', '_CONSOLE',
               '_CRT_SECURE_NO_DEPRECATE=1', '_CRT_NON_CONFORMING_SWPRINTFS=1',
               '_SCL_SECURE_NO_WARNINGS=1', '__WXMSW__', '_UNICODE', 'UNICODE', 'NOPCH'
    if ($debug) { $defines += '_DEBUG' } else { $defines += 'NDEBUG' }
    $includes = $guiDir, (Join-Path $root 'AFS_Desktop\include'), (Join-Path $root 'AFS_Core\include'),
                (Join-Path $GTestDir 'include'), (Join-Path $VcpkgDir 'include'), $BoostDir,
                (Join-Path $wxLibDir $wxSetupDir), (Join-Path $WxDir 'include'),
                (Join-Path $llamaPkgDir 'include'), (Join-Path $ggmlPkgDir 'include')

    $clArgs = @('/nologo', '/c', '/EHsc', '/std:c++20', '/MP')
    if ($debug) { $clArgs += '/MDd', '/Zi' } else { $clArgs += '/MD', '/O2' }
    foreach ($d in $defines) { $clArgs += "/D$d" }
    foreach ($i in $includes) { $clArgs += "/I`"$i`"" }
    # Doppelter Abschluss-Backslash: einfach wuerde in der Antwortdatei das
    # schliessende Anfuehrungszeichen entwerten, cl sieht dann eine Datei statt
    # eines Verzeichnisses und lehnt mehrere Quellen ab (D8036).
    $clArgs += "/Fo`"$buildDir\\`"", "/Fd`"$buildDir\vc.pdb`""
    foreach ($s in (Get-ProjectSources)) { $clArgs += "`"$s`"" }

    # Antwortdateien: die Kommandozeile aus ~70 Quellen und ebenso vielen
    # Objekten sprengt das Laengenlimit von cmd.
    $clRsp = Join-Path $buildDir 'cl.rsp'
    Set-Content -Path $clRsp -Value $clArgs -Encoding ascii
    Write-Host "Uebersetze GUI-Tests ($Configuration)..."
    Push-Location $guiDir
    try { Invoke-InVcEnv "cl @`"$clRsp`"" } finally { Pop-Location }

    $wxLibs = "wxmsw33${wxSuffix}_html.lib", "wxmsw33${wxSuffix}_core.lib", "wxmsw33${wxSuffix}_adv.lib",
              "wxbase33${wxSuffix}.lib", "wxtiff$(if ($debug) {'d'}).lib", "wxjpeg$(if ($debug) {'d'}).lib",
              "wxpng$(if ($debug) {'d'}).lib", "wxzlib$(if ($debug) {'d'}).lib",
              "wxregexu$(if ($debug) {'d'}).lib", "wxexpat$(if ($debug) {'d'}).lib"
    $llamaLibDir = Join-Path $llamaPkgDir "$(if ($debug) {'debug\'})lib"
    $ggmlLibDir = Join-Path $ggmlPkgDir "$(if ($debug) {'debug\'})lib"
    $linkArgs = @("/OUT:`"$exe`"", '/SUBSYSTEM:CONSOLE', '/DEBUG',
                  "/LIBPATH:`"$gtestLibDir`"", "/LIBPATH:`"$vcpkgLibDir`"", "/LIBPATH:`"$wxLibDir`"",
                  "/LIBPATH:`"$llamaLibDir`"", "/LIBPATH:`"$ggmlLibDir`"")
    foreach ($o in (Get-ChildItem "$buildDir\*.obj")) { $linkArgs += "`"$($o.FullName)`"" }
    $linkArgs += 'gtest.lib', 'libssh2.lib', 'libssl.lib', 'libcrypto.lib', 'ws2_32.lib', 'crypt32.lib'
    $linkArgs += 'llama.lib', 'ggml.lib', 'ggml-cpu.lib', 'ggml-base.lib'
    $linkArgs += $wxLibs
    $linkArgs += 'kernel32.lib', 'user32.lib', 'gdi32.lib', 'comdlg32.lib', 'winspool.lib', 'winmm.lib',
                 'shell32.lib', 'shlwapi.lib', 'comctl32.lib', 'ole32.lib', 'oleaut32.lib', 'uuid.lib',
                 'rpcrt4.lib', 'advapi32.lib', 'version.lib', 'wsock32.lib', 'wininet.lib'
    $linkRsp = Join-Path $buildDir 'link.rsp'
    Set-Content -Path $linkRsp -Value $linkArgs -Encoding ascii
    Write-Host "Linke $exe..."
    Invoke-InVcEnv "link /nologo @`"$linkRsp`""
}

if (-not (Test-Path $exe)) { throw "Testprogramm nicht gefunden: $exe" }

# libssh2, OpenSSL, llama und ggml kommen als DLL aus dem vcpkg-Baum.
$llamaBinDir = Join-Path $llamaPkgDir "$(if ($debug) {'debug\'})bin"
$ggmlBinDir = Join-Path $ggmlPkgDir "$(if ($debug) {'debug\'})bin"
$env:PATH = "$vcpkgBinDir;$gtestBinDir;$llamaBinDir;$ggmlBinDir;$env:PATH"
Write-Host ''
Write-Host 'GUI-Tests laufen auf dem aktiven Desktop - bitte Maus und Tastatur nicht benutzen.' -ForegroundColor Yellow
$testArgs = @()
if ($Filter) { $testArgs += "--gtest_filter=$Filter" }
& $exe @testArgs
exit $LASTEXITCODE
