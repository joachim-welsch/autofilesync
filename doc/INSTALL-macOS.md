# AutoFileSync – Installation unter macOS

Diese Anleitung gilt für Apple Silicon (arm64) mit Homebrew unter
`/opt/homebrew`. Auf Intel-Macs liegt Homebrew unter `/usr/local`; die Pfade
sind entsprechend anzupassen.

## Übersicht: was wohin installiert wird

| Inhalt | Ziel |
| --- | --- |
| Programmbündel | `/Applications/AutoFileSync.app` |
| Chat-Modell (GGUF) | `~/Library/Application Support/autofilesync/models/` |
| LaunchAgent für die Modellvariable | `~/Library/LaunchAgents/com.autofilesync.llamamodel.plist` |
| LaunchAgent des Hintergrunddienstes | `~/Library/LaunchAgents/com.autofilesync.backgroundsync.plist` |

## 1. Installation aus dem Disk-Image

Wer ein fertiges `AutoFileSync-<Version>.dmg` erhalten hat, braucht weder
Homebrew noch einen Übersetzer: Alle benötigten Bibliotheken und das Hilfebuch
stecken im Programmbündel.

1. Das Disk-Image mit einem Doppelklick öffnen.
2. Im erscheinenden Fenster das Symbol **AutoFileSync** auf **Applications**
   ziehen.
3. Das Disk-Image auswerfen.

### Beim ersten Start

Das Bündel ist nur ad hoc signiert und nicht bei Apple notarisiert. Ein aus dem
Netz geladenes Image trägt deshalb die Quarantäne-Markierung, und macOS
verweigert den ersten Start mit dem Hinweis auf einen nicht verifizierten
Entwickler. Abhilfe: das Programm einmal starten, die Meldung wegklicken und
unter Systemeinstellungen → Datenschutz & Sicherheit → *Dennoch öffnen* den
Start freigeben. Danach startet AutoFileSync wie jedes andere Programm.

Ein mit einer Developer-ID signiertes und notarisiertes Image zeigt diese
Rückfrage nicht (siehe Abschnitt 3).

## 2. Voraussetzungen zum Selbstübersetzen

```bash
brew install wxwidgets@3.2 libssh boost openssl cmake
```

Der Build klont beim Konfigurieren zusätzlich llama.cpp (siehe
`AFS_Core/cmake/AfsCore.cmake`), braucht also eine funktionierende Internetverbindung.

## 3. Übersetzen, installieren und weitergeben

Beide Skripte teilen sich die Schritte in `AFS_Desktop/installer/macos_bundle.sh`: Release
bauen, Hilfebuch und Symbol in die Bündelressourcen legen, die Version aus
`AFS_Desktop/include/Version.h` in die `Info.plist` schreiben, alle Bibliotheken aus
Homebrew einbetten, signieren und prüfen, dass das Bündel danach nichts mehr
außerhalb seiner selbst lädt.

### Auf dem eigenen Rechner installieren

```bash
scripts/installMacOS.sh
```

Legt das fertige Bündel nach `/Applications/AutoFileSync.app`. Mit
`--no-install` entsteht es nur unter `build/Release`.

### Ein Disk-Image zum Weitergeben erzeugen

```bash
scripts/makeMacOSInstaller.sh
```

Erzeugt `build/AutoFileSync-<Version>.dmg` mit dem Programmbündel, einer
Verknüpfung auf den Programme-Ordner und dem Hintergrundbild aus
`AFS_Desktop/installer/`.

| Option | Wirkung |
| --- | --- |
| `--no-build` | Vorhandenes Bündel aus `build/Release` verwenden |
| `--output DIR` | Anderer Ablageort für das Disk-Image |
| `--require-layout` | Abbrechen, statt ein Image ohne Fensterlayout auszuliefern |

Das Fensterlayout – Symbolpositionen, Fenstergröße und Hintergrundbild – kann
nur der Finder schreiben. Beim ersten Lauf fragt macOS deshalb nach der
Erlaubnis, den Finder zu steuern; ohne sie entsteht ein brauchbares, aber
unformatiertes Image. Nachträglich erteilt man sie unter Systemeinstellungen →
Datenschutz & Sicherheit → Automation beim aufrufenden Programm (Terminal) für
„Finder". Für Release-Images verhindert `--require-layout`, dass eine fehlende
Erlaubnis unbemerkt ein unformatiertes Image erzeugt.

### Ohne Gatekeeper-Rückfrage weitergeben

```bash
AFS_CODESIGN_IDENTITY="Developer ID Application: <Name> (<TeamID>)" scripts/makeMacOSInstaller.sh
```

