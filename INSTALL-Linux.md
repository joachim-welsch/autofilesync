# AutoFileSync – Installation unter Linux

Diese Anleitung beschreibt den vollständigen Weg von den Quellen bis zur
lauffähigen Installation inklusive Hilfeseiten, Menüeintrag und optionalem
Hintergrunddienst.

Getestet unter Ubuntu 24.04 LTS. Für andere Distributionen unterscheiden sich
nur die Paketnamen in Schritt 1.

## Übersicht: was wohin installiert wird

| Inhalt | Zielpfad |
|---|---|
| Programmdatei | `/opt/autofilesync/autofilesync` |
| Hilfeseiten (`help.htb` und Quelldateien) | `/opt/autofilesync/help/` |
| Starter im Suchpfad | `/usr/local/bin/autofilesync` |
| Menüeintrag | `/usr/local/share/applications/autofilesync.desktop` |
| Programmsymbol | `/usr/local/share/icons/hicolor/*/apps/autofilesync.png` |
| Konfiguration (pro Benutzer, wird automatisch angelegt) | `~/.config/autofilesync/autofilesync.json` |
| Protokolldatei (pro Benutzer) | `~/.config/autofilesync/autofilesync.log` |

Programm und Hilfe liegen bewusst in **einem** Verzeichnis: Das Programm sucht
das Hilfebuch in `help/` neben der eigenen Programmdatei. Das Arbeitsverzeichnis
spielt dabei keine Rolle, der Start über Menüeintrag, Autostart oder Symlink
funktioniert also gleichermaßen.

## 1. Voraussetzungen installieren

### Ubuntu / Debian

```bash
sudo apt update
sudo apt install build-essential cmake git pkg-config \
                 libwxgtk3.2-dev libboost-filesystem-dev \
                 libssh-dev libssl-dev imagemagick gettext
```

### Fedora

```bash
sudo dnf install gcc-c++ cmake git pkgconf-pkg-config \
                 wxGTK-devel boost-devel \
                 libssh-devel openssl-devel ImageMagick gettext
```

### Mindestversionen

| Paket | Mindestversion | Prüfen mit |
|---|---|---|
| CMake | 3.12 | `cmake --version` |
| C++-Compiler | C++20 (g++ 10+, clang 12+) | `g++ --version` |
| wxWidgets | 3.2 (mit `core base aui html`) | `wx-config --version` |
| Boost | 1.83 (Komponente `filesystem`) | `dpkg -s libboost-dev` |
| libssh | 0.9 | `pkg-config --modversion libssh` |
| OpenSSL | 3.0 | `openssl version` |

`imagemagick` wird nur für die Umwandlung des Programmsymbols in Schritt 6
gebraucht und kann danach wieder entfernt werden.

Ubuntu 22.04 liefert nur Boost 1.74 und wxWidgets 3.0 – beides ist zu alt. Dort
müssen wxWidgets und Boost aus den Quellen gebaut oder aus einem Backports-/PPA-
Repository bezogen werden.

## 2. Quellen bereitstellen

Alle Quellen liegen im Repository; die früher separat benötigte Bibliothek
mylib ist vollständig in `AFS_Core` aufgegangen.

```bash
mkdir -p ~/Entwicklung
git clone <url-des-repositories> ~/Entwicklung/autofilesync
```

## 3. Übersetzen

```bash
cd ~/Entwicklung/autofilesync
./build.sh -r
```

Das Ergebnis sind `build/Release/autofilesync` und der Hintergrunddienst
`build/Release/backgroundsync`.

Alternativ direkt mit CMake:

```bash
cmake -S . -B build/Release -DCMAKE_BUILD_TYPE=Release
cmake --build build/Release --target autofilesync backgroundsync -j $(nproc)
```

Hinweise zum ersten Durchlauf:

- CMake klont beim Konfigurieren llama.cpp (lokaler Chat-Assistent) vollständig
  nach `build/Release/_deps`. Das dauert einige Minuten und sieht aus wie ein
  hängender Build – es ist keiner. Folgende Builds nutzen den Klon erneut.
- Der Build braucht eine funktionierende Internetverbindung, weil llama.cpp
  heruntergeladen wird.
