# Autofilesync

## Zweck

Benutzerfreundliche automatische Synchronisation von Dateien hin bis Verzeichnisbäumen.

## Architektur
### Projektstruktur
- `AFS_Core/`: Synchronisationskern als statische Bibliothek `afs_core` (GPLv3).
  Enthält die aus mylib übernommenen Hilfsquellen unter `AFS_Core/mylib/`.
  Darf nichts aus AFS_Desktop einbinden (einzige noch offene Ausnahme:
  `ProgressDialog.h` in `SyncFile.cpp`, siehe `AFS_Core/cmake/AfsCore.cmake`).
- `AFS_Desktop/`: Benutzeroberfläche und weiterführende Funktionalität,
  erzeugt das Programm `autofilesync`.
- Quelllisten stehen in `AFS_Core/cmake/AfsCore.cmake` und
  `AFS_Desktop/cmake/AfsDesktop.cmake`; neue Dateien dort eintragen (und in den
  `.vcxproj`-Dateien für Windows).

### Sprache
modernstes C++ (mindestens C++20)

### GUI
Bibliothek: wxWidgets
Ziele:
- maximale Benutzerfreundlichkeit
- unerfahrene Benutzer sollen intuitiv die Benutzung herausfinden können, ohne Dokumentation zu lesen

### Daemon / Service
Synchronisation im Hintergrund mit der gleichen Konfiguration wie das GUI-Programm

## Priorität

Wenn Regeln sich widersprechen:
1. Robustheit
1. Intuitive Benutzung.
2. Benutzerfreundlichkeit.
3. Geschwindigkeit.

## Code Style
Kompakter Code wird bevorzugt (keine unnötigen Leerzeilen)

Wenn möglich sollen die folgenden Regeln eingehalten werden.
- Nicht notwendige Klammern werden wegelassen.
- Single Responsibility Principle (SRP) eine Verantwortung pro Funktion.
- Keep Functions Small – Funktionen kurz halten.
- Low Cyclomatic Complexity – möglichst geringe Verzweigungskomplexität.
- One Level of Abstraction per Function – innerhalb einer Funktion auf derselben Abstraktionsebene bleiben.
- Selbsterklärender Code soll Inline-Kommentaren bevorzugt werden.

### Kommentare
- Wenn sich ein Kommentar durch einen besseren Bezeichner oder eine kleine Refaktorisierung vermeiden lässt, ist dies zu bevorzugen.
-Der Code enthält nur Kommentare, die zum Verständnis des Codes notwendig sind;
     Architektur, Anforderungen oder Bedienkonzepte gehören in die Dokumentation.
- Kommentare sollten nur lokalen Kontext erklären.     
- Sprache: English
- Kein Kommentar, wenn nur der Code widerholt wird.
- Ein Kommentar soll mindestens eine dieser Fragen beantworten:
  Warum wird das so gemacht?
  Warum nicht anders?
  Welche Annahme gilt hier?
  Welche Randbedingung muss beachtet werden?
- Kommentare müssen bei jeder Codeänderung mit gepflegt werden. Veraltete Kommentare sind zu entfernen oder anzupassen.
- Kommentare beschreiben keine offensichtlichen Sprachkonstrukte oder Standardbibliotheksfunktionen.
- Der Kode soll nicht den kompakten Code stören (z.B. unnötige Zeilen)
  Kommentare sollen den Lesefluss des Codes nicht stören. Unnötige Leerzeilen oder großflächige Kommentarblöcke sind zu vermeiden.

## Tests

### Unittests

### Integrationstests
Die gesamte Funktionalität soll getestet werden

### GUI-Tests
Nach jedem GUI-Test soll der gleiche Zustand in der Konfiguration und im Quell- und Target-Dateisystem sein wie vor dem GUI-Test.

## Dokumentation
