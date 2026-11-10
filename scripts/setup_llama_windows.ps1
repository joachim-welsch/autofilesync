<#
.SYNOPSIS
    Richtet das Modell fuer den Chat-Assistenten von AutoFileSync ein (Windows).
.DESCRIPTION
    Gegenstueck zu setup_llama_macos.sh. Laedt ein zum Arbeitsspeicher passendes
    GGUF-Modell nach %LOCALAPPDATA%\autofilesync\models - dort findet
    AutoFileSync es von selbst, ohne Umgebungsvariable und ohne Neustart.
    Mit -Model wird eine vorhandene Datei verwendet; weil die woanders liegt,
    setzt das Skript dann AUTOFILESYNC_LLAMA_MODEL fuer den Benutzer.
.EXAMPLE
    .\setup_llama_windows.ps1
.EXAMPLE
    .\setup_llama_windows.ps1 -Size 3b
.EXAMPLE
    .\setup_llama_windows.ps1 -Model D:\Modelle\qwen.gguf
#>
param(
    [ValidateSet('3b', '7b', '14b')][string]$Size,
    [string]$Model,
    [switch]$NoVerify,
    [switch]$Pause
)

$ErrorActionPreference = 'Stop'
$variable = 'AUTOFILESYNC_LLAMA_MODEL'
$modelDir = Join-Path $env:LOCALAPPDATA 'autofilesync\models'
$modelNames = @{
    '3b'  = 'Qwen2.5-Coder-3B-Instruct'  # Qwen Research License: not for commercial use, see THIRD-PARTY-NOTICES.md
    '7b'  = 'Qwen2.5-Coder-7B-Instruct'
    '14b' = 'Qwen2.5-Coder-14B-Instruct'
}
# Geladen wird ein festgelegter Stand des Repositorys und geprueft gegen den
# SHA-256 seiner Datei: "main" kann sich aendern, und ein Modell wird vom
# Programm geladen und ausgefuehrt. Wer ein neueres Modell will, traegt hier
# Stand und Pruefsumme nach (Dateiliste des Repositorys, Spalte "SHA256").
$modelSources = @{
    'Qwen2.5-Coder-3B-Instruct'  = @{ Revision = '7c137640ef0332dfedb229f2504c58d83ed4307a'; Sha256 = '3da3afe6cf5c674ac195803ea0dd6fee7e1c228c2105c1ce8c66890d1d4ab460' }
    'Qwen2.5-Coder-7B-Instruct'  = @{ Revision = '1f629da0c8bed16b9e50cee91c70693650e66c35'; Sha256 = '1664fccab734674a50763490a8c6931b70e3f2f8ec10031b54806d30e5f956b6' }
    'Qwen2.5-Coder-14B-Instruct' = @{ Revision = '5b379ec4bf71bafecb5f9081ad28b19939128988'; Sha256 = '2946d28c9e1bb2bcae6d42e8678863a31775df6f740315c7d7e6d6b6411f5937' }
}