Signiert Bündel und Image mit der angegebenen Identität. Für einen Start ohne
jede Rückfrage muss das Image anschließend noch von Apple notarisiert werden
(`xcrun notarytool submit`).

### Das Installationsprogramm prüfen

```bash
scripts/test_macOS_installer.sh
```

Hängt das neueste Image aus `build/` ein und prüft es vollständig: Aufbau und
Layout des Images, Angaben und Inhalt des Bündels, dass keine Bibliothek
doppelt oder von außerhalb geladen wird, dass eine durch Ziehen erzeugte
Installation nach dem Auswerfen weiterläuft und dass ein Probestart die
Konfiguration des Benutzers unberührt lässt. Der Probestart läuft mit einem
eigenen `HOME`; `--no-launch` überspringt ihn.

## 4. Hintergrunddienst

Der Daemon `backgroundsync` synchronisiert mit derselben Konfiguration weiter,
während das GUI-Programm nicht läuft. Er steckt im Programmbündel neben dem
GUI-Programm und wird mit jedem Update des Bündels mit aktualisiert. Unter macOS
steuert AutoFileSync ihn über launchd – das Gegenstück zur systemd-Unit unter
Linux – als LaunchAgent der angemeldeten Person, weil er deren Verzeichnisse mit
deren Zugangsdaten abgleicht.

| | |
| --- | --- |
| Daemon | `/Applications/AutoFileSync.app/Contents/MacOS/backgroundsync` |
| Label | `com.autofilesync.backgroundsync` |
| LaunchAgent | `~/Library/LaunchAgents/com.autofilesync.backgroundsync.plist` |

Einzurichten ist nichts: Das Menü **Hintergrunddienst** nennt den Zustand in
seiner ersten Zeile und gibt jeweils nur den Befehl frei, der dazu passt:

| Erste Zeile | Bedeutung | Freigegeben |
| --- | --- | --- |
| Zustand: läuft | Der Agent ist registriert und sein Prozess lebt | Stop |
| Zustand: gestoppt | Der Daemon liegt im Bündel, läuft aber nicht | Start |
| Zustand: nicht installiert | Das Bündel enthält keinen Daemon (Entwicklungsstand, der nur das GUI gebaut hat) | keiner |

*Start* schreibt den LaunchAgent mit dem Pfad des Daemons im Bündel, registriert
ihn (`launchctl bootstrap`) und startet ihn (`launchctl kickstart`) – der zweite
Schritt ist nötig, weil ein Daemon, der sich selbst beendet hat, registriert
bleibt. Ab dann startet der Daemon bei jeder Anmeldung von selbst. Ein
LaunchAgent, der noch auf einen anderen Pfad zeigt, etwa auf eine früher von
Hand installierte Kopie, wird dabei ersetzt. *Stop* hebt die Registrierung auf
(`launchctl bootout`) und entfernt den LaunchAgent; den Prozess nur abzuschießen
genügt nicht, weil launchd einen Agenten mit `KeepAlive` sofort neu startet.

### Entfernen

*Stop* im Menü, dann das Programmbündel in den Papierkorb. Mehr hinterlässt der
Dienst nicht. Eine früher von Hand installierte Kopie unter
`~/Library/Application Support/autofilesync/backgroundsync` kann gelöscht werden.

### Festplattenvollzugriff nicht vergessen

macOS lässt einen Hintergrundprozess nicht ungefragt an Dokumente, Schreibtisch,
Downloads oder externe Volumes. Der Daemon hat keine Oberfläche und kann den
Zustimmungsdialog nicht beantworten – **genau diese Synchronisationen schlagen
sonst still fehl**, während das Menü einen laufenden Dienst zeigt.

Systemeinstellungen → Datenschutz & Sicherheit → Festplattenvollzugriff →
`/Applications/AutoFileSync.app/Contents/MacOS/backgroundsync` hinzufügen (im
Dateidialog mit Befehl+Umschalt+G den Pfad eingeben).

Den Zugriff am besten vor dem ersten Start erteilen.

## 5. Chat-Assistent (optional)

Der eingebaute Assistent rechnet lokal über llama.cpp und benötigt eine
Modelldatei im GGUF-Format. Das Programm nimmt die Datei aus der
Umgebungsvariablen `AUTOFILESYNC_LLAMA_MODEL`, falls gesetzt, sonst die dem
Namen nach erste `*.gguf` in
`~/Library/Application Support/autofilesync/models/` – dort auch ohne Neustart.

```bash
scripts/setup_llama_macos.sh
```

