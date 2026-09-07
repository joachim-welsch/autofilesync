# AutoFileSync - Build-Dokumentation

## Voraussetzungen

- **CMake** >= 3.14
- **C++20** Compiler (g++ oder clang++)
- **wxWidgets** >= 3.0
- **Boost** (property_tree)
- **Git** (optional)

## Build-Typen

### Debug-Build (Standard)
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j 10
```
- Keine Optimierungen
- Debug-Symbole enthalten
- Größere Binärdatei
- Für Entwicklung und Debugging

**Ausgabe:** `build/autofilesync`

### Release-Build
```bash
cmake -S . -B build/Release -DCMAKE_BUILD_TYPE=Release
cmake --build build/Release -j 10
```
- Volle Optimierungen (-O3)
- Keine Debug-Symbole
- Kleinere und schnellere Binärdatei
- Für Produktiv-Einsatz

**Ausgabe:** `build/Release/autofilesync`

## Build in VS Code

Die Konfiguration liegt in `.vscode/` (CMake Tools + C/C++-Erweiterung,
empfohlen in `extensions.json`). Beim Öffnen konfiguriert CMake Tools den
Debug-Baum `build/` mit `AFS_BUILD_TESTS=ON`, damit die Tests auch im
Test-Explorer (CTest) erscheinen.

### Bauen (Tasks, Ctrl+Shift+P → "Tasks: Run Task")
- **Ctrl+Shift+B** - "CMake: build", Debug-Version von `autofilesync`
- "CMake: build backgroundsync" / "... Integrationtests" / "... GuiTests" / "... all"
- "CMake: build Release" - konfiguriert und baut `build/Release`
- "CMake: clean"

### Testen (Tasks, Ctrl+Shift+P → "Tasks: Run Test Task")
- "Test: Integrationtests" - `run_Integrationtests.sh`, fragt nach einem gtest-Filter
- "Test: GUI-Tests" - `run_GUI_Tests.sh` auf einem privaten X-Display
- "Test: ctest" - alle bei CTest registrierten Tests

### Starten/Debuggen (F5, Auswahl in der Debug-Ansicht)
- "autofilesync (gdb)" - baut und startet das Programm
- "backgroundsync (gdb)" - Daemon mit `-singlesync`
- "Integrationtests (gdb)" - fragt nach einem gtest-Filter, hält bei Fehlschlag an
- "GuiTests (gdb, real desktop)" - läuft auf dem echten Desktop, Maus und
  Tastatur werden während des Tests übernommen; Filter auf einen Test setzen
- "Attach to process (gdb)" - an ein laufendes `autofilesync` anhängen

## Build in Xcode

Das Xcode-Projekt wird von CMake erzeugt, in einem eigenen Baubaum neben dem
Makefile-Build:
```bash
cmake -S . -B xcode -G Xcode -DAFS_BUILD_TESTS=ON
cmake --build xcode --config Debug --target autofilesync
```
Jedes CMake-Ziel wird ein Xcode-Target (`afs_core`, `autofilesync`,
`backgroundsync`, `Integrationtests`, `GuiTests`); Debug oder Release wählt
Xcode beim Bauen, nicht `CMAKE_BUILD_TYPE`.

Ist `AUTOFILESYNC_LLAMA_MODEL` beim Konfigurieren gesetzt, erzeugt CMake dazu
ein Schema, das den Modellpfad an Debug-Läufe weitergibt.

Nach Änderungen an der Verzeichnisstruktur beschreibt ein vorhandener Baubaum
noch die alte Aufteilung; er ist zu löschen und neu zu erzeugen. Der llama.cpp-
Checkout lässt sich dabei mit `-DFETCHCONTENT_SOURCE_DIR_LLAMA_CPP=<Pfad>`
weiterverwenden.

## Projekt-Struktur

Das Projekt besteht aus zwei Teilen: **AFS_Core** (Synchronisationskern,
GPLv3) und **AFS_Desktop** (wxWidgets-Oberfläche und weiterführende
Funktionen). Die Programmdatei heißt weiterhin `autofilesync`.

```
autofilesync/
├── CMakeLists.txt              # Gesamtprojekt: AFS_Core + AFS_Desktop
├── build/                      # Debug-Build (build/autofilesync)
├── build/Release/              # Release-Build (build/Release/autofilesync)
├── xcode/                      # Xcode-Projekt (cmake -G Xcode)
├── .vscode/                    # VS Code Konfiguration
├── AFS_Core/                   # Kern - statische Bibliothek afs_core
│   ├── CMakeLists.txt
│   ├── cmake/AfsCore.cmake     # Quelllisten, Abhängigkeiten, afs_core_add_library()
│   ├── cmake/AfsWxWidgets.cmake# wxWidgets als Link-Target afs_wxwidgets
│   ├── include/, src/          # Synchronisation, SSH, Konfiguration, LlamaClient,
│   │                           # Hilfsquellen (Logging, FilesystemUtil, util, ...)
│   └── test/                   # Integrationstests, Testdaten (resources, tree)
└── AFS_Desktop/                # Anwendung - Programm autofilesync
    ├── CMakeLists.txt
    ├── cmake/AfsDesktop.cmake  # Quellliste, afs_desktop_configure_target()
    ├── include/, src/          # Fenster, Dialoge, Hintergrunddienst
    ├── bitmaps/, icons/        # Bildressourcen
    ├── help/                   # Hilfebuch (wird neben die Programmdatei kopiert)
    ├── installer/              # Bausteine des macOS-Installationsprogramms
    ├── Info.plist, autofilesync.rc, *.vcxproj  # macOS-Bündel, Windows-Build
    └── test/GUI-Tests/         # GUI-Tests