# LlamaClient rechnet mit n_gpu_layers = 0 auf der CPU: ein groesseres Modell
# kostet Antwortzeit, nicht nur Speicher. Gezaehlt werden die eingebauten
# Module, denn TotalPhysicalMemory liegt um den reservierten Teil darunter und
# stuft 16 GB sonst als 15 GB ein. Virtuelle Maschinen melden oft keine Module.
function Get-DefaultSize {
    $bytes = (Get-CimInstance Win32_PhysicalMemory | Measure-Object Capacity -Sum).Sum
    if (-not $bytes) { $bytes = (Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory }
    $ramGb = [math]::Round($bytes / 1GB)
    Write-Host "Arbeitsspeicher: $ramGb GB"
    if ($ramGb -ge 32) { return '14b' }
    if ($ramGb -ge 16) { return '7b' }
    return '3b'
}

# curl.exe gehoert seit Windows 10 1803 zum System und setzt einen abgebrochenen
# Download fort; Invoke-WebRequest ist unter PowerShell 5.1 bei mehreren GB
# wegen seiner Fortschrittsanzeige quaelend langsam. Geladen wird nach .part,
# damit AutoFileSync keine halbe Datei als Modell findet.
function Save-Model([string]$name) {
    $file = "$name-Q4_K_M.gguf"
    $target = Join-Path $modelDir $file
    if (Test-Path $target) {
        Write-Host "Modell bereits vorhanden: $target" -ForegroundColor Green
        return $target
    }
    New-Item -ItemType Directory -Force $modelDir | Out-Null
    Write-Host "Lade $file herunter..." -ForegroundColor Yellow
    $source = $modelSources[$name]
    & curl.exe -L -C - --fail --progress-bar -o "$target.part" "https://huggingface.co/bartowski/$name-GGUF/resolve/$($source.Revision)/$file"
    if ($LASTEXITCODE -ne 0) { throw "Download fehlgeschlagen (curl: $LASTEXITCODE). Ein erneuter Aufruf setzt ihn fort." }
    if (-not $NoVerify) {
        Write-Host 'Pruefe die Pruefsumme...'
        $hash = (Get-FileHash -Algorithm SHA256 "$target.part").Hash
        if ($hash -ne $source.Sha256) {
            Remove-Item "$target.part"
            throw "Die geladene Datei hat nicht die erwartete Pruefsumme und wurde verworfen (erwartet $($source.Sha256), erhalten $hash)."
        }
    }
    Move-Item "$target.part" $target
    return $target
}

# Ein Ladeversuch kostete auf der CPU Minuten; geprueft wird deshalb, was in der
# Praxis schiefgeht: ein abgeschnittener Download oder eine HTML-Fehlerseite.
function Test-Model([string]$path) {
    $stream = [System.IO.File]::OpenRead($path)
    try {
        $magic = New-Object byte[] 4
        $read = $stream.Read($magic, 0, 4)
        $length = $stream.Length
    } finally { $stream.Dispose() }
    if ($read -ne 4 -or [System.Text.Encoding]::ASCII.GetString($magic) -ne 'GGUF') { throw "Keine GGUF-Datei: $path" }
    if ($length -lt 1GB) { throw "Modell wirkt abgeschnitten ($length Bytes): $path" }
    Write-Host "Modell geprueft ($([math]::Round($length / 1GB, 1)) GB)" -ForegroundColor Green
}

# Die Variable geht dem Modellverzeichnis vor; eine alte wuerde das gerade
# geladene Modell verdecken.
function Clear-ModelVariable {
    if ([Environment]::GetEnvironmentVariable($variable, 'User')) {
        [Environment]::SetEnvironmentVariable($variable, $null, 'User')
        Write-Host "$variable entfernt, sie haette das Modell verdeckt. Ein laufendes AutoFileSync muss neu gestartet werden." -ForegroundColor Yellow
    }
    if ([Environment]::GetEnvironmentVariable($variable, 'Machine')) {
        Write-Warning "$variable ist systemweit gesetzt und verdeckt das Modell. Entfernen mit Administratorrechten."
    }
}

function Install-DownloadedModel {
    $selected = if ($Size) { $Size } else { Get-DefaultSize }
    $path = Save-Model $modelNames[$selected]
    if (-not $NoVerify) {
        try { Test-Model $path } catch { Remove-Item $path; throw }
    }
    Clear-ModelVariable
    Write-Host "Fertig. Der Chat-Assistent verwendet ab sofort $path" -ForegroundColor Green
}

function Install-ExistingModel {
    if (-not (Test-Path -PathType Leaf $Model)) { throw "Datei nicht gefunden: $Model" }
    $path = (Resolve-Path $Model).ProviderPath
    if (-not $NoVerify) { Test-Model $path }
    [Environment]::SetEnvironmentVariable($variable, $path, 'User')
    Write-Host "Fertig. $variable = $path" -ForegroundColor Green
    Write-Host 'Ein laufendes AutoFileSync muss neu gestartet werden.' -ForegroundColor Yellow
}

$failed = $false
try {
    if ($Model) { Install-ExistingModel } else { Install-DownloadedModel }
} catch {
    Write-Host "Fehler: $($_.Exception.Message)" -ForegroundColor Red
    $failed = $true
} finally {
    # Aus dem Startmenue gestartet schliesst sich das Fenster sonst, bevor
    # jemand Ergebnis oder Fehler lesen konnte.
    if ($Pause) { Read-Host 'Eingabetaste schliesst das Fenster' | Out-Null }
}
if ($failed) { exit 1 }