- llama.cpp wird mit `GGML_NATIVE=ON` für die CPU des Baurechners übersetzt.
  Soll das Ergebnis auf einem anderen Rechner laufen, ist zusätzlich
  `-DGGML_NATIVE=OFF` anzugeben, sonst droht dort ein Absturz mit „Illegal
  instruction".

## 4. Programm und Hilfeseiten kopieren

```bash
cd ~/Entwicklung/autofilesync

sudo mkdir -p /opt/autofilesync/help
sudo install -m 755 build/Release/autofilesync /opt/autofilesync/autofilesync
sudo install -m 644 AFS_Desktop/help/* /opt/autofilesync/help/
```

Die Hilfe besteht aus dem kompilierten Hilfebuch `help.htb` (ein ZIP-Archiv mit
allen Seiten) sowie den unkomprimierten Quelldateien. Das Programm liest zur
Laufzeit ausschließlich `help/help.htb`; die übrigen Dateien werden mitkopiert,
damit die Hilfe am Zielsystem nachvollziehbar und neu erzeugbar bleibt.

Kontrolle:

```bash
ls -l /opt/autofilesync/help/help.htb
```

### Hilfeseiten nach Änderungen neu erzeugen

Werden die HTML-Seiten unter `AFS_Desktop/help/` geändert, muss `help.htb` neu gepackt
werden – das Programm liest nur das Archiv:

```bash
sudo apt install zip                     # falls noch nicht vorhanden
cd ~/Entwicklung/autofilesync/AFS_Desktop/help
rm -f help.htb
zip -r help.htb *.html *.hhc *.hhk *.hhp wxhelp.map
```

Anschließend Schritt 4 wiederholen.

## 5. Programm in den Suchpfad aufnehmen

```bash
sudo ln -sf /opt/autofilesync/autofilesync /usr/local/bin/autofilesync
```

Ein Symlink genügt: Das Programm ermittelt seinen Installationsort über
`/proc/self/exe`, der Symlink wird dabei aufgelöst und die Hilfe unter
`/opt/autofilesync/help/` gefunden.

Test:

```bash
autofilesync
```

Im Menü **Hilfe → Hilfe** muss sich das Hilfefenster mit Inhaltsverzeichnis
öffnen. Erscheint stattdessen „Hilfedatei nicht gefunden", ist Schritt 4
unvollständig – die Meldung nennt den erwarteten Pfad.

Wird statt des Symlinks ein Starter-Skript gebraucht (etwa für die
Umgebungsvariablen aus Schritt 10 oder 14), muss es die Programmdatei per
`exec` starten, damit `/proc/self/exe` weiterhin auf sie zeigt:

```sh
#!/bin/sh
exec /opt/autofilesync/autofilesync "$@"
```

## 6. Programmsymbol installieren

Das mitgelieferte `icons/afs.ico` enthält alle benötigten Größen und wird in das
hicolor-Symbolschema umgewandelt:

```bash
cd ~/Entwicklung/autofilesync
for size in 16 24 32 48 64 128 256; do
    sudo mkdir -p /usr/local/share/icons/hicolor/${size}x${size}/apps
    sudo convert 'icons/afs.ico[6]' -resize ${size}x${size} \
        /usr/local/share/icons/hicolor/${size}x${size}/apps/autofilesync.png
done
sudo gtk-update-icon-cache -f /usr/local/share/icons/hicolor 2>/dev/null || true
```

`[6]` wählt aus der Symboldatei die Ebene mit 256×256 Bildpunkten aus, aus der
alle kleineren Größen verkleinert werden. Unter ImageMagick 7 heißt der Befehl
`magick` statt `convert`.

## 7. Menüeintrag anlegen

```bash
sudo tee /usr/local/share/applications/autofilesync.desktop > /dev/null <<'EOF'
[Desktop Entry]
Type=Application
Name=AutoFileSync
Comment=Automatische Synchronisation von Dateien und Verzeichnissen
Exec=/usr/local/bin/autofilesync
Icon=autofilesync
Terminal=false
Categories=Utility;FileTools;
EOF
sudo update-desktop-database /usr/local/share/applications 2>/dev/null || true
```

## 8. Automatischer Start beim Anmelden (optional)

In der GUI unter **Einstellungen → Anwendung automatisch starten** aktivieren.
Das Programm legt daraufhin `~/.config/autostart/my_program.desktop` mit dem
Pfad der laufenden Programmdatei an. Eine Nacharbeit ist nicht nötig; die Hilfe
wird auch beim automatischen Start gefunden.