```

Beide Teile lassen sich auch einzeln konfigurieren (`cmake -S AFS_Core ...`
bzw. `cmake -S AFS_Desktop ...`); AFS_Desktop zieht AFS_Core dann selbst hinzu.

### CMake-Optionen

| Option | Vorgabe | Wirkung |
| --- | --- | --- |
| `AFS_BUILD_TESTS` | OFF | Baut zusätzlich `Integrationtests` und `GuiTests` |
| `AFS_CORE_WITH_WXWIDGETS` | ON | Kern mit seinen wxWidgets-Anbindungen (`_WXWIDGETS`); AFS_Desktop braucht ON |
| `FETCHCONTENT_SOURCE_DIR_LLAMA_CPP` | – | Vorhandenen llama.cpp-Checkout verwenden statt neu zu klonen |

## Externe Abhängigkeiten

Die früher separat erwartete Bibliothek **mylib** ist vollständig in `AFS_Core`
aufgegangen (Logging, LogCut, FilesystemUtil, MountedDrives, RecycleBin,
SmtpMailer, StrUtil, util). Weitere Abhängigkeiten: wxWidgets 3.2, Boost
(header-only), libssh, OpenSSL, Google Test (nur Tests); llama.cpp wird beim
Konfigurieren geklont. GNU gettext (libintl, `msgfmt`) ist optional - ohne die
Bibliothek bleiben die Texte deutsch.

## Mehrsprachigkeit (GNU gettext)

Die Texte, die `syncItemType2Str()` liefert, sind beispielhaft über GNU gettext
übersetzbar. Die msgid ist jeweils der deutsche Originaltext, deshalb zeigt ein
Build ohne Katalog (oder ganz ohne libintl) unverändert die deutschen Texte.

- Kataloge: `po/<sprache>.po`, Vorlage `po/autofilesync.pot`.
- `msgfmt` übersetzt sie beim Build nach `build/locale/<sprache>/LC_MESSAGES/autofilesync.mo`
  (auf macOS zusätzlich in `autofilesync.app/Contents/Resources/locale`).
  Fehlt `msgfmt`, entfällt nur der Übersetzungsschritt.
- Sprache zur Laufzeit wählen: `LANGUAGE=en ./autofilesync` (gettext wertet
  `LANGUAGE` vor der Locale aus).
- Neue übersetzbare Texte einsammeln:
  ```bash
  xgettext --keyword=translate --from-code=UTF-8 -o po/autofilesync.pot AFS_Core/src/*.cpp
  msgmerge --update po/en.po po/autofilesync.pot
  ```

In der Konfigurationsdatei steht weiterhin der unübersetzte Schlüssel
(`syncItemTypeKey()`), damit eine mit einer Sprache geschriebene Konfiguration
mit jeder anderen lesbar bleibt.

## Häufige Probleme

### Build schlägt fehl
```bash
# Build-Verzeichnis löschen und neu konfigurieren
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j 10
```

### wxWidgets nicht gefunden
```bash
# Ubuntu/Debian
sudo apt install libwxgtk3.0-gtk3-dev

# Fedora
sudo dnf install wxGTK-devel
```

### Boost nicht gefunden
```bash
# Ubuntu/Debian
sudo apt install libboost-all-dev

# Fedora
sudo dnf install boost-devel
```

## Tests ausführen

Die Tests werden mit `-DAFS_BUILD_TESTS=ON` im selben Baubaum wie das
Programm gebaut; die Skripte im Wurzelverzeichnis erledigen das:

### Integrations-Tests (AFS_Core)
```bash
./run_Integrationtests.sh                 # Argumente gehen an gtest durch
# oder von Hand:
cmake -S . -B build -DAFS_BUILD_TESTS=ON
cmake --build build --target Integrationtests
./build/Integrationtests
```

### GUI-Tests (AFS_Desktop)
```bash
./run_GUI_Tests.sh
# oder von Hand:
cmake --build build --target GuiTests
./AFS_Desktop/test/GUI-Tests/run_gui_tests.sh build/GuiTests
```
Auf macOS steuern die Tests echte Maus- und Tastaturereignisse auf dem aktiven
Desktop (`wxUIActionSimulator`); dafür braucht das ausführende Terminal die
Berechtigung Systemeinstellungen → Datenschutz & Sicherheit → Bedienungshilfen.

## Installation

Siehe [INSTALL-Linux.md](INSTALL-Linux.md) (Linux) bzw.
[INSTALL-macOS.md](INSTALL-macOS.md) (macOS).

Unter macOS erzeugt `./makeMacOSInstaller.sh` ein verteilbares Disk-Image,
`./installMacOS.sh` installiert direkt nach `/Applications` und
`./test_macOS_installer.sh` prüft das erzeugte Image. Die gemeinsamen Schritte
liegen in `AFS_Desktop/installer/macos_bundle.sh`.

Das Verzeichnis `AFS_Desktop/help/` gehört neben die installierte
Programmdatei – dort sucht das Programm sein Hilfebuch. Der Build kopiert es
nach jedem Linken neben die Programmdatei (`build/help/`), ein Build braucht
also keine Installation, um die Hilfe anzeigen zu können. In das macOS-Bündel
legen Build und Installationsskripte es unter `Contents/Resources/help`, weil
dort neben der Programmdatei kein Platz dafür ist.

## Autostart konfigurieren

Das Programm unterstützt Autostart unter Linux:
- Einstellung in der GUI: **Einstellungen** → **Anwendung automatisch starten**
- Erstellt eine Desktop-Datei in: `~/.config/autostart/`

## Logging

Log-Dateien werden standardmäßig gespeichert in:
```
~/.config/autofilesync/autofilesync.log
```

Log-Level kann in der GUI konfiguriert werden:
**Einstellungen** → **Logging**

## Lizenz

[Lizenzinformationen einfügen]

## Kontakt

[Kontaktinformationen einfügen]
