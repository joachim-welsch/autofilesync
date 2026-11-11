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
- "Test: Integrationtests" - `scripts/run_Integrationtests.sh`, fragt nach einem gtest-Filter
- "Test: GUI-Tests" - `scripts/run_GUI_Tests.sh` auf einem privaten X-Display
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

## Build unter Windows

Windows baut nicht über CMake, sondern über `AFS_Desktop\autofilesync.sln`: dort
stehen die Pfade zu wxWidgets, Boost und vcpkg, die diese Plattform braucht.
`scripts\build.cmd` erledigt das Gegenstück zu `build.sh`:

```cmd
scripts\build.cmd              :: Release
scripts\build.cmd -d           :: Debug
scripts\build.cmd clean -d     :: Build-Verzeichnis löschen und Debug bauen
```

Voraussetzung ist ein vcpkg mit `vcpkg integrate install` und den Paketen aus
`vcpkg.json`. Die Solution liegt nicht neben dem Manifest, MSBuild baut also im
klassischen Modus – die Pakete müssen im globalen vcpkg-Baum stehen und aus dem
vcpkg-Verzeichnis heraus installiert werden (im Repo-Verzeichnis erzwingt das
`vcpkg.json` den Manifest-Modus, der keine Paketargumente annimmt):

```cmd
cd C:\Entwicklung\extlib\vcpkg
vcpkg install "gettext[core,tools]:x64-windows" libssh2:x64-windows llama-cpp:x64-windows
```

Das Feature `tools` ist nötig, weil `msgfmt` sonst nicht mitkommt: das Paket
`gettext` liefert ohne es nur die Laufzeitbibliothek `libintl`.

Es sucht MSBuild über `vswhere`, baut `Configuration=<Typ>|Platform=x64` und
übersetzt anschließend die Kataloge aus `po/` nach
`AFS_Desktop\x64\<Typ>\locale\<sprache>\LC_MESSAGES\autofilesync.mo` – den
Schritt hat das Projekt selbst nicht (siehe unten). `msgfmt` sucht es im PATH
und sonst unter `<vcpkg>\installed\x64-windows\tools\gettext\bin`; das
vcpkg-Verzeichnis kommt aus `%VCPKG_ROOT%` oder aus der von
`vcpkg integrate install` hinterlegten `vcpkg.path.txt`. Findet es keines von
beidem, warnt das Skript und baut weiter.

**Ausgabe:** `AFS_Desktop\x64\<Typ>\autofilesync.exe` und der Hintergrunddienst
`backgroundsync\x64\<Typ>\backgroundsync.exe`. Die Solution enthält dafür
`backgroundsync\backgroundsync.vcxproj`, das die Kernquellen ohne wxWidgets
übersetzt – wie `afs_core_headless` in `backgroundsync/CMakeLists.txt`. Der
Einstiegspunkt ist `WindowsService.cpp`: vom Dienstmanager gestartet läuft das
Programm als Windows-Dienst, aus einer Konsole gestartet im Vordergrund bis
Strg+C. Neue Kernquellen gehören in beide `.vcxproj`-Dateien.

### Installationspaket (MSI)

`makeWindowsInstaller.cmd` erzeugt `build\AutoFileSync-<Version>.msi` – das
Gegenstück zu `makeMacOSInstaller.sh`. Voraussetzung ist das WiX-Werkzeug
(`dotnet tool install --global wix`); die Erweiterungen UI und Util lädt das
Skript selbst nach, und zwar in der Version von `wix` selbst, weil eine neuere
sich nicht laden lässt.

```cmd
scripts\makeWindowsInstaller.cmd              :: Release bauen, signieren und paketieren
scripts\makeWindowsInstaller.cmd -NoBuild     :: vorhandenen Build verwenden
scripts\makeWindowsInstaller.cmd -NoSigning   :: unsigniertes Paket (Entwicklerbuild)
scripts\makeWindowsInstaller.cmd -Output C:\Temp
scripts\makeWindowsInstaller.cmd -DevFirewallRules  :: Paket für den eigenen Rechner
```

