# AutoFileSync

AutoFileSync hält Dateien an zwei Orten auf demselben Stand: eine einzelne
Datei, ein Verzeichnis oder ein ganzer Verzeichnisbaum. Ein Ort kann ein
Verzeichnis auf dem eigenen Rechner sein, ein Netzlaufwerk, ein USB-Datenträger
oder ein Rechner, der über SSH erreichbar ist. Einrichten, starten, vergessen.

AutoFileSync ist freie Software unter der GNU General Public License,
Version 3 oder neuer, und läuft unter Windows, macOS und Linux.

Website, Quelltext und Fehlermeldungen: <https://welsch-software.com>

## Funktionen

- Synchronisation von Dateien, Verzeichnissen und Verzeichnisbäumen, mit
  Platzhaltern zum Eingrenzen.
- Vergleich nach Datum, Größe oder Inhalt; Zwei-Wege-Synchronisation;
  Verschieben statt Kopieren; wählbarer Umgang mit gelöschten und neueren
  Zieldateien.
- Nichts geht verloren: Generationenprinzip für ältere Stände, Papierkorb für
  Löschungen, und ein abgezogenes Laufwerk wird nie als „alles gelöscht“
  gedeutet.
- Quelle oder Ziel über SSH auf einem NAS, Server oder Rechner im Heimnetz;
  Passwörter werden verschlüsselt abgelegt.
- Hintergrunddienst, der mit derselben Konfiguration synchronisiert, auch ohne
  angemeldeten Benutzer.
- Protokoll mit wählbarer Stufe, E-Mail-Benachrichtigung bei schweren Fehlern
  und ein Sicherungsnachweis im JSON-Format.
- Oberfläche und Hilfe auf Deutsch und Englisch.

## Installation

Fertige Pakete für Windows, macOS und Linux stehen auf
<https://welsch-software.com> bereit. Wer selbst baut, findet den Weg in `BUILD.md`, die vollständige
Einrichtung samt Hintergrunddienst in `INSTALL-Linux.md` und
`INSTALL-macOS.md`.

Kurzfassung für Linux:

```bash
sudo apt install build-essential cmake git pkg-config libwxgtk3.2-dev \
                 libboost-filesystem-dev libssh-dev libssl-dev imagemagick gettext
scripts/build.sh
scripts/run_autofilesync.sh
```

## Aufbau

| Verzeichnis | Inhalt |
|---|---|
| `AFS_Core/` | Synchronisationskern als statische Bibliothek, ohne Oberfläche |
| `AFS_Desktop/` | Programm mit wxWidgets-Oberfläche, Hilfe und Installer-Vorlagen |
| `backgroundsync/` | Hintergrunddienst für Linux, macOS und Windows |
| `po/` | Sprachkataloge |
| `third-party/` | Lizenztexte der verwendeten Bibliotheken |

Tests: `scripts/run_Integrationtests.sh` für den Kern, `scripts/run_GUI_Tests.sh` für die
Oberfläche; Einzelheiten in `BUILD.md`.

## Mitmachen

Fehlerberichte und Wünsche gehen über <https://welsch-software.com>,
Änderungen kommen als Patch. Beiträge stehen unter derselben Lizenz wie
das Projekt (`LICENSING.md`). Code-Stil und Ablauf stehen in `CONTRIBUTING.md`.

## Unterstützung

AutoFileSync ist und bleibt kostenlos. Wer die Entwicklung unterstützen möchte,
kann per PayPal an `joachim.welsch@posteo.de` spenden. Eigene Erweiterungs- und
Anpassungswünsche werden gerne gegen Bezahlung umgesetzt; Anfragen an dieselbe
Adresse.

## Lizenz

GNU General Public License v3 oder neuer, siehe `LICENSE` und `LICENSING.md`.
Copyright © 2026 Joachim Welsch. Komponenten Dritter und ihre Lizenzen stehen
in `THIRD-PARTY-NOTICES.md`.

---

*English: AutoFileSync keeps files, directories and whole directory trees in
sync between two places, locally, on network drives or over SSH, on Windows,
macOS and Linux. It is free software under the GNU GPL v3 or later. Build
instructions are in `BUILD.md`; the user interface and help are available in
English.*