## 9. Hintergrunddienst (optional)

Der Dienst `backgroundsync` synchronisiert mit derselben Konfiguration weiter,
während die GUI nicht läuft. `./build.sh` baut ihn zusammen mit der GUI nach
`build/Release/backgroundsync` (Abschnitt 3); die GUI steuert ihn nur über
`systemctl` (Menü **Dienst**).

```bash
cd ~/Entwicklung/autofilesync
./build.sh -r
backgroundsync/install.sh
```

`install.sh` kopiert `build/Release/backgroundsync` nach `/usr/sbin/backgroundsync`, die
Übersetzungen nach `/usr/share/locale/<sprache>/LC_MESSAGES/autofilesync.mo`, die
Unit-Datei nach `/etc/systemd/system/backgroundsync.service` und aktiviert den
Dienst. Vor dem Aufruf ist `backgroundsync.service` anzupassen – vor allem
`User=`, `Group=` und die `Wants=`/`After=`-Einträge für die Laufwerke, auf denen
die Synchronisationsziele liegen. Der Dienst liest die Konfiguration des unter
`User=` eingetragenen Benutzers (`~/.config/autofilesync/autofilesync.json`) und
protokolliert daher in dieselbe Protokolldatei wie die GUI.

Zustand prüfen:

```bash
systemctl status backgroundsync.service
journalctl -u backgroundsync.service -b --no-pager
```

Ohne installierten Dienst zeigt die GUI im Dienst-Menü „nicht installiert" an;
alle übrigen Funktionen sind davon unberührt.

## 10. Chat-Assistent (optional)

Der eingebaute Assistent rechnet lokal über llama.cpp und benötigt eine
Modelldatei im GGUF-Format. Am einfachsten liegt sie im Modellverzeichnis des
Benutzers; dort findet das Programm sie von selbst, auch ohne Neustart:

```bash
mkdir -p ~/.local/share/autofilesync/models
cp ~/Downloads/<modell>.gguf ~/.local/share/autofilesync/models/
```

Ist `XDG_DATA_HOME` gesetzt, tritt es an die Stelle von `~/.local/share`.
Liegen mehrere Modelle dort, nimmt das Programm das dem Dateinamen nach erste.

Soll eine Datei außerhalb dieses Verzeichnisses für alle Benutzer dienen, wird
ihr Pfad über die Umgebungsvariable `AUTOFILESYNC_LLAMA_MODEL` bekannt gegeben;
sie geht dem Modellverzeichnis vor:

```bash
sudo mkdir -p /opt/autofilesync/models
sudo cp ~/Downloads/<modell>.gguf /opt/autofilesync/models/
```

Damit die Variable auch beim Start über Menü oder Autostart gesetzt ist, tritt
an die Stelle des Symlinks aus Schritt 5 ein Starter-Skript:

```bash
sudo rm -f /usr/local/bin/autofilesync
sudo tee /usr/local/bin/autofilesync > /dev/null <<'EOF'
#!/bin/sh
AUTOFILESYNC_LLAMA_MODEL=/opt/autofilesync/models/<modell>.gguf
export AUTOFILESYNC_LLAMA_MODEL
exec /opt/autofilesync/autofilesync "$@"
EOF
sudo chmod 755 /usr/local/bin/autofilesync
```

Ohne Modell meldet der Chat-Dialog „Kein lokales Modell gefunden" und nennt
das Modellverzeichnis; der Rest des Programms funktioniert normal.

## 11. Installation prüfen

```bash
which autofilesync                       # -> /usr/local/bin/autofilesync
ls /opt/autofilesync/help/help.htb       # Hilfebuch vorhanden
autofilesync                             # GUI startet
```

In der GUI:

1. **Hilfe → Hilfe** öffnet das Hilfefenster mit Inhaltsverzeichnis und Index.
2. In einem Dialog **F1** drücken – es muss die passende Kontexthilfeseite
   erscheinen (Zuordnung siehe `help/wxhelp.map`).
3. Nach dem ersten Beenden existiert `~/.config/autofilesync/autofilesync.json`.

## 12. Aktualisieren

Für eine neue Version genügen Übersetzen und erneutes Kopieren; Konfiguration
und Protokoll im Benutzerverzeichnis bleiben unberührt:

