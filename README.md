# AutoFileSync

AutoFileSync hält Dateien an zwei Orten auf demselben Stand: eine einzelne
Datei, ein Verzeichnis oder ein ganzer Verzeichnisbaum. Ein Ort kann ein
Verzeichnis auf dem eigenen Rechner sein, ein Netzlaufwerk, ein USB-Datenträger,
ein Rechner, der über SSH erreichbar ist, oder ein Android-Handy.
Einrichten, starten, vergessen.

AutoFileSync ist freie Software unter der GNU General Public License,
Version 3 oder neuer, und läuft unter Windows, macOS und Linux.

Website: <https://welsch-software.com>
Quelltext und Fehlermeldungen: <https://github.com/joachim-welsch/autofilesync>

## Funktionen

- Synchronisation von Dateien, Verzeichnissen und Verzeichnisbäumen, mit
  Platzhaltern zum Eingrenzen.
- Vergleich nach Datum, Größe oder Inhalt; Verschieben statt Kopieren;
  wählbarer Umgang mit gelöschten und neueren Zieldateien.
- Zwei-Wege-Synchronisation, die sich den letzten Stand merkt: Löschungen und
  Umbenennungen werden übernommen, auf beiden Seiten geänderte Dateien bleiben
  als Konfliktkopie erhalten.
- Vorschau, die zeigt, was ein Lauf tun würde, bevor sich etwas ändert.
- Nichts geht verloren: Generationenprinzip für ältere Stände, Papierkorb für
  Löschungen, ein abgezogenes Laufwerk wird nie als „alles gelöscht“ gedeutet,
  und Massenlöschungen werden erst nach Bestätigung in der Vorschau ausgeführt.
- Quelle oder Ziel über SSH auf einem NAS, Server oder Rechner im Heimnetz;
  Passwörter werden verschlüsselt abgelegt.
- Android-Handys werden per QR-Code verbunden; Vorlagen sichern Fotos auf dem
  PC, halten Ordner gleich oder bringen Musik aufs Handy. Die App
  (`AFS_Android/`) synchronisiert im Hintergrund, wahlweise nur im WLAN und
  beim Laden.
- Änderungen werden sofort übernommen: lokale Verzeichnisse werden beobachtet,
  statt nur in festen Abständen geprüft.
- Hintergrunddienst, der mit derselben Konfiguration synchronisiert, auch ohne
  angemeldeten Benutzer.
- Eingebauter Assistent, der Fragen zur Bedienung mit der Hilfe beantwortet,
  erklärt, warum ein Ziel nicht aktuell ist, und passende Befehle vorschlägt.
- Protokoll mit wählbarer Stufe, E-Mail-Benachrichtigung bei schweren Fehlern
  und ein Sicherungsnachweis im JSON-Format.
- Oberfläche und Hilfe auf Deutsch, Englisch, Spanisch, Französisch, Italienisch,
  Polnisch und Ukrainisch; die Sprache lässt sich in den Einstellungen wählen.

## Installation

Fertige Pakete für Windows, macOS und Linux stehen auf
<https://welsch-software.com> bereit. Wer selbst baut, findet den Weg in `doc/BUILD.md`, die vollständige
Einrichtung samt Hintergrunddienst in `doc/INSTALL-Linux.md` und
`doc/INSTALL-macOS.md`.

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
| `AFS_Android/` | Android-App (Kotlin), Bauanleitung in `AFS_Android/README.md` |
| `po/` | Sprachkataloge |
| `third-party/` | Lizenztexte der verwendeten Bibliotheken |

Tests: `scripts/run_Integrationtests.sh` für den Kern, `scripts/run_GUI_Tests.sh` für die
Oberfläche; Einzelheiten in `doc/BUILD.md`.

## Mitmachen

Fehlerberichte und Wünsche gehen als Issue an
<https://github.com/joachim-welsch/autofilesync/issues>, Änderungen kommen als Pull Request. Beiträge stehen unter derselben Lizenz wie
das Projekt (`doc/LICENSING.md`). Code-Stil und Ablauf stehen in `doc/CONTRIBUTING.md`.

## Unterstützung

AutoFileSync ist und bleibt kostenlos. Wer die Entwicklung unterstützen möchte,
kann per PayPal an `contact@welsch-software.com` spenden. Eigene Erweiterungs- und
Anpassungswünsche werden gerne gegen Bezahlung umgesetzt; Anfragen an dieselbe
Adresse.

## Lizenz

GNU General Public License v3 oder neuer, siehe `LICENSE` und `doc/LICENSING.md`.
Copyright © 2026 Joachim Welsch. Komponenten Dritter und ihre Lizenzen stehen
in `doc/THIRD-PARTY-NOTICES.md`.

---

*English: AutoFileSync keeps files, directories and whole directory trees in
sync between two places: local folders, USB and network drives, computers
reachable over SSH and Android phones, on Windows, macOS and Linux. It offers
one-way and two-way sync with conflict copies, a preview before each run,
generations of older versions, protection against mass deletions and a
background service. It is free software under the GNU GPL v3 or later. Build
instructions are in `doc/BUILD.md`; the user interface and help are available in
English.*