Das Skript lädt ein zur installierten Arbeitsspeichergröße passendes Modell
herunter, legt es unter `~/Library/Application Support/autofilesync/models/`
ab und veröffentlicht die Variable. Optionen:

| Option | Wirkung |
| --- | --- |
| `--size 3b\|7b\|14b` | Modellgröße erzwingen |
| `--model DATEI` | Vorhandene GGUF-Datei verwenden, nichts herunterladen |
| `--no-verify` | Prüfung der heruntergeladenen Datei überspringen |

Ein bereits laufendes AutoFileSync übernimmt die Variable nicht mehr; das
Programm ist nach der Einrichtung neu zu starten.

### Warum die Variable an drei Stellen gesetzt wird

Ein aus dem Finder oder Dock gestartetes `.app`-Bündel erbt die Umgebung des
Anmeldevorgangs, nicht die einer Shell. Ein `export` in `~/.zshrc` wirkt daher
nur beim Start aus dem Terminal. Das Skript setzt die Variable deshalb
zusätzlich per `launchctl setenv` für die laufende Anmeldesitzung und legt
einen LaunchAgent ab, der sie bei jeder weiteren Anmeldung wieder setzt.

### Modellgröße

`LlamaClient` lädt das Modell mit `n_gpu_layers = 0` und rechnet damit
ausschließlich auf der CPU – unabhängig davon, ob der Mac eine leistungsfähige
GPU besitzt. Eine größere Modelldatei verlängert deshalb unmittelbar die
Antwortzeit im Chatfenster. Der Standard ist entsprechend zurückhaltend
gewählt:

| Arbeitsspeicher | Modell |
| --- | --- |
| ab 32 GB | Qwen2.5-Coder-14B (Q4_K_M) |
| ab 16 GB | Qwen2.5-Coder-7B (Q4_K_M) |
| darunter | Qwen2.5-Coder-3B (Q4_K_M) |

### Modell von Hand einrichten

```bash
mkdir -p ~/Library/Application\ Support/autofilesync/models
cp <modell>.gguf ~/Library/Application\ Support/autofilesync/models/
launchctl setenv AUTOFILESYNC_LLAMA_MODEL \
    ~/Library/Application\ Support/autofilesync/models/<modell>.gguf
```

Ohne LaunchAgent geht die so gesetzte Variable bei der nächsten Anmeldung
verloren.

## 6. Installation prüfen

```bash
open -a AutoFileSync
launchctl getenv AUTOFILESYNC_LLAMA_MODEL
```

## 7. Deinstallation

```bash
rm -rf /Applications/AutoFileSync.app
rm -rf ~/Library/Application\ Support/autofilesync
rm -f ~/Library/LaunchAgents/com.autofilesync.llamamodel.plist
launchctl bootout "gui/$(id -u)/com.autofilesync.backgroundsync" 2>/dev/null
rm -f ~/Library/LaunchAgents/com.autofilesync.backgroundsync.plist
launchctl unsetenv AUTOFILESYNC_LLAMA_MODEL
```

Die Zeile `export AUTOFILESYNC_LLAMA_MODEL=...` in `~/.zshrc` und
`~/.zprofile` ist zusätzlich von Hand zu entfernen.

## 8. Fehlerbehebung

### „Kein lokales Modell gefunden"

Weder ist `AUTOFILESYNC_LLAMA_MODEL` im Prozess gesetzt, noch liegt eine
`*.gguf` im Modellverzeichnis.

```bash
ls ~/Library/Application\ Support/autofilesync/models/
```

Ist das Verzeichnis leer, richtet `scripts/setup_llama_macos.sh` das Modell ein.

### „Modell konnte nicht geladen werden"

Die Variable zeigt auf eine Datei, die llama.cpp nicht öffnen kann – meist ein
abgebrochener Download. Prüfen:

```bash
head -c 4 "$(launchctl getenv AUTOFILESYNC_LLAMA_MODEL)"
```

Eine gültige GGUF-Datei beginnt mit `GGUF`. Andernfalls die Datei löschen und
`scripts/setup_llama_macos.sh` erneut ausführen.

### Antworten des Assistenten kommen sehr langsam

Der Assistent rechnet auf der CPU (siehe Abschnitt 3). Ein kleineres Modell
schafft Abhilfe:

```bash
scripts/setup_llama_macos.sh --size 3b
```

### Die GUI-Tests bewegen Maus und Tastatur

Die Tests steuern echte Ereignisse auf dem aktiven Desktop. Das ausführende
Terminal braucht dafür die Berechtigung unter Systemeinstellungen → Datenschutz
& Sicherheit → Bedienungshilfen.