Das Skript stellt die Nutzlast in `build\windows_installer\payload` zusammen –
Programmdatei, die vcpkg-Laufzeitbibliotheken, `help\` und `locale\` – und
prüft sie gegen eine Liste geforderter Einträge, bevor WiX daraus ein Paket
macht. Fehlt ein Katalog oder eine DLL, bricht es dort ab statt ein Paket
auszuliefern, das erst beim Anwender auffällt.

Das Paket installiert nach `%ProgramFiles%\AutoFileSync`, legt eine Verknüpfung
im Startmenü an und ersetzt bei jeder Installation die vorherige – auch eine
derselben Version, weil WiX pro Build einen neuen ProductCode vergibt und sich
zwei Pakete sonst nebeneinandersetzen. Ein eigener Dialog fragt zusätzlich nach
einem Starticon auf dem Desktop; der Haken ist vorbelegt, still steuert ihn
`INSTALLDESKTOPSHORTCUT` (leer heißt kein Icon):

```cmd
msiexec /i build\AutoFileSync-<Version>.msi INSTALLDESKTOPSHORTCUT="" /qn
```

Eine weitere Seite richtet den Hintergrunddienst ein: `backgroundsync.exe` wird
als Windows-Dienst `backgroundsync` registriert, startet automatisch und wird
beim Entfernen des Pakets gestoppt und mit entfernt. Der Dienst läuft unter dem
Konto der installierenden Person, weil er deren Konfiguration, Zugangsdaten und
Netzlaufwerke braucht; die Seite fragt deshalb nach dem Anmeldepasswort und
verleiht dem Konto das Recht „Als Dienst anmelden“. Ein zuvor von Hand mit
`sc create` registrierter Dienst gleichen Namens wird dabei ersetzt. Der Haken
ist vorbelegt; still steuern ihn `INSTALLSERVICE` sowie `SERVICEUSER`,
`SERVICEDOMAIN` und `SERVICEPASSWORD` – ohne Passwort bleibt der Dienst weg:

```cmd
msiexec /i build\AutoFileSync-<Version>.msi SERVICEPASSWORD=geheim /qn
msiexec /i build\AutoFileSync-<Version>.msi INSTALLSERVICE="" /qn
```

Ein falsches Passwort lässt die Installation nicht scheitern: der Dienst bleibt
dann gestoppt, und das Menü **Hintergrunddienst** nennt beim Start den Grund.

Für die Handy-Verbindung legt das Paket zwei Firewall-Ausnahmen an, „AutoFileSync
(Handy)“ für das Programm und „AutoFileSync Hintergrunddienst (Handy)“ für den
Dienst: eingehend, alle Profile, nur aus dem lokalen Subnetz, TCP und UDP.
Windows filtert nach Programmpfad, deshalb gelten sie nur für die installierten
Dateien unter `%ProgramFiles%\AutoFileSync`. Ein Paket, das mit
`-DevFirewallRules` gebaut wurde, legt dieselben Ausnahmen zusätzlich für
`autofilesync.exe` und `backgroundsync.exe` im Build-Verzeichnis an (Release
und Debug, Namen mit „Entwicklungsbuild“), damit ein aus Visual Studio
gestartetes Programm das Handy ohne Firewall-Abfrage erreicht. Ein solches
Paket gehört nur auf den Entwicklungsrechner, weil es dessen Pfade enthält.

Die Produktdefinition steht in `AFS_Desktop/installer/autofilesync.wxs`, die
Versionsnummer kommt wie beim macOS-Bündel aus `AFS_Desktop/include/Version.h`.

Das Modell für den Chat-Assistenten ist nicht im Paket – es ist mehrere GB groß
und hängt vom Arbeitsspeicher ab. Das Paket legt dafür im Startmenü
„AutoFileSync - Chat-Assistent einrichten“ an; der Eintrag startet
`setup_llama_windows.cmd`, das ein passendes Modell nach
`%LOCALAPPDATA%\autofilesync\models` lädt. Dort findet das Programm es ohne
Umgebungsvariable und ohne Neustart. Aus dem Repository heraus:

```cmd
scripts\setup_llama_windows.cmd                      :: Größe nach Arbeitsspeicher
scripts\setup_llama_windows.cmd -Size 3b             :: Größe erzwingen
scripts\setup_llama_windows.cmd -Model D:\m.gguf     :: vorhandene Datei, setzt AUTOFILESYNC_LLAMA_MODEL
```

Gesucht wird in dieser Reihenfolge: `AUTOFILESYNC_LLAMA_MODEL`, falls gesetzt,
sonst die dem Namen nach erste `*.gguf` im Modellverzeichnis
(`model_location::find()`).

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
│   │                           # Hilfsquellen (Logging, FilesystemUtil, Util, ...)
│   └── test/                   # Integrationstests, Testdaten (resources, tree)
└── AFS_Desktop/                # Anwendung - Programm autofilesync
    ├── CMakeLists.txt
    ├── cmake/AfsDesktop.cmake  # Quellliste, afs_desktop_configure_target()
    ├── include/, src/          # Fenster, Dialoge, Hintergrunddienst
    ├── bitmaps/, icons/        # Bildressourcen
    ├── help/                   # Hilfeseiten (Deutsch, en/ Englisch); die Bücher help.htb packt der Build daraus (CMake bzw. tools/packHelp.*)
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
SmtpMailer, StrUtil, Util). Weitere Abhängigkeiten: wxWidgets 3.2, Boost
(header-only), libssh, OpenSSL, Google Test (nur Tests); llama.cpp wird beim
Konfigurieren geklont. GNU gettext (libintl) ist **erforderlich**: die msgids sind
technische Schlüssel, ein Build ohne libintl zeigt also `source_dir_not_reachable`
statt eines Satzes. `msgfmt` bleibt optional - fehlt es, entfällt nur der Schritt,
der die Kataloge übersetzt.

## Mehrsprachigkeit (GNU gettext)

Alle benutzersichtbaren Texte von `AFS_Core` und `AFS_Desktop` laufen über das Modul
`Translation` von AFS_Core; die GUI-Tests holen die Texte, an denen sie einen Dialog
erkennen, aus demselben Katalog und laufen so in jeder Sprache.
Die msgids sind technische Schlüssel (`source_dir_not_reachable`), kein Anzeigetext:
alle Sprachen sind gleichberechtigte Kataloge, und ein Build ohne Katalog hat
keine Sprache, auf die er zurückfallen könnte - er zeigt den Schlüssel.

- Kataloge: `po/de.po`, `po/en.po`, `po/es.po`, `po/fr.po`, `po/it.po`, `po/pl.po`
  und `po/uk.po`, Vorlage `po/autofilesync.pot`.
  Eine Sprache ohne Katalog fällt auf Englisch zurück. Eine neue Sprache braucht
  `po/<sprache>.po` - Build und Installationspakete sammeln alle `po/*.po` ein - und
  einen Eintrag in `translation::LANGUAGES`, damit sie in den Einstellungen wählbar ist.
- `msgfmt` übersetzt sie beim Build nach `build/locale/<sprache>/LC_MESSAGES/autofilesync.mo`
  (auf macOS zusätzlich in `autofilesync.app/Contents/Resources/locale`).
  Beide Programme suchen dieses Verzeichnis neben der eigenen Binary.
- Sprache zur Laufzeit wählen: in den globalen Einstellungen (Schlüssel `Language`
  der Konfiguration), oder ohne gewählte Sprache über die Umgebung:
  `LANGUAGE=en ./autofilesync` (gettext wertet
  `LANGUAGE` vor der Locale aus). Eine Sitzung, die gar keine Sprache nennt, bekommt
  Deutsch - siehe `translation::init_locale()` und `translation::DEFAULT_LOCALES`.
- Neue übersetzbare Texte einsammeln:
  ```bash
  xgettext --keyword=translate --keyword=translate_utf8 --keyword=msgid \
           --from-code=UTF-8 --no-wrap -o po/autofilesync.pot \
           AFS_Core/src/*.cpp AFS_Desktop/src/*.cpp AFS_Desktop/include/TextResources.h backgroundsync/*.cpp
  msgmerge --no-wrap --update po/de.po po/autofilesync.pot
  msgmerge --no-wrap --update po/en.po po/autofilesync.pot
  msgmerge --no-wrap --update po/es.po po/autofilesync.pot
  msgmerge --no-wrap --update po/fr.po po/autofilesync.pot
  msgmerge --no-wrap --update po/it.po po/autofilesync.pot
  msgmerge --no-wrap --update po/pl.po po/autofilesync.pot
  msgmerge --no-wrap --update po/uk.po po/autofilesync.pot
  ```
  `--keyword=msgid` ist nötig für die Schlüssel, die erst gespeichert und später
  übersetzt werden (`translation::msgid(...)` in Tabellen und Prompts).

In der Konfigurationsdatei steht weiterhin ein unübersetzter Schlüssel
(`sync_item_type_key()`), der vom msgid getrennt ist: eine mit einer Sprache
geschriebene Konfiguration bleibt mit jeder anderen lesbar, und ein umbenannter
msgid ändert das Dateiformat nicht.

### Windows (Visual Studio)

Der CMake-Build erzwingt libintl; die `.vcxproj`-Projekte holen es über vcpkg
(`gettext` in `vcpkg.json`), definieren `AFS_WITH_GETTEXT` und linken `intl.lib`.
Heißt die Bibliothek in `installed\x64-windows\lib` anders (vcpkg kennt auch
`libintl` und `intl-8`), ist der Eintrag in `AdditionalDependencies` anzupassen.

Einen `msgfmt`-Schritt haben die Projekte **nicht**: die `.mo`-Dateien müssen neben
der Exe liegen, sonst zeigt auch ein Windows-Build die Schlüssel. `build.cmd` erledigt
das nach dem Linken; wer aus Visual Studio heraus baut, kopiert das `locale`-Verzeichnis
aus einem CMake-Build daneben oder erzeugt es von Hand:

```cmd
md locale\de\LC_MESSAGES locale\en\LC_MESSAGES locale\es\LC_MESSAGES locale\fr\LC_MESSAGES locale\it\LC_MESSAGES locale\pl\LC_MESSAGES locale\uk\LC_MESSAGES
msgfmt -o locale\de\LC_MESSAGES\autofilesync.mo po\de.po
msgfmt -o locale\en\LC_MESSAGES\autofilesync.mo po\en.po
msgfmt -o locale\es\LC_MESSAGES\autofilesync.mo po\es.po
msgfmt -o locale\fr\LC_MESSAGES\autofilesync.mo po\fr.po
msgfmt -o locale\it\LC_MESSAGES\autofilesync.mo po\it.po
msgfmt -o locale\pl\LC_MESSAGES\autofilesync.mo po\pl.po
msgfmt -o locale\uk\LC_MESSAGES\autofilesync.mo po\uk.po
```

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
scripts/run_Integrationtests.sh                 # Argumente gehen an gtest durch
# oder von Hand:
cmake -S . -B build -DAFS_BUILD_TESTS=ON
cmake --build build --target Integrationtests
./build/Integrationtests
```

### GUI-Tests (AFS_Desktop)
Unter Linux brauchen die GUI-Tests zusätzlich Google Test, Xvfb und die
XTest-Erweiterung; ohne den lokalen SSH-Testserver aus
`AFS_Core/test/Integrationstests/doc/SSH_TESTS_README.md` werden die SSH-Tests
übersprungen:
```bash
sudo apt install libgtest-dev xvfb libxtst-dev
```
```bash
scripts/run_GUI_Tests.sh
# oder von Hand:
cmake --build build --target GuiTests
./AFS_Desktop/test/GUI-Tests/run_gui_tests.sh build/GuiTests
```
Die Tests des Assistenten (`DISABLED_ChatGuiTest.*` und `DISABLED_SlowGuiTest.*`)
brauchen das lokale Sprachmodell und dauern Minuten; sie sind deaktiviert. Sie
laufen nur ausdrücklich mit
`./AFS_Desktop/test/GUI-Tests/run_gui_tests.sh build/GuiTests --gtest_also_run_disabled_tests --gtest_filter='DISABLED_ChatGuiTest.*:DISABLED_SlowGuiTest.*'`.
Auf macOS steuern die Tests echte Maus- und Tastaturereignisse auf dem aktiven
Desktop (`wxUIActionSimulator`); dafür braucht das ausführende Terminal die
Berechtigung Systemeinstellungen → Datenschutz & Sicherheit → Bedienungshilfen.

### Windows
`run_Tests.cmd` baut und startet beide Testarten: die Integrationstests über
`AFS_Core\test\Integrationstests\Integrationtests.sln` (MSBuild, vcpkg-Manifest),
die GUI-Tests über `run_GUI_Tests.bat`. Beide laufen auch dann, wenn die erste
fehlschlägt; am Ende steht eine Zusammenfassung, der Exit-Code ist 1, sobald eine
fehlschlägt.
```cmd
scripts\run_Tests.cmd                              :: beide, Debug
scripts\run_Tests.cmd integration --filter *Ssh*   :: nur Integrationstests, gtest-Filter
scripts\run_Tests.cmd gui --no-build -r            :: nur GUI-Tests, vorhandener Release-Build
```
Die GUI-Tests übernehmen auch hier Maus und Tastatur des aktiven Desktops.

## Installation

Siehe [INSTALL-Linux.md](INSTALL-Linux.md) (Linux) bzw.
[INSTALL-macOS.md](INSTALL-macOS.md) (macOS).

Unter macOS erzeugt `scripts/makeMacOSInstaller.sh` ein verteilbares Disk-Image,
`scripts/installMacOS.sh` installiert direkt nach `/Applications` und
`scripts/test_macOS_installer.sh` prüft das erzeugte Image. Die gemeinsamen Schritte
liegen in `AFS_Desktop/installer/macos_bundle.sh`.

Das Verzeichnis `AFS_Desktop/help/` gehört neben die installierte
Programmdatei – dort sucht das Programm sein Hilfebuch: `help/help.htb` auf
Deutsch, `help/<sprache>/help.htb` für jede andere Sprache, ersatzweise
`help/en/help.htb`. Eine weitere Sprache braucht nur ihre Seiten samt `help.hhp`
in `AFS_Desktop/help/<sprache>/`: Build, `tools/packHelp.*` und die
Paketskripte nehmen jedes solche Verzeichnis selbst auf. Die Bücher sind
nicht versioniert: der CMake-Build packt es aus den Seiten (Ziel `help_book`),
der MSBuild-Build über `build.cmd` mit `tools/packHelp.ps1`. Der Build kopiert
das Verzeichnis nach jedem Linken neben die Programmdatei (`build/help/`), ein
Build braucht also keine Installation, um die Hilfe anzeigen zu können. In das macOS-Bündel
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

## Signieren des Windows-Pakets

`makeWindowsInstaller.ps1` signiert `autofilesync.exe` und das MSI mit `signtool`
(Windows SDK) und Zeitstempel, wenn ein Zertifikat angegeben ist:

    $env:AFS_SIGN_THUMBPRINT = '<SHA1-Fingerabdruck>'   # Zertifikat in CurrentUser\My
    # oder
    $env:AFS_SIGN_PFX = 'C:\pfad\zertifikat.pfx'; $env:AFS_SIGN_PFX_PASSWORD = '...'

Ohne Zertifikat bricht das Skript ab, damit kein unsigniertes Paket unbemerkt
entsteht; `-NoSigning` baut bewusst ein unsigniertes Paket (Entwicklerbuild). Für eine Auslieferung braucht es ein Code-Signing-
Zertifikat einer öffentlichen CA (OV oder EV); ein selbst ausgestelltes
Zertifikat ergibt zwar eine gültige Signatur, aber keine Vertrauensstellung bei
den Kunden. Die Versionsangaben der Programmdatei (Eigenschaften-Dialog,
Signaturdialog) kommen aus dem VERSIONINFO-Block in `autofilesync.rc`, der
`include/Version.h` liest.

## Lizenz

AutoFileSync steht unter der GNU GPL v3 oder neuer (`LICENSE`). Einzelheiten
in `LICENSING.md`, Komponenten Dritter in `THIRD-PARTY-NOTICES.md`. Jede
Quelldatei trägt den Lizenz-Header; `python tools/licenseHeaders.py --check`
prüft das.

## Kontakt

Fehler und Wünsche: <https://welsch-software.com>.
Rechteinhaber: Joachim Welsch, <https://welsch-software.com>.
