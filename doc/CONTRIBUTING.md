# Mitmachen bei AutoFileSync

Fehlerberichte, Wünsche und Änderungen sind willkommen. Alles läuft über
GitHub: Fehler und Wünsche als Issue unter <https://github.com/joachim-welsch/autofilesync/issues>,
Änderungen als Pull Request gegen `master`.

## Lizenz der Beiträge

AutoFileSync steht unter der GNU GPL v3 oder neuer. Wer Änderungen beisteuert,
stellt den Beitrag unter dieselbe Lizenz. Ein Beitrag gilt mit seiner
Einreichung als unter dieser Bedingung eingereicht (`LICENSING.md`). Neue
Quelldateien tragen den Lizenz-Header; `python tools/licenseHeaders.py --apply`
setzt ihn, `--check` prüft ihn.

## Bevor der Pull Request abgeschickt wird

- Der Kern (`AFS_Core`) bindet nichts aus `AFS_Desktop` ein.
- Neue Dateien stehen in `AFS_Core/cmake/AfsCore.cmake` bzw.
  `AFS_Desktop/cmake/AfsDesktop.cmake` und in den `.vcxproj`-Dateien.
- `scripts/run_Integrationtests.sh` und `scripts/run_GUI_Tests.sh` laufen durch. Jeder
  Test räumt hinter sich auf; Dateisystem und Konfiguration sehen danach aus
  wie davor.
- Texte der Oberfläche kommen aus den Sprachkatalogen in `po/`, Deutsch und
  Englisch.

## Code-Stil

Wenn Regeln sich widersprechen, gilt diese Reihenfolge: Robustheit, intuitive
Benutzung, Benutzerfreundlichkeit, Geschwindigkeit.

**Sprache:** modernes C++, mindestens C++20, nach den C++ Core Guidelines.

**Bezeichner**

- Klassen und Typen in `PascalCase`, Member, Funktionen und Parameter in
  `snake_case`, Konstanten und Makros in `SCREAMING_SNAKE_CASE`.
- Membervariablen enden auf einen Unterstrich (`name_`).
- Keine Hungarian Notation, keine führenden Unterstriche.
- So kurz wie möglich, so lang wie nötig; der Kontext gibt den Namen.

**Form**

- Kompakter Code ohne unnötige Leerzeilen; öffnende und schließende Klammer
  einer Funktion stehen in eigener Zeile.
- Unnötige Klammern entfallen.
- Eine Verantwortung pro Funktion, kurze Funktionen, wenig Verzweigungen, eine
  Abstraktionsebene pro Funktion.
- Konstanten statt Makros, wo immer möglich.

**Kommentare**

- Sprache Englisch. Ein besserer Bezeichner oder eine kleine Refaktorisierung
  schlägt jeden Kommentar.
- Ein Kommentar beantwortet: Warum? Welche Annahme? Welche Randbedingung?
  Er wiederholt nicht den Code und erklärt keine Sprachkonstrukte.
- Inline-Kommentare stehen nur hinter dem Code, nie in eigener Zeile. Einzige
  Ausnahme ist der Lizenz-Header am Dateianfang.
- Längere Erklärungen gehören in die Kommentardatei des Moduls
  (`src/<Modul>.md`); der Inline-Kommentar verweist mit
  `[<Modul>.md#abschnitt]` dorthin.
- Architektur, Anforderungen und Bedienkonzepte gehören in die Dokumentation,
  nicht in den Code. Veraltete Kommentare werden entfernt.

**Oberfläche:** wxWidgets. Ziel ist, dass unerfahrene Benutzer die Bedienung
ohne Dokumentation herausfinden.

---

*English summary: contributions are welcome as issues and pull requests on
<https://github.com/joachim-welsch/autofilesync> and are licensed under the GNU GPL v3 or later by submission. Code is
modern C++20 following the C++ Core Guidelines: `PascalCase` types,
`snake_case` members and functions with a trailing underscore for member
variables, compact code, small single-purpose functions, English comments
only where they explain why, placed behind the code or in the module's `.md`
file. Run `scripts/run_Integrationtests.sh` and `scripts/run_GUI_Tests.sh` before
submitting.*
