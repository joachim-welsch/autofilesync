# Autofilesync

## Zweck

Benutzerfreundliche automatische Synchronisation von Dateien hin bis Verzeichnisbäumen.

## Architektur
### Projektstruktur
- `AFS_Core/`: Synchronisationskern als statische Bibliothek `afs_core` (GPLv3).
  Darf nichts aus AFS_Desktop einbinden.
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
### Bezeichner
Für Klassen- und Type-Namen wird PascalCase verwendet. Für Member, Funktionen und Parameter wird snake_case verwendet. Vorhandener Code wird diesbezüglich angepasst.
Für Konstanten und Makros wird SCREAMING_SNAKE_CASE verwendet.
Hungarian Notation und führende Unterstriche sind verboten.
Membervariablen werden mit einem abschließenden Unterstrich gekennzeichnet.
Es werden Kontextbezogene Namen (NL.1) verwendet.
Bezeichner sollen möglichst kurz sein. Wenn es für den selbsterklärenden Code notwendig ist, dürfen sie auch lang werden.
### Leerzeilen
Kompakter Code wird bevorzugt (keine unnötigen Leerzeilen)
Die öffnende und schließende Klammer einer Funktion oder Methode soll in einer eigenen Zeile stehen.
### Allgemeine Regeln
Die C++ Core Guidelines (Bjarne Stroustrup und Herb Sutter) sollen eingehalten werden.
Der Code soll selbsterklärend sein.
Wenn möglich sollen die folgenden Regeln eingehalten werden.
- Nicht notwendige Klammern werden wegelassen.
- Single Responsibility Principle (SRP) eine Verantwortung pro Funktion.
- Keep Functions Small – Funktionen kurz halten.
- Low Cyclomatic Complexity – möglichst geringe Verzweigungskomplexität.
- One Level of Abstraction per Function – innerhalb einer Funktion auf derselben Abstraktionsebene bleiben.
- Selbsterklärender Code soll Inline-Kommentaren bevorzugt werden.
Konstanten werden Makros wenn immer möglich bevorzugt.
### Kommentare
- Wenn sich ein Kommentar durch einen besseren Bezeichner oder eine kleine Refaktorisierung vermeiden lässt, ist dies zu bevorzugen.
- Der Code enthält nur Kommentare, die zum Verständnis des Codes notwendig sind;
     Architektur, Anforderungen oder Bedienkonzepte gehören nicht in den Code, sondern in die Dokumentation.
- Kommentare sollten nur den lokalen Kontext erklären.
- Sprache: English
- Kein Kommentar, wenn nur der Code wiederholt wird.
- Ein Kommentar soll mindestens eine dieser Fragen beantworten:
  Warum wird das so gemacht?
  Welche Annahme gilt hier?
  Welche Randbedingung muss beachtet werden?
- Kommentare müssen bei jeder Codeänderung mit gepflegt werden. Veraltete Kommentare sind zu entfernen oder anzupassen.
- Kommentare beschreiben keine offensichtlichen Sprachkonstrukte oder Standardbibliotheksfunktionen.
- Der Kommentar soll nicht den kompakten Code stören (z.B. unnötige Zeilen)
  Kommentare sollen den Lesefluss des Codes nicht stören. Unnötige Leerzeilen oder großflächige Kommentarblöcke sind verboten.
- Zu jedem Modul wird eine Kommentardatei gepflegt und zwar im src-Verzeichnis. Hier wird der mehrzeilige Kommentar abgelegt.
- Inline-Kommentar ist im Quelltext nur hinter dem Code erlaubt. Inline-Kommentare stehen auf derselben Zeile wie der Code. Zeilen, die nur Kommentar enthalten sind strikt verboten.
- Im Inline-Kommentar sollten Sprungmarken zum Modulkommentar stehen, wenn es um die Erklärung eines Abschnitts des Codes geht.
- Ausnahme: Der Lizenz-Header am Dateianfang (GPLv3-Hinweis) ist als Kommentarblock erlaubt und in allen Quelldateien von AFS_Core, AFS_Desktop und backgroundsync Pflicht. `python tools/licenseHeaders.py --apply` setzt ihn, `--check` prüft ihn.
## Tests

### Unittests

### Integrationstests
Die gesamte Funktionalität soll getestet werden.
Nach jedem Test muss aufgeräumt werden, sodass der Zustand des Dateisystems nach dem Test wie vorher ist.
Das Logging soll im temp-Verzeichnis erfolgen und die Logdatei soll auch nach dem Test wieder gelöscht werden.

### GUI-Tests
Nach jedem GUI-Test soll der gleiche Zustand in der Konfiguration und im Quell- und Target-Dateisystem sein wie vor dem GUI-Test.

## Dokumentation