```bash
cd ~/Entwicklung/autofilesync
git pull
./build.sh -r
sudo install -m 755 build/Release/autofilesync /opt/autofilesync/autofilesync
sudo install -m 644 help/* /opt/autofilesync/help/
```

Ist der Hintergrunddienst installiert, aktualisiert ihn `install.sh`; es stoppt
ihn vorher und startet ihn danach wieder:

```bash
backgroundsync/install.sh
```

## 13. Deinstallation

```bash
sudo systemctl disable --now backgroundsync.service 2>/dev/null || true
sudo rm -f /etc/systemd/system/backgroundsync.service /usr/sbin/backgroundsync
sudo rm -f /usr/share/locale/*/LC_MESSAGES/autofilesync.mo
sudo systemctl daemon-reload

sudo rm -rf /opt/autofilesync
sudo rm -f /usr/local/bin/autofilesync
sudo rm -f /usr/local/share/applications/autofilesync.desktop
sudo rm -f /usr/local/share/icons/hicolor/*/apps/autofilesync.png
sudo gtk-update-icon-cache -f /usr/local/share/icons/hicolor 2>/dev/null || true

rm -f ~/.config/autostart/my_program.desktop
```

Die Benutzerdaten werden bewusst nicht mitentfernt. Wer auch sie loswerden will:

```bash
rm -rf ~/.config/autofilesync        # Konfiguration, Protokoll, Schlüsseldatei
```

Achtung: `~/.config/autofilesync/secret.key` entschlüsselt die gespeicherten
SSH-Kennwörter. Nach dem Löschen müssen alle SSH-Konten neu eingerichtet werden.

## 14. Fehlerbehebung

### „Hilfedatei nicht gefunden"

Die Meldung nennt den Pfad, an dem das Hilfebuch erwartet wird – in aller Regel
`/opt/autofilesync/help/help.htb`. Schritt 4 wiederholen und prüfen:

```bash
ls -l /opt/autofilesync/help/help.htb
```

Das Programm sucht das Buch neben der eigenen Programmdatei, danach in bis zu
drei übergeordneten Verzeichnissen (damit es im Baubaum ohne Installation
funktioniert) und zuletzt im Arbeitsverzeichnis.

### Programm startet nicht, Fehler beim Laden von Bibliotheken

Auf Systemen mit Snap-Paketen kann ein untergeschobener `LD_LIBRARY_PATH` die
wxWidgets-Bibliotheken verdecken. Dagegen hilft ein Starter-Skript anstelle des
Symlinks aus Schritt 5 (vgl. `run_autofilesync.sh` im Projekt):

```sh
#!/bin/sh
LD_LIBRARY_PATH=/usr/local/lib:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu
export LD_LIBRARY_PATH
exec /opt/autofilesync/autofilesync "$@"
```

### CMake findet wxWidgets nicht

`wx-config --version` muss 3.2 oder neuer melden. Ist mehr als eine Version
installiert, hilft ein expliziter Verweis:

```bash
cmake -S . -B build/Release -DCMAKE_BUILD_TYPE=Release \
      -DwxWidgets_CONFIG_EXECUTABLE=$(which wx-config-3.2)
```

### CMake findet Boost oder libssh nicht

Prüfen mit `pkg-config --modversion libssh` und `dpkg -s libboost-dev`. Boost
muss mindestens 1.83 sein, die Komponente `filesystem` muss installiert sein.

### Build bricht in llama.cpp ab oder hängt beim Konfigurieren

Der erste Lauf klont llama.cpp vollständig. Bei abgebrochenem Klon hilft:

```bash
rm -rf build/Release/_deps
cmake -S . -B build/Release -DCMAKE_BUILD_TYPE=Release
```

### Menüeintrag erscheint nicht

```bash
sudo update-desktop-database /usr/local/share/applications
```

Danach ab- und wieder anmelden. Manche Desktop-Umgebungen berücksichtigen
`/usr/local/share/applications` nur, wenn `XDG_DATA_DIRS` diesen Pfad enthält;
im Zweifel den Eintrag stattdessen nach `/usr/share/applications` kopieren.

### Dienst-Menü meldet dauerhaft „nicht installiert"

`systemctl show backgroundsync -p LoadState` ausführen. Meldet es
`LoadState=not-found`, fehlt die Unit-Datei – Schritt 9 nachholen.
