# Änderungen

## Unveröffentlicht

- Assistent: Fragen wie „Wie stelle ich die Sprache um?“ beantwortet er mit
  dem passenden Abschnitt der Hilfe und nennt die Hilfeseite. Da die Hilfe nur
  auf Deutsch vorliegt, greift das vor allem bei deutschen Fragen.
- Assistent: Ein langes Gespräch lässt seine ältesten Runden weg, statt jede
  weitere Frage mit „Unterhaltung zu lang“ abzulehnen, bis das Programm neu
  gestartet wird. **Neues Gespräch** im Chatfenster beginnt von vorn.
- Assistent: Fragen nach dem Protokoll werden in jeder Sprache erkannt
  („journal“, „registro“, „dziennik“, „журнал“ …), nicht nur auf Deutsch und
  Englisch; nur dann bekommt er den Auszug aus der Logdatei.
- Der Datumsvergleich hat eine einstellbare Toleranz (Reiter Optionen, Vorgabe
  2 Sekunden). Bisher galt eine Datei auf einem NTFS-, exFAT- oder
  FAT-Datenträger bei jedem Durchlauf als geändert, weil das Ziel die
  Änderungszeit gröber speichert als die Quelle, und wurde immer wieder
  kopiert.
- Der Fortschrittsbalken bewegt sich auch innerhalb einer Datei: Er zeigt die
  fertigen Dateien plus den schon kopierten Anteil der laufenden Datei, auch
  über SSH. Abbrechen wirkt mitten in einer großen Datei und lässt keine halbe
  Datei im Ziel zurück. Während „Einmal synchronisieren“ die Dateien zählt,
  zeigt der Mauszeiger die Wartesanduhr.
- Die Oberfläche folgt den Plattformregeln enger: Tastenkürzel für Neu,
  Öffnen, Speichern, Speichern als, Einstellungen, Beenden und Hilfe;
  Menüpunkte, die einen Dialog öffnen, enden mit „…“; Bestätigungen fragen
  mit „Löschen“, „Überschreiben“ bzw. „Speichern“/„Nicht speichern“ statt
  Ja/Nein, und Abbrechen ist die Vorgabe. Unter macOS stehen „Einstellungen…“
  und „Info“ im Programm-Menü, das Fenster zeigt ungespeicherte Änderungen im
  Schließen-Knopf, und das Info-Fenster ist das native.
- Dialoge nach den Plattformregeln: Feldbeschriftungen enden mit Doppelpunkt,
  die Knöpfe „...“ und „ssh“ heißen „Auswählen…“ und „SSH…“, der Log-Level ist
  ein Aufklappmenü, die globalen Einstellungen lassen sich abbrechen, und jede
  Meldung trägt ihr Symbol und erscheint vor dem richtigen Fenster. Der Hinweis
  auf den minimierten Start und auf ein fehlendes Hilfebuch kommt als
  Systembenachrichtigung statt als Dialog, der sich von selbst schließt.
- Die Werkzeugleiste beginnt mit „Neue Synchronisation“. Im Menü Datei stehen
  „Neue Synchronisation…“ und „Neue SSH-Verbindung…“ direkt. Im Dialog einer
  Synchronisation bilden die Vergleichskriterien eine eigene Gruppe. Dialoge
  und Tabelle skalieren mit der Bildschirmauflösung, die Tabellenköpfe nutzen
  die Systemschrift, und die Wurzel der Baumansicht heißt „Dieser Computer“
  statt „PC“.
- Konfiguration und Sicherungsnachweise stehen lesbar in der Datei: Zeichen wie
  kyrillische Buchstaben erscheinen als sie selbst statt als `\u0423…`, und
  Emoji in Namen gehen nicht mehr verloren.
- Der Benutzer, unter dem der Hintergrunddienst läuft, darf ihn ohne
  Administratorpasswort starten und stoppen: Das Debian-Paket legt dafür eine
  polkit-Regel an, der Windows-Installer gibt dem Dienstkonto die Rechte am
  Dienst.
- Die Konfiguration wird als UTF-8 geschrieben. Unter Linux und macOS ließ sie
  sich bisher nicht speichern, sobald Name oder Pfad einer Synchronisation
  einen Umlaut enthielt.
- Sicherungsnachweise werden als UTF-8 geschrieben. Bisher entstand kein
  Nachweis, wenn Name oder Pfad einer Synchronisation einen Umlaut enthielt,
  und in Französisch und Polnisch gar keiner.
- Die globalen Einstellungen haben eine Sprachauswahl. Die gewählte Sprache
  gilt sofort, steht in der Konfiguration und gilt bei jedem Start, auch für
  den Hintergrunddienst; ohne Auswahl gilt die Sprache des Betriebssystems.
- Ein Zahnrad rechts in der Werkzeugleiste öffnet die globalen Einstellungen.
- Übersetzungen ins Spanische, Französische, Italienische, Polnische und
  Ukrainische (`po/es.po`, `po/fr.po`, `po/it.po`, `po/pl.po`, `po/uk.po`);
  sie gelten, wenn die Umgebung die Sprache einstellt, z.B. `LANGUAGE=pl`.
- In einer Sprache ohne eigene Übersetzung (z.B. Schwedisch) erscheinen die
  Meldungen auf Englisch statt als technische Schlüssel wie
  `source_dir_not_reachable`.

## 0.2.0 – 2026-10-09

- macOS: Der Hintergrunddienst steckt im Programmbündel und damit im
  Disk-Image; Start im Menü Hintergrunddienst schreibt den LaunchAgent selbst,
  Stop entfernt ihn wieder. Bisher musste er aus den Quellen gebaut und von
  Hand eingerichtet werden und blieb bei Updates auf dem alten Stand.
- Windows: Der Hintergrunddienst wird mit dem Programm gebaut und vom
  Installationspaket als Dienst eingerichtet, beim Entfernen gestoppt und mit
  entfernt; eine Seite des Installationsprogramms fragt nach dem Konto, unter
  dem er laufen soll. Zuvor lief auf Windows ein alter, von Hand registrierter
  Stand, dem die Kennzeichnung der Logzeilen (G/D) und die übrigen
  Verbesserungen des Dienstes fehlten.
- Der Hintergrunddienst übernimmt eine gespeicherte Konfiguration von selbst,
  statt bis zum nächsten Neustart mit dem alten Stand zu arbeiten.
- Jede Änderung der Konfiguration wird sofort gespeichert; Datei → Speichern
  bleibt nur zum Nachholen eines fehlgeschlagenen Schreibens anwählbar.
- Der Fenstertitel zeigt "(nicht gespeichert)", solange eine Änderung nicht in
  der Datei steht; der Start des Hintergrunddienstes fragt dann vorher, ob
  gespeichert werden soll (Ja/Nein/Abbrechen).
- Das Hauptfenster wird nicht mehr bei jedem Start breiter; Größe und Position
  werden auf den Bildschirm beschnitten, auch wenn der zuletzt benutzte
  Monitor fehlt.
- Windows: Das Kürzen der gemeinsamen Logdatei beim Start scheitert nicht mehr
  mit "Zugriff verweigert", während der Hintergrunddienst sie offen hält.
- Bauen: Die Skripte liegen in `scripts/`, die Dokumente in `doc/`; der
  CMake-Build packt das Hilfebuch `help.htb` selbst.

## 0.1.0 – 2026-09-24

- Alle Texte der Oberfläche und des Synchronisationskerns kommen aus
  Sprachkatalogen (Deutsch, Englisch); die Sprache folgt der Umgebung, z.B.
  `LANGUAGE=en`.
- Lizenz: GNU GPL v3 oder neuer (`LICENSING.md`), Lizenz-Header in allen
  Quelldateien, Hinweise zu Komponenten Dritter, Lizenzseite im
  Windows-Installationsprogramm.
- Meldungen eines Einzellaufs erscheinen nach den Filtern Fehler/Warnung/Info
  in einer Zusammenfassung.
- Die Statuszeile zeigt durchgehend, was die Synchronisation tut.
- 94 GUI-Tests decken jede Hilfeseite ab; dabei behoben: Stopp ohne aktiven
  Lauf, wirkungsloses Abbrechen im Fortschrittsdialog, doppelte Fehlerbox beim
  fehlenden Editor, Hilfe-Knopf der Werkzeugleiste, Stillstand durch ein
  schlafendes Netzlaufwerk.
